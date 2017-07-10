/** helper-functions for strings *********************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/maybe.hpp>

#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

namespace lux {
namespace util {

	inline maybe<std::string> read_file_to_string(std::string path) {
		std::ifstream stream(path, std::ios::in);
		if(!stream.is_open()) {
			std::cerr<<"Unable to load from '"<<path<<"'."<<std::endl;
			return nothing;
		}

		return just(std::string(
					std::istreambuf_iterator<char>{stream},
					std::istreambuf_iterator<char>{} ));
	}

	inline std::string& replace_inplace(std::string& subject, const std::string& search,
	                          const std::string& replace) {
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::string::npos) {
			 subject.replace(pos, search.length(), replace);
			 pos += replace.length();
		}

		return subject;
	}

	inline std::string replace(std::string subject, const std::string& search,
	                          const std::string& replace) {
		replace_inplace(subject, search, replace);
		return subject;
	}

	template <class T>
	inline std::string to_string(const T& t)
	{
		std::stringstream ss;
		ss << t;
		return ss.str();
	}

	// trim from start
	inline std::string &ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		return s;
	}

	// trim from end
	inline std::string &rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
		return s;
	}

	// trim from both ends
	inline std::string& trim(std::string& s) {
		return ltrim(rtrim(s));
	}
	inline std::string trim_copy(std::string s) {
		return trim(s);
	}

	inline std::pair<std::string, std::string> split(const std::string& line, const std::string& delim) {
		auto delIter = line.find(delim);

		if(delIter!=std::string::npos)
			return std::make_pair(
						trim_copy(line.substr(0, delIter)),
						trim_copy(line.substr(delIter+1)));

		else
			return std::make_pair(trim_copy(line), trim_copy(""));
	}

	inline std::pair<std::string, std::string> split_on_last(const std::string& line, const std::string& delim) {
		auto delIter = line.find_last_of(delim);

		if(delIter!=std::string::npos)
			return std::make_pair(
						trim_copy(line.substr(0, delIter)),
						trim_copy(line.substr(delIter+1)));

		else
			return std::make_pair(trim_copy(line), trim_copy(""));
	}

	inline void to_lower_inplace(std::string& str) {
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	}

	inline std::string to_lower(std::string str) {
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		return str;
	}

	template<class T>
	bool contains(const std::string& str, T&& pattern) {
		return str.find(pattern) != std::string::npos;
	}
	inline bool starts_with(const std::string& str, const std::string& pattern) {
		if(pattern.length()>str.length())
			return false;

		for(auto i=0ul; i<pattern.length(); ++i)
			if(pattern[i]!=str[i])
				return false;

		return true;
	}
	inline bool ends_with(const std::string& str, const std::string& pattern) {
		if(pattern.length()>str.length())
			return false;

		auto begin = str.length()-pattern.length();
		for(auto i=0u; i<pattern.length(); ++i)
			if(pattern[i]!=str[begin + i])
				return false;

		return true;
	}

}
}
