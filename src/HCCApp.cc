#include "HCCApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "HCCPacket_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <set> // Added for Gateway detection

namespace inet {

Define_Module(HCCApp);

HCCApp::HCCApp() {
}

HCCApp::~HCCApp() {
    cancelAndDelete(beaconTimer);
}

void HCCApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        localPort = par("localPort");
        destPort = par("destPort");
        beaconInterval = par("beaconInterval");
        neighborValidityInterval = par("neighborValidityInterval");

        const char *destAddrStr = par("destAddress");
        if (destAddrStr != nullptr && *destAddrStr != '\0') {
             destAddress = L3AddressResolver().resolve(destAddrStr);
        }

        // --- REGISTER SIGNALS HERE ---
        clusterRoleSignal = registerSignal("clusterRole");
        clusterSizeSignal = registerSignal("clusterSize");
        nodeDegreeSignal = registerSignal("nodeDegree");
        chLifetimeSignal = registerSignal("chLifetime");
        roleChangesSignal = registerSignal("roleChanges");
        controlOverheadSignal = registerSignal("controlOverhead");

        // NEW: Register the bridging signal
        gatewayBridgingSignal = registerSignal("gatewayBridging");

        timeBecameCH = SIMTIME_ZERO;

        beaconTimer = new cMessage("sendBeacon");
        myId = getParentModule()->getId();

        socket.setOutputGate(gate("socketOut"));
    }
}

void HCCApp::handleStartOperation(LifecycleOperation *operation)
{
    if (!socket.isOpen()) {
        socket.bind(localPort);
        socket.setBroadcast(true);

        if (!destAddress.isUnspecified() && destAddress.isMulticast()) {
            NetworkInterface *mcastInterface = L3AddressResolver().interfaceTableOf(getContainingNode(this))->findInterfaceByName("wlan0");
            if (mcastInterface) {
                socket.joinMulticastGroup(destAddress, mcastInterface->getInterfaceId());
            } else {
                socket.joinMulticastGroup(destAddress);
            }
        }
        socket.setCallback(this);
    }

    if (!beaconTimer->isScheduled()) {
        scheduleAt(simTime() + uniform(0, 0.1), beaconTimer);
    }

    updateVisuals();
}

void HCCApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(beaconTimer);
    socket.close();
}

void HCCApp::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(beaconTimer);
    if (socket.isOpen()) {
        socket.close();
    }
}

void HCCApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg == beaconTimer) {
        sendBeacon();
        scheduleAt(simTime() + beaconInterval, beaconTimer);
    }
    else {
        socket.processMessage(msg);
    }
}

void HCCApp::sendBeacon()
{
    // Maintenance step before sending: clean old neighbors and run algorithm
    cleanUpNeighbors();
    runHCCAlgorithm();

    emit(controlOverheadSignal, 1);

    const char *payloadName = "HCCBeacon";
    Packet *packet = new Packet(payloadName);

    auto chunk = makeShared<HCCPacket>();
    chunk->setSrcId(myId);
    chunk->setDegree(myDegree);
    chunk->setRole(myRole);
    chunk->setClusterHeadId(myClusterHeadId);
    chunk->setChunkLength(B(16)); // Simulate header size

    packet->insertAtBack(chunk);

    socket.sendTo(packet, destAddress, destPort);
}

void HCCApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    auto chunk = packet->peekAtFront<HCCPacket>();
    if (chunk) {
        processBeacon(chunk);
    }
    delete packet;
}

void HCCApp::processBeacon(const Ptr<const HCCPacket>& payload)
{
    int senderId = payload->getSrcId();
    if (senderId == myId) return;

    // Access the struct in the map (creates default if new)
    NeighborInfo& info = neighborTable[senderId];

    // Update all fields
    info.id = senderId;
    info.degree = payload->getDegree();
    info.role = payload->getRole();
    info.clusterHeadId = payload->getClusterHeadId();
    info.lastSeen = simTime();
}

