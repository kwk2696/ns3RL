#include <string>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/packet-sink.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RouterTest");

void ReceivePacket (Ptr<Socket> socket) {
  std::cout << "Recieved packet!" << std::endl;
  //Ptr<Packet> packet = socket->Recv ();
}

static void SendPacket (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval){
	if(pktCount > 0) {
		socket -> Send (Create<Packet> (pktSize));
		Simulator::Schedule (pktInterval, &SendPacket,
							 socket, pktSize, pktCount - 1, pktInterval);
	}
	else {
		socket->Close ();
	}
}

int main (int argc, char** argv)
{
  uint32_t packetSize = 1024;
  uint32_t packetCount = 1;
  double packetInterval = 0.5; //sec
  
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  // LogComponentEnable ("TcpSocketBase", LOG_LEVEL_ALL);
  // LogComponentEnable ("TcpTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable ("TcpRxBuffer", LOG_LEVEL_FUNCTION);
  // LogComponentEnable ("TcpL4Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_INFO);
  
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
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));
  NetDeviceContainer d1 = csma.Install (net1);
  NetDeviceContainer d2 = csma.Install (net2);
  

  
  NS_LOG_INFO ("Create Internet Stack.");
  InternetStackHelper stack;
  stack.Install(all);
  
  //
  // TrafficControlHelper tch; //define the discipline for queue
  // tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("2p"));
	
  // QueueDiscContainer qdiscs = tch.Install (d1); //creates QueueDiscs for each devices in the container
  // QueueDiscContainer qdiscs2 = tch.Install (d2);
  //
  
  NS_LOG_INFO ("Assign IP4 Adresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_net1 = ipv4.Assign (d1);  
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_net2 = ipv4.Assign (d2); 
  
  NS_LOG_INFO ("Create Routes.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables(); 
  
  NS_LOG_INFO ("Create Sockets.");
  uint16_t port = 9;
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  // Receiver socket on n1
  Ptr<Socket> server_rc = Socket::CreateSocket (all.Get (2), tid);
  server_rc->Bind (InetSocketAddress (Ipv4Address::GetAny (), port));
  server_rc->Listen ();
  //server_rc->ShutdownSend ();
  server_rc->SetRecvCallback (MakeCallback (&ReceivePacket));
  
  // Sender socket on n0
  Ptr<Socket> source = Socket::CreateSocket (all.Get (0), tid);
  source->Bind ();
  NS_LOG_INFO("check");
  source->Connect (InetSocketAddress (iface_net2.GetAddress (1), port));
  NS_LOG_INFO("check");
  //source->ShutdownRecv ();
  
  NS_LOG_INFO("Ascii.");
  AsciiTraceHelper ascii;
  //csma.EnableAsciiAll (ascii.CreateFileStream ("TCP-routing.tr"));
  csma.EnablePcapAll ("TCP-routing", true);
  
  // Schedule Send Packet
  Time interPacketInterval = Seconds (packetInterval);
  Simulator::ScheduleWithContext (source->GetNode ()-> GetId(),
								  Seconds (1.0), &SendPacket,
								  source, packetSize, packetCount,interPacketInterval);
								
  
  NS_LOG_INFO ("Run Simulation");
  Simulator::Run();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
  
