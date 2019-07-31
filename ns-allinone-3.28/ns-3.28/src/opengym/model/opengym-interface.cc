#include "ns3/log.h"
#include "opengym-interface.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OpenGymInterface");

NS_OBJECT_ENSURE_REGISTERED (OpenGymInterface);

TypeId
OpenGymInterface::GetTypeId (void) {
	static TypeId tid = TypeId ("ns3::OpenGymInterface")
		.SetParent<Object> ()
		.SetGroupName ("OpenGym")
		.AddConstructor<OpenGymInterface> ()
	;
	
	return tid;
}

OpenGymInterface::OpenGymInterface (uint32_t port)
	:m_port(port), m_context (1), m_socket (m_context, ZMQ_REQ) {
}

OpenGymInterface::~OpenGymInterface () {
}

bool
OpenGymInterface::Init () {

	m_socket.connect ("tcp://localhost:5050");
	zmq::message_t request(5);
	memcpy (request.data(), "Hello", 5);
	m_socket.send (request);

	zmq::message_t reply;
	m_socket.recv (&reply);
	std::cout << "recv" << std::endl;

	return true;
}

}
