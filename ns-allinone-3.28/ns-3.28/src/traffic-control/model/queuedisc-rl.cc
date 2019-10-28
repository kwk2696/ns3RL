#include "ns3/log.h"
#include "queuedisc-rl.h"
#include "ns3/object-factory.h"
#include "ns3/queue-disc.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MfifoQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (MfifoQueueDisc);
NS_OBJECT_ENSURE_REGISTERED (VirtualPacket);

TypeId
VirtualPacket::GetTypeId (void) {
	static TypeId tid = TypeId ("VirtualPacket")
		.SetParent<Object> ()
		.SetGroupName ("TrafficControl")
		.AddConstructor<VirtualPacket> ()
	;
	return tid;
}

VirtualPacket::VirtualPacket ()
{
}

VirtualPacket::~VirtualPacket ()
{
}

TypeId
MfifoQueueDisc::GetTypeId (void) {
	static TypeId tid = TypeId ("MfifoQueueDisc")
		.SetParent<QueueDisc> ()
		.SetGroupName ("TrafficControl")
		.AddConstructor<MfifoQueueDisc> ()
		.AddAttribute ("MaxSize",
									 "The max queue size",
									 QueueSizeValue (QueueSize ("1000p")),
									 MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
																					&QueueDisc::GetMaxSize),
									 MakeQueueSizeChecker ())
		.AddTraceSource ("QueueWeight",
										 "Set Queue Weight",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_qwTrace),
										 "ns3::MfifoQueueDisc::QueueWeightTracedCallback")
		.AddTraceSource ("NDrop",
										 "No Drop before Enqueue",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_nDrop),
										 "ns3::MfifoQueueDisc::NDropTracedCallback")
		.AddTraceSource ("YDrop",
										 "Drop before Enqueue",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_yDrop),
										 "ns3::MfifoQueueDisc::YDropTracedCallback")
		.AddTraceSource ("UpdateQueueInfo",
										 "Update Queue Information",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_qUpdate),
										 "ns3::MfifoQueueDisc::QueueUpdateTracedCallback")
		.AddTraceSource ("GetAction",
										 "Get Action",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_qAction),
										 "ns3::MfifoQueueDisc::SelectQueueTracedCallback")
		.AddTraceSource ("SendReward",
										 "Send Reward",
										 MakeTraceSourceAccessor (&MfifoQueueDisc::m_qReward),
										 "ns3::MfifoQueueDisc::SendRewardTracedCallback")
	;
	
	return tid;
}

MfifoQueueDisc::MfifoQueueDisc ()
	: QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES) {
}

MfifoQueueDisc::~MfifoQueueDisc () {
}

void
MfifoQueueDisc::GetQueueInfo (uint32_t *info) {
	for (uint32_t i = 0; i < 3; ++i) {
		info[i] = GetInternalQueue (i)-> GetCurrentSize (). GetValue ();
	}
}

void 
MfifoQueueDisc::SetQueueWeight (uint32_t weight) {
	m_weight = weight;
}

void 
MfifoQueueDisc::SetAction (uint32_t action) {
	m_action = action;
}

float
MfifoQueueDisc::GetReward () {
	return m_reward;
}

/* MfifoQueueDisc */

const float MfifoQueueDisc::limit[3] = {0.01, 0.05, 0.1}; 
void 
MfifoQueueDisc::CalculateReward (void) {
	//std::cout << m_rvectors.size () << std::endl;
	for (uint32_t i = 0; i < 3; ++i) {
		RecordVector *rvector =  m_rvectors.at (i);
		float time = Simulator::Now (). GetSeconds ();
		//std::cout << "now: " << time << std::endl;
		for (RecordVector::iterator itr = rvector->begin (); itr != rvector->end (); itr++) {
			//std::cout << "rvector: " << *itr << std::endl;
			float diff = time - *itr;
			switch (i) {
				case 0:
					if(diff >= limit[0]) 
						m_reward -= 0.1;
					break;
				case 1:
					if(diff >= limit[1]) 
						m_reward -= 0.1;
					break;
				case 2:
					if(diff >= limit[2]) 
						m_reward -= 0.1;
					break;
				default:
					break;
			}
		}
	}
	//std::cout << "reward : " << m_reward << std::endl;
}

