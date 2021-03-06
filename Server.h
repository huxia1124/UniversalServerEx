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


#pragma once

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <memory>
#include <map>
#include "STXUtility.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include "ThreadPool.h"
#include <vector>
#include <atomic>
#include "Buffer.h"
#include <future>
#include <iostream>

//////////////////////////////////////////////////////////////////////////
class Server;

//////////////////////////////////////////////////////////////////////////
// ClientContext

class ClientContext
{
	friend class SubServer;
protected:
	int _fd;
	Buffer _recvBuffer;
	char _address[64];
	unsigned int _port = 0;
	bufferevent *_bev = nullptr;

public:
	ClientContext();
	ClientContext(int fd, const char*ip, unsigned int port);
	virtual ~ClientContext();

protected:
	void AppendRecvBufferData(char *buffer, size_t len);

	void SetFD(int fd);
	void SetIP(const char *ip);
	void SetPort(unsigned int port);

public:
	const char* GetPeerIP();
	unsigned int GetPeerPort();
	Buffer &GetReceiveBuffer();

};

//////////////////////////////////////////////////////////////////////////
// SubServer

class SubServer
{
	friend class Server;
public:
	SubServer();
	virtual ~SubServer();

protected:

	event_base *_base = nullptr;						//For listening thread
	evconnlistener *_listener = nullptr;
	unsigned int _port = 0;
	event_base **_baseWorkers;
	event **_workerDefaultEvents;						//Empty events just to keep the event loop running
	std::vector<std::shared_ptr<std::thread>> _workerThreads;
	std::shared_ptr<std::thread> _listeningThread;
	unsigned int _workerThreadNumber;
	void *_serverParam = nullptr;						//Custom server parameter
	Server *_server = nullptr;

	std::promise<bool> _listenerReady;					//Notified when listener is ready or fail


	CSTXHashMap<int, std::shared_ptr<ClientContext>> _clients;

private:
	unsigned int _workerThreadIndexBase = 0;

protected:
	virtual void onThreadStart(size_t threadIndex);
	virtual void onThreadEnd(size_t threadIndex);
	virtual void onThreadRun(size_t threadIndex);

protected:
	virtual void preClientReceived(int fd, char *buffer, size_t len);
	virtual void preClientDisconnected(int fd);

protected:
	//Methods to override in subclasses
	virtual ClientContext* onCreateNewClientContext();
	virtual void onClientReceived(ClientContext *clientContext, char *buffer, size_t len);
	virtual void onClientDisconnected(ClientContext *clientContext);
	virtual size_t isClientDataReadable(ClientContext *clientContext);

private:
	void SetServer(Server *server);
	void CreateWorkerThreads();
	bool CreateListeningHandler(unsigned int port);
	event_base *GetNextAvailableBase();
	void ReleaseWorkerThreads();
	std::shared_ptr<ClientContext> AddNewClient(int fd, const char*ip, unsigned int port);
	void Initialize(unsigned int workerThreadNumber);

public:
	size_t SendDataToClient(int fd, char *buffer, size_t len);
	size_t SendDataToClient(ClientContext *clientContext, char *buffer, size_t len);
	

};

//////////////////////////////////////////////////////////////////////////

class Server
{
	friend class SubServer;
public:
	Server();
	virtual ~Server();

protected:
	std::mutex _mtxShutdown;
	std::condition_variable _shutdownVariable;
	std::atomic<int> _reference;

protected:
	CSTXHashMap<unsigned int, std::shared_ptr<SubServer>> _subServers;			//port -> subserver
	CSTXHashMap<int, std::shared_ptr<SubServer>> _subServersByFD;				//fd -> subserver

protected:
	void IncreaseReference();
	void DecreaseReference();
	void LinkSubserverByFD(unsigned int port, int fd);

public:

	//Create a TCP subserver at specified port

	template <class SubServerType>
	bool CreateSubServer(unsigned int port, void *param, unsigned int workerThreadNumber = 0)
	{
		if (workerThreadNumber == 0)
		{
			workerThreadNumber = std::thread::hardware_concurrency();
		}

		auto subServer = std::make_shared<SubServerType>();
		subServer->Initialize(workerThreadNumber);

		IncreaseReference();
		subServer->SetServer(this);
		if (subServer->CreateListeningHandler(port))
		{
			subServer->_serverParam = param;

			_subServers.lock(port);
			_subServers[port] = subServer;
			_subServers.unlock(port);
		}
		else
		{
			DecreaseReference();
			std::wcout << "Failed to create TCP subserver at port " << port << std::endl;
		}
	}

	void TerminateSubServer(unsigned int port);

	void Start();
	
};

