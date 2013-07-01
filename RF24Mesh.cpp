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

  // Delay manager to send pings regularly
const unsigned long interval = 2000; // ms
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


  radio.openReadingPipe(0, rTable.getBroadcastNode().mac);
  radio.openReadingPipe(1, rTable.getCurrentNode().mac);
  
  radio.startListening();

  // Spew debugging state about the radio
  radio.printDetails();


}

/*************************************************************/

void RF24Mesh::loop(void)
{
  // Pump the network regularly
  updateNetworkTopology();

  listenRadio(false);
  
	handlePacket();
}

/******************************************************************/

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
		handle_J(header);
		break;
    case 'W':
      handle_WelcomeMessage(header);
      break;
    case 'D':
      handle_DataMessage(header);
      break;
    default:
	  printf_P(PSTR("*** WARNING *** Unknown message type %s\n\r"),header.toString());
      read(header,0,0);
      break;
    };
  }
}
void RF24Mesh::updateNetworkTopology(void)
{
	
	 if(!rTable.amImaster())
	  {
		
  
	  //IF_SERIAL_DEBUG(printf_P(PSTR("%lu: updateNetworkTopology\n\r"),millis()));
	  
	   if(last_join_time == 0|| (millis()-last_join_time) > 60*1000) //bir dakika
	{
	  
		  rTable.cleanTable();
		  joinNetwork();
		  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: Join network called last_join_time: %lu \n\r"),millis(),last_join_time));
	  }
  }
  
}

void RF24Mesh::listenRadio(bool waitJoinAck)
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

				handlePacket();
		  }
		  else
		  {
			  printf_P(PSTR("%lu: MAC Received message **NOT for me**, forwarding %d != %d \n\r"), rTable.getMillis(), header.to_node, rTable.getCurrentNode().ip);
				// Relay it
			  uint64_t mac = rTable.getMac(header.to_node);
			  if(mac != 0)
				write(mac);
		  }
			
		  

		}
	  }
	  
	if(waitJoinAck)
	{
		wait = (millis()-stime>10000)?false:true;
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
	send_JoinMessage();
	listenRadio(true);
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
    printf_P(PSTR("ok\n\r"));
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
  static char buffer[65];
  snprintf_P(buffer,sizeof(buffer),PSTR(" msg_id %04x from ip 0%d to ip 0%d type %c data %lx"),id,from_node,to_node,type, payload);
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
  
	header.join_data = rTable.getCurrentNode();
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu:Sending join message to ip:%d  as my ip:%d and mymac:%lu myweight:%lu --------\n\r"),rTable.getMillis(),header.to_node,header.join_data.ip, header.join_data.mac, header.join_data.weight));
  return write(header,rTable.getBroadcastNode().mac);
}

bool RF24Mesh::send_WelcomeMessage(IP_MAC toNode)
{
	RF24NetworkHeader header(toNode.ip, 'W',rTable.getMillis(), rTable.getCurrentNode().ip);
  
  // The 'T' message that we send is just a ulong, containing the time
	//IP_MAC message = rTable.getCurrentNode();
	header.join_data = rTable.getCurrentNode();
  printf_P(PSTR("---------------------------------\n\r"));
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Sending Welcome Message to ip: %d as my ip: %d and my mac: %lu my weight:%lu...\n\r"),rTable.getMillis(),header.to_node,header.join_data.ip,header.join_data.mac, header.join_data.weight));
  return write(header,toNode.mac);
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
	RF24NetworkHeader header(rTable.getMasterNode().ip,  'D', data );
  
  // The 'T' message that we send is just a ulong, containing the time
  
  IF_SERIAL_DEBUG(printf_P(PSTR("---------------------------------\n\r")));
  printf_P(PSTR("%lu: APP Sending send_SensorData %lu to ip %lu...\n\r"),rTable.getMillis(),data, rTable.getShortestRouteNode().ip);
  bool result = write(header,0,0);
  while (!result)
  {
  	  rTable.removeUnreacheable(rTable.getShortestRouteNode());

	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: APP Repeating failed send_SensorData, data:%lu"),rTable.getMillis(),data));
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
void RF24Mesh::handle_J(RF24NetworkHeader& header)
{
  // The 'T' message is just a ulong, containing the time
  //IP_MAC message;
  read(header,0,0);
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_J APP Received J message from ip:%d msg_ip:%d from mac %lu\n\r"),rTable.getMillis(),header.from_node, header.join_data.ip,header.join_data.mac));

  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this->node_address || header.from_node > 00 )
  {
	  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_J farkli node kaydet ve cevap ver\n\r"),millis()));
	  rTable.addNearNode(header.join_data);
	  send_WelcomeMessage(header.join_data);
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
  IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_WelcomeMessage (%s) APP Received ip %d mac %lu weight:%d \n\r"),millis(),header.toString(),header.join_data.ip,header.join_data.mac,header.join_data.weight));
  rTable.setMillis(header.payload);

  last_join_time = millis();
  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this->node_address || header.from_node > 00 )
	  if(rTable.addNearNode(header.join_data))
	  {
		   IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_WelcomeMessage, update join status"),millis()));
		  send_JoinMessage();
	  }

   rTable.printTable();
}

/**
 * Handle an 'N' message, the active node list
 */
void RF24Mesh::handle_DataMessage(RF24NetworkHeader& header)
{
  uint64_t message;
  read(header,&message,sizeof(uint64_t));

  if(header.from_node == rTable.getCurrentNode().ip)
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: handle_DataMessage APP I got my own data omitting. %lu from %d\n\r"),rTable.getMillis(),message,header.from_node));
  else
  {
	  callback.incomingData(header);
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
