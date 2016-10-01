//
//Copyright(c) 2016. Huan Xia
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
//documentation files(the "Software"), to deal in the Software without restriction, including without limitation
//the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software,
//and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions
//of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
//THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
//CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//DEALINGS IN THE SOFTWARE.


#include "Server.h"
#include <string.h>		//memset
#include <iostream>
#include <arpa/inet.h>
#include <event2/event-config.h>
#include <event2/thread.h>

//////////////////////////////////////////////////////////////////////////
// ClientContext

ClientContext::ClientContext(int fd, const char*ip, unsigned int port)
{
	_fd = fd;
	strcpy(_address, ip);
	_port = port;
}

ClientContext::ClientContext()
{

}

ClientContext::~ClientContext()
{
	//std::cout << "ClientContext destroyed." << std::endl;
}

void ClientContext::AppendRecvBufferData(char *buffer, size_t len)
{
	_recvBuffer.AppendData(buffer, len);
}

void ClientContext::SetFD(int fd)
{
	_fd = fd;
}

void ClientContext::SetIP(const char *ip)
{
	strcpy(_address, ip);
}

void ClientContext::SetPort(unsigned int port)
{
	_port = port;
}

const char* ClientContext::GetPeerIP()
{
	return _address;
}

unsigned int ClientContext::GetPeerPort()
{
	return _port;
}

Buffer &ClientContext::GetReceiveBuffer()
{
	return _recvBuffer;
}

//////////////////////////////////////////////////////////////////////////

SubServer::SubServer()
{

}

SubServer::~SubServer()
{
	_clients.foreach([](std::pair<int, std::shared_ptr<ClientContext>> client) {
		auto fd = bufferevent_getfd(client.second->_bev);
		evutil_closesocket(fd);
		bufferevent_free(client.second->_bev);
	});

	for (int i = 0; i < _workerThreadNumber; i++)
	{
		_workerThreads[i]->join();
	}
	if (_listeningThread && _listeningThread->joinable()) {
		_listeningThread->join();
	}

	delete[]_workerDefaultEvents;
	delete[]_baseWorkers;
	event_base_free(_base);
}

void SubServer::onThreadStart(size_t threadIndex)
{
	auto tid = std::this_thread::get_id();
	//std::cout << "Worker thread started. thread[" << threadIndex << "]=" << tid << std::endl;
	_baseWorkers[threadIndex] = event_base_new();

	//Create a dummy event to keep the event loop alive even if there is no socket event.
	_workerDefaultEvents[threadIndex] = event_new(_baseWorkers[threadIndex], -1, EV_READ, [](auto fd, auto what, auto arg) {
		//Do nothing
		//std::cout << "Dummy event triggered. TID=" << std::this_thread::get_id() << std::endl;
	}, this);

	event_add(_workerDefaultEvents[threadIndex], nullptr);
}

void SubServer::onThreadEnd(size_t threadIndex)
{
	auto tid = std::this_thread::get_id();
	std::cout << "Worker thread terminated. thread[" << threadIndex << "]=" << tid << std::endl;
	event_base_free(_baseWorkers[threadIndex]);
	_baseWorkers[threadIndex] = nullptr;
}

void SubServer::onThreadRun(size_t threadIndex)
{
	event_base_dispatch(_baseWorkers[threadIndex]);
}

ClientContext* SubServer::onCreateNewClientContext()
{
	//TODO: in derived class, create an object of ClientContext-derived class and return it. must be created using 'new' keyword
	return new ClientContext();
}

void SubServer::preClientReceived(int fd, char *buffer, size_t len)
{
	_clients.lock(fd); 
	auto it = _clients.find(fd);
	if (it != _clients.end(fd)) {
		auto clientContext = it->second;
		_clients.unlock(fd);

		clientContext->AppendRecvBufferData(buffer, len);
		size_t messageSize = 0;
		while ((messageSize = isClientDataReadable(clientContext.get())) > 0)
		{
			auto &buf = clientContext->_recvBuffer;
			onClientReceived(clientContext.get(), (char*)buf.GetReadPtr(), messageSize);
			buf.SkipData(messageSize);
		}
		return;
	}
	_clients.unlock(fd);
}

void SubServer::preClientDisconnected(int fd)
{
	_clients.lock(fd);
	auto it = _clients.find(fd);
	if (it != _clients.end(fd)) {
		auto clientContext = it->second;
		_clients.erase(it);
		_clients.unlock(fd);

		onClientDisconnected(clientContext.get());		
		return;
	}
	_clients.unlock(fd);
}

