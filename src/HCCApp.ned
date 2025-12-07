package src;

import inet.applications.contract.IApp;

simple HCCApp like IApp
{
    parameters:
        @class(inet::HCCApp);
        int localPort = default(5000);
        int destPort = default(5000);

        // HATA ÇÖZÜMÜ: Bu satırı ekleyin.
        // Varsayılan olarak multicast adresini buraya koyuyoruz.
        string destAddress = default("224.0.0.1");

        volatile double beaconInterval @unit(s) = default(1s);
        double neighborValidityInterval @unit(s) = default(4s);

        @display("i=block/network2");

        @signal[clusterRole](type="long");
        @signal[clusterSize](type="long");
        @signal[nodeDegree](type="long");

        @statistic[clusterRole](title="Cluster Role"; source="clusterRole"; record=vector,timeavg);
        @statistic[nodeDegree](title="Node Degree"; source="nodeDegree"; record=vector,mean);

    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}