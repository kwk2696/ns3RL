#ifndef OPENGYM_INTERFACE_H
#define OPENGYM_INTERFACE_H

#include "ns3/object.h"
#include <zmq.hpp>

namespace ns3 {

class OpenGymInterface : public Object {
public: 
	static TypeId GetTypeId (void);

	OpenGymInterface (uint32_t port=5050);
	virtual ~OpenGymInterface ();
	
	//bool Init ();
	void Send (std::string);

	uint32_t Communicate (uint32_t[]);
	void SendObservation (uint32_t[], uint32_t);
	uint32_t SetAction (void);
	void SendReward (uint8_t);
	void SendEnd (uint8_t);
	
private: 
	uint32_t m_port;
	//zmq::context_t m_context;
	//zmq::socket_t m_socket;
};

}
#endif
