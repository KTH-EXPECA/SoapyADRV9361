#include "UDPServer.h"

UDPServer::UDPServer(int _commandPort,int _streamPort, Controller* _controller) :
	commandPort(_commandPort), streamPort(_streamPort), rxBufferSizeByte(DUMMYBUF_SIZE_BYTE), txBufferSizeByte(DUMMYBUF_SIZE_BYTE), controller(_controller)
{
	send_count = 0;
	recv_count = 0;
	recv_fr_count = 0;
	send_fr_count = 0;
	clientInitiated = false;

	command_thread_active = true;

        memset(&servCMDAddr, 0, sizeof(servCMDAddr));
        servCMDAddr.sin_family = AF_INET;                  // Internet address family
        servCMDAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        servCMDAddr.sin_port = htons(commandPort);           // Local port

        // Create socket for receiving datagrams
        if ((commandSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the command socket");

        // Set the socket as reusable
        int true_v = 1;
        if (setsockopt(commandSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the command socket reusable");

        // Bind to the local address
        if (bind(commandSocket, (struct sockaddr *) &servCMDAddr, sizeof(servCMDAddr)) < 0)
                throw runtime_error("Unable to bind the socket");

       	// Streaming  
	memset(&servSTRAddr, 0, sizeof(servSTRAddr));
        servSTRAddr.sin_family = AF_INET;                  // Internet address family
        servSTRAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        servSTRAddr.sin_port = htons(streamPort);           // Local port
	
	// Create socket for receiving datagrams
        if ((streamSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the command socket");

        // Set the socket as reusable
        true_v = 1;
        if (setsockopt(streamSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the command socket reusable");

        // Bind to the local address
        if (bind(streamSocket, (struct sockaddr *) &servSTRAddr, sizeof(servSTRAddr)) < 0)
                throw runtime_error("Unable to bind the socket");

	// Init controller server
	controller->setServer(this);
	
	// A clean controller stop
	controller->stop(RX);
	controller->stop(TX);
	
	// Start the command runner thread (waits for the client "init" command)
        //if(pthread_create(&command_thread, NULL, &UDPServer::runCommands,this) != 0)
        //        throw runtime_error("Unable to start command thread");


}

UDPServer::~UDPServer()
{
	command_thread_active = false;
	pthread_join(command_thread, NULL);
	close(commandSocket);
	close(streamSocket);
}

void UDPServer::initClient(struct sockaddr_in cliAddr)
{
	// stop controller
	controller->stop(RX);
	controller->stop(TX);

	// Reset buffer size
	rxBufferSizeByte = DUMMYBUF_SIZE_BYTE;
	txBufferSizeByte = DUMMYBUF_SIZE_BYTE;

	// set the new address
	clntCMDAddr = cliAddr;

	// Send the response
	string a = string("ack");
        int sendMsgSize = sendto(commandSocket, a.c_str(), a.length(), 0,(struct sockaddr*) &(clntCMDAddr), sizeof(clntCMDAddr));
        if (sendMsgSize<0)
        	throw runtime_error("Unable to respond to commands");

        // Blocking receive of the first stream buffer (dummy)
        char dummy[rxBufferSizeByte];
	unsigned int cliAddrLen = sizeof(clntSTRAddr);
	int recvDummySize = recvfrom(streamSocket, dummy, rxBufferSizeByte, 0,(struct sockaddr *) &(clntSTRAddr), &cliAddrLen);
        if(recvDummySize < 0)
                throw runtime_error("Failed to receive the dummy buffer");
	
	// Printout the result
	char ip[INET_ADDRSTRLEN];
	uint16_t cmdport;
	uint16_t strport;

	inet_ntop(AF_INET, &(clntCMDAddr.sin_addr), ip, sizeof(ip));
	cmdport = htons(clntCMDAddr.sin_port);
	strport = htons(clntSTRAddr.sin_port);

	clientInitiated = true;

	printf("UDPServer is connected to %s, cmd port: %d, str port: %d, continue...\n",ip,cmdport,strport);
}

void* UDPServer::runCommands(void* server)
{
	UDPServer* p = static_cast<UDPServer*>(server);
	try
	{
		while(p->command_thread_active)
		{
			int ret;
			char msg[MESSAGE_LENGTH_CHAR];
			struct sockaddr_in cliAddr;
			unsigned int cliAddrLen = sizeof(cliAddr);
			if ((ret = recvfrom(p->commandSocket, msg, MESSAGE_LENGTH_CHAR, 0,(struct sockaddr *) &(cliAddr), &cliAddrLen)) < 0 )
        	        	throw runtime_error("Unable to receive first command");
			
			if(ret == 0)
			{
				cout << "UDP empty datagram received, continue..." << endl;
				continue;
			}


			if(string(msg,ret)=="init")
			{
				// A new client is there, fix the addresses
				p->initClient(cliAddr);
				continue;
			}
	
			if(!p->clientInitiated)
			{
				// Client init did not happen
				cout << "Client has not been initialized, continue..." << endl;
				continue;
			}
				
			// Send the string up! Process the command
			string response = p->controller->runCommand(string(msg,ret));

			// Send back the response
			int sendMsgSize = sendto(p->commandSocket, response.c_str(), response.length(), 0,(struct sockaddr*) &(p->clntCMDAddr), sizeof(p->clntCMDAddr));
                	if (sendMsgSize<0)
        	        	throw runtime_error("Unable to respond to the incomming commands");
	
		}
	}
	catch(runtime_error& re)
	{
		cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
	}

	// call the upper layer to stop everything
	p->controller->stop(RX);
	p->controller->stop(TX);
		
	close(p->commandSocket);
	close(p->streamSocket);
	
	printf("Network UDP command serving thread is stopped\n");
}

void UDPServer::runCommand()
{
	int ret;
	char msg[MESSAGE_LENGTH_CHAR];
	memset(msg,0,MESSAGE_LENGTH_CHAR);
	struct sockaddr_in cliAddr;
	unsigned int cliAddrLen = sizeof(cliAddr);
	ret = recvfrom(commandSocket, msg, MESSAGE_LENGTH_CHAR, 0,(struct sockaddr *) &(cliAddr), &cliAddrLen);
	if (ret <= 0)
               	throw runtime_error("Unable to receive first command");

	//cout << "char: " << ret << " " << msg << endl;
	
	char res[ret];
	strcpy(res,msg);
	
	string smsg;
	smsg.assign(res,ret);
	
	//cout << "Received cmdstr: " << smsg << endl;
	
	if(ret == 0)		
	{
		cout << "UDP empty datagram received, continue..." << endl;
		return;
	}


	if(smsg=="init")
	{
		// A new client is there, fix the addresses
		initClient(cliAddr);
		return;
	}
	
	if(!clientInitiated)
	{
		// Client init did not happen
		cout << "Client has not been initialized, continue..." << endl;
		return;
	}
				
	// Send the string up! Process the command
	string response = controller->runCommand(smsg);

	// Send back the response
	int sendMsgSize = sendto(commandSocket, response.c_str(), response.length(), 0,(struct sockaddr*) &(clntCMDAddr), sizeof(clntCMDAddr));
        if (sendMsgSize<0)
        	throw runtime_error("Unable to respond to the incomming commands");
}

int UDPServer::sendStreamBuffer(char* pBuffer)
{
	int ret = sendto(streamSocket, pBuffer, txBufferSizeByte, 0, (struct sockaddr*) &(clntSTRAddr), sizeof(clntSTRAddr));
	if(ret < 0)
        	throw runtime_error("Sending the buffer failed");
	
	send_count += ret;
	send_fr_count ++;
	return ret;
}

unsigned long timePrev = 0;

int UDPServer::receiveStreamBuffer(char* pBuffer)
{
	int ret = recv(streamSocket, pBuffer, rxBufferSizeByte, 0);
        if((ret < 0) || (ret != rxBufferSizeByte))
        	throw runtime_error("Receiving the buffer faild");
	
	recv_count += ret;
	recv_fr_count ++;
	return ret;
}

// Debug function
uint16_t oldid = 0;
uint16_t accdrops = 0;
char tpBuffer[(5760+1500+6)*4];
int UDPServer::receiveStreamDiscard()
{
        int ret = recv(streamSocket, tpBuffer, (5760+1500+6)*4, 0);
        if((ret < 0) || (ret != ((5760+1500+6)*4)))
                throw runtime_error("Receiving the buffer faild");

        // Just a little test for reading the id
        uint16_t* txidp = (uint16_t*)(tpBuffer + (5760+1500)*4 + 4);
        if(*txidp - oldid > 1)
        {
        	accdrops += *txidp - oldid -1;
                cout << "Drops: "<< accdrops << "/" << *txidp << endl;
        }
        oldid = *txidp;
	
        recv_count += ret;
        recv_fr_count ++;
        return ret;
}

// Debug function
uint64_t rxIdCounter = 0;
uint64_t rxTimestamp = 0;
uint64_t rxIdOffset = 0;
vector<uint64_t> rxDrops;
int UDPServer::sendStreamDiscard(char* buffer)
{
	int bufferSize = txBufferSizeByte;
	double ts_to_ns = 10;
	double rxSamplingFreq = 1920000;
	double bufferDurationNS = (((double)(bufferSize/4)-6.000)/(rxSamplingFreq))*((double)1e9);

	// Read the RX timestamp
        uint64_t* pRxTimestamp = (uint64_t*)(buffer+bufferSize-8);
        uint64_t rxT = ((double)(*pRxTimestamp))*((double)ts_to_ns);

        // Read RX link stats data
        uint64_t* pRxIdCounter = (uint64_t*)(buffer+bufferSize-24);
        if (rxIdCounter == 0)
        {
        	rxIdOffset = (*pRxIdCounter)-1;
		rxTimestamp = rxT;
       	}
        uint64_t rxIdC = (*pRxIdCounter) - rxIdOffset;

        if(rxIdC - rxIdCounter > 1)
        {
        	for(uint64_t i = rxIdCounter+1; i<rxIdC; i++)
		{
                	rxDrops.push_back(i);
			cout << "DROPPED id: " << i << endl;
		}
        }
        rxIdCounter = rxIdC;

	int diffNS = rxT - rxTimestamp;
        int diffMS = (int)round((double)diffNS/bufferDurationNS);
	if(diffMS > 1)
	{
		cout << "UNDERFLOW number: " << diffMS << endl;
	}
	rxTimestamp = rxT;
	
	int ret = txBufferSizeByte;
	send_count += ret;
	send_fr_count++;
	return ret;
}


void UDPServer::setTXBufferSizeByte(int size)
{
	if(size > 65535)
		throw runtime_error("Unable to set larger than 65535 buffer size TX");

	txBufferSizeByte = size;
}

void UDPServer::setRXBufferSizeByte(int size)
{
	if(size > 65535)
		throw runtime_error("Unable to set larger than 65535 buffer size RX");

	rxBufferSizeByte = size;
}
