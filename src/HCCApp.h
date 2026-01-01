#ifndef __INET_HCCAPP_H
#define __INET_HCCAPP_H

#include <map> // neighborTable için gerekli
#include "inet/common/INETDefs.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet {

class HCCPacket;

class HCCApp : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    struct NeighborInfo {
            int id;
            int degree;
            int role;
            int clusterHeadId;
            simtime_t lastSeen;
        };

    int localPort = -1;
    int destPort = -1;
    L3Address destAddress;
    simtime_t beaconInterval;
    simtime_t neighborValidityInterval;
    std::map<int, int> neighborClusterTable;

    simsignal_t chLifetimeSignal;
    simsignal_t roleChangesSignal;
    simsignal_t controlOverheadSignal;
    simsignal_t gatewayBridgingSignal;

    simtime_t timeBecameCH;

    int myId = 0;
    int myDegree = 0;
    int myRole = 0;
    int myClusterHeadId = -1;

    std::map<int, NeighborInfo> neighborTable;

    UdpSocket socket;
    cMessage *beaconTimer = nullptr;

    simsignal_t clusterRoleSignal;
    simsignal_t clusterSizeSignal;
    simsignal_t nodeDegreeSignal;

  public:
    HCCApp();
    virtual ~HCCApp();

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;

    void sendBeacon();

    void processBeacon(const Ptr<const HCCPacket>& payload);
    void cleanUpNeighbors();
    void runHCCAlgorithm();
    void updateVisuals();
};

}

#endif
