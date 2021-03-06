/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
#define SERIAL_DEBUG

#include "RF24Network_config.h"
#include "RF24.h"
#include "RF24Mesh.h"
#include "RoutingTable.h"


uint16_t RF24NetworkHeader::next_id = 1;

unsigned long last_time_sent;

RoutingTable rTable;

// Array of nodes we are aware of
const short max_active_nodes = 10;
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

uint64_t pipe_address( uint16_t node, uint8_t pipe );
bool is_valid_address( uint16_t node );




/******************************************************************/

RF24Mesh::RF24Mesh( RF24& _radio, StatusCallback& _callback ): radio(_radio), callback(_callback), receive_frame(receive_queue), send_frame_p(send_queue), state(INIT), state_time(0)
{
	last_join_time = 0;
}

/******************************************************************/

void RF24Mesh::begin(uint8_t _channel, T_IP _node_address )
{

  node_address = _node_address;
  rTable.setCurrentNode(node_address);

  if(rTable.amImaster())
	  state = JOINED;

  radio.begin();
  // Set up the radio the way we want it to look
  radio.setChannel(_channel);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setCRCLength(RF24_CRC_8);
  radio.setRetries(5,15);


  radio.openReadingPipe(0, rTable.getBroadcastMac());
  radio.openReadingPipe(1, rTable.getMac(_node_address));

  radio.setAutoAck(0, false);
  radio.setAutoAck(1, true);

  radio.startListening();

  // Spew debugging state about the radio
  radio.printDetails();
}

/*************************************************************/

void RF24Mesh::loop(void)
{
	static int i=0;

	fastloop();

	i++;

	//if(i == 10)
	{
		slowloop();
		i = 0;
	}

}


void RF24Mesh::fastloop()
{
	// Pump the network regularly
	  updateNetworkTopology();

	  listenRadio();

	  handlePacket();
}

// TODO slow loop icin sure ve timer ayarlanacak
void RF24Mesh::slowloop()
{
	handlePacket();

	sendPackets();
}
/******************************************************************/
void RF24Mesh::updateNetworkTopology(void)
{
	if (!rTable.amImaster() && (isState(INIT) || isState(NJOINED)))
	{
			setState(NJOINED);
			joinNetwork();
			setState(SENDJOIN);

			IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Join network called last_join_time: %lu \n\r"),rTable.getMillis(),last_join_time));
	}
	else if (!rTable.amImaster() && (isState(JOINED)))
		{
			if (last_join_time == 0 || (millis() - last_join_time) > JOIN_DURATION) //bir dakika
			{
				setState(NJOINED);
				joinNetwork();
				setState(SENDJOIN);

				IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Join network called last_join_time: %lu \n\r"),rTable.getMillis(),last_join_time));
			}
		}
	else if (isState(NEW_JOINED))
	{
		setState(JOINED);
		last_join_time = millis();
		send_UpdateWeight();
	}
	else if (isState(SENDJOIN))
	{
		if (millis() - state_time >= JOIN_WAIT_WELCOME*2)
		{
			sendAckToWelcome(); //TODO sanki buna gerek yok zaten ici bos
			if(rTable.amIJoinedNetwork())
			{
				setState(JOINED);
				last_join_time = millis();
				IF_SERIAL_DEBUG(printf_P(PSTR("%lu: I have joined the network: %lu \n\r"),rTable.getMillis(),last_join_time));
			}
			else
			{
				setState(NJOINED);
				IF_SERIAL_DEBUG(printf_P(PSTR("%lu: I could ****NOT*** joined the network: %lu \n\r"),rTable.getMillis(),last_join_time));
			}
		}
	}

/*	else if(isState(JOINRECEIVED))
	{
		if(rTable.isPathShortened())
		{
			rTable.connectShortened();
			send_JoinMessage();
			setState(SENDJOIN);
		}
		else
		{
			sendWelcomeToJoin();
		}
	}
*/

}

