#include "../mxrdb.h"

int main(int argc, char* argv[])
{
	if(argc != 4)
	{
		std::cout << "USAGE: ./rdbreader <hostname> <key> <type>" << std::endl;
		return 0;
	}

	if(!mulex::SysConnectToExperiment(argv[1]))
	{
		return 0;
	}

	mulex::RdbAccess ra;

	std::string key = argv[2];
	std::string type = argv[3];

	std::cout << "===== Value =====" << std::endl;
	if(type == "string")
	{
		mulex::mxstring<512> fa = ra[key];
		std::cout << fa.c_str() << std::endl;
	}
	else if(type == "int")
	{
		std::int32_t fa = ra[key];
		std::cout << fa << std::endl;
	}
	else if(type == "uint")
	{
		std::uint32_t fa = ra[key];
		std::cout << fa << std::endl;
	}
	else if(type == "float")
	{
		float fa = ra[key];
		std::cout << fa << std::endl;
	}
	else if(type == "double")
	{
		double fa = ra[key];
		std::cout << fa << std::endl;
	}
	else if(type == "bool")
	{
		bool fa = ra[key];
		std::cout << fa << std::endl;
	}
	else
	{
		mulex::LogError("Type <%s> not implemented on rdbreader or overall unrecognized.", type.c_str());
	}
	std::cout << "=================" << std::endl;
	
	mulex::SysDisconnectFromExperiment();
}
