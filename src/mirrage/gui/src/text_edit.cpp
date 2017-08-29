#include <mirrage/gui/gui.hpp>


namespace mirrage::gui {

	Text_edit::Text_edit() : _data(nk_text_edit{}) { nk_textedit_init_default(&_data.get_or_throw()); }
	Text_edit::~Text_edit() {
		if(_data.is_some()) {
			nk_textedit_free(&_data.get_or_throw());
		}
	}

	void Text_edit::reset(const std::string& str) {
		INVARIANT(_data.is_some(), "Text_edit is in moved-from state!");

		nk_textedit_free(&_data.get_or_throw());
		nk_textedit_init_default(&_data.get_or_throw());
		nk_str_append_text_char(&_data.get_or_throw().string, str.data(), static_cast<int>(str.length()));
	}

	void Text_edit::get(std::string& str) const {
		INVARIANT(_data.is_some(), "Text_edit is in moved-from state!");

		auto& cstr = _data.get_or_throw().string;

		str.assign(reinterpret_cast<const char*>(nk_str_get_const(&cstr)),
		           static_cast<std::size_t>(cstr.buffer.allocated));
	}

	void Text_edit::update_and_draw(nk_context* ctx, nk_flags type, std::string& str) {
		update_and_draw(ctx, type);
		get(str);
	}

	void Text_edit::update_and_draw(nk_context* ctx, nk_flags type) {
		INVARIANT(_data.is_some(), "Text_edit is in moved-from state!");

		auto ev = nk_edit_buffer(ctx, type, &_data.get_or_throw(), nk_filter_default);

		_active = (ev & NK_EDIT_INACTIVE) != NK_EDIT_INACTIVE;
	}
}
