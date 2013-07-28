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

typedef enum  {INIT, NJOINED, JOINED} states;

states state=INIT;

/******************************************************************/

RF24Mesh::RF24Mesh( RF24& _radio, StatusCallback& _callback ): radio(_radio), callback(_callback), next_frame(frame_queue)
{
	last_join_time = 0;
}

/******************************************************************/

void RF24Mesh::begin(uint8_t _channel, T_IP _node_address )
{

  node_address = _node_address;
  rTable.setCurrentNode(node_address);

  radio.begin();
  // Set up the radio the way we want it to look
  radio.setChannel(_channel);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5,15);
  radio.setAutoAck(false);


  radio.openReadingPipe(0, rTable.getMac(0));
  //radio.openReadingPipe(1, rTable.getCurrentNode().mac);
  
  radio.startListening();

  // Spew debugging state about the radio
  radio.printDetails();
}

/*************************************************************/

void RF24Mesh::loop(void)
{
  // Pump the network regularly
  updateNetworkTopology();

  listenRadio();
  
  handlePacket();
}

/******************************************************************/
void RF24Mesh::updateNetworkTopology(void)
{

	if (!rTable.amImaster() && (state == INIT || state == NJOINED))
	{

		if (last_join_time == 0 || (millis() - last_join_time) > JOIN_DURATION) //bir dakika
		{

			joinNetwork();
			IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Join network called last_join_time: %lu \n\r"),rTable.getMillis(),last_join_time));
		}
	}

}

void RF24Mesh::handlePacket()
{
	// Is there anything ready for us?
  while ( available() )
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("There are available received message \n\r")));

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
    default:
	  printf_P(PSTR("*** WARNING *** Unknown message type %s\n\r"),header.toString());
      read(header,0,0);
      break;
    };
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

	if(state == NJOINED || state == INIT)
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
  
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Enqueue @%x "),rTable.getMillis(),next_frame-frame_queue));

  // Copy the current frame into the frame queue
  if ( next_frame < frame_queue + sizeof(frame_queue) )
  {
    memcpy(next_frame,frame_buffer, frame_size );
    next_frame += frame_size; 

    result = true;
    IF_SERIAL_DEBUG(printf_P(PSTR("ok\n\r")));
  }
  else
  {
    printf_P(PSTR("failed\n\r"));
  }

  return result;
}

/******************************************************************/

bool RF24Mesh::available(void)
{
  // Are there frames on the queue for us?
  return (next_frame > frame_queue);
}

/******************************************************************/

void RF24Mesh::peek(RF24NetworkHeader& header)
{
  if ( available() )
  {
    // Copy the next available frame from the queue into the provided buffer
    memcpy(&header,next_frame-frame_size,sizeof(RF24NetworkHeader));
  }
}

/******************************************************************/

size_t RF24Mesh::read(RF24NetworkHeader& header,void* message, size_t maxlen)
{
  size_t bufsize = 0;

  if ( available() )
  {
    // Move the pointer back one in the queue 
    next_frame -= frame_size;
    uint8_t* frame = next_frame;
      
    // How much buffer size should we actually copy?
    bufsize = min(maxlen,frame_size-sizeof(RF24NetworkHeader));

    // Copy the next available frame from the queue into the provided buffer
    memcpy(&header,frame,sizeof(RF24NetworkHeader));
    memcpy(message,frame+sizeof(RF24NetworkHeader),bufsize);
    
    IF_SERIAL_DEBUG(printf_P(PSTR("%lu: *****NET RF24Mesh::read Received (%s)\n\r"),rTable.getMillis(),header.toString()));
  }

  return bufsize;
}

bool RF24Mesh::write(RF24NetworkHeader& header, T_MAC mac)
{
  // Fill out the header
//	header.from_node = rTable.getCurrentNode().ip;
	
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Sending message(%s) \n\r"),rTable.getMillis(),header.toString()));

  // Build the full frame to send
  memcpy(frame_buffer,&header,sizeof(RF24NetworkHeader));

  // If the user is trying to send it to himself
  if ( header.to_node == rTable.getCurrentNode().ip )
    // Just queue it in the received queue
    return enqueue();
  else
    // Otherwise send it out over the air
	return write(mac);
}

/******************************************************************/

bool RF24Mesh::write(RF24NetworkHeader& header,const void* message, size_t len)
{
  // Fill out the header
	header.from_node = rTable.getCurrentNode().ip;
	
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Sending message(%s) \n\r"),rTable.getMillis(),header.toString()));

  // Build the full frame to send
  memcpy(frame_buffer,&header,sizeof(RF24NetworkHeader));
  if (len)
    memcpy(frame_buffer + sizeof(RF24NetworkHeader),message,min(frame_size-sizeof(RF24NetworkHeader),len));

  if (len)
  {
    IF_SERIAL_DEBUG(const uint16_t* i = reinterpret_cast<const uint16_t*>(message);printf_P(PSTR("%lu: NET message %04x\n\r"),rTable.getMillis(),*i));
  }

  // If the user is trying to send it to himself
  if ( header.to_node == rTable.getCurrentNode().ip )
    // Just queue it in the received queue
    return enqueue();
  else
    // Otherwise send it out over the air
	return write(rTable.getShortestMac(header.to_node));
}

/******************************************************************/

bool RF24Mesh::write(T_MAC to_mac)
{
  bool ok = false;
  
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: NET Trying to write mac %lu \n\r"),rTable.getMillis(),to_mac) );

  // First, stop listening so we can talk
  radio.stopListening();
 
  // Open the correct pipe for writing.  
  radio.openWritingPipe(to_mac);

  // Retry a few times
  short attempts = 5;
  do
  {
    ok = radio.write( frame_buffer, frame_size );
  }
  while ( !ok && --attempts );
  // Now, continue listening
  radio.startListening();

  if(!ok)
	  callback.sendingFailed(to_mac);

  return ok;
}


