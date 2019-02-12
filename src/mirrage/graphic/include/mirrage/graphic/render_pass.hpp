#pragma once

#include <mirrage/graphic/vk_wrapper.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/glm.hpp>
#include <gsl/gsl>
#include <vulkan/vulkan.hpp>

#include <string>
#include <unordered_map>


namespace mirrage::graphic {

	using Attachment_ref = struct Attachment_t*;

	using Stage_id = util::Str_id;


	enum class Shader_stage { vertex, tessellation_control, tessellation_eval, geometry, fragment };

	using Shader_module = vk::UniqueShaderModule;

	struct Pipeline_stage {
		Shader_stage                            stage;
		asset::Ptr<Shader_module>               shader;
		std::string                             entry_point;
		vk::SpecializationInfo                  constants_info;
		std::vector<vk::SpecializationMapEntry> constants;
		std::vector<char>                       constant_buffer;

		Pipeline_stage() = default;
		Pipeline_stage(Shader_stage stage, asset::Ptr<Shader_module> shader, std::string entry_point)
		  : stage(stage), shader(std::move(shader)), entry_point(std::move(entry_point))
		{
		}

		template <class T>
		void add_constant(std::uint32_t id, const T& data)
		{
			add_constant(id, gsl::span<const char>(reinterpret_cast<const char*>(&data), sizeof(T)));
		}

		void add_constant(std::uint32_t id, gsl::span<const char> data);
	};

	struct Pipeline_description {
	  public:
		Pipeline_description();

		Pipeline_description(Pipeline_description&&) noexcept;
		Pipeline_description& operator=(Pipeline_description&&) noexcept;

		Pipeline_description(const Pipeline_description&);
		Pipeline_description& operator=(const Pipeline_description&);


		vk::PipelineVertexInputStateCreateInfo               vertex_input;
		vk::PipelineInputAssemblyStateCreateInfo             input_assembly;
		vk::PipelineRasterizationStateCreateInfo             rasterization;
		util::maybe<vk::PipelineColorBlendStateCreateInfo>   color_blending;
		util::maybe<vk::PipelineTessellationStateCreateInfo> tessellation;
		util::maybe<vk::PipelineMultisampleStateCreateInfo>  multisample;
		util::maybe<vk::PipelineDepthStencilStateCreateInfo> depth_stencil;


		template <class T, class... Member>
		void vertex(int binding, bool per_instance_data, Member&&... members)
		{
			if(binding < 0)
				binding = gsl::narrow<int>(vertex_bindings.size());

			vertex_bindings.emplace_back(binding,
			                             gsl::narrow<uint32_t>(sizeof(T)),
			                             per_instance_data ? vk::VertexInputRate::eInstance
			                                               : vk::VertexInputRate::eVertex);

			if constexpr(sizeof...(members) == 0) {
				add_vertex_attributes<T>(binding, 0, 0);
			} else {
				util::apply2(
				        [&](auto location, auto& member) {
					        add_vertex_attributes<decltype(util::get_member_type(member))>(
					                binding, location, util::get_member_offset(member));
				        },
				        members...);
			}
		}

		void add_push_constant(util::Str_id  id,
		                       std::uint32_t offset,
		                       std::uint32_t size,
		                       vk::ShaderStageFlags);
		void add_push_constant(util::Str_id id, std::uint32_t size, vk::ShaderStageFlags);
		void add_descriptor_set_layout(vk::DescriptorSetLayout);


	  private:
		friend class Subpass_builder;
		friend class Render_pass_builder;
		friend class Stage_builder;

		bool          used_as_base = false;
		int           base_index   = -1;
		int           index        = -1;
		std::uint32_t subpass_id;
		std::uint32_t next_push_constant_offset = 0;

		std::vector<Pipeline_stage>                        stages;
		std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;
		std::vector<vk::PipelineShaderStageCreateInfo>     stage_create_infos;
		std::vector<vk::VertexInputBindingDescription>     vertex_bindings;
		std::vector<vk::VertexInputAttributeDescription>   vertex_attributes;

		std::vector<vk::PushConstantRange>                      push_constants;
		std::unordered_map<util::Str_id, vk::PushConstantRange> push_constant_table;

		std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;

		vk::PipelineLayoutCreateInfo pipeline_layout;

		template <class Member>
		void add_vertex_attributes(int binding, int location, std::size_t offset)
		{
			(void) binding;
			(void) location;
			(void) offset;
			static_assert(util::dependent_false<Member>, "Unknown type");
			MIRRAGE_FAIL("Unknown type passed to add_vertex_attributes(...)");
		}

		void build_create_info(const vk::Device&, vk::GraphicsPipelineCreateInfo&, vk::UniquePipelineLayout&);
	};


	class Stage_builder {
	  public:
		auto shader(const asset::AID& id, Shader_stage stage, std::string entry_point = "main")
		        -> Stage_builder&;

