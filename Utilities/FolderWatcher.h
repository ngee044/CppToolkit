#pragma once

#include <mutex>
#include <vector>
#include <atomic>
#include <memory>
#include <string>
#include <functional>

#include <efsw/efsw.hpp>

namespace Utilities
{

	class FolderWatcher : public efsw::FileWatchListener
	{
	private:
		FolderWatcher(void);

	public:
		~FolderWatcher(void);

		auto set_callback(const std::function<void(const std::string&, const std::string&, efsw::Action, const std::string&)>& callback) -> void;
		auto start(const std::vector<std::pair<std::string, bool>>& targets) -> void;
		auto stop(void) -> void;

	protected:
		auto handleFileAction(efsw::WatchID watch_id, const std::string& dir, const std::string& filename, efsw::Action action, std::string old_filename = "")
			-> void override;

	private:
		std::unique_ptr<efsw::FileWatcher> file_watcher_;
		std::vector<efsw::WatchID> watch_ids_;
		std::function<void(const std::string&, const std::string&, efsw::Action, const std::string&)> callback_;


#pragma region Handle
	public:
		static auto handle(void) -> FolderWatcher&;
		static auto destroy() -> void;

	private:
		static std::unique_ptr<FolderWatcher> handle_;
		static std::once_flag once_;
#pragma endregion
	};
}
