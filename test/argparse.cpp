#include "test.h"
#include "../mxsystem.h"

int main(int argc, char* argv[])
{
	using namespace mulex;

	SysAddArgument("name", 'n', true, [](const std::string& val){
		std::cout << "Got arg: " << val << std::endl;
	});

	SysAddArgument("daemon", 'D', false, [](const std::string& val){
		std::cout << "Running daemon." << std::endl;
	});

	// Fails
	SysAddArgument("help", 'h', false, [](const std::string& val){
		std::cout << "Fake help." << std::endl;
	});


	SysAddArgument("proc", 'p', false, [](const std::string& val){
		std::cout << SysGetBinaryName() << std::endl;
	});

	SysParseArguments(argc, argv);
	return 0;
}
