#include <string.h>
#include <cstdlib>
#include <ctime> 

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"
#include "ns3/error-model.h"
#include "ns3/internet-apps-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/opengym-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project");

// static void TestTracer (Ptr<RedQueueDisc>);

std::ofstream rttFile("Red-RTT.txt");
std::ofstream queueFile ("RedQueue-Size.txt");

static void PingRtt (std::string context, Time rtt)
{ 
  std::cout << Simulator::Now (). GetSeconds() << " RTT = " << rtt.GetMilliSeconds () << " ms" << std::endl;
  rttFile << context << "=" << rtt.GetMilliSeconds () << " ms" << std::endl;
}
class ClientApp : public Application 
{
public:
	ClientApp ();
	virtual ~ClientApp();

	static TypeId GetTypeId (void);
	void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
	virtual void StartApplication (void);
	virtual void StopApplication (void);

	void SendPacket (void);
	void ScheduleTx (void);

	Ptr<Socket>	m_socket;
	Address			m_peer;
	uint32_t		m_packetSize;
	uint32_t		m_nPackets;
	DataRate		m_dataRate;
	EventId			m_sendEvent;
	bool				m_running;
	uint32_t		m_packetsSent;
	uint32_t	  m_totBytes;
};

ClientApp::ClientApp ()
	: m_socket(0),
	  m_peer (),
		m_packetSize (0),
		m_nPackets (0),
		m_dataRate (0),
		m_sendEvent (),
		m_running (false),
		m_packetsSent (0),
		m_totBytes (0)
{
}

ClientApp::~ClientApp (){
	m_socket = 0;
}

TypeId ClientApp::GetTypeId (void) {
	static TypeId tid = TypeId ("ClientApp")
		.SetParent<Application> ()
		.SetGroupName ("Project")
		.AddConstructor<ClientApp> ()
		;
		return tid;
}

void
ClientApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate) {
	m_socket = socket;
	m_peer = address;
	m_packetSize = packetSize;
	m_nPackets = nPackets;
	m_dataRate = dataRate;
}

void
ClientApp::StartApplication (void) {
	m_running = true;
	m_packetsSent = 0;
	m_socket->Bind ();
	m_socket->Connect (m_peer);
	m_socket->ShutdownRecv ();

	SendPacket();
}

void
ClientApp::StopApplication (void) {
	m_running = false;

	if (m_sendEvent.IsRunning ()) {
		Simulator::Cancel (m_sendEvent);
	}

	if (m_socket) {
		m_socket->Close ();
	}
}

void
ClientApp::SendPacket (void) {
	Ptr<Packet> packet = Create<Packet> (m_packetSize);
	m_socket->Send (packet);
	m_totBytes += packet->GetSize ();
	
	//std::cout << "ClientApp: " << m_totBytes << std::endl;
	//if(++m_packetsSent < m_nPackets) {
	//	ScheduleTx ();
	//}

	if (m_nPackets == 0) {
		ScheduleTx ();
	} else {
		StopApplication ();
	}
}

void
ClientApp::ScheduleTx (void) {
	if (m_running) {
		// uint32_t bits = m_packetSize * 8;
		// Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
		Time tNext (NanoSeconds (std::floor(std::rand() % 10000) + 10000));
		//std::cout << "term: " << tNext << std::endl;
		m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
	}
}

