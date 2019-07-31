#include "ns3/log.h"
#include "queuedisc-rl.h"
#include "ns3/object-factory.h"
#include "ns3/queue-disc.h"
#include "ns3/queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MfifoQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (MfifoQueueDisc);

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
	;
	
	return tid;
}

MfifoQueueDisc::MfifoQueueDisc ()
	: QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES) {
}

MfifoQueueDisc::~MfifoQueueDisc () {
}

bool
MfifoQueueDisc::GetQueueInfo (void) {
	return true;
}

void 
MfifoQueueDisc::SetQueueWeight (uint32_t weight) {
	m_weight = weight;
}

bool
MfifoQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item) {
	m_qwTrace(this);
	std::cout << this << ": " << m_weight << std::endl;

	bool retval = GetInternalQueue (0)-> Enqueue (item);
	return retval;
}

Ptr<QueueDiscItem>
MfifoQueueDisc::DoDequeue (void) {
	Ptr<QueueDiscItem> item = GetInternalQueue (0)-> Dequeue ();
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

