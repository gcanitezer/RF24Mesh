#include "RoutingTable.h"
#include "RF24Network_config.h"

const IP_MAC BROADCAST_ADDRESS = {0x7918,  0};

const IP_MAC MASTER_SYNC_ADDRESS = {0x0000,  0};

const T_MAC base_address = 0xE8E8E8E8LL;

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


void RoutingTable::setMillis(uint8_t data[16])
{
	unsigned long a=0;
	//memcpy(&a,&data, sizeof(unsigned long));
	//unsigned long a =  data;
	a = data[3];
	a=  (a << 8) + data[2];
	a = (a << 8) + data[1];
	a = (a << 8) + data[0];

	if(a > millis())
	{
		millis_delta = a - millis();
		millis_delta_positive =  true;
	}else
	{
		millis_delta = millis() - a;
		millis_delta_positive =  false;
	}
	printf_P(PSTR("SetMillis called: sizeof unsigned long is %d a:%lx delta:%lx d0:%d d1:%d d2:%d d3:%d \n\r"),sizeof(unsigned long),a,millis_delta, data[0], data[1],data[2],data[3]);
}
unsigned long RoutingTable::getMillis()
{
	if(millis_delta_positive)
		return millis() + millis_delta;
	else
		return millis() - millis_delta;
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
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: getCurrentNode IP:%u weight: %u \n\r"),millis(), myNode.ip, myNode.weight));
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
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu:SetCurrent This is master node ip:%u,  weight:%02x\n\r"),millis(),myNode.ip,  myNode.weight));
	}
	else
	{
		this->myNode.ip = myIP;
		this->myNode.weight = MAX_WEIGHT;
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: SetCurrent3 MyNode IP:%d  weight:%u \n\r"),millis(),myNode.ip, this->myNode.weight ));
		if(this->myNode.weight == MAX_WEIGHT)
		{
			IF_SERIAL_DEBUG(printf_P(PSTR("%lu: SetCurrent4 it is 0 MyNode IP:%d  weight:%u \n\r"),millis(),myNode.ip, this->myNode.weight ));
		}
	}
	
}
int8_t RoutingTable::checkTable(T_IP ip)
{
	for(int i=0;i<tableCount;i++)
	{
		if(table[i].ip_mac.ip == ip)
			return i;
	}
	
	return -1;
}
bool RoutingTable::addNearNode(IP_MAC nearNode)
{
	bool result = false;
	int8_t position = checkTable(nearNode.ip);
	
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: addNearNode IP:%d weight:0%d myweight:0%d \n\r"),millis(),nearNode.ip,nearNode.weight,myNode.weight ));
	if(nearNode.weight + 1 < myNode.weight)
	{
		IF_SERIAL_DEBUG(printf_P(PSTR("%lu: addNearNode IP:%d position:%d \n\r"),millis(),nearNode.ip, position ));
		myNode.weight = nearNode.weight + 1;
		if(position != -1)
		{
			table[position].ip_mac = nearNode;
			shortestPath = position;
		}
		else
		{
			table[tableCount].ip_mac = nearNode;
			shortestPath = tableCount++;
		}
		table[shortestPath].time = getMillis();
		table[shortestPath].status = SHORTENED;
		result = true;
	}
	else
	{
		if(position != -1)
		{
			table[position].ip_mac = nearNode;
			table[position].status = GOT_JOIN;
		}
		else
		{
			table[tableCount].status = GOT_JOIN;
			table[tableCount++].ip_mac = nearNode;

		}
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
		if(table[i].ip_mac.weight<result)
		{	
			result = table[i].ip_mac.weight;
			pos = i;
		}
	}
	return pos;
}

bool RoutingTable::removeUnreacheable(IP_MAC nearNode)
{
	int8_t position = checkTable(nearNode.ip);
	
	printf_P(PSTR("%lu: removeUnreacheable IP:%d weight:%lu myweight:%lu \n\r"),millis(),nearNode.ip,nearNode.weight,myNode.weight );
	
	for(int i=position;i<tableCount;i++)
	{
		table[i] = table[i+1];
	}
	tableCount--;
	
	if(tableCount == -1)
	{
		return false;
	}

	printf_P(PSTR("%lu: removeUnreacheable shortestPath == position IP:%d position:%d \n\r"),millis(),nearNode.ip, position );
	position = getShortestNodePosition();
	if(position == MAX_WEIGHT)
		cleanTable();

	return true;
}
void RoutingTable::addReacheableNode(T_IP nearNodeID, T_IP* reachableNodeID, int numOfReacheableNodes)
{

}

IP_MAC RoutingTable::getShortestRouteNode()
{
	IF_SERIAL_DEBUG(printf_P(PSTR("%lu: getShortestRouteNode \r\n"), millis()));
	if(iAmMaster)
	{
		return MASTER_SYNC_ADDRESS;
	}
	else 
		return table[shortestPath].ip_mac;
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

void RoutingTable::sentData(RF24NetworkHeader h)
{
	//TODO fill in sentData(RF24NetworkHeader h)
}
bool RoutingTable::amIJoinedNetwork()
{
	if(shortestPath >= MAX_WEIGHT)
		return false;
	else 
		return true;
}

int RoutingTable::getTableSize()
{
	return tableCount;
}

RoutingData* RoutingTable::getTable()
{
	return table;
}

void RoutingTable::printTable()
{
	printf_P(PSTR("----JOINED TABLE---------\n\r"));
	for(int i=0;i<tableCount;i++)
	{
		printf_P(PSTR("ip:%u  weight:%u\n\r"),table[i].ip_mac.ip,  table[i].ip_mac.weight);
	}
	printf_P(PSTR("my_ip:%u  my_weight:%u shortest path = ip:%u  weight:%u \n\r"),myNode.ip,  myNode.weight, table[shortestPath].ip_mac.ip,  table[shortestPath].ip_mac.weight);
	printf_P(PSTR("----END OF JOINED TABLE---------\n\r"));
}

bool RoutingTable::isPathShortened()
{
//TODO isPathShortened
}
void RoutingTable::connectShortened()
{
//TODO connectShortened
}

int RoutingTable::getNumOfWelcomes()
{
//TODO getNumOfWelcomes
}

int RoutingTable::getNumOfJoines()
{
//TODO getNumOfJoines
}

void RoutingTable::setWelcomeMessageSent(T_IP ip)
{
//TODO setWelcomeMessageSent
}

void RoutingTable::setConnected(T_IP ip)
{
//TODO setConnected
}

T_MAC RoutingTable::getBroadcastMac()
{
	return base_address + BROADCAST_ADDRESS.ip;
}

T_MAC RoutingTable::getMac(T_IP ip)
{
	T_MAC result=0;
/*
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
*/	
	result = base_address + ip;

	return result;
}

T_MAC RoutingTable::getShortestMac(T_IP ip)
{
	T_MAC result=0;

	if(iAmMaster)
	{
		printf_P(PSTR("%lu: *********DONT CALLME********I AM MASTER********getShortestMac ip:%u \r\n"), millis(),ip);
	}
	/*
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
	*/


	result = base_address;

	return result;
}
