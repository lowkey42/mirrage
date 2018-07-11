#include "game_engine.hpp"

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/pass/blit_pass.hpp>
#include <mirrage/renderer/pass/bloom_pass.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>
#include <mirrage/renderer/pass/frustum_culling_pass.hpp>
#include <mirrage/renderer/pass/gen_mipmap_pass.hpp>
#include <mirrage/renderer/pass/gi_pass.hpp>
#include <mirrage/renderer/pass/gui_pass.hpp>
#include <mirrage/renderer/pass/shadowmapping_pass.hpp>
#include <mirrage/renderer/pass/ssao_pass.hpp>
#include <mirrage/renderer/pass/taa_pass.hpp>
#include <mirrage/renderer/pass/tone_mapping_pass.hpp>


namespace mirrage {

	Game_engine::Game_engine(const std::string& org,
	                         const std::string& title,
	                         std::uint32_t      version_major,
	                         std::uint32_t      version_minor,
	                         bool               debug,
	                         int                argc,
	                         char**             argv,
	                         char**             env)
	  : Engine(org, title, version_major, version_minor, debug, false, argc, argv, env)
	  , _renderer_factory(std::make_unique<renderer::Deferred_renderer_factory>(
	            *this,
	            window(),
	            util::make_vector(renderer::make_pass_factory<renderer::Frustum_culling_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Shadowmapping_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Deferred_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gen_mipmap_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Ssao_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gi_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Taa_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Bloom_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Tone_mapping_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Blit_pass_factory>(),
	                              renderer::make_pass_factory<renderer::Gui_pass_factory>())))
	{
	}

	Game_engine::~Game_engine()
	{
		screens().clear(); // destroy all screens before the engine
	}

	void Game_engine::_on_post_frame(util::Time) { _renderer_factory->finish_frame(); }

} // namespace mirrage
