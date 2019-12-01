#include <cmath>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/opengym-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project");

Ptr<OpenGymInterface> openGymInterface;

uint32_t nWifi = 3;
uint32_t sim_time = 10;	
uint32_t segment_size = 1024;
float avgRtt[3] = {0};
float cntRtt[3] = {0};
float interval[3] = {0.001,0.001,0.001};

// count %
float cnt[3] = {0};
float com[3] = {0};

// standard RTT
float rtt_0 = 0.001;
float rtt_1 = 0.005;
float rtt_2 = 0.02;

bool setRL = false;
float check_time = 0.005;

static void TxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<TcpSocketBase>);
static void RxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<TcpSocketBase>);
static void SetInterval();
static void PrintTime () {
	std::cout << Simulator::Now().GetSeconds() << std::endl;
	Simulator::Schedule(Seconds(check_time), &PrintTime);
}

class ClientApp : public Application 
{
public:
	ClientApp ();
	virtual ~ClientApp();

	static TypeId GetTypeId (void);
	void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, uint32_t clientNum);

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
    uint32_t        m_clientNum;
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
		m_totBytes (0),
        m_clientNum (0)
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
ClientApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate, uint32_t clientNum) {
	m_socket = socket;
	m_peer = address;
	m_packetSize = packetSize;
	m_nPackets = nPackets;
	m_dataRate = dataRate;
    m_clientNum = clientNum;
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
		// uint32_t bits = m_packetSize * 8;
        // Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
       
        // float t;
        // if (m_clientNum == 0) t = interval[0];
        // if (m_clientNum == 1) t = interval[1];
        // if (m_clientNum == 2) t = interval[2];
        
        Time tNext(Seconds(0.0022));      
		m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
	}
}


int
main (int argc, char *argv[])
{
    LogComponentEnable ("Project", LOG_LEVEL_INFO);
    
    // while(1) {
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (segment_size));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
    
    for (uint16_t i = 0; i < 3; i++) {
        avgRtt[i] = 0;
        cntRtt[i] = 0;
        interval[i] = 0.001;
        cnt[i] = 0;
        com[i] = 0;
    }
    
    /*-----	Create Nodes -----*/
	NS_LOG_INFO("Create Nodes.");
	NodeContainer p2pNodes;
	p2pNodes.Create (2);
	
	/*-----	Create P2P	------*/
	NS_LOG_INFO ("Create P2P");
	PointToPointHelper p2pHelper;
	p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
	p2pHelper.SetChannelAttribute ("Delay", StringValue ("0ms"));

	NetDeviceContainer p2pDevices = p2pHelper.Install (p2pNodes);

	/*-----	Create Wifi ------*/
	NS_LOG_INFO ("Create Wifi");
	NodeContainer wifiStaNodes;
	wifiStaNodes.Create (nWifi);
	NodeContainer wifiApNode = p2pNodes.Get (0);

	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetChannel (channel.Create ()); 
	
	WifiHelper wifi;
	wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");
	// wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
	wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
	
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
	NS_LOG_INFO ("Create Mobility");
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
	
	address.SetBase ("10.2.1.0", "255.255.255.0");
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
	std::vector <uint32_t> ranRate = {1000000, 2000000, 4000000};
	InetSocketAddress serverAddress (p2pInterfaces.GetAddress (1), serverPort);
	for (uint32_t i = 0; i < nWifi; i++) {
		Ptr<Socket> sock = Socket::CreateSocket (wifiStaNodes.Get (i), tid);
		Ptr<TcpSocket> _tcpsock = DynamicCast<TcpSocket> (sock);
        Ptr<TcpSocketBase> _tcpsocketbase = DynamicCast<TcpSocketBase> (_tcpsock);
        _tcpsocketbase->TraceConnectWithoutContext ("RecordTx", MakeCallback (&TxTracer));
		_tcpsocketbase->TraceConnectWithoutContext ("RecordRx", MakeCallback (&RxTracer));
        _tcpsocketbase->_tag = i;
        
		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (sock, serverAddress, segment_size, 0, DataRate("8Mbps"), i);
		wifiStaNodes.Get (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (1.0));
		clientApps->SetStopTime (Seconds (sim_time));
	}
    
    if (setRL) {
        Simulator::Schedule (Seconds(1.0), &SetInterval);
    }
    if (true) {
		Simulator::Schedule (Seconds(1.0), &PrintTime);
	}
    
    /*----- Create FlowMonitor -----*/
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	
	Simulator::Stop (Seconds (sim_time + 0.11));
	

	/*----- Run Sim -----*/
	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();
	
	/*----- Flow Monitor -----*/
	Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (serverApp.Get (0));
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
	
    std::cout << "Complete percent client0: " << (com[0]/cnt[0]) * 100 << "%" << std::endl;
    std::cout << "Complete percent client1: " << (com[1]/cnt[1]) * 100 << "%" << std::endl;
    std::cout << "Complete percent client2: " << (com[2]/cnt[2]) * 100 << "%" << std::endl;
    
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
    // }
	return 0;
}

