#define NK_IMPLEMENTATION

#include <mirrage/gui/gui.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/input/events.hpp>
#include <mirrage/input/input_manager.hpp>

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <sf2/sf2.hpp>

#include <string>


namespace mirrage::gui {

	namespace {
		struct Nk_renderer;

		struct Font_desc {
			std::string aid;
			float       size;
			bool        default_font = false;
		};

		struct Gui_cfg {
			std::vector<Font_desc> fonts;
		};
		sf2_structDef(Font_desc, aid, size, default_font);
		sf2_structDef(Gui_cfg, fonts);

		void nk_sdl_clipbard_paste(nk_handle, struct nk_text_edit* edit) {
			auto text = SDL_GetClipboardText();
			if(text) {
				nk_textedit_paste(edit, text, nk_strlen(text));
			}
		}

		void nk_sdl_clipbard_copy(nk_handle, const char* text, int len) {
			if(len <= 0)
				return;

			auto str = std::string(text, static_cast<std::size_t>(len));
			SDL_SetClipboardText(str.c_str());
		}

		auto sdl_to_nk_mb(Uint8 button) -> util::maybe<nk_buttons> {
			switch(button) {
				case SDL_BUTTON_LEFT: return NK_BUTTON_LEFT;
				case SDL_BUTTON_MIDDLE: return NK_BUTTON_MIDDLE;
				case SDL_BUTTON_RIGHT: return NK_BUTTON_RIGHT;
				default: return util::nothing;
			}
		}

		class Gui_event_filter : public input::Sdl_event_filter {
		  public:
			Gui_event_filter(input::Input_manager& input_mgr,
			                 nk_context*           ctx,
			                 const glm::vec4&      viewport,
			                 const glm::mat4&      ui_matrix)
			  : Sdl_event_filter(input_mgr)
			  , _input_mgr(input_mgr)
			  , _ctx(ctx)
			  , _viewport(viewport)
			  , _ui_matrix(ui_matrix) {}

			void pre_input_events() override {
				_grab_clicks = nk_window_is_any_hovered(_ctx);
				_grab_inputs = nk_item_is_any_active(_ctx);

				nk_input_begin(_ctx);
			}
			void post_input_events() override { nk_input_end(_ctx); }

			bool handle_key(bool down, SDL_Keycode key) {
				const auto state = SDL_GetKeyboardState(nullptr);
				const auto ctrl  = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];

				switch(key) {
					case SDLK_RSHIFT:
					case SDLK_LSHIFT: nk_input_key(_ctx, NK_KEY_SHIFT, down); return false;

					case SDLK_DELETE: nk_input_key(_ctx, NK_KEY_DEL, down); return false;

					case SDLK_RETURN: nk_input_key(_ctx, NK_KEY_ENTER, down); return false;

					case SDLK_TAB: nk_input_key(_ctx, NK_KEY_TAB, down); return false;

					case SDLK_BACKSPACE: nk_input_key(_ctx, NK_KEY_BACKSPACE, down); return false;

					case SDLK_HOME:
						nk_input_key(_ctx, NK_KEY_TEXT_START, down);
						nk_input_key(_ctx, NK_KEY_SCROLL_START, down);
						return false;
					case SDLK_END:
						nk_input_key(_ctx, NK_KEY_TEXT_END, down);
						nk_input_key(_ctx, NK_KEY_SCROLL_END, down);
						return false;

					case SDLK_PAGEDOWN: nk_input_key(_ctx, NK_KEY_SCROLL_DOWN, down); return false;
					case SDLK_PAGEUP: nk_input_key(_ctx, NK_KEY_SCROLL_UP, down); return false;

					case SDLK_z: nk_input_key(_ctx, NK_KEY_TEXT_UNDO, down && ctrl); return false;
					case SDLK_y: nk_input_key(_ctx, NK_KEY_TEXT_REDO, down && ctrl); return false;

					case SDLK_c: nk_input_key(_ctx, NK_KEY_COPY, down && ctrl); return false;
					case SDLK_x: nk_input_key(_ctx, NK_KEY_CUT, down && ctrl); return false;
					case SDLK_v: nk_input_key(_ctx, NK_KEY_PASTE, down && ctrl); return false;

					case SDLK_b: nk_input_key(_ctx, NK_KEY_TEXT_LINE_START, down && ctrl); return false;
					case SDLK_e: nk_input_key(_ctx, NK_KEY_TEXT_LINE_END, down && ctrl); return false;

					case SDLK_LEFT:
						nk_input_key(_ctx, NK_KEY_TEXT_WORD_LEFT, down && ctrl);
						nk_input_key(_ctx, NK_KEY_LEFT, down && !ctrl);
						return false;

					case SDLK_RIGHT:
						nk_input_key(_ctx, NK_KEY_TEXT_WORD_RIGHT, down && ctrl);
						nk_input_key(_ctx, NK_KEY_RIGHT, down && !ctrl);
						return false;

					default: return true;
				}
			}

