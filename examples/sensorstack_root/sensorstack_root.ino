

/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example of pinging across a mesh network
 *
 * Using this sketch, each node will send a ping to every other node
 * in the network every few seconds. 
 * The RF24Network library will route the message across
 * the mesh to the correct node.
 *
 * This sketch is greatly complicated by the fact that at startup time, each
 * node (including the base) has no clue what nodes are alive.  So,
 * each node builds an array of nodes it has heard about.  The base
 * periodically sends out its whole known list of nodes to everyone.
 *
 * To see the underlying frames being relayed, compile RF24Network with
 * #define SERIAL_DEBUG.
 *
 * The logical node address of each node is set in EEPROM.  The nodeconfig
 * module handles this by listening for a digit (0-9) on the serial port,
 * and writing that number to EEPROM.
 *
 * To use the sketch, upload it to two or more units.  Run each one in
 * turn.  Attach a serial monitor, and send a single-digit address to
 * each.  Make the first one '0', and the following units '1', '2', etc.
 */
#include <RF24Mesh.h>
#include <RF24Network_config.h>



#include <avr/pgmspace.h>
#include <RF24Mesh.h>
#include <RF24.h>
#include <SPI.h>
#include "nodeconfig.h"
#include "printf.h"
// This is for git version tracking.  Safe to ignore
#ifdef VERSION_H
#include "version.h"
#else
const char program_version[] = "05.10.2013";
#endif

StatusCallback callme;
// nRF24L01(+) radio using the Getting Started board
RF24 radio(9,10);
RF24Mesh network(radio, callme);

// Our node address
uint16_t this_node;

long message_no = 0;


void setup(void)
{
  //
  // Print preamble
  //
  
  Serial.begin(115200);
  printf_begin();
  printf_P(PSTR("\n\rRF24Mesh/examples/sensorstack_root/\n\r"));
  printf_P(PSTR("VERSION: %s\n\r"),program_version);
  
  //
  // Pull node address out of eeprom 
  //

  // Which node are we?
  this_node = 0; //root,sync node

  //
  // Bring up the RF network
  //

  SPI.begin();
  
  network.begin(/*channel*/ 88, /*node address*/ this_node );
}

void loop(void)
{
  // Pump the network regularly
  network.loop();
    
}










// vim:ai:cin:sts=2 sw=2 ft=cpp
