#include "DBStoredProcedure.h"
#include <sstream>

namespace GameDataBase
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
		// 자동 정리
	}

	auto DBStoredProcedure::add_output_parameter(const std::wstring& param_name, SQLSMALLINT sql_type, SQLULEN size) -> void
	{
		ProcedureParameter param;
		param.name = param_name;
		param.direction = ParameterDirection::OUTPUT;
		param.sql_type = sql_type;
		param.size = size;
		param.indicator = SQL_NULL_DATA;
		
		// 출력 파라미터를 위한 버퍼 할당
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
		
		// 리턴 값은 항상 첫 번째 파라미터로 추가
		parameters_.insert(parameters_.begin(), std::move(param));
	}

	auto DBStoredProcedure::execute() -> std::tuple<bool, std::optional<std::string>>
	{
		// 호출문 생성
		std::wstring call_statement = build_call_statement();
		
		// 파라미터 바인딩
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
		// 리턴 값 파라미터 찾기
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
		// 다음 결과 집합으로 이동
		// SQL Server에서는 SQLMoreResults 함수 사용
		// 여기서는 간단히 구현
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
			
			// 파라미터 타입에 따라 바인딩
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
		
		// 리턴 값이 있는 경우
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
		
		// 파라미터 수만큼 ? 추가 (리턴 값 제외)
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
