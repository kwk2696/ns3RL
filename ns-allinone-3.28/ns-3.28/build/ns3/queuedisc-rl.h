#ifndef QUEUE_DISC_RL_H
#define QUEUE_DISC_RL_H


#include "ns3/queue-disc.h"
#include <list>

namespace ns3 {

class VirtualPacket : public Object {
public:
    static TypeId GetTypeId (void);
    
    VirtualPacket ();
    virtual ~VirtualPacket ();

private:
    int m_reward = 0;
};

class MfifoQueueDisc : public QueueDisc {
public:
	static TypeId GetTypeId (void);

	MfifoQueueDisc ();
	virtual ~MfifoQueueDisc ();
	
	void GetQueueInfo (uint32_t*);
	void SetQueueWeight (uint32_t);
	void SetAction (uint32_t);
	float GetReward ();
  
	typedef std::list<float> RecordVector;
private:
	static const uint32_t prio2band[9];
	static const float limit[3];
	
	void  CalculateReward (void);
	
	virtual bool DoEnqueue (Ptr<QueueDiscItem>);
	virtual Ptr<QueueDiscItem> DoDequeue (void);
	virtual Ptr<const QueueDiscItem> DoPeek (void);
	virtual bool CheckConfig (void);
	virtual void InitializeParams (void);
	
	uint32_t m_size = 3; //number of queues
	uint32_t m_weight = 0;
	uint32_t m_action = 0;
	uint32_t m_SYN = 0;
	float m_reward = 0;
	bool m_running = false;
	
	std::vector<RecordVector*> m_rvectors;
protected:
	TracedCallback<Ptr<MfifoQueueDisc>> m_qwTrace;
	TracedCallback<Ptr<MfifoQueueDisc>> m_nDrop;
	TracedCallback<Ptr<MfifoQueueDisc>> m_yDrop;
	TracedCallback<Ptr<MfifoQueueDisc>> m_qUpdate;
	TracedCallback<Ptr<MfifoQueueDisc>> m_qAction;
	TracedCallback<Ptr<MfifoQueueDisc>> m_qReward;
};



}
#endif
