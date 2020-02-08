#include <mirrage/renderer/decal.hpp>

#include <glm/gtc/packing.hpp>


namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Decal_comp& comp)
	{
		state.read_virtual(sf2::vmember("decals", comp.decals));

		for(auto& b : comp.decals) {
			if(!b.material || b.material.aid() != b.material_aid) {
				b.material = state.assets.load<Material>(b.material_aid);
			}
			b.material_aid = {}; // reset aid to safe memory
		}
	}
	void save_component(ecs::Serializer& state, const Decal_comp& comp)
	{
		auto decals = comp.decals;
		for(auto& b : decals)
			b.material_aid = b.material.aid().str();

		state.write_virtual(sf2::vmember("decals", decals));
	}

	namespace {
		auto near_zero(float v) { return std::abs(v) < 0.0001f; }
	} // namespace
	auto construct_push_constants(const Decal& bb, const glm::mat4& model) -> Decal_push_constants
	{
		auto pcs       = Decal_push_constants{};
		pcs.model_view = model * glm::translate(glm::mat4(1), bb.offset) * glm::toMat4(bb.rotation)
		                 * glm::scale(glm::mat4(1), glm::vec3(bb.size.x, bb.size.y, bb.thickness));
		pcs.model_view_inv = glm::inverse(pcs.model_view);

		MIRRAGE_INVARIANT(near_zero(pcs.model_view[0][3]) && near_zero(pcs.model_view[1][3])
		                          && near_zero(pcs.model_view[2][3]) && near_zero(pcs.model_view[3][3] - 1.f),
		                  "Invalid matrix.");

		MIRRAGE_INVARIANT(near_zero(pcs.model_view_inv[0][3]) && near_zero(pcs.model_view_inv[1][3])
		                          && near_zero(pcs.model_view_inv[2][3])
		                          && near_zero(pcs.model_view_inv[3][3] - 1.f),
		                  "Invalid matrix.");

		auto emissive_color = bb.emissive_color;
		emissive_color.a /= 10000.0f;
		if(!bb.material->has_emission())
			emissive_color = util::Rgba{0, 0, 0, 0};

		pcs.model_view_inv[0][3] = emissive_color.r;
		pcs.model_view_inv[1][3] = emissive_color.g;
		pcs.model_view_inv[2][3] = emissive_color.b;
		pcs.model_view_inv[3][3] = emissive_color.a;

		auto tmp = glm::uint();

		tmp = glm::packSnorm2x16(glm::vec2{bb.clip_rect.r, bb.clip_rect.g} / 10.f);
		std::memcpy(&pcs.model_view[0][3], &tmp, sizeof(tmp));

		tmp = glm::packSnorm2x16(glm::vec2{bb.clip_rect.b, bb.clip_rect.a} / 10.f);
		std::memcpy(&pcs.model_view[1][3], &tmp, sizeof(tmp));

		auto color = bb.material->has_albedo() ? bb.color : util::Rgba{0, 0, 0, 0};
		tmp        = glm::packUnorm2x16(glm::vec2{color.r, color.g});
		std::memcpy(&pcs.model_view[2][3], &tmp, sizeof(tmp));

		tmp = glm::packUnorm2x16(glm::vec2{color.b, color.a});
		std::memcpy(&pcs.model_view[3][3], &tmp, sizeof(tmp));

		return pcs;
	}

} // namespace mirrage::renderer

namespace mirrage::asset {
	auto Loader<renderer::Decal>::load(istream in) -> async::task<renderer::Decal>
	{
		auto& assets = in.manager();
		auto  data   = Loader<renderer::Decal_data>::load(std::move(in));

		auto mat = assets.load<renderer::Material>(data.material_aid);
		return mat.internal_task().then([data = std::move(data), mat = std::move(mat)](auto&) mutable {
			return renderer::Decal{std::move(data), std::move(mat)};
		});
	}

} // namespace mirrage::asset
