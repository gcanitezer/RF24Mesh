// MyTestSuite1.h
#include <cxxtest/TestSuite.h>
#include <RoutingTable.h>

class MyTestSuite1 : public CxxTest::TestSuite
{
	RoutingTable rTable;
public:
void testAddition(void)
{
TS_ASSERT(1 + 1 > 1);
TS_ASSERT_EQUALS(1 + 1, 2);
}

void testSubtraction(void)
{
	//
}
};