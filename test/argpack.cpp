#include "../mxsystem.h"
#include "test.h"

struct TrivialCopyType
{
	TrivialCopyType() {}
	TrivialCopyType(int m0, double m1) : m0(m0), m1(m1) { }
	int m0;
	double m1;
	int f() { return m0; }
};

int main()
{
	using namespace mulex;

	int a0 = 42;
	double a1 = 3.141592654;
	TrivialCopyType t = { 7, 35.4 };

	auto buffer = SysPackArguments(a0, a1, t);
	auto [ra0, ra1, rt] = SysUnpackArguments<int, double, TrivialCopyType>(buffer);

	ASSERT_THROW(a0 == ra0);
	ASSERT_THROW(a1 == ra1);
	ASSERT_THROW(t.m0 == rt.m0);
	ASSERT_THROW(t.m1 == rt.m1);
	ASSERT_THROW(t.f() == rt.f());

	return 0;
}
