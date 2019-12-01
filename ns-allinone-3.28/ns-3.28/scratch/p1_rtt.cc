#include <string.h>
#include <cstdlib>
#include <ctime> 
#include <map> 

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
int _segsize = 1024;

std::map<SequenceNumber32, int64_t> _rttHistory;
float _rttMax = 0;
float _rttMin = 0;
float _rttAvg = 0;
float _rttCur = 0;

static void TxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<const TcpSocketBase>);
static void RxTracer (Ptr<const Packet>, const TcpHeader&, Ptr<const TcpSocketBase>);

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
	bool			m_running;
	uint32_t		m_packetsSent;
	uint32_t	 	m_totBytes;
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

	// Ptr<TcpSocket> _tcpsocket = DynamicCast<TcpSocket> (m_socket);
	// Ptr<TcpSocketBase> _tcpsocketbase = DynamicCast<TcpSocketBase> (_tcpsocket);
	// _tcpsocketbase->TraceConnectWithoutContext ("RttHistroy", MakeCallback (&SendRttHistoryTracer));
	//std::cout << "ccOps: " << _tcpsocketbase -> GetTcpCongestionOps ()->GetName () << std::endl;
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

	//std::cout << "ClientApp: " << m_totBytes << "   " << Simulator::Now ().GetSeconds () << std::endl;
	//if(++m_packetsSent < m_nPackets) {
	//	ScheduleTx ();
	//}

	if (m_nPackets == 0 || ++m_packetsSent < m_nPackets) {
		ScheduleTx ();
	} else {
		std::cout << "stop 0" << std::endl;
		StopApplication ();
	}
	
	if (Simulator::Now ().GetSeconds () > Seconds (10.1)) {
		std::cout << "stop 1" << std::endl;
		StopApplication ();
	}
}

void
ClientApp::ScheduleTx (void) {
	if (m_running) {
		uint32_t bits = m_packetSize * 8;
		Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
		//Time tNext (MilliSeconds (std::rand() % 5 + 1));
		//std::cout << "term: " << tNext << std::endl;
		m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
	}
}

