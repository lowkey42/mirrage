#include <mirrage/graphic/render_pass.hpp>

#include <mirrage/graphic/device.hpp>

#include <mirrage/utils/ranges.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	const vk::ColorComponentFlags all_color_components =
	        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
	        | vk::ColorComponentFlagBits::eA;


	void Pipeline_stage::add_constant(std::uint32_t id, gsl::span<const char> data)
	{
		constants.emplace_back(id, gsl::narrow<std::uint32_t>(constant_buffer.size()), data.size());
		constant_buffer.insert(constant_buffer.end(), data.begin(), data.end());
	}

	Pipeline_description::Pipeline_description() { rasterization.lineWidth = 1.f; }
	Pipeline_description::Pipeline_description(Pipeline_description&& rhs) noexcept
	  : vertex_input(std::move(rhs.vertex_input))
	  , input_assembly(std::move(rhs.input_assembly))
	  , rasterization(std::move(rhs.rasterization))
	  , color_blending(std::move(rhs.color_blending))
	  , tessellation(std::move(rhs.tessellation))
	  , multisample(std::move(rhs.multisample))
	  , depth_stencil(std::move(rhs.depth_stencil))
	  , used_as_base(std::move(rhs.used_as_base))
	  , base_index(std::move(rhs.base_index))
	  , index(std::move(rhs.index))
	  , subpass_id(std::move(rhs.subpass_id))
	  , next_push_constant_offset(std::move(rhs.next_push_constant_offset))
	  , stages(std::move(rhs.stages))
	  , color_blend_attachments(std::move(rhs.color_blend_attachments))
	  , stage_create_infos(std::move(rhs.stage_create_infos))
	  , vertex_bindings(std::move(rhs.vertex_bindings))
	  , vertex_attributes(std::move(rhs.vertex_attributes))
	  , push_constants(std::move(rhs.push_constants))
	  , push_constant_table(std::move(rhs.push_constant_table))
	  , descriptor_set_layouts(std::move(rhs.descriptor_set_layouts))
	  , pipeline_layout(std::move(rhs.pipeline_layout))
	{
	}
	Pipeline_description& Pipeline_description::operator=(Pipeline_description&& rhs) noexcept
	{
		vertex_input              = std::move(rhs.vertex_input);
		input_assembly            = std::move(rhs.input_assembly);
		rasterization             = std::move(rhs.rasterization);
		color_blending            = std::move(rhs.color_blending);
		tessellation              = std::move(rhs.tessellation);
		multisample               = std::move(rhs.multisample);
		depth_stencil             = std::move(rhs.depth_stencil);
		used_as_base              = std::move(rhs.used_as_base);
		base_index                = std::move(rhs.base_index);
		index                     = std::move(rhs.index);
		subpass_id                = std::move(rhs.subpass_id);
		next_push_constant_offset = std::move(rhs.next_push_constant_offset);
		stages                    = std::move(rhs.stages);
		color_blend_attachments   = std::move(rhs.color_blend_attachments);
		stage_create_infos        = std::move(rhs.stage_create_infos);
		vertex_bindings           = std::move(rhs.vertex_bindings);
		vertex_attributes         = std::move(rhs.vertex_attributes);
		push_constants            = std::move(rhs.push_constants);
		push_constant_table       = std::move(rhs.push_constant_table);
		descriptor_set_layouts    = std::move(rhs.descriptor_set_layouts);
		pipeline_layout           = std::move(rhs.pipeline_layout);

		return *this;
	}
	Pipeline_description::Pipeline_description(const Pipeline_description& rhs)
	  : vertex_input(rhs.vertex_input)
	  , input_assembly(rhs.input_assembly)
	  , rasterization(rhs.rasterization)
	  , color_blending(rhs.color_blending)
	  , tessellation(rhs.tessellation)
	  , multisample(rhs.multisample)
	  , depth_stencil(rhs.depth_stencil)
	  , used_as_base(rhs.used_as_base)
	  , base_index(rhs.base_index)
	  , index(rhs.index)
	  , subpass_id(rhs.subpass_id)
	  , color_blend_attachments(rhs.color_blend_attachments)
	  , vertex_bindings(rhs.vertex_bindings)
	  , vertex_attributes(rhs.vertex_attributes)
	  , push_constants(rhs.push_constants)
	  , push_constant_table(rhs.push_constant_table)
	  , descriptor_set_layouts(rhs.descriptor_set_layouts)
	  , pipeline_layout(rhs.pipeline_layout)
	{

		MIRRAGE_INVARIANT(stages.empty(), "Copied already used Pipeline_description");
	}

	Pipeline_description& Pipeline_description::operator=(const Pipeline_description& rhs)
	{
		auto rhs_copy = rhs;
		*this         = std::move(rhs_copy);

		return *this;
	}

	void Pipeline_description::add_push_constant(util::Str_id         id,
	                                             std::uint32_t        offset,
	                                             std::uint32_t        size,
	                                             vk::ShaderStageFlags stage)
	{
		push_constants.emplace_back(stage, offset, size);
		push_constant_table.emplace(id, push_constants.back());
		next_push_constant_offset = std::max(next_push_constant_offset, offset + size);
	}
	void Pipeline_description::add_push_constant(util::Str_id         id,
	                                             std::uint32_t        size,
	                                             vk::ShaderStageFlags stage)
	{
		add_push_constant(id, next_push_constant_offset, size, stage);
	}

	void Pipeline_description::add_descriptor_set_layout(vk::DescriptorSetLayout layout)
	{
		descriptor_set_layouts.emplace_back(layout);
	}

	namespace {

		const auto default_viewport       = vk::Viewport{};
		const auto default_viewport_state = vk::PipelineViewportStateCreateInfo{
		        vk::PipelineViewportStateCreateFlags{}, 1, nullptr, 1, nullptr};
		const vk::DynamicState default_dynamic_states[]{
		        vk::DynamicState::eScissor, vk::DynamicState::eViewport, vk::DynamicState::eBlendConstants};
		const auto default_dynamic_state = vk::PipelineDynamicStateCreateInfo{
		        vk::PipelineDynamicStateCreateFlags{}, 3, default_dynamic_states};
	} // namespace
	void Pipeline_description::build_create_info(const vk::Device&               device,
	                                             vk::GraphicsPipelineCreateInfo& cinfo,
	                                             vk::UniquePipelineLayout&       layout)
	{

		pipeline_layout.setLayoutCount         = gsl::narrow<std::uint32_t>(descriptor_set_layouts.size());
		pipeline_layout.pSetLayouts            = descriptor_set_layouts.data();
		pipeline_layout.pushConstantRangeCount = gsl::narrow<std::uint32_t>(push_constants.size());
		pipeline_layout.pPushConstantRanges    = push_constants.data();
		layout                                 = device.createPipelineLayoutUnique(pipeline_layout);

		stage_create_infos.clear();
		stage_create_infos.reserve(stages.size());
		for(auto& s : stages) {
			auto vk_stage = [&] {
				switch(s.stage) {
					case Shader_stage::vertex: return vk::ShaderStageFlagBits::eVertex;
					case Shader_stage::tessellation_control:
						return vk::ShaderStageFlagBits::eTessellationControl;
					case Shader_stage::tessellation_eval:
						return vk::ShaderStageFlagBits::eTessellationEvaluation;
					case Shader_stage::geometry: return vk::ShaderStageFlagBits::eGeometry;
					case Shader_stage::fragment: return vk::ShaderStageFlagBits::eFragment;
				}
				MIRRAGE_FAIL("Unecpected Shader_stage " << static_cast<int>(s.stage));
			}();

			s.constants_info = vk::SpecializationInfo{gsl::narrow<std::uint32_t>(s.constants.size()),
			                                          s.constants.data(),
			                                          s.constant_buffer.size(),
			                                          s.constant_buffer.data()};
			stage_create_infos.emplace_back(vk::PipelineShaderStageCreateFlags{},
			                                vk_stage,
			                                **s.shader,
			                                s.entry_point.c_str(),
			                                s.constants.empty() ? nullptr : &s.constants_info);
		}

		vertex_input.vertexBindingDescriptionCount   = gsl::narrow<std::uint32_t>(vertex_bindings.size());
		vertex_input.pVertexBindingDescriptions      = vertex_bindings.data();
		vertex_input.vertexAttributeDescriptionCount = gsl::narrow<std::uint32_t>(vertex_attributes.size());
		vertex_input.pVertexAttributeDescriptions    = vertex_attributes.data();

		cinfo.subpass             = subpass_id;
		cinfo.layout              = *layout;
		cinfo.stageCount          = gsl::narrow<std::uint32_t>(stage_create_infos.size());
		cinfo.pStages             = stage_create_infos.data();
		cinfo.pVertexInputState   = &vertex_input;
		cinfo.pInputAssemblyState = &input_assembly;
		cinfo.pRasterizationState = &rasterization;
		cinfo.pVertexInputState   = &vertex_input;
		cinfo.pViewportState      = &default_viewport_state;
		cinfo.pDynamicState       = &default_dynamic_state;

		color_blending.process([&](auto& s) {
			s.attachmentCount      = gsl::narrow<std::uint32_t>(color_blend_attachments.size());
			s.pAttachments         = color_blend_attachments.data();
			cinfo.pColorBlendState = &s;
		});

		tessellation.process([&](auto& s) { cinfo.pTessellationState = &s; });
		multisample.process([&](auto& s) { cinfo.pMultisampleState = &s; });
		depth_stencil.process([&](auto& s) { cinfo.pDepthStencilState = &s; });


		if(base_index > 0) {
			cinfo.basePipelineIndex = base_index;
			cinfo.flags             = cinfo.flags | vk::PipelineCreateFlagBits::eDerivative;
		}

		if(used_as_base) {
			cinfo.flags = cinfo.flags | vk::PipelineCreateFlagBits::eAllowDerivatives;
		}
	}


	auto Stage_builder::shader(const asset::AID& id, Shader_stage stage, std::string entry_point)
	        -> Stage_builder&
	{
		pipeline().stages.emplace_back(
		        stage, _builder._assets.load<Shader_module>(id), std::move(entry_point));

		return *this;
	}

	auto Stage_builder::color_mask(std::uint32_t index, vk::ColorComponentFlags mask) -> Stage_builder&
	{
		pipeline().color_blend_attachments.at(index).setColorWriteMask(mask);
		return *this;
	}

	auto Stage_builder::add_push_constant(util::Str_id         id,
	                                      std::uint32_t        offset,
	                                      std::uint32_t        size,
	                                      vk::ShaderStageFlags shader_stages) -> Stage_builder&
	{
		pipeline().add_push_constant(id, offset, size, shader_stages);
		return *this;
	}

	auto Stage_builder::pipeline() -> Pipeline_description&
	{
		return _builder._pipelines.at(_pipeline_index);
	}

	Stage_builder::Stage_builder(Render_pass_builder& builder, std::size_t pipeline)
	  : _builder(builder), _pipeline_index(pipeline)
	{
	}


	Subpass_builder::Subpass_builder(std::size_t          index,
	                                 Render_pass_builder& builder,
	                                 std::size_t          pipeline_index)
	  : _index(index), _builder(builder), _pipeline_index(pipeline_index)
	{
	}

	auto Subpass_builder::stage(Stage_id id) -> Stage_builder&
	{
		auto& stage = _stages[id];
		if(!stage) {
			if(_stages.size() == 1) {
				// first stage uses pipeline created with subpass
				stage.reset(new Stage_builder(_builder, _pipeline_index));
			} else {
				auto pipeline_copy       = _builder._pipelines.at(_pipeline_index);
				pipeline_copy.index      = gsl::narrow<int>(_builder._pipelines.size());
				pipeline_copy.base_index = gsl::narrow<int>(_pipeline_index);
				_builder._pipelines.emplace_back(pipeline_copy);

				// inherit previous pipeline
				auto& subpass_pipeline        = _builder._pipelines.at(_pipeline_index);
				subpass_pipeline.used_as_base = true;

				stage.reset(new Stage_builder(_builder, _builder._pipelines.size() - 1));
			}
		}

		return *stage;
	}


	auto Subpass_builder::color_attachment(Attachment_ref                attachment,
	                                       vk::ColorComponentFlags       colorWriteMask,
	                                       util::maybe<Attachment_blend> blend) -> Subpass_builder&
	{

		auto attachment_idx = gsl::narrow_cast<uint32_t>(reinterpret_cast<std::intptr_t>(attachment));
		_color_attachments.emplace_back(attachment_idx, vk::ImageLayout::eColorAttachmentOptimal);

		auto blend_state = vk::PipelineColorBlendAttachmentState{};
		blend_state.setColorWriteMask(colorWriteMask);

		blend.process([&](auto& blend) {
			blend_state.setBlendEnable(true);
			blend_state.setSrcColorBlendFactor(blend.srcColorFactor);
			blend_state.setDstColorBlendFactor(blend.dstColorFactor);
			blend_state.setColorBlendOp(blend.colorOp);

			blend_state.setSrcAlphaBlendFactor(blend.srcAlphaFactor);
			blend_state.setDstAlphaBlendFactor(blend.dstAlphaFactor);
			blend_state.setAlphaBlendOp(blend.alphaOp);
		});

		if(_stages.empty()) {
			_builder._pipelines[_pipeline_index].color_blend_attachments.emplace_back(blend_state);
		} else {
			for(auto& stage : _stages) {
				stage.second->pipeline().color_blend_attachments.emplace_back(blend_state);
			}
		}

		return *this;
	}

	auto Subpass_builder::input_attachment(Attachment_ref attachment, vk::ImageLayout layout)
	        -> Subpass_builder&
	{
		auto attachment_idx = gsl::narrow_cast<uint32_t>(reinterpret_cast<std::intptr_t>(attachment));
		_input_attachments.emplace_back(attachment_idx, layout);

		return *this;
	}
	auto Subpass_builder::depth_stencil_attachment(Attachment_ref attachment, vk::ImageLayout layout)
	        -> Subpass_builder&
	{
		auto attachment_idx       = gsl::narrow_cast<uint32_t>(reinterpret_cast<std::intptr_t>(attachment));
		_depth_stencil_attachment = vk::AttachmentReference{attachment_idx, layout};
		return *this;
	}
	auto Subpass_builder::preserve_attachment(Attachment_ref attachment) -> Subpass_builder&
	{
		auto attachment_idx = gsl::narrow_cast<uint32_t>(reinterpret_cast<std::intptr_t>(attachment));
		_preserve_attachments.emplace_back(attachment_idx);

		return *this;
	}

	auto Subpass_builder::build_description() -> vk::SubpassDescription
	{
		auto desc                    = vk::SubpassDescription{};
		desc.colorAttachmentCount    = gsl::narrow<std::uint32_t>(_color_attachments.size());
		desc.pColorAttachments       = _color_attachments.data();
		desc.inputAttachmentCount    = gsl::narrow<std::uint32_t>(_input_attachments.size());
		desc.pInputAttachments       = _input_attachments.data();
		desc.preserveAttachmentCount = gsl::narrow<std::uint32_t>(_preserve_attachments.size());
		desc.pPreserveAttachments    = _preserve_attachments.data();
		_depth_stencil_attachment.process([&](auto& a) { desc.pDepthStencilAttachment = &a; });

		return desc;
	}


	Render_pass_builder::Render_pass_builder(const vk::Device&     device,
	                                         vk::PipelineCache     cache,
	                                         asset::Asset_manager& assets)
	  : _device(device), _pipeline_cache(cache), _assets(assets)
	{
	}

	auto Render_pass_builder::add_attachment(vk::AttachmentDescription desc) -> Attachment_ref
	{
		_attachments.emplace_back(desc);

		return reinterpret_cast<Attachment_ref>(_attachments.size() - 1);
	}

	auto Render_pass_builder::add_subpass(Pipeline_description& pipeline) -> Subpass_builder&
	{
		if(pipeline.index >= 0) { // already added => use prev as base
			_pipelines.at(gsl::narrow<std::size_t>(pipeline.index)).used_as_base = true;
		}

		_pipelines.emplace_back(pipeline);
		auto& new_pipeline        = _pipelines.back();
		new_pipeline.index        = gsl::narrow<std::int32_t>(_pipelines.size() - 1);
		new_pipeline.base_index   = pipeline.index;
		new_pipeline.used_as_base = false;
		new_pipeline.subpass_id   = gsl::narrow<std::uint32_t>(_subpasses.size());

		if(pipeline.index < 0) {
			pipeline.index = new_pipeline.index;
		}

		// doesn't use make_unique, because the constructor is private and we are its friend
		_subpasses.emplace_back(std::unique_ptr<Subpass_builder>(
		        new Subpass_builder(_subpasses.size(), *this, std::size_t(new_pipeline.index))));

		return *_subpasses.back();
	}

	auto Render_pass_builder::add_dependency(util::maybe<Subpass_builder&> src,
	                                         vk::PipelineStageFlags        srcStageMask,
	                                         vk::AccessFlags               srcAccessMask,
	                                         util::maybe<Subpass_builder&> dst,
	                                         vk::PipelineStageFlags        dstStageMask,
	                                         vk::AccessFlags               dstAccessMask,
	                                         vk::DependencyFlags           flags) -> Render_pass_builder&
	{

		auto src_id =
		        src.process(VK_SUBPASS_EXTERNAL, [](auto& s) { return gsl::narrow<uint32_t>(s._index); });
		auto dst_id =
		        dst.process(VK_SUBPASS_EXTERNAL, [](auto& s) { return gsl::narrow<uint32_t>(s._index); });

		_dependencies.emplace_back(
		        src_id, dst_id, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, flags);

		return *this;
	}

	auto Render_pass_builder::build() -> Render_pass
	{
		MIRRAGE_INVARIANT(!_created_render_pass, "Multiple calls to Render_pass_builder::build()");

		auto stages = std::vector<std::unordered_map<Stage_id, std::size_t>>();

		// construct _create_info
		auto subpass_descriptions = std::vector<vk::SubpassDescription>();
		subpass_descriptions.reserve(_subpasses.size());
		for(auto& s : _subpasses) {
			auto& subpass_stages = stages.emplace_back();
			subpass_stages.reserve(s->_stages.size());

			subpass_descriptions.emplace_back(s->build_description());

			for(auto& stage : s->_stages) {
				subpass_stages.emplace(stage.first, stage.second->pipeline_id());
			}
		}

		auto pre_dependency_missing  = true;
		auto post_dependency_missing = true;
		for(auto& dep : _dependencies) {
			if(dep.srcSubpass == VK_SUBPASS_EXTERNAL)
				pre_dependency_missing = false;
			if(dep.dstSubpass == VK_SUBPASS_EXTERNAL)
				post_dependency_missing = false;
		}

		if(pre_dependency_missing) {
			add_dependency(
			        util::nothing,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput
			                | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                | vk::PipelineStageFlagBits::eHost | vk::PipelineStageFlagBits::eLateFragmentTests
			                | vk::PipelineStageFlagBits::eTransfer,
			        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eMemoryWrite
			                | vk::AccessFlagBits::eHostWrite
			                | vk::AccessFlagBits::eDepthStencilAttachmentWrite
			                | vk::AccessFlagBits::eTransferWrite,
			        *_subpasses.front(),
			        vk::PipelineStageFlagBits::eColorAttachmentOutput
			                | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                | vk::PipelineStageFlagBits::eFragmentShader,
			        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead
			                | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                | vk::AccessFlagBits::eDepthStencilAttachmentWrite
			                | vk::AccessFlagBits::eShaderRead);
		}
		if(post_dependency_missing) {
			add_dependency(*_subpasses.back(),
			               vk::PipelineStageFlagBits::eColorAttachmentOutput
			                       | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                       | vk::PipelineStageFlagBits::eLateFragmentTests,
			               vk::AccessFlagBits::eColorAttachmentWrite
			                       | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			               util::nothing,
			               vk::PipelineStageFlagBits::eColorAttachmentOutput
			                       | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                       | vk::PipelineStageFlagBits::eFragmentShader
			                       | vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost,
			               vk::AccessFlagBits::eColorAttachmentWrite
			                       | vk::AccessFlagBits::eColorAttachmentRead
			                       | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                       | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead
			                       | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eMemoryRead);
		}

		auto create_info = vk::RenderPassCreateInfo{vk::RenderPassCreateFlags{},
		                                            gsl::narrow<std::uint32_t>(_attachments.size()),
		                                            _attachments.data(),
		                                            gsl::narrow<std::uint32_t>(subpass_descriptions.size()),
		                                            subpass_descriptions.data(),
		                                            gsl::narrow<std::uint32_t>(_dependencies.size()),
		                                            _dependencies.data()};

		auto render_pass     = _device.createRenderPassUnique(create_info);
		_created_render_pass = *render_pass;

		// build pipelines
		auto pipeline_layouts = std::vector<vk::UniquePipelineLayout>();
		pipeline_layouts.reserve(_pipelines.size());
		auto pipeline_create_infos = std::vector<vk::GraphicsPipelineCreateInfo>();
		pipeline_create_infos.reserve(_pipelines.size());
		auto push_constants = std::vector<std::unordered_map<util::Str_id, vk::PushConstantRange>>();
		push_constants.reserve(_pipelines.size());

		for(auto& p : _pipelines) {
			pipeline_create_infos.emplace_back();
			pipeline_layouts.emplace_back();

			p.build_create_info(_device, pipeline_create_infos.back(), pipeline_layouts.back());
			pipeline_create_infos.back().renderPass = _created_render_pass;
			push_constants.emplace_back(std::move(p.push_constant_table));
		}

		return Render_pass{std::move(render_pass),
		                   _device.createGraphicsPipelinesUnique(_pipeline_cache, pipeline_create_infos),
		                   std::move(pipeline_layouts),
		                   std::move(stages),
		                   std::move(push_constants)};
	}

	auto Render_pass_builder::build_framebuffer(gsl::span<Framebuffer_attachment_desc> attachments,
	                                            int                                    width,
	                                            int                                    height,
	                                            int                                    layers) -> Framebuffer
	{
		MIRRAGE_INVARIANT(_created_render_pass, "build_framebuffer() must be called after build().");

		MIRRAGE_INVARIANT(gsl::narrow<std::size_t>(attachments.size()) == _attachments.size(),
		                  "Given number of attachments doesn't match configured one. "
		                  "Found: "
		                          << attachments.size() << ", expected: " << _attachments.size());

		auto attachment_images = std::vector<vk::ImageView>();
		auto clear_values      = std::vector<vk::ClearValue>();


		attachment_images.reserve(gsl::narrow<std::size_t>(attachments.size()));
		clear_values.reserve(gsl::narrow<std::size_t>(attachments.size()));
		for(auto& a : attachments) {
			attachment_images.emplace_back(a.image_view);
			clear_values.emplace_back(a.clear_value);
		}

		// remove unused clear_values at the end to silence validation layers
		for(auto& a : util::range_reverse(_attachments)) {
			if(a.loadOp != vk::AttachmentLoadOp::eClear && a.stencilLoadOp != vk::AttachmentLoadOp::eClear) {
				clear_values.pop_back();
			} else {
				break;
			}
		}

		auto info = vk::FramebufferCreateInfo{vk::FramebufferCreateFlags{},
		                                      _created_render_pass,
		                                      gsl::narrow_cast<std::uint32_t>(attachment_images.size()),
		                                      attachment_images.data(),
		                                      gsl::narrow_cast<std::uint32_t>(width),
		                                      gsl::narrow_cast<std::uint32_t>(height),
		                                      gsl::narrow_cast<std::uint32_t>(layers)};

		return {_device.createFramebufferUnique(info),
		        vk::Viewport(0, 0, float(width), float(height), 0.f, 1.f),
		        vk::Rect2D{
		                vk::Offset2D(0, 0),
		                vk::Extent2D(gsl::narrow<std::uint32_t>(width), gsl::narrow<std::uint32_t>(height))},
		        std::move(clear_values)};
	}



	Render_pass::Render_pass(Render_pass&&) = default;
	Render_pass& Render_pass::operator=(Render_pass&&) = default;
	Render_pass::~Render_pass()                        = default;

	Render_pass::Render_pass(vk::UniqueRenderPass                  render_pass,
	                         std::vector<vk::UniquePipeline>       pipelines,
	                         std::vector<vk::UniquePipelineLayout> pipeline_layouts,
	                         Stages                                stages,
	                         std::vector<Push_constant_map>        push_constants)
	  : _render_pass(std::move(render_pass))
	  , _pipelines(std::move(pipelines))
	  , _pipeline_layouts(std::move(pipeline_layouts))
	  , _stages(std::move(stages))
	  , _push_constants(std::move(push_constants))
	{
	}


	void Render_pass::next_subpass(bool inline_contents)
	{
		auto& cmb =
		        _current_command_buffer.get_or_throw("next_pass can only be called inside an execute block!");

		_subpass_index++;
		MIRRAGE_INVARIANT(_subpass_index < _stages.size(),
		                  "Render_pass::next_subpass() called more often than there are subpasses: "
		                          << _subpass_index << " vs " << _stages.size());

		cmb.nextSubpass(inline_contents ? vk::SubpassContents::eInline
		                                : vk::SubpassContents::eSecondaryCommandBuffers);
	}

	namespace {
		auto print_stages(const std::unordered_map<Stage_id, std::size_t>& stages)
		{
			auto os = std::stringstream();

			os << '[';
			auto first = true;
			for(auto&& [key, value] : stages) {
				(void) value;
				if(first)
					first = false;
				else
					os << ", ";
				os << key;
			}
			os << ']';
			return os.str();
		}
	} // namespace

	void Render_pass::set_stage(util::Str_id stage_id)
	{
		auto& cmb =
		        _current_command_buffer.get_or_throw("set_stage can only be called inside an execute block!");

		auto& stages       = _stages.at(_subpass_index);
		auto  pipeline_idx = stages.find(stage_id);
		MIRRAGE_INVARIANT(pipeline_idx != stages.end(),
		                  "Unknown render stage '" << stage_id.str() << "'! Expected one of "
		                                           << print_stages(stages));

		_bound_pipeline = pipeline_idx->second;
		cmb.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipelines.at(pipeline_idx->second));
	}

	void Render_pass::push_constant(util::Str_id id, gsl::span<const char> data)
	{
		auto& cmb = _current_command_buffer.get_or_throw(
		        "push_constant can only be called inside an execute block!");

		auto info = util::find_maybe(_push_constants[_bound_pipeline], id)
		                    .get_or_throw("No push constant with name '", id.str(), "' found");

		MIRRAGE_INVARIANT(data.size() == info.size,
		                  "Data passed to push_constant doesn't equal configured range. "
		                  "Found: "
		                          << data.size() << ", Expected: " << info.size);

		cmb.pushConstants(*_pipeline_layouts[_bound_pipeline],
		                  info.stageFlags,
		                  info.offset,
		                  vk::ArrayProxy<const char>(gsl::narrow<std::uint32_t>(data.size()), data.data()));
	}

	void Render_pass::bind_descriptor_sets(std::uint32_t                      first_set,
	                                       gsl::span<const vk::DescriptorSet> sets,
	                                       gsl::span<const std::uint32_t>     dynamic_offsets)
	{
		auto& cmb = _current_command_buffer.get_or_throw(
		        "bind_descriptor_sets can only be called inside an execute block!");

		cmb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
		                       *_pipeline_layouts[_bound_pipeline],
		                       first_set,
		                       gsl::narrow<std::uint32_t>(sets.size()),
		                       sets.data(),
		                       gsl::narrow<std::uint32_t>(dynamic_offsets.size()),
		                       dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
	}

	void Render_pass::_pre(const Command_buffer& cb, const Framebuffer& fb)
	{
		MIRRAGE_INVARIANT(_current_command_buffer.is_nothing(), "execute blocks can not be nested!");
		_current_command_buffer = cb;
		_bound_pipeline         = 0;
		_subpass_index          = 0;

		vk::RenderPassBeginInfo rp_info = {*_render_pass, *fb._fb, fb._scissor};
		rp_info.setClearValueCount(gsl::narrow<std::uint32_t>(fb._clear_values.size()));
		rp_info.setPClearValues(fb._clear_values.data());

		cb.beginRenderPass(&rp_info, vk::SubpassContents::eInline);
		cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipelines[0]);
		cb.setViewport(0, {fb._viewport});
		cb.setScissor(0, {fb._scissor});
	}

	void Render_pass::_post()
	{
		MIRRAGE_INVARIANT(_current_command_buffer.is_some(), "execute block closed without being opened!");
		_current_command_buffer.get_or_throw().endRenderPass();

		_current_command_buffer = util::nothing;
	}
} // namespace mirrage::graphic

namespace mirrage::asset {

	auto Loader<graphic::Shader_module>::load(istream in) -> graphic::Shader_module
	{
		auto code = in.bytes();
		auto module_info =
		        vk::ShaderModuleCreateInfo{{}, code.size(), reinterpret_cast<const uint32_t*>(code.data())};

		return _device.vk_device()->createShaderModuleUnique(module_info);
	}

} // namespace mirrage::asset
