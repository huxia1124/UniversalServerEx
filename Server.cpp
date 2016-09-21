#include "Server.h"
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <arpa/inet.h>

//////////////////////////////////////////////////////////////////////////

SubServer::SubServer(unsigned int workerThreadNumber) : _workerThreadNumber(workerThreadNumber)
{
	_base = event_base_new();
	_baseWorkers = new event_base*[workerThreadNumber];
	_workerDefaultEvents = new event*[workerThreadNumber];
	for (auto i = 0; i < workerThreadNumber; i++)
	{
		_baseWorkers[i] = nullptr;
		_workerDefaultEvents[i] = nullptr;
	}
}

SubServer::~SubServer()
{
	delete[]_workerDefaultEvents;
	delete[]_baseWorkers;
	event_base_free(_base);
}

void SubServer::onThreadStart(size_t threadIndex)
{
	_baseWorkers[threadIndex] = event_base_new();
	_workerDefaultEvents[threadIndex] = event_new(_baseWorkers[threadIndex], -1, EV_READ, [](auto fd, auto what, auto arg) {


	}, this);

	event_add(_workerDefaultEvents[threadIndex], nullptr);
}

void SubServer::onThreadEnd(size_t threadIndex)
{
	event_base_free(_baseWorkers[threadIndex]);
	_baseWorkers[threadIndex] = nullptr;
}

void SubServer::onThreadRun(size_t threadIndex)
{
	//event_base_loop(_baseWorkers[threadIndex], EVLOOP_ONCE);
	event_base_dispatch(_baseWorkers[threadIndex]);

}

void SubServer::CreateWorkerThreads()
{
	for (int i = 0; i < _workerThreadNumber; i++)
	{
		std::shared_ptr<std::thread> thread = std::make_shared<std::thread>([i](void* ptr) {

			SubServer *pThis = (SubServer*)ptr;
			pThis->onThreadStart(i);
			pThis->onThreadRun(i);
			pThis->onThreadEnd(i);
		}, this);

		_workerThreads.push_back(thread);
	}
}

//////////////////////////////////////////////////////////////////////////
// Function to get the id of current thread
pid_t gettid(void)
{
	return syscall(__NR_gettid);
}


bool SubServer::CreateListeningHandler(unsigned int port)
{
	CreateWorkerThreads();

	std::shared_ptr<std::thread> listeningThread = std::make_shared<std::thread>([port,this](auto ptr) {

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htons(0);

		_listener = evconnlistener_new_bind(_base, [](auto listener, auto fd, auto addr_in, auto addr_len, auto ptr) {

			char szAddress[INET_ADDRSTRLEN] = { 0 };
			auto addr_in_ptr = (sockaddr_in*)addr_in;
			inet_ntop(AF_INET, &(addr_in_ptr->sin_addr), szAddress, INET_ADDRSTRLEN);

			std::cout << "new connection accepted! FD=" << fd << ", IP=" << szAddress << ":" << addr_in_ptr->sin_port << std::endl;
			SubServer *pThis = (SubServer*)ptr;
			/* We got a new connection! Set up a bufferevent for it. */
			//struct event_base *base = evconnlistener_get_base(listener);
			struct event_base *base = pThis->GetNextAvailableBase();
			struct bufferevent *bev = bufferevent_socket_new(
				base, fd, BEV_OPT_CLOSE_ON_FREE);

			bufferevent_setcb(bev, [](auto bev, auto ctx) {

				auto bfd = bufferevent_getfd(bev);
				auto tid = gettid();
				std::cout << "data received! thread=" << tid << ", FD=" << bfd << std::endl;

				/* This callback is invoked when there is data to read on bev. */
				struct evbuffer *input = bufferevent_get_input(bev);
				struct evbuffer *output = bufferevent_get_output(bev);

				/* Copy all the data from the input buffer to the output buffer. */
				evbuffer_add_buffer(output, input);

			}, NULL, [](auto bev, auto events, auto ctx) {

				auto bfd = bufferevent_getfd(bev);
				auto tid = gettid();
				std::cout << "Error! thread=" << tid << ", Event=0x" << std::hex << events << std::dec << ", FD=" << bfd << std::endl;

				if (events & BEV_EVENT_ERROR)
					perror("Error from bufferevent");
				if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
					bufferevent_free(bev);
				}
				if (events & BEV_EVENT_EOF) {	//socket closed
				}
			}, NULL);

			bufferevent_enable(bev, EV_READ | EV_WRITE);


		}, this, 0, -1, (sockaddr*)&addr, sizeof(addr));

		_port = port;

		auto tid = gettid();
		std::cout << "SubServer started, listening on port " << port <<"! thread=" << tid << std::endl;
		event_base_dispatch(_base);

	}, this);

	_listeningThread = listeningThread;	
}

event_base * SubServer::GetNextAvailableBase()
{
	return _baseWorkers[(_workerThreadIndexBase++) % _workerThreadNumber];
}

//////////////////////////////////////////////////////////////////////////

Server::Server()
{

}


Server::~Server()
{
}

bool Server::CreateSubServer(unsigned int port, void *param)
{
	auto subServer = std::make_shared<SubServer>();

	subServer->CreateListeningHandler(port);

	_subServers.lock(port);
	_subServers[port] = subServer;
	_subServers.unlock(port);
}

void Server::Start()
{
	//event_base_dispatch(_base);

	std::unique_lock<std::mutex> mlock(_mtxShutdown);
	_shutdownVariable.wait(mlock);
}
