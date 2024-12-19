#pragma once
#include <string>
#include <iostream>
#include <stdarg.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <chrono>
#include <tracy/Tracy.hpp>

namespace mulex
{
	namespace detail
	{
		struct LogErrorPolicy
		{
			constexpr static std::string_view Prefix() { return "[ERROR]"; }
			constexpr static std::uint32_t Color() { return 0xFF0000; }
		};

		struct LogWarningPolicy
		{
			constexpr static std::string_view Prefix() { return  "[WARN]"; }
			constexpr static std::uint32_t Color() { return 0xFFF000; }
		};

		struct LogMessagePolicy
		{
			constexpr static std::string_view Prefix() { return   "[MSG]"; }
			constexpr static std::uint32_t Color() { return 0x0068FF; }
		};

		struct LogDebugPolicy
		{
			constexpr static std::string_view Prefix() { return "[DEBUG]"; }
			constexpr static std::uint32_t Color() { return 0x4D4D4D; }
		};

		struct LogTracePolicy
		{
			constexpr static std::string_view Prefix() { return "[TRACE]"; }
			constexpr static std::uint32_t Color() { return 0xBFBFBF; }
		};
	}

	static std::string GetTimeString()
	{
		std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		char time_buf[64];
		std::strftime(time_buf, 64, "%d.%m.%Y %H:%M:%S", std::localtime(&time));
		return time_buf;
	}

	template<typename Policy>
	void Log(const char* fmt, ...)
	{
#ifndef LTRACE
		// If LTRACE is off do not print/log traces
		if constexpr(std::is_same_v<Policy, detail::LogTracePolicy>) return;
#endif
		constexpr bool kLogToStdOut = true;
		constexpr bool kLogToFile = true;
		constexpr bool kLogToTracy = true;

		va_list vargs, vargscpy;
		va_start(vargs, fmt);
		va_copy(vargscpy, vargs);

		int vsize = vsnprintf(nullptr, 0, fmt, vargs);
		va_end(vargs);
		std::vector<char> buffer;
		buffer.resize(vsize + 1);
		vsnprintf(buffer.data(), vsize + 1, fmt, vargscpy);
		va_end(vargscpy);

		// Fully form the output
		// This avoids the need to use a mutex to print
		// Making the logger natively thread-safe
		std::stringstream ss;
		ss << "[" << GetTimeString() << "]" << Policy::Prefix() << " " << buffer.data() << '\n';
		std::string output = ss.str();
		
		if constexpr(kLogToStdOut)
		{
			std::cout << output << std::flush;
		}

		if constexpr(kLogToFile)
		{
			static std::ofstream file("log.txt");
			file << output;
		}

		if constexpr(kLogToTracy)
		{
			TracyMessageC(output.c_str(), output.size(), Policy::Color());
		}
	}

	constexpr void(*LogError)(const char*, ...)   = &Log<detail::LogErrorPolicy>;
	constexpr void(*LogWarning)(const char*, ...) = &Log<detail::LogWarningPolicy>;
	constexpr void(*LogMessage)(const char*, ...) = &Log<detail::LogMessagePolicy>;
	constexpr void(*LogDebug)(const char*, ...)   = &Log<detail::LogDebugPolicy>;
	constexpr void(*LogTrace)(const char*, ...)   = &Log<detail::LogTracePolicy>;
} // namespace mulex
