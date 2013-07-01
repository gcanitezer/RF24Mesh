#include "RoutingTable.h"
#include "RF24Network_config.h"

const IP_MAC BROADCAST_ADDRESS = {0x7918, 0xE8E8E8E8LL, 0};

const IP_MAC MASTER_SYNC_ADDRESS = {0x0000, 0xD7D7D7D7LL, 0};

const T_MAC base_address = 0xC5C50000LL;

const uint8_t MAX_WEIGHT = 255;

RoutingTable::RoutingTable(void)
{
	tableCount = 0;
	myNode.weight = MAX_WEIGHT;
	iAmMaster=false;
	shortestPath = MAX_WEIGHT;
	millis_delta = 0;
	printf_P(PSTR("Created new routing table"));
}

RoutingTable::~RoutingTable(void)
{
}


void RoutingTable::setMillis(uint64_t data)
{
	unsigned long a =  data;
	millis_delta = millis() - a;

	printf_P(PSTR("SetMillis called: data:%lu delta:%l \n\r"),a,millis_delta);
}
unsigned long RoutingTable::getMillis()
{
	return millis() + millis_delta;
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
	//printf_P(PSTR("%lu: getCurrentNode IP:%u mac: %lu \n\r"),millis(), myNode.ip, myNode.mac);
	return myNode;
}


void RoutingTable::setCurrentNode(T_IP myIP)
{
	static int i=0;
	printf_P(PSTR("%lu:SetCurrentNode called %u times"),millis(), ++i);
	
	if(myIP == MASTER_SYNC_ADDRESS.ip)
	{
		this->myNode = MASTER_SYNC_ADDRESS;
		iAmMaster = true;
		printf_P(PSTR("%lu:SetCurrent This is master node ip:%u, mac:%lu weight:%02x\n\r"),millis(),myNode.ip, myNode.mac, myNode.weight);
	}
	else
	{
		this->myNode.ip = myIP;
		this->myNode.mac = base_address | myIP;
		this->myNode.weight = MAX_WEIGHT;
		printf_P(PSTR("%lu: SetCurrent3 MyNode IP:%d mac: %lu weight:%u \n\r"),millis(),myNode.ip,myNode.mac, this->myNode.weight );
		if(this->myNode.weight == MAX_WEIGHT)
		{
			printf_P(PSTR("%lu: SetCurrent4 it is 0 MyNode IP:%d mac: %lu weight:%u \n\r"),millis(),myNode.ip,myNode.mac, this->myNode.weight );
		}
	}
	
}
int8_t RoutingTable::checkTable(T_IP ip)
{
	for(int i=0;i<tableCount;i++)
	{
		if(table[i].ip == ip)
			return i;
	}
	
	return -1;
}
bool RoutingTable::addNearNode(IP_MAC nearNode)
{
	bool result = false;
	int8_t position = checkTable(nearNode.ip);
	
	printf_P(PSTR("%lu: addNearNode IP:%d mac: %lu weight:%lu myweight:%lu \n\r"),millis(),nearNode.ip,nearNode.mac,nearNode.weight,myNode.weight );
	if(nearNode.weight + 1 < myNode.weight)
	{
		printf_P(PSTR("%lu: addNearNode IP:%d position:%d \n\r"),millis(),nearNode.ip, position );
		myNode.weight = nearNode.weight + 1;
		if(position != -1)
		{
			table[position] = nearNode;
			shortestPath = position;
		}
		else
		{
			table[tableCount] = nearNode;
			shortestPath = tableCount++;
		}
		result = true;
	}
	else
	{
		if(position != -1)
			table[position] = nearNode;
		else
			table[tableCount++] = nearNode;
	}

	if(tableCount == MAX_NEAR_NODE)
	{			
		printf_P(PSTR("%lu: *****WARNING**** reached to maximum routing table size %d\r\n"), millis(), MAX_NEAR_NODE);;
		tableCount--;
	}
	return result;
}
int8_t RoutingTable::getShortestNodePosition()
{
	uint8_t result = MAX_WEIGHT;
    int8_t pos = MAX_WEIGHT;

	for(int i=0;i<tableCount;i++)
	{
		if(table[i].weight<result)
		{	
			result = table[i].weight;
			pos = i;
		}
	}
	return pos;
}

void RoutingTable::removeUnreacheable(IP_MAC nearNode)
{
	int8_t position = checkTable(nearNode.ip);
	
	printf_P(PSTR("%lu: removeUnreacheable IP:%d mac: %lu weight:%lu myweight:%lu \n\r"),millis(),nearNode.ip,nearNode.mac,nearNode.weight,myNode.weight );
	
	for(int i=position;i<tableCount;i++)
	{
		table[i] = table[i+1];
	}
	tableCount--;
	
	printf_P(PSTR("%lu: removeUnreacheable shortestPath == position IP:%d position:%d \n\r"),millis(),nearNode.ip, position );
	position = getShortestNodePosition();
	if(position == MAX_WEIGHT)
		cleanTable();
	
}
void RoutingTable::addReacheableNode(T_IP nearNodeID, T_IP* reachableNodeID, int numOfReacheableNodes)
{

}

IP_MAC RoutingTable::getShortestRouteNode()
{
	printf_P(PSTR("%lu: getShortestRouteNode \r\n"), millis());
	if(iAmMaster)
	{
		printf_P(PSTR("%lu: *********DONT CALLME********I AM MASTER********getShortestRouteNode \r\n"), millis());
		return MASTER_SYNC_ADDRESS;
	}
	else 
		return table[shortestPath];
}

void RoutingTable::cleanTable()
{
	if(!amImaster())
	{
		tableCount = 0;
		myNode.weight = MAX_WEIGHT;
		shortestPath = MAX_WEIGHT;
	}
}

bool RoutingTable::amIJoinedNetwork()
{
	if(shortestPath == MAX_WEIGHT)
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

void RoutingTable::printTable()
{
	printf_P(PSTR("----JOINED TABLE---------\n\r"));
	for(int i=0;i<tableCount;i++)
	{
		printf_P(PSTR("ip:%u mac:%lu weight:%u\n\r"),table[i].ip, table[i].mac, table[i].weight); 
	}
	printf_P(PSTR("----END OF JOINED TABLE---------\n\r"));
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
	printf_P(PSTR("%lu: ----WARNING--- getMac called for IP: %u mac: %lu\n\r"), millis(),ip,result);
	return result;
}

T_MAC RoutingTable::getShortestMac(T_IP ip)
{
	T_MAC result=0;

	if(iAmMaster)
	{
		printf_P(PSTR("%lu: *********DONT CALLME********I AM MASTER********getShortestMac ip:%u \r\n"), millis(),ip);
	}
	
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
		printf_P(PSTR("%lu: ----WARNING--- getShortestMac called for IP: %u mac: %lu\n\r"), millis(),ip,result);
	return result;
}