#ifndef __ROUTINGTABLE_H__
#define __ROUTINGTABLE_H__

#include <stddef.h>
#include <stdint.h>

#define T_IP uint16_t
#define T_MAC uint64_t


typedef struct 
{
	T_IP ip;
	T_MAC mac;
	uint16_t weight;
}IP_MAC,*P_IP_MAC;

#define MAX_NEAR_NODE 10

class RoutingTable
{
private:
	IP_MAC myNode;
	IP_MAC table[MAX_NEAR_NODE];
	uint8_t tableCount;
	uint8_t shortestPath;
	bool iAmMaster;
	long millis_delta;
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
	void setCurrentNode(T_IP myNode);
	bool addNearNode(IP_MAC nearNodeID);
	void addReacheableNode(T_IP nearNodeID, T_IP* reachableNodeID, int numOfReacheableNodes);
	IP_MAC getShortestRouteNode();
	int getTableSize();
	void* getTable();
	void printTable();
	int8_t checkTable(T_IP ip);
	T_MAC getMac(T_IP ip);
	T_MAC getShortestMac(T_IP ip);
	void setMillis(uint64_t data);
	unsigned long getMillis();
	void removeUnreacheable(IP_MAC nearNode);
	int8_t getShortestNodePosition();
};
#endif //__ROUTINGTABLE_H__