/******************************************************************/

const char* RF24NetworkHeader::toString(void) const
{
  uint32_t p1;
  uint32_t p2;
  memcpy(&p1, &payload,4);
  memcpy(&p2, &payload + 4,4);
  static char buffer[125];
  snprintf_P(buffer,sizeof(buffer),PSTR(" msg_id %04x from ip 0%d to ip 0%d type %c data %lx %lx ip_data_ip:%x ipdata_weight:%x "),id,from_node,to_node,type, p1,p2, ip_data.ip, ip_data.weight);
  return buffer;
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

/******************************************************************/

/**
 * Send a 'T' message, the current time
 */
bool RF24Mesh::send_JoinMessage()
{
	RF24NetworkHeader header(rTable.getBroadcastNode().ip, 'J', 0, rTable.getCurrentNode().ip);
  
	header.ip_data.ip = rTable.getCurrentNode().ip;
	header.ip_data.weight = rTable.getCurrentNode().weight;
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu:Sending join message (%s) \n\r"),rTable.getMillis(),header.toString()));
	return write(header,rTable.getMac(0));
}

bool RF24Mesh::send_WelcomeMessage(IP_MAC toNode)
{
	unsigned long time = rTable.getMillis();
	uint64_t payload;
	memset(&payload,0,8);
	memcpy(&payload+4,&time,4);
	RF24NetworkHeader header(toNode.ip, 'W',payload , rTable.getCurrentNode().ip);
	header.ip_data.ip = rTable.getCurrentNode().ip;
	header.ip_data.weight = rTable.getCurrentNode().weight;
  

  printf_P(PSTR("---------------------------------\n\r"));
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Sending Welcome Message (%s)...\n\r"),rTable.getMillis(),header.toString()));
  return write(header,rTable.getMac(0)); //broadcast mac'ine gonder.
}

/**
 * Send a 'T' message, the current time
 */
bool RF24Mesh::send_SensorData(uint64_t data)
{
	if(rTable.amImaster())
	{
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: send_SensorData, since I am master i do not send to myself ------------\n\r"),rTable.getMillis()));
		 return true;
	}

	if(!rTable.amIJoinedNetwork())
	{
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: send_SensorData, I havent joined yet\n\r"),rTable.getMillis()));
		callback.sendingFailed(0);
		return false;
	}

	T_IP ip = rTable.getShortestRouteNode().ip;
	unsigned char type = 'D';

	if (ip != rTable.getMasterNode().ip)
	{	type = 'F';
		printf_P(PSTR("%lu: APP Send_SersorData short ip: %d masterip: %d"),rTable.getMillis(),ip,rTable.getMasterNode().ip);
	}
	RF24NetworkHeader header(ip,  type, data );
	header.ip_data.ip = rTable.getCurrentNode().ip; //source ip
	header.ip_data.weight = 0; //not important
  
  
  IF_SERIAL_DEBUG(printf_P(PSTR("---------------------------------\n\r")));
  printf_P(PSTR("%lu: APP Sending send_SensorData %s ...\n\r"),rTable.getMillis(), header.toString());
  bool result = write(header,0,0);
  while (!result)
  {
  	  if(!rTable.removeUnreacheable(rTable.getShortestRouteNode()))//There are no neighbour nodes
  	  {
  		  state = NJOINED;
  		  //goker joinNetwork();
  		  return false;
  	  }

	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Repeating failed send_SensorData, (%s)"),rTable.getMillis(),header.toString()));
	  result = write(header,0,0);
  }
  return result;
}

/**
 * Send an 'N' message, the active node list
 */
bool RF24Mesh::send_N(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);

  int i = rTable.getTableSize();
  void* table = rTable.getTable(); 
  
  IF_SERIAL_DEBUG(printf_P(PSTR("---------------------------------\n\r")));
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Sending active nodes to 0%o...\n\r"),rTable.getMillis(),to));
  //bool result = write(header,active_nodes,sizeof(active_nodes));
  bool result = write(header,table,i);
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

  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this->node_address || header.from_node > 00 )
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_J farkli node kaydet ve cevap ver\n\r"),rTable.getMillis()));
	  rTable.addNearNode(header.ip_data);
	  send_WelcomeMessage(header.ip_data);
  }
  rTable.printTable();
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

  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this->node_address || header.from_node > 00 )
	  if(rTable.addNearNode(header.ip_data))
	  {
		  rTable.setMillis(header.payload);
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_WelcomeMessage, update join status"),rTable.getMillis()));
		  send_JoinMessage();
	  }

   rTable.printTable();

   if(rTable.amIJoinedNetwork())
   {
	   last_join_time = millis();
	   state = JOINED;
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

	  header.ip_data.weight++;
	  header.from_node = rTable.getCurrentNode().ip;
	  header.to_node = ip;
      if (ip != rTable.getMasterNode().ip)
	  {	
		  type = 'F';
		  printf_P(PSTR("%lu: APP handle_ForwardData short ip: %d masterip: %d"),rTable.getMillis(),ip,rTable.getMasterNode().ip);
	  }
	  header.type = type;
 
	  write(header, rTable.getMac(0));
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
	printf_P(PSTR("%lu: NET On node 0%x has written failed\n\r"),rTable.getMillis(),node);
}

void StatusCallback::incomingData(RF24NetworkHeader packet)
{
	receivedPacket++;
	printf_P(PSTR("%lu: Callback %dth data received (%s)\n\r"),rTable.getMillis(),receivedPacket, packet.toString());
}
// vim:ai:cin:sts=2 sw=2 ft=cpp
