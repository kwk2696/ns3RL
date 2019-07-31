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

void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t, uint32_t, uint32_t, Time);
static void SendPacket (Ptr<Socket>, uint32_t, uint32_t, Time); 
void ReceivePacket (Ptr<Socket>); 

void
queuedisctrace0 (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "******QueueDisc0: " << oldValue << " to " << newValue << std::endl;
}

void
queuedisctrace1 (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "******QueueDisc1: " << oldValue << " to " << newValue << std::endl;
}

void
queuetrace0(uint32_t oldValue, uint32_t newValue)
{
  std::cout << "******Queue0: " << oldValue << " to " << newValue << std::endl;
}

void
queuetrace1(uint32_t oldValue, uint32_t newValue)
{
  std::cout << "******Queue1: " << oldValue << " to " << newValue << std::endl;
}

void
queuediscEnqueue (Ptr<const QueueDiscItem> item) 
{
	std::cout << "QueueDiscEnqueue: " << item->GetPacket () << std::endl;
}

int main (int argc, char** argv)
{
  uint32_t packetSize = 1024;
  uint32_t packetCount = 1;
  double packetInterval = 0.5; //sec
  
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  LogComponentEnable ("RouterTest", LOG_LEVEL_ALL);
  //LogComponentEnable ("NetDeviceQueueInterface", LOG_LEVEL_ALL);
  
  // LogComponentEnable ("TcpSocketBase", LOG_LEVEL_ALL);
  // LogComponentEnable ("TcpTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable ("TcpRxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable ("QueueDisc", LOG_LEVEL_ALL);
  // LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);
  
  // LogComponentEnable ("DropTailQueue", LOG_LEVEL_ALL);
  //LogComponentEnable ("TcpL4Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_INFO);
  //LogComponentEnable ("Ipv4ListRouting", LOG_LEVEL_ALL);
  
  
  NS_LOG_INFO ("Create Nodes.");
  NodeContainer n0;
  n0.Create(1);
  NodeContainer n1;
  n1.Create(1);
  
  NodeContainer net(n0, n1);
  
  NS_LOG_INFO ("Create Channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute ("Delay", TimeValue(MilliSeconds(5)));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
  NetDeviceContainer netdevice = p2p.Install (net);
  
  NS_LOG_INFO ("Create Internet Stack.");
  InternetStackHelper stack;
  stack.Install(net);

  // discipline for queue	
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("2p"));
  QueueDiscContainer qdiscs = tch.Install (netdevice);
  
  //trace queue_discs
  Ptr<QueueDisc> q0 = qdiscs.Get(0);
  q0->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&queuedisctrace0));
  q0->TraceConnectWithoutContext ("Enqueue", MakeCallback (&queuediscEnqueue));
  Ptr<QueueDisc> q1 = qdiscs.Get(1);
  q1->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&queuedisctrace1));
  
  //trace queue
  Ptr<NetDevice> nd0 = netdevice.Get(0);
  Ptr<PointToPointNetDevice> ptpnd0 = DynamicCast<PointToPointNetDevice> (nd0);
  Ptr<Queue<Packet>> queue0 = ptpnd0->GetQueue ();
  queue0->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&queuetrace0));
  Ptr<NetDevice> nd1 = netdevice.Get(1);
  Ptr<PointToPointNetDevice> ptpnd1 = DynamicCast<PointToPointNetDevice> (nd1);
  Ptr<Queue<Packet>> queue1 = ptpnd1->GetQueue ();
  queue1->TraceConnectWithoutContext("PacketsInQueue", MakeCallback (&queuetrace1));
  
  
  NS_LOG_INFO ("Assign IP4 Adresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface = ipv4.Assign (netdevice); 
  
  NS_LOG_INFO ("Create Routes.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables(); 
  
  NS_LOG_INFO ("Create Sockets.");
  uint16_t port = 9;
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  // Receiver socket on n1
  Ptr<Socket> server_rc = Socket::CreateSocket (net.Get (1), tid);
  server_rc->Bind (InetSocketAddress (Ipv4Address::GetAny (), port));
  server_rc->Listen ();
 // server_rc->ShutdownSend ();
  server_rc->SetRecvCallback (MakeCallback (&ReceivePacket));
  
  // Sender socket on n0
  Ptr<Socket> source = Socket::CreateSocket (net.Get (0), tid);
  source->Bind ();
  
  NS_LOG_INFO("Ascii.");
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("TCP-routing.tr"));
  p2p.EnablePcapAll ("TCP-routing", true);
  
  // Schedule Send Packet
  Time interPacketInterval = Seconds (packetInterval);		
				
  Simulator::Schedule (Seconds(0.1), &StartFlow, source, iface.GetAddress (1), port,
					   packetSize, packetCount, interPacketInterval); 								
  
  NS_LOG_INFO ("Run Simulation");
  Simulator::Run();
  
  QueueDisc::Stats st1 = qdiscs.Get(0)->GetStats();
  QueueDisc::Stats st2 = qdiscs.Get(1)->GetStats();
  std::cout << st1 << std:: endl;
  std::cout << st2 << std:: endl;
  
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

void StartFlow (Ptr<Socket> socket, Ipv4Address servAddress,
                        uint16_t servPort, uint32_t pktSize, 
						uint32_t pktCount, Time pktInterval){
							
	socket->Connect (InetSocketAddress (servAddress, servPort));	
    //socket->ShutdownRecv ();
	
	
    SendPacket(socket, pktSize, pktCount, pktInterval);
}

static void SendPacket(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval){
	if(pktCount > 0) {
		socket -> Send (Create<Packet> (pktSize));
		Simulator::Schedule (pktInterval, &SendPacket,
							 socket, pktSize, pktCount - 1, pktInterval);
	}
	else {
		socket->Close ();
	}
}


void ReceivePacket (Ptr<Socket> socket) {
    
  Ptr<Packet> packet;
  Address from;
  
  while((packet = socket->RecvFrom(from))){
      if (packet->GetSize () == 0) {
          break;
      }
      std::cout << "Recv Packt!" << std::endl;
  }
  
  // NS_LOG_INFO ("Received one packet!");
  // std::cout << "Recieved packet!" << std::endl;
  // Ptr<Packet> packet = socket->Recv ();
}
