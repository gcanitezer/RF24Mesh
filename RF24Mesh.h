/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#ifndef __RF24NETWORK_H__
#define __RF24NETWORK_H__

/**
 * @file RF24Network.h
 *
 * Class declaration for RF24Network
 */

#include <stddef.h>
#include <stdint.h>
#include "RF24NetworkHeader.h"
#include "RoutingTable.h"

class RF24;




#define SEND_QUEUE_SIZE 5
#define RECEIVE_QUEUE_SIZE 5
/**
* Callback Interface
* 
*/

class StatusCallback
{
	uint16_t receivedPacket;
	
public:
 StatusCallback();
 void println(const char * str);
 void sendingFailed(T_MAC node);
 void incomingData(RF24NetworkHeader packet);
};

/**
 * Network Layer for RF24 Radios
 *
 * This class implements an OSI Network Layer using nRF24L01(+) radios driven
 * by RF24 library.
 */
typedef enum  {INIT = 0, NJOINED, SENDJOIN, JOINRECEIVED, NEW_JOINED, JOINED} STATES;

class RF24Mesh
{
public:
  /**
   * Construct the network
   *
   * @param _radio The underlying radio driver instance
   * 
   */
  RF24Mesh( RF24& _radio, StatusCallback& _callback );

  /**
   * Bring up the network
   *
   * @warning Be sure to 'begin' the radio first.
   *
   * @param _channel The RF channel to operate on
   * @param _node_address The logical address of this node
   */
  void begin(uint8_t _channel, uint16_t _node_address );
  
  /**
   * Main layer loop
   *
   * This function must be called regularly to keep the layer going.  This is where all
   * the action happens!
   */
  void updateNetworkTopology(void);

  /**
   * Test whether there is a message available for this node
   * 
   * @return Whether there is a message available for this node
   */
  bool available(void);

  bool send_available(void);

 
  /**
   * Read the next available header
   *
   * Reads the next available header without advancing to the next
   * incoming message.  Useful for doing a switch on the message type
   *
   * If there is no message available, the header is not touched
   *
   * @param[out] header The header (envelope) of the next message
   */
  void peek(RF24NetworkHeader& header);

  /**
   * Read a message
   *
   * @param[out] header The header (envelope) of this message
   * @param[out] message Pointer to memory where the message should be placed
   * @param maxlen The largest message size which can be held in @p message
   * @return The total number of bytes copied into @p message
   */
  size_t read(RF24NetworkHeader& header, void* message, size_t maxlen);
  
  /**
   * Send a message
   *
   * @param[in,out] header The header (envelope) of this message.  The critical
   * thing to fill in is the @p to_node field so we know where to send the
   * message.  It is then updated with the details of the actual header sent.
   * @param message Pointer to memory where the message is located 
   * @param len The size of the message 
   * @return Whether the message was successfully received 
   */
  bool write(RF24NetworkHeader& header);
  
  /**
   * Send a message, the current time
   *
   */
  bool send_SensorData(uint8_t data[16]);

  bool send_WelcomeMessage(T_IP);

  bool send_JoinMessage();

  bool send_UpdateWeight();
/**
 * Handle a 'T' message
 *
 * Add the node to the list of active nodes
 */
void handle_WelcomeMessage(RF24NetworkHeader& header);

/**
 * Handle an 'N' message, the active node list
 */
void handle_DataMessage(RF24NetworkHeader& header);
void handle_ForwardData(RF24NetworkHeader& header);

void handle_JoinMessage(RF24NetworkHeader& header);
void handle_UpdateWeightMessage(RF24NetworkHeader& header);

/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node);



void loop(void);


void joinNetwork();
bool isJoined();

protected:
	void fastloop();
	void slowloop();

	void setState(STATES s);
	bool isState(STATES s);

  void open_pipes(void);
  uint16_t find_node( uint16_t current_node, uint16_t target_node );
  void shiftleft(uint8_t *send_queue, const int frame_size, uint8_t *frame_buffer, uint8_t *last_pointer);
  bool write(T_MAC);
  int write();
  bool write(RF24NetworkHeader& header, T_MAC mac);
  bool write_to_pipe( uint16_t node, uint8_t pipe );
  bool enqueue(void);
  bool send_enqueue();

  bool is_valid_address( uint16_t node );
  void listenRadio();
  void sendAckToWelcome();
  void sendWelcomeToJoin();
  void handlePacket();
  void sendPackets();
	unsigned short int getMyIP();

