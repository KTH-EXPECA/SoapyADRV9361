#include "Controller.h"


string Controller::runCommand(string cmdStr)
{
	// Convert to lower case
	transform(cmdStr.begin(), cmdStr.end(), cmdStr.begin(),[](unsigned char c){ return tolower(c); });

	// Parse the lower case string
	stringstream ss(cmdStr);
	istream_iterator<string> begin(ss);
	istream_iterator<string> end;
	vector<string> vstrings(begin, end);

	enum iodev d;
	char* endptr = NULL;

	string response;

	try
	{

		// Switch case on the commands
		if (vstrings[1]== "rx")
	  	{
			d = RX;
		}
		else if (vstrings[1]=="tx")
		{
			d = TX;
		}
		else
		{
			throw runtime_error("wrong RX/TX command");
		}

		if (vstrings[0]=="set")
	  	{
			if(vstrings[2]=="antenna")
			{
				struct stream_cfg conf = dev->getConfig(d);
				conf.rfport = vstrings[3];
				dev->setConfig(conf,d);
			}
			else if(vstrings[2]=="gain")
			{
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);
				dev->setGain(d,val);
			}
			else if(vstrings[2]=="frequency")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.lo_hz = val;

				// configure
				dev->setConfig(conf,d);

			}
			else if(vstrings[2]=="samplerate")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.fs_hz = val;

				// configure
				dev->setConfig(conf,d);
			}
			else if(vstrings[2]=="bandwidth")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.bw_hz = val;

				// configure
				dev->setConfig(conf,d);

			}
			else
			{
				throw runtime_error("wrong set command");
			}

		}
		else if (vstrings[0]=="get")
		{
			string res;
			if(vstrings[2]=="antenna")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				res = conf.rfport;
			}
			else if(vstrings[2]=="gain")
			{
				long long gainVal = dev->getGain(d);
				res = to_string(gainVal);
			}
			else if(vstrings[2]=="frequency")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				res = to_string(conf.lo_hz);
			}
			else if(vstrings[2]=="samplerate")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				res = to_string(conf.fs_hz);

			}
			else if(vstrings[2]=="bandwidth")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				res = to_string(conf.bw_hz);
			}
			else
			{
				throw runtime_error("wrong get command");
			}
	  	}
		else if (vstrings[0]=="start")
		{
			// first a clean stop
			stop(d);

			// enable streaming
			dev->enableChannels(d);

			// Start streaming thread
			switch (d)
			{
		        	case RX:
			        {
					// Start the rx streamer thread
					if(pthread_create(&rx_thread, NULL, &Controller::streamRX,this) != 0)
					        throw runtime_error("Unable to start stream RX thread");

		                	break;
		        	}
			        case TX:
			        {
					// Start the tx streamer thread
					if(pthread_create(&tx_thread, NULL, &Controller::streamTX,this) != 0)
					        throw runtime_error("Unable to start stream TX thread");

		                	break;
		        	}
			}

	  	}
		else if (vstrings[0]=="stop")
		{
			stop(d);
	  	}
		else
		{
			throw runtime_error("Wrong command, not parsable");
		}

		response = "done";
	}
	catch(runtime_error& re)
	{
		response = string("Runtime error: ") + re.what();
	}

	return response;
}

void* Controller::streamRX(void* controller)
{
        Controller* p = static_cast<Controller*>(controller);
	
	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamRX.");
	
	p->rx_thread_active = true;

	try
	{
		while(p->rx_thread_active)
                {
			// get the buffer from IIO
			char* buffer = p->dev->receiveBuffer();
			// send it to the network
			p->server->sendStreamBuffer(buffer);
		}

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }

	// Announce
	p->rx_thread_active = false;
	printf("Controller RX thread is stopped\n");

}

void* Controller::streamTX(void* controller)
{
        Controller* p = static_cast<Controller*>(controller);

	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamTX.");

	p->tx_thread_active = true;
	
	try
	{
		while(p->tx_thread_active)
                {
			// get the IIO buffer pointer
			char* buffer = p->dev->getTXBufferPointer();
			// fill it from the network
			p->server->receiveStreamBuffer(buffer);
			// push IIO buffer
			p->dev->sendBufferFast();
		}
	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }

	// Announce
	p->tx_thread_active = false;
	printf("Controller TX thread is stopped\n");
}

void Controller::stop(enum iodev d)
{
	if(d==RX)
	{
		// Stop the rx streamer thread
	        rx_thread_active = false;
        	pthread_join(rx_thread, NULL);
		
		// Stop streaming
	        dev->disableChannels(RX);
        
	}
	else if(d==TX)
	{
		// Stop the tx streamer thread
	        tx_thread_active = false;
        	pthread_join(tx_thread, NULL);
		
		// Stop streaming
	        dev->disableChannels(TX);
	}
}
