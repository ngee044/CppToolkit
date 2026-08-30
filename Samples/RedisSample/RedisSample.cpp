#include <iostream>

#include "Logger.h"
#include "RedisClient.h"
#include "ArgumentParser.h"

#include <format>
#include <expected>

#include <string>

using namespace Utilities;
using namespace Redis;

auto parse_arguments(ArgumentParser& arguments) -> void;
auto run_sample(RedisClient& client) -> int32_t;
auto print_help_message(void) -> void;

#ifdef _DEBUG
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Sequence;
#else
LogTypes write_file_ = LogTypes::None;
LogTypes write_console_ = LogTypes::Information;
#endif

std::string host_ = "127.0.0.1";
int port_ = 6379;
int db_index_ = 0;
std::string key_ = "cpptoolkit:sample:greeting";
std::string value_ = "hello redis";
long ttl_seconds_ = 60;
bool show_help_ = false;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	ArgumentParser arguments(argc, argv);
	parse_arguments(arguments);

	if (show_help_)
	{
		print_help_message();

		return 0;
	}

	// EXPIRE에 0 이하를 주면 만료가 아니라 즉시 삭제이며 set_ttl()은 이를 성공으로 보고한다.
	// 샘플이 키를 조용히 지우고 뒤이은 del()이 0을 반환하는 흐름을 막는다.
	if (ttl_seconds_ <= 0)
	{
		std::cout << "Error: --ttl_seconds must be greater than 0." << std::endl;
		print_help_message();

		return 0;
	}

	Logger::handle().file_mode(write_file_);
	Logger::handle().console_mode(write_console_);
	Logger::handle().log_root(arguments.program_folder());

	Logger::handle().start("RedisSample");

	RedisClient client(host_, port_, TLSOptions(), db_index_);

	int32_t exit_code = run_sample(client);

	Logger::handle().stop();
	Logger::destroy();

	return exit_code;
}