void RF24Mesh::sendAckToWelcome()
{
	//TODO doldur sendAckToWelcome
	int size = rTable.getNumOfWelcomes();
	T_IP ips[size];

	for(int i=0;i<size;i++)
	{
		//send_UpdateWeight(ips[i]);
		//rTable.setConnected(ips[i]);
	}

}

void RF24Mesh::sendWelcomeToJoin()
{
	//TODO doldur. sendWelcomeToJoin
	int size = rTable.getNumOfJoines();
	T_IP ips[size];

	for(int i=0;i<size;i++)
	{
		//send_WelcomeMessage(ips[i]);
		//rTable.setWelcomeMessageSent(ips[i]);

	}
}
void RF24Mesh::handlePacket()
{
	int count=0;
	// Is there anything ready for us?
  while ( available() )
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("There are available received message %d \n\r"),count++));

    // If so, take a look at it 
    RF24NetworkHeader header;
    peek(header);

    // Dispatch the message to the correct handler.
    switch (header.type)
    {
	case 'J':
		handle_JoinMessage(header);
		break;
    case 'W':
      handle_WelcomeMessage(header);
      break;
    case 'D':
      handle_DataMessage(header);
      break;
	case 'F':
	  handle_ForwardData(header);
	  break;
	case 'U':
	  handle_UpdateWeightMessage(header);
	  break;
    default:
	  printf_P(PSTR("*** WARNING *** Unknown message type %s\n\r"),header.toString());
      read(header,0,0);
      break;
    };
  }

}

void RF24Mesh::sendPackets()
{
	static int error_rate = 0;
	// Is there anything ready for us?
	  while ( send_available() )
	  {
		  if(write() == false)
			  error_rate++;
		  else
			  error_rate = 0;
	  }

	  if (error_rate > 4)
	  {
		  setState(NJOINED);
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Fazla gonderme hatasi oldugu icin network dustu\n\r"),rTable.getMillis()));
		  error_rate = 0;
	  }
}

void RF24Mesh::listenRadio()
{
	long stime = millis();
	bool wait = true;

  while(wait)
  {

		// if there is data ready
	  uint8_t pipe_num;
	  while ( radio.available(&pipe_num) )
	  {
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET radio available pipe: %x\n\r"),rTable.getMillis(),pipe_num));

		// Dump the payloads until we've gotten everything
		boolean done = false;
		while (!done)
		{
		  // Fetch the payload, and see if this was the last one.
		  done = radio.read( frame_buffer, sizeof(frame_buffer) );
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: done is %s"),rTable.getMillis(), (done ? "true":"false")));

		  // Read the beginning of the frame as the header
		  RF24NetworkHeader& header = * reinterpret_cast<RF24NetworkHeader*>(frame_buffer);

		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: MAC Received on pipe %u %s\n\r"),rTable.getMillis(),pipe_num,header.toString()));

		  // Is this for us?
		  if ( header.to_node == rTable.getCurrentNode().ip || header.to_node == rTable.getBroadcastNode().ip)
		  {
			    IF_SERIAL_DEBUG(printf_P(PSTR("%lu: MAC Received message for me, enqueuing \n\r"),rTable.getMillis()));
				// Add it to the buffer of frames for us
				enqueue();

				//goker handlePacket();
		  }
		  else
		  {
			  printf_P(PSTR("%lu: MAC Received message *****NOT for me**, *WARNING* wrong message not forwarding %d != %d \n\r"), rTable.getMillis(), header.to_node, rTable.getCurrentNode().ip);
		  }



		}
	  }

	if(state == SENDJOIN)
	{
			wait = (millis() - stime > JOIN_WAIT_WELCOME) ? false : true;
	}
	else
	{
		//wait = (millis()-stime>1000)?false:true;
		wait = false;
	}
  }
}
void RF24Mesh::joinNetwork()
{
	rTable.cleanTable();
	send_JoinMessage();
	//listenRadio(true);
}
bool RF24Mesh::isJoined()
{
	return rTable.amIJoinedNetwork();
}
/******************************************************************/