void SubServer::onClientReceived(ClientContext *clientContext, char *buffer, size_t len)
{
	//TODO: implement in derived classes. respond to the received package
	auto tid = std::this_thread::get_id();
	std::cout << len << " bytes of data received! thread=" << tid << ", IP=" << clientContext->_address << std::endl;

	SendDataToClient(clientContext, buffer, len);
}

void SubServer::onClientDisconnected(ClientContext *clientContext)
{
	//TODO: implement in derived classes. respond to the disconnecting event of a client
	auto tid = std::this_thread::get_id();
	std::cout << "Client disconnected! thread=" << tid << ", IP=" << clientContext->_address << std::endl;
}

size_t SubServer::isClientDataReadable(ClientContext *clientContext)
{
	//TODO: implement in derived classes. return the size of a single package
	auto &buf = clientContext->GetReceiveBuffer();
	return buf.GetDataLength();
}

void SubServer::SetServer(Server *server)
{
	_server = server;
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
	std::cout << _workerThreads.size() << " worker threads created!" << std::endl;
}

bool SubServer::CreateListeningHandler(unsigned int port)
{
	CreateWorkerThreads();

	std::future<bool> futureListenerReady = _listenerReady.get_future();

	std::shared_ptr<std::thread> listeningThread = std::make_shared<std::thread>([port,this](auto ptr) {

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htons(0);

		_listener = evconnlistener_new_bind(_base, [](auto listener, auto fd, auto addr_in, auto addr_len, auto ptr) {

			SubServer *pThis = (SubServer*)ptr;
			char szAddress[INET_ADDRSTRLEN] = { 0 };
			auto addr_in_ptr = (sockaddr_in*)addr_in;
			inet_ntop(AF_INET, &(addr_in_ptr->sin_addr), szAddress, INET_ADDRSTRLEN);

			std::cout << "new connection accepted! FD=" << fd << ", IP=" << szAddress << ":" << addr_in_ptr->sin_port << std::endl;

			auto clientContext = pThis->AddNewClient(fd, szAddress, addr_in_ptr->sin_port);

			/* We got a new connection! Set up a bufferevent for it. */

			struct event_base *base = pThis->GetNextAvailableBase();
			struct bufferevent *bev = bufferevent_socket_new(
				base, fd, BEV_OPT_CLOSE_ON_FREE);

			clientContext->_bev = bev;

			bufferevent_setcb(bev, [](auto bev, auto ctx) {
				/* This callback is invoked when there is data to read on bev. */
				struct evbuffer *input = bufferevent_get_input(bev);
				auto dataLen = evbuffer_get_length(input);
				auto dataLenOriginal = dataLen;
				struct evbuffer *output = bufferevent_get_output(bev);

				auto bfd = bufferevent_getfd(bev);

				SubServer *pThisInner = (SubServer*)ctx;
				const size_t bufferSize = 4096;
				char szBuffer[bufferSize];
				size_t readLen = 0;
				while (dataLen > 0)
				{
					readLen = evbuffer_copyout(input, szBuffer, bufferSize);
					if (readLen <= 0)
						break;
					dataLen -= readLen;
					pThisInner->preClientReceived(bfd, szBuffer, readLen);
					evbuffer_drain(input, readLen);
				}

				//evbuffer_drain(input, dataLenOriginal);

				/* Copy all the data from the input buffer to the output buffer. */
				//evbuffer_add_buffer(output, input);

			}, NULL, [](auto bev, auto events, auto ctx) {

				auto bfd = bufferevent_getfd(bev);
				auto tid = std::this_thread::get_id();
				std::cout << "Error! thread=" << tid << ", Event=0x" << std::hex << events << std::dec << ", FD=" << bfd << std::endl;

				SubServer *pThisInner = (SubServer*)ctx;
				if (events & BEV_EVENT_ERROR)
					perror("Error from bufferevent");

				if (events & BEV_EVENT_EOF) {	//socket closed
					pThisInner->preClientDisconnected(bfd);
					bufferevent_free(bev);
				}
			}, pThis);

			bufferevent_enable(bev, EV_READ | EV_WRITE);


		}, this, 0, -1, (sockaddr*)&addr, sizeof(addr));

		auto tid = std::this_thread::get_id();
		if (_listener)
		{
			_port = port;
			std::cout << "SubServer started, listening on port " << port << ". thread=" << tid << std::endl;

			this->_listenerReady.set_value(true);
			event_base_dispatch(_base);

			std::cout << "SubServer terminated on port " << port << ". thread=" << tid << std::endl;
			this->_server->DecreaseReference();
			this->ReleaseWorkerThreads();
		}
		else
		{
			std::cout << "SubServer failed to listen on port " << port << ". thread=" << tid << std::endl;
			this->ReleaseWorkerThreads();
			this->_listenerReady.set_value(false);
		}

	}, this);

	auto listenerReady = futureListenerReady.get();
	if (listenerReady)
	{
		auto fd = evconnlistener_get_fd(_listener);
		_server->LinkSubserverByFD(port, fd);
		_listeningThread = listeningThread;
	}
	else
	{
		listeningThread->join();
	}
			
	return listenerReady;
}