auto run_sample(RedisClient& client) -> int32_t
{
	// 1. 연결. connect()는 redis++ 객체를 만들 뿐 소켓을 열거나 PING을 보내지 않는다.
	//    여기서 성공해도 서버 도달성은 아직 알 수 없고, 잘못된 host/port는 아래 set()에서 처음 드러난다.
	auto connect_result = client.connect();
	if (!connect_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to connect: {}", connect_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("connector ready for {}:{} (db {})", host_, port_, db_index_));

	// 2. 쓰기. ttl_seconds가 0보다 크면 SET과 EXPIRE를 하나의 트랜잭션으로 실행한다.
	auto set_result = client.set(key_, value_, ttl_seconds_);
	if (!set_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to set '{}': {}", key_, set_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("set '{}' = '{}' (ttl {}s)", key_, value_, ttl_seconds_));

	// 3. 읽기.
	auto get_result = client.get(key_);
	if (!get_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to get '{}': {}", key_, get_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("get '{}' = '{}'", key_, get_result.value()));

	// 4. TTL 갱신. EXPIRE는 키가 없으면 실패로 보고되므로 set() 이후에만 호출한다.
	auto ttl_result = client.set_ttl(key_, ttl_seconds_ * 2);
	if (!ttl_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to set ttl on '{}': {}", key_, ttl_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("set_ttl '{}' -> {}s", key_, ttl_seconds_ * 2));

	// 5. 삭제. 반환값은 삭제된 키 개수이며, 없는 키를 지우면 오류가 아니라 성공값 0이다.
	auto del_result = client.del(key_);
	if (!del_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to delete '{}': {}", key_, del_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("del '{}' removed {} key(s)", key_, del_result.value()));

	// 6. 삭제 확인. get()은 키가 없을 때 빈 값이나 nullopt가 아니라 unexpected를 돌려준다.
	//    따라서 여기서의 실패는 기대한 결과이며, 오류로 취급하면 안 된다.
	auto verify_result = client.get(key_);
	if (verify_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("'{}' still holds '{}' after del", key_, verify_result.value()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, std::format("removal confirmed - get() reports: {}", verify_result.error()));

	// 7. 종료. RedisConnector 소멸자도 연결을 닫지만, 종료 시점을 코드로 드러내기 위해 명시적으로 호출한다.
	auto disconnect_result = client.disconnect();
	if (!disconnect_result)
	{
		Logger::handle().write(LogTypes::Error, std::format("failed to disconnect: {}", disconnect_result.error()));

		return 1;
	}

	Logger::handle().write(LogTypes::Information, "disconnected");

	return 0;
}

auto parse_arguments(ArgumentParser& arguments) -> void
{
	// ArgumentParser는 "--help"에 "display help"를 넣으므로 to_bool로는 판정할 수 없다.
	show_help_ = (arguments.to_string("--help") != std::nullopt);

	auto int_target = arguments.to_int("--write_console_log");
	if (int_target != std::nullopt)
	{
		write_console_ = (LogTypes)int_target.value();
	}

	int_target = arguments.to_int("--write_file_log");
	if (int_target != std::nullopt)
	{
		write_file_ = (LogTypes)int_target.value();
	}

	int_target = arguments.to_int("--port");
	if (int_target != std::nullopt)
	{
		port_ = int_target.value();
	}

	int_target = arguments.to_int("--db_index");
	if (int_target != std::nullopt)
	{
		db_index_ = int_target.value();
	}

	auto long_target = arguments.to_long("--ttl_seconds");
	if (long_target != std::nullopt)
	{
		ttl_seconds_ = static_cast<long>(long_target.value());
	}

	auto string_target = arguments.to_string("--host");
	if (string_target != std::nullopt)
	{
		host_ = string_target.value();
	}

	string_target = arguments.to_string("--key");
	if (string_target != std::nullopt)
	{
		key_ = string_target.value();
	}

	string_target = arguments.to_string("--value");
	if (string_target != std::nullopt)
	{
		value_ = string_target.value();
	}
}

auto print_help_message(void) -> void
{
	std::cout << "Usage: RedisSample [OPTIONS]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  --help                      Show this help message and exit" << std::endl;
	std::cout << "  --host <address>            Redis host (default: 127.0.0.1)" << std::endl;
	std::cout << "  --port <port>               Redis port (default: 6379)" << std::endl;
	std::cout << "  --db_index <index>          Redis database index (default: 0)" << std::endl;
	std::cout << "  --key <key>                 Key to write and read (default: cpptoolkit:sample:greeting)" << std::endl;
	std::cout << "  --value <value>             Value to store (default: hello redis)" << std::endl;
	std::cout << "  --ttl_seconds <seconds>     TTL applied on set (default: 60)" << std::endl;
	std::cout << "  --write_console_log <level> Console log level (default: 4, or 6 when _DEBUG is defined)" << std::endl;
	std::cout << "  --write_file_log <level>    File log level (default: 0)" << std::endl;
	std::cout << std::endl;
	std::cout << "Log levels: 0 None, 1 Exception, 2 Error, 3 Warning, 4 Information, 5 Debug, 6 Sequence, 7 Parameter, 8 Packet" << std::endl;
	std::cout << std::endl;
	std::cout << "Flow: connect -> set(ttl) -> get -> set_ttl -> del -> get(expected to fail) -> disconnect" << std::endl;
	std::cout << std::endl;
	std::cout << "Notes:" << std::endl;
	std::cout << "  - connect() only builds the redis++ handle. An unreachable host surfaces on the first command (set)." << std::endl;
	std::cout << "  - get() returns unexpected when the key is absent, not an empty value." << std::endl;
	std::cout << "  - set_ttl() fails when the key does not exist, so it must follow a successful set()." << std::endl;
	std::cout << "  - --ttl_seconds must be > 0. EXPIRE with a non-positive timeout deletes the key instead of expiring it." << std::endl;
	std::cout << "  - This sample connects in plaintext. Enable TLS with TLSOptions().use_tls(true).ca_cert(...).verify_peer(true)." << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "1. Run against a local Redis:" << std::endl;
	std::cout << "   RedisSample" << std::endl;
	std::cout << std::endl;
	std::cout << "2. Run against a remote Redis with a custom key:" << std::endl;
	std::cout << "   RedisSample --host 10.0.0.5 --port 6379 --key my:key --value my-value --ttl_seconds 300" << std::endl;
	std::cout << std::endl;
	std::cout << "3. Trace every step:" << std::endl;
	std::cout << "   RedisSample --write_console_log 6" << std::endl;
}
