/** iostreams for assets *****************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/aid.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/stacktrace.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <gsl/gsl>

#include <iostream>
#include <memory>
#include <string>
#include <vector>


namespace mirrage::asset {

	struct File_handle;
	class Asset_manager;


	class stream {
	  public:
		stream(AID aid, Asset_manager& manager, File_handle* file, const std::string& path);
		stream(stream&&);
		stream(const stream&) = delete;
		~stream() noexcept;

		stream& operator=(const stream&) = delete;
		stream& operator                 =(stream&&) noexcept;

		auto length() const noexcept -> size_t;

		auto  aid() const noexcept { return _aid; }
		auto& manager() noexcept { return _manager; }

		void close();

	  protected:
		File_handle*   _file;
		AID            _aid;
		Asset_manager& _manager;

		class fbuf;
		std::unique_ptr<fbuf> _fbuf;
	};
	class istream : public stream, public std::istream {
	  public:
		istream(AID aid, Asset_manager& manager, const std::string& path);
		istream(istream&&);

		auto operator=(istream &&) -> istream&;

		auto lines() -> std::vector<std::string>;
		auto content() -> std::string;
		auto bytes() -> std::vector<char>;

		// low-level direct read operation (without intermediate buffer). Disturbs normal stream operation
		// No further reads allowed after this operation!
		void read_direct(char* target, std::size_t size);
	};
	class ostream : public stream, public std::ostream {
	  public:
		ostream(AID aid, Asset_manager& manager, const std::string& path);
		ostream(ostream&&);
		~ostream();

		auto operator=(ostream &&) -> ostream&;
	};

} // namespace mirrage::asset

#ifdef ENABLE_SF2_ASSETS
#include <sf2/sf2.hpp>

namespace mirrage::asset {
	/**
	 * Specialize this template for each asset-type
	 * Instances should be lightweight
	 * Implementations should NEVER return nullptr
	 */
	template <class T>
	struct Loader {
		static_assert(sf2::is_loadable<T, sf2::format::Json_reader>::value,
		              "Required AssetLoader specialization not provided.");

		static auto load(istream in) -> T {
			auto r = T();

			sf2::deserialize_json(in,
			                      [&](auto& msg, uint32_t row, uint32_t column) {
				                      MIRRAGE_ERROR("Error parsing JSON from " << in.aid().str() << " at "
				                                                               << row << ":" << column << ": "
				                                                               << msg);
			                      },
			                      r);

			return r;
		}
		static void save(ostream out, const T& asset) { sf2::serialize_json(out, asset); }
	};

} // namespace mirrage::asset
#else

namespace mirrage::asset {
	/**
	 * Specialize this template for each asset-type
	 * Instances should be lightweight
	 * Implementations should NEVER return nullptr
	 */
	template <class T>
	struct Loader {
		static_assert(util::dependent_false<T>(), "Required AssetLoader specialization not provided.");

		static auto load(istream in) -> T;
		static void save(ostream out, const T& asset);
	};
} // namespace mirrage::asset
#endif

namespace mirrage::asset {

	using Bytes = std::vector<char>;

	template <>
	struct Loader<Bytes> {
		static auto load(istream in) -> Bytes { return in.bytes(); }
		void        save(ostream out, const Bytes& data) {
            out.write(data.data(), gsl::narrow<std::streamsize>(data.size()));
		}
	};

} // namespace mirrage::asset
