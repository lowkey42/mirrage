[![Build Status](https://travis-ci.org/lowkey42/sf2.svg?branch=master)](https://travis-ci.org/lowkey42/sf2)

SF2 - Simple and Fast Struct Formater
===

SF2 is a simple header-only JSON-based serializer for C++ classes. It is designed to be as lightweight and simple to use as possible but also support a wide range of possible use cases (see examples below).

The library consists of 2 main parts, a reflection backend and the serializer itself. The reflection backend defines the two macros sf2_enumDef and sf2_structDef, which can be used to annotate a enum class or struct/class and define the fields that should be serialized. This information can then be accessed through the sf2::Enum_info and the sf2::Struct_info class.

The serializer uses the provided information to load or save an instance of an annotated struct to JSON and write it into a std::iostream.

## Supported Types
* any enum class with a sf2_enumDef definition in the same namespace
* any default constructible class or struct with a sf2_structDef definition in the same namespace and all serialized members are public
* any default constructible class or struct with a sf2_structDef definition in the same namespace and a friend declaration for sf2_accesor(ClassName)
* any default constructible type with an adl function load(sf2::JsonDeserializer&,T&) is desrializable and any type with an adl function save(sf2::JsonSerializer&, const T&) is serializable
* std::shared_ptr
* std::unique_ptr
* std::string
* const char* (only serialization)
* any float or integer type
* any range (adl begin and end functions) T that has
* T::key_type, T::mapped_type and the member functions: clear() and emplace(T::key_type, T::mapped_type).
Interpreted as a map (JSON object)
* T::key_type, T::value_type and the member functions: clear() and emplace(T::value_type).
Interpreted as a set (JSON array)
* T::value_type and member functions: clear() and emplace_back(T::value_type).
Interpreted as a list (JSON array)

## Requirements
A C++14 compliant compiler (constexpr, variadic templates, SFINAE, ...).

===
Simple Example:
``` cpp
#include <sf2/sf2.hpp>
#include <sstream>

enum class Color {
	red, green, blue
};
sf2_enumDef(Color, red, green, blue)

struct Position {
	float x, y, z;
};
sf2_structDef(Position, x, y, z)

struct Player {
	Position position;
	Color color;
	std::string name;
};
sf2_structDef(Player, position, color, name)

int main() {
	Player player{Position{1,2,3}, Color::red, "example"};
	
	// save
	std::stringstream out;
	sf2::serialize_json(out, player);

	// load
	std::istreamstream in{out.str()};
	sf2::deserialize_json(in, player);
}
```

Manual Load/Save and virtual struct:
``` cpp
struct Abstract_data {
	virtual void load(sf2::JsonDeserializer& s) = 0;
	virtual void save(sf2::JsonSerializer& s)const = 0;
};
void load(sf2::JsonDeserializer& s, Abstract_data& d) {
	d.load(s);
}
void save(sf2::JsonSerializer& s, const Abstract_data& d) {
	d.save(s);
}

struct My_data : Abstract_data {
	void load(sf2::JsonDeserializer& s) override {
		s.read_virtual(
		            sf2::vmember("a", a),
		            sf2::vmember("b", b) );
	}
	void save(sf2::JsonSerializer& s)const override {
		s.write_virtual(
		            sf2::vmember("a", a),
		            sf2::vmember("b", b) );
	}

	float a;
	bool b;
};
```


