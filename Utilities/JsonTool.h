#pragma once

#include <ostream>
#include <string>

#include <boost/json.hpp>

namespace Utilities
{
	class JsonTool
	{
	public:
		static auto pretty_print(std::ostream& output_stream, const boost::json::value& value, int indent_width = 2, int depth = 0) -> void;
		static auto pretty_format(const boost::json::value& value, int indent_width = 2, int depth = 0) -> std::string;
	};
}
