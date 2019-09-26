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

#include "ns3/opengym-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project");

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
	std::cout << "ClientApp: " << m_totBytes << std::endl;
	
	if (m_nPackets == 0) {
		ScheduleTx ();
	} else {
		StopApplication ();
	}
}


void
ClientApp::ScheduleTx (void) {
	if (m_running) {
		uint32_t bits = m_packetSize * 8;
		Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
		m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
	}
}

int main (int argc, char ** argv)
{
	LogComponentEnable ("Project", LOG_LEVEL_INFO);

	/*-----	Create Nodes -----*/
	NS_LOG_INFO("Create Nodes.");
	NodeContainer n0, n1, n2;
	n0.Create (1);
	n1.Create (1);
	n2.Create (1);

	NodeContainer n0n1 (n0, n1);
	NodeContainer n1n2 (n1, n2);
	NodeContainer nodes (n0, n1, n2);
	
	/*-----	Create Channels	------*/
	NS_LOG_INFO("Create Channels.");
	PointToPointHelper bottleNeckLink;
	bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue ("1Gbps")); //DataRate
	bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (20))); //Propagation Delay
	//bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

	PointToPointHelper p2pLeaf;
	p2pLeaf.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
	p2pLeaf.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
	//p2pLeaf.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
	PointToPointDumbbellHelper dumble (1, p2pLeaf,
																		 1, p2pLeaf,
																		 bottleNeckLink);

	/*-----	Create Internet Stack ------*/
	NS_LOG_INFO ("Create Internet Stack.");
	InternetStackHelper stack;
	stack.InstallAll ();
	
	/*----- Create Queue Disc ------*/
	NS_LOG_INFO ("Create Queue Disc.");
	TrafficControlHelper tchFifo;
	tchFifo.SetRootQueueDisc ("ns3::FifoQueueDisc");
	//tchFifo.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("10p"));
	QueueDiscContainer qdisc1 = tchFifo.Install (dumble.GetLeft ()->GetDevice (0));
	
	/*------ Assign IP4 Address -----*/
	NS_LOG_INFO ("Assign IP4 Addresses.");
	dumble.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
															Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
															Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));


	/*----- Create Routes ------*/
	NS_LOG_INFO ("Create Routes.");
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	/*----- Create Server ------*/
	NS_LOG_INFO ("Create Server");
	uint16_t serverPort = 8080;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApps;
	
	for (uint32_t i = 0; i < dumble.RightCount (); ++i) {
		serverApps.Add (packetSinkHelper.Install (dumble.GetRight (i)));
	}
	serverApps.Start (Seconds(0.));
	serverApps.Stop (Seconds(60.));

	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	for (uint32_t i = 0; i < dumble.LeftCount (); ++i) {
		//InetSocketAddress serverAddress (dumble.GetRightIpv4Address (i), serverPort);
		Address serverAddress (InetSocketAddress (dumble.GetRightIpv4Address (i), serverPort));
		OnOffHelper onoff ("ns3::TcpSocketFactory", serverAddress);
		onoff.SetAttribute ("PacketSize", UintegerValue (1448));
		onoff.SetAttribute ("DataRate", StringValue ("50Mbps")); //bit/s
		ApplicationContainer clientApps = onoff.Install (dumble.GetLeft(i));

		clientApps.Start (Seconds (1.));
		clientApps.Stop (Seconds(59.));
		
	}

	// Configure and install ping
	V4PingHelper ping = V4PingHelper (dumble.GetLeftIpv4Address(0));
	ping.Install (n0);

	/*----- Ascii Tracer -----*/
	NS_LOG_INFO ("Tracer Enable");
	AsciiTraceHelper ascii;
	//p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
	bottleNeckLink.EnablePcapAll ("ns3_topology");

	Simulator::Stop (Seconds(60.1));

	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();

	/*----- QueueDisc Stats -----*/
	QueueDisc::Stats st = qdisc1.Get (0)->GetStats ();
	std::cout << st << std::endl;

	/*----- Total Rx Counts -----*/
	for (uint32_t i = 0; i < dumble.RightCount () ; ++i) {
		uint32_t totalRx = DynamicCast<PacketSink> (serverApps.Get (i))->GetTotalRx();
		std::cout << i << "th total RX: " << totalRx << std::endl;
	}
	
	Simulator::Destroy ();
	
	NS_LOG_INFO ("Done.");
}
