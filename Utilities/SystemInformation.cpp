#include "SystemInformation.h"
#include "boost/asio.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif


namespace Utilities
{
	uint64_t SystemInformation::memory_usage(void)
	{
#ifdef _WIN32
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);

		return (memInfo.ullTotalPhys / 1024 / 1024) - (memInfo.ullAvailPhys / 1024 / 1024);
#else
		struct rusage r_usage;
		getrusage(RUSAGE_SELF, &r_usage);

		return r_usage.ru_maxrss / 1024;
#endif
	}

	auto SystemInformation::get_ip_addresses(void) -> std::vector<std::string>
	{
		std::vector<std::string> ip_addresses;

		boost::asio::io_context io_context;
		boost::asio::ip::tcp::resolver resolver(io_context);

		auto host = boost::asio::ip::host_name();

		boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, "");

		for (const auto& endpoint : endpoints)
		{
			std::string ip_address = endpoint.endpoint().address().to_string();
			ip_addresses.push_back(ip_address);
		}

		return ip_addresses;
	}
}
