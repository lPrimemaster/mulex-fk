#include "../mxrdb.h"

int main(int argc, char* argv[])
{
	if(argc != 6)
	{
		std::cout << "USAGE: ./rdbreader <hostname> <op> <key> <type> <value>" << std::endl;
		return 0;
	}

	if(!mulex::SysConnectToExperiment(argv[1]))
	{
		return 0;
	}

	mulex::RdbAccess ra;

	std::string op = argv[2];

	std::string key = argv[3];
	std::string type = argv[4];

	if(op == "read")
	{
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
		else if(type == "int64")
		{
			std::int64_t fa = ra[key];
			std::cout << fa << std::endl;
		}
		else if(type == "uint64")
		{
			std::uint64_t fa = ra[key];
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
	}
	else if(op == "write")
	{
		if(!ra[key].exists())
		{
			std::string value = argv[5];
			if(type == "int8")
			{
				ra[key].create(mulex::RdbValueType::INT8, std::int8_t(std::stoi(value)));
			}
			else if(type == "uint8")
			{
				ra[key].create(mulex::RdbValueType::UINT8, std::uint8_t(std::stoi(value)));
			}
		}
		else
		{
			std::string value = argv[5];
			if(type == "int8")
			{
				ra[key] = std::int8_t(std::stoi(value));
			}
			else if(type == "uint8")
			{
				ra[key] = std::uint8_t(std::stoi(value));
			}
		}
	}
	
	mulex::SysDisconnectFromExperiment();
}
