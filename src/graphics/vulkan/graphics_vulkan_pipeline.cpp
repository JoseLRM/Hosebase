#include "Hosebase/defines.h"

#if SV_GRAPHICS

#define SV_VULKAN_IMPLEMENTATION
#include "graphics_vulkan.h"

namespace sv
{

	size_t graphics_vulkan_pipeline_compute_hash(const GraphicsState &state)
	{
		Shader_vk *vs = reinterpret_cast<Shader_vk *>(state.vertex_shader);
		Shader_vk *ps = reinterpret_cast<Shader_vk *>(state.pixel_shader);
		Shader_vk *gs = reinterpret_cast<Shader_vk *>(state.geometry_shader);
		BlendState_vk &blendState = *reinterpret_cast<BlendState_vk *>(state.blend_state);
		DepthStencilState_vk &depthStencilState = *reinterpret_cast<DepthStencilState_vk *>(state.depth_stencil_state);

		// TODO: Optimize
		u64 hash = 0u;
		if (vs)
			hash = hash_combine(hash, vs->ID);
		if (ps)
			hash = hash_combine(hash, ps->ID);
		if (gs)
			hash = hash_combine(hash, gs->ID);
		hash = hash_combine(hash, blendState.hash);
		hash = hash_combine(hash, depthStencilState.hash);

		// Input layout
		{
			hash = hash_combine(hash, state.input_layout.slot_count);

			for (u32 i = 0; i < state.input_layout.slot_count; ++i)
			{
				hash = hash_combine(hash, state.input_layout.slots[i].instanced);
				hash = hash_combine(hash, state.input_layout.slots[i].stride);

				hash = hash_combine(hash, state.input_layout.slots[i].element_count);

				for (u32 j = 0; j < state.input_layout.slots[i].element_count; ++j)
				{
					hash = hash_combine(hash, state.input_layout.slots[i].elements[j].format);
					hash = hash_combine(hash, state.input_layout.slots[i].elements[j].index);
					hash = hash_combine(hash, state.input_layout.slots[i].elements[j].offset);
					hash = hash_combine(hash, hash_string(state.input_layout.slots[i].elements[j].name));
				}
			}
		}

		// Rasterizer
		{
			hash = hash_combine(hash, state.rasterizer.clockwise);
			hash = hash_combine(hash, state.rasterizer.cull_mode);
			hash = hash_combine(hash, state.rasterizer.wireframe);
		}

		hash = hash_combine(hash, u64(state.topology));

		return hash;
	}

	bool graphics_vulkan_pipeline_create(VulkanPipeline &p, Shader_vk *pVertexShader, Shader_vk *pPixelShader, Shader_vk *pGeometryShader)
	{
		Graphics_vk &gfx = graphics_vulkan_device_get();

		p.mutex = mutex_create();
		p.creationMutex = mutex_create();

		// Create
		SV_LOCK_GUARD(p.creationMutex, lock);

		// Check if it is created
		if (p.layout != VK_NULL_HANDLE)
			return true;

		// Create Pipeline Layout
		{
			// TODO: Optimize
			std::vector<VkDescriptorSetLayout> layouts;

			if (pVertexShader)
			{
				layouts.push_back(pVertexShader->layout.setLayout);
			}

			if (pPixelShader)
			{
				layouts.push_back(pPixelShader->layout.setLayout);
			}

			if (pGeometryShader)
			{
				layouts.push_back(pGeometryShader->layout.setLayout);
			}

			VkPipelineLayoutCreateInfo layout_info{};
			layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layout_info.setLayoutCount = u32(layouts.size());
			layout_info.pSetLayouts = layouts.data();
			layout_info.pushConstantRangeCount = 0u;

			if (vkCreatePipelineLayout(gfx.device, &layout_info, nullptr, &p.layout) != VK_SUCCESS)
				return false;
		}

		p.lastUsage = timer_now();

		return true;
	}

