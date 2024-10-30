#pragma once
#include <string>
#include <iostream>
#include <stdarg.h>
#include <vector>
#include <sstream>

namespace mulex
{
	namespace detail
	{
		struct LogErrorPolicy   { constexpr static std::string_view Prefix() { return "[ERROR]"; } };
		struct LogWarningPolicy { constexpr static std::string_view Prefix() { return  "[WARN]"; } };
		struct LogMessagePolicy { constexpr static std::string_view Prefix() { return   "[MSG]"; } };
		struct LogDebugPolicy   { constexpr static std::string_view Prefix() { return "[DEBUG]"; } };
		struct LogTracePolicy   { constexpr static std::string_view Prefix() { return "[TRACE]"; } };
	}

	template<typename Policy>
	void Log(const char* fmt, ...)
	{
#ifndef LTRACE
		// If LTRACE is off do not print/log traces
		if constexpr(std::is_same_v<Policy, detail::LogTracePolicy>) return;
#endif
		constexpr bool kLogToStdOut = true;

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
		// TODO: Create some GetBackendName() function
		ss << Policy::Prefix() << " " << buffer.data() << '\n';
		std::string output = ss.str();
		
		if constexpr(kLogToStdOut)
		{
			std::cout << output << std::flush;
		}
	}

	constexpr void(*LogError)(const char*, ...)   = &Log<detail::LogErrorPolicy>;
	constexpr void(*LogWarning)(const char*, ...) = &Log<detail::LogWarningPolicy>;
	constexpr void(*LogMessage)(const char*, ...) = &Log<detail::LogMessagePolicy>;
	constexpr void(*LogDebug)(const char*, ...)   = &Log<detail::LogDebugPolicy>;
	constexpr void(*LogTrace)(const char*, ...)   = &Log<detail::LogTracePolicy>;
} // namespace mulex
