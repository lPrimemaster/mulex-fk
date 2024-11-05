#include "test.h"
#include "../mxsystem.h"
#include <sstream>

int main(int argc, char* argv[])
{
	using namespace mulex;

	// Fails
	SysAddArgument("help", 'h', false, [](const std::string& val){
		std::cout << "Fake help." << std::endl;
	});

#ifdef __linux__
	ASSERT_THROW(SysGetBinaryName() == "test_argparse");
#else
	ASSERT_THROW(SysGetBinaryName() == "test_argparse.exe");
#endif

	std::stringstream ss;
	{
		cout_redirect redg(ss.rdbuf());
		
		char* argvf[] = {
			(char*)"",
			(char*)"-h",
			NULL
		};

		SysParseArguments(2, argvf);

		ASSERT_THROW(ss.str() == 
			"Arguments help:\n\t-D\t--daemon\n\t\tTurn the current process into a daemon (linux only).\n\n\t-h\t--help\n\t\tPrints this help message.\n\n"
		);
	}
	std::cout << ss.str() << std::endl;
	return 0;
}
