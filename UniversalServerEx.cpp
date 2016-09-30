

#include <iostream>
#include "Server.h"
#include "STXProtocol.h"
using namespace std;

class MyClientContext : public ClientContext
{

};

//////////////////////////////////////////////////////////////////////////
//  
// Sample Subserver 1
// Stream-based. data is not organized as packages but just bytes
// onClientReceived will be called whenever data is available.

class MySubServer : public SubServer
{

protected:
	virtual ClientContext* onCreateNewClientContext() override
	{
		return new MyClientContext();
	}

	virtual size_t isClientDataReadable(ClientContext *clientContext)
	{
		//TODO: return the size of a single package
		auto &buf = clientContext->GetReceiveBuffer();
		return buf.GetDataLength();
	}

	virtual void onClientReceived(ClientContext *clientContext, char *buffer, size_t len) override
	{
		auto tid = std::this_thread::get_id();
		std::cout << len << " bytes of data received in MySubServer! thread=" << tid << ", IP=" << clientContext->GetPeerIP() << std::endl;

		SendDataToClient(clientContext, buffer, len);
	}
};

//////////////////////////////////////////////////////////////////////////
//  
// Sample Subserver 2
// Each package has a length field at the beginning. The length field is fixed to be of 2 bytes
// onClientReceived will only be called when a while package is received

class MySubServer2 : public SubServer
{

protected:

	virtual size_t isClientDataReadable(ClientContext *clientContext)
	{
		auto &buf = clientContext->GetReceiveBuffer();

		if (buf.GetDataLength() < sizeof(uint16_t))
			return 0;

		uint16_t wSize = *((uint16_t*)buf.GetReadPtr());
		if (wSize <= buf.GetDataLength())
			return wSize;
	}

	virtual void onClientReceived(ClientContext *clientContext, char *buffer, size_t len) override
	{
		auto tid = std::this_thread::get_id();
		std::cout << len << " bytes of data received in MySubServer2! thread=" << tid << ", IP=" << clientContext->GetPeerIP() << std::endl;

		SendDataToClient(clientContext, buffer, len);
	}
};


//////////////////////////////////////////////////////////////////////////
//  
// Sample Subserver 3
// Each package has a length field at the beginning but the length field can be one or more bytes
// See CSTXProtocol class about how it is handled.

class MySubServer3 : public SubServer
{

protected:

	virtual size_t isClientDataReadable(ClientContext *clientContext)
	{
		auto &buf = clientContext->GetReceiveBuffer();
		unsigned char nLengthBytes = 0;
		long packageSize = CSTXProtocol::DecodeCompactInteger(buf.GetReadPtr(), &nLengthBytes);

		if (packageSize < 0)
			return 0;
		if (packageSize + nLengthBytes > buf.GetDataLength())
			return 0;

		return packageSize + nLengthBytes;
	}

	virtual void onClientReceived(ClientContext *clientContext, char *buffer, size_t len) override
	{
		auto tid = std::this_thread::get_id();
		std::cout << len << " bytes of data received in MySubServer3! thread=" << tid << ", IP=" << clientContext->GetPeerIP() << std::endl;

		CSTXProtocol p;
		p.Decode(buffer, nullptr);

		if (p.GetNextFieldType() == STXPROTOCOL_DATA_TYPE_UTF8 || p.GetNextFieldType() == STXPROTOCOL_DATA_TYPE_UNICODE)
		{
			std::string s = p.GetNextString();
			std::cout << s << std::endl;
		}


		SendDataToClient(clientContext, buffer, len);
	}
};

int main(int argc, char *argv[])
{
	char sz[] = "Hardware concurrency=";
	cout << sz << std::thread::hardware_concurrency() << endl;

	Server server;
	server.CreateSubServer<MySubServer>(9988, nullptr);
	server.CreateSubServer<MySubServer3>(9987, nullptr);
	server.CreateSubServer<MySubServer3>(9986, nullptr);

	server.Start();

	return 0;
}