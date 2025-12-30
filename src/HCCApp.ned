package src;

import inet.applications.contract.IApp;

simple HCCApp like IApp
{
    parameters:
        @class(inet::HCCApp);
        int localPort = default(5000);
        int destPort = default(5000);
        string destAddress = default("224.0.0.1");
        volatile double beaconInterval @unit(s) = default(1s);
        double neighborValidityInterval @unit(s) = default(4s);

        @display("i=block/network2");

        @signal[clusterRole](type="long");
        @signal[nodeDegree](type="long");

        @signal[chLifetime](type="simtime_t");  // Duration a node stays as CH
        @signal[roleChanges](type="long");      // Count of role switches (Stability)
        @signal[controlOverhead](type="long");  // Number of beacons sent

        @statistic[clusterRole](title="Cluster Role"; source="clusterRole"; record=vector,timeavg);
        @statistic[nodeDegree](title="Node Degree"; source="nodeDegree"; record=vector,mean);

        @statistic[roleChanges](title="Cluster Instability (Role Changes)"; source="roleChanges"; record=count,vector);

        @statistic[chLifetime](title="Cluster Head Lifetime"; source="chLifetime"; record=vector,mean,max);

        @statistic[controlOverhead](title="Control Overhead (Packets)"; source="controlOverhead"; record=count,sum);

    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}