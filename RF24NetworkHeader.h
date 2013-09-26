/**
 * Header which is sent with each message
 *
 * The frame put over the air consists of this header and a message
 */
#ifndef __RF24NETWORK_HEADER_H__
#define __RF24NETWORK_HEADER_H__

#include <stddef.h>
#include <stdint.h>

#define T_IP uint16_t
#define T_MAC uint64_t


typedef struct
{
	T_IP ip;
	uint16_t weight;

}IP_MAC,*P_IP_MAC;

struct RF24NetworkHeader
{
  T_IP from_node; /**< Logical address where the message was generated */
  T_IP prev_node;
  T_IP to_node; /**< Logical address where the message is going */
  uint16_t id; /**< Sequential message ID, incremented every message */
  uint64_t payload;
  IP_MAC source_data; //it is used as ip and weight info for join data; original ip and hop count for sensor data
  unsigned char type; /**< Type of the packet.  0-127 are user-defined types, 128-255 are reserved for system */

  static uint16_t next_id; /**< The message ID of the next message to be sent */

  /**
   * Default constructor
   *
   * Simply constructs a blank header
   */
  RF24NetworkHeader() {}

  /**
   * Send constructor
   *
   * Use this constructor to create a header and then send a message
   *
   * @code
   *  RF24NetworkHeader header(recipient_address,'t');
   *  network.write(header,&message,sizeof(message));
   * @endcode
   *
   * @param _to The logical node address where the message is going
   * @param _type The type of message which follows.  Only 0-127 are allowed for
   * user messages.
   */
  RF24NetworkHeader(uint16_t _to, unsigned char _type = 0, uint64_t _data = 0, uint16_t _from = 0): to_node(_to), payload(_data),id(next_id++), type(_type&0x7f), from_node(_from), prev_node(0) {}

  /**
   * Create debugging string
   *
   * Useful for debugging.  Dumps all members into a single string, using
   * internal static memory.  This memory will get overridden next time
   * you call the method.
   *
   * @return String representation of this object
   */
  const char* toString(void) const;
};

#endif
