#include "FolderWatcher.h"

namespace Utilities
{

	FolderWatcher::FolderWatcher(void) : file_watcher_(nullptr), callback_(nullptr)
	{
	}

	FolderWatcher::~FolderWatcher(void) { stop(); }

	auto FolderWatcher::set_callback(const std::function<void(const std::string&, const std::string&, efsw::Action, const std::string&)>& callback) -> void
	{
		callback_ = callback;
	}

	auto FolderWatcher::start(const std::vector<std::pair<std::string, bool>>& targets) -> void 
	{
		stop();

		file_watcher_ = std::make_unique<efsw::FileWatcher>(false);

		for (auto& [target_folder, target_recursive] : targets)
		{
			watch_ids_.push_back(file_watcher_->addWatch(target_folder, this, target_recursive));
		}

		file_watcher_->watch(); 
	}
	auto FolderWatcher::stop(void) -> void 
	{
		if (file_watcher_ == nullptr)
		{
			return;
		}

		for (auto& watch_id : watch_ids_)
		{
			file_watcher_->removeWatch(watch_id);
		}

		file_watcher_.reset();
	}

	auto FolderWatcher::handleFileAction(efsw::WatchID watch_id, const std::string& dir, const std::string& filename, efsw::Action action, std::string old_filename)
		-> void
	{
		if (callback_ == nullptr)
		{
			return;
		}
		
		callback_(dir, filename, action, old_filename);
	}

#pragma region Handle
	std::unique_ptr<FolderWatcher> FolderWatcher::handle_;
	std::once_flag FolderWatcher::once_;

	FolderWatcher& FolderWatcher::handle(void)
	{
		std::call_once(once_, []() { handle_.reset(new FolderWatcher()); });

		return *handle_.get();
	}

	void FolderWatcher::destroy(void) { handle_.reset(); }
#pragma endregion
}
