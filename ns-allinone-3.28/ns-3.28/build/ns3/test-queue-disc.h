#ifndef TEST_QUEUE_DISC_H
#define TEST_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3 {

class TestQueueDisc : public QueueDisc {
public:
	TestQueueDisc ();
	virtual ~TestQueueDisc();

	static TypeId GetTypeId (void);
	static constexpr const char* DROP = "Forced drop";

private:
	virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
	virtual Ptr<QueueDiscItem> DoDequeue (void);
	virtual Ptr<const QueueDiscItem> DoPeek (void);
	virtual bool CheckConfig (void);
	virtual void InitializeParams (void);
};

}
#endif