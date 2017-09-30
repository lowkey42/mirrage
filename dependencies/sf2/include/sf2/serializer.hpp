/***********************************************************\
 * Generic de-/ serializers for annotated types            *
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

#include <functional>
#include <memory>
#include <iostream>

#include "reflection_data.hpp"

namespace sf2 {

	template<typename Writer>
	struct Serializer;

	template<typename Writer>
	struct Deserializer;

	using Error_handler = std::function<void (const std::string& msg, uint32_t row, uint32_t column)>;


	namespace details {
		using std::begin; using std::end;

		template<class Writer, class T>
		struct has_save {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(decltype(save(std::declval<Serializer<Writer>&>(), std::declval<const C&>()))*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};
		template<class Reader, class T>
		struct has_load {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(decltype(load(std::declval<Deserializer<Reader>&>(), std::declval<C&>()))*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};
		template<class T>
		struct has_post_load {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(decltype(post_load(std::declval<C&>()))*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};
		template<class T>
		void call_post_load(T&, typename std::enable_if_t<!has_post_load<T>::value>* = nullptr) {
		}
		template<class T>
		void call_post_load(T& inst, typename std::enable_if_t<has_post_load<T>::value>* = nullptr) {
			post_load(inst);
		}

		template<class T>
		struct is_range {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(decltype(begin(std::declval<C&>()))*, decltype(end(std::declval<C&>()))*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0, 0)) == sizeof(char) };
		};

		template<class T>
		struct has_key_type {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(typename C::key_type*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};

		template<class T>
		struct has_mapped_type {
			private:
				typedef char one;
				typedef long two;

				template <typename C> static one test(typename C::mapped_type*);
				template <typename C> static two test(...);


			public:
				enum { value = sizeof(test<T>(0)) == sizeof(char) };
		};

		template<class T>
		struct is_map {
			enum { value = is_range<T>::value &&
			               has_key_type<T>::value &&
			               has_mapped_type<T>::value };
		};
		template<class T>
		struct is_set {
			enum { value = is_range<T>::value &&
			               has_key_type<T>::value &&
			               !has_mapped_type<T>::value };
		};
		template<class T>
		struct is_list {
			enum { value = is_range<T>::value &&
			               !has_key_type<T>::value &&
			               !has_mapped_type<T>::value };
		};
	}

	template<typename T>
	auto vmember(String_literal name, T&& v) {
		return std::pair<String_literal, T&>(name, v);
	}


	template<typename Writer>
	struct Serializer {
		Serializer(Writer&& w) : writer(std::move(w)) {}

		template<class T>
		std::enable_if_t<is_annotated_struct<T>::value && not details::has_save<Writer,T>::value>
		  write(const T& inst) {
			writer.begin_obj();

			get_struct_info<T>().for_each([&](auto n, auto mptr) {
				this->write_member(n, inst.*mptr);
			});

			writer.end_current();
		}
		template<class T>
		std::enable_if_t<details::has_save<Writer,T>::value>
		  write(const T& inst) {
			save(*this, inst);
		}

		template<typename... Members>
		inline void write_virtual(Members&&... m) {
			writer.begin_obj();

			auto i = {0, write_member_pair(m)...};
			(void)i;

			writer.end_current();
		}

		template<typename Func>
		inline void write_lambda(Func func) {
			writer.begin_obj();

			func();

			writer.end_current();
		}

		private:
			Writer writer;

			template<class K, class T>
			int write_member_pair(std::pair<K, T&> inst) {
				writer.write(inst.first);
				write_value(inst.second);
				return 0;
			}
			template<class T>
			int write_member_pair(std::pair<String_literal, T&> inst) {
				writer.write(inst.first.data, inst.first.len);
				write_value(inst.second);
				return 0;
			}

			template<class T>
			void write_member(String_literal name, const T& inst) {
				writer.write(name.data, name.len);
				write_value(inst);
			}

		public:
			template<class T>
			std::enable_if_t<not details::has_save<Writer,T*>::value>
			  write_value(const T* inst) {
				if(inst)
					write_value(*inst);
				else
					writer.write_nullptr();
			}
			template<class T>
			std::enable_if_t<not details::has_save<Writer,std::unique_ptr<T>>::value>
			  write_value(const std::unique_ptr<T>& inst) {
				if(inst)
					write_value(*inst);
				else
					writer.write_nullptr();
			}
			template<class T>
			std::enable_if_t<not details::has_save<Writer,std::shared_ptr<T>>::value>
			  write_value(const std::shared_ptr<T>& inst) {
				if(inst)
					write_value(*inst);
				else
					writer.write_nullptr();
			}


			// annotated struct
			template<class T>
			std::enable_if_t<is_annotated_struct<T>::value && not details::has_save<Writer,T>::value>
			  write_value(const T& inst) {
				writer.begin_obj();

				get_struct_info<T>().for_each([&](auto n, auto mptr) {
					this->write_member(n, inst.*mptr);
				});

				writer.end_current();
			}

			// annotated enum
			template<class T>
			std::enable_if_t<is_annotated_enum<T>::value && not details::has_save<Writer,T>::value>
			  write_value(const T& inst) {
				auto name = get_enum_info<T>().name_of(inst);
				writer.write(name.data, name.len);
			}

			// map
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::has_save<Writer,T>::value
			                 && details::is_map<T>::value>
			  write_value(const T& inst) {

				writer.begin_obj();

				for(auto& v : inst) {
					write_value(v.first);
					write_value(v.second);
				}

				writer.end_current();
			}

			// other collection
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::has_save<Writer,T>::value
			                 && (details::is_list<T>::value || details::is_set<T>::value)>
			  write_value(const T& inst) {

				writer.begin_array();

				for(auto& v : inst)
					write_value(v);

				writer.end_current();
			}

			// manual save-function
			template<class T>
			std::enable_if_t<details::has_save<Writer,T>::value>
			  write_value(const T& inst) {
				save(*this, inst);
			}

			// other
			template<class T>
			std::enable_if_t<std::is_integral<T>::value || std::is_floating_point<T>::value>
			  write_value(const T& inst) {
				writer.write(inst);
			}

			void write_value(const std::string& inst) {
				writer.write(inst);
			}
			void write_value(const char* inst) {
				writer.write(inst);
			}
			void write_value(String_literal str) {
				writer.write(str.data, str.len);
			}
	};

	template<typename Writer, typename T>
	inline void serialize(Writer&& w, const T& v) {
		Serializer<Writer>{std::move(w)}.write(v);
	}

	template<typename Writer, typename... Members>
	inline void serialize_virtual(Writer&& w, Members&&... m) {
		Serializer<Writer>{std::move(w)}.write_virtual(std::forward<Members>(m)...);
	}


	template<typename Reader>
	struct Deserializer {
		Deserializer(Reader&& r, Error_handler error_handler=Error_handler())
		    : reader(std::move(r)), error_handler(error_handler) {
			buffer.reserve(64);
		}

		template<class T>
		std::enable_if_t<is_annotated_struct<T>::value && not details::has_load<Reader,T>::value>
		  read(T& inst) {

			while(reader.in_obj()) {
				reader.read(buffer);

				auto match = false;
				auto key = String_literal{buffer};

				get_struct_info<T>().for_each([&](String_literal n, auto mptr) {
					if(!match && n==key) {
						this->read_value(inst.*mptr);
						match = true;
					}
				});

				if(!match) {
					on_error("Unexpected key "+buffer);
				}
			}

			details::call_post_load(inst);
		}

		// manual load-function
		template<class T>
		std::enable_if_t<details::has_load<Reader,T>::value>
		  read(T& inst) {
			load(*this, inst);

			details::call_post_load(inst);
		}

		template<typename... Members>
		inline void read_virtual(Members&&... m) {
			while(reader.in_obj()) {
				reader.read(buffer);

				bool match = false;
				auto key = String_literal{buffer};

				auto i = {read_member_pair(match, key, m)...};
				(void)i;

				if(!match) {
					on_error("Unexpected key "+buffer);
				}
			}
		}

		template<typename Func>
		inline void read_lambda(Func func) {
			while(reader.in_obj()) {
				reader.read(buffer);

				bool match = func(buffer);

				if(!match) {
					on_error("Unexpected key "+buffer);
				}
			}
		}

		private:
			Reader reader;
			std::string buffer;
			Error_handler error_handler;

			void on_error(const std::string& e) {
				if(error_handler)
					error_handler(e, reader.row(), reader.column());
				else
					std::cerr<<"Error parsing JSON at "<<reader.row()<<":"<<reader.column()<<" : "<<e<<std::endl;
			}

			template<class K, class T>
			int read_member_pair(bool& match, String_literal n, std::pair<K, T&> inst) {
				if(!match && inst.first==n) {
					read_value(inst.second);
					match = true;
				}
				return 0;
			}

		public:
			template<class T>
			std::enable_if_t<not details::has_load<Reader,std::unique_ptr<T>>::value>
			  read_value(std::unique_ptr<T>& inst) {
				if(reader.read_nullptr())
					inst = nullptr;

				else {
					inst = std::make_unique<T>();
					read_value(*inst);
				}
			}
			template<class T>
			std::enable_if_t<not details::has_load<Reader,std::shared_ptr<T>>::value>
			  read_value(std::shared_ptr<T>& inst) {
				if(reader.read_nullptr())
					inst = nullptr;

				else {
					inst = std::make_shared<T>();
					read_value(*inst);
				}
			}


			// annotated struct
			template<class T>
			std::enable_if_t<is_annotated_struct<T>::value && not details::has_load<Reader,T>::value>
			  read_value(T& inst) {
				while(reader.in_obj()) {
					reader.read(buffer);

					bool match = false;
					auto key = String_literal{buffer};

					get_struct_info<T>().for_each([&](auto n, auto mptr) {
						if(!match && n==key) {
							this->read_value(inst.*mptr);
							match = true;
						}
					});

					if(!match) {
						on_error("Unexpected key "+buffer);
					}
				}

				details::call_post_load(inst);
			}

			// annotated enum
			template<class T>
			std::enable_if_t<is_annotated_enum<T>::value && not details::has_load<Reader,T>::value>
			  read_value(T& inst) {
				reader.read(buffer);
				inst = get_enum_info<T>().value_of(buffer);

				details::call_post_load(inst);
			}

			// map
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::has_load<Reader,T>::value
			                 && details::is_map<T>::value>
			  read_value(T& inst) {
				inst.clear();

				while(reader.in_obj()) {
					typename T::key_type key;
					typename T::mapped_type val;

					read_value(key);
					read_value(val);

					inst.emplace(std::move(key), std::move(val));
				}
			}

			//set
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::has_load<Reader,T>::value
			                 && details::is_set<T>::value>
			  read_value(T& inst) {
				inst.clear();

				while(reader.in_array()) {
					typename T::value_type v;
					read_value(v);

					inst.emplace(std::move(v));
				}
			}

			// other collection
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::has_load<Reader,T>::value
			                 && details::is_list<T>::value>
			  read_value(T& inst) {
				inst.clear();

				while(reader.in_array()) {
					typename T::value_type v;
					read_value(v);

					inst.emplace_back(std::move(v));
				}
			}

			// manual load-function
			template<class T>
			std::enable_if_t<details::has_load<Reader,T>::value>
			  read_value(T& inst) {
				load(*this, inst);

				details::call_post_load(inst);
			}

			// other
			template<class T>
			std::enable_if_t<not is_annotated<T>::value
			                 && not details::is_range<T>::value
			                 && not details::has_load<Reader,T>::value>
			  read_value(T& inst) {
				reader.read(inst);
			}

			void read_value(std::string& inst) {
				reader.read(inst);
			}

			void skip_obj() {
				reader.skip_obj();
			}
	};

	template<typename Reader, typename T>
	inline void deserialize(Reader&& r, T& v) {
		Deserializer<Reader>{std::move(r)}.read(v);
	}

	template<typename Reader, typename... Members>
	inline void deserialize_virtual(Reader&& r, Members&&... m) {
		Deserializer<Reader>{std::move(r)}.read_virtual(std::forward<Members>(m)...);
	}

	template<class T, class Reader>
	using is_loadable = std::disjunction<is_annotated<T>, details::has_load<Reader,T>>;

}
