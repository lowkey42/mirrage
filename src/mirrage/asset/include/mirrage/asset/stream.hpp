/** iostreams for assets *****************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/aid.hpp>

#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/stacktrace.hpp>
#include <mirrage/utils/maybe.hpp>

#include <memory>
#include <iostream>
#include <string>
#include <vector>


namespace lux {
namespace asset {
	struct File_handle;
	class Asset_manager;

	struct Loading_failed : public util::Error {
		explicit Loading_failed(const std::string& msg)noexcept : util::Error(msg) {}
		virtual ~Loading_failed();
	};

	class stream {
		public:
			stream(AID aid, Asset_manager& manager, File_handle* file, const std::string& path);
			stream(stream&&);
			stream(const stream&)=delete;
			~stream()noexcept;

			stream& operator=(const stream&) = delete;
			stream& operator=(stream&&)noexcept;

			auto length()const noexcept -> size_t;

			auto aid()const noexcept {return _aid;}
			auto& manager()noexcept {return _manager;}

			auto physical_location()const noexcept -> util::maybe<std::string>;

			void close();

		protected:
			File_handle* _file;
			AID _aid;
			Asset_manager& _manager;

			class fbuf;
			std::unique_ptr<fbuf> _fbuf;
	};
	class istream : public stream, public std::istream {
		public:
			istream(AID aid, Asset_manager& manager, const std::string& path);
			istream(istream&&);

			auto operator=(istream&&) -> istream&;

			auto lines() -> std::vector<std::string>;
			auto content() -> std::string;
			auto bytes() -> std::vector<char>;
	};
	class ostream : public stream, public std::ostream {
		public:
			ostream(AID aid, Asset_manager& manager, const std::string& path);
			ostream(ostream&&);
			~ostream();

			auto operator=(ostream&&) -> ostream&;
	};
}
}

#ifdef ENABLE_SF2_ASSETS
#include <sf2/sf2.hpp>

namespace lux {
namespace asset {
	/**
	 * Specialize this template for each asset-type
	 * Instances should be lightweight
	 * Implementations should NEVER return nullptr
	 */
	template<class T>
	struct Loader {
		static_assert(sf2::is_annotated_struct<T>::value, "Required AssetLoader specialization not provided.");

		static auto load(istream in) -> std::shared_ptr<T> {
			auto r = std::make_shared<T>();

			sf2::deserialize_json(in, [&](auto& msg, uint32_t row, uint32_t column) {
				ERROR("Error parsing JSON from "<<in.aid().str()<<" at "<<row<<":"<<column<<": "<<msg);
			}, *r);

			return r;
		}
		static void store(ostream out, const T& asset) {
			sf2::serialize_json(out,asset);
		}
	};

	template<class T>
	struct Interceptor {
		static auto on_intercept(Asset_manager&, const AID& interceptor_aid,
		                         const AID& org_aid) -> std::shared_ptr<T> {
			FAIL("Required Interceptor specialization not found loading '"<<org_aid.str()<<"' via '"<<interceptor_aid.str()<<"'");
		}
	};

}
}
#else

namespace lux {
namespace asset {
	/**
	 * Specialize this template for each asset-type
	 * Instances should be lightweight
	 * Implementations should NEVER return nullptr
	 */
	template<class T>
	struct Loader {
		static_assert(util::dependent_false<T>(), "Required AssetLoader specialization not provided.");

		static auto load(istream in) throw(Loading_failed) -> std::shared_ptr<T>;
		static void store(ostream out, const T& asset) throw(Loading_failed);
	};

}
}
#endif
