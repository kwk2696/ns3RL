#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctime> 
#include <zmq.hpp>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project");

void ReceivePacket (Ptr<Socket>);
void SendPacket (Ptr<Socket>, uint32_t);
static void CwndTracer (uint32_t, uint32_t);
static void TraceCwnd (std::string);
static void QueueDiscEnqueueTracer (Ptr<MfifoQueueDisc>);

static bool firstCwnd = true;
static uint32_t cWndValue; 

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
	//uint32_t rd = rand() % 2048;
	//std::cout << "rand: " << rd << std::endl;
	Ptr<Packet> packet = Create<Packet> (m_packetSize);
	//Ptr<TcpSocketBase> temp = DynamicCast<TcpSocketBase> (m_socket);
	//temp->m_tcb->m_cWnd = 536;
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
		m_sendEvent = Simulator::Schedule (tNext, &ClientApp::SendPacket, this);
	}
}

// class TestQueueDisc : public QueueDisc {
// public:
	// TestQueueDisc ();
	// virtual ~TestQueueDisc();

	// static TypeId GetTypeId (void);
	// static constexpr const char* DROP = "Forced drop";

// private:
	// virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
	// virtual Ptr<QueueDiscItem> DoDequeue (void);
	// virtual Ptr<const QueueDiscItem> DoPeek (void);
	// virtual bool CheckConfig (void);
	// virtual void InitializeParams (void);
// };

// TestQueueDisc::TestQueueDisc ()
	// : QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE) {
// }

// TestQueueDisc::~TestQueueDisc () {
// }

// TypeId TestQueueDisc::GetTypeId (void) {
	// static TypeId tid = TypeId ("TestQueueDisc")
		// .SetParent<QueueDisc> ()
		// .SetGroupName ("TrafficControl")
		// .AddConstructor<TestQueueDisc> ()
		// .AddAttribute ("MaxSize",
									 // "The max queue size",
									 // QueueSizeValue (QueueSize ("1000p")),
									 // MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
																					// &QueueDisc::GetMaxSize),
									 // MakeQueueSizeChecker ())
	// ;
	// return tid;
// }

// bool
// TestQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item) {

	// bool retval = QueueDisc::GetInternalQueue (0)->Enqueue (item);
	// return retval;
// }

// Ptr<QueueDiscItem>
// TestQueueDisc::DoDequeue (void) {
	// Ptr<QueueDiscItem> item = QueueDisc::GetInternalQueue (0)->Dequeue ();
	// return item;
// }

// Ptr<const QueueDiscItem>
// TestQueueDisc::DoPeek (void) {
	// Ptr<const QueueDiscItem> item = QueueDisc::GetInternalQueue (0)-> Peek ();
	// return item;
// }

// bool
// TestQueueDisc::CheckConfig (void) {
	// if (QueueDisc::GetNInternalQueues () == 0) {
		// ObjectFactory factory;
		// factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
		// factory.Set ("MaxSize", QueueSizeValue (QueueDisc::GetMaxSize ()));
		// AddInternalQueue (factory.Create <InternalQueue> ());
		// AddInternalQueue (factory.Create <InternalQueue> ());
	// }
	// return true;
// }

// void
// TestQueueDisc::InitializeParams (void) {
// }