bool RF24Mesh::enqueue(void)
{
  bool result = false;
  
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Enqueue @%x "),rTable.getMillis(),receive_frame-receive_queue));

  // Copy the current frame into the frame queue
  if ( receive_frame < receive_queue + sizeof(receive_queue) )
  {
    memcpy(receive_frame,frame_buffer, frame_size );
    receive_frame += frame_size; 

    result = true;
    IF_SERIAL_DEBUG(printf_P(PSTR("ok\n\r")));
  }
  else
  {
    printf_P(PSTR("failed\n\r"));
  }

  return result;
}

bool RF24Mesh::send_enqueue()
{
	bool result = false;

	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Send Enqueue @%x "),rTable.getMillis(),send_frame_p-send_queue));

	// Copy the current frame into the frame queue
	if (send_frame_p < send_queue + sizeof(send_queue))
	{
		memcpy(send_frame_p, frame_buffer, frame_size);
		send_frame_p += frame_size;

		result = true;
		IF_SERIAL_DEBUG(printf_P(PSTR("copied to send buffer\n\r")));
	}
	else
	{
		printf_P(PSTR("*******WARNING!!!!!!******* failed to write send queue Memory is not enough for message buffer\n\r"));
	}

	return result;
}
/******************************************************************/

bool RF24Mesh::available(void)
{
  // Are there frames on the queue for us?
  return (receive_frame > receive_queue);
}

bool RF24Mesh::send_available(void)
{
  // Are there frames on the queue for us?
  return (send_frame_p > send_queue);
}
/******************************************************************/

void RF24Mesh::peek(RF24NetworkHeader& header)
{
  if ( available() )
  {
    // Copy the next available frame from the queue into the provided buffer
    memcpy(&header,receive_frame-frame_size,sizeof(RF24NetworkHeader));
  }
}

/******************************************************************/

size_t RF24Mesh::read(RF24NetworkHeader& header,void* message, size_t maxlen)
{
  size_t bufsize = 0;

  if ( available() )
  {
    // Move the pointer back one in the queue 
    receive_frame -= frame_size;
    uint8_t* frame = receive_frame;
      
    // How much buffer size should we actually copy?
    bufsize = min(maxlen,frame_size-sizeof(RF24NetworkHeader));

    // Copy the next available frame from the queue into the provided buffer
    memcpy(&header,frame,sizeof(RF24NetworkHeader));
    memcpy(message,frame+sizeof(RF24NetworkHeader),bufsize);
    
    IF_SERIAL_DEBUG(printf_P(PSTR("%lu: *****NET _RF24Mesh::read Received (%s)\n\r"),rTable.getMillis(),header.toString()));
  }

  return bufsize;
}

bool RF24Mesh::write(RF24NetworkHeader& header, T_MAC mac)
{
  // Fill out the header
//	header.from_node = rTable.getCurrentNode().ip;
	
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET 1 Sending message(%s) \n\r"),rTable.getMillis(),header.toString()));

  // Build the full frame to send
  memcpy(frame_buffer,&header,sizeof(RF24NetworkHeader));

  // If the user is trying to send it to himself
  if ( header.to_node == rTable.getCurrentNode().ip )
    // Just queue it in the received queue
    return enqueue();
  else
    // Otherwise send it out over the air
	return send_enqueue(); //write(mac);
}

/******************************************************************/

bool RF24Mesh::write(RF24NetworkHeader& header)
{
  // Fill out the header
	header.from_node = rTable.getCurrentNode().ip;
	
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET 2 Sending message(%s) \n\r"),rTable.getMillis(),header.toString()));

  // Build the full frame to send
  memcpy(frame_buffer,&header,sizeof(RF24NetworkHeader));

  // If the user is trying to send it to himself
  if ( header.to_node == rTable.getCurrentNode().ip )
    // Just queue it in the received queue
    return enqueue();
  else
    // Otherwise send it out over the air
	  return send_enqueue(); //write(mac); return write(rTable.getMac(header.to_node));
}

