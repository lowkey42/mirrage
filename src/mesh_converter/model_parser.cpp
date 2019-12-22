#include <sf2/sf2.hpp>

#include "model_parser.hpp"

#include "animation_parser.hpp"
#include "material_parser.hpp"
#include "skeleton_parser.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>

#include <assimp/postprocess.h> // Post processing flags
#include <assimp/scene.h>       // Output data structure
#include <assimp/Importer.hpp>  // C++ importer interface
#include <assimp/ProgressHandler.hpp>

#include <indicators/block_progress_bar.hpp>

#include <glm/gtx/norm.hpp>
#include <gsl/gsl>

#include <fstream>
#include <iostream>


namespace mirrage {

	namespace {
		struct Sub_mesh_data {
			std::uint32_t index_offset;
			std::uint32_t index_count;
			std::string   material_id;

			Sub_mesh_data() = default;
			Sub_mesh_data(std::uint32_t index_offset, std::uint32_t index_count, std::string material_id)
			  : index_offset(index_offset), index_count(index_count), material_id(std::move(material_id))
			{
			}
		};

		auto last_of(const std::string& str, char c)
		{
			auto idx = str.find_last_of(c);
			return idx != std::string::npos ? util::just(idx + 1) : util::nothing;
		}

		auto extract_file_name(const std::string& path)
		{
			auto filename_delim_begin = last_of(path, '/').get_or(last_of(path, '\\').get_or(0));

			auto filename_delim_end = path.find_last_of('.');

			return path.substr(filename_delim_begin, filename_delim_end - filename_delim_begin);
		}

		auto extract_dir(const std::string& path)
		{
			auto filename_delim_end = last_of(path, '/').get_or(last_of(path, '\\').get_or(0));

			return path.substr(0, filename_delim_end - 1);
		}

		struct Bounding_sphere_calculator {
			bool      first_point = true;
			glm::vec3 min_vertex_pos;
			glm::vec3 max_vertex_pos;

			glm::vec3 center;
			float     radius_2 = 0.0f;

			void add_point(glm::vec3 p)
			{
				if(first_point) {
					first_point    = false;
					min_vertex_pos = p;
					max_vertex_pos = p;
					return;
				}

				min_vertex_pos.x = std::min(min_vertex_pos.x, p.x);
				min_vertex_pos.y = std::min(min_vertex_pos.y, p.y);
				min_vertex_pos.z = std::min(min_vertex_pos.z, p.z);

				max_vertex_pos.x = std::max(max_vertex_pos.x, p.x);
				max_vertex_pos.y = std::max(max_vertex_pos.y, p.y);
				max_vertex_pos.z = std::max(max_vertex_pos.z, p.z);
			}
			void start_process() { center = min_vertex_pos + (max_vertex_pos - min_vertex_pos) / 2.f; }
			void process_point(glm::vec3 p) { radius_2 = std::max(radius_2, glm::distance2(p, center)); }
		};

		void add_bone_weight(renderer::Model_rigged_vertex& v, int bone_idx, float weight)
		{
			auto min_weight = 0;
			for(auto i = 1; i < 4; i++) {
				if(v.bone_weights[i] < v.bone_weights[min_weight])
					min_weight = i;
			}

			if(v.bone_weights[min_weight] < weight) {
				v.bone_ids[min_weight]     = bone_idx;
				v.bone_weights[min_weight] = weight;
			}
		}

		struct MyProgressHandler : public Assimp::ProgressHandler {
			indicators::BlockProgressBar* progresss;

			MyProgressHandler(indicators::BlockProgressBar* p) : progresss(p) {}

			bool Update(float p) override
			{
				if(progresss)
					progresss->set_progress(p * 20.f);
				return true;
			}
		};
	} // namespace