int main (int argc, char ** argv)
{
	LogComponentEnable ("Project", LOG_LEVEL_INFO);
	
	//LogComponentEnable ("TcpSocketBase", LOG_LEVEL_ALL);
	//LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);
	//LogComponentEnable ("QueueDisc", LOG_LEVEL_ALL);
	//LogComponentEnable ("TcpTxBuffer", LOG_LEVEL_ALL);	
	//LogComponentEnable ("DropTailQueue", LOG_LEVEL_ALL);
	
	std::srand ((unsigned int) (std::time(NULL)));
	
	/*----- ZMQ Connect -----*/
	NS_LOG_INFO("ZMQ Connect");
    
	/*----- Error Model -----*/
	NS_LOG_INFO("Error Model");
	
	/*-----	Create Nodes -----*/
	NS_LOG_INFO("Create Nodes.");
	NodeContainer n0, n1, n2, n3, n4;
	n0.Create (1);
	n1.Create (1);
	n2.Create (1);
	n3.Create (1); // router 
	n4.Create (1); // server
	
	NodeContainer n0n3 (n0, n3);
	NodeContainer n1n3 (n1, n3);
	NodeContainer n2n3 (n2, n3);
	NodeContainer n3n4 (n3, n4);
	NodeContainer clientNodes (n0, n1, n2);
	NodeContainer nodes (n0, n1, n2, n3, n4);
	
	/*-----	Create Channels	------*/
	NS_LOG_INFO("Create Channels.");
	PointToPointHelper bottleNeckLink;
	bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue ("1Gbps")); //DataRate
	bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0))); //Propagation Delay
	//bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
	NetDeviceContainer deviceBottleneckLink = bottleNeckLink.Install (n3n4);
	
	PointToPointHelper accessLink;
	accessLink.SetDeviceAttribute ("DataRate", StringValue ("500Mbps"));
	accessLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0)));
	//accessLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
	NetDeviceContainer deviceAccessLink0 = accessLink.Install (n0n3);
	NetDeviceContainer deviceAccessLink1 = accessLink.Install (n1n3);
	NetDeviceContainer deviceAccessLink2 = accessLink.Install (n2n3);
	
	/*-----	Create Internet Stack ------*/
	NS_LOG_INFO ("Create Internet Stack.");
	InternetStackHelper stack;
	stack.InstallAll ();
	
	/*----- Create Queue Disc ------*/
	NS_LOG_INFO ("Create Queue Disc.");
	TrafficControlHelper tchFifo;
	tchFifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", StringValue ("120p"));
	QueueDiscContainer qdisc = tchFifo.Install (deviceBottleneckLink);
	Ptr<QueueDisc> q = qdisc.Get (0);
	Ptr<RedQueueDisc> fq = DynamicCast<RedQueueDisc> (q);
	// fq->TraceConnectWithoutContext ("Test", MakeCallback (&TestTracer));
	
	/*------ Assign IP4 Address -----*/
	NS_LOG_INFO ("Assign IP4 Addresses.");
	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer clientInterfaces;
	clientInterfaces = address.Assign (deviceAccessLink0);
	address.NewNetwork ();
	clientInterfaces = address.Assign (deviceAccessLink1);
	address.NewNetwork ();	
	clientInterfaces = address.Assign (deviceAccessLink2);
	address.NewNetwork ();
	
	address.SetBase ("10.2.1.0", "255.255.255.0");
	Ipv4InterfaceContainer serverInterfaces;
	serverInterfaces = address.Assign (deviceBottleneckLink);
	
	/*----- Create Routes ------*/
	NS_LOG_INFO ("Create Routes.");
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	/*----- Create Server ------*/
	NS_LOG_INFO ("Create Server");
	uint16_t serverPort = 8080;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApps;
	serverApps.Add (packetSinkHelper.Install (n4));
	serverApps.Start (Seconds(0.));
	serverApps.Stop (Seconds(1.));

	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	std::vector <uint8_t> tosValues = {0x28, 0xb8, 0x70}; //AC_BE, AC_BK, AC_VI
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1024));
	for (uint32_t i = 0; i < 3; ++i) {
		InetSocketAddress serverAddress (serverInterfaces.GetAddress (1), serverPort);
		serverAddress.SetTos (tosValues.at(i));
		Ptr<Socket> clientSocket = Socket::CreateSocket (clientNodes.Get(i), tid);
		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (clientSocket, serverAddress, 1024, 0, DataRate ("500Mbps"));
		clientNodes.Get (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (0.1));
		clientApps->SetStopTime (Seconds (1.1));
		
		//Address serverAddress (InetSocketAddress (serverInterfaces.GetAddress (1), serverPort));
		
		//OnOffHelper onoff ("ns3::TcpSocketFactory", serverAddress);
		//onoff.SetAttribute ("PacketSize", UintegerValue (1024));
		//onoff.SetAttribute ("DataRate", StringValue ("500Mbps")); //bit/s
		//ApplicationContainer clientApps = onoff.Install (clientNodes.Get (i));

		//clientApps.Start (Seconds (1.));
		//clientApps.Stop (Seconds(9.));
	}
	
	/*----- Configure ping -----*/
	V4PingHelper ping = V4PingHelper (serverInterfaces.GetAddress(1));
	ping.Install (n0);
	ping.Install (n1);
	ping.Install (n2);
	Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));
	
	/*----- Ascii Tracer -----*/
	NS_LOG_INFO ("Tracer Enable");
	AsciiTraceHelper ascii;
	//p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
	bottleNeckLink.EnablePcapAll ("ns3_topology");
	
	/*----- Create FlowMonitor -----*/
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	Simulator::Stop (Seconds(12));
	
	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();
	
	/*----- Flow Monitor -----*/
	Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (serverApps.Get (0));
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
		// if(t.sourceAddress == "10.1.1.1"){
			// std::cout << "  Goodput: "    << sink1->GetTotalRx() * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
	  // }
		std::cout << "  First Packet Rx: " << i->second.timeFirstRxPacket.GetSeconds() << "sec\n";
		std::cout << "  Last Packet Rx: " << i->second.timeLastRxPacket.GetSeconds() << "sec\n";
    }
	
	/*----- QueueDisc Stats -----*/
	QueueDisc::Stats st = qdisc.Get (0)->GetStats ();
	std::cout << st << std::endl;

	/*----- Total Rx Counts -----*/
	uint32_t totalRx = DynamicCast<PacketSink> (serverApps.Get (0))->GetTotalRx();
	std::cout << "th total RX: " << totalRx << std::endl;
	
	
	Simulator::Destroy ();
	
	NS_LOG_INFO ("Done.");
}

// static void TestTracer (Ptr<RedQueueDisc> fq) {
	// std::cout << fq->GetInternalQueue (0)-> GetCurrentSize (). GetValue () << " ";
	// queueFile << fq->GetInternalQueue (0)-> GetCurrentSize (). GetValue ()  << " ";
// }
