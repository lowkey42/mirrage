/***********************************************************\
 * JSON reader                                             *
 *     ___________ _____                                   *
 *    /  ___|  ___/ __  \                                  *
 *    \ `--.| |_  `' / /'                                  *
 *     `--. \  _|   / /                                    *
 *    /\__/ / |   ./ /___                                  *
 *    \____/\_|   \_____/                                  *
 *                                                         *
 *                                                         *
 *  Copyright (c) 2014 Florian Oetke                       *
 *                                                         *
 *  This file is part of SF2 and distributed under         *
 *  the MIT License. See LICENSE file for details.         *
\***********************************************************/

#pragma once

#include <cctype>
#include <string>
#include <istream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
#include <functional>
#include <cstdlib>

namespace sf2 {
namespace format {

	using Error_handler = std::function<void (const std::string& msg, uint32_t row, uint32_t column)>;

	class Json_reader {
		public:
			Json_reader(std::istream& stream, Error_handler ehandler=Error_handler{});

			// returns true if the next key is ready to be read
			bool in_obj();
			bool in_array();

			void skip_obj();

			bool read_nullptr(); // look-ahead if false

			void read(std::string&);
			void read(bool&);
			void read(float&);
			void read(double&);
			void read(uint8_t&);
			void read(int8_t&);
			void read(uint16_t&);
			void read(int16_t&);
			void read(uint32_t&);
			void read(int32_t&);
			void read(uint64_t&);
			void read(int64_t&);

			auto row()const noexcept {return _row;}
			auto column()const noexcept {return _column;}

		private:
			char _get();
			void _unget();
			void _mark();
			void _rewind();

			char _next(bool in_string=false);
			void _post_read();

			template<typename T>
			T _read_decimal();

			template<typename T>
			T _read_int();

			template<typename T>
			T _read_float();

			void _on_error(const std::string&);

			enum class State {
				obj_key, obj_value, array
			};

			std::istream& _stream;
			Error_handler _error_handler;
			bool _error = false;
			std::vector<State> _state;
			uint32_t _column = 1;
			uint32_t _row = 1;

			std::iostream::pos_type _marked_pos;
			uint32_t _saved_column = 1;
			uint32_t _saved_row = 1;
	};



	inline Json_reader::Json_reader(std::istream& stream, Error_handler ehandler)
	    : _stream(stream), _error_handler(ehandler) {
		_state.reserve(16);
	}

	inline void Json_reader::_on_error(const std::string& e) {
		if(_error)
			return; // ignore all errors after the first

		if(_error_handler) {
			_error_handler(e, _row, _column);
			_error = true;

		} else {
			std::cerr<<"Error parsing JSON at "<<_row<<":"<<_column<<" : "<<e<<std::endl;
			abort();
		}
	}
	inline char Json_reader::_get() {
		if(_error) {
			return 0;
		}

		auto c = _stream.get();
		_column++;
		if(c=='\n') {
			_column=1;
			_row++;
		}
		if(c==EOF) {
			std::string msg = "Unexpected end of file";
			for(auto s : _state) {
				msg+= ". Unclosed ";
				msg+= s==State::array ? "array" : "object";
			}
			_on_error(msg);
		}
		return c;
	}
	inline void Json_reader::_unget() {
		_stream.unget();
		auto c = _stream.peek();
		_column--;
		if(c=='\n') {
			_column=-1;
			_row--;
		}
	}
	inline void Json_reader::_mark() {
		_marked_pos = _stream.tellg();
		_saved_column = _column;
		_saved_row = _row;
	}
	inline void Json_reader::_rewind() {
		_stream.seekg(_marked_pos);
		_column = _saved_column;
		_row = _saved_row;
	}

	inline char Json_reader::_next(bool in_string) {
		if(_error) {
			return 0;
		}

		auto c = _get();
		if(c=='/' && !in_string && _stream.peek()=='*') { // comment
			_get();

			while(!_error && c=='*' && (c=_get())=='/') {
			}
		}

		if(!in_string) {
			while(!_error && !std::isgraph(c)) {
				c = _get();
			}
		}

		return c;
	}
	inline void Json_reader::_post_read() {
		if(_state.back()==State::obj_key) {
			_state.back() = State::obj_value;
			auto c = _next();
			if(c!=':') {
				_on_error("Missing ':' after object key");
			}
		}
	}

	template<typename T>
	T Json_reader::_read_decimal() {
		T val = 0;

		char c=_next();
		for(double dec=10; c>='0' && c<='9'; c=_get(), dec*=10.f)
			val+= (c-'0') / dec;

		_unget();

		return val;
	}

	template<typename T>
	T Json_reader::_read_int() {
		T val=0;
		bool negativ = false;

		char c = _next();

		if(c=='-' || c=='+') {
			negativ = c=='-';
			c = _next();
		}

		for(; c>='0' && c<='9'; c=_get())
			val= (10*val)+ (c-'0');

		_unget();

		if( negativ )
			val*=-1;

		return val;
	}

