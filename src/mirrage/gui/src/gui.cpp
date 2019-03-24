#include <mirrage/gui/gui.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/input/events.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/utils/ranges.hpp>

#include <SDL.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <sf2/sf2.hpp>

#include <cstdint>
#include <string>


extern void ref_embedded_assets_mirrage_gui();

namespace ImGui {

	void PositionNextWindow(glm::vec2 size, WindowPosition_X x, WindowPosition_Y y, glm::vec2 offset)
	{
		auto sw = GetIO().DisplaySize.x;
		auto sh = GetIO().DisplaySize.y;

		SetNextWindowSize(ImVec2(size.x, size.y), ImGuiCond_Once);

		auto position = ImVec2(offset.x, offset.y);
		switch(x) {
			case WindowPosition_X::left: position.x += 0.f; break;
			case WindowPosition_X::right: position.x += sw - size.x; break;
			case WindowPosition_X::center: position.x += sw / 2.f - size.x / 2.f; break;
		}

		switch(y) {
			case WindowPosition_Y::top: position.y += 0.f; break;
			case WindowPosition_Y::bottom: position.y += sh - size.y; break;
			case WindowPosition_Y::center: position.y += sh / 2.f - size.y / 2.f; break;
		}

		SetNextWindowPos(position, ImGuiCond_Once);
	}

	void BeginTable(const char*                   id,
	                std::initializer_list<Column> columns,
	                bool                          first_call,
	                bool                          border,
	                bool                          separator)
	{
		ImGui::Columns(int(columns.size()), id, border);

		if(separator)
			ImGui::Separator();

		float       offset = 0.0f;
		ImGuiStyle& style  = ImGui::GetStyle();

		for(auto& column : columns) {
			if(first_call) {
				auto width = column.size >= 0 ? column.size
				                              : (ImGui::CalcTextSize(column.header, nullptr, true).x
				                                 + 2 * style.ItemSpacing.x);

				ImGui::SetColumnOffset(-1, offset);
				ImGui::SetColumnWidth(-1, width);

				offset += width;
			}
			ImGui::Text("%s", column.header);
			ImGui::NextColumn();
		}

		if(separator)
			ImGui::Separator();
	}

	float ValueSliderFloat(
	        const char* label, float v, float v_min, float v_max, const char* format, float power)
	{
		SliderFloat(label, &v, v_min, v_max, format, power);
		return v;
	}
} // namespace ImGui


// copied from imgui_stdlib.cpp
struct InputTextCallback_UserData {
	std::string*           Str;
	ImGuiInputTextCallback ChainCallback;
	void*                  ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
	InputTextCallback_UserData* user_data = static_cast<InputTextCallback_UserData*>(data->UserData);
	if(data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		// Resize string callback
		// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
		std::string* str = user_data->Str;
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(std::size_t(data->BufTextLen));
		data->Buf = const_cast<char*>(str->c_str());
	} else if(user_data->ChainCallback) {
		// Forward to user callback, if any
		data->UserData = user_data->ChainCallbackUserData;
		return user_data->ChainCallback(data);
	}
	return 0;
}

bool ImGui::InputText(const char*            label,
                      std::string*           str,
                      ImGuiInputTextFlags    flags,
                      ImGuiInputTextCallback callback,
                      void*                  user_data)
{
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str                   = str;
	cb_user_data.ChainCallback         = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputText(label,
	                 const_cast<char*>(str->c_str()),
	                 str->capacity() + 1,
	                 flags,
	                 InputTextCallback,
	                 &cb_user_data);
}

bool ImGui::InputTextMultiline(const char*            label,
                               std::string*           str,
                               const ImVec2&          size,
                               ImGuiInputTextFlags    flags,
                               ImGuiInputTextCallback callback,
                               void*                  user_data)
{
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str                   = str;
	cb_user_data.ChainCallback         = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputTextMultiline(label,
	                          const_cast<char*>(str->c_str()),
	                          str->capacity() + 1,
	                          size,
	                          flags,
	                          InputTextCallback,
	                          &cb_user_data);
}

bool ImGui::InputTextWithHint(const char*            label,
                              const char*            hint,
                              std::string*           str,
                              ImGuiInputTextFlags    flags,
                              ImGuiInputTextCallback callback,
                              void*                  user_data)
{
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str                   = str;
	cb_user_data.ChainCallback         = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputTextWithHint(label,
	                         hint,
	                         const_cast<char*>(str->c_str()),
	                         str->capacity() + 1,
	                         flags,
	                         InputTextCallback,
	                         &cb_user_data);
}


