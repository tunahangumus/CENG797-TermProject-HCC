#include "HCCApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "HCCPacket_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"

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

        clusterRoleSignal = registerSignal("clusterRole");
        clusterSizeSignal = registerSignal("clusterSize");
        nodeDegreeSignal = registerSignal("nodeDegree");
        chLifetimeSignal = registerSignal("chLifetime");
        roleChangesSignal = registerSignal("roleChanges");
        controlOverheadSignal = registerSignal("controlOverhead");

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
    chunk->setChunkLength(B(16));

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

    NeighborInfo& info = neighborTable[senderId];
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
    while (it != neighborTable.end()) {
        if (now - it->second.lastSeen > neighborValidityInterval) {
            it = neighborTable.erase(it);
        } else {
            ++it;
        }
    }
    myDegree = neighborTable.size();
    emit(nodeDegreeSignal, myDegree);
}

void HCCApp::runHCCAlgorithm()
{
    int bestDegree = myDegree;
    int bestNodeId = myId;

    for (auto const& [id, neighbor] : neighborTable) {
        if (neighbor.degree > bestDegree) {
            bestDegree = neighbor.degree;
            bestNodeId = neighbor.id;
        }
        else if (neighbor.degree == bestDegree) {
            if (neighbor.id > bestNodeId) {
                bestNodeId = neighbor.id;
            }
        }
    }

    int newRole = (bestNodeId == myId) ? CLUSTER_HEAD : CLUSTER_MEMBER;
    int newCH = bestNodeId;

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
        }
}

void HCCApp::updateVisuals()
{
    cModule *host = getParentModule();
    if (!host) return;

    if (myRole == CLUSTER_HEAD) {
        host->getDisplayString().setTagArg("i", 1, "red");
        host->getDisplayString().setTagArg("t", 0, "CH");
    } else if (myRole == CLUSTER_MEMBER) {
        host->getDisplayString().setTagArg("i", 1, "blue");
        host->getDisplayString().setTagArg("t", 0, "Mbr");
    } else {
        host->getDisplayString().setTagArg("i", 1, "grey");
        host->getDisplayString().setTagArg("t", 0, "Und");
    }
}

void HCCApp::socketErrorArrived(UdpSocket *socket, Indication *indication) { delete indication; }
void HCCApp::socketClosed(UdpSocket *socket) {}
void HCCApp::finish() {}

} // namespace inet
