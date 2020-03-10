#include <mirrage/entry_point.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/gui/debug_ui.hpp>

#include <doctest.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>


namespace mirrage::detail {

	auto init_logging(int                             argc,
	                  char**                          argv,
	                  char**                          env,
	                  std::uint32_t                   version_major,
	                  std::uint32_t                   version_minor,
	                  const std::string&              org_name,
	                  const std::string&              app_name,
	                  const util::maybe<std::string>& base_dir) -> bool
	{
		doctest::Context context;
		context.setOption("no-run", true);
		context.applyCommandLine(argc, argv);
		context.run();

		if(context.shouldExit())
			return false;

		const auto write_dir = asset::write_dir(argv[0], org_name, app_name, base_dir);

		static auto fileAppender = plog::RollingFileAppender<plog::TxtFormatter>(
		        (write_dir + "/mirrage.log").c_str(), 1024L * 1024L, 4);
		static auto consoleAppender = plog::ColorConsoleAppender<plog::TxtFormatter>();
		plog::init(plog::debug, &fileAppender)
		        .addAppender(&consoleAppender)
		        .addAppender(&gui::debug_console_appender());

		LOG(plog::debug) << "\n"
		                 << app_name << " by " << org_name << " V" << version_major << "." << version_minor
		                 << "\n"
		                 << "Started from: " << argv[0] << "\n"
		                 << "Base dir: " << base_dir.get_ref_or("<NONE>") << "\n"
		                 << "Working dir: " << asset::pwd() << "\n"
		                 << "Write dir: " << write_dir << "\n"
		                 << "Engine Version: " << version_info::name << "\n"
		                 << "Engine Version-Hash: " << version_info::hash << "\n"
		                 << "Engine Version-Date: " << version_info::date << "\n"
		                 << "Engine Version-Subject: " << version_info::subject << "\n";

		return true;
	}

} // namespace mirrage::detail
