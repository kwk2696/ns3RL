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
	:m_port(port) {
}

OpenGymInterface::~OpenGymInterface () {
}

bool
OpenGymInterface::Init () {
	std::cout << "check0" << std::endl;

	std::string connectAddr = "tcp://localhost:5555";
	
	std::cout << "check1" <<std::endl;
	
	void *context = (void*)zmq_ctx_new ();
	void *requester = zmq_socket (context, ZMQ_REQ);
	int retval  = zmq_connect(requester, "tcp://locakhost:5555");
	std::cout << retval << std::endl;
	//zmq_connect ((void*)m_socket, connectAddr.c_str());
  retval = zmq_send(requester, "hello", 6, 0);
	std::cout << retval << std::endl;
	return true;
}

}
