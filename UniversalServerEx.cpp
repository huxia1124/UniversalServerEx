

#include <iostream>
#include "Server.h"
#include "STXProtocol.h"
using namespace std;

class MyClientContext : public ClientContext
{

};

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

class MySubServer2 : public SubServer
{

protected:

	virtual size_t isClientDataReadable(ClientContext *clientContext)
	{
		//TODO: return the size of a single package
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
		std::cout << len << " bytes of data received in MySubServer! thread=" << tid << ", IP=" << clientContext->GetPeerIP() << std::endl;

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
	CSTXProtocol p;

	p.AppendData(u"ABC");
	p.AppendUnicodeString(u"12201");
	
	char sz[] = "Hardware concurrency=";
	cout << sz << std::thread::hardware_concurrency() << endl;


	Server server;
	server.CreateSubServer<MySubServer>(9988, nullptr);
	server.CreateSubServer<MySubServer2>(9987, nullptr);

	server.Start();

	return 0;
}