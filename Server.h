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

//////////////////////////////////////////////////////////////////////////

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
	event **_workerDefaultEvents;
	std::vector<std::shared_ptr<std::thread>> _workerThreads;
	std::shared_ptr<std::thread> _listeningThread;
	unsigned int _workerThreadNumber;

private:
	unsigned int _workerThreadIndexBase = 0;

protected:
	virtual void onThreadStart(size_t threadIndex);
	virtual void onThreadEnd(size_t threadIndex);
	virtual void onThreadRun(size_t threadIndex);

private:
	void CreateWorkerThreads();
	bool CreateListeningHandler(unsigned int port);
	event_base *GetNextAvailableBase();

public:

};

//////////////////////////////////////////////////////////////////////////

class Server
{
public:
	Server();
	virtual ~Server();

protected:
	std::mutex _mtxShutdown;
	std::condition_variable _shutdownVariable;

protected:
	CSTXHashMap<unsigned int, std::shared_ptr<SubServer>> _subServers;
	
	
public:
	bool CreateSubServer(unsigned int port, void *param);

	void Start();
	
};

