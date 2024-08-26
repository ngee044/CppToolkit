#include "FileSendingJob.h"

#include "File.h"
#include "Logger.h"
#include "Combiner.h"
#include "Converter.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

using namespace Thread;
using namespace Utilities;

namespace Network
{
	FileSendingJob::FileSendingJob(const std::vector<uint8_t>& file_information,
								   const std::function<std::tuple<bool, std::optional<std::string>>(
									   const DataModes&, const std::vector<uint8_t>&)>& callback)
		: Job(JobPriorities::Low, file_information, "FileSendingJob"), sending_callback_(callback)
	{
	}

	FileSendingJob::~FileSendingJob(void) {}

	auto FileSendingJob::working(void) -> std::tuple<bool, std::optional<std::string>>
	{
		if (sending_callback_ == nullptr)
		{
			return { false, "cannot complete FileSendingJob with null callback" };
		}

		auto file_information = get_data();
		if (file_information.empty())
		{
			return { false, "cannot complete FileSendingJob with null data" };
		}

		size_t index = 0;
		auto guid = Combiner::divide(file_information, index);
		auto file_index = Combiner::divide(file_information, index);
		auto file_path = Converter::to_string(Combiner::divide(file_information, index));
		auto file_message = Combiner::divide(file_information, index);

		File source;
		source.open(file_path, std::ios::in | std::ios::binary, std::locale(""));
		const auto [source_data, message] = source.read_bytes();
		source.close();

		if (source_data == std::nullopt)
		{
			std::vector<uint8_t> data;
			Combiner::append(data, guid);
			Combiner::append(data, { (uint8_t)FileModes::Failure });
			Combiner::append(data, file_index);
			Combiner::append(data, file_message);

			return sending_callback_(DataModes::File, data);
		}

		std::vector<uint8_t> data;
		Combiner::append(data, guid);
		Combiner::append(data, { (uint8_t)FileModes::Success });
		Combiner::append(data, file_index);
		Combiner::append(data, file_message);
		Combiner::append(data, source_data.value());

		return sending_callback_(DataModes::File, data);
	}
}