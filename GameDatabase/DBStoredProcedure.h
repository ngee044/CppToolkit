#pragma once

#include "DBConnection.h"
#include "DBBind.h"

#include <memory>
#include <variant>
#include <any>

namespace GameDatabase
{
	// 저장 프로시저 파라미터 타입
	enum class ParameterDirection
	{
		INPUT,
		OUTPUT,
		INPUT_OUTPUT,
		RETURN_VALUE
	};

	// 파라미터 정보 구조체
	struct ProcedureParameter
	{
		std::wstring name;
		ParameterDirection direction;
		SQLSMALLINT sql_type;
		SQLULEN size;
		std::any value;
		SQLLEN indicator;
	};

	// 저장 프로시저 호출을 위한 래퍼 클래스
	class DBStoredProcedure
	{
	public:
		DBStoredProcedure(std::shared_ptr<DBConnection> connection, const std::wstring& procedure_name);
		~DBStoredProcedure();

		// 입력 파라미터 추가
		template<typename T>
		auto add_input_parameter(const std::wstring& param_name, const T& value) -> void;
		
		// 출력 파라미터 추가
		auto add_output_parameter(const std::wstring& param_name, SQLSMALLINT sql_type, SQLULEN size = 0) -> void;
		
		// 입출력 파라미터 추가
		template<typename T>
		auto add_input_output_parameter(const std::wstring& param_name, T& value) -> void;
		
		// 리턴 값 파라미터 추가
		auto add_return_value_parameter() -> void;

		// 저장 프로시저 실행
		auto execute() -> std::tuple<bool, std::optional<std::string>>;
		
		// 결과 집합 가져오기
		auto fetch() -> std::tuple<bool, std::optional<std::string>>;
		
		// 출력 파라미터 값 가져오기
		template<typename T>
		auto get_output_parameter(const std::wstring& param_name, T& value) -> std::tuple<bool, std::optional<std::string>>;
		
		// 리턴 값 가져오기
		auto get_return_value(std::int32_t& value) -> std::tuple<bool, std::optional<std::string>>;

		// 다중 결과 집합 처리
		auto next_result_set() -> std::tuple<bool, std::optional<std::string>>;
		
		// 결과 컬럼 바인딩
		template<typename T>
		auto bind_column(std::int32_t column_index, T& value) -> std::tuple<bool, std::optional<std::string>>;

		// 파라미터 초기화
		auto clear_parameters() -> void;

	private:
		// 파라미터 바인딩
		auto bind_parameters() -> std::tuple<bool, std::optional<std::string>>;
		
		// 저장 프로시저 호출문 생성
		auto build_call_statement() -> std::wstring;

	private:
		std::shared_ptr<DBConnection> connection_;
		std::wstring procedure_name_;
		std::vector<ProcedureParameter> parameters_;
		std::vector<SQLLEN> column_indicators_;
		bool is_prepared_;
		std::int32_t return_value_;
		SQLLEN return_indicator_;
	};

	// 템플릿 구현
	template<typename T>
	auto DBStoredProcedure::add_input_parameter(const std::wstring& param_name, const T& value) -> void
	{
		ProcedureParameter param;
		param.name = param_name;
		param.direction = ParameterDirection::INPUT;
		param.value = value;
		param.indicator = sizeof(T);
		
		// SQL 타입 매핑
		if constexpr (std::is_same_v<T, bool>)
		{
			param.sql_type = SQL_C_BIT;
		}
		else if constexpr (std::is_same_v<T, std::int16_t>)
		{
			param.sql_type = SQL_C_SSHORT;
		}
		else if constexpr (std::is_same_v<T, std::int32_t>)
		{
			param.sql_type = SQL_C_SLONG;
		}
		else if constexpr (std::is_same_v<T, std::int64_t>)
		{
			param.sql_type = SQL_C_SBIGINT;
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			param.sql_type = SQL_C_FLOAT;
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			param.sql_type = SQL_C_DOUBLE;
		}
		else if constexpr (std::is_same_v<T, std::wstring>)
		{
			param.sql_type = SQL_C_WCHAR;
			param.size = static_cast<SQLULEN>(value.length() + 1);
		}
		
		parameters_.push_back(std::move(param));
	}

	template<typename T>
	auto DBStoredProcedure::add_input_output_parameter(const std::wstring& param_name, T& value) -> void
	{
		ProcedureParameter param;
		param.name = param_name;
		param.direction = ParameterDirection::INPUT_OUTPUT;
		param.value = value;
		param.indicator = sizeof(T);
		
		// SQL 타입 매핑 (add_input_parameter와 동일)
		if constexpr (std::is_same_v<T, bool>)
		{
			param.sql_type = SQL_C_BIT;
		}
		else if constexpr (std::is_same_v<T, std::int16_t>)
		{
			param.sql_type = SQL_C_SSHORT;
		}
		else if constexpr (std::is_same_v<T, std::int32_t>)
		{
			param.sql_type = SQL_C_SLONG;
		}
		else if constexpr (std::is_same_v<T, std::int64_t>)
		{
			param.sql_type = SQL_C_SBIGINT;
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			param.sql_type = SQL_C_FLOAT;
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			param.sql_type = SQL_C_DOUBLE;
		}
		else if constexpr (std::is_same_v<T, std::wstring>)
		{
			param.sql_type = SQL_C_WCHAR;
			param.size = static_cast<SQLULEN>(value.length() + 1);
		}
		
		parameters_.push_back(std::move(param));
	}

	template<typename T>
	auto DBStoredProcedure::get_output_parameter(const std::wstring& param_name, T& value) -> std::tuple<bool, std::optional<std::string>>
	{
		auto it = std::find_if(parameters_.begin(), parameters_.end(),
			[&param_name](const ProcedureParameter& param) 
			{
				return param.name == param_name && 
					(param.direction == ParameterDirection::OUTPUT || 
						param.direction == ParameterDirection::INPUT_OUTPUT);
			});
			
		if (it == parameters_.end())
		{
			return { false, "Output parameter not found: " + std::string(param_name.begin(), param_name.end()) };
		}
		
		try
		{
			value = std::any_cast<T>(it->value);
			return { true, std::nullopt };
		}
		catch (const std::bad_any_cast& e)
		{
			return { false, "Type mismatch for output parameter" };
		}
	}

	template<typename T>
	auto DBStoredProcedure::bind_column(std::int32_t column_index, T& value) -> std::tuple<bool, std::optional<std::string>>
	{
		if (column_indicators_.size() < static_cast<size_t>(column_index))
		{
			column_indicators_.resize(column_index);
		}
		
		return connection_->bind_column(column_index, &value, &column_indicators_[column_index - 1]);
	}
}
