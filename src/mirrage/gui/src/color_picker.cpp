#include <mirrage/gui/gui.hpp>

#include <glm/gtx/norm.hpp>


namespace mirrage::gui {

	namespace {
		bool color_picker(nk_context* ctx, nk_colorf& cf, float width, bool hasAlpha)
		{
			auto c = nk_color{static_cast<nk_byte>(cf.r * 255),
			                  static_cast<nk_byte>(cf.g * 255),
			                  static_cast<nk_byte>(cf.b * 255),
			                  static_cast<nk_byte>(cf.a * 255)};
			if(nk_combo_begin_color(ctx, c, nk_vec2(width, width))) {
				nk_layout_row_dynamic(ctx, 120, 1);
				cf = nk_color_picker(ctx, cf, hasAlpha ? NK_RGBA : NK_RGB);

				nk_layout_row_dynamic(ctx, 14, 1);
				cf.r = nk_propertyf(ctx, "#R:", 0, cf.r, 1, 0.1f, 0.01f);
				cf.g = nk_propertyf(ctx, "#G:", 0, cf.g, 1, 0.1f, 0.01f);
				cf.b = nk_propertyf(ctx, "#B:", 0, cf.b, 1, 0.1f, 0.01f);
				if(hasAlpha) {
					cf.a = nk_propertyf(ctx, "#A:", 0, cf.a, 1, 0.1f, 0.01f);
				}

				nk_combo_end(ctx);

				return true;
			}
			return false;
		}
	} // namespace

	bool color_picker(nk_context* ctx, util::Rgb& color, float width, float factor)
	{
		nk_colorf c = nk_colorf{color.r / factor, color.g / factor, color.b / factor, 1.f};

		if(color_picker(ctx, c, width, false)) {
			auto newColor = util::Rgb{c.r * factor, c.g * factor, c.b * factor};

			if(glm::length2(newColor - color) > 0.0001f) {
				color = newColor;
				return true;
			}
		}
		return false;
	}

	bool color_picker(nk_context* ctx, util::Rgba& color, float width, float factor)
	{
		nk_colorf c = nk_colorf{color.r / factor, color.g / factor, color.b / factor, color.a / factor};

		if(color_picker(ctx, c, width, true)) {
			auto newColor = util::Rgba{c.r * factor, c.g * factor, c.b * factor, c.a * factor};

			if(glm::length2(newColor - color) > 0.0001f) {
				color = newColor;
				return true;
			}
		}
		return false;
	}
} // namespace mirrage::gui
