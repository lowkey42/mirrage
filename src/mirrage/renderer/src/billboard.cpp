#include <mirrage/renderer/billboard.hpp>


namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Billboard_comp& comp)
	{
		state.read_virtual(sf2::vmember("billboards", comp.billboards));

		for(auto& b : comp.billboards) {
			if(!b.material || b.material.aid() != b.material_aid) {
				b.material = state.assets.load<Material>(b.material_aid);
			}
			b.material_aid = {}; // reset aid to safe memory
		}
	}
	void save_component(ecs::Serializer& state, const Billboard_comp& comp)
	{
		auto billboards = comp.billboards;
		for(auto& b : billboards)
			b.material_aid = b.material.aid().str();

		state.write_virtual(sf2::vmember("billboards", billboards));
	}

	auto construct_push_constants(const Billboard& bb,
	                              const glm::vec3& pos,
	                              const glm::mat4& view,
	                              const glm::vec4& viewport) -> Billboard_push_constants
	{
		auto pcs     = Billboard_push_constants{};
		pcs.position = glm::vec4(bb.offset + pos, 1.f);
		if(!bb.absolute_screen_space) {
			pcs.position = view * pcs.position;
			pcs.position /= pcs.position.w;
		} else {
			pcs.position.x = pcs.position.x / (viewport.z / 2) - 1.f;
			pcs.position.y = -(pcs.position.y / (viewport.w / 2) - 1.f);
		}
		pcs.position.w = bb.vertical_rotation ? 1.f : 0.f;
		pcs.size       = glm::vec4(bb.size,
                             bb.absolute_screen_space ? 1.f : 0.f,
                             bb.fixed_screen_size || bb.absolute_screen_space ? 1.f : 0.f);
		if(bb.absolute_screen_space || bb.fixed_screen_size) {
			pcs.size.x /= viewport.z / 2.f;
			pcs.size.y /= viewport.w / 2.f;
		}
		pcs.clip_rect      = bb.clip_rect;
		pcs.color          = bb.color;
		pcs.emissive_color = bb.emissive_color;
		pcs.emissive_color.a /= 10000.0f;

		return pcs;
	}

} // namespace mirrage::renderer

namespace mirrage::asset {
	auto Loader<renderer::Billboard>::load(istream in) -> async::task<renderer::Billboard>
	{
		auto& assets = in.manager();
		auto  data   = Loader<renderer::Billboard_data>::load(std::move(in));

		auto mat = assets.load<renderer::Material>(data.material_aid);
		return mat.internal_task().then([data = std::move(data), mat = std::move(mat)](auto&) mutable {
			return renderer::Billboard{std::move(data), std::move(mat)};
		});
	}

} // namespace mirrage::asset
