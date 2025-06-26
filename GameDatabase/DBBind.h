#pragma once

#include "DBConnection.h"

#include <memory>

namespace GameDatabase
{
	// Compile-time bit mask generation
	template<std::int32_t C>
	struct FullBits
	{
		static constexpr std::int32_t value = (C > 0) ? ((1 << (C - 1)) | FullBits<C - 1>::value) : 0;
	};

	template<>
	struct FullBits<0> 
	{ 
		static constexpr std::int32_t value = 0;
	};

	template<>
	struct FullBits<1> 
	{
		static constexpr std::int32_t value = 1;
	};


	template <std::int32_t param_count, std::int32_t column_count> 
	class DBBind
	{
	public:
		DBBind(std::shared_ptr<DBConnection> db_connection, const WCHAR* query)
			: db_connection_(db_connection), query_(query)
		{
			if (db_connection_ == nullptr)
			{
				throw std::invalid_argument("db_connection cannot be null");
			}

			memset(param_index_, 0, sizeof(param_index_));
			memset(column_index_, 0, sizeof(column_index_));

			param_flag_ = 0;
			column_flag_ = 0;

			db_connection->unbind();

		}

		auto validate() -> std::tuple<bool, std::optional<std::string>>
		{
			bool param_valid = param_flag_ == FullBits<param_count>::value;
			bool column_valid = column_flag_ == FullBits<column_count>::value;
			bool is_valid = param_valid && column_valid;
			
			if (!is_valid)
			{
				std::string error_msg = "Binding validation failed: ";
				if (!param_valid)
				{
					error_msg += "parameters not fully bound ";
				}
				if (!column_valid)
				{
					error_msg += "columns not fully bound";
				}
				return { false, error_msg };
			}
			
			return { true, std::nullopt };
		}

		auto execute() -> std::tuple<bool, std::optional<std::string>>
		{
			auto [success, error] = validate();
			if (!success)
			{
				return { false, error };
			}

			return db_connection_->execute(query_);
		}

		auto fetch() -> std::tuple<bool, std::optional<std::string>>
		{
			return db_connection_->fetch();
		}

	protected:
		template<typename T>
		auto bind_param(std::int32_t index, T* value) -> void
		{
			db_connection_->bind_param(index + 1, value, &param_index_[index]);
			param_flag_ |= (1LL << index);
		}

		auto bind_param(std::int32_t index, WCHAR* value) -> void
		{
			db_connection_->bind_param(index + 1, value, &param_index_[index]);
			param_flag_ |= (1LL << index);
		}

		template<typename T, std::int32_t N>
		auto bind_param(std::int32_t index, T(&value)[N]) -> void
		{
			db_connection_->bind_param(index + 1, (const BYTE*)value, sizeof(T) * N, &param_index_[index]);
			param_flag_ |= (1LL << index);
		}

		template<typename T>
		auto bind_param(std::int32_t index, T* value, std::int32_t N) -> void
		{
			db_connection_->bind_param(index + 1, (const BYTE*)value, sizeof(T) * N, &param_index_[index]);
			param_flag_ |= (1LL << index);
		}

		template<std::int32_t N>
		auto bind_column(std::int32_t index, WCHAR(&value)[N]) -> void
		{
			db_connection_->bind_column(index + 1, value, N - 1, &column_index_[index]);
			column_flag_ |= (1LL << index);
		}

		auto bind_column(std::int32_t index, WCHAR* value, std::int32_t len) -> void
		{
			db_connection_->bind_column(index + 1, value, len - 1, &column_index_[index]);
			column_flag_ |= (1LL << index);
		}

		template<typename T>
		auto bind_column(std::int32_t index, T* value) -> void
		{
			db_connection_->bind_column(index + 1, value, &column_index_[index]);
			column_flag_ |= (1LL << index);
		}

		template<typename T, std::int32_t N>
		auto bind_column(std::int32_t index, T(&value)[N]) -> void
		{
			db_connection_->bind_column(index + 1, (BYTE*)value, sizeof(T) * N, &column_index_[index]);
			column_flag_ |= (1LL << index);
		}

	private:
		std::shared_ptr<DBConnection> db_connection_;
		const WCHAR* query_;
		SQLLEN param_index_[param_count > 0 ? param_count : 1];
		SQLLEN column_index_[column_count > 0 ? column_count : 1];
		std::uint64_t param_flag_;
		std::uint64_t column_flag_;


	};
}