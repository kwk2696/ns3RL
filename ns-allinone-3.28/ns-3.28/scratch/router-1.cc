#include <string>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/packet-sink.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RouterTest");

std::ofstream rttFile("router-RTT.txt");

static void PingRtt (std::string context, Time rtt)
{  
  std::cout << context << "=" << rtt.GetMilliSeconds () << " ms" << std::endl;
  rttFile << context << "=" << rtt.GetMilliSeconds () << " ms" << std::endl;
}

static void
GoodputSampling (std::string fileName, ApplicationContainer app, Ptr<OutputStreamWrapper> stream, float period)
{
  Simulator::Schedule (Seconds (period), &GoodputSampling, fileName, app, stream, period);
  double goodput;
  uint32_t totalPackets = DynamicCast<PacketSink> (app.Get (0))->GetTotalRx ();
  goodput = totalPackets * 8 / (Simulator::Now ().GetSeconds () * 1024); // Kbit/s
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << goodput << std::endl;
}

int main (int argc, char** argv)
{
  uint32_t maxBytes = 5000000;
  
  float startTime = 0.1; // in sec
  float simDuration = 60;
  float samplingPeriod = 1;
  float stopTime = startTime + simDuration; 
  
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  NS_LOG_INFO ("Create Nodes.");
  NodeContainer n0;
  n0.Create(1);
  NodeContainer r;
  r.Create(1);
  NodeContainer n1;
  n1.Create(1);
  
  NodeContainer net1 (n0, r);
  NodeContainer net2 (r, n1);
  NodeContainer all (n0, r, n1);
  
  NS_LOG_INFO ("Create Channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue(MilliSeconds(2)));
  NetDeviceContainer d1 = csma.Install (net1);
  NetDeviceContainer d2 = csma.Install (net2);
  
  NS_LOG_INFO ("Create Internet Stack.");
  InternetStackHelper stack;
  stack.Install(all);
  
  NS_LOG_INFO ("Assign IP4 Adresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_net1 = ipv4.Assign (d1);  
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_net2 = ipv4.Assign (d2); 
  
  NS_LOG_INFO ("Create Routes.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables(); 
  
  NS_LOG_INFO ("Create Application.");
  uint16_t port = 9;
  
  //Create a packet source
  BulkSendHelper source ("ns3::TcpSocketFactory",
                          InetSocketAddress (iface_net2.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes)); // Set the amount of data to send in bytes.  Zero is unlimited.
  ApplicationContainer sourceApps = source.Install (all.Get (0));
  
  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer sinkApps = sink.Install (all.Get(2));
  

  // Configure and install ping
  V4PingHelper ping = V4PingHelper (iface_net2.GetAddress(1));
  ping.Install (n0);
  
  // Print Ping RTT
  
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));
  
  sourceApps.Start (Seconds (0));
  sourceApps.Stop (Seconds (stopTime - 0.1));
  sinkApps.Start (Seconds (0));
  sinkApps.Stop (Seconds (stopTime));
  
  
  AsciiTraceHelper ascii;
  
  // Ouput trace, pcap file
  csma.EnableAsciiAll (ascii.CreateFileStream ("router.tr"));
  csma.EnablePcapAll ("router");
  
  // Output Goodput from Sink Application
  Ptr<OutputStreamWrapper> uploadGoodputStream = ascii.CreateFileStream ("router-Goodput.txt");
  Simulator::Schedule (Seconds (samplingPeriod), &GoodputSampling, "router-Goodput.txt", sinkApps,
                       uploadGoodputStream, samplingPeriod);
  					   
  // Create Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
					   
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
  
  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes App Recieved: " << sink1->GetTotalRx() << "bytes\n\n";
  
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
	  if(t.sourceAddress == "10.1.1.1"){
		  std::cout << "  Goodput: "    << sink1->GetTotalRx() * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
	  }
	  std::cout << "  First Packet Rx: " << i->second.timeFirstRxPacket.GetSeconds() << "sec\n";
	  std::cout << "  Last Packet Rx: " << i->second.timeLastRxPacket.GetSeconds() << "sec\n";
    }
	
  rttFile.close();  
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

}