int main (int argc, char ** argv)
{
	std::srand(5232);

	LogComponentEnable ("Project", LOG_LEVEL_INFO);
	//LogComponentEnable ("TcpSocketBase", LOG_LEVEL_ALL);
	//LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);
	//LogComponentEnable ("QueueDisc", LOG_LEVEL_ALL);
	//LogComponentEnable ("TcpTxBuffer", LOG_LEVEL_ALL);	
	//LogComponentEnable ("DropTailQueue", LOG_LEVEL_ALL);
	/*----- Communicate with Python ------*/
//	int client_socket;

//	struct sockaddr_in server_addr;

//	client_socket = socket (PF_INET, SOCK_STREAM, 0);
//	if (-1 == client_socket) {
//		std::cout << "Socket Failure" << std::endl;
//		exit (1);
//	}

//	memset ( &server_addr, 0, sizeof(server_addr));
//	server_addr.sin_family = AF_INET;
//	server_addr.sin_port = htons (4000);
//	server_addr.sin_addr.s_addr = inet_addr ("127.0.0.1");

//	if (-1 == connect (client_socket, (struct sockaddr*) &server_addr, sizeof(server_addr))) {
	//	std::cout << "Connect Failure" << std::endl;
	//	exit (1);
//	}

//	write (client_socket, argv[1], strlen ( argv[1])+1);

	/*----- ZMQ Connect -----*/
	NS_LOG_INFO("ZMQ Connect");
	//std::string connectAddr = "tcp://localhost:5050";
//	zmq::context_t m_zmq_context (1);
	//zmq::socket_t m_zmq_socket (m_zmq_context, ZMQ_REQ);
	//zmq_connect ((void*)m_zmq_socket
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
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute ("DataRate", StringValue("5Mbps"));
	p2p.SetChannelAttribute ("Delay", TimeValue(MilliSeconds(5)));
	p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
	NetDeviceContainer netd1 = p2p.Install (n0n1);
	NetDeviceContainer netd2 = p2p.Install (n1n2);

	/*-----	Create Internet Stack ------*/
	NS_LOG_INFO ("Create Internet Stack.");
	InternetStackHelper stack;
	stack.Install (nodes);
	
	/*----- Create Queue Disc ------*/
	NS_LOG_INFO ("Create Queue Disc.");
	TrafficControlHelper tch1;
	tch1.SetRootQueueDisc ("MfifoQueueDisc", "MaxSize", StringValue ("1p"));
	//tch1.AddChildQueueDisc (0,0, "ns3::FifoQueueDisc");
	QueueDiscContainer qdiscs1 = tch1.Install (netd1);
	QueueDiscContainer qdiscs2 = tch1.Install (netd2);
	Ptr<QueueDisc> q = qdiscs1.Get(0);
	Ptr<MfifoQueueDisc> mfq = DynamicCast<MfifoQueueDisc> (q);
	mfq->TraceConnectWithoutContext ("QueueWeight", MakeCallback (&QueueDiscEnqueueTracer));

	/*------ Assign IP4 Address -----*/
	NS_LOG_INFO ("Assign IP4 Addresses.");
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer iface1 = ipv4.Assign (netd1);
	ipv4.SetBase ("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer iface2 = ipv4.Assign (netd2);

	/*----- Create Routes ------*/
	NS_LOG_INFO ("Create Routes.");
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	/*----- Create Server ------*/
	NS_LOG_INFO ("Create Server");
	uint16_t serverPort = 8080;
	TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get(2));
	serverApp.Start (Seconds(0.));
	serverApp.Stop (Seconds(5.));

	/*----- Create Client ------*/
	NS_LOG_INFO ("Create Client");
	Address serverAddress (InetSocketAddress (iface2.GetAddress (1), serverPort));
	Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get(0), tid);
	clientSocket->SetSendCallback (MakeCallback (&SendPacket));
	Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase> (clientSocket);
	Ptr<TcpCongestionOps> congestionOps = CreateObject<TcpNewReno> ();
	tcpSocket->SetCongestionControlAlgorithm (congestionOps);
	Ptr<ClientApp> clientApp = CreateObject <ClientApp> ();
	clientApp->Setup (clientSocket, serverAddress, 1024, 10, DataRate ("1Mbps"));
	nodes.Get (0)->AddApplication (clientApp);
	clientApp->SetStartTime (Seconds (1.));
	clientApp->SetStopTime (Seconds (5.));

	/*----- Ascii Tracer -----*/
	NS_LOG_INFO ("Tracer Enable");
	AsciiTraceHelper ascii;
	p2p.EnableAsciiAll (ascii.CreateFileStream ("ns3_topology.tr"));
	p2p.EnablePcapAll ("ns3_topology");
	Simulator::Schedule (Seconds (0.00001), &TraceCwnd, "ns3_topology-cwnd.data");
	
	Simulator::Stop (Seconds(5.1));

	NS_LOG_INFO ("Run Simulation");
	Simulator::Run ();
	
	/*----- QueueDisc Stats -----*/
	QueueDisc::Stats st = qdiscs1.Get (0)->GetStats ();
	std::cout << st << std::endl;
	st = qdiscs2.Get (0)->GetStats();
	std::cout << st << std::endl;

	Simulator::Destroy ();
	//close(client_socket);
	NS_LOG_INFO ("Done.");

}

void ReceivePacket (Ptr<Socket>) {	
	std::cout << "Receive Packet" << std::endl;
}

void SendPacket (Ptr<Socket>, uint32_t) {
	std::cout << Simulator::Now ().GetSeconds ()  << " Send Packet" << std::endl;
}

static void CwndTracer (uint32_t oldval, uint32_t newval) {
	if (firstCwnd) {
		std::cout << "0.0 " << oldval << std::endl;
		firstCwnd = false;
	}
	std::cout << Simulator::Now ().GetSeconds () << " " << newval << std::endl;;
	cWndValue = newval;
}
static void TraceCwnd (std::string cwnd_tr_file_name) {
    std::cout << "Check" << std::endl;
	Config::ConnectWithoutContext ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback (&CwndTracer));
}

static void QueueDiscEnqueueTracer (Ptr<MfifoQueueDisc> queuedisc) {
	if(queuedisc->GetQueueInfo () == true ) {
		queuedisc->SetQueueWeight (1);
	}
}
