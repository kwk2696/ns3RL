#ifndef QUEUE_DISC_RL_H
#define QUEUE_DISC_RL_H

#include "ns3/queue-disc.h"

namespace ns3 {

class MfifoQueueDisc : public QueueDisc {
public:
	static TypeId GetTypeId (void);

	MfifoQueueDisc ();
	virtual ~MfifoQueueDisc ();
	
	bool GetQueueInfo (void);
	void SetQueueWeight (uint32_t);
private:
	virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
	virtual Ptr<QueueDiscItem> DoDequeue (void);
	virtual Ptr<const QueueDiscItem> DoPeek (void);
	virtual bool CheckConfig (void);
	virtual void InitializeParams (void);

	uint32_t m_weight = 0;
protected:
	TracedCallback<Ptr<MfifoQueueDisc>> m_qwTrace;
};

}
#endif