			bool propagate(SDL_Event& evt) override {
				switch(evt.type) {
					case SDL_KEYUP:
					case SDL_KEYDOWN:
						if(!_grab_inputs)
							return true;

						handle_key(evt.type == SDL_KEYDOWN, evt.key.keysym.sym);
						return evt.key.keysym.sym == SDLK_ESCAPE;

					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
						sdl_to_nk_mb(evt.button.button).process([&](auto button) {
							auto p = glm::unProject(glm::vec3{evt.button.x, evt.button.y, 0.99f},
							                        glm::mat4(1),
							                        _ui_matrix,
							                        _viewport);
							nk_input_button(_ctx,
							                button,
							                static_cast<int>(p.x),
							                static_cast<int>(p.y),
							                evt.type == SDL_MOUSEBUTTONDOWN);
						});
						return !_grab_clicks;

					case SDL_MOUSEMOTION: {
						if(!_input_mgr.capture_mouse()) {
							auto p = glm::unProject(glm::vec3{evt.motion.x, evt.motion.y, 0.99f},
							                        glm::mat4(1),
							                        _ui_matrix,
							                        _viewport);
							nk_input_motion(_ctx, static_cast<int>(p.x), static_cast<int>(p.y));
						}
						return true; //< never swallow mouse-move
					}

					case SDL_MOUSEWHEEL:
						if(!_grab_clicks)
							return true;

						nk_input_scroll(
						        _ctx,
						        nk_vec2(static_cast<float>(evt.wheel.x), static_cast<float>(evt.wheel.y)));
						return false;

					case SDL_TEXTINPUT: {
						if(!_grab_inputs)
							return true;

						nk_glyph glyph;
						memcpy(glyph, evt.text.text, NK_UTF_SIZE);
						nk_input_glyph(_ctx, glyph);
						return false;
					}

					default: return true;
				}
			}

		  private:
			input::Input_manager& _input_mgr;
			nk_context*           _ctx;
			const glm::vec4&      _viewport;
			const glm::mat4&      _ui_matrix;
			bool                  _grab_clicks = false;
			bool                  _grab_inputs = false;
		};

		struct Wnk_Context {
			nk_context ctx;

			Wnk_Context() {
				nk_init_default(&ctx, nullptr);
				ctx.clip.copy     = nk_sdl_clipbard_copy;
				ctx.clip.paste    = nk_sdl_clipbard_paste;
				ctx.clip.userdata = nk_handle_ptr(nullptr);
			}
			~Wnk_Context() { nk_free(&ctx); }
		};
		struct Wnk_Buffer {
			nk_buffer buffer;

			Wnk_Buffer() { nk_buffer_init_default(&buffer); }
			~Wnk_Buffer() { nk_buffer_free(&buffer); }
		};