namespace mirrage::gui {

	namespace {
		struct Font_desc {
			util::Str_id id;
			std::string  aid;
			float        size;
		};

		struct Gui_cfg {
			std::vector<Font_desc> fonts;
		};
		sf2_structDef(Font_desc, id, aid, size);
		sf2_structDef(Gui_cfg, fonts);

		void set_clipboard(void*, const char* text) { SDL_SetClipboardText(text); }

		class Gui_event_filter : public input::Sdl_event_filter {
		  public:
			Gui_event_filter(input::Input_manager& input_mgr)
			  : Sdl_event_filter(input_mgr), _input_mgr(input_mgr)
			{
				_cursors[ImGuiMouseCursor_Arrow]      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
				_cursors[ImGuiMouseCursor_TextInput]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
				_cursors[ImGuiMouseCursor_ResizeAll]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
				_cursors[ImGuiMouseCursor_ResizeNS]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
				_cursors[ImGuiMouseCursor_ResizeEW]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
				_cursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
				_cursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
				_cursors[ImGuiMouseCursor_Hand]       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

				ImGuiIO& io = ImGui::GetIO();
				io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
				io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				io.IniFilename = nullptr;

				// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
				io.KeyMap[ImGuiKey_Tab]        = SDL_SCANCODE_TAB;
				io.KeyMap[ImGuiKey_LeftArrow]  = SDL_SCANCODE_LEFT;
				io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
				io.KeyMap[ImGuiKey_UpArrow]    = SDL_SCANCODE_UP;
				io.KeyMap[ImGuiKey_DownArrow]  = SDL_SCANCODE_DOWN;
				io.KeyMap[ImGuiKey_PageUp]     = SDL_SCANCODE_PAGEUP;
				io.KeyMap[ImGuiKey_PageDown]   = SDL_SCANCODE_PAGEDOWN;
				io.KeyMap[ImGuiKey_Home]       = SDL_SCANCODE_HOME;
				io.KeyMap[ImGuiKey_End]        = SDL_SCANCODE_END;
				io.KeyMap[ImGuiKey_Insert]     = SDL_SCANCODE_INSERT;
				io.KeyMap[ImGuiKey_Delete]     = SDL_SCANCODE_DELETE;
				io.KeyMap[ImGuiKey_Backspace]  = SDL_SCANCODE_BACKSPACE;
				io.KeyMap[ImGuiKey_Space]      = SDL_SCANCODE_SPACE;
				io.KeyMap[ImGuiKey_Enter]      = SDL_SCANCODE_RETURN;
				io.KeyMap[ImGuiKey_Escape]     = SDL_SCANCODE_ESCAPE;
				io.KeyMap[ImGuiKey_A]          = SDL_SCANCODE_A;
				io.KeyMap[ImGuiKey_C]          = SDL_SCANCODE_C;
				io.KeyMap[ImGuiKey_V]          = SDL_SCANCODE_V;
				io.KeyMap[ImGuiKey_X]          = SDL_SCANCODE_X;
				io.KeyMap[ImGuiKey_Y]          = SDL_SCANCODE_Y;
				io.KeyMap[ImGuiKey_Z]          = SDL_SCANCODE_Z;

				io.ClipboardUserData  = this;
				io.SetClipboardTextFn = set_clipboard;
				io.GetClipboardTextFn = +[](void* userdata) -> const char* {
					return reinterpret_cast<Gui_event_filter*>(userdata)->get_clipboard();
				};

#ifdef _WIN32
				SDL_SysWMinfo wmInfo;
				SDL_VERSION(&wmInfo.version);
				SDL_GetWindowWMInfo(_input_mgr.window(), &wmInfo);
				io.ImeWindowHandle = wmInfo.info.win.window;
#endif
			}

			auto get_clipboard() -> char*
			{
				_clipboard_data = {SDL_GetClipboardText(), SDL_free};
				return reinterpret_cast<char*>(_clipboard_data.get());
			}

			void notify_frame_rendered() { _last_frame_rendered = true; }

			void pre_input_events() override
			{
				if(!_last_frame_rendered)
					ImGui::EndFrame();
			}