/******************************************************************/
int RF24Mesh::write()
{
	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: write to air \n\r"),rTable.getMillis()));
	  bool result = false;

	  if ( send_available() )
	  {
		shiftleft(send_queue, frame_size, frame_buffer, send_frame_p);

		  // Move the pointer back one in the queue
	    send_frame_p -= frame_size;

	    RF24NetworkHeader h;
	    // Copy the next available frame from the queue into the provided buffer
	    memcpy(&h,frame_buffer,sizeof(RF24NetworkHeader));

	    result = write(rTable.getMac(h.to_node));
	    rTable.sentData(h);
	    IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET *RF24*Mesh::sent to Air to mac: %lx (%s)\n\r"),rTable.getMillis(), rTable.getMac(h.to_node), h.toString()));
	  }

	  return result;
}

void RF24Mesh::shiftleft(uint8_t *send_queue, const int frame_size, uint8_t *frame_buffer,  uint8_t *last_pointer)
{
	memcpy(frame_buffer, send_queue, frame_size);

	uint8_t* p1 = send_queue;
	uint8_t* p2 = send_queue + frame_size;

	while (p2<=last_pointer)
	{
		memcpy(p1, p2, frame_size);
		p1 += frame_size;
		p2 += frame_size;
	}

}

bool RF24Mesh::write(T_MAC to_mac)
{
  bool ok = false;
  
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Trying to write mac %lu \n\r"),rTable.getMillis(),to_mac) );

  // First, stop listening so we can talk
  radio.stopListening();
 
  if (to_mac != rTable.getBroadcastMac())
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: It is not broadcast mac so acknowledge is true %lu \n\r"),rTable.getMillis(),to_mac) );
	  radio.setAutoAck(0,true);
  }
  else
	  radio.setAutoAck(0,false);

  // Open the correct pipe for writing.  
  radio.openWritingPipe(to_mac);

  // Retry a few times
  short attempts = 15;
  do
  {
    ok = radio.write( frame_buffer, frame_size );
    IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Tried to send packet result:%d attempt %d \n\r"),rTable.getMillis(),ok, attempts) );
  }
  while ( !ok && --attempts );

  // Now, continue listening
  radio.startListening();
  radio.setAutoAck(0,false);

  if(!ok)
	  callback.sendingFailed(to_mac);

  return ok;
}




/******************************************************************/



/******************************************************************/



/******************************************************************/



/******************************************************************/


/******************************************************************/

bool RF24Mesh::is_valid_address( uint16_t node )
{
  bool result = true;


  return result;
}

void RF24Mesh::setState(STATES s)
{
	IF_SERIAL_DEBUG(printf_P(PSTR("setState %d\n\r"), s));

	state = s;
	state_time = millis();
}

bool RF24Mesh::isState(STATES s)
{
	return state == s;
}
/******************************************************************/

/**
 * Send a 'T' message, the current time
 */
bool RF24Mesh::send_JoinMessage()
{

	uint64_t data = 0;
	RF24NetworkHeader header(rTable.getBroadcastNode().ip, 'J', data, rTable.getCurrentNode().ip);
  
	header.source_data.ip = rTable.getCurrentNode().ip;
	header.source_data.weight = rTable.getCurrentNode().weight;
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu:Sending join message (%s) \n\r"),rTable.getMillis(),header.toString()));
	return write(header);
}

bool RF24Mesh::send_UpdateWeight()
{

	uint64_t data = 0;
	RF24NetworkHeader header(rTable.getBroadcastNode().ip, 'U', data, rTable.getCurrentNode().ip);

	header.source_data.ip = rTable.getCurrentNode().ip;
	header.source_data.weight = rTable.getCurrentNode().weight;
	header.prev_node = rTable.getShortestRouteNode().ip;
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu:Sending update weight (%s) \n\r"),rTable.getMillis(),header.toString()));
	return write(header);
}

