#include <sf2/sf2.hpp>

#include "model_parser.hpp"

#include "material_parser.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/log.hpp>

#include <assimp/postprocess.h> // Post processing flags
#include <assimp/scene.h>       // Output data structure
#include <assimp/Importer.hpp>  // C++ importer interface

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

		template <typename T>
		void write(std::ostream& out, const T& value)
		{
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");
			out.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		template <typename T>
		void write(std::ostream& out, const std::vector<T>& value)
		{
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");

			out.write(reinterpret_cast<const char*>(value.data()), value.size() * sizeof(T));
		}
	} // namespace


	void convert_model(const std::string& path, const std::string& output)
	{
		LOG(plog::info) << "Convert model \"" << path << "\" with output directory \"" << output << "\"";

		auto base_dir   = extract_dir(path);
		auto model_name = extract_file_name(path);

		auto importer = Assimp::Importer();

		auto scene = importer.ReadFile(
		        path,
		        aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals | aiProcess_Triangulate
		                | aiProcess_PreTransformVertices | aiProcess_ImproveCacheLocality
		                | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes
		                | aiProcess_ValidateDataStructure | aiProcess_FlipUVs | aiProcess_FixInfacingNormals);

		MIRRAGE_INVARIANT(scene, "Unable to load model '" << path << "': " << importer.GetErrorString());

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

			auto mat_id = model_name + "_" + name.C_Str();
			if(!convert_material(mat_id, *mat, base_dir, output)) {
				LOG(plog::warning) << "Unable to parse material \"" << name.C_Str() << "\"!";
				loaded_material_ids.emplace_back(util::nothing);
				continue;
			}
			loaded_material_ids.emplace_back(std::move(mat_id) + ".msf");
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
		auto vertices   = std::vector<renderer::Model_vertex>();
		auto sub_meshes = std::vector<Sub_mesh_data>();
		sub_meshes.reserve(meshes.size());
		indices.reserve(index_count);
		vertices.reserve(vertex_count);

		for(auto& mesh : meshes) {
			const auto first_index = vertices.size();

			for(auto i = std::size_t(0); i < mesh->mNumVertices; i++) {
				auto& v  = mesh->mVertices[i];
				auto& n  = mesh->mNormals[i];
				auto& uv = mesh->mTextureCoords[0][i];
				vertices.emplace_back(v.x, v.y, v.z, n.x, n.y, n.z, uv.x, uv.y);
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
				indices.emplace_back(face.mIndices[0] + first_index);
				indices.emplace_back(face.mIndices[1] + first_index);
				indices.emplace_back(face.mIndices[2] + first_index);
			}
		}


		// write file
		auto model_out_filename = output + "/models/" + model_name + ".mmf";
		auto model_out_file = std::ofstream(model_out_filename, std::ostream::binary | std::ostream::trunc);

		MIRRAGE_INVARIANT(model_out_file.is_open(),
		                  "Unable to open output file \"" << model_out_filename << "\"!");

		auto header          = renderer::Model_file_header();
		header.vertex_count  = gsl::narrow<std::uint32_t>(vertices.size() * sizeof(renderer::Model_vertex));
		header.index_count   = gsl::narrow<std::uint32_t>(indices.size() * sizeof(std::uint32_t));
		header.submesh_count = gsl::narrow<std::uint32_t>(sub_meshes.size());

		// write header
		write(model_out_file, header);

		// write sub meshes
		for(auto& sub_mesh : sub_meshes) {
			write(model_out_file, sub_mesh.index_offset);
			write(model_out_file, sub_mesh.index_count);

			auto material_id_length = gsl::narrow<std::uint32_t>(sub_mesh.material_id.size());
			write(model_out_file, material_id_length);
			model_out_file.write(sub_mesh.material_id.data(), material_id_length);
		}

		// write vertices
		write(model_out_file, vertices);

		// write indices
		write(model_out_file, indices);

		// write footer
		write(model_out_file, renderer::Model_file_header::type_tag_value);

		LOG(plog::info) << "Done.";
	}
} // namespace mirrage
