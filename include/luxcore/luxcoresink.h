/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _LUXCORE_SINKS_H
#define	_LUXCORE_SINKS_H

#include <mutex>
#include <string>

#include "spdlog/sinks/rotating_file_sink.h"

//------------------------------------------------------------------------------
// Our SpdLog sink for LuxCore call back handler
//------------------------------------------------------------------------------

namespace spdlog {
namespace sinks {
	template<typename Mutex>
	class luxcore_callback_sink final : public base_sink<Mutex> {
	public:
		explicit luxcore_callback_sink(void (*handler)(const char *)) : logHandler(handler) {
		}

	protected:
		void sink_it_(const details::log_msg &msg) override {
			if (logHandler) {
				memory_buf_t formatted;
				base_sink<Mutex>::formatter_->format(msg, formatted);

				logHandler(formatted.c_str());
			}
		}

		void flush_() override {
		}

	private:
		void (*logHandler)(const char *msg);
	};

	using luxcore_callback_sink_mt = luxcore_callback_sink<std::mutex>;
	using luxcore_callback_sink_st = luxcore_callback_sink<details::null_mutex>;

}

template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> luxcore_callback_mt(const std::string &logger_name, void (*handler)(const char *)) {
	return Factory::template create<sinks::luxcore_callback_sink_mt>(logger_name, handler);
}

template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> luxcore_callback_st(const std::string &logger_name, void (*handler)(const char *)) {
	return Factory::template create<sinks::luxcore_callback_sink_st>(logger_name, handler);
}
}

#endif	/* _LUXCORE_SINKS_H */
