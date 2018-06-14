/** Manager for nuklear ui ***************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include <nuklear.h>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/vec2.hpp>
#include <gsl/gsl>

#include <memory>


struct nk_context;

namespace mirrage {
	class Engine;

	namespace asset {
		class AID;
	}
	namespace asset {
		class Asset_manager;
	}
	namespace input {
		class Input_manager;
	}
} // namespace mirrage
namespace mirrage::gui {

	class Gui;

	namespace detail {
		struct Nk_renderer;
	}

	struct Gui_vertex {
		glm::vec2 position;
		glm::vec2 uv;
		nk_byte   color[4];
	};

	class Gui_renderer {
	  public:
		virtual ~Gui_renderer() = default;

		void draw_gui();

		virtual auto load_texture(int width, int height, int channels, const std::uint8_t* data)
		        -> std::shared_ptr<struct nk_image> = 0;

		virtual auto load_texture(const asset::AID&) -> std::shared_ptr<struct nk_image> = 0;

	  protected:
		friend class Gui;
		friend struct detail::Nk_renderer;

		virtual void prepare_draw(gsl::span<const std::uint16_t> indices,
		                          gsl::span<const Gui_vertex>    vertices,
		                          glm::mat4                      view_proj)  = 0;
		virtual void draw_elements(int           texture_handle,
		                           glm::vec4     clip_rect,
		                           std::uint32_t offset,
		                           std::uint32_t count) = 0;
		virtual void finalize_draw()                    = 0;

	  private:
		Gui* _gui = nullptr;
	};

	// TODO: gamepad input: https://gist.github.com/vurtun/519801825b4ccfad6767
	//                      https://github.com/vurtun/nuklear/issues/50
	// TODO: theme support: https://github.com/vurtun/nuklear/blob/master/demo/style.c
	//                      https://github.com/vurtun/nuklear/blob/master/example/skinning.c
	class Gui {
	  public:
		Gui(glm::vec4                        viewport,
		    asset::Asset_manager&            assets,
		    input::Input_manager&            input,
		    util::tracking_ptr<Gui_renderer> renderer);
		~Gui();

		void draw();
		void start_frame();

		auto ctx() -> nk_context*;
		auto renderer() noexcept -> auto& { return _renderer; }

		void viewport(glm::vec4 new_viewport);

		auto centered(int width, int height) -> struct nk_rect;
		auto centered_left(int width, int height) -> struct nk_rect;
		auto centered_right(int width, int height) -> struct nk_rect;


	  private:
		struct PImpl;

		glm::vec4                        _viewport;
		asset::Asset_manager&            _assets;
		input::Input_manager&            _input;
		util::tracking_ptr<Gui_renderer> _renderer;
		Gui_renderer*                    _last_renderer;
		std::unique_ptr<PImpl>           _impl;

		void _init();
	};


	// widgets
	extern bool color_picker(nk_context*, util::Rgb& color, int width, float factor = 1.f);
	extern bool color_picker(nk_context*, util::Rgba& color, int width, float factor = 1.f);

	extern void begin_menu(nk_context*, int& active);
	extern bool menu_button(nk_context*, const char* text, bool enabled = true);
	extern void end_menu(nk_context*);

	class Text_edit {
	  public:
		Text_edit();
		Text_edit(Text_edit&&) = default;
		Text_edit& operator=(Text_edit&&) = default;
		~Text_edit();

		void reset(const std::string&);
		void get(std::string&) const;

		auto active() const noexcept { return _active; }

		void update_and_draw(nk_context*, nk_flags type);
		void update_and_draw(nk_context*, nk_flags type, std::string&);

	  private:
		util::maybe<nk_text_edit> _data;
		bool                      _active = false;
	};
} // namespace mirrage::gui