		template <class T>
		auto get_buffer_data_ref(nk_buffer& src) -> gsl::span<const T> {
			nk_memory_status info;
			nk_buffer_info(&info, &src);

			static_assert(sizeof(T) < 4 || sizeof(T) % 4 == 0, "T is not 4 bytes alligned");

			auto size = info.allocated;
			if(size % 4 != 0) {
				auto align_diff = (4 - size % 4);
				MIRRAGE_INVARIANT(align_diff + info.allocated <= info.size,
				                  "Can't align buffer: " << info.allocated << " / " << info.size);

				size += align_diff;
			}

			return gsl::span<const T>(reinterpret_cast<const T*>(nk_buffer_memory_const(&src)),
			                          size / sizeof(T));
		}
	} // namespace

	namespace detail {
		struct Nk_renderer {
			Gui_renderer& renderer;

			Wnk_Buffer           commands;
			nk_draw_null_texture null_tex;
			Wnk_Buffer           vbo;
			Wnk_Buffer           ibo;


			Nk_renderer(Gui_renderer& renderer) : renderer(renderer) {}

			void draw(nk_context& ctx, glm::vec4 viewport, glm::vec2 screen_size, const glm::mat4& ui_matrix) {

				glm::vec2 scale = glm::vec2(viewport.z - viewport.x, viewport.w - viewport.y) / screen_size;

				// flush nk stuff to buffers
				nk_convert_config config;
				memset(&config, 0, sizeof(config));
				config.global_alpha         = 1.0f;
				config.shape_AA             = NK_ANTI_ALIASING_ON;
				config.line_AA              = NK_ANTI_ALIASING_ON;
				config.circle_segment_count = 22;
				config.curve_segment_count  = 22;
				config.arc_segment_count    = 22;
				config.null                 = null_tex;

				constexpr struct nk_draw_vertex_layout_element vertex_layout[] = {
				        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(Gui_vertex, position)},
				        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(Gui_vertex, uv)},
				        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(Gui_vertex, color)},
				        {NK_VERTEX_LAYOUT_END}};
				config.vertex_layout    = vertex_layout;
				config.vertex_size      = sizeof(struct Gui_vertex);
				config.vertex_alignment = NK_ALIGNOF(struct Gui_vertex);

				nk_buffer_clear(&vbo.buffer);
				nk_buffer_clear(&ibo.buffer);
				nk_buffer_clear(&commands.buffer);
				nk_convert(&ctx, &commands.buffer, &vbo.buffer, &ibo.buffer, &config);

				auto indices  = get_buffer_data_ref<std::uint16_t>(ibo.buffer);
				auto vertices = get_buffer_data_ref<Gui_vertex>(vbo.buffer);

				if(!vertices.empty() && !indices.empty()) {
					renderer.prepare_draw(indices, vertices, ui_matrix);

					ON_EXIT { renderer.finalize_draw(); };

					// draw stuff

					auto cmd    = static_cast<const nk_draw_command*>(nullptr);
					int  offset = 0;
					nk_draw_foreach(cmd, &ctx, &commands.buffer) {
						if(cmd->elem_count == 0)
							continue;

						auto clip = glm::vec4{glm::clamp(cmd->clip_rect.x * scale.x, viewport.x, viewport.z),
						                      glm::clamp(cmd->clip_rect.y * scale.y, viewport.y, viewport.w),
						                      glm::clamp(cmd->clip_rect.w * scale.x, viewport.x, viewport.z),
						                      glm::clamp(cmd->clip_rect.h * scale.y, viewport.y, viewport.w)};

						renderer.draw_elements(cmd->texture.id, clip, offset, cmd->elem_count);

						offset += cmd->elem_count;
					}
				}

				//nk_clear(&ctx);
			}
		};
	} // namespace detail

	namespace {
		struct Wnk_font_atlas {
			nk_font_atlas  atlas;
			const uint8_t* data;
			int            width;
			int            height;

			Wnk_font_atlas(asset::Asset_manager& assets) {
				nk_font_atlas_init_default(&atlas);
				nk_font_atlas_begin(&atlas);

				assets.load_maybe<Gui_cfg>("cfg:gui"_aid, false).process([&](auto& cfg) {
					for(auto& font : cfg->fonts) {
						assets.load_maybe<asset::Bytes>(font.aid, false).process([&](auto&& data) {
							auto f = nk_font_atlas_add_from_memory(
							        &atlas, const_cast<char*>(data->data()), data->size(), font.size, nullptr);
							if(font.default_font) {
								atlas.default_font = f;
							}
							MIRRAGE_DEBUG("Loaded font \"" << font.aid << "\" in fontsize " << font.size);
						});
					}
				});

				data = static_cast<const uint8_t*>(
				        nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32));
			}
			void post_init(nk_context& ctx, nk_draw_null_texture& null_tex, struct nk_image tex) {
				data   = nullptr;
				width  = 0;
				height = 0;

				nk_font_atlas_end(&atlas, tex.handle, &null_tex); // frees data
				if(atlas.default_font)
					nk_style_set_font(&ctx, &atlas.default_font->handle);
			}

			~Wnk_font_atlas() { nk_font_atlas_clear(&atlas); }
		};

		glm::vec2 normalize_screen_size(int window_width, int window_height, int target_height) {
			auto width  = static_cast<float>(window_width);
			auto height = static_cast<float>(window_height);

			if(width / height <= 5 / 3.f) { // special case for weird resolutions
				target_height *= 1.25f;
			}

			if(width < height) { // special case for portrait-mode
				float vheight = std::round(height * (static_cast<float>(target_height)) / width);
				return {target_height, vheight};

			} else {
				float vwidth = std::round(width * (static_cast<float>(target_height)) / height);
				return {vwidth, target_height};
			}
		}

		auto build_ui_mat(glm::vec2 size) {
			auto m = glm::ortho(-size.x / 2.f, size.x / 2.f, size.y / 2.f, -size.y / 2.f, -1.f, 1.f);
			m[1][1] *= -1.f;

			return m * glm::translate(glm::mat4(1.f), glm::vec3(-size.x / 2, -size.y / 2, 0));
		}
	} // namespace

	struct Gui::PImpl {
		glm::vec4                        viewport;
		glm::vec2                        screen_size;
		glm::mat4                        ui_matrix;
		Wnk_Context                      ctx;
		Wnk_Buffer                       buffer;
		detail::Nk_renderer              renderer;
		Wnk_font_atlas                   atlas;
		std::shared_ptr<struct nk_image> atlas_tex;
		Gui_event_filter                 input_filter;

		PImpl(glm::vec4             viewport,
		      asset::Asset_manager& assets,
		      input::Input_manager& input,
		      Gui_renderer&         renderer)
		  : renderer(renderer)
		  , atlas(assets)
		  , atlas_tex(renderer.load_texture(atlas.width, atlas.height, 4, atlas.data))
		  , input_filter(input, &ctx.ctx, this->viewport, ui_matrix) {

			change_viewport(viewport);

			atlas.post_init(ctx.ctx, this->renderer.null_tex, *atlas_tex);

			ctx.ctx.style.window.background        = nk_rgba(0, 0, 0, 240);
			ctx.ctx.style.window.fixed_background  = nk_style_item_color(nk_rgba(0, 0, 0, 240));
			ctx.ctx.style.window.header.normal     = nk_style_item_color(nk_rgba(5, 5, 5, 255));
			ctx.ctx.style.window.header.hover      = nk_style_item_color(nk_rgba(5, 5, 5, 255));
			ctx.ctx.style.window.header.active     = nk_style_item_color(nk_rgba(5, 5, 5, 255));
			ctx.ctx.style.window.border_color      = nk_rgb(0, 0, 0);
			ctx.ctx.style.window.menu_border_color = nk_rgb(0, 0, 0);
		}
		~PImpl() {}

		void change_viewport(glm::vec4 new_viewport) {
			viewport    = new_viewport;
			screen_size = normalize_screen_size(static_cast<int>(viewport.z - viewport.x),
			                                    static_cast<int>(viewport.w - viewport.y),
			                                    720);
			ui_matrix   = build_ui_mat(screen_size);
		}
	};

	Gui::Gui(glm::vec4                        viewport,
	         asset::Asset_manager&            assets,
	         input::Input_manager&            input,
	         util::tracking_ptr<Gui_renderer> renderer)
	  : _viewport(viewport)
	  , _assets(assets)
	  , _input(input)
	  , _renderer(renderer)
	  , _last_renderer(_renderer.get()) {

		_init();
	}
	Gui::~Gui() {
		if(_renderer) {
			_renderer->_gui = nullptr;
		}
	}

	void Gui::_init() {
		_last_renderer = _renderer.get();
		if(_last_renderer) {
			_impl                = std::make_unique<PImpl>(_viewport, _assets, _input, *_last_renderer);
			_last_renderer->_gui = this;

		} else {
			MIRRAGE_WARN("Gui initialized without a valid renderer. Nothing will be drawn!");
		}
	}

	void Gui::viewport(glm::vec4 new_viewport) {
		_viewport = new_viewport;
		_impl->change_viewport(new_viewport);
	}

	void Gui_renderer::draw_gui() {
		if(_gui) {
			_gui->draw();
		}
	}

	void Gui::draw() {
		if(_renderer.modified(_last_renderer)) {
			_init();
		}

		if(!_impl) {
			MIRRAGE_INFO("no impl");
			return;
		}

		viewport(_input.viewport());

		_impl->renderer.draw(_impl->ctx.ctx, _impl->viewport, _impl->screen_size, _impl->ui_matrix);
	}
	void Gui::start_frame() { nk_clear(&_impl->ctx.ctx); }

	auto Gui::ctx() -> nk_context* {
		if(_renderer.modified(_last_renderer)) {
			_init();
		}

		MIRRAGE_INVARIANT(_impl, "Not initialized when nk_context was requested!");

		return &_impl->ctx.ctx;
	}

	auto Gui::centered(int width, int height) -> struct nk_rect {
		return nk_rect(_impl->screen_size.x / 2.f - width / 2.f,
		               _impl->screen_size.y / 2.f - height / 2.f,
		               width,
		               height);
	} auto Gui::centered_left(int width, int height) -> struct nk_rect {
		return nk_rect(0, _impl->screen_size.y / 2.f - height / 2.f, width, height);
	} auto Gui::centered_right(int width, int height) -> struct nk_rect {
		return nk_rect(
		        _impl->screen_size.x - width, _impl->screen_size.y / 2.f - height / 2.f, width, height);
	}
} // namespace mirrage::gui
