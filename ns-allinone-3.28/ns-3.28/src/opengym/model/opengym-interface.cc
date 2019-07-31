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
	:m_port(port), m_zmq_context(1), m_zmq_socket(m_zmq_context, ZMQ_REQ) {
}

OpenGymInterface::~OpenGymInterface () {
}

}
