#ifndef __INET_HCCAPP_H
#define __INET_HCCAPP_H

#include <map> // neighborTable için gerekli
#include "inet/common/INETDefs.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet {

// HCCPacket sınıfını burada önceden bildiriyoruz ki pointer olarak kullanabilelim.
// Not: .cc dosyanızda #include "HCCPacket_m.h" ekli olmalıdır.
class HCCPacket;

class HCCApp : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    // Komşular hakkında bilgi tutacak yapı
    struct NeighborInfo {
        int id;
        int degree;
        int role;        // 0: Undecided, 1: CH, 2: CM
        int clusterHeadId;
        simtime_t lastSeen;
    };

    // --- Konfigürasyon Değişkenleri ---
    int localPort = -1;
    int destPort = -1;
    L3Address destAddress;
    simtime_t beaconInterval;
    simtime_t neighborValidityInterval;

    // --- Durum Değişkenleri (Eksik olanlar eklendi) ---
    int myId = 0;
    int myDegree = 0;
    int myRole = 0;          // 0: Undecided, 1: CH, 2: CM
    int myClusterHeadId = -1;

    // Komşu Tablosu (std::map kullanımı)
    std::map<int, NeighborInfo> neighborTable;

    // --- Altyapı Değişkenleri ---
    UdpSocket socket;
    cMessage *beaconTimer = nullptr;

    // --- Sinyaller (İstatistikler için) ---
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

    // Lifecycle metotları
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // Socket callback'leri
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;

    // HCC Algoritma Fonksiyonları
    void sendBeacon();
    // Ptr kullanımı için INETDefs.h yeterlidir
    void processBeacon(const Ptr<const HCCPacket>& payload);
    void cleanUpNeighbors();
    void runHCCAlgorithm();
    void updateVisuals();
};

} // namespace inet

#endif