static void TxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<TcpSocketBase> s) {
	uint8_t flags = h.GetFlags ();
	bool hasSyn = flags & TcpHeader::SYN;
	bool hasFin = flags & TcpHeader::FIN;
	
	SequenceNumber32 sq = h.GetSequenceNumber ();
	if (!hasSyn && !hasFin && p->GetSize ()) {
		s->_sendTime.insert(std::make_pair (sq+p->GetSize (), Simulator::Now ().GetSeconds ()));
	}
    
    std::cout << s->_tag << "send: " << sq+p->GetSize () << std::endl;
}

static void RxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<TcpSocketBase> s) {
	uint8_t flags = h.GetFlags ();
	bool hasSyn = flags & TcpHeader::SYN;
	bool hasFin = flags & TcpHeader::FIN;
		
	SequenceNumber32 ack = h.GetAckNumber ();
	if(hasSyn && hasFin) return;
	
    
	s->_rttCur = Simulator::Now ().GetSeconds () - s->_sendTime.find (ack) -> second;
    std::cout << s->_tag << "ack: " << ack << " seq: " << s->_sendTime.find (ack)->first << " : " << s->_rttCur << std::endl;
    
    cntRtt[s->_tag] += 1;
    avgRtt[s->_tag] = avgRtt[s->_tag] + (1/cntRtt[s->_tag]) *  (s->_rttCur - avgRtt[s->_tag]);  
    
    // count success 
    cnt[s->_tag] += 1;
    if(s->_tag == 0 && s->_rttCur < rtt_0)
        com[s->_tag] += 1;
    if(s->_tag == 1 && s->_rttCur < rtt_1)
        com[s->_tag] += 1;
    if(s->_tag == 2 && s->_rttCur < rtt_2)
        com[s->_tag] += 1;
}

static void SetInterval(){
    // Calculate Reward
    float reward0 = (rtt_0 - avgRtt[0]) / rtt_0;
    float reward1 = (rtt_1 - avgRtt[1]) / rtt_1;
    float reward2 = (rtt_2 - avgRtt[2]) / rtt_2;
    
    float reward = 0.3333 * reward0 + 0.3333 * reward1 + 0.3333 * reward2;
    
    
    std::string message = "{\"state00\":" + std::to_string((interval[0] - 0.0005)/(0.001)) + ",\"state01\":" + std::to_string(avgRtt[0]/10.0) 
						+ ",\"state10\":" + std::to_string((interval[1] - 0.0005)/(0.001)) + ",\"state11\":" + std::to_string(avgRtt[1]/10.0) 
						+ ",\"state20\":" + std::to_string((interval[2] - 0.0005)/(0.001)) + ",\"state21\":" + std::to_string(avgRtt[2]/10.0) 
						+ ",\"reward\":" + std::to_string(reward) + "}";
    // std::cout << message << std::endl;     

    openGymInterface->Send(message);
    
    if(Simulator::Now ().GetSeconds() == sim_time + 0.1) {
		openGymInterface->SendEnd(1);
		return;
	} 
	else {
		openGymInterface->SendEnd(0);
		Simulator::Schedule (Seconds(check_time), &SetInterval);
	}       
     
    //** Get Action
	interval[0] = openGymInterface->SetAction () / 10000.0;
	interval[1] = openGymInterface->SetAction () / 10000.0;
	interval[2] = openGymInterface->SetAction () / 10000.0;
    // std::cout << "action: " << interval[0] << ", " << interval[1] << ", " << interval[2] << std::endl;
    
    avgRtt[0] = 0;
    cntRtt[0] = 0;
    avgRtt[1] = 0;
    cntRtt[1] = 0;
    avgRtt[2] = 0;
    cntRtt[2] = 0;      
}