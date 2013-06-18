#include "RoutingTable.h"
#include "RF24Network_config.h"

const IP_MAC BROADCAST_ADDRESS = {0x7918, 0xE8E8E8E8E8LL, 0};

const IP_MAC MASTER_SYNC_ADDRESS = {0x0000, 0xD7D7D7D7D7LL, 0};

const T_MAC base_address = 0xC5C5C50000LL;


RoutingTable::RoutingTable(void)
{
	tableCount = 0;
	myNode.weight = 0xFF;
	iAmMaster=false;
	shortestPath = 0xFF;
	printf_P(PSTR("Created new routing table"));
}

RoutingTable::~RoutingTable(void)
{
}

IP_MAC RoutingTable::getMasterNode()
{
	//printf_P(PSTR("getMasterNode"));
	return MASTER_SYNC_ADDRESS;
}

IP_MAC RoutingTable::getBroadcastNode()
{
	//printf_P(PSTR("getBroadcastNode"));
	return BROADCAST_ADDRESS;
}


IP_MAC RoutingTable::getCurrentNode()
{
	//printf_P(PSTR("%lu: getCurrentNode IP:%d mac: %lu \n\r"),millis(), myNode.ip, myNode.mac);
	return myNode;
}


void RoutingTable::setCurrentNode(T_IP myIP)
{
	if(myIP == MASTER_SYNC_ADDRESS.ip)
	{
		this->myNode = MASTER_SYNC_ADDRESS;
		iAmMaster = true;
		printf_P(PSTR("%lu:SetCurrent This is master node ip:%d, mac:%lu \n\r"),millis(),myNode.ip, myNode.mac);
	}
	else
	{
		myNode.ip = myIP;
		myNode.mac = base_address | myIP;
		myNode.weight = 0xFF;
		printf_P(PSTR("%lu: SetCurrent MyNode IP:%d mac: %lu \n\r"),millis(),myNode.ip,myNode.mac );
	}
	
}

bool RoutingTable::addNearNode(IP_MAC nearNode)
{
	bool result = false;
	printf_P(PSTR("%lu: addNearNode IP:%d mac: %lu weight:%d \n\r"),millis(),nearNode.ip,nearNode.mac,nearNode.weight );
	if(nearNode.weight < myNode.weight)
	{
		myNode.weight = nearNode.weight + 1;
		table[tableCount] = nearNode;
		shortestPath = tableCount;
		result = true;
	}
	else
	{
		table[tableCount] = nearNode;
	}

	if(tableCount != MAX_NEAR_NODE)
	{			
		tableCount++;
	}
	return result;
}
void RoutingTable::addReacheableNode(T_IP nearNodeID, T_IP* reachableNodeID, int numOfReacheableNodes)
{

}

IP_MAC RoutingTable::getShortestRouteNode()
{
	printf_P(PSTR("%lu: getShortestRouteNode \r\n"), millis());
	if(iAmMaster)
		return MASTER_SYNC_ADDRESS;
	else 
		return table[shortestPath];
}

void RoutingTable::cleanTable()
{
	tableCount = 0;
	myNode.weight = 0xFF;
	shortestPath = 0xFF;
}

bool RoutingTable::amIJoinedNetwork()
{
	if(shortestPath == 0xFF)
		return false;
	else 
		return true;
}

int RoutingTable::getTableSize()
{
	return tableCount;
}

void* RoutingTable::getTable()
{
	return table;
}

T_MAC RoutingTable::getMac(T_IP ip)
{
	T_MAC result=0;

	if(ip == BROADCAST_ADDRESS.ip)
		result = BROADCAST_ADDRESS.mac;
	else if(ip == MASTER_SYNC_ADDRESS.ip)
		result = MASTER_SYNC_ADDRESS.mac;
	else

		for(int i=0;i<tableCount;i++)
		{
			if(ip == table[i].ip)
				result = table[i].mac;
		}
	
	if(result == 0)
	printf_P(PSTR("%lu: ----WARNING--- getMac called for IP: %d mac: %lu\n\r"), millis(),ip,result);
	return result;
}

T_MAC RoutingTable::getShortestMac(T_IP ip)
{
	T_MAC result=0;

	
	for(int i=0;i<tableCount;i++)
	{
		if(ip == table[i].ip)
			result = table[i].mac;
	}

	if(result == 0)
	{
		if(ip == BROADCAST_ADDRESS.ip)
			result = BROADCAST_ADDRESS.mac;
		else if(ip == MASTER_SYNC_ADDRESS.ip)
			result = getShortestRouteNode().mac;
	}
		
	if(result == 0)
		printf_P(PSTR("%lu: ----WARNING--- getMac called for IP: %d mac: %lu\n\r"), millis(),ip,result);
	return result;
}