void HCCApp::cleanUpNeighbors()
{
    simtime_t now = simTime();
    auto it = neighborTable.begin();

    // 1. Remove expired neighbors
    while (it != neighborTable.end()) {
        if (now - it->second.lastSeen > neighborValidityInterval) {
            it = neighborTable.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Update Degree Metric
    myDegree = neighborTable.size();
    emit(nodeDegreeSignal, myDegree);

    // 3. Update Cluster Size Metric (If I am CH)
    if (myRole == CLUSTER_HEAD) {
        int currentClusterSize = 0;
        // Count myself
        currentClusterSize++;
        // Count neighbors who claim I am their CH
        for (auto const& [id, neighbor] : neighborTable) {
            if (neighbor.clusterHeadId == myId) {
                currentClusterSize++;
            }
        }
        emit(clusterSizeSignal, currentClusterSize);
    }
}
void HCCApp::runHCCAlgorithm()
{
    int bestDegree = myDegree;
    int bestNodeId = myId;

    // 1. Determine Cluster Head (Highest Connectivity Logic)
    for (auto const& [id, neighbor] : neighborTable) {
        // [FIX] Skip expired neighbors (moved away)
        if (simTime() - neighbor.lastSeen > neighborValidityInterval) continue;

        if (neighbor.degree > bestDegree) {
            bestDegree = neighbor.degree;
            bestNodeId = neighbor.id;
        }
        else if (neighbor.degree == bestDegree) {
            // Tie-breaking: higher ID wins
            if (neighbor.id > bestNodeId) {
                bestNodeId = neighbor.id;
            }
        }
    }

    int newRole = UNDECIDED;
    int newCH = bestNodeId;

    if (bestNodeId == myId) {
        newRole = CLUSTER_HEAD;

        // [FIX] CHs are not Gateways -> emit 0 to clear previous status
        emit(gatewayBridgingSignal, 0);
    }
    else {
        // 2. If not CH, determine if Member or Gateway
        std::set<int> uniqueClusters;

        // Add my own potential CH (the cluster I belong to)
        uniqueClusters.insert(newCH);

        // Check neighbors to see what clusters they belong to
        for (auto const& [id, neighbor] : neighborTable) {
            // [FIX] Skip expired neighbors here too
            if (simTime() - neighbor.lastSeen > neighborValidityInterval) continue;

            // If neighbor is a CH, they represent their own cluster
            if (neighbor.role == CLUSTER_HEAD) {
                uniqueClusters.insert(neighbor.id);
            }
            // If neighbor is Member/Gateway, they belong to a CH
            else if (neighbor.clusterHeadId != -1) {
                uniqueClusters.insert(neighbor.clusterHeadId);
            }
        }

        // If I can see more than 1 cluster (My Cluster + At least one other)
        if (uniqueClusters.size() > 1) {
            newRole = GATEWAY;

            // Emit the number of clusters bridged (e.g., 2, 3...)
            emit(gatewayBridgingSignal, (long)uniqueClusters.size());
        } else {
            newRole = CLUSTER_MEMBER;

            // [FIX] Not bridging -> emit 0
            emit(gatewayBridgingSignal, 0);
        }
    }

    // 3. State Change Handling
    if (newRole != myRole) {
        emit(roleChangesSignal, 1);

        if (myRole == CLUSTER_HEAD && newRole != CLUSTER_HEAD) {
            simtime_t duration = simTime() - timeBecameCH;
            emit(chLifetimeSignal, duration);
        }

        if (newRole == CLUSTER_HEAD) {
            timeBecameCH = simTime();
        }

        myRole = newRole;
        myClusterHeadId = newCH;

        emit(clusterRoleSignal, myRole);
        updateVisuals();
    }
    else if (newCH != myClusterHeadId) {
        myClusterHeadId = newCH;
        updateVisuals();
    }
}
void HCCApp::updateVisuals()
{
    cModule *host = getParentModule();
    if (!host) return;

    if (myRole == CLUSTER_HEAD) {
        host->getDisplayString().setTagArg("i", 1, "red");
        host->getDisplayString().setTagArg("t", 0, "CH");
    }
    else if (myRole == GATEWAY) {
        host->getDisplayString().setTagArg("i", 1, "yellow");
        host->getDisplayString().setTagArg("t", 0, "GW");
    }
    else if (myRole == CLUSTER_MEMBER) {
        host->getDisplayString().setTagArg("i", 1, "blue");
        host->getDisplayString().setTagArg("t", 0, "Mbr");
    }
    else {
        host->getDisplayString().setTagArg("i", 1, "grey");
        host->getDisplayString().setTagArg("t", 0, "Und");
    }
}

void HCCApp::socketErrorArrived(UdpSocket *socket, Indication *indication) { delete indication; }
void HCCApp::socketClosed(UdpSocket *socket) {}
void HCCApp::finish() {}

} // namespace inet
