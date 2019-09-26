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

int m_segsize = 1024;
std::ofstream rttFile0("Heu-RTT0.txt");
std::ofstream rttFile1("Heu-RTT1.txt");
std::ofstream rttFile2("Heu-RTT2.txt");

static void TxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<TcpSocketBase>);
static void RxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<TcpSocketBase>);
static void QueueDiscTracer (Ptr<MfifoQueueDisc>);
uint32_t HeuristicAction (uint32_t []); 

static void PingRtt (std::string context, Time rtt)
{ 
  std::cout << Simulator::Now (). GetSeconds() << " RTT = " << rtt.GetMilliSeconds () << " ms" << std::endl;
}

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
			Time tNext (NanoSeconds ((std::rand() % 1000000 + m_ranRate)));
			m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
		}	
		else {
			uint32_t bits = m_packetSize * 8;
			Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
			m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
		}
		// std::cout << "tNext: " << tNext << std::endl;
	}
}


int main (int argc, char ** argv)
{

	LogComponentEnable ("Project", LOG_LEVEL_INFO);

	std::srand ((unsigned int) (std::time(NULL)));
	uint32_t sim_time = 4;
	bool ascii_enable = true;
	bool ping_enable = false;
	NS_LOG_INFO("This is Heuristic Algorithm");
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
	bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue ("10Mbps")); //DataRate
	bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0))); //Propagation Delay
	//bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
	NetDeviceContainer deviceBottleneckLink = bottleNeckLink.Install (n3n4);
	
	PointToPointHelper accessLink;
	accessLink.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
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
	tchFifo.SetRootQueueDisc ("MfifoQueueDisc", "MaxSize", StringValue ("120p"));
	QueueDiscContainer qdisc = tchFifo.Install (deviceBottleneckLink);
	Ptr<QueueDisc> q = qdisc.Get (0);
	Ptr<MfifoQueueDisc> mfq = DynamicCast<MfifoQueueDisc> (q);	
	mfq->TraceConnectWithoutContext ("GetAction", MakeCallback (&QueueDiscTracer));
	
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
	serverApps.Stop (Seconds(sim_time + 1));
	
	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	std::vector <uint8_t> tosValues = {0x28, 0xb8, 0x70}; //AC_BE, AC_BK, AC_VI
	std::vector <std::string> dataRate = {"10Mbps", "6Mbps", "4Mbps"};
	std::vector <uint32_t> ranRate = {1000000, 2000000, 4000000};
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (m_segsize));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
	for (uint32_t i = 0; i < 3; ++i) {
		InetSocketAddress serverAddress (serverInterfaces.GetAddress (1), serverPort);
		serverAddress.SetTos (tosValues.at(i));
		Ptr<Socket> clientSocket = Socket::CreateSocket (clientNodes.Get(i), tid);
		Ptr<TcpSocket> _tcpsocket = DynamicCast<TcpSocket> (clientSocket);
		Ptr<TcpSocketBase> _tcpsocketbase = DynamicCast<TcpSocketBase> (_tcpsocket);
		_tcpsocketbase->TraceConnectWithoutContext ("RecordTx", MakeCallback (&TxTracer));
		_tcpsocketbase->TraceConnectWithoutContext ("RecordRx", MakeCallback (&RxTracer));
		_tcpsocketbase->_tag = i;

		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (clientSocket, serverAddress, m_segsize, 0, DataRate (dataRate.at(i)), true, ranRate.at(i));
		clientNodes.Get (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (0.1));
		clientApps->SetStopTime (Seconds (sim_time + 0.1));
	}
	
	// /*----- Configure ping -----*/
	if (ping_enable) {
		V4PingHelper ping = V4PingHelper (serverInterfaces.GetAddress(1));
		ping.Install (n0);
		ping.Install (n1);
		ping.Install (n2);
		Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));
	}

	/*----- Ascii Tracer -----*/
	// NS_LOG_INFO ("Tracer Enable");
	if (ascii_enable) {
		AsciiTraceHelper ascii;
		// p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
		bottleNeckLink.EnablePcapAll ("p1_heuristic");
	}
	
	/*----- Create FlowMonitor -----*/
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	
	Simulator::Stop (Seconds(sim_time + 2));
	
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
		std::cout << "  First Packet Rx: " << i->second.timeFirstRxPacket.GetSeconds() << "sec\n";
		std::cout << "  Last Packet Rx: " << i->second.timeLastRxPacket.GetSeconds() << "sec\n";
    }
	
	/*----- QueueDisc Stats -----*/
	QueueDisc::Stats st = qdisc.Get (0)->GetStats ();
	std::cout << st << std::endl;	
	
	Simulator::Destroy ();
	
	NS_LOG_INFO ("Done.");
}

static void TxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<TcpSocketBase> s) {
	uint8_t flags = h.GetFlags ();
	bool hasSyn = flags & TcpHeader::SYN;
	bool hasFin = flags & TcpHeader::FIN;
	
	SequenceNumber32 sq = h.GetSequenceNumber ();
	
	if (!hasSyn && !hasFin && p->GetSize ()) {
		s->_sendTime.insert(std::make_pair (sq+p->GetSize (), Simulator::Now ().GetSeconds ()));
	}
}

float m_rewardRtt[3] = {0};
static void RxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<TcpSocketBase> s) {
	uint8_t flags = h.GetFlags ();
	bool hasSyn = flags & TcpHeader::SYN;
	bool hasFin = flags & TcpHeader::FIN;
	
	SequenceNumber32 ack = h.GetAckNumber ();
	
	if(!hasSyn && !hasFin) {
		s->_rttCur = Simulator::Now ().GetSeconds () - s->_sendTime.find (ack) -> second;
		
		s->_rttMax = (s->_rttCur > s->_rttMax) ? s->_rttCur : s->_rttMax;
		if(s->_rttMin == 0) s->_rttMin = s->_rttCur;
		else s->_rttMin = (s->_rttCur < s->_rttMin) ? s->_rttCur : s->_rttMin; 
		
		if(s->_rttAvg == 0) s->_rttAvg = s->_rttCur;
		else s->_rttAvg = 0.875 * s->_rttAvg + 0.125 * s->_rttCur;
		
		if (s->_rttMax == s->_rttMin) m_rewardRtt[s->_tag] = 0;
		else m_rewardRtt[s->_tag] = (s->_rttMax - s->_rttMin) / (s->_rttMax - s->_rttAvg);
	
		/* print RTT file */ 
		if (s->_tag == 0) {
			// rttFile0 << s->_rttMax << ", " << s->_rttMin << ", " << s->_rttAvg << std::endl;
			rttFile0 << "RttAvg" << "=" << s->_rttAvg << " sec" << std::endl;
		}
		else if (s->_tag == 1) rttFile1 << "RttAvg" << "=" << s->_rttAvg << " sec" << std::endl;
		else rttFile2 << "RttAvg" << "=" << s->_rttAvg << " sec" << std::endl;
		
		s->_sendTime.erase (ack);
	}	
}


static void QueueDiscTracer (Ptr<MfifoQueueDisc> mfq) {
	uint32_t info[3];
	mfq->GetQueueInfo (info);
	
	// std::cout << "[" << info[0] << "," << info[1] << "," << info[2] << "]" << std::endl;
	uint32_t action = HeuristicAction (info);
	// std::cout << "action: " << action << std::endl;

	mfq->SetAction (action);
}

/* Find Max length queue */
uint32_t HeuristicAction (uint32_t info[]) {
	if(info[0] >= info[1] && info[0] >= info[2]) return 0;
	else if (info[1] >= info[0] && info[1] >= info[2]) return 1;
	else return 2;
}