	void convert_model(const std::string&           path,
	                   const std::string&           output,
	                   const Mesh_converted_config& cfg,
	                   helper::Progress_container&  progress,
	                   bool                         ansi)
	{
		LOG(plog::info) << "Convert model \"" << path << "\" with output directory \"" << output << "\"";

		auto model_progress = indicators::BlockProgressBar{};

		if(ansi) {
			model_progress.set_postfix_text("Parse model");
			model_progress.set_foreground_color(indicators::Color::RED);
			model_progress.set_progress(0.f);
		}

		auto base_dir   = extract_dir(path);
		auto model_name = extract_file_name(path);

		auto importer = Assimp::Importer();
		importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
		importer.SetProgressHandler(new MyProgressHandler(ansi ? &model_progress : nullptr));

		auto scene = importer.ReadFile(
		        path,
		        aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals | aiProcess_Triangulate
		                | aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials
		                | aiProcess_OptimizeMeshes | aiProcess_ValidateDataStructure
		                | aiProcess_FindDegenerates | aiProcess_FindInvalidData | aiProcess_FlipUVs
		                | aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights | aiProcess_OptimizeGraph
		                | aiProcess_GenUVCoords | aiProcess_TransformUVCoords | aiProcess_SortByPType);

		MIRRAGE_INVARIANT(scene, "Unable to load model '" << path << "': " << importer.GetErrorString());


		if(ansi) {
			model_progress.set_postfix_text("Parse animations");
			model_progress.set_progress(20.f);
		}

		// load skeleton
		auto skeleton = parse_skeleton(model_name, output, *scene, cfg);
		auto rigged   = !skeleton.bones.empty();
		parse_animations(model_name, output, *scene, cfg, skeleton);

		if(ansi) {
			model_progress.set_postfix_text("Parse materials");
			model_progress.set_progress(30.f);
		}

		// load materials
		auto materials = gsl::span<const aiMaterial* const>(scene->mMaterials, scene->mNumMaterials);
		auto loaded_material_ids = std::vector<util::maybe<std::string>>();
		loaded_material_ids.reserve(materials.size());
		for(auto& mat : materials) {
			aiString name;
			if(mat->Get(AI_MATKEY_NAME, name) != aiReturn_SUCCESS) {
				LOG(plog::warning) << "material number " << loaded_material_ids.size() << " has no name!";
				loaded_material_ids.emplace_back(util::nothing);
				continue;
			}

			auto mat_id = std::string(name.C_Str());
			if(cfg.prefix_materials) {
				mat_id = model_name + "_" + mat_id;
			}
			util::to_lower_inplace(mat_id);
			if(!convert_material(mat_id, *mat, base_dir, output, cfg, progress)) {
				LOG(plog::warning) << "Unable to parse material \"" << name.C_Str() << "\"!";
				loaded_material_ids.emplace_back(util::nothing);
				continue;
			}
			loaded_material_ids.emplace_back(std::move(mat_id) + ".msf");
		}

		if(ansi) {
			model_progress.set_progress(60.f);
			model_progress.set_postfix_text("Parse vertex data");
		}

		// calc vertex/index counts
		auto meshes = gsl::span<const aiMesh* const>(scene->mMeshes, scene->mNumMeshes);

		auto index_count  = std::size_t(0);
		auto vertex_count = std::size_t(0);

		for(auto& mesh : meshes) {
			index_count += mesh->mNumFaces * 3;
			vertex_count += mesh->mNumVertices;
		}


		// load meshes
		auto indices    = std::vector<std::uint32_t>();
		auto sub_meshes = std::vector<Sub_mesh_data>();
		sub_meshes.reserve(std::size_t(meshes.size()));
		indices.reserve(index_count);

		auto normal_vertices = std::vector<renderer::Model_vertex>();
		auto rigged_vertices = std::vector<renderer::Model_rigged_vertex>();

		if(rigged)
			rigged_vertices.reserve(vertex_count);
		else
			normal_vertices.reserve(vertex_count);

		auto foreach_vertex_position = [&](auto&& callback) {
			if(rigged)
				for(auto& v : rigged_vertices)
					callback(v.position);
			else
				for(auto& v : normal_vertices)
					callback(v.position);
		};
		auto get_vertex_position = [&](auto idx) {
			if(rigged)
				return rigged_vertices.at(idx).position;
			else
				return normal_vertices.at(idx).position;
		};


		auto traverse = [&](const aiNode* node, auto parent_transform, auto&& recurse) -> void {
			auto transform = parent_transform * to_glm(node->mTransformation);

			for(auto& mesh_idx : gsl::span(node->mMeshes, node->mNumMeshes)) {
				auto mesh = scene->mMeshes[mesh_idx];
				if(mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
					LOG(plog::warning) << "Skipped non-triangle mesh";
					continue;
				}

				const auto first_index = rigged ? rigged_vertices.size() : normal_vertices.size();

				for(auto i = std::size_t(0); i < mesh->mNumVertices; i++) {
					auto v  = transform * glm::vec4{to_glm(mesh->mVertices[i]), 1};
					auto n  = transform * glm::vec4{to_glm(mesh->mNormals[i]), 0};
					auto uv = mesh->mTextureCoords[0]
					                  ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
					                  : glm::vec2(0, 0);

					if(rigged) {
						rigged_vertices.emplace_back(v.x, v.y, v.z, n.x, n.y, n.z, uv.x, uv.y);
					} else
						normal_vertices.emplace_back(v.x, v.y, v.z, n.x, n.y, n.z, uv.x, uv.y);
				}

				if(mesh->mNumFaces > 0) {
					auto& mat = loaded_material_ids.at(mesh->mMaterialIndex);
					if(mat.is_some()) {
						sub_meshes.emplace_back(indices.size(), mesh->mNumFaces * 3, mat.get_or_throw());
					} else {
						LOG(plog::warning) << "Required material is missing/defect!";
					}
				}

				for(auto& face : gsl::span<const aiFace>(mesh->mFaces, mesh->mNumFaces)) {
					if(face.mIndices[0] < mesh->mNumVertices && face.mIndices[1] < mesh->mNumVertices
					   && face.mIndices[2] < mesh->mNumVertices) {
						indices.emplace_back(face.mIndices[0] + first_index);
						indices.emplace_back(face.mIndices[1] + first_index);
						indices.emplace_back(face.mIndices[2] + first_index);
					} else {
						LOG(plog::warning) << "Skipped face with out-of-range indices";
					}
				}

				if(rigged && mesh->HasBones()) {
					// populate bone idx/weights for each new vertex
					for(auto& bone : gsl::span(mesh->mBones, mesh->mNumBones)) {
						auto bone_idx =
						        util::find_maybe(skeleton.bones_by_name, bone->mName.C_Str())
						                .get_or_throw("References bone doesn't exist: ", bone->mName.C_Str());

						for(auto& weight : gsl::span(bone->mWeights, bone->mNumWeights)) {
							auto idx = first_index + weight.mVertexId;
							add_bone_weight(rigged_vertices.at(idx), bone_idx, weight.mWeight);
						}
					}
				}
			}

			for(auto& c : gsl::span(node->mChildren, node->mNumChildren)) {
				recurse(c, transform, recurse);
			}
		};

		traverse(scene->mRootNode, glm::mat4(1), traverse);

		if(rigged) {
			// re-normalize bone weights
			for(auto& v : rigged_vertices) {
				auto sum = glm::dot(v.bone_weights, glm::vec4(1, 1, 1, 1));
				if(sum <= 0.000001f) {
					v.bone_weights = glm::vec4(0, 0, 0, 0);
				} else {
					v.bone_weights /= sum;
				}
			}
		}

		if(ansi) {
			model_progress.set_postfix_text("Write model");
			model_progress.set_progress(80.f);
		}

		// write file
		auto model_out_filename = output + "/models/" + util::to_lower(model_name) + ".mmf";
		auto model_out_file = std::ofstream(model_out_filename, std::ostream::binary | std::ostream::trunc);

		MIRRAGE_INVARIANT(model_out_file.is_open(),
		                  "Unable to open output file \"" << model_out_filename << "\"!");

		auto header = renderer::Model_file_header();
		if(rigged)
			header.vertex_size =
			        gsl::narrow<std::uint32_t>(rigged_vertices.size() * sizeof(decltype(rigged_vertices[0])));
		else
			header.vertex_size =
			        gsl::narrow<std::uint32_t>(normal_vertices.size() * sizeof(decltype(normal_vertices[0])));

		header.index_size    = gsl::narrow<std::uint32_t>(indices.size() * sizeof(std::uint32_t));
		header.submesh_count = gsl::narrow<std::uint32_t>(sub_meshes.size());
		header.flags         = rigged ? 1 : 0;

		// calculate bounding sphere
		auto bounding_sphere = Bounding_sphere_calculator{};
		foreach_vertex_position([&](auto& p) { bounding_sphere.add_point(p); });

		bounding_sphere.start_process();
		foreach_vertex_position([&](auto& p) { bounding_sphere.process_point(p); });

		header.bounding_sphere_radius   = std::sqrt(bounding_sphere.radius_2);
		header.bounding_sphere_offset_x = bounding_sphere.center.x;
		header.bounding_sphere_offset_y = bounding_sphere.center.y;
		header.bounding_sphere_offset_z = bounding_sphere.center.z;

		// write header
		write(model_out_file, header);
		if(rigged) {
			write(model_out_file, std::int32_t(skeleton.bones.size()));
		}

		// write sub meshes
		for(auto& sub_mesh : sub_meshes) {
			write(model_out_file, sub_mesh.index_offset);
			write(model_out_file, sub_mesh.index_count);

			// calculate bounding sphere
			auto bounding_sphere = Bounding_sphere_calculator{};
			for(auto i = 0u; i < sub_mesh.index_count; i++) {
				auto idx = indices.at(i + sub_mesh.index_offset);
				bounding_sphere.add_point(get_vertex_position(idx));
			}

			bounding_sphere.start_process();
			for(auto i = 0u; i < sub_mesh.index_count; i++) {
				auto idx = indices.at(i + sub_mesh.index_offset);
				bounding_sphere.process_point(get_vertex_position(idx));
			}

			write(model_out_file, std::sqrt(bounding_sphere.radius_2));
			write(model_out_file, bounding_sphere.center.x);
			write(model_out_file, bounding_sphere.center.y);
			write(model_out_file, bounding_sphere.center.z);

			auto material_id_length = gsl::narrow<std::uint32_t>(sub_mesh.material_id.size());
			write(model_out_file, material_id_length);
			model_out_file.write(sub_mesh.material_id.data(), material_id_length);
		}

		// write vertices
		if(rigged)
			write(model_out_file, rigged_vertices);
		else
			write(model_out_file, normal_vertices);

		// write indices
		write(model_out_file, indices);

		// write footer
		write(model_out_file, renderer::Model_file_header::type_tag_value);

		if(ansi) {
			model_progress.set_postfix_text("Model Converted");
			model_progress.set_progress(100.0001f);
		}

		LOG(plog::info) << "Bounds: " << header.bounding_sphere_radius << " at "
		                << header.bounding_sphere_offset_x << "/" << header.bounding_sphere_offset_y << "/"
		                << header.bounding_sphere_offset_z;

		for(auto& sub_mesh : sub_meshes) {
			(void) sub_mesh;
			LOG(plog::info) << "Sub-Bounds: " << std::sqrt(bounding_sphere.radius_2) << " at "
			                << bounding_sphere.center.x << "/" << bounding_sphere.center.y << "/"
			                << bounding_sphere.center.z;
		}

		LOG(plog::info) << "Bone count: " << skeleton.bones.size();

		if(cfg.print_animations) {
			auto animations = gsl::span(scene->mAnimations, scene->mNumAnimations);
			for(auto anim : animations) {
				LOG(plog::info) << "Animation for model " << model_name << ": " << anim->mName.C_Str();
			}
		}
	}
} // namespace mirrage
