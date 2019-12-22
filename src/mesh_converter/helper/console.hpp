#pragma once

#include <plog/Appenders/ColorConsoleAppender.h>

#include <indicators/color.hpp>

#include <functional>


namespace mirrage::helper {

	template <class Formatter>
	class Mirrage_console_appender : public plog::ConsoleAppender<Formatter> {
	  public:
#ifdef _WIN32
		Mirrage_console_appender(bool color) : _color(color), m_originalAttr()
		{
			if(this->m_isatty) {
				CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
				GetConsoleScreenBufferInfo(this->m_stdoutHandle, &csbiInfo);

				m_originalAttr = csbiInfo.wAttributes;
			}
		}
#else
		Mirrage_console_appender(bool color) : _color(color) {}
#endif

		void on_log(std::function<void()> on_message) { _on_message = std::move(on_message); }

		auto mutex() -> plog::util::Mutex& { return plog::ConsoleAppender<Formatter>::m_mutex; }

		void write(const plog::Record& record) override
		{
			plog::util::nstring   str = Formatter::format(record);
			plog::util::MutexLock lock(this->m_mutex);

			if(_on_message)
				_on_message();

			if(_color) {
				std::cout << "\r                                                                             "
				             "    "
				             "                                                 \r"
				          << termcolor::reset;
				setColor(record.getSeverity());
			}
			this->writestr(str);
			if(_color) {
				resetColor();
			}
		}

	  private:
		std::function<void()> _on_message;
		bool                  _color = true;

	  private:
		void setColor(plog::Severity severity)
		{
			if(this->m_isatty) {
				switch(severity) {
#ifdef _WIN32
					case fatal:
						SetConsoleTextAttribute(this->m_stdoutHandle,
						                        foreground::kRed | foreground::kGreen | foreground::kBlue
						                                | foreground::kIntensity
						                                | background::kRed); // white on red background
						break;

					case error:
						SetConsoleTextAttribute(this->m_stdoutHandle,
						                        static_cast<WORD>(foreground::kRed | foreground::kIntensity
						                                          | (m_originalAttr & 0xf0))); // red
						break;

					case warning:
						SetConsoleTextAttribute(this->m_stdoutHandle,
						                        static_cast<WORD>(foreground::kRed | foreground::kGreen
						                                          | foreground::kIntensity
						                                          | (m_originalAttr & 0xf0))); // yellow
						break;

					case debug:
					case verbose:
						SetConsoleTextAttribute(this->m_stdoutHandle,
						                        static_cast<WORD>(foreground::kGreen | foreground::kBlue
						                                          | foreground::kIntensity
						                                          | (m_originalAttr & 0xf0))); // cyan
						break;
#else
					case plog::fatal:
						std::cout << "\x1B[97m\x1B[41m"; // white on red background
						break;

					case plog::error:
						std::cout << "\x1B[91m"; // red
						break;

					case plog::warning:
						std::cout << "\x1B[93m"; // yellow
						break;

					case plog::debug:
					case plog::verbose:
						std::cout << "\x1B[96m"; // cyan
						break;
#endif
					default: break;
				}
			}
		}

		void resetColor()
		{
			if(this->m_isatty) {
#ifdef _WIN32
				SetConsoleTextAttribute(this->m_stdoutHandle, m_originalAttr);
#else
				std::cout << "\x1B[0m\x1B[0K";
#endif
			}
		}

	  private:
#ifdef _WIN32
		WORD m_originalAttr;
#endif
	};

} // namespace mirrage::helper
