#pragma once

#include "DBConnection.h"
#include <memory>

namespace GameDataBase
{
	enum class IsolationLevel
	{
		READ_UNCOMMITTED = 1,  // SQL_TXN_READ_UNCOMMITTED
		READ_COMMITTED = 2,    // SQL_TXN_READ_COMMITTED
		REPEATABLE_READ = 4,   // SQL_TXN_REPEATABLE_READ
		SERIALIZABLE = 8       // SQL_TXN_SERIALIZABLE
	};

	class DBTransaction
	{
	public:
		DBTransaction(std::shared_ptr<DBConnection> connection, IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED);
		~DBTransaction();

		// 복사 및 이동 금지 (트랜잭션은 단일 소유권을 가져야 함)
		DBTransaction(const DBTransaction&) = delete;
		DBTransaction& operator=(const DBTransaction&) = delete;
		DBTransaction(DBTransaction&&) = delete;
		DBTransaction& operator=(DBTransaction&&) = delete;

		// 트랜잭션 제어
		auto begin() -> std::tuple<bool, std::optional<std::string>>;
		auto commit() -> std::tuple<bool, std::optional<std::string>>;
		auto rollback() -> std::tuple<bool, std::optional<std::string>>;

		// 트랜잭션 상태 확인
		auto is_active() const -> bool { return is_active_; }
		auto is_committed() const -> bool { return is_committed_; }

		// 격리 수준 설정
		auto set_isolation_level(IsolationLevel level) -> std::tuple<bool, std::optional<std::string>>;
		auto get_isolation_level() const -> IsolationLevel { return isolation_level_; }

		// Savepoint 지원 (중첩 트랜잭션)
		auto create_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>;
		auto rollback_to_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>;
		auto release_savepoint(const std::wstring& savepoint_name) -> std::tuple<bool, std::optional<std::string>>;

	private:
		std::shared_ptr<DBConnection> connection_;
		bool is_active_;
		bool is_committed_;
		IsolationLevel isolation_level_;
		std::mutex transaction_mutex_;
	};

	// RAII 스타일 트랜잭션 가드
	class TransactionGuard
	{
	public:
		TransactionGuard(std::shared_ptr<DBTransaction> transaction);
		~TransactionGuard();

		// 명시적 커밋
		auto commit() -> std::tuple<bool, std::optional<std::string>>;

		// 트랜잭션 포기 (자동 롤백 방지)
		auto release() -> void;

	private:
		std::shared_ptr<DBTransaction> transaction_;
		bool should_rollback_;
	};
}