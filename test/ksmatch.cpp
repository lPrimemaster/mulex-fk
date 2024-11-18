#include "test.h"
#include "../mxsystem.h"
#include "../mxlogger.h"

void MakePrintCase(const std::string& p, const std::string& t, bool expect)
{
	using namespace mulex;

	LogTrace("Pattern: %s", p.c_str());
	LogTrace(" Target: %s", t.c_str());

	bool match = SysMatchPattern(p, t);
	LogTrace("  Match: %d\n", match);

	ASSERT_THROW(match == expect);
}

int main(void)
{
	MakePrintCase("/system/*/value", "/system/key0/value", true);
	MakePrintCase("/system/*/value", "/system/key0/key1/value", true);
	MakePrintCase("/system/*/value", "/system/key0/novalue", false);
	MakePrintCase("/system/*/value", "/system/key0/key1/novalue", false);
	MakePrintCase("/system/*/value", "/system/key0/valueno", false);
	MakePrintCase("/system/*/value", "/system/key0/key1/valueno", false);

	MakePrintCase("/system/*/value", "/system0/key0/value", false);
	MakePrintCase("/system/*/value", "/system0/key0/key1/value", false);
	MakePrintCase("/system/*/value", "/system/value", true);
	MakePrintCase("/system/*/value", "/system0/value", false);
	MakePrintCase("/system/*/value", "/system/value0", false);

	MakePrintCase("/system/*/intermediate/*/value", "/system/key0/intermediate/key1/value", true);
	MakePrintCase("/system/*/intermediate/*/value", "/system/key0/intermediate0/key1/value", false);
	MakePrintCase("/system/*/intermediate/*/value", "/system/intermediate/value", true);

	MakePrintCase("/system/*/k1/*/k3/*/value", "/system/k0/k1/k2/k3/k4/value", true);

	MakePrintCase("/system/*", "/system/value0", true);
	MakePrintCase("/system/*", "/system/value1", true);

	return 0;
}
