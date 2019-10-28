#include <string.h>
#include <cstdlib>
#include <ctime> 
#include <list>

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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RouterTest");

class ClientApp : public Application 
{
public:
	ClientApp ();
	virtual ~ClientApp();

	static TypeId GetTypeId (void);
	void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, bool random, uint32_t ranRate);

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
	bool			m_running;
	bool			m_random;
	uint32_t		m_ranRate;
	uint32_t		m_packetsSent;
	uint32_t	 	m_totBytes;
};

ClientApp::ClientApp ()
	: 	m_socket(0),
		m_peer (),
		m_packetSize (0),
		m_nPackets (0),
		m_dataRate (0),
		m_sendEvent (),
		m_running (false),
		m_random (false),
		m_ranRate (0),
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
ClientApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, bool random, uint32_t ranRate) {
	m_socket = socket;
	m_peer = address;
	m_packetSize = packetSize;
	m_nPackets = nPackets;
	m_dataRate = dataRate;
	m_random = random;
	m_ranRate = ranRate; 
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

	if (m_nPackets == 0 || ++m_packetsSent < m_nPackets) {
		ScheduleTx ();
	} else {
		std::cout << "stop 0" << std::endl;
		StopApplication ();
	}
}
void
ClientApp::ScheduleTx (void) {
	if (m_running) {
		if (m_random) {
			Time tNext (NanoSeconds ((std::rand() % 1000000 + m_ranRate))); //1000000
			m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
		}	
		else {
			uint32_t bits = m_packetSize * 8;
			Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
			m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
			// std::cout << "tNext: " << tNext << std::endl;
		}
		// std::cout << "tNext: " << tNext << std::endl;
	}
}

int main (int argc, char** argv)
{
	LogComponentEnable ("RouterTest", LOG_LEVEL_INFO);
	std::srand ((unsigned int) (std::time(NULL)));
	uint32_t sim_time = 4;
	int m_segsize = 1024;
  
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
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute ("Delay", TimeValue(MilliSeconds(0)));
  NetDeviceContainer d1 = p2p.Install (net1);
  NetDeviceContainer d2 = p2p.Install (net2);
  
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
  
	NS_LOG_INFO ("Create Sockets.");
	uint16_t serverPort = 8080;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApps;
	serverApps.Add (packetSinkHelper.Install (n1));
	serverApps.Start (Seconds(0.));
	serverApps.Stop (Seconds(sim_time + 1));
  
	InetSocketAddress serverAddress (iface_net2.GetAddress (1), serverPort);
	Ptr<Socket> clientSocket = Socket::CreateSocket (n0.Get(0), tid);

	Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
	clientApps->Setup (clientSocket, serverAddress, m_segsize, 0, DataRate ("10Mbps"), true, 4000000);
	n0.Get(0)->AddApplication (clientApps);
		
	clientApps->SetStartTime (Seconds (0.1));
	clientApps->SetStopTime (Seconds (sim_time + 0.1));			

	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	
	Simulator::Stop (Seconds(sim_time + 2));
	
	NS_LOG_INFO ("Run Simulation");
	Simulator::Run();
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
		std::cout << "  First Packet Rx: " << i->second.timeFirstRxPacket.GetSeconds() << "sec\n";
		std::cout << "  Last Packet Rx: " << i->second.timeLastRxPacket.GetSeconds() << "sec\n";
    }
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
}
  
