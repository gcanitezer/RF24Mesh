

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
#include <stdint.h>

// This is for git version tracking.  Safe to ignore
#ifdef VERSION_H
#include "version.h"
#else
const char program_version[] = "01.10.2013";
#endif

StatusCallback callme;
// nRF24L01(+) radio using the Getting Started board
RF24 radio(9,10);
RF24Mesh network(radio, callme);

// Our node address
uint16_t this_node;

long message_no = 0;
#include <SoftwareSerial.h>

SoftwareSerial mySerial(4,5);

int serial_putc( char c, FILE * ) 
{
  Serial.write(c);
  //mySerial.write( c );

  return c;
} 

void printf_begin(void)
{
  Serial.begin(115200);
  //mySerial.begin(115200);
  fdevopen( &serial_putc, 0 );
}


void setup(void)
{
  //
  // Print preamble
  //
  printf_begin();
  printf_P(PSTR("\n\rRF24Mesh/examples/sensorstack_pde/\n\r"));
  printf_P(PSTR("VERSION: %s\n\r"),program_version);
  
  printf_P(PSTR("sizeof Header %d\n\r"), sizeof(RF24NetworkHeader));
  //
  // Pull node address out of eeprom 
  //

  // Which node are we?
  this_node = 7;//nodeconfig_read();

  //
  // Bring up the RF network
  //

  SPI.begin();
  
  network.begin(/*channel*/ 88, /*node address*/ this_node );
}

void loop(void)
{
  uint8_t data[16];
  
  data[4] = message_no++;
  // Pump the network regularly
  network.loop();
  if (this_node != 0 && network.isJoined() && millis()%10000 == 0)
  network.send_SensorData(data);
  
  
}










// vim:ai:cin:sts=2 sw=2 ft=cpp