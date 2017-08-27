#include <mirrage/asset/stream.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <physfs.h>

#include <streambuf>
#include <cstring>
#include <cstdio>

namespace mirrage {
namespace asset {

	Loading_failed::~Loading_failed() = default;


	class stream::fbuf : public std::streambuf {
		fbuf(const fbuf & other) = delete;
		fbuf& operator=(const fbuf& other) = delete;

		int_type underflow() {
			if (PHYSFS_eof(file)) {
				return traits_type::eof();
			}
			size_t bytesRead = PHYSFS_read(file, buffer, 1, bufferSize);
			if (bytesRead < 1) {
				return traits_type::eof();
			}
			setg(buffer, buffer, buffer + bytesRead);
			return (unsigned char) *gptr();
		}

		pos_type seekoff(std::streamoff pos, std::ios_base::seekdir dir, std::ios_base::openmode mode) {
			switch (dir) {
				case std::ios_base::beg:
					PHYSFS_seek(file, pos);
					break;
				case std::ios_base::cur:
					// subtract characters currently in buffer from seek position
					PHYSFS_seek(file, (PHYSFS_tell(file) + pos) - (egptr() - gptr()));
					break;
					//case std::_S_ios_seekdir_end:
				case std::ios_base::end:
				default:
					PHYSFS_seek(file, PHYSFS_fileLength(file) + pos);
					break;
			}
			if (mode & std::ios_base::in) {
				setg(egptr(), egptr(), egptr());
			}
			if (mode & std::ios_base::out) {
				setp(buffer, buffer);
			}
			return PHYSFS_tell(file);
		}

		pos_type seekpos(pos_type pos, std::ios_base::openmode mode) {
			PHYSFS_seek(file, pos);
			if (mode & std::ios_base::in) {
				setg(egptr(), egptr(), egptr());
			}
			if (mode & std::ios_base::out) {
				setp(buffer, buffer);
			}
			return PHYSFS_tell(file);
		}

		int_type overflow( int_type c = traits_type::eof() ) {
			if (pptr() == pbase() && c == traits_type::eof()) {
				return 0; // no-op
			}

			auto res = PHYSFS_write(file, pbase(), pptr() - pbase(), 1);
			if (res<1) {
				return traits_type::eof();
			}
			if (c != traits_type::eof()) {
				res = PHYSFS_write(file, &c, 1, 1);
				if (res < 1) {
					return traits_type::eof();
				}
			}
			setp(buffer, static_cast<size_t>(res)==bufferSize ? buffer+bufferSize : buffer+res);

			return 0;
		}

		int sync() {
			return overflow();
		}

		char * buffer;
		size_t const bufferSize;
	protected:
		PHYSFS_File * const file;
	public:
		fbuf(File_handle* file, std::size_t bufferSize = 1024L*256)
				: bufferSize(bufferSize), file((PHYSFS_File*)file) {
			buffer = new char[bufferSize];
			char * end = buffer + bufferSize;
			setg(end, end, end);
			setp(buffer, end);
		}

		~fbuf() {
			sync();
			delete [] buffer;
		}
	};

	struct File_handle{};

	stream::stream(AID aid, Asset_manager& manager, File_handle* file, const std::string& path) : _file(file), _aid(aid), _manager(manager) {
		INVARIANT(file, "Error opening file \""<<path<<"\": "<< PHYSFS_getLastError());

		_fbuf.reset(new fbuf(file));
	}

	stream::stream(stream&& o) : _file(o._file), _aid(std::move(o._aid)), _manager(o._manager), _fbuf(std::move(o._fbuf)) {
		o._file = nullptr;
	}

	stream::~stream()noexcept {
		_fbuf.reset();

		if(_file)
			PHYSFS_close((PHYSFS_File*)_file);
	}

	stream& stream::operator=(stream&& rhs)noexcept {
		INVARIANT(&_manager==&rhs._manager, "cross-manager move");
		_file = std::move(rhs._file);
		_aid = std::move(rhs._aid);
		_fbuf = std::move(rhs._fbuf);
		return *this;
	}

	void stream::close() {
		_fbuf.reset();

		if(_file)
			PHYSFS_close((PHYSFS_File*)_file);
	}

	auto stream::physical_location()const noexcept -> util::maybe<std::string> {
		return _manager.physical_location(_aid);
	}

	size_t stream::length()const noexcept {
		return PHYSFS_fileLength((PHYSFS_File*)_file);
	}

	istream::istream(AID aid, Asset_manager& manager, const std::string& path)
	  : stream(aid, manager, (File_handle*)PHYSFS_openRead(path.c_str()), path), std::istream(_fbuf.get()) {
	}
	istream::istream(istream&& o)
	  : stream(std::move(o)), std::istream(_fbuf.get()) {
	}
	auto istream::operator=(istream&& s) -> istream& {
		stream::operator=(std::move(s));
		init(_fbuf.get());
		return *this;
	}

	std::vector<std::string> istream::lines() {
		std::vector<std::string> lines;

		std::string str;
		while (std::getline(*this, str)) {
			util::replace_inplace(str, "\r", "");
			lines.emplace_back(std::move(str));
		}

		return lines;
	}
	std::string istream::content() {
		std::string content;
		content.reserve(length());
		content.assign(std::istreambuf_iterator<char>{*this}, std::istreambuf_iterator<char>{});

		util::replace_inplace(content, "\r", "");

		return content;
	}
	std::vector<char> istream::bytes() {
		std::vector<char> res(length(), 0);
		read((char*)res.data(), res.size());

		return res;
	}
	void istream::read_direct(char* target, std::size_t size) {
		seekg(0, cur);
		PHYSFS_read((PHYSFS_File*)_file, target, 1, size);
	}


	ostream::ostream(AID aid, Asset_manager& manager, const std::string& path)
	  : stream(aid, manager, (File_handle*)PHYSFS_openWrite(path.c_str()), path), std::ostream(_fbuf.get()) {
	}
	ostream::ostream(ostream&& o)
	  : stream(std::move(o)), std::ostream(_fbuf.get()) {
	}
	ostream::~ostream() {
//		if(good())
//			flush();

		_manager._post_write();
	}

	auto ostream::operator=(ostream&& s) -> ostream& {
		stream::operator=(std::move(s));
		init(_fbuf.get());
		return *this;
	}

}
}
