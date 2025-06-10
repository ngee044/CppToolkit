#pragma once

#include "DBConnection.h"


namespace GameDataBase
{
class DBConnectionPool
{
public:
	DBConnectionPool();
	~DBConnectionPool();

	auto connect(std::int32_t connection_count, const std::wstring& connection_string) -> std::tuple<bool, std::optional<std::string>>;
	auto clear() -> void;

	auto pop() -> std::shared_ptr<DBConnection>;
	auto push(std::shared_ptr<DBConnection> connection) -> void;

private:
	std::mutex mutex_;

	SQLHENV environment_;
	std::vector<std::shared_ptr<DBConnection>> connections_;
};
}