	bool graphics_vulkan_pipeline_destroy(VulkanPipeline &pipeline)
	{
		Graphics_vk &gfx = graphics_vulkan_device_get();

		mutex_destroy(pipeline.mutex);
		mutex_destroy(pipeline.creationMutex);

		vkDestroyPipelineLayout(gfx.device, pipeline.layout, nullptr);

		for (auto it : pipeline.pipelines)
		{
			vkDestroyPipeline(gfx.device, it.second, nullptr);
		}
		return true;
	}

	VkPipeline graphics_vulkan_pipeline_get(VulkanPipeline &pipeline, GraphicsState &state, size_t hash)
	{
		Graphics_vk &gfx = graphics_vulkan_device_get();
		RenderPass_vk &renderPass = *reinterpret_cast<RenderPass_vk *>(state.render_pass);
		VkPipeline res = VK_NULL_HANDLE;

		hash = hash_combine(hash, (u64)renderPass.renderPass);

		SV_LOCK_GUARD(pipeline.mutex, lock);

		auto it = pipeline.pipelines.find(hash);
		if (it == pipeline.pipelines.end())
		{

			// Shader Stages
			VkPipelineShaderStageCreateInfo shaderStages[ShaderType_GraphicsCount] = {};
			u32 shaderStagesCount = 0u;

			Shader_vk *vertex_shader = reinterpret_cast<Shader_vk *>(state.vertex_shader);
			Shader_vk *pixel_shader = reinterpret_cast<Shader_vk *>(state.pixel_shader);
			Shader_vk *geometry_shader = reinterpret_cast<Shader_vk *>(state.geometry_shader);

			if (vertex_shader == NULL)
				return VK_NULL_HANDLE;

			{
				VkPipelineShaderStageCreateInfo &stage = shaderStages[shaderStagesCount++];
				stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stage.flags = 0u;
				stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
				stage.module = vertex_shader->module;
				stage.pName = "main";
				stage.pSpecializationInfo = nullptr;
			}
			if (pixel_shader)
			{
				VkPipelineShaderStageCreateInfo &stage = shaderStages[shaderStagesCount++];
				stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stage.flags = 0u;
				stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				stage.module = pixel_shader->module;
				stage.pName = "main";
				stage.pSpecializationInfo = nullptr;
			}
			if (geometry_shader)
			{
				VkPipelineShaderStageCreateInfo &stage = shaderStages[shaderStagesCount++];
				stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stage.flags = 0u;
				stage.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
				stage.module = geometry_shader->module;
				stage.pName = "main";
				stage.pSpecializationInfo = nullptr;
			}

			// Input Layout
			VkPipelineVertexInputStateCreateInfo vertexInput{};
			vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			VkVertexInputBindingDescription bindings[GraphicsLimit_InputSlot];
			VkVertexInputAttributeDescription attributes[GraphicsLimit_InputSlot * GraphicsLimit_InputElement];
			{
				u32 element_count = 0;

				for (u32 i = 0; i < state.input_layout.slot_count; ++i)
				{
					bindings[i].binding = i;
					bindings[i].inputRate = state.input_layout.slots[i].instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
					bindings[i].stride = state.input_layout.slots[i].stride;

					for (u32 j = 0; j < state.input_layout.slots[i].element_count; ++j)
					{
						u32 att = element_count + j;

						attributes[j].binding = i;
						attributes[j].format = graphics_vulkan_parse_format(state.input_layout.slots[i].elements[j].format);
						attributes[j].location = vertex_shader->semanticNames[state.input_layout.slots[i].elements[j].name] + state.input_layout.slots[i].elements[j].index;
						attributes[j].offset = state.input_layout.slots[i].elements[j].offset;
					}

					element_count += state.input_layout.slots[i].element_count;
				}

				vertexInput.vertexBindingDescriptionCount = state.input_layout.slot_count;
				vertexInput.pVertexBindingDescriptions = bindings;
				vertexInput.pVertexAttributeDescriptions = attributes;
				vertexInput.vertexAttributeDescriptionCount = element_count;
			}

			// Rasterizer State
			VkPipelineRasterizationStateCreateInfo rasterizerStateInfo{};
			{
				rasterizerStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizerStateInfo.flags = 0u;
				rasterizerStateInfo.depthClampEnable = VK_FALSE;
				rasterizerStateInfo.rasterizerDiscardEnable = VK_FALSE;
				rasterizerStateInfo.polygonMode = state.rasterizer.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
				rasterizerStateInfo.cullMode = graphics_vulkan_parse_cullmode(state.rasterizer.cull_mode);
				// Is inverted beacuse the scene is rendered inverse and is flipped at presentation!!!
				rasterizerStateInfo.frontFace = state.rasterizer.clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
				rasterizerStateInfo.depthBiasEnable = VK_FALSE;
				rasterizerStateInfo.depthBiasConstantFactor = 0;
				rasterizerStateInfo.depthBiasClamp = 0;
				rasterizerStateInfo.depthBiasSlopeFactor = 0;
				rasterizerStateInfo.lineWidth = 1.f;
			}

			// Blend State
			VkPipelineColorBlendStateCreateInfo blendStateInfo{};
			VkPipelineColorBlendAttachmentState attachments[GraphicsLimit_AttachmentRT];
			{
				const BlendStateInfo &bDesc = state.blend_state->info;

				u32 blend_att_count = SV_MIN(bDesc.attachment_count, renderPass.info.attachment_count);

				for (u32 i = 0; i < blend_att_count; ++i)
				{
					const BlendAttachmentDesc &b = bDesc.attachments[i];

					attachments[i].blendEnable = b.blend_enabled ? VK_TRUE : VK_FALSE;
					attachments[i].srcColorBlendFactor = graphics_vulkan_parse_blendfactor(b.src_color_blend_factor);
					attachments[i].dstColorBlendFactor = graphics_vulkan_parse_blendfactor(b.dst_color_blend_factor);
					;
					attachments[i].colorBlendOp = graphics_vulkan_parse_blendop(b.color_blend_op);
					attachments[i].srcAlphaBlendFactor = graphics_vulkan_parse_blendfactor(b.src_alpha_blend_factor);
					;
					attachments[i].dstAlphaBlendFactor = graphics_vulkan_parse_blendfactor(b.dst_alpha_blend_factor);
					;
					attachments[i].alphaBlendOp = graphics_vulkan_parse_blendop(b.alpha_blend_op);
					;
					attachments[i].colorWriteMask = graphics_vulkan_parse_colorcomponent(b.color_write_mask);
				}

				blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				blendStateInfo.flags = 0u;
				blendStateInfo.logicOpEnable = VK_FALSE;
				blendStateInfo.logicOp;
				blendStateInfo.attachmentCount = blend_att_count;
				blendStateInfo.pAttachments = attachments;
				blendStateInfo.blendConstants[0] = bDesc.blend_constants.x;
				blendStateInfo.blendConstants[1] = bDesc.blend_constants.y;
				blendStateInfo.blendConstants[2] = bDesc.blend_constants.z;
				blendStateInfo.blendConstants[3] = bDesc.blend_constants.w;
			}

			// DepthStencilState
			VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{};
			{
				const DepthStencilStateInfo &dDesc = state.depth_stencil_state->info;
				depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				depthStencilStateInfo.flags = 0u;
				depthStencilStateInfo.depthTestEnable = dDesc.depth_test_enabled;
				depthStencilStateInfo.depthWriteEnable = dDesc.depth_write_enabled;
				depthStencilStateInfo.depthCompareOp = graphics_vulkan_parse_compareop(dDesc.depth_compare_op);

				depthStencilStateInfo.stencilTestEnable = dDesc.stencil_test_enabled;
				depthStencilStateInfo.front.failOp = graphics_vulkan_parse_stencilop(dDesc.front.fail_op);
				depthStencilStateInfo.front.passOp = graphics_vulkan_parse_stencilop(dDesc.front.pass_op);
				depthStencilStateInfo.front.depthFailOp = graphics_vulkan_parse_stencilop(dDesc.front.depth_fail_op);
				depthStencilStateInfo.front.compareOp = graphics_vulkan_parse_compareop(dDesc.front.compare_op);
				depthStencilStateInfo.front.compareMask = dDesc.read_mask;
				depthStencilStateInfo.front.writeMask = dDesc.write_mask;
				depthStencilStateInfo.front.reference = 0u;
				depthStencilStateInfo.back.failOp = graphics_vulkan_parse_stencilop(dDesc.back.fail_op);
				depthStencilStateInfo.back.passOp = graphics_vulkan_parse_stencilop(dDesc.back.pass_op);
				depthStencilStateInfo.back.depthFailOp = graphics_vulkan_parse_stencilop(dDesc.back.depth_fail_op);
				depthStencilStateInfo.back.compareOp = graphics_vulkan_parse_compareop(dDesc.back.compare_op);
				depthStencilStateInfo.back.compareMask = dDesc.read_mask;
				depthStencilStateInfo.back.writeMask = dDesc.write_mask;
				depthStencilStateInfo.back.reference = 0u;

				depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
				depthStencilStateInfo.minDepthBounds = 0.f;
				depthStencilStateInfo.maxDepthBounds = 1.f;
			}

			// Topology
			VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = graphics_vulkan_parse_topology(state.topology);

			// ViewportState
			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.flags = 0u;
			viewportState.viewportCount = GraphicsLimit_Viewport;
			viewportState.pViewports = nullptr;
			viewportState.scissorCount = GraphicsLimit_Scissor;
			viewportState.pScissors = nullptr;

			// MultisampleState
			VkPipelineMultisampleStateCreateInfo multisampleState{};
			multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleState.flags = 0u;
			multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleState.sampleShadingEnable = VK_FALSE;
			multisampleState.minSampleShading = 1.f;
			multisampleState.pSampleMask = nullptr;
			multisampleState.alphaToCoverageEnable = VK_FALSE;
			multisampleState.alphaToOneEnable = VK_FALSE;

			// Dynamic States
			VkDynamicState dynamicStates[] = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_STENCIL_REFERENCE,
				VK_DYNAMIC_STATE_LINE_WIDTH};

			VkPipelineDynamicStateCreateInfo dynamicStatesInfo{};
			dynamicStatesInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStatesInfo.dynamicStateCount = 4u;
			dynamicStatesInfo.pDynamicStates = dynamicStates;

			// Creation
			VkGraphicsPipelineCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			create_info.flags = 0u;
			create_info.stageCount = shaderStagesCount;
			create_info.pStages = shaderStages;
			create_info.pVertexInputState = &vertexInput;
			create_info.pInputAssemblyState = &inputAssembly;
			create_info.pTessellationState = nullptr;
			create_info.pViewportState = &viewportState;
			create_info.pRasterizationState = &rasterizerStateInfo;
			create_info.pMultisampleState = &multisampleState;
			create_info.pDepthStencilState = &depthStencilStateInfo;
			create_info.pColorBlendState = &blendStateInfo;
			create_info.pDynamicState = &dynamicStatesInfo;
			create_info.layout = pipeline.layout;
			create_info.renderPass = renderPass.renderPass;
			create_info.subpass = 0u;

			vkAssert(vkCreateGraphicsPipelines(gfx.device, VK_NULL_HANDLE, 1u, &create_info, nullptr, &res));
			pipeline.pipelines[hash] = res;
		}
		else
		{
			res = it->second;
		}

		pipeline.lastUsage = timer_now();

		return res;
	}

}

#endif