			void post_input_events() override
			{
				ImGuiIO& io     = ImGui::GetIO();
				auto     window = _input_mgr.window();

				int w, h;
				int display_w, display_h;
				SDL_GetWindowSize(window, &w, &h);
				SDL_GL_GetDrawableSize(window, &display_w, &display_h);
				io.DisplaySize = ImVec2(w, h);
				if(w > 0 && h > 0)
					io.DisplayFramebufferScale =
					        ImVec2(static_cast<float>(display_w) / w, static_cast<float>(display_h) / h);

				// Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
				static const auto frequency    = SDL_GetPerformanceFrequency();
				auto              current_time = SDL_GetPerformanceCounter();
				io.DeltaTime =
				        _time > 0 ? static_cast<float>(static_cast<double>(current_time - _time) / frequency)
				                  : (1.0f / 60.0f);
				_time = current_time;


				if(io.WantSetMousePos)
					SDL_WarpMouseInWindow(
					        window, static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y));
				else
					io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

				int    mx, my;
				Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
				io.MouseDown[0] =
				        _mouse_pressed[0]
				        || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT))
				                   != 0; // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
				io.MouseDown[1]   = _mouse_pressed[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
				io.MouseDown[2]   = _mouse_pressed[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
				_mouse_pressed[0] = _mouse_pressed[1] = _mouse_pressed[2] = false;

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
				SDL_Window* focused_window = SDL_GetKeyboardFocus();
				if(_input_mgr.window() == focused_window) {
					// SDL_GetMouseState() gives mouse position seemingly based on the last window entered/focused(?)
					// The creation of a new windows at runtime and SDL_CaptureMouse both seems to severely mess up with that, so we retrieve that position globally.
					int wx, wy;
					SDL_GetWindowPosition(focused_window, &wx, &wy);
					SDL_GetGlobalMouseState(&mx, &my);
					mx -= wx;
					my -= wy;
					io.MousePos = ImVec2(static_cast<float>(mx), static_cast<float>(my));
				}

				// SDL_CaptureMouse() let the OS know e.g. that our imgui drag outside the SDL window boundaries shouldn't e.g. trigger the OS window resize cursor.
				// The function is only supported from SDL 2.0.4 (released Jan 2016)
				bool any_mouse_button_down = ImGui::IsAnyMouseDown();
				SDL_CaptureMouse(any_mouse_button_down ? SDL_TRUE : SDL_FALSE);
#else
				if(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
					io.MousePos = ImVec2(static_cast<float>(mx), static_cast<float>(my));
#endif

				// update cursor icon
				if(!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) {
					auto imgui_cursor = ImGui::GetMouseCursor();

					if(io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
						// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
						SDL_ShowCursor(SDL_FALSE);
					} else {
						// Show OS mouse cursor
						auto idx = std::size_t(imgui_cursor);
						SDL_SetCursor(_cursors[idx].data
						                      ? _cursors[idx].data
						                      : _cursors[std::size_t(ImGuiMouseCursor_Arrow)].data);
						SDL_ShowCursor(SDL_TRUE);
					}
				}

				ImGui::NewFrame();
				_last_frame_rendered = false;
			}

			void handle_key(bool down, SDL_Scancode key)
			{
				ImGuiIO& io = ImGui::GetIO();

				IM_ASSERT(key >= 0 && key < static_cast<int>(sizeof(io.KeysDown) / sizeof(*io.KeysDown)));
				io.KeysDown[key] = down;
				io.KeyShift      = ((SDL_GetModState() & KMOD_SHIFT) != 0);
				io.KeyCtrl       = ((SDL_GetModState() & KMOD_CTRL) != 0);
				io.KeyAlt        = ((SDL_GetModState() & KMOD_ALT) != 0);
				io.KeySuper      = ((SDL_GetModState() & KMOD_GUI) != 0);
			}

			bool propagate(SDL_Event& evt) override
			{
				ImGuiIO& io = ImGui::GetIO();

				switch(evt.type) {
					case SDL_KEYUP:
					case SDL_KEYDOWN:
						handle_key(evt.type == SDL_KEYDOWN, evt.key.keysym.scancode);
						return evt.key.keysym.sym == SDLK_ESCAPE || !io.WantCaptureKeyboard;

					case SDL_MOUSEBUTTONDOWN:
						if(evt.button.button > 0 && evt.button.button < 4) {
							_mouse_pressed[evt.button.button - 1] = true;
						}
						return !io.WantCaptureMouse;

					case SDL_MOUSEWHEEL:
						if(evt.wheel.x > 0)
							io.MouseWheelH += 1;
						if(evt.wheel.x < 0)
							io.MouseWheelH -= 1;
						if(evt.wheel.y > 0)
							io.MouseWheel += 1;
						if(evt.wheel.y < 0)
							io.MouseWheel -= 1;
						return !io.WantCaptureMouse;

					case SDL_TEXTINPUT: {
						io.AddInputCharactersUTF8(evt.text.text);
						return !io.WantCaptureKeyboard;
					}

					default: return true;
				}
			}

		  private:
			struct Cursor {
				SDL_Cursor* data = nullptr;

				Cursor(SDL_Cursor* data = nullptr) : data(data) {}
				Cursor(const Cursor& c) = delete;
				~Cursor() { SDL_FreeCursor(data); }

				Cursor& operator=(Cursor&& c)
				{
					SDL_FreeCursor(data);
					data   = c.data;
					c.data = nullptr;
					return *this;
				}
				Cursor& operator=(const Cursor& c) = delete;
			};

			input::Input_manager& _input_mgr;
			std::array<bool, 3>   _mouse_pressed       = {false, false, false};
			std::uint64_t         _time                = 0;
			bool                  _last_frame_rendered = true;

			std::array<Cursor, ImGuiMouseCursor_COUNT> _cursors;
			std::unique_ptr<void, void (*)(void*)>     _clipboard_data{nullptr, &SDL_free};
		};
	} // namespace

