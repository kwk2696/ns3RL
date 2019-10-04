#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project");

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
		}
	}
}

int
main (int argc, char *argv[])
{
	LogComponentEnable ("Project", LOG_LEVEL_INFO);

	uint32_t nWifi = 3;
	uint32_t segment_size = 1024;
	uint32_t sim_time = 4;
	bool tracing = false;
	
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (segment_size));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1)); //one ACK during timeout period
	
	/*-----	Create P2P	------*/
	NS_LOG_INFO ("Create P2P");
	NodeContainer p2pNodes;
	p2pNodes.Create (2);
	
	PointToPointHelper p2pHelper;
	p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
	p2pHelper.SetChannelAttribute ("Delay", StringValue ("0ms"));

	NetDeviceContainer p2pDevices;
	p2pDevices = p2pHelper.Install (p2pNodes);

	/*-----	Create Wifi ------*/
	NS_LOG_INFO ("Create Wifi");
	NodeContainer wifiStaNodes;
	wifiStaNodes.Create (nWifi);
	NodeContainer wifiApNode = p2pNodes.Get (0); //Ap

	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetChannel (channel.Create ()); 
	
	WifiHelper wifi;
	wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
	//wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
	
	WifiMacHelper mac;
	Ssid ssid = Ssid ("ns-3-ssid");
	mac.SetType ("ns3::StaWifiMac",
							 "Ssid", SsidValue (ssid),
							 "ActiveProbing", BooleanValue (false));
	
	NetDeviceContainer staDevices;
	staDevices = wifi.Install (phy, mac, wifiStaNodes);

	mac.SetType ("ns3::ApWifiMac",
							 "Ssid", SsidValue (ssid));
	
	NetDeviceContainer apDevice;
	apDevice = wifi.Install (phy, mac, wifiApNode);

	/*----- Mobility model -----*/
	MobilityHelper mobility;
	mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

	mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
	mobility.Install (wifiStaNodes);
	
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (wifiApNode);
	
	/*-----	Create Internet Stack ------*/
	NS_LOG_INFO ("Create Interent Stack");
	InternetStackHelper stack;
	stack.InstallAll ();

	/*------ Assign IP4 Address -----*/
	NS_LOG_INFO ("Create Ipv4 Address");
	Ipv4AddressHelper address;

	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer p2pInterfaces;
	p2pInterfaces = address.Assign (p2pDevices);

	address.SetBase ("10.1.2.0", "255.255.255.0");
	address.Assign (staDevices);
	address.Assign (apDevice);

	/*----- Create Routes ------*/
	NS_LOG_INFO ("Create Routes.");
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	
	/*----- Create Server ------*/
	NS_LOG_INFO("Create Server");
	uint16_t serverPort = 9;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApp = sinkHelper.Install (p2pNodes.Get (1));
	serverApp.Start (Seconds (0.0));
	serverApp.Stop (Seconds (sim_time));
	
	/*----- Create Client ------*/
	NS_LOG_INFO("Create Client");
	InetSocketAddress serverAddress (p2pInterfaces.GetAddress (1), serverPort);
	for (uint32_t i = 0; i < nWifi; i++) {
		Ptr<Socket> sock = Socket::CreateSocket (wifiStaNodes.Get (i), tid);
		
		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (sock, serverAddress, segment_size, 0, DataRate("10Mbps"), false, 0);
		wifiStaNodes.Get (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (1.0));
		clientApps->SetStopTime (Seconds (sim_time));
	}
	
	/*----- Pcap Tracer -----*/
	if (tracing == true)
    {
      p2pHelper.EnablePcapAll ("p1_wifi");
      phy.EnablePcap ("p1_wifi", apDevice.Get (0));
    }
	
	/*----- Create FlowMonitor -----*/
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	
	Simulator::Stop (Seconds (sim_time + 1.0));
	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();
	
	/*----- Flow Monitor -----*/
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
	return 0;
}
