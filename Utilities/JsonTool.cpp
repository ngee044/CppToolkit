#include "JsonTool.h"

#include <sstream>

namespace Utilities
{
	auto JsonTool::pretty_print(std::ostream& output_stream, const boost::json::value& value, int indent_width, int depth) -> void
	{
		std::string indent(depth * indent_width, ' ');
		switch (value.kind())
		{
		case boost::json::kind::object:
		{
			output_stream << "{";
			std::string new_indent((depth + 1) * indent_width, ' ');
			auto const& obj = value.get_object();
			if (!obj.empty())
			{
				output_stream << "\n";
				auto it = obj.begin();
				for (;;)
				{
					output_stream << new_indent << boost::json::serialize(it->key()) << ": ";
					pretty_print(output_stream, it->value(), indent_width, depth + 1);
					if (++it == obj.end())
					{
						break;
					}
					output_stream << ",\n";
				}
				output_stream << "\n" << indent;
			}
			output_stream << "}";
			break;
		}
		case boost::json::kind::array:
		{
			output_stream << "[";
			std::string new_indent((depth + 1) * indent_width, ' ');
			auto const& arr = value.get_array();
			if (!arr.empty())
			{
				output_stream << "\n";
				auto it = arr.begin();
				for (;;)
				{
					output_stream << new_indent;
					pretty_print(output_stream, *it, indent_width, depth + 1);
					if (++it == arr.end())
					{
						break;
					}
					output_stream << ",\n";
				}
				output_stream << "\n" << indent;
			}
			output_stream << "]";
			break;
		}
		default:
			output_stream << boost::json::serialize(value);
			break;
		}

		if (depth == 0)
		{
			output_stream << "\n";
		}
	}
	auto JsonTool::pretty_format(const boost::json::value& value, int indent_width, int depth) -> std::string
	{
		std::stringstream ss;
		pretty_print(ss, value, indent_width, depth);
		return ss.str();
	}
} // namespace Utilities