	namespace detail {
		struct Nk_renderer {
			Gui_renderer_interface& renderer;

			Nk_renderer(Gui_renderer_interface& renderer) : renderer(renderer) {}

			void draw()
			{
				ImGui::Render();
				auto draw_data = ImGui::GetDrawData();

				// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
				auto fb_width  = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
				auto fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
				if(fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount == 0)
					return;

				auto vertex_size = static_cast<std::size_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
				auto index_size  = static_cast<std::size_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);
				if(vertex_size <= 0 || index_size <= 0)
					return;

				auto write_data = [&](std::uint16_t* indices, Gui_vertex* vertices) {
					for(auto&& cmd_list :
					    util::range(draw_data->CmdLists, draw_data->CmdLists + draw_data->CmdListsCount)) {
						memcpy(vertices,
						       cmd_list->VtxBuffer.Data,
						       static_cast<std::size_t>(cmd_list->VtxBuffer.Size) * sizeof(ImDrawVert));
						memcpy(indices,
						       cmd_list->IdxBuffer.Data,
						       static_cast<std::size_t>(cmd_list->IdxBuffer.Size) * sizeof(ImDrawIdx));

						vertices += cmd_list->VtxBuffer.Size;
						indices += cmd_list->IdxBuffer.Size;
					}
				};


				auto size   = glm::vec2(draw_data->DisplaySize.x, draw_data->DisplaySize.y);
				auto offset = glm::vec2(draw_data->DisplayPos.x, draw_data->DisplayPos.y);

				auto projection =
				        glm::ortho(-size.x / 2.f, size.x / 2.f, size.y / 2.f, -size.y / 2.f, -1.f, 1.f);
				projection[1][1] *= -1.f;
				projection = projection
				             * glm::translate(glm::mat4(1.f),
				                              glm::vec3(-size.x / 2 - offset.x, -size.y / 2 - offset.y, 0));

				renderer.prepare_draw(index_size, vertex_size, projection, write_data);
				ON_EXIT { renderer.finalize_draw(); };

				// draw stuff
				// Will project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
				ImVec2 clip_scale =
				        draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

