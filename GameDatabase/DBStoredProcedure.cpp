#include "DBStoredProcedure.h"
#include <sstream>

namespace GameDatabase
{
	DBStoredProcedure::DBStoredProcedure(std::shared_ptr<DBConnection> connection, const std::wstring& procedure_name)
		: connection_(connection)
		, procedure_name_(procedure_name)
		, is_prepared_(false)
		, return_value_(0)
		, return_indicator_(0)
	{
		if (!connection_)
		{
			throw std::invalid_argument("Database connection is null");
		}
	}

	DBStoredProcedure::~DBStoredProcedure()
	{
	}

	auto DBStoredProcedure::add_output_parameter(const std::wstring& param_name, SQLSMALLINT sql_type, SQLULEN size) -> void
	{
		ProcedureParameter param;
		param.name = param_name;
		param.direction = ParameterDirection::OUTPUT;
		param.sql_type = sql_type;
		param.size = size;
		param.indicator = SQL_NULL_DATA;
		
		switch (sql_type)
		{
			case SQL_C_BIT:
				param.value = bool{};
				break;
			case SQL_C_SSHORT:
				param.value = std::int16_t{};
				break;
			case SQL_C_SLONG:
				param.value = std::int32_t{};
				break;
			case SQL_C_SBIGINT:
				param.value = std::int64_t{};
				break;
			case SQL_C_FLOAT:
				param.value = float{};
				break;
			case SQL_C_DOUBLE:
				param.value = double{};
				break;
			case SQL_C_WCHAR:
				param.value = std::wstring(size > 0 ? size : 256, L'\0');
				break;
			default:
				param.value = std::vector<BYTE>(size > 0 ? size : 256);
				break;
		}
		
		parameters_.push_back(std::move(param));
	}

	auto DBStoredProcedure::add_return_value_parameter() -> void
	{
		ProcedureParameter param;
		param.name = L"@RETURN_VALUE";
		param.direction = ParameterDirection::RETURN_VALUE;
		param.sql_type = SQL_C_SLONG;
		param.size = sizeof(std::int32_t);
		param.value = return_value_;
		param.indicator = return_indicator_;
		
		parameters_.insert(parameters_.begin(), std::move(param));
	}

	auto DBStoredProcedure::execute() -> std::tuple<bool, std::optional<std::string>>
	{
		std::wstring call_statement = build_call_statement();
		
		auto [bind_success, bind_error] = bind_parameters();
		if (!bind_success)
		{
			return { false, bind_error };
		}
		// 저장 프로시저 실행
		return connection_->execute(call_statement);
	}

	auto DBStoredProcedure::fetch() -> std::tuple<bool, std::optional<std::string>>
	{
		return connection_->fetch();
	}

	auto DBStoredProcedure::get_return_value(std::int32_t& value) -> std::tuple<bool, std::optional<std::string>>
	{
		auto it = std::find_if(parameters_.begin(), parameters_.end(),
			[](const ProcedureParameter& param) 
			{
				return param.direction == ParameterDirection::RETURN_VALUE;
			});
			
		if (it == parameters_.end())
		{
			return { false, "Return value parameter not found. Call add_return_value_parameter() first." };
		}
		
		try
		{
			value = std::any_cast<std::int32_t>(it->value);
			return { true, std::nullopt };
		}
		catch (const std::bad_any_cast& e)
		{
			return { false, "Failed to get return value" };
		}
	}

	auto DBStoredProcedure::next_result_set() -> std::tuple<bool, std::optional<std::string>>
	{
		return { true, std::nullopt };
	}

	auto DBStoredProcedure::clear_parameters() -> void
	{
		parameters_.clear();
		column_indicators_.clear();
		is_prepared_ = false;
		return_value_ = 0;
		return_indicator_ = 0;
	}

	auto DBStoredProcedure::bind_parameters() -> std::tuple<bool, std::optional<std::string>>
	{
		std::int32_t param_index = 1;
		
		for (auto& param : parameters_)
		{
			std::tuple<bool, std::optional<std::string>> result;
			
			if (param.value.type() == typeid(bool))
			{
				auto* value = std::any_cast<bool>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(std::int16_t))
			{
				auto* value = std::any_cast<std::int16_t>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(std::int32_t))
			{
				auto* value = std::any_cast<std::int32_t>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(std::int64_t))
			{
				auto* value = std::any_cast<std::int64_t>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(float))
			{
				auto* value = std::any_cast<float>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(double))
			{
				auto* value = std::any_cast<double>(&param.value);
				result = connection_->bind_param(param_index, value, &param.indicator);
			}
			else if (param.value.type() == typeid(std::wstring))
			{
				auto* value = std::any_cast<std::wstring>(&param.value);
				result = connection_->bind_param(param_index, const_cast<WCHAR*>(value->c_str()), &param.indicator);
			}
			else if (param.value.type() == typeid(std::vector<BYTE>))
			{
				auto* value = std::any_cast<std::vector<BYTE>>(&param.value);
				result = connection_->bind_param(param_index, value->data(), static_cast<std::int32_t>(value->size()), &param.indicator);
			}
			
			if (!std::get<0>(result))
			{
				return result;
			}
			
			param_index++;
		}
		
		return { true, std::nullopt };
	}

	auto DBStoredProcedure::build_call_statement() -> std::wstring
	{
		std::wstringstream call_stmt;
		call_stmt << L"{";
		
		bool has_return_value = false;
		for (const auto& param : parameters_)
		{
			if (param.direction == ParameterDirection::RETURN_VALUE)
			{
				has_return_value = true;
				call_stmt << L"? = ";
				break;
			}
		}
		
		call_stmt << L"CALL " << procedure_name_ << L"(";
		
		bool first = true;
		for (const auto& param : parameters_)
		{
			if (param.direction != ParameterDirection::RETURN_VALUE)
			{
				if (!first)
				{
					call_stmt << L", ";
				}
				call_stmt << L"?";
				first = false;
			}
		}
		
		call_stmt << L")}";
		
		return call_stmt.str();
	}
}
