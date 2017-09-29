#include <mirrage/gui/gui.hpp>

#include <glm/gtx/norm.hpp>


namespace mirrage::gui {

	void begin_menu(nk_context*, int& active) {
		// TODO
	}

	bool menu_button(nk_context* ctx, const char* text, bool enabled) {
		if(!enabled)
			return false;

		nk_layout_row_dynamic(ctx, 80, 1);
		return nk_button_label(ctx, text);
	}

	void end_menu(nk_context*) {
		// TODO
	}
} // namespace mirrage::gui