bool RF24Mesh::send_WelcomeMessage(T_IP toNode)
{
	unsigned long time = rTable.getMillis();
	uint8_t data[16];

	memset(&data,0,sizeof(unsigned long));
	memcpy(&data,&time,sizeof(unsigned long));

	RF24NetworkHeader header(toNode, 'W',data , rTable.getCurrentNode().ip);
	header.source_data.ip = rTable.getCurrentNode().ip;
	header.source_data.weight = rTable.getCurrentNode().weight;
  
	printf_P(PSTR("---------------------------------\n\r"));
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Sending Welcome Message (%s)...\n\r"),rTable.getMillis(),header.toString()));
	return write(header); //broadcast mac'ine gonder.
}

/**
 * Send a 'T' message, the current time
 */
bool RF24Mesh::send_SensorData(uint8_t data[16])
{
	if(rTable.amImaster())
	{
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: send_SensorData, since I am master i do not send to myself ------------\n\r"),rTable.getMillis()));
		 return true;
	}

	if(state != JOINED)
	{
		printf_P(PSTR("%lu: send_SensorData, I havent joined yet\n\r"),rTable.getMillis());
		callback.sendingFailed(0);
		return false;
	}

	T_IP ip = rTable.getShortestRouteNode().ip;
	unsigned char type = 'D';

	if (ip != rTable.getMasterNode().ip)
	{	type = 'F';
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Send_SersorData short ip: %d masterip: %d"),rTable.getMillis(),ip,rTable.getMasterNode().ip));
	}
	RF24NetworkHeader header(ip,  type, data );
	header.source_data.ip = rTable.getCurrentNode().ip; //source ip
	header.source_data.weight = 0; //not important
  
  
  IF_SERIAL_DEBUG(printf_P(PSTR("---------------------------------\n\r")));
  printf_P(PSTR("%lu: APP Sending send_SensorData %s ...\n\r"),rTable.getMillis(), header.toString());
  bool result = write(header);
  int trycount=0;
  while (!result)
  {
  	  if(!rTable.removeUnreacheable(rTable.getShortestRouteNode()))//There are no neighbour nodes
  	  {
  		  setState(NJOINED);
  		  return false;
  	  }

	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Repeating failed send_SensorData, (%s)"),rTable.getMillis(),header.toString()));
	  result = write(header);
	  trycount++;
	  if(trycount > 10)
		  return false;
  }
  return result;
}



/**
 * Handle a 'J' message
 *
 * Add the node to the list of active nodes
 */
void RF24Mesh::handle_JoinMessage(RF24NetworkHeader& header)
{

  read(header,0,0);
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_JoinMessage (%s) \n\r"),rTable.getMillis(),header.toString()));

  if(state == JOINED)
  {
	  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
	  if ( header.from_node != rTable.getShortestRouteNode().ip )
	  {

		  send_WelcomeMessage(header.source_data.ip);

	  }
	  else //benim bagli oldugum node'dan join mesaji gelmis
	  {

		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: bagli oldugum node Join mesaji gonderdi. Demekki hattan dustum.**********\n\r"),rTable.getMillis()));
		  setState(NJOINED);
		  rTable.cleanTable();
	  }
	  rTable.printTable();
  }


 // if(available()) printf_P(PSTR("%lu: there are more messages  \n\r"),rTable.getMillis());
 //   else printf_P(PSTR("%lu: there is not more message  \n\r"),rTable.getMillis());
}

unsigned short int RF24Mesh::getMyIP()
{
	return rTable.getCurrentNode().ip;
}