		template <typename... T>
		auto shader(const asset::AID& aid, Shader_stage stage, std::string entry_point, T&&... constants)
		        -> Stage_builder&
		{
			shader(aid, stage, entry_point);
			util::apply2(
			        [&](auto&& id, auto&& value) {
				        pipeline().stages.back().add_constant(std::forward<decltype(id)>(id),
				                                              std::forward<decltype(value)>(value));
			        },
			        std::forward<T>(constants)...);

			return *this;
		}

		template <class T, class... Member>
		auto vertex(int binding, bool per_instance_data, Member&&... members) -> Stage_builder&
		{
			pipeline().vertex(binding, per_instance_data, std::forward<Member>(members)...);
			return *this;
		}

		auto color_mask(std::uint32_t index, vk::ColorComponentFlags) -> Stage_builder&;

		auto add_push_constant(util::Str_id id, std::uint32_t offset, std::uint32_t size, vk::ShaderStageFlags)
		        -> Stage_builder&;

		auto pipeline() -> Pipeline_description&;
		auto pipeline_id() const noexcept { return _pipeline_index; }

	  private:
		friend class Subpass_builder;

		Render_pass_builder& _builder;
		std::size_t          _pipeline_index;

		Stage_builder(Render_pass_builder&, std::size_t pipeline);
	};


	extern const vk::ColorComponentFlags all_color_components;

	struct Attachment_blend {
		vk::BlendFactor srcColorFactor;
		vk::BlendFactor dstColorFactor;
		vk::BlendOp     colorOp;

		vk::BlendFactor srcAlphaFactor;
		vk::BlendFactor dstAlphaFactor;
		vk::BlendOp     alphaOp;
	};

	inline constexpr auto blend_premultiplied_alpha = Attachment_blend{vk::BlendFactor::eOne,
	                                                                   vk::BlendFactor::eOneMinusSrcAlpha,
	                                                                   vk::BlendOp::eAdd,
	                                                                   vk::BlendFactor::eOne,
	                                                                   vk::BlendFactor::eOneMinusSrcAlpha,
	                                                                   vk::BlendOp::eAdd};

	inline constexpr auto blend_add = Attachment_blend{vk::BlendFactor::eOne,
	                                                   vk::BlendFactor::eOne,
	                                                   vk::BlendOp::eAdd,
	                                                   vk::BlendFactor::eOne,
	                                                   vk::BlendFactor::eOne,
	                                                   vk::BlendOp::eAdd};

	class Subpass_builder {
	  public:
		auto stage(Stage_id) -> Stage_builder&;

		// TODO(if required): resolve_attachment
		auto color_attachment(Attachment_ref,
		                      vk::ColorComponentFlags       colorWriteMask = all_color_components,
		                      util::maybe<Attachment_blend> blend = util::nothing) -> Subpass_builder&;

		auto input_attachment(Attachment_ref) -> Subpass_builder&;
		auto depth_stencil_attachment(Attachment_ref) -> Subpass_builder&;
		auto preserve_attachment(Attachment_ref) -> Subpass_builder&;


	  private:
		friend class Render_pass_builder;

		std::size_t                          _index;
		Render_pass_builder&                 _builder;
		std::size_t                          _pipeline_index;
		std::vector<vk::AttachmentReference> _color_attachments;
		std::vector<vk::AttachmentReference> _input_attachments;
		std::vector<vk::AttachmentReference> _preserve_attachments;
		util::maybe<vk::AttachmentReference> _depth_stencil_attachment;

		std::unordered_map<Stage_id, std::unique_ptr<Stage_builder>> _stages;

		Subpass_builder(std::size_t index, Render_pass_builder&, std::size_t pipeline_index);

		auto build_description() -> vk::SubpassDescription;
	};

	struct Framebuffer_attachment_desc {
		vk::ImageView  image_view;
		vk::ClearValue clear_value;

		Framebuffer_attachment_desc() = default;
		Framebuffer_attachment_desc(vk::ImageView v, util::Rgba color)
		  : image_view(v)
		  , clear_value(vk::ClearColorValue(std::array<float, 4>{color.r, color.g, color.b, color.a}))
		{
		}
		Framebuffer_attachment_desc(vk::ImageView v, float depth, std::uint32_t stencil = 0)
		  : image_view(v), clear_value(vk::ClearDepthStencilValue(depth, stencil))
		{
		}
	};

	class Render_pass_builder {
	  public:
		auto add_attachment(vk::AttachmentDescription) -> Attachment_ref;

		auto add_subpass(Pipeline_description&) -> Subpass_builder&;

		auto add_dependency(util::maybe<Subpass_builder&> src,
		                    vk::PipelineStageFlags        srcStageMask,
		                    vk::AccessFlags               srcAccessMask,
		                    util::maybe<Subpass_builder&> dst,
		                    vk::PipelineStageFlags        dstStageMask,
		                    vk::AccessFlags               dstAccessMask,
		                    vk::DependencyFlags = vk::DependencyFlagBits::eByRegion) -> Render_pass_builder&;

		auto build() -> Render_pass;
		auto build_framebuffer(gsl::span<Framebuffer_attachment_desc> attachments,
		                       int                                    width,
		                       int                                    height,
		                       int                                    layers = 1) -> Framebuffer;

