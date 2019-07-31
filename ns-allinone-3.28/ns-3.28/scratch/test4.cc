#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/net-device.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("test4");

int
main(int argc, char *argv[])
{
	NodeContainer nodes;
	nodes.Create(2);
	
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
	p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
	p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue("1p")); //each p2pNetDevice must have queue to pass through
	
	InternetStackHelper stack;
	stack.Install (nodes);
	
	NetDeviceContainer netdevices;
	netdevices = p2p.Install (nodes); //create p2pChannel, create p2pNetDevice for all nodes, create a queue for net device
	
	TrafficControlHelper tch; //define the discipline for queue
	tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("2p"));
	
	QueueDiscContainer qdiscs = tch.Install (netdevices); //creates QueueDiscs for each devices in the container
	/* Queue discs
	   queues: actually store the packets waiting for trasmission
	   classes: allow to reserve a different treatment to different packets
	   filters: determine the qeue or class which a packet is destined to
	*/
	Ptr<QueueDisc> q = qdiscs.Get(0);
	
	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");
	
	Ipv4InterfaceContainer interfaces = address.Assign (netdevices);
	
	std::cout << interfaces.GetAddress(0) << std::endl;
	std::cout << netdevices.Get(0)<< std::endl;
	
}

