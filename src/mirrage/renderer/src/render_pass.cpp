#include <mirrage/renderer/render_pass.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/model_comp.hpp>


namespace mirrage::renderer {

	Render_pass_factory::Render_pass_factory() = default;

	auto Render_pass::_mark_subpass(Frame_data& fd) -> Raii_marker
	{
		return {graphic::Queue_debug_label(_renderer.device().context(), fd.main_command_buffer, name()),
		        _renderer.profiler().push(name())};
	}

} // namespace mirrage::renderer
