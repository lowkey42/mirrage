/** basic types and enumerations used by the Input_manager *******************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/str_id.hpp>

#include <memory>
#include <glm/vec2.hpp>
#include <SDL2/SDL.h>


namespace lux {
namespace input {

	enum class Key : int32_t {
		invalid       = -1,
		Return        = SDLK_RETURN,
		Escape        = SDLK_ESCAPE,
		Backspace     = SDLK_BACKSPACE,
		Tab           = SDLK_TAB,
		Space         = SDLK_SPACE,
		Exclaim       = SDLK_EXCLAIM,
		Quotedbl      = SDLK_QUOTEDBL,
		Hash          = SDLK_HASH,
		Percent       = SDLK_PERCENT,
		Dollar        = SDLK_DOLLAR,
		Ampersand     = SDLK_AMPERSAND,
		Quote         = SDLK_QUOTE,
		LeftParan     = SDLK_LEFTPAREN,
		RightParan    = SDLK_RIGHTPAREN,
		Asterisk      = SDLK_ASTERISK,
		Plus          = SDLK_PLUS,
		Comma         = SDLK_COMMA,
		Minus         = SDLK_MINUS,
		Period        = SDLK_PERIOD,
		Slash         = SDLK_SLASH,
		T_0           = SDLK_0,
		T_1           = SDLK_1,
		T_2           = SDLK_2,
		T_3           = SDLK_3,
		T_4           = SDLK_4,
		T_5           = SDLK_5,
		T_6           = SDLK_6,
		T_7           = SDLK_7,
		T_8           = SDLK_8,
		T_9           = SDLK_9,
		Colon         = SDLK_COLON,
		Semicolon     = SDLK_SEMICOLON,
		Less          = SDLK_LESS,
		Equals        = SDLK_EQUALS,
		Greater       = SDLK_GREATER,
		Question      = SDLK_QUESTION,
		At            = SDLK_AT,
		LeftBracket   = SDLK_LEFTBRACKET,
		RightBracket  = SDLK_BACKSLASH,
		Backslash     = SDLK_RIGHTBRACKET,
		Caret         = SDLK_CARET,
		Underscore    = SDLK_UNDERSCORE,
		Backquote     = SDLK_BACKQUOTE,
		A             = SDLK_a,
		B             = SDLK_b,
		C             = SDLK_c,
		D             = SDLK_d,
		E             = SDLK_e,
		F             = SDLK_f,
		G             = SDLK_g,
		H             = SDLK_h,
		I             = SDLK_i,
		J             = SDLK_j,
		K             = SDLK_k,
		L             = SDLK_l,
		M             = SDLK_m,
		N             = SDLK_n,
		O             = SDLK_o,
		P             = SDLK_p,
		Q             = SDLK_q,
		R             = SDLK_r,
		S             = SDLK_s,
		T             = SDLK_t,
		U             = SDLK_u,
		V             = SDLK_v,
		W             = SDLK_w,
		X             = SDLK_x,
		Y             = SDLK_y,
		Z             = SDLK_z,
		Capslock      = SDLK_CAPSLOCK,

		F1            = SDLK_F1,
		F2            = SDLK_F2,
		F3            = SDLK_F3,
		F4            = SDLK_F4,
		F5            = SDLK_F5,
		F6            = SDLK_F6,
		F7            = SDLK_F7,
		F8            = SDLK_F8,
		F9            = SDLK_F9,
		F10           = SDLK_F10,
		F11           = SDLK_F11,
		F12           = SDLK_F12,
		PrintScreen   = SDLK_PRINTSCREEN,
		Scrolllock    = SDLK_SCROLLLOCK,
		Pause         = SDLK_PAUSE,
		Insert        = SDLK_INSERT,
		Home          = SDLK_HOME,
		PageUp        = SDLK_PAGEUP,
		Delete        = SDLK_DELETE,
		End           = SDLK_END,
		PageDown      = SDLK_PAGEDOWN,

		Left          = SDLK_LEFT,
		Right         = SDLK_RIGHT,
		Up            = SDLK_UP,
		Down          = SDLK_DOWN,

		KP_0          = SDLK_KP_0,
		KP_1          = SDLK_KP_1,
		KP_2          = SDLK_KP_2,
		KP_3          = SDLK_KP_3,
		KP_4          = SDLK_KP_4,
		KP_5          = SDLK_KP_5,
		KP_6          = SDLK_KP_6,
		KP_7          = SDLK_KP_7,
		KP_8          = SDLK_KP_8,
		KP_9          = SDLK_KP_9,

		LControl      = SDLK_LCTRL,
		LShift        = SDLK_LSHIFT,
		LAlt          = SDLK_LALT,
		LSuper        = SDLK_LGUI,
		RControl      = SDLK_RCTRL,
		RShift        = SDLK_RSHIFT,
		RAlt          = SDLK_RALT,
		RSuper        = SDLK_RGUI
	};

	enum class Pad_stick : int8_t {
		invalid       = -1,
		left, right
	};
	constexpr auto pad_stick_count = static_cast<int8_t>(Pad_stick::right)+1;

	enum class Pad_button : int8_t {
		invalid        = -1,

		a              = SDL_CONTROLLER_BUTTON_A,
		b              = SDL_CONTROLLER_BUTTON_B,
		x              = SDL_CONTROLLER_BUTTON_X,
		y              = SDL_CONTROLLER_BUTTON_Y,
		back           = SDL_CONTROLLER_BUTTON_BACK,
		guide          = SDL_CONTROLLER_BUTTON_GUIDE,
		start          = SDL_CONTROLLER_BUTTON_START,
		left_stick     = SDL_CONTROLLER_BUTTON_LEFTSTICK,
		right_stick    = SDL_CONTROLLER_BUTTON_RIGHTSTICK,
		left_shoulder  = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		right_shoulder = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
		d_pad_up       = SDL_CONTROLLER_BUTTON_DPAD_UP,
		d_pad_down     = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		d_pad_left     = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		d_pad_right    = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,

		left_trigger,
		right_trigger
	};
	constexpr auto pad_button_count = static_cast<int8_t>(Pad_button::right_trigger)+1;

	using Mouse_button = uint8_t;

	struct Mouse_click {
		Mouse_button button;
		int8_t clicks = -1;

		bool operator<(Mouse_click rhs)const noexcept {
			return std::tie(button,clicks) < std::tie(rhs.button,rhs.clicks);
		}
		bool operator==(Mouse_click rhs)const noexcept {
			return button==rhs.button && clicks==rhs.clicks;
		}
	};

	using Action_id = util::Str_id;
	using Context_id = util::Str_id;
	using Input_source = int8_t;

	enum class Reaction_type {
		none, once, continuous, range
	};


#ifdef sf2_structDef
	sf2_enumDef(Key,
		Return,
		Escape,
		Backspace,
		Tab,
		Space,
		Exclaim,
		Quotedbl,
		Hash,
		Percent,
		Dollar,
		Ampersand,
		Quote,
		LeftParan,
		RightParan,
		Asterisk,
		Plus,
		Comma,
		Minus,
		Period,
		Slash,
		T_0,
		T_1,
		T_2,
		T_3,
		T_4,
		T_5,
		T_6,
		T_7,
		T_8,
		T_9,
		Colon,
		Semicolon,
		Less,
		Equals,
		Greater,
		Question,
		At,
		LeftBracket,
		RightBracket,
		Backslash,
		Caret,
		Underscore,
		Backquote,
		A,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		Capslock,

		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		PrintScreen,
		Scrolllock,
		Pause,
		Insert,
		Home,
		PageUp,
		Delete,
		End,
		PageDown,

		Left,
		Right,
		Up,
		Down,

		KP_0,
		KP_1,
		KP_2,
		KP_3,
		KP_4,
		KP_5,
		KP_6,
		KP_7,
		KP_8,
		KP_9,

		LControl,
		LShift,
		LAlt,
		LSuper,
		RControl,
		RShift,
		RAlt,
		RSuper
	)

	sf2_enumDef(Pad_stick,
		left,
		right
	)

	sf2_enumDef(Pad_button,
		a,
		b,
		x,
		y,
		back,
		guide,
		start,
		left_stick,
		right_stick,
		left_shoulder,
		right_shoulder,
		d_pad_up,
		d_pad_down,
		d_pad_left,
		d_pad_right,
		left_trigger,
		right_trigger
	)

	sf2_structDef(Mouse_click, button, clicks)

	sf2_enumDef(Reaction_type, none, once, continuous, range)
#endif

}
}

namespace std {
	template <> struct hash<lux::input::Key> {
		size_t operator()(lux::input::Key ac)const noexcept {
			return static_cast<size_t>(ac);
		}
	};
	template <> struct hash<lux::input::Pad_button> {
		size_t operator()(lux::input::Pad_button ac)const noexcept {
			return static_cast<size_t>(ac);
		}
	};
	template <> struct hash<lux::input::Pad_stick> {
		size_t operator()(lux::input::Pad_stick ac)const noexcept {
			return static_cast<size_t>(ac);
		}
	};
	template <> struct hash<lux::input::Mouse_click> {
		size_t operator()(lux::input::Mouse_click b)const noexcept {
			return static_cast<size_t>(b.button) + 101 * static_cast<std::size_t>(b.clicks+128);
		}
	};
}
