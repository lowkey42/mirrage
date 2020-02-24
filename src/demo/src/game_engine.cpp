#include "game_engine.hpp"

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/pass/animation_pass.hpp>
#include <mirrage/renderer/pass/billboard_pass.hpp>
#include <mirrage/renderer/pass/blit_pass.hpp>
#include <mirrage/renderer/pass/bloom_pass.hpp>
#include <mirrage/renderer/pass/clear_pass.hpp>
#include <mirrage/renderer/pass/debug_draw_pass.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>
#include <mirrage/renderer/pass/depth_of_field_pass.hpp>
#include <mirrage/renderer/pass/frustum_culling_pass.hpp>
#include <mirrage/renderer/pass/gen_mipmap_pass.hpp>
#include <mirrage/renderer/pass/gi_pass.hpp>
#include <mirrage/renderer/pass/gui_pass.hpp>
#include <mirrage/renderer/pass/particle_pass.hpp>
#include <mirrage/renderer/pass/shadowmapping_pass.hpp>
#include <mirrage/renderer/pass/ssao_pass.hpp>
#include <mirrage/renderer/pass/taa_pass.hpp>
#include <mirrage/renderer/pass/tone_mapping_pass.hpp>
#include <mirrage/renderer/pass/transparent_pass.hpp>


namespace mirrage {

	Game_engine::Game_engine(const std::string&       org,
	                         const std::string&       title,
	                         util::maybe<std::string> base_dir,
	                         std::uint32_t            version_major,
	                         std::uint32_t            version_minor,
	                         bool                     debug,
	                         int                      argc,
	                         char**                   argv,
	                         char**                   env)
	  : Engine(org, title, std::move(base_dir), version_major, version_minor, debug, false, argc, argv, env)
	  , _debug_ui(assets(), gui(), bus())
	  , _renderer_factory(renderer::make_deferred_renderer_factory<renderer::Frustum_culling_pass,
	                                                               renderer::Particle_pass,
	                                                               renderer::Animation_pass,
	                                                               renderer::Shadowmapping_pass,
	                                                               renderer::Clear_pass,
	                                                               renderer::Deferred_pass,
	                                                               renderer::Gen_mipmap_pass,
	                                                               renderer::Ssao_pass,
	                                                               renderer::Gi_pass,
	                                                               renderer::Transparent_pass,
	                                                               renderer::Taa_pass,
	                                                               renderer::Depth_of_field_pass,
	                                                               renderer::Bloom_pass,
	                                                               renderer::Tone_mapping_pass,
	                                                               renderer::Billboard_pass,
	                                                               renderer::Debug_draw_pass,
	                                                               renderer::Blit_pass,
	                                                               renderer::Gui_pass>(*this, window()))
	  , _global_render(_renderer_factory->create_renderer<renderer::Gui_pass>())
	  , _render_pass_mask(_renderer_factory->all_passes_mask())
	{
		util::erase_fast(_render_pass_mask, renderer::render_pass_id_of<renderer::Clear_pass>());
		util::erase_fast(_render_pass_mask, renderer::render_pass_id_of<renderer::Gui_pass>());
	}

	Game_engine::~Game_engine()
	{
		screens().clear(); // destroy all screens before the engine
	}

	void Game_engine::_on_post_frame(util::Time dt)
	{
		_debug_ui.draw();

		_global_render->update(dt);
		_global_render->draw();
		_renderer_factory->finish_frame();
	}

} // namespace mirrage