	template<typename T>
	T Json_reader::_read_float() {
		bool negativ = false;

		_mark();
		if(_next()=='-') {
			negativ = true;
		} else
			_rewind();


		T val = _read_int<T>();

		char c = _get();

		if(c=='.') {
			val += _read_decimal<T>();
			c = _get();
		}

		if(c=='e' || c=='E') {
			int64_t exp = _read_int<int64_t>();

			val *= (T) std::pow(exp>0 ? 10.0 : 0.1, static_cast<double>(std::abs(exp)));
		}else
			_unget();

		if( negativ )
			val*=-1;

		return val;
	}

	inline bool Json_reader::in_obj() {
		auto c = _next();

		switch(c) {
			case '{':
				c = _next();
				if(c=='}') {
					_post_read();
					return false;
				} else
					_unget();

				_state.push_back(State::obj_key);
				return true;

			case ',':
				assert(!_state.empty() &&_state.back()==State::obj_value);
				_state.back() = State::obj_key;
				return true;

			case '}':
				assert(!_state.empty() &&_state.back()==State::obj_value);
				_state.pop_back();
				if(!_state.empty())
					_post_read();
				return false;

			default:
				_on_error(std::string("Unexpected character ")+c+" in object");
				return false;
		}
	}

	inline bool Json_reader::in_array() {
		auto c = _next();

		switch(c) {
			case '[':
				c = _next();
				if(c==']') {
					_post_read();
					return false;
				} else
					_unget();

				_state.push_back(State::array);
				return true;

			case ',':
				assert(!_state.empty() &&_state.back()==State::array);
				_state.back() = State::array;
				return true;

			case ']':
				assert(!_state.empty() &&_state.back()==State::array);
				_state.pop_back();
				_post_read();
				return false;

			default:
				_on_error(std::string("Unexpected character ")+c+" in array");
				return false;
		}
	}

	inline void Json_reader::skip_obj() {
		auto c = _next();
		if(c!='{') {
			_on_error(std::string("Unexpected character ")+c+" in object");
			return;
		}

		int obj_depth = 1;
		while(obj_depth>0) {
			switch(_next()) {
				case '{':
					obj_depth++;
					break;
				case '}':
					obj_depth--;
					break;
				case '"': {
					auto str_c = _get();
					while(str_c!='"') {
						if(str_c=='\\') _get();
						str_c = _get();
					}
					break;
				}
				default:
					if(_error) {
						return;
					}

					break; // skip
			}
		}

		_post_read();
	}

	inline bool Json_reader::read_nullptr() { // look-ahead if false
		_mark();

		if(_next()=='n' && _get()=='u' && _get()=='l' && _get()=='l') {
			_post_read();
			return true;
		}

		_rewind();
		return false;
	}

	inline void Json_reader::read(std::string& val) {
		auto c = _next();

		if(c!='\"') {
			_on_error("Missing '\"' at the start of string");
			return;
		}

		c = _get();

		val.clear();
		while(c!='"') {
			if(c=='\\')
				val+=_get();
			else
				val+=c;

			c = _get();
		}

		_post_read();
	}

	inline void Json_reader::read(bool& val) {
		char chars[] {
		    _next(),
		    _get(),
		    _get(),
		    _get()
		};

		if(chars[0]=='t' && chars[1]=='r' && chars[2]=='u' && chars[3]=='e')
			val = true;
		else if(chars[0]=='f' && chars[1]=='a' && chars[2]=='l' && chars[3]=='s' && _get()=='e')
			val = false;
		else
			_on_error(std::string("Unknown boolean constant '")+chars+"'");

		_post_read();
	}

	inline void Json_reader::read(float& val) {
		val = _read_float<float>();

		_post_read();
	}

	inline void Json_reader::read(double& val) {
		val = _read_float<double>();

		_post_read();
	}

	inline void Json_reader::read(uint8_t& val) {
		val = _read_int<uint8_t>();

		_post_read();
	}

	inline void Json_reader::read(int8_t& val) {
		val = _read_int<int8_t>();

		_post_read();
	}

	inline void Json_reader::read(uint16_t& val) {
		val = _read_int<uint16_t>();

		_post_read();
	}

	inline void Json_reader::read(int16_t& val) {
		val = _read_int<int16_t>();

		_post_read();
	}

	inline void Json_reader::read(uint32_t& val) {
		val = _read_int<uint32_t>();

		_post_read();
	}

	inline void Json_reader::read(int32_t& val) {
		val = _read_int<int32_t>();

		_post_read();
	}

	inline void Json_reader::read(uint64_t& val) {
		val = _read_int<uint64_t>();

		_post_read();
	}

	inline void Json_reader::read(int64_t& val) {
		val = _read_int<int64_t>();

		_post_read();
	}

}
}
