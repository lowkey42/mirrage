#pragma once

#include <mirrage/renderer/animation.hpp>

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sf2/sf2.hpp>

#include <atomic>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>


namespace mirrage {

	extern std::atomic<std::size_t> parallel_tasks_started;
	extern std::atomic<std::size_t> parallel_tasks_done;

	sf2_enum(Texture_type, albedo, metalness, roughness, normal, emission);

	using Texture_mapping = std::unordered_map<Texture_type, std::vector<int>>;


	struct Mesh_converted_config {
		Texture_mapping texture_mappings{
		        {Texture_type::albedo, {1, 12}}, // aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR
		        {Texture_type::metalness,
		         {3, 11, 15}}, // aiTextureType_AMBIENT, aiTextureType_REFLECTION aiTextureType_METALNESS
		        {Texture_type::roughness,
		         {7,
		          2,
		          16}}, // aiTextureType_SHININESS, aiTextureType_SPECULAR, aiTextureType_DIFFUSE_ROUGHNESS
		        {Texture_type::normal,
		         {6, 13, 5}}, // aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT
		        {Texture_type::emission, {4, 14}}, // aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR
		};

		std::unordered_map<std::string, std::unordered_map<Texture_type, std::string>>
		        material_texture_override;

		std::vector<std::string> empty_bones_to_keep;

		std::string default_output_directory = "output";

		renderer::Skinning_type skinning_type = renderer::Skinning_type::dual_quaternion_skinning;

		bool print_material_info = false;
		bool print_animations    = false;
		bool prefix_materials    = true;
		int  dxt_level           = 4;
		bool trim_bones          = false;
		bool only_animations     = false;
		bool skip_materials      = false;

		float scale = 1.f;

		bool animate_scale       = true;
		bool animate_translation = true;
		bool animate_orientation = true;

		bool use_material_colors = false;
		bool ignore_textures     = false;

		float smooth_normal_angle = 80.f;
		bool  discard_normals     = false;
	};
	sf2_structDef(Mesh_converted_config,
	              texture_mappings,
	              material_texture_override,
	              empty_bones_to_keep,
	              default_output_directory,
	              skinning_type,
	              print_material_info,
	              print_animations,
	              prefix_materials,
	              dxt_level,
	              trim_bones,
	              only_animations,
	              skip_materials,
	              scale,
	              use_material_colors,
	              ignore_textures,
	              smooth_normal_angle,
	              discard_normals);


	template <typename T>
	void write(std::ostream& out, const T& value)
	{
		static_assert(!std::is_pointer<T>::value, "T is a pointer. That is DEFINITLY not what you wanted!");
		out.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	inline void write(std::ostream& out, const std::string& value)
	{
		auto len = gsl::narrow<std::uint32_t>(value.size());
		write(out, len);
		out.write(value.data(), len);
	}

	template <typename T>
	void write(std::ostream& out, const std::vector<T>& value)
	{
		static_assert(!std::is_pointer<T>::value, "T is a pointer. That is DEFINITLY not what you wanted!");

		out.write(reinterpret_cast<const char*>(value.data()), value.size() * sizeof(T));
	}


	inline auto to_glm(aiMatrix4x4 in) -> glm::mat4
	{
		/*
		return glm::transpose(glm::mat4{{in.a1, in.a2, in.a3, in.a4},
		                                {in.b1, in.b2, in.b3, in.b4},
		                                {in.c1, in.c2, in.c3, in.c4},
		                                {in.d1, in.d2, in.d3, in.d4}});
										*/
		return glm::transpose(glm::make_mat4(&in.a1));
	}
	inline auto to_glm(aiVector3D v) { return glm::vec3(v.x, v.y, v.z); }

} // namespace mirrage
