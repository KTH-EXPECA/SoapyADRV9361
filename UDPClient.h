#include <vector>
#include <string>
#include <stdexcept>
#include <errno.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <signal.h>
#include <iostream>

using namespace std;

// srsLTE
#define DEFAULT_RX_BUFFER_SIZE (5760+6)
#define DEFAULT_TX_BUFFER_SIZE (5760+1500+6)

#define DUMMYBUF_SIZE_BYTE 4096*4

// OAI 
//#define DEFAULT_RX_BUFFER_SIZE (7680+6)
//#define DEFAULT_TX_BUFFER_SIZE (7680+1500+6)

#define MESSAGE_LENGTH_CHAR 1024

class UDPClient
{
	public:
		UDPClient(int _serverCommandPort,int _serverStreamPort, string _serverIP,int _rxBufferSizeByte,int _txBufferSizeByte);
		~UDPClient();
		static UDPClient* findServer(int _serverCommandPort,int _serverStreamPort, string _serverIP,int _rxBufferSizeByte,int _txBufferSizeByte);
		string sendCommand(string cmd);
		int sendStreamBuffer(char* pBuffer);
		int receiveStreamBuffer(char* pBuffer);
		void setTXBufferSizeByte(int sizeByte);
		void setRXBufferSizeByte(int sizeByte);
		int getTXBufferSizeByte() { return txBufferSizeByte; }
		int getRXBufferSizeByte() { return rxBufferSizeByte; }
		int getServerCommandPort() { return serverCommandPort; }
		int getServerStreamPort() { return serverStreamPort; }
		int getSentCount() { return send_count; }
		int getRecvCount() { return recv_count; }
		void initProcedure();
	private:
		struct sockaddr_in servCMDAddr;
		struct sockaddr_in servSTRAddr;
		int serverCommandPort;
		int serverStreamPort;
		int streamSocket;
		int commandSocket;
		int rxBufferSizeByte;
		int txBufferSizeByte;
		string serverIP;
		struct timeval tv;
		int send_count;
		int recv_count;
};