		auto build_framebuffer(Framebuffer_attachment_desc attachment, int width, int height, int layers = 1)
		        -> Framebuffer
		{
			return build_framebuffer(
			        gsl::span<Framebuffer_attachment_desc>{&attachment, 1}, width, height, layers);
		}

	  private:
		friend class Device;
		friend class Subpass_builder;
		friend class Stage_builder;

		const vk::Device&     _device;
		vk::PipelineCache     _pipeline_cache;
		asset::Asset_manager& _assets;

		std::vector<std::unique_ptr<Subpass_builder>> _subpasses;
		std::vector<vk::AttachmentDescription>        _attachments;
		std::vector<vk::SubpassDependency>            _dependencies;

		std::vector<Pipeline_description> _pipelines;

		vk::RenderPass _created_render_pass;

		Render_pass_builder(const vk::Device&, vk::PipelineCache, asset::Asset_manager& assets);
	};



	class Render_pass {
	  public:
		Render_pass(Render_pass&&);
		Render_pass& operator=(Render_pass&&);
		~Render_pass();

		void next_subpass(bool inline_contents = true);

		void set_stage(util::Str_id stage_id);

		void push_constant(util::Str_id id, gsl::span<const char> data);

		template <class T>
		void push_constant(util::Str_id id, T&& t)
		{
			push_constant(id, gsl::span<const char>(reinterpret_cast<const char*>(&t), sizeof(T)));
		}

		void bind_descriptor_set(std::uint32_t                  firstSet,
		                         vk::DescriptorSet              set,
		                         gsl::span<const std::uint32_t> dynamic_offsets = {})
		{
			bind_descriptor_sets(firstSet, {&set, 1}, dynamic_offsets);
		}
		void bind_descriptor_sets(std::uint32_t firstSet,
		                          gsl::span<const vk::DescriptorSet>,
		                          gsl::span<const std::uint32_t> dynamic_offsets = {});

		template <typename F>
		void execute(const Command_buffer& buffer, const Framebuffer& fb, F&& f)
		{
			_pre(buffer, fb);
			f();
			_post();
		}

		void unsafe_begin_renderpass(const Command_buffer& buffer, const Framebuffer& fb)
		{
			_pre(buffer, fb);
		}
		void unsafe_end_renderpass() { _post(); }


	  private:
		friend class Render_pass_builder;

		using Push_constant_map = std::unordered_map<util::Str_id, vk::PushConstantRange>;
		using Stages            = std::vector<std::unordered_map<Stage_id, std::size_t>>;

		vk::UniqueRenderPass                  _render_pass;
		std::vector<vk::UniquePipeline>       _pipelines;
		std::vector<vk::UniquePipelineLayout> _pipeline_layouts;
		Stages                                _stages;
		std::vector<Push_constant_map>        _push_constants;

		std::size_t                        _bound_pipeline = 0;
		std::size_t                        _subpass_index  = 0;
		util::maybe<const Command_buffer&> _current_command_buffer;

		Render_pass(vk::UniqueRenderPass                  render_pass,
		            std::vector<vk::UniquePipeline>       pipelines,
		            std::vector<vk::UniquePipelineLayout> pipeline_layouts,
		            Stages                                stages,
		            std::vector<Push_constant_map>        push_constants);

		void _pre(const Command_buffer& buffer, const Framebuffer& fb);
		void _post();
	};



	// IMPL

	template <>
	inline void Pipeline_description::add_vertex_attributes<float>(int         binding,
	                                                               int         location,
	                                                               std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32Sfloat, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<std::int32_t>(int         binding,
	                                                                      int         location,
	                                                                      std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32Sint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<std::uint32_t>(int         binding,
	                                                                       int         location,
	                                                                       std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32Uint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::vec2>(int         binding,
	                                                                   int         location,
	                                                                   std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32Sfloat, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::ivec2>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32Sint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::uvec2>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32Uint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::vec3>(int         binding,
	                                                                   int         location,
	                                                                   std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32Sfloat, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::ivec3>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32Sint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::uvec3>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32Uint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::vec4>(int         binding,
	                                                                   int         location,
	                                                                   std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32A32Sfloat, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::ivec4>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32A32Sint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<glm::uvec4>(int         binding,
	                                                                    int         location,
	                                                                    std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR32G32B32A32Uint, gsl::narrow<uint32_t>(offset));
	}
	template <>
	inline void Pipeline_description::add_vertex_attributes<std::uint8_t (&)[4]>(int         binding,
	                                                                             int         location,
	                                                                             std::size_t offset)
	{
		vertex_attributes.emplace_back(location, binding, vk::Format::eR8G8B8A8Unorm, gsl::narrow<uint32_t>(offset));
	}
} // namespace mirrage::graphic

namespace mirrage::asset {

	template <>
	struct Loader<graphic::Shader_module> {
	  public:
		Loader(graphic::Device& device) : _device(device) {}

		auto load(istream in) -> graphic::Shader_module;
		void save(ostream, const graphic::Shader_module&)
		{
			MIRRAGE_FAIL("Save of shader modules is not supported!");
		}

	  private:
		graphic::Device& _device;
	};

} // namespace mirrage::asset
