#include "ns3/log.h"
#include "test-queue-disc.h"
#include "ns3/object-factory.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/net-device-queue-interface.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TestQueueDisc);

TestQueueDisc::TestQueueDisc ()
	: QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE) {
}

TestQueueDisc::~TestQueueDisc () {
}

TypeId TestQueueDisc::GetTypeId (void) {
	static TypeId tid = TypeId ("TestQueueDisc")
		.SetParent<QueueDisc> ()
		.SetGroupName ("TrafficControl")
		.AddConstructor<TestQueueDisc> ()
		.AddAttribute ("MaxSize",
									 "The max queue size",
									 QueueSizeValue (QueueSize ("1000p")),
									 MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
																					&QueueDisc::GetMaxSize),
									 MakeQueueSizeChecker ())
	;
	return tid;
}

bool
TestQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item) {
	int test;
	std::cin >> test;
	std::cout << test << std::endl;
	bool retval = QueueDisc::GetInternalQueue (0)->Enqueue (item);
	return retval;
}

Ptr<QueueDiscItem>
TestQueueDisc::DoDequeue (void) {
	Ptr<QueueDiscItem> item = QueueDisc::GetInternalQueue (0)->Dequeue ();
	return item;
}

Ptr<const QueueDiscItem>
TestQueueDisc::DoPeek (void) {
	Ptr<const QueueDiscItem> item = QueueDisc::GetInternalQueue (0)-> Peek ();
	return item;
}

bool
TestQueueDisc::CheckConfig (void) {
	if (QueueDisc::GetNInternalQueues () == 0) {
		ObjectFactory factory;
		factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
		factory.Set ("MaxSize", QueueSizeValue (QueueDisc::GetMaxSize ()));
		AddInternalQueue (factory.Create <InternalQueue> ());
		AddInternalQueue (factory.Create <InternalQueue> ());
	}
	return true;
}

void
TestQueueDisc::InitializeParams (void) {
}

}