event_base * SubServer::GetNextAvailableBase()
{
	return _baseWorkers[(_workerThreadIndexBase++) % _workerThreadNumber];
}

void SubServer::ReleaseWorkerThreads()
{
	for (auto i = 0; i < _workerThreadNumber; i++)
	{
		event_active(_workerDefaultEvents[i], EV_READ, 0);
	}
}

std::shared_ptr<ClientContext> SubServer::AddNewClient(int fd, const char*ip, unsigned int port)
{
	std::shared_ptr<ClientContext> client(onCreateNewClientContext());
	if (!client)
		return client;

	client->SetFD(fd);
	client->SetIP(ip);
	client->SetPort(port);

	_clients.lock(fd);
	_clients[fd] = client;
	_clients.unlock(fd);
	return client;
}

void SubServer::Initialize(unsigned int workerThreadNumber)
{
	_workerThreadNumber = workerThreadNumber;
	_base = event_base_new();
	_baseWorkers = new event_base*[workerThreadNumber];
	_workerDefaultEvents = new event*[workerThreadNumber];
	for (auto i = 0; i < workerThreadNumber; i++)
	{
		_baseWorkers[i] = nullptr;
		_workerDefaultEvents[i] = nullptr;
	}
}

size_t SubServer::SendDataToClient(int fd, char *buffer, size_t len)
{
	_clients.lock(fd);
	auto it = _clients.find(fd);
	if (it != _clients.end(fd))
	{
		auto clientContext = it->second;
		_clients.unlock(fd);

		auto output = bufferevent_get_output(clientContext->_bev);
		evbuffer_add(output, buffer, len);
		return len;
	}
	_clients.unlock(fd);
	return 0;
}

size_t SubServer::SendDataToClient(ClientContext *clientContext, char *buffer, size_t len)
{
	auto output = bufferevent_get_output(clientContext->_bev);
	evbuffer_add(output, buffer, len);
}

//////////////////////////////////////////////////////////////////////////

Server::Server()
{
	_reference = 0;
#ifdef WIN32
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif
}


Server::~Server()
{
}

void Server::IncreaseReference()
{
	_reference++;
}

void Server::DecreaseReference()
{
	_reference--;
	if (_reference == 0) {
		_shutdownVariable.notify_all();
	}
}


void Server::LinkSubserverByFD(unsigned int port, int fd)
{
	_subServers.lock(port);
	auto it = _subServers.find(port);
	if (it != _subServers.end(port)) {
		auto subServer = it->second;
		_subServers.unlock(port);
		_subServersByFD.lock(fd);
		_subServersByFD[fd] = subServer;
		_subServersByFD.unlock(fd);
		return;
	}
	_subServers.unlock(port);
}

void Server::TerminateSubServer(unsigned int port)
{
	_subServers.lock(port);
	auto it = _subServers.find(port);
	if (it != _subServers.end(port))
	{
		auto subServer = it->second;
		_subServers.unlock(port);
		auto fd = evconnlistener_get_fd(subServer->_listener);
		_subServersByFD.erase(fd);
		evconnlistener_free(subServer->_listener);
		return;
	}
	_subServers.unlock(port);
}

void Server::Start()
{
	if (_subServers.size() == 0)
	{
		std::cout << "No subserver to run. Server terminated..." << std::endl;
		return;
	}

	std::unique_lock<std::mutex> mlock(_mtxShutdown);
	_shutdownVariable.wait(mlock);
}
