#include "DBTransaction.h"
// // #include <format>

namespace GameDatabase
{
	DBTransaction::DBTransaction(std::shared_ptr<DBConnection> connection, IsolationLevel isolation_level)
		: connection_(connection)
		, is_active_(false)
		, is_committed_(false)
		, isolation_level_(isolation_level)
	{
	}

	DBTransaction::~DBTransaction()
	{
		if (is_active_ && !is_committed_)
		{
			rollback();
		}
	}

	auto DBTransaction::begin() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (is_active_)
		{
			return { false, "Transaction is already active" };
		}

		if (!connection_)
		{
			return { false, "Database connection is null" };
		}

		auto [isolation_success, isolation_error] = set_isolation_level(isolation_level_);
		if (!isolation_success)
		{
			return { false, isolation_error };
		}

		auto [success, error] = connection_->execute(L"BEGIN TRANSACTION");
		if (success)
		{
			is_active_ = true;
			is_committed_ = false;
		}

		return { success, error };
	}

	auto DBTransaction::commit() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (!is_active_)
		{
			return { false, "No active transaction to commit" };
		}

		if (is_committed_)
		{
			return { false, "Transaction already committed" };
		}

		auto [success, error] = connection_->execute(L"COMMIT TRANSACTION");
		if (success)
		{
			is_active_ = false;
			is_committed_ = true;
		}

		return { success, error };
	}

	auto DBTransaction::rollback() -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (!is_active_)
		{
			return { false, "No active transaction to rollback" };
		}

		auto [success, error] = connection_->execute(L"ROLLBACK TRANSACTION");
		if (success)
		{
			is_active_ = false;
			is_committed_ = false;
		}

		return { success, error };
	}

	auto DBTransaction::set_isolation_level(IsolationLevel level) -> std::tuple<bool, std::optional<std::string>>
	{
		if (!connection_)
		{
			return { false, "Database connection is null" };
		}

		std::wstring isolation_query;
		switch (level)
		{
			case IsolationLevel::READ_UNCOMMITTED:
				isolation_query = L"SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
				break;
			case IsolationLevel::READ_COMMITTED:
				isolation_query = L"SET TRANSACTION ISOLATION LEVEL READ COMMITTED";
				break;
			case IsolationLevel::REPEATABLE_READ:
				isolation_query = L"SET TRANSACTION ISOLATION LEVEL REPEATABLE READ";
				break;
			case IsolationLevel::SERIALIZABLE:
				isolation_query = L"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE";
				break;
			default:
				return { false, "Invalid isolation level" };
		}

		auto [success, error] = connection_->execute(isolation_query);
		if (success)
		{
			isolation_level_ = level;
		}

		return { success, error };
	}

	auto DBTransaction::create_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (!is_active_)
		{
			return { false, "No active transaction for savepoint" };
		}

		if (savepoint_name.empty())
		{
			return { false, "Savepoint name cannot be empty" };
		}

		std::wstring query = L"SAVE TRANSACTION " + savepoint_name;
		return connection_->execute(query);
	}

	auto DBTransaction::rollback_to_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (!is_active_)
		{
			return { false, "No active transaction for savepoint rollback" };
		}

		if (savepoint_name.empty())
		{
			return { false, "Savepoint name cannot be empty" };
		}

		std::wstring query = L"ROLLBACK TRANSACTION " + savepoint_name;
		return connection_->execute(query);
	}

	auto DBTransaction::release_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>
	{
		std::lock_guard<std::mutex> lock(transaction_mutex_);

		if (!is_active_)
		{
			return { false, "No active transaction for savepoint release" };
		}

		return { true, std::nullopt };
	}

	TransactionGuard::TransactionGuard(std::shared_ptr<DBTransaction> transaction)
		: transaction_(transaction)
		, should_rollback_(true)
	{
		if (transaction_)
		{
			transaction_->begin();
		}
	}

	TransactionGuard::~TransactionGuard()
	{
		if (transaction_ && should_rollback_ && transaction_->is_active())
		{
			transaction_->rollback();
		}
	}

	auto TransactionGuard::commit() -> std::tuple<bool, std::optional<std::string>>
	{
		if (!transaction_)
		{
			return { false, "Transaction is null" };
		}

		auto result = transaction_->commit();
		if (std::get<0>(result))
		{
			should_rollback_ = false;
		}

		return result;
	}

	auto TransactionGuard::release() -> void
	{
		should_rollback_ = false;
	}
}
