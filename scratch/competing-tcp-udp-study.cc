#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace ns3;

static constexpr uint64_t TCP_EXPECTED_BYTES = 4ULL * 1024 * 1024;
static constexpr double APP_START = 1.0;
static constexpr double APP_STOP = 19.0;

static uint64_t tcpRxTraceBytes = 0;
static double tcpCompletionSeconds = -1.0;

static void TcpReceiveTrace(Ptr<const Packet> packet, const Address &from)
{
    (void)from;
    tcpRxTraceBytes += packet->GetSize();

    if (tcpCompletionSeconds < 0.0 &&
        tcpRxTraceBytes >= TCP_EXPECTED_BYTES)
    {
        tcpCompletionSeconds = Simulator::Now().GetSeconds() - APP_START;
    }
}

int main(int argc, char *argv[])
{
    std::string scenario = "combined";
    std::string udpRate = "2.5Mbps";
    std::string prefix = "combined-test";

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario",
                 "tcp-only, udp-only, or combined", scenario);
    cmd.AddValue("udpRate",
                 "UDP offered rate, e.g. 500Kbps, 2.5Mbps, 4.5Mbps",
                 udpRate);
    cmd.AddValue("prefix",
                 "Output filename prefix", prefix);
    cmd.Parse(argc, argv);

    if (scenario != "tcp-only" &&
        scenario != "udp-only" &&
        scenario != "combined")
    {
        std::cerr << "Invalid scenario. Use tcp-only, udp-only, or combined.\n";
        return 1;
    }

    const bool enableTcp = scenario != "udp-only";
    const bool enableUdp = scenario != "tcp-only";

    // Node IDs: 0 = TCP sender, 1 = UDP sender,
    //           2 = router, 3 = destination server.
    NodeContainer nodes;
    nodes.Create(4);

    // Fast access links.
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer tcpAccess = access.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer udpAccess = access.Install(nodes.Get(1), nodes.Get(2));

    // Shared bottleneck: both traffic sources must cross this link.
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", StringValue("50p"));

    NetDeviceContainer bottleneckDevices =
        bottleneck.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer tcpIf = address.Assign(tcpAccess);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer udpIf = address.Assign(udpAccess);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckIf = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    const Ipv4Address tcpSourceIp = tcpIf.GetAddress(0);
    const Ipv4Address udpSourceIp = udpIf.GetAddress(0);
    const Ipv4Address serverIp = bottleneckIf.GetAddress(1);

    // Receiver applications.
    PacketSinkHelper tcpSinkHelper(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), 5000));

    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(3));
    tcpSinkApp.Start(Seconds(0.5));
    tcpSinkApp.Stop(Seconds(20.0));
    tcpSinkApp.Get(0)->TraceConnectWithoutContext(
        "Rx", MakeCallback(&TcpReceiveTrace));

    PacketSinkHelper udpSinkHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), 6000));

    ApplicationContainer udpSinkApp = udpSinkHelper.Install(nodes.Get(3));
    udpSinkApp.Start(Seconds(0.5));
    udpSinkApp.Stop(Seconds(20.0));

    // TCP bulk transfer.
    if (enableTcp)
    {
        BulkSendHelper tcpSender(
            "ns3::TcpSocketFactory",
            InetSocketAddress(serverIp, 5000));

        tcpSender.SetAttribute("MaxBytes",
                               UintegerValue(TCP_EXPECTED_BYTES));
        tcpSender.SetAttribute("SendSize", UintegerValue(1024));

        ApplicationContainer tcpApp = tcpSender.Install(nodes.Get(0));
        tcpApp.Start(Seconds(APP_START));
        tcpApp.Stop(Seconds(APP_STOP));
    }

    // Constant-rate UDP stream. Only the offered UDP rate changes
    // between the combined-load experiments.
    if (enableUdp)
    {
        OnOffHelper udpSender(
            "ns3::UdpSocketFactory",
            InetSocketAddress(serverIp, 6000));

        udpSender.SetAttribute(
            "DataRate", DataRateValue(DataRate(udpRate)));
        udpSender.SetAttribute("PacketSize", UintegerValue(1024));
        udpSender.SetAttribute(
            "OnTime",
            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        udpSender.SetAttribute(
            "OffTime",
            StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer udpApp = udpSender.Install(nodes.Get(1));
        udpApp.Start(Seconds(APP_START));
        udpApp.Stop(Seconds(APP_STOP));
    }

    // Save each artifact type in its own results directory.
    const std::string resultsDir = "results/";
    bottleneck.EnablePcap(resultsDir + "wireshark/" + prefix + "-bottleneck",
                          bottleneckDevices.Get(1), true, true);

    // Optional NetAnim output for the new experiment.
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AnimationInterface anim(resultsDir + "netanim/" + prefix + "-animation.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 60.0);
    anim.SetConstantPosition(nodes.Get(2), 50.0, 40.0);
    anim.SetConstantPosition(nodes.Get(3), 90.0, 40.0);

    anim.UpdateNodeDescription(0, "TCP Sender");
    anim.UpdateNodeDescription(1, "UDP Sender");
    anim.UpdateNodeDescription(2, "Router");
    anim.UpdateNodeDescription(3, "Destination Server");

    anim.UpdateNodeColor(0, 40, 100, 220);
    anim.UpdateNodeColor(1, 30, 160, 80);
    anim.UpdateNodeColor(2, 230, 150, 30);
    anim.UpdateNodeColor(3, 180, 60, 60);

    // Presentation-focused visual enhancements.
    anim.UpdateNodeSize(0, 4.0, 4.0);
    anim.UpdateNodeSize(1, 4.0, 4.0);
    anim.UpdateNodeSize(2, 5.0, 5.0);
    anim.UpdateNodeSize(3, 5.0, 5.0);

    // Make the network paths understandable during the demo.
    anim.UpdateLinkDescription(0, 2, "TCP access link - 100 Mbps");
    anim.UpdateLinkDescription(1, 2, "UDP access link - 100 Mbps");
    anim.UpdateLinkDescription(2, 3, "Shared Link (5 Mbps)");


    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    std::cout << "\n========================================\n";
    std::cout << "TCP/UDP COMPETING-TRAFFIC EXPERIMENT\n";
    std::cout << "========================================\n";
    std::cout << "Scenario: " << scenario << "\n";
    std::cout << "TCP enabled: " << (enableTcp ? "yes" : "no") << "\n";
    std::cout << "UDP enabled: " << (enableUdp ? "yes" : "no") << "\n";
    std::cout << "UDP offered rate: "
              << (enableUdp ? udpRate : "disabled") << "\n";
    std::cout << "Shared bottleneck: 5 Mbps, 10 ms delay, 50-packet queue\n";
    std::cout << "Server IP: " << serverIp << "\n";
    std::cout << "Application interval: 1 to 19 seconds\n";

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    uint64_t tcpFlowRxBytes = 0;
    uint64_t udpTxPackets = 0;
    uint64_t udpRxPackets = 0;
    double udpDelaySumSeconds = 0.0;

    std::map<FlowId, FlowMonitor::FlowStats> stats =
        monitor->GetFlowStats();

    for (const auto &entry : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple =
            classifier->FindFlow(entry.first);

        // Count only application data flows entering the destination.
        if (tuple.destinationAddress != serverIp)
            continue;

        const FlowMonitor::FlowStats &s = entry.second;

        if (tuple.destinationPort == 5000 && tuple.protocol == 6)
        {
            tcpFlowRxBytes += s.rxBytes;
        }
        else if (tuple.destinationPort == 6000 && tuple.protocol == 17)
        {
            udpTxPackets += s.txPackets;
            udpRxPackets += s.rxPackets;
            udpDelaySumSeconds += s.delaySum.GetSeconds();
        }
    }

    Ptr<PacketSink> tcpSink =
        DynamicCast<PacketSink>(tcpSinkApp.Get(0));
    Ptr<PacketSink> udpSink =
        DynamicCast<PacketSink>(udpSinkApp.Get(0));

    const uint64_t tcpApplicationBytes = tcpSink->GetTotalRx();
    const uint64_t udpApplicationBytes = udpSink->GetTotalRx();

    const double measurementSeconds = APP_STOP - APP_START;
    const double tcpGoodputMbps =
        tcpApplicationBytes * 8.0 / measurementSeconds / 1e6;

    const double udpDeliveryPercent = udpTxPackets > 0
        ? 100.0 * udpRxPackets / udpTxPackets : 0.0;

    const double udpLossPercent = udpTxPackets > 0
        ? 100.0 * (udpTxPackets - udpRxPackets) / udpTxPackets : 0.0;

    const double udpMeanDelayMs = udpRxPackets > 0
        ? 1000.0 * udpDelaySumSeconds / udpRxPackets : 0.0;

    std::cout << std::fixed << std::setprecision(3);

    std::cout << "\n===== MEASURED RESULTS =====\n";
    if (enableTcp)
    {
        std::cout << "TCP application bytes received: "
                  << tcpApplicationBytes << "\n";
        std::cout << "TCP goodput over 18-second window: "
                  << tcpGoodputMbps << " Mbps\n";
        std::cout << "TCP FlowMonitor received bytes: "
                  << tcpFlowRxBytes << "\n";
        std::cout << "TCP completion time from application start: ";
        if (tcpCompletionSeconds >= 0.0)
            std::cout << tcpCompletionSeconds << " seconds\n";
        else
            std::cout << "not completed within the simulation\n";
    }
    else
    {
        std::cout << "TCP metrics: disabled for this scenario\n";
    }

    if (enableUdp)
    {
        std::cout << "UDP application bytes received: "
                  << udpApplicationBytes << "\n";
        std::cout << "UDP packets sent (FlowMonitor): "
                  << udpTxPackets << "\n";
        std::cout << "UDP packets received (FlowMonitor): "
                  << udpRxPackets << "\n";
        std::cout << "UDP delivery ratio: "
                  << udpDeliveryPercent << "%\n";
        std::cout << "UDP estimated loss ratio: "
                  << udpLossPercent << "%\n";
        std::cout << "UDP mean FlowMonitor delay: "
                  << udpMeanDelayMs << " ms\n";
    }
    else
    {
        std::cout << "UDP metrics: disabled for this scenario\n";
    }

    std::ofstream csv(resultsDir + "csv/" + prefix + ".csv");
    csv << "scenario,udp_rate,bottleneck_mbps,tcp_rx_bytes,"
           "tcp_goodput_mbps,tcp_completion_seconds,"
           "tcp_complete,udp_tx_packets,udp_rx_packets,"
           "udp_delivery_percent,udp_loss_percent,"
           "udp_mean_delay_ms\n";

    csv << scenario << ","
        << (enableUdp ? udpRate : "disabled") << ",5,"
        << tcpApplicationBytes << ","
        << tcpGoodputMbps << ","
        << tcpCompletionSeconds << ","
        << (tcpApplicationBytes >= TCP_EXPECTED_BYTES ? 1 : 0) << ","
        << udpTxPackets << ","
        << udpRxPackets << ","
        << udpDeliveryPercent << ","
        << udpLossPercent << ","
        << udpMeanDelayMs << "\n";

    csv.close();

    monitor->SerializeToXmlFile(resultsDir + "flowmon/" + prefix + "-flowmon.xml", true, true);

    std::cout << "\nCSV results: " << prefix << ".csv\n";
    std::cout << "PCAP prefix: " << prefix << "-bottleneck\n";
    std::cout << "FlowMonitor XML: " << prefix << "-flowmon.xml\n";
    std::cout << "Animation XML: " << prefix << "-animation.xml\n";

    Simulator::Destroy();
    return 0;
}
