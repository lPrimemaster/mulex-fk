#pragma once
#include <string>
#include <cstring>

namespace mulex
{
	template<std::uint64_t S>
	struct mxstring
	{
		inline mxstring() = default;

		inline /* implicit */ mxstring(const char* data)
		{
			std::size_t sz = std::strlen(data) + 1;
			std::memcpy(_data, data, sz > S ? S : sz);
		}

		inline /* implicit */ mxstring(const std::string& data) : mxstring(data.c_str()) { }

		inline void operator=(const char* data)
		{
			std::size_t sz = std::strlen(data) + 1;
			std::memcpy(_data, data, sz > S ? S : sz);
		}

		inline void operator=(const std::string& data)
		{
			operator=(data.c_str());
		}

		inline const char* c_str() const
		{
			return &_data[0];
		}
	private:
		char _data[S];
	};

	using string32 = mxstring<32>;
}
