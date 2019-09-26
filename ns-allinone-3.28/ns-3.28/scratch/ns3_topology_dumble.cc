#include <string.h>

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

static void QueueDiscEnqueueTracer (Ptr<MfifoQueueDisc>);
static void NDropRewardTracer (Ptr<MfifoQueueDisc>);
static void YDropRewardTracer (Ptr<MfifoQueueDisc>);
static void UpdaeteQueueTracer (Ptr<MfifoQueueDisc>);

std::ofstream rttFile("router-RTT.txt");
static void PingRtt (std::string context, Time rtt)
{ 
  std::cout << Simulator::Now (). GetSeconds() << " RTT = " << rtt.GetMilliSeconds () << " ms" << std::endl;
  rttFile << context << "=" << rtt.GetMilliSeconds () << " ms" << std::endl;
}

static void
GoodputSampling (std::string fileName, ApplicationContainer app, Ptr<OutputStreamWrapper> stream, float period)
{
  Simulator::Schedule (Seconds (period), &GoodputSampling, fileName, app, stream, period);
  double goodput;
  uint32_t totalPackets = DynamicCast<PacketSink> (app.Get (0))->GetTotalRx ();
  goodput = totalPackets * 8 / (Simulator::Now ().GetSeconds () * 1024); // Kbit/s
  //goodput = totalPackets * 8 / (Simulator::Now ().GetSeconds () * 1000000.0); // Kbit/s
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << goodput << std::endl;
  
  //std::cout << Simulator::Now (). GetSeconds() << " Goodput = " << goodput << " Mbps" << std::endl;
}

Ptr<OpenGymInterface> openGymInterface;

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
};

ClientApp::ClientApp ()
	: m_socket(0),
	  m_peer (),
		m_packetSize (0),
		m_nPackets (0),
		m_dataRate (0),
		m_sendEvent (),
		m_running (false),
		m_packetsSent (0)
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

	if(++m_packetsSent < m_nPackets) {
		ScheduleTx ();
	}
}

void
ClientApp::ScheduleTx (void) {
	if (m_running) {
		uint32_t bits = m_packetSize * 8;
		Time tNext (Seconds (bits / static_cast<double> (m_dataRate.GetBitRate ())));
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

	/*----- ZMQ Connect -----*/
	NS_LOG_INFO("ZMQ Connect");
    
	/*----- Error Model -----*/
	NS_LOG_INFO("Error Model");
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
	uv->SetStream (50);
	RateErrorModel error_model;
	error_model.SetRandomVariable (uv);
	error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
	error_model.SetRate (0.1);
	
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
	bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue ("2Mbps")); //DataRate
	bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0.01))); //Propagation Delay
	bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	//bottleNeckLink.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (&error_model));

	PointToPointHelper p2pLeaf;
	p2pLeaf.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
	p2pLeaf.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (20)));
	p2pLeaf.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	
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
	tchFifo.SetRootQueueDisc ("MfifoQueueDisc", "MaxSize", StringValue ("1p"));
	QueueDiscContainer qdisc1 = tchFifo.Install (dumble.GetLeft ()->GetDevice (0));
	QueueDiscContainer qdisc2 = tchFifo.Install (dumble.GetRight ()->GetDevice (0));
	Ptr<QueueDisc> q = qdisc1.Get(0);
	Ptr<MfifoQueueDisc> mfq = DynamicCast<MfifoQueueDisc> (q);
	mfq->TraceConnectWithoutContext ("QueueWeight", MakeCallback (&QueueDiscEnqueueTracer));
	mfq->TraceConnectWithoutContext ("NDrop", MakeCallback (&NDropRewardTracer));
	mfq->TraceConnectWithoutContext ("YDrop", MakeCallback (&YDropRewardTracer));
	mfq->TraceConnectWithoutContext ("UpdateQueueInfo", MakeCallback(&UpdaeteQueueTracer));
	
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
	serverApps.Stop (Seconds(100.));

	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	Address serverAddress (InetSocketAddress (dumble.GetRightIpv4Address (0), serverPort));
	for (uint32_t i = 0; i < dumble.LeftCount (); ++i) {
		// BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", serverAddress);
		// bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
		// ApplicationContainer clientApp = bulkSendHelper.Install (dumble.GetLeft (i));
		
		Ptr<Socket> clientSocket = Socket::CreateSocket (dumble.GetLeft(i), tid);
		clientSocket->SetIpTos(1);
		Ptr<ClientApp> clientApps = CreateObject <ClientApp> ();
		clientApps->Setup (clientSocket, serverAddress, 1024, 1000, DataRate ("1Mbps"));
		dumble.GetLeft (i)->AddApplication (clientApps);
		
		clientApps->SetStartTime (Seconds (1.));
		clientApps->SetStopTime (Seconds (99.));
	}

	// Configure and install ping
	V4PingHelper ping = V4PingHelper (dumble.GetLeftIpv4Address(0));
	ping.Install (n0);
  
	// Print Ping RTT
	Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));

	/*----- Ascii Tracer -----*/
	NS_LOG_INFO ("Tracer Enable");
	AsciiTraceHelper ascii;
	//p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
	bottleNeckLink.EnablePcapAll ("ns3_topology");
	//Simulator::Schedule (Seconds (0.00001), &TraceCwnd, "ns3_topology-cwnd.data");
	Ptr<OutputStreamWrapper> uploadGoodputStream = ascii.CreateFileStream ("ns3_topology-goodput.txt");
	Simulator::Schedule (MilliSeconds (1), &GoodputSampling, "ns3_topology-goodput.txt", serverApps, uploadGoodputStream, 1);

	Simulator::Stop (Seconds(100.1));

	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();
	
	/*----- QueueDisc Stats -----*/
	QueueDisc::Stats st = qdisc1.Get (0)->GetStats ();
	std::cout << st << std::endl;
	st = qdisc2.Get (0)->GetStats();
	std::cout << st << std::endl;

	/*----- Total Rx Counts -----*/
	uint32_t totalRx = DynamicCast<PacketSink> (serverApps.Get (0))->GetTotalRx();
	std::cout << "total RX: " << totalRx << std::endl;
	
	Simulator::Destroy ();
	
	uint32_t send[3] = {4294967295, 0, 0};
	openGymInterface->SendObservation (send, 3);
	
	//close(client_socket);
	NS_LOG_INFO ("Done.");

}

static void QueueDiscEnqueueTracer (Ptr<MfifoQueueDisc> queuedisc) {
	uint32_t info[3];
	queuedisc->GetQueueInfo (info);
	
	/* obs update */
	openGymInterface->SendObservation (info, 3);
	
	/* recv action from .py */
	uint32_t action = openGymInterface->SetAction ();
	
	/* queuedisc act */
	queuedisc->SetQueueWeight (action);
	
	//std::cout << "action: " << action << std::endl;
}

static void NDropRewardTracer (Ptr<MfifoQueueDisc> queuedisc) {
	/* send reward to .py */
	openGymInterface->SendReward (0);
	//std::cout << "reward: 1" << std::endl;
}

static void YDropRewardTracer (Ptr<MfifoQueueDisc> queuedisc) {
	openGymInterface->SendReward (-1);
	//std::cout << "reward: 0" << std::endl;
}

static void UpdaeteQueueTracer (Ptr<MfifoQueueDisc> queuedisc) {
	uint32_t info[3];
	queuedisc->GetQueueInfo (info);
	
	/* obs update */
	// openGymInterface->SendObservation (info, 3);
	//std::cout << "obs" <<std::endl;
}
