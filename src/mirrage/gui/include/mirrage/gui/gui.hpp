/** Manager for nuklear ui ***************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>


#define IM_VEC2_CLASS_EXTRA                        \
	ImVec2(const glm::vec2& f) : x(f.x), y(f.y) {} \
	operator glm::vec2() const { return glm::vec2(x, y); }
#define IM_VEC4_CLASS_EXTRA                                        \
	ImVec4(const glm::vec4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {} \
	operator glm::vec4() const { return glm::vec4(x, y, z, w); }

#include <imgui.h>

#include <cstdint>
#include <memory>


namespace mirrage::gui::literals {
	inline ImColor operator"" _imcolor(const char* str, std::size_t len)
	{
		auto c = mirrage::util::hex_to_color(str, len);
		return {std::pow(c.r, 2.2f), std::pow(c.g, 2.2f), std::pow(c.b, 2.2f), c.a};
	}
} // namespace mirrage::gui::literals

namespace mirrage::gui {
	class Gui;
}

namespace ImGui {
	inline constexpr float SIZE_AUTO = 0.0f;

	enum class WindowPosition_X { center = 0, left = 1, right = 2 };
	enum class WindowPosition_Y { center = 0, top = 1, bottom = 2 };

	extern void PositionNextWindow(glm::vec2        size,
	                               WindowPosition_X x,
	                               WindowPosition_Y y,
	                               glm::vec2        offset = {});

	struct Column {
		const char* header = "";
		float       size   = -1.f;

		Column() = default;
		/*implicit*/ Column(const char* header, float size = -1) : header(header), size(size) {}
	};

	extern void BeginTable(const char* id,
	                       std::initializer_list<Column>,
	                       bool first_call,
	                       bool border    = true,
	                       bool separator = true);

	extern float ValueSliderFloat(const char* label,
	                              float       v,
	                              float       v_min,
	                              float       v_max,
	                              const char* format = "%.3f",
	                              float       power  = 1.0f);

	// ImGui::InputText() with std::string
	// Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
	IMGUI_API bool InputText(const char*            label,
	                         std::string*           str,
	                         ImGuiInputTextFlags    flags     = 0,
	                         ImGuiInputTextCallback callback  = nullptr,
	                         void*                  user_data = nullptr);
	IMGUI_API bool InputTextMultiline(const char*            label,
	                                  std::string*           str,
	                                  const ImVec2&          size      = ImVec2(0, 0),
	                                  ImGuiInputTextFlags    flags     = 0,
	                                  ImGuiInputTextCallback callback  = nullptr,
	                                  void*                  user_data = nullptr);
	IMGUI_API bool InputTextWithHint(const char*            label,
	                                 const char*            hint,
	                                 std::string*           str,
	                                 ImGuiInputTextFlags    flags     = 0,
	                                 ImGuiInputTextCallback callback  = nullptr,
	                                 void*                  user_data = nullptr);


	enum class Image_scaling { stretch, fill_x, fill_y, fill_min, fill_max };
	void DrawImage(mirrage::gui::Gui&,
	               void*         image,
	               glm::vec2     center,
	               glm::vec2     size,
	               Image_scaling scaling = Image_scaling::stretch);

	void BackgroundImage(mirrage::gui::Gui&, void* image, Image_scaling scaling = Image_scaling::stretch);

	bool TexturedButton(mirrage::gui::Gui& gui,
	                    const char*        label,
	                    void*              image,
	                    glm::vec2          size    = {0, 0},
	                    Image_scaling      scaling = Image_scaling::stretch);

} // namespace ImGui

struct ImFont;

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
		glm::vec2    position;
		glm::vec2    uv;
		std::uint8_t color[4];
	};

	class Gui_renderer_interface {
	  public:
		Gui_renderer_interface()          = default;
		virtual ~Gui_renderer_interface() = default;

		void draw_gui();

		virtual auto load_texture(int width, int height, int channels, const std::uint8_t* data)
		        -> std::shared_ptr<void> = 0;

		virtual auto load_texture(const asset::AID&) -> std::shared_ptr<void> = 0;

		virtual auto texture_size(void* texture_handle) -> util::maybe<glm::ivec2> = 0;

	  protected:
		friend class Gui;
		friend struct detail::Nk_renderer;

		using Prepare_data_src = std::function<void(std::uint16_t*, Gui_vertex*)>;

		virtual void prepare_draw(std::size_t      index_count,
		                          std::size_t      vertex_count,
		                          glm::mat4        view_proj,
		                          Prepare_data_src write_data)  = 0;
		virtual void draw_elements(void*         texture_handle,
		                           glm::vec4     clip_rect,
		                           std::uint32_t offset,
		                           std::uint32_t count,
		                           std::uint32_t vertex_offset) = 0;
		virtual void finalize_draw()                            = 0;

	  private:
		template <class>
		friend class Gui_renderer_instance;

		Gui* _gui;
	};


	class Gui {
	  public:
		Gui(glm::vec4 viewport, asset::Asset_manager& assets, input::Input_manager& input);
		~Gui();

		void draw();
		void start_frame();

		auto find_font(util::Str_id) const -> util::maybe<ImFont*>;

		auto viewport() const noexcept { return _viewport; }
		void viewport(glm::vec4 new_viewport);

		auto ready() const noexcept { return bool(_impl); }

		auto load_texture(const asset::AID&) -> std::shared_ptr<void>;
		auto texture_size(void* texture_handle) -> util::maybe<glm::ivec2>;

	  private:
		template <class>
		friend class Gui_renderer_instance;

		struct PImpl;

		glm::vec4               _viewport;
		asset::Asset_manager&   _assets;
		input::Input_manager&   _input;
		Gui_renderer_interface* _renderer = nullptr;
		std::unique_ptr<PImpl>  _impl;

		void _reset_renderer(Gui_renderer_interface*);
	};

	template <class Base>
	class Gui_renderer_instance : public Base {
		static_assert(std::is_base_of_v<Gui_renderer_interface, Base>,
		              "A gui renderer need to derive from mirrage::gui::Gui_renderer_interface!");

	  public:
		template <class... Args>
		Gui_renderer_instance(Gui& gui, Args&&... args) : Base(std::forward<Args>(args)...)
		{
			this->_gui = &gui;
			gui._reset_renderer(this);
		}
		~Gui_renderer_instance()
		{
			this->_gui->_reset_renderer(nullptr);
			this->_gui = nullptr;
		}
	};

} // namespace mirrage::gui