void RF24Mesh::handle_UpdateWeightMessage(RF24NetworkHeader& header)
{

  read(header,0,0);
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_UpdateWeightMessage (%s) \n\r"),rTable.getMillis(),header.toString()));

  if(isState(JOINED))
  {
	  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
		if (header.from_node != rTable.getShortestRouteNode().ip
				&& header.prev_node != getMyIP())
	  {
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_U farkli node kaydet ve cevap ver\n\r"),rTable.getMillis()));
		  bool shortenedPath = rTable.addNearNode(header.source_data);
		  if(shortenedPath)
		  {
			  send_UpdateWeight();
		  }
	  }
	  else //msg came from my child about me; omit the msg
	  {

	  }
	  rTable.printTable();
  }
  else if(isState(NJOINED))
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_U farkli node kaydet ve cevap ver\n\r"),rTable.getMillis()));
	  bool shortenedPath = rTable.addNearNode(header.source_data);
	  if(shortenedPath)
	  {
		  setState(JOINED);
		  last_join_time = millis();
		  send_UpdateWeight();
	  }
  }
}
/**
 * Handle a 'T' message
 *
 * Add the node to the list of active nodes
 */
void RF24Mesh::handle_WelcomeMessage(RF24NetworkHeader& header)
{
  //IP_MAC message;
  read(header,0,0);
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_WelcomeMessage (%s) \n\r"),rTable.getMillis(),header.toString()));

//  if(isState(SENDJOIN) || isState(JOINED))
  {
	  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
	  if ( header.from_node != this->node_address)
		  if(rTable.addNearNode(header.source_data))
		  {
			  rTable.setMillis(header.payload);
			  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_WelcomeMessage, update join status\n\r"),rTable.getMillis()));
			  setState(NEW_JOINED);
		  }
	   rTable.printTable();
  }
 // else
  {
//	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: OMITTING WelcomeMessage (%s) \n\r"),rTable.getMillis(),header.toString()));
  }
}

/**
 * Handle an 'N' message, the active node list
 */
void RF24Mesh::handle_DataMessage(RF24NetworkHeader& header)
{
  //uint64_t message;
  read(header,NULL,0);

  if(header.from_node == rTable.getCurrentNode().ip)
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_DataMessage APP I got my own data omitting.(%s)\n\r"),rTable.getMillis(), header.toString()));
  else
  {
	  callback.incomingData(header);
  }

}

void RF24Mesh::handle_ForwardData(RF24NetworkHeader& header)
{
  uint64_t message;
  read(header,NULL,0);

  if(header.from_node == rTable.getCurrentNode().ip)
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_DataMessage APP I got my own data omitting. (%s)\n\r"),rTable.getMillis(),header.toString()));
  else
  {
	  callback.incomingData(header);

	  T_IP ip = rTable.getShortestRouteNode().ip;
	  unsigned char type = 'D';

	  header.source_data.weight++;
	  header.prev_node = header.from_node;
	  header.from_node = rTable.getCurrentNode().ip;
	  header.to_node = ip;
      if (ip != rTable.getMasterNode().ip)
	  {	
		  type = 'F';
		  printf_P(PSTR("%lu: APP handle_ForwardData short ip: %d masterip: %d"),rTable.getMillis(),ip,rTable.getMasterNode().ip);
	  }
	  header.type = type;
 
	  write(header);
  }

}

StatusCallback::StatusCallback()
{
	receivedPacket = 0;
}
void StatusCallback::println(const char * str)
{

}

void StatusCallback::sendingFailed(T_MAC node)
{
	//TODO buraya fail sebebi gelmeli
	printf_P(PSTR("%lu: ******NET On node 0%x has written failed******\n\r"),rTable.getMillis(),node);
}

void StatusCallback::incomingData(RF24NetworkHeader packet)
{
	receivedPacket++;
	printf_P(PSTR("%lu: Callback %dth data received (%s)\n\r"),rTable.getMillis(),receivedPacket, packet.toString());
}
// vim:ai:cin:sts=2 sw=2 ft=cpp
