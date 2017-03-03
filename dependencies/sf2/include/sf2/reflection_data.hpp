/***********************************************************\
 * structures that describe the collected enum/struct info *
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

#include <string>
#include <map>
#include <unordered_map>
#include <array>
#include <cassert>

#include <iostream>

namespace sf2 {

	namespace details {
		constexpr auto calc_hash(const char* data, std::size_t len)noexcept -> std::size_t {
			return len==0 ? 0 :
			                (len==1 ? 0 : calc_hash(data+1, len-1)*101) + data[0];
			/*
			// requires N3652 (Relaxing constraints on constexpr-functions), that is not supported by gcc < 5.0 (mingw & ndk)
			std::size_t h = 0;
			for(std::size_t i=0; i<len; ++i)
				h = h*101 + data[i];

			return h;
			*/
		}
	}

	struct String_literal {
		const char* data;
		std::size_t len;
		std::size_t hash;

		template<std::size_t N>
		constexpr String_literal(const char (&data)[N]) : data(data), len(N-1), hash(details::calc_hash(data, len)) {

		}
		String_literal(const std::string& str) : data(str.data()), len(str.size()), hash(details::calc_hash(data,len)) {
		}

		bool operator==(const String_literal& rhs)const {
			if(len!=rhs.len) {
				return false;
			}
			if(hash!=rhs.hash)
				return false;

			for(std::size_t i=0; i<len; ++i) {
				if(data[i]!=rhs.data[i])
					return false;
			}

			return true;
		}
		bool operator==(const char* rhs)const {
			for(std::size_t i=0; i<len; ++i) {
				if(rhs[i]==0 || data[i]!=rhs[i])
					return false;
			}

			if(rhs[len]!=0)
				return false;

			return true;
		}
	};
}
namespace std {
	template<>
	struct hash<sf2::String_literal> {
		size_t operator()(const sf2::String_literal& s) const {
			return s.hash;
		}
	};
}

namespace sf2 {
	template<typename T>
	class Enum_info {
		public:
			using Value_type = std::pair<T, String_literal>;

		public:
			Enum_info(String_literal name, std::initializer_list<Value_type> v)
			    : _name(name) {
				for(auto& e : v) {
					_names.emplace(e.first, e.second);
					_values.emplace(e.second, e.first);
				}
			}

			auto name()const noexcept {return _name;}

			auto value_of(const String_literal& name)const noexcept -> T {
				auto i = _values.find(name);
				assert(i!=_values.end());
				return i->second;
			}
			auto value_of(const std::string& name)const noexcept -> T {
				auto i = _values.find(String_literal{name});
				assert(i!=_values.end());
				return i->second;
			}

			auto name_of (T value)const noexcept -> String_literal {
				auto i = _names.find(value);
				assert(i!=_names.end());
				return i->second;
			}

		private:
			struct Enum_hash {
				auto operator()(T v)const {
					return static_cast<std::size_t>(v);
				}
			};

			String_literal _name;
			std::unordered_map<T, String_literal, Enum_hash> _names;
			std::unordered_map<String_literal, T> _values;
	};


	template<typename ST, typename MT>
	using Member_ptr = MT ST::*;

	template<typename T, typename MemberT>
	using Member_data = std::tuple<MemberT T::*, String_literal>;

	template<typename T, typename... MemberT>
	class Struct_info {
		public:
			static constexpr std::size_t member_count = sizeof...(MemberT);

			constexpr Struct_info(String_literal name, Member_data<T,MemberT>... members) : _name(name), _member_names{{std::get<1>(members)...}}, _members(std::get<0>(members)...) {
			}

			constexpr auto name()const {return _name;}
			constexpr auto& members()const {return _member_names;}
			constexpr auto& member_ptrs()const {return _members;}
			constexpr auto size()const {return member_count;}


			template<std::size_t I = 0, typename FuncT>
			inline typename std::enable_if<I == member_count, void>::type
			for_each( FuncT)const {}

			template<std::size_t I = 0, typename FuncT>
			inline typename std::enable_if<I < member_count, void>::type
			for_each(FuncT f)const {
				f(_member_names[I], std::get<I>(_members));
				for_each<I + 1, FuncT>(f);
			}

		private:
			String_literal _name;
			std::array<String_literal, member_count> _member_names;
			std::tuple<Member_ptr<T,MemberT>...> _members;
	};


	template<typename T>
	auto get_enum_info() -> decltype(sf2_enum_info_factory((T*)nullptr)) {
		return sf2_enum_info_factory((T*)nullptr);
	}

	template<typename T>
	auto get_struct_info() -> decltype(sf2_struct_info_factory((T*)nullptr)) {
		return sf2_struct_info_factory((T*)nullptr);
	}


	template<class T>
	struct is_annotated_struct {
		private:
			typedef char one;
			typedef long two;

			template <typename C> static one test( std::remove_reference_t<decltype(sf2_struct_info_factory((C*)nullptr))>* ) ;
			template <typename C> static two test(...);


		public:
			enum { value = sizeof(test<T>(0)) == sizeof(char) };
	};

	template<class T>
	struct is_annotated_enum {
		private:
			typedef char one;
			typedef long two;

			template <typename C> static one test( std::remove_reference_t<decltype(sf2_enum_info_factory((C*)nullptr))>* ) ;
			template <typename C> static two test(...);


		public:
			enum { value = sizeof(test<T>(0)) == sizeof(char) };
	};

	template<class T>
	struct is_annotated {
		enum { value = is_annotated_struct<T>::value ||
		               is_annotated_enum<T>::value };
	};

}
