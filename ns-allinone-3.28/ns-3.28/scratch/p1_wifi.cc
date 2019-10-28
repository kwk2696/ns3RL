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

static void BackOffTracer (Ptr<DcaTxop>);
static void Test (Ptr<DcfState>, uint32_t);
static void PacketSinkRx (Ptr<const Packet>, const Address&);
static void SetBackoffRL ();
static void PrintTime () {
	std::cout << Simulator::Now().GetSeconds() << std::endl;
	Simulator::Schedule(Seconds(0.005), &PrintTime);
}

void Record (float, float, float); 
 
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

uint32_t nWifi = 3;
uint32_t sim_time = 10;	
uint32_t cw[3] = {5,5,5}; // set cw
std::list<float> m_queue0; 
std::list<float> m_queue1;
std::list<float> m_queue2;
float rewardThr[3] = {0};
// StartTime, LastTime, TotalRecvBytes
float client1[3] = {0};
float client2[3] = {0}; 
float client3[3] = {0};
int
main (int argc, char *argv[])
{
	LogComponentEnable ("Project", LOG_LEVEL_INFO);
	// LogComponentEnable ("DcfState", LOG_LEVEL_DEBUG);

	uint32_t segment_size = 1024;
	bool tracing = true;
	bool backoffon = true;
	uint32_t sim_count = 1;	
	while(1) {
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (segment_size));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1)); //one ACK during timeout period
	
	std::cout << "simulation count: " << sim_count << std::endl;
	sim_count ++;
	for (uint32_t i = 0; i < 3; i++) {
		cw[i] = 5;
		client1[i] = 0;
		client2[i] = 0;
		client3[i] = 0;
		rewardThr[i] = 0;
	}
	m_queue0.clear();
	m_queue1.clear();
	m_queue2.clear();
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
	wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
	wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
	
	WifiMacHelper mac;
	Ssid ssid = Ssid ("ns-3-ssid");
	mac.SetType ("ns3::StaWifiMac",
							 "Ssid", SsidValue (ssid),
							 "ActiveProbing", BooleanValue (false));
	NetDeviceContainer staDevices;
	staDevices = wifi.Install (phy, mac, wifiStaNodes);
	
	/*----- Backoff Control-----*/
	if(true) {
		for (uint32_t i = 0; i < nWifi; i++) {
			Ptr<NetDevice> nd = staDevices.Get(i);
			Ptr<WifiNetDevice> wnd = DynamicCast<WifiNetDevice> (nd);
			Ptr<WifiMac> wm = wnd->GetMac ();
			Ptr<RegularWifiMac> rwm = DynamicCast<RegularWifiMac> (wm);
			Ptr<DcaTxop> dtxop = rwm->GetDcaTxopPublic ();
			if(backoffon) {
				dtxop->TraceConnectWithoutContext ("SetCw", MakeCallback (&BackOffTracer));
			}
			dtxop->TraceConnectWithoutContext ("SetBackOff", MakeCallback (&Test));
			dtxop->m_tag = i;
		}
	}
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
	

	/*------ Handle Accepted Server Sockets and Count Throughput -----*/
	if (backoffon) {
		Ptr<Application> app = serverApp.Get(0);
		Ptr<PacketSink> ps = DynamicCast<PacketSink> (app);
		ps->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketSinkRx));
	}


	/*----- Create Client ------*/
	NS_LOG_INFO("Create Client");
	std::vector <uint32_t> ranRate = {1000000, 2000000, 4000000};
	InetSocketAddress serverAddress (p2pInterfaces.GetAddress (1), serverPort);
	for (uint32_t i = 0; i < nWifi; i++) {
		Ptr<Socket> sock = Socket::CreateSocket (wifiStaNodes.Get (i), tid);
		
		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (sock, serverAddress, segment_size, 0, DataRate("10Mbps"), false, ranRate.at(0));
		wifiStaNodes.Get (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (1.0));
		clientApps->SetStopTime (Seconds (sim_time));
	}
	
	/*----- Schedule When to Set BackOff -----*/
	if (backoffon) {
		Simulator::Schedule (Seconds(1.0), &SetBackoffRL);
	}
	if (false) {
		Simulator::Schedule (Seconds(1.0), &PrintTime);
	}
	
	/*----- Pcap Tracer -----*/
	if (tracing)
    {
      p2pHelper.EnablePcapAll ("p1_wifi");
      phy.EnablePcap ("p1_wifi", apDevice.Get (0));
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
	
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
	}
	return 0;
}


static void BackOffTracer (Ptr<DcaTxop> dca) {
	uint32_t tag = dca->m_tag;
	int c = 0;
	
	//** Set the Backoff Here
	if(tag==0) c = pow(2,  cw[0]) - 1;
	else if(tag==1) c = pow(2,  cw[1]) - 1;
	else if(tag==2) c = pow(2,  cw[2]) - 1;
	else std::cout << "error: " << tag << std::endl;
	
	dca->SetCw(c);
}
static void Test (Ptr<DcfState> dcf, uint32_t backoff) {
	// std::cout << "p1_wifi: " << backoff << std::endl;
}