				// Render command lists
				auto idx_offset  = std::uint32_t(0);
				auto vert_offset = std::uint32_t(0);
				for(int n = 0; n < draw_data->CmdListsCount; n++) {
					const ImDrawList* cmd_list = draw_data->CmdLists[n];
					for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
						const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
						if(pcmd->UserCallback) {
							pcmd->UserCallback(cmd_list, pcmd);
						} else {
							// Project scissor/clipping rectangles into framebuffer space
							glm::vec4 clip_rect;
							clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
							clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
							clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
							clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

							if(clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f
							   && clip_rect.w >= 0.0f) {
								renderer.draw_elements(
								        pcmd->TextureId, clip_rect, idx_offset, pcmd->ElemCount, vert_offset);
							}
						}
						idx_offset += pcmd->ElemCount;
					}
					vert_offset += std::uint32_t(cmd_list->VtxBuffer.Size);
				}
			}
		};
	} // namespace detail

	namespace {
		struct Wnk_font_atlas {
			std::unordered_map<util::Str_id, ImFont*> fonts;
			std::shared_ptr<void>                     texture;

			Wnk_font_atlas(asset::Asset_manager& assets, Gui_renderer_interface& renderer)
			{
				ImGuiIO& io = ImGui::GetIO();
				io.Fonts->AddFontDefault();

				auto fonts = std::vector<asset::Ptr<asset::Bytes>>();

				auto cfg                 = ImFontConfig{};
				cfg.FontDataOwnedByAtlas = false;

				auto load_font = [&](const Font_desc& font) {
					assets.load_maybe<asset::Bytes>(font.aid, false).process([&](auto&& data) {
						auto f = io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(data->data()),
						                                        static_cast<int>(data->size()),
						                                        font.size,
						                                        &cfg);
						this->fonts.emplace(font.id, f);
						LOG(plog::debug) << "Loaded font \"" << font.aid << "\" in fontsize " << font.size;
						fonts.emplace_back(std::move(data));
					});
				};

				assets.load_maybe<Gui_cfg>("cfg:gui"_aid, false).process([&](auto& cfg) {
					for(auto& font : cfg->fonts) {
						load_font(font);
					}
				});

				// bake
				unsigned char* pixels;
				int            width, height;
				// TODO: alpha8 would be more compact/faster
				io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
				texture         = renderer.load_texture(width, height, 4, pixels);
				io.Fonts->TexID = texture.get();
			}
		};

		struct Imgui_context {
			Imgui_context()
			{
				ImGui::CreateContext();
				ImGui::GetIO();
				ImGui::StyleColorsDark();
			}
			~Imgui_context() { ImGui::DestroyContext(); }
		};
	} // namespace

	struct Gui::PImpl {
		Imgui_context       ctx;
		Wnk_font_atlas      atlas;
		detail::Nk_renderer renderer;
		Gui_event_filter    input_filter;

		PImpl(asset::Asset_manager& assets, input::Input_manager& input, Gui_renderer_interface& renderer)
		  : atlas(assets, renderer), renderer(renderer), input_filter(input)
		{
		}
		~PImpl() {}
	};


	Gui::Gui(glm::vec4 viewport, asset::Asset_manager& assets, input::Input_manager& input)
	  : _viewport(viewport), _assets(assets), _input(input)
	{
		ref_embedded_assets_mirrage_gui();
	}
	Gui::~Gui() { MIRRAGE_INVARIANT(_renderer == nullptr, "GUI still has a renderer registered (leak?)"); }

	void Gui::_reset_renderer(Gui_renderer_interface* renderer)
	{
		if(_renderer == renderer)
			return;

		if(renderer) {
			MIRRAGE_INVARIANT(_renderer == nullptr,
			                  "Gui already has a different renderer: " << _renderer << "!=" << renderer);
			_impl = std::make_unique<PImpl>(_assets, _input, *renderer);
		} else {
			_impl.reset();
		}

		_renderer = renderer;
	}

	auto Gui::find_font(util::Str_id id) const -> util::maybe<ImFont*>
	{
		if(!ready()) {
			LOG(plog::error) << "No gui renderer instantiated when Gui::draw was called!";
			return util::nothing;
		}

		auto iter = _impl->atlas.fonts.find(id);
		if(iter == _impl->atlas.fonts.end())
			return util::nothing;
		else
			return util::justCopy(iter->second);
	}
	void Gui::viewport(glm::vec4 new_viewport) { _viewport = new_viewport; }

	void Gui_renderer_interface::draw_gui()
	{
		if(_gui) {
			_gui->draw();
		}
	}

	void Gui::draw()
	{
		if(!ready()) {
			LOG(plog::error) << "No gui renderer instantiated when Gui::draw was called!";
			return;
		}

		viewport(_input.viewport());
		_impl->renderer.draw();
		_impl->input_filter.notify_frame_rendered();
	}
	void Gui::start_frame() {}

	auto Gui::load_texture(const asset::AID& aid) -> std::shared_ptr<void>
	{
		MIRRAGE_INVARIANT(_renderer, "No gui renderer instantiated when load_texture was called!");
		return _renderer->load_texture(aid);
	}

} // namespace mirrage::gui
