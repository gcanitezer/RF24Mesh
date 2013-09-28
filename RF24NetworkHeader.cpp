/*
 * RF24NetworkHeader.cpp
 *
 *  Created on: Aug 6, 2013
 *      Author: altan
 */
#include "RF24Network_config.h"
#include "RF24NetworkHeader.h"



/******************************************************************/

const char* RF24NetworkHeader::toString(void) const
{
  uint32_t p1;
  uint32_t p2;
  uint32_t p3;
  uint32_t p4;
  memcpy(&p1, &payload,4);
  memcpy(&p2, &payload + 4,4);
  memcpy(&p3, &payload + 8,4);
  memcpy(&p4, &payload + 12,4);
  static char buffer[125];
  snprintf_P(buffer,sizeof(buffer),PSTR(" msg_id %04x from prev_ip:0%d ip: 0%d to ip 0%d type %c data %lx %lx %lx %lx ip_data_ip:%x ipdata_weight:%x "),id, prev_node, from_node,to_node,type, p1,p2,p3,p4, source_data.ip, source_data.weight);
  return buffer;
}
