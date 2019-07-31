#ifndef OPENGYM_INTERFACE_H
#define OPENGYM_INTERFACE_H

#include <zmq.hpp>

namespace ns3 {

class OpenGymInterface {
public: 
	static TypeId GetTypeId (void);

	OpenGymInterface ();
	virtual ~OpenGymInterface ();

pritvate: 
	uint32_t m_port;
	zmq::context_t m_zmq_context;
	zmq::socket_t m_zmq_socket;
};

}
#endif
