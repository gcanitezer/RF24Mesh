#ifndef __ROUTINGTABLE_H__
#define __ROUTINGTABLE_H__

#include <stddef.h>
#include <stdint.h>
#include "RF24NetworkHeader.h"


#define MAX_NEAR_NODE 10

typedef enum {SENT_WELCOME, GOT_WELCOME, GOT_JOIN, SHORTENED, CONNECTED, DEAD} RoutingStates;

typedef struct _RoutingData
{
	IP_MAC ip_mac;
	uint16_t send_id;
	uint16_t rec_id;
	RoutingStates status; //TODO stateleri kullan
	unsigned long time;
} RoutingData;

class RoutingTable
{
private:
	IP_MAC myNode;
	RoutingData table[MAX_NEAR_NODE];
	uint8_t tableCount;
	uint8_t shortestPath;
	bool iAmMaster;
	unsigned long millis_delta;
public:
	RoutingTable(void);
	~RoutingTable(void);
	IP_MAC getMasterNode();
	IP_MAC getBroadcastNode();
	IP_MAC getCurrentNode();
	bool amImaster(){
	return iAmMaster;
	};
	bool amIJoinedNetwork();
	void cleanTable();
	void sentData(RF24NetworkHeader h);
	void setCurrentNode(T_IP myNode);
	bool addNearNode(IP_MAC nearNodeID);
	void addReacheableNode(T_IP nearNodeID, T_IP* reachableNodeID, int numOfReacheableNodes);
	IP_MAC getShortestRouteNode();
	int getTableSize();
	RoutingData* getTable();
	void printTable();
	bool isPathShortened();
	void connectShortened();
	int getNumOfWelcomes();
	int getNumOfJoines();
	void setWelcomeMessageSent(T_IP ip);
	void setConnected(T_IP ip);
	int8_t checkTable(T_IP ip);
	T_MAC getMac(T_IP ip);
	T_MAC getShortestMac(T_IP ip);
	void setMillis(uint8_t data[16]);
	unsigned long getMillis();
	bool removeUnreacheable(IP_MAC nearNode);
	int8_t getShortestNodePosition();
};
#endif //__ROUTINGTABLE_H__