int main (int argc, char ** argv)
{
	LogComponentEnable ("Project", LOG_LEVEL_INFO);
  //std::map<string, int> m;
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
	NodeContainer n0, n1;
	n0.Create (1);
	n1.Create (1);
	
	NodeContainer n0n1 (n0, n1);
	
	/*-----	Create Channels	------*/
	NS_LOG_INFO("Create Channels.");
	PointToPointHelper bottleNeckLink;
	bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue ("1Gbps")); //DataRate
	bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (20))); //Propagation Delay
	//bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
	NetDeviceContainer deviceBottleneckLink = bottleNeckLink.Install (n0n1);

	/*-----	Create Internet Stack ------*/
	NS_LOG_INFO ("Create Internet Stack.");
	InternetStackHelper stack;
	stack.InstallAll ();
	
	/*------ Assign IP4 Address -----*/
	NS_LOG_INFO ("Assign IP4 Addresses.");
	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer serverInterfaces;
	serverInterfaces = address.Assign (deviceBottleneckLink);

	/*----- Create Server ------*/
	NS_LOG_INFO ("Create Server");
	uint16_t serverPort = 8080;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApps;
	serverApps.Add (packetSinkHelper.Install (n1));
	serverApps.Start (Seconds(0.));
	serverApps.Stop (Seconds(11.));

	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (_segsize));
	InetSocketAddress serverAddress (serverInterfaces.GetAddress (1), serverPort);
	Ptr<Socket> clientSocket = Socket::CreateSocket (n0.Get(0), tid);
	Ptr<TcpSocket> _tcpsocket = DynamicCast<TcpSocket> (clientSocket);
	Ptr<TcpSocketBase> _tcpsocketbase = DynamicCast<TcpSocketBase> (_tcpsocket);
	_tcpsocketbase->TraceConnectWithoutContext ("Tx", MakeCallback (&TxTracer));
	_tcpsocketbase->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTracer));
	Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
	clientApps->Setup (clientSocket, serverAddress, _segsize, 100, DataRate ("500Mbps"));
	n0.Get(0)->AddApplication (clientApps);
		
	clientApps->SetStartTime (Seconds (0.1));
	clientApps->SetStopTime (Seconds (10.1));

	// OnOffHelper onoff ("ns3::TcpSocketFactory", serverAddress);
	// onoff.SetAttribute ("PacketSize", UintegerValue (_segsize));
	// onoff.SetAttribute ("DataRate", StringValue ("500Mbps")); //bit/s
	// ApplicationContainer clientApps = onoff.Install (n0.Get(0));

	// clientApps.Start (Seconds (0.1));
	// clientApps.Stop (Seconds(10.1));
	
	/*----- Ascii Tracer -----*/
	NS_LOG_INFO ("Tracer Enable");
	AsciiTraceHelper ascii;
	//p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
	bottleNeckLink.EnablePcapAll ("ns3_topology");
	
	/*----- Create FlowMonitor -----*/
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
	Simulator::Stop (Seconds(12.));
	
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
		if(t.sourceAddress == "10.1.1.1"){
			std::cout << "  Goodput: "    << sink1->GetTotalRx() * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
	  }
		std::cout << "  First Packet Rx: " << i->second.timeFirstRxPacket.GetSeconds() << "sec\n";
		std::cout << "  Last Packet Rx: " << i->second.timeLastRxPacket.GetSeconds() << "sec\n";
    }
	

	/*----- Total Rx Counts -----*/
	uint32_t totalRx = DynamicCast<PacketSink> (serverApps.Get (0))->GetTotalRx();
	std::cout << "th total RX: " << totalRx << std::endl;
	
	
	Simulator::Destroy ();

	for(auto it = _rttHistory.begin (); it != _rttHistory.end (); it++) {
		std::cout << "key: " << it->first << " value: " << it->second << std::endl;
	}
	std::cout << _rttMax << "  " << _rttMin << "  " << _rttAvg << std::endl;
	NS_LOG_INFO ("Done.");
}


static void TxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<const TcpSocketBase> s) {
	uint8_t flags = h.GetFlags ();
	bool hasSyn = flags & TcpHeader::SYN;
	bool hasFin = flags & TcpHeader::FIN;
	
	SequenceNumber32 sq = h.GetSequenceNumber ();
	if (hasSyn) sq = sq + 1;
	else if (hasFin) sq = sq + _segsize + 1; 
	else sq = sq + _segsize; 

	_rttHistory.insert (std::make_pair (sq, Simulator::Now ().GetMilliSeconds ()));
}

static void RxTracer (Ptr<const Packet> p, const TcpHeader& h, Ptr<const TcpSocketBase> s) {
	// uint8_t flags = h.GetFlags ();
	// bool hasFin = flags & TcpHeader::FIN;
	
	
	SequenceNumber32 ack = h.GetAckNumber ();
	if(h.GetFlags () & TcpHeader::FIN) return;
	
	_rttCur = Simulator::Now ().GetMilliSeconds () - _rttHistory.find (ack) -> second;
    std::cout << "rtt: " << _rttCur << std::endl;
	_rttMax = (_rttCur > _rttMax) ? _rttCur : _rttMax;
	if(_rttMin == 0) _rttMin = _rttCur;
	else _rttMin = (_rttCur < _rttMin) ? _rttCur : _rttMin; 
	if(_rttAvg == 0) _rttAvg = _rttCur;
	else _rttAvg = 0.875 * _rttAvg + 0.125 * _rttCur;

	std::cout << ack << "  " << _rttCur << std::endl;
	for (auto it = _rttHistory.begin (); it != _rttHistory.end (); it++) {
		if (it->first <= ack) _rttHistory.erase(it->first);
	}
	
	//std::cout << "ack: " << ack << " time : " << Simulator::Now ().GetMilliSeconds () << std::endl;
}