const uint32_t MfifoQueueDisc::prio2band[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
bool
MfifoQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item) {
	uint8_t priority = 0;
	
	SocketPriorityTag priorityTag;
	if (item->GetPacket ()->PeekPacketTag (priorityTag)) {
		priority = priorityTag.GetPriority ();
	}

	uint32_t 	band = prio2band[priority & 0x0f];
	
	//std::cout << this << " enqueue: " << Simulator::Now ().GetMilliSeconds () << std::endl;
	bool retval = GetInternalQueue (band)->Enqueue (item);

	if(retval) {
		m_rvectors.at (band)->push_back (Simulator::Now (). GetSeconds ());
	}
	else {
		m_reward -= 100; // if dropped, minus reward
	}
	m_qwTrace(this);
	
	return retval;
}

Ptr<QueueDiscItem>
MfifoQueueDisc::DoDequeue (void) {
	//std::cout << "Dequeue: " << Simulator::Now ().GetSeconds () << std::endl;
	// if(!m_running) {
		// m_running = true; 
	// } else {
		// CalculateReward ();
		// m_qReward(this);
	// }
	//CalculateReward ();	
	m_qAction(this);
	//std::cout << "3. qdisc dequeue: " <<  this << "  " << m_action << std::endl;

	 //std::cout << "(" << GetInternalQueue (0)-> GetCurrentSize (). GetValue () << "," 
	//	<< GetInternalQueue (1)-> GetCurrentSize (). GetValue () << ","
	//	<< GetInternalQueue (2)-> GetCurrentSize (). GetValue () << ") " << std::endl;
	//std::cout << m_action << std::endl;

	//std::cout << "action: " << m_action << std::endl;
	Ptr<QueueDiscItem> item = GetInternalQueue (m_action)-> Dequeue ();
	//std::cout << this << " action: " << Simulator::Now (). GetMilliSeconds () << std::endl;
	if(item) {
		m_rvectors.at (m_action)->pop_front ();
	}
	//std::cout << "4. qdisc" << std::endl;
	m_reward = 0; // initialize reward after dequeue
	if(!item) {
		//std::cout << "no popped" << std::endl;
		m_reward = -100; // if anything dequeued, minus reward
	}//else {
		//Ptr<QueueDiscItem> item = GetInternalQueue(m_action) -> Dequeue();
		//m_rvectors.at (m_action)->pop_front ();
	//}
	//std::cout << "5. qdisc" << std::endl;
	return item;
}

Ptr<const QueueDiscItem>
MfifoQueueDisc::DoPeek (void) { 
	Ptr<const QueueDiscItem> item = GetInternalQueue (0)-> Peek ();
	return item;
}


bool
MfifoQueueDisc::CheckConfig (void) {
	
	if (GetNInternalQueues () == 0) {
		ObjectFactory factory;
		factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
		factory.Set ("MaxSize", QueueSizeValue (QueueDisc::GetMaxSize ()));
		AddInternalQueue (factory.Create <InternalQueue> ());
		AddInternalQueue (factory.Create <InternalQueue> ());
		AddInternalQueue (factory.Create <InternalQueue> ());
		for (uint32_t i = 0; i < m_size; ++i) {
			RecordVector *rvector = new RecordVector;
			m_rvectors.push_back (rvector);
		}
	}
	
	if (GetNInternalQueues () != 3) {
		NS_LOG_ERROR ("MfifoQueue CheckConfig Error");
		return false;
	}
	
	return true;
}

void
MfifoQueueDisc::InitializeParams (void) {
}

} //ns3 end