static void PacketSinkRx (Ptr<const Packet> packet, const Address& from) {
	InetSocketAddress address = InetSocketAddress::ConvertFrom(from).GetIpv4();
	
	if (address == InetSocketAddress (Ipv4Address ("10.2.1.1"))) {		
		if (client1[0] == 0) client1[0] = Simulator::Now ().GetSeconds (); 
		client1[1] = Simulator::Now ().GetSeconds (); 
		client1[2] += packet->GetSize ();
		
		// for (uint32_t i = 0; i < 3; i++) std::cout << client1[i] << std::endl;
	}
	else if (address == InetSocketAddress (Ipv4Address ("10.2.1.2"))) {
		if (client2[0] == 0) client2[0] = Simulator::Now ().GetSeconds (); 
		client2[1] = Simulator::Now ().GetSeconds (); 
		client2[2] += packet->GetSize ();
		// std::cout << Simulator::Now ().GetSeconds () << std::endl;
	}
	else if (address == InetSocketAddress (Ipv4Address ("10.2.1.3"))) {
		if (client3[0] == 0) client3[0] = Simulator::Now ().GetSeconds (); 
		client3[1] = Simulator::Now ().GetSeconds (); 
		client3[2] += packet->GetSize ();
		// std::cout << Simulator::Now ().GetSeconds () << std::endl;
	}
}

void SetBackoffRL () {
	// std::cout << Simulator::Now ().GetSeconds () << std::endl;
	
	
	// Calculate Reward
	float Thr[3] = {0};
	Thr[0] = client1[2] * 8.0 / (client1[1] - client1[0]) / 1000000;
	if(client1[1] == client1[0]) Thr[0] = 0;
	Thr[1] = client2[2] * 8.0 / (client2[1] - client2[0]) / 1000000;
	if(client2[1] == client2[0]) Thr[1] = 0;
	Thr[2] = client3[2] * 8.0 / (client3[1] - client3[0]) / 1000000;
	if(client3[1] == client3[0]) Thr[2] = 0;
	
	Record (Thr[0], Thr[1], Thr[2]);
	float reward = 0.3333 * rewardThr[0] + 0.3333 * rewardThr[1] + 0.3333 * rewardThr[2];
	// std::cout << rewardThr[0] << ":" << rewardThr[1] << ":" << rewardThr[2] << std::endl;
	//** Send State and Reward
	std::string message = "{\"state00\":" + std::to_string(cw[0]/10.0) + ",\"state01\":" + std::to_string(Thr[0]/10.0) 
						+ ",\"state10\":" + std::to_string(cw[1]/10.0) + ",\"state11\":" + std::to_string(Thr[1]/10.0) 
						+ ",\"state20\":" + std::to_string(cw[2]/10.0) + ",\"state21\":" + std::to_string(Thr[2]/10.0) 
						+ ",\"reward\":" + std::to_string(reward) + "}";
	// std::cout << message << std::endl;
	openGymInterface->Send(message);
	
	if(Simulator::Now ().GetSeconds() == sim_time + 0.1) {
		openGymInterface->SendEnd(1);
		return;
	} 
	else {
		openGymInterface->SendEnd(0);
		Simulator::Schedule (Seconds(0.005), &SetBackoffRL);
	}
	
	//** Get Action
	cw[0] = openGymInterface->SetAction ();
	cw[1] = openGymInterface->SetAction ();
	cw[2] = openGymInterface->SetAction ();
	
	for (uint32_t i = 0; i < 3; i++) {
		// std::cout << cw[i];
		client1[i] = 0;
		client2[i] = 0;
		client3[i] = 0;
	}
}

uint32_t MAX = 3;
void Record (float Thr0, float Thr1, float Thr2) {

	// queue0
	if(m_queue0.size() != MAX) {
		m_queue0.push_back (Thr0);
	}
	else {
		m_queue0.pop_front ();
		m_queue0.push_back (Thr0);
	}
	// If only one, 0 reward
	if(m_queue0.size() == 1) {
		rewardThr[0] = 0;
	}
	else {
		float max = 0, min = 100, avg = 0;
		for (auto it = m_queue0.begin (); it != m_queue0.end (); it++) {
			max = (*it > max) ? *it : max;
			min = (*it < min) ? *it : min;
			avg = 0.875 * *it + 0.125 * avg;// moving average 로 바꿔야하나?
			// std::cout << "Thr: " << *it << std::endl;
		}
		
		if(min == avg || max == min) rewardThr[0] = 0;
		else rewardThr[0] = (avg - min) / (max - min);
	}

	// queue1 
	if(m_queue1.size() != MAX) {
		m_queue1.push_back (Thr1);
	}
	else {
		m_queue1.pop_front ();
		m_queue1.push_back (Thr1);
	}
	if(m_queue1.size() == 1) {
		rewardThr[1] = 0;
	}
	else {
		float max = 0, min = 100, avg = 0;
		for (auto it = m_queue1.begin (); it != m_queue1.end (); it++) {
			max = (*it > max) ? *it : max;
			min = (*it < min) ? *it : min;
			avg = 0.875 * *it + 0.125 * avg;
		}
		
		if(min == avg || max == min) rewardThr[1] = 0;
		else rewardThr[1] = (avg - min) / (max - min);
	}

	//queue2
	if(m_queue2.size() != MAX) {
		m_queue2.push_back (Thr2);
	}
	else {
		m_queue2.pop_front ();
		m_queue2.push_back (Thr2);
	}
	
	if(m_queue2.size() == 1) {
		rewardThr[2] = 0;
	}
	else {
		float max = 0, min = 100, avg = 0;
		for (auto it = m_queue2.begin (); it != m_queue2.end (); it++) {
			max = (*it > max) ? *it : max;
			min = (*it < min) ? *it : min;
			avg = 0.875 * *it + 0.125 * avg;
		}
		
		if(min == avg || max == min) rewardThr[2] = 0;
		else rewardThr[2] = (avg - min) / (max - min);
	}
}