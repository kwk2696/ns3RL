#include "ns3/log.h"
#include "opengym-interface.h"

#include <zmq.hpp> 
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
	: m_port (port){	
}

OpenGymInterface::~OpenGymInterface () {
}

void
OpenGymInterface::Send (std::string message) {
	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);
	_socket.connect ("tcp://localhost:5050");

	// Send JSON to Python
	zmq::message_t request (message.size ());
	memcpy (request.data (), message.c_str (), message.size ());
	_socket.send (request);

	zmq::message_t reply;
	_socket.recv (&reply);
}

uint32_t
OpenGymInterface::Communicate (uint32_t info[]) {

	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);

	_socket.connect ("tcp://localhost:5050");

	// Send obs to Python 
	for(uint32_t i = 0; i < 3; ++i) {
		
		zmq::message_t request(sizeof(info[i]));
		memcpy (request.data(), &info[i], sizeof(info[i]));
		_socket.send (request);

		zmq::message_t reply;
		_socket.recv (&reply);
	}
	
	// Recieve action form Python
	zmq::message_t action_request (7);
	memcpy (action_request.data (), "action", 7);
	_socket.send (action_request);

	zmq::message_t action;
	_socket.recv(&action);
	
	std::string rpl = std::string (static_cast<char*> (action.data()), action.size ());
	uint32_t retval = atoi(rpl.c_str ());
	
	return retval;
}

void 
OpenGymInterface::SendObservation (uint32_t info[], uint32_t size) {
	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);

	_socket.connect ("tcp://localhost:5050");

	// Send obs to Python
	for (uint32_t i = 0; i < size; ++i) {
		zmq::message_t request (sizeof (info[i]));
		memcpy (request.data (), &info[i], sizeof( info[i]));
		_socket.send (request);

		zmq::message_t reply;
		_socket.recv (&reply);
	}
}

uint32_t
OpenGymInterface::SetAction (void) {
	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);
	
	_socket.connect ("tcp://localhost:5050");

	// Recv action from Python
	zmq::message_t request (2);
	memcpy (request.data (), "A", 2);
	_socket.send (request);

	zmq::message_t reply;
	_socket.recv (&reply);

	std::string action = std::string (static_cast<char*> (reply.data ()), reply.size ());
	uint32_t retval = atoi (action.c_str ());

	return retval;
}

void
OpenGymInterface::SendReward (uint8_t reward) {
	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);

	_socket.connect ("tcp://localhost:5050");

	zmq::message_t request(sizeof (reward));
	memcpy (request.data (), &reward, sizeof (reward));
	_socket.send (request);

	zmq::message_t reply;
	_socket.recv (&reply);
}

void
OpenGymInterface::SendEnd (uint8_t end) {
	zmq::context_t _context(1);
	zmq::socket_t _socket (_context, ZMQ_REQ);

	_socket.connect ("tcp://localhost:5050");

	zmq::message_t request(sizeof (end));
	memcpy (request.data (), &end, sizeof (end));
	_socket.send (request);

	zmq::message_t reply;
	_socket.recv (&reply);
}

}
