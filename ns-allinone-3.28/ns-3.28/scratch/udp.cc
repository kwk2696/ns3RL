#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpTest");

int
main (int argc, char *argv[])
{
	CommandLine cmd;
  cmd.Parse (argc, argv);
  
	NodeContainer n0;
	n0.Create (1);
	NodeContainer n1;
	n1.Create (1);
	NodeContainer n2;
	n2.Create (1);

	NodeContainer net1 (n0, n1);
	NodeContainer net2 (n1, n2);
	NodeContainer nodes (n0, n1, n2);


	PointToPointHelper access;
	access.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
	access.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));

	NetDeviceContainer deviceAccess = access.Install (net1);

	PointToPointHelper bottle;
	bottle.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
	bottle.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (20)));


	NetDeviceContainer deviceBottle = bottle.Install (net2);

	InternetStackHelper stack;
	stack.Install (nodes);

	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer clientInterface = address.Assign (deviceAccess);

	address.SetBase ("10.2.1.0", "255.255.255.0");
	Ipv4InterfaceContainer serverInterface = address.Assign (deviceBottle);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	uint16_t serverPort = 8080;
	PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
	ApplicationContainer serverApp;
	serverApp.Add (packetSinkHelper.Install (n2));
	serverApp.Start (Seconds (0.));
	serverApp.Stop (Seconds (11.));

	Address serverAddress (InetSocketAddress (serverInterface.GetAddress (1), serverPort));
	//Config::SetDefault ("ns3::UdpSocketFactory", UintegerValue (1024));
	OnOffHelper onoff ("ns3::UdpSocketFactory", serverAddress);
	onoff.SetAttribute ("PacketSize", UintegerValue (1024));
	onoff.SetAttribute ("DataRate", StringValue ("500Mbps"));

	//BulkSendHelper bulk ("ns3::UdpSocketFactory", serverAddress);
	//bulk.SetAttribute ("SendSize", UintergerValue (1024));
	//bulk.SetAttribute ("MaxBytes", UintegerValue 
	ApplicationContainer clientApp = onoff.Install (n0);

	clientApp.Start (Seconds (0.1));
	clientApp.Stop (Seconds (10.1));

	Simulator::Stop (Seconds (12.));

	Simulator::Run ();
	Simulator::Destroy ();

	return 0;
}