private:
  RF24& radio; /**< Underlying radio driver, provides link/physical layers */ 
  StatusCallback& callback;
  uint16_t node_address; /**< Logical node address of this unit, 1 .. UINT_MAX */
  const static int frame_size = 32; /**< How large is each frame over the air */ 
  uint8_t frame_buffer[frame_size]; /**< Space to put the frame that will be sent/received over the air */
  uint8_t receive_queue[RECEIVE_QUEUE_SIZE*frame_size]; /**< Space for a small set of frames that need to be delivered to the app layer */
  uint8_t* receive_frame; /**< Pointer into the @p frame_queue where we should place the next received frame */

  uint8_t send_queue[SEND_QUEUE_SIZE*frame_size];
  uint8_t* send_frame_p;

  //uint16_t parent_node; /**< Our parent's node address */
  //uint8_t parent_pipe; /**< The pipe our parent uses to listen to us */
  //uint16_t node_mask; /**< The bits which contain signfificant node address information */
  long last_join_time;
	static const int JOIN_DURATION = 120000;
	static const int JOIN_WAIT_WELCOME = 5000;

	unsigned long state_time; //use millis()
	STATES state;
};

/**
 * @example helloworld_tx.pde
 *
 * Simplest possible example of using RF24Network.  Put this sketch
 * on one node, and helloworld_rx.pde on the other.  Tx will send
 * Rx a nice message every 2 seconds which rx will print out for us.
 */

/**
 * @example helloworld_rx.pde
 *
 * Simplest possible example of using RF24Network.  Put this sketch
 * on one node, and helloworld_tx.pde on the other.  Tx will send
 * Rx a nice message every 2 seconds which rx will print out for us.
 */

/**
 * @example meshping.pde
 *
 * Example of pinging across a mesh network
 * Using this sketch, each node will send a ping to the base every
 * few seconds.  The RF24Network library will route the message across
 * the mesh to the correct node.
 */

/**
 * @example sensornet.pde
 *
 * Example of a sensor network.
 * This sketch demonstrates how to use the RF24Network library to
 * manage a set of low-power sensor nodes which mostly sleep but
 * awake regularly to send readings to the base.
 */
