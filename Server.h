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

//////////////////////////////////////////////////////////////////////////
class Server;


class SubServer
{
	friend class Server;
public:
	SubServer(unsigned int workerThreadNumber = 4);
	~SubServer();

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

	std::mutex mtx;
	std::condition_variable listenReady;
	bool _listenerReady = false;

private:
	unsigned int _workerThreadIndexBase = 0;

protected:
	virtual void onThreadStart(size_t threadIndex);
	virtual void onThreadEnd(size_t threadIndex);
	virtual void onThreadRun(size_t threadIndex);

private:
	void SetServer(Server *server);
	void CreateWorkerThreads();
	bool CreateListeningHandler(unsigned int port);
	event_base *GetNextAvailableBase();
	void ReleaseWorkerThreads();

public:

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
	CSTXHashMap<unsigned int, std::shared_ptr<SubServer>> _subServers;
	
protected:
	void IncreaseReference();
	void DecreaseReference();
	
public:

	//Create a TCP subserver at specified port
	bool CreateSubServer(unsigned int port, void *param);

	void Start();
	
};

