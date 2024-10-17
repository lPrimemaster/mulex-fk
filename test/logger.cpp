#include "test.h"
#include "../mxlogger.h"
#include <sstream>

using namespace mulex;


void TestLogBasic()
{
	std::stringstream ss;
	cout_redirect redirect_guard(ss.rdbuf());

	LogError("Error log");
	ASSERT_THROW(ss.str() == "[ERROR] Error log\n");
	ss.str("");
	LogWarning("Warning log");
	ASSERT_THROW(ss.str() == "[WARN] Warning log\n");
	ss.str("");
	LogMessage("Message log");
	ASSERT_THROW(ss.str() == "[MSG] Message log\n");
	ss.str("");
	LogDebug("Debug log");
	ASSERT_THROW(ss.str() == "[DEBUG] Debug log\n");
}

int main(void)
{
	TestLogBasic();
	return 0;
}