/**
 * @mainpage Network Layer for RF24 Radios
 *
 * This class implements an <a href="http://en.wikipedia.org/wiki/Network_layer">OSI Network Layer</a> using nRF24L01(+) radios driven
 * by the <a href="http://maniacbug.github.com/RF24/">RF24</a> library.
 *
 * @section Purpose Purpose/Goal
 *
 * Create an alternative to ZigBee radios for Arduino communication.
 *
 * Xbees are excellent little radios, backed up by a mature and robust standard 
 * protocol stack.  They are also expensive.
 *
 * For many Arduino uses, they seem like overkill.  So I am working to build
 * an alternative using nRF24L01 radios.  Modules are available for less than 
 * $6 from many sources.  With the RF24Network layer, I hope to cover many
 * common communication scenarios.
 *
 * Please see the @ref Zigbee page for a comparison against the ZigBee protocols
 *
 * @section Features Features
 *
 * The layer provides:
 * @li Host Addressing.  Each node has a logical address on the local network.
 * @li Message Forwarding.  Messages can be sent from one node to any other, and
 * this layer will get them there no matter how many hops it takes.
 * @li Ad-hoc Joining.  A node can join a network without any changes to any
 * existing nodes.
 *
 * The layer does not (yet) provide:
 * @li Fragmentation/reassembly.  Ability to send longer messages and put them
 * all back together before exposing them up to the app.
 * @li Power-efficient listening.  It would be useful for nodes who are listening
 * to sleep for extended periods of time if they could know that they would miss
 * no traffic.
 * @li Dynamic address assignment.
 *
 * @section More How to learn more
 *
 * @li <a href="http://maniacbug.github.com/RF24/">RF24: Underlying radio driver</a>
 * @li <a href="classRF24Network.html">RF24Network Class Documentation</a>
 * @li <a href="https://github.com/maniacbug/RF24Network/">Source Code</a>
 * @li <a href="https://github.com/maniacbug/RF24Network/archives/master">Downloads Page</a>
 * @li <a href="examples.html">Examples Page</a>.  Start with <a href="helloworld_rx_8pde-example.html">helloworld_rx</a> and <a href="helloworld_tx_8pde-example.html">helloworld_tx</a>.
 *
 * @section Topology Topology for Mesh Networks using nRF24L01(+)
 *
 * This network layer takes advantage of the fundamental capability of the nRF24L01(+) radio to
 * listen actively to up to 6 other radios at once.  The network is arranged in a 
 * <a href="http://en.wikipedia.org/wiki/Network_Topology#Tree">Tree Topology</a>, where
 * one node is the base, and all other nodes are children either of that node, or of another.
 * Unlike a true mesh network, multiple nodes are not connected together, so there is only one
 * path to any given node.
 *
 * @section Octal Octal Addressing
 *
 * Each node must be assigned an 15-bit address by the administrator.  This address exactly
 * describes the position of the node within the tree.  The address is an octal number.  Each
 * digit in the address represents a position in the tree further from the base.
 *
 * @li Node 00 is the base node.
 * @li Nodes 01-05 are nodes whose parent is the base.
 * @li Node 021 is the second child of node 01.
 * @li Node 0321 is the third child of node 021, an so on.
 * @li The largest node address is 05555, so 3,125 nodes are allowed on a single channel.
 *
 * @section Routing How routing is handled
 *
 * When sending a message using RF24Mesh::write(), you fill in the header with the logical
 * node address.  The network layer figures out the right path to find that node, and sends
 * it through the system until it gets to the right place.  This works even if the two nodes
 * are far separated, as it will send the message down to the base node, and then back out
 * to the final destination.
 *
 * All of this work is handled by the RF24Mesh::update() method, so be sure to call it
 * regularly or your network will miss packets.
 *
 * @section Startup Starting up a node
 *
 * When a node starts up, it only has to contact its parent to establish communication.
 * No direct connection to the Base node is needed.  This is useful in situations where
 * relay nodes are being used to bridge the distance to the base, so leaf nodes are out
 * of range of the base.
 *
 * @section Directionality Directionality 
 *
 * By default all nodes are always listening, so messages will quickly reach
 * their destination.  
 * 
 * You may choose to sleep any nodes which do not have any active children on the network
 * (i.e. leaf nodes).  This is useful in a case where
 * the leaf nodes are operating on batteries and need to sleep.
 * This is useful for a sensor network.  The leaf nodes can sleep most of the time, and wake
 * every few minutes to send in a reading.  However, messages cannot be sent to these 
 * sleeping nodes.
 *
 * In the future, I plan to write a system where messages can still be passed upward from
 * the base, and get delivered when a sleeping node is ready to receive them.  The radio
 * and underlying driver support 'ack payloads', which will be a handy mechanism for this.
 *
 * @page Zigbee Comparison to ZigBee
 *
 * This network layer is influenced by the design of ZigBee, but does not implement it
 * directly.  
 *
 * @section Advantage Which is better?
 *
 * ZigBee is a much more robust, feature-rich set of protocols, with many different vendors
 * providing compatible chips.
 *
 * RF24Network is cheap.  While ZigBee radios are well over $20, nRF24L01 modules can be found
 * for under $6.  My personal favorite is 
 * <a href="http://www.mdfly.com/index.php?main_page=product_info&products_id=82">MDFly RF-IS2401</a>.
 *
 * @section Contrast Similiarities & Differences
 *
 * Here are some comparisons between RF24Network and ZigBee.
 *
 * @li Both networks support Star and Tree topologies.  Only Zigbee supports a true mesh.
 * @li In both networks, only leaf nodes can sleep (see @ref NodeNames).
 * @li ZigBee nodes are configured using AT commands, or a separate Windows application. 
 * RF24 nodes are configured by recompiliing the firmware or writing to EEPROM.
 *
 * @section NodeNames Node Naming
 *
 * @li Leaf node: A node at the outer edge of the network with no children.  ZigBee calls it
 * an End Device node.
 * @li Relay node: A node which has both parents and children, and relays messages from one
 * to the other.  ZigBee calls it a Router.
 * @li Base node.  The top of the tree node with no parents, only children.  Typically this node
 * will bridge to another kind of network like Ethernet.  ZigBee calls it a Co-ordinator node.
 */

#endif // __RF24NETWORK_H__
// vim:ai:cin:sts=2 sw=2 ft=cpp
