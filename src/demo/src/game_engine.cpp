#include "game_engine.hpp"

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/pass/animation_pass.hpp>
#include <mirrage/renderer/pass/billboard_pass.hpp>
#include <mirrage/renderer/pass/blit_pass.hpp>
#include <mirrage/renderer/pass/bloom_pass.hpp>
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
	  , _renderer_factory(std::make_unique<renderer::Deferred_renderer_factory>(
	            *this,
	            window(),
	            util::make_vector(renderer::make_pass_factory<renderer::Frustum_culling_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Particle_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Animation_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Shadowmapping_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Deferred_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gen_mipmap_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Ssao_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gi_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Taa_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Depth_of_field_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Bloom_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Tone_mapping_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Billboard_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Debug_draw_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Blit_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gui_pass_factory>())))
	  , _global_render(_renderer_factory->create_renderer<renderer::Gui_pass_factory>())
	  , _render_pass_mask(_renderer_factory->all_passes_mask())
	{
		util::erase_fast(_render_pass_mask, renderer::render_pass_id_of<renderer::Gui_pass_factory>());
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
