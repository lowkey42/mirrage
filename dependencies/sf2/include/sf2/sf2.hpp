/***********************************************************\
 * The public interface of SF2                             *
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

#ifndef SF2_HPP_
#define SF2_HPP_
#pragma once

#include "reflection.hpp"
#include "serializer.hpp"

#include "formats/json_reader.hpp"
#include "formats/json_writer.hpp"


namespace sf2 {

	using JsonSerializer   = Serializer<format::Json_writer>;
	using JsonDeserializer = Deserializer<format::Json_reader>;

	template<typename T>
	inline void serialize_json(std::ostream& stream, const T& v) {
		JsonSerializer{format::Json_writer{stream}}.write(v);
	}
	template<typename... Members>
	inline void serialize_json_virtual(std::ostream& stream, Members&&... m) {
		JsonSerializer{format::Json_writer{stream}}.write_virtual(std::forward<Members>(m)...);
	}

	template<typename T>
	inline void deserialize_json(std::istream& stream, T& v) {
		JsonDeserializer{format::Json_reader{stream}}.read(v);
	}
	template<typename T>
	inline void deserialize_json(std::istream& stream, format::Error_handler on_error, T& v) {
		JsonDeserializer{format::Json_reader{stream, on_error}, on_error}.read(v);
	}
	template<typename... Members>
	inline void deserialize_json_virtual(std::istream& stream, Members&&... m) {
		JsonDeserializer{format::Json_reader{stream}}.read_virtual(std::forward<Members>(m)...);
	}
	template<typename... Members>
	inline void deserialize_json_virtual(std::istream& stream, format::Error_handler on_error, Members&&... m) {
		JsonDeserializer{format::Json_reader{stream, on_error}, on_error}.read_virtual(std::forward<Members>(m)...);
	}

}

#endif
