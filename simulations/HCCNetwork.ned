package simulations;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.physicallayer.wireless.common.contract. packetlevel.IRadioMedium;
import inet.visualizer.common.IntegratedVisualizer;

network HCCNetwork
{
    parameters:
        int numHosts = default(20); // Varsayılan 20 node olsun
        @display("bgb=1000,1000"); // 1000x1000 metrelik bir alan
    submodules:
        visualizer: IntegratedVisualizer {
            @display("p=100,50");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=100,100");
        }
        radioMedium: <default("Ieee80211ScalarRadioMedium")> like IRadioMedium {
            @display("p=100,200");
        }
        host[numHosts]: AdhocHost {
            @display("p=500,500"); // Başlangıçta rastgele dağılacaklar
        }
}