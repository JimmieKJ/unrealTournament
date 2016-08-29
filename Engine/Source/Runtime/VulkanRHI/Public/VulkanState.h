// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.h: Vulkan state definitions.
=============================================================================*/

#pragma once

class FVulkanDevice;

class FVulkanSamplerState : public FRHISamplerState
{
public:
	FVulkanSamplerState(const FSamplerStateInitializerRHI& Initializer, FVulkanDevice& InDevice);
	~FVulkanSamplerState();

	VkSampler Sampler;
	FVulkanDevice& Device;
};

class FVulkanRasterizerState : public FRHIRasterizerState
{
public:
	FVulkanRasterizerState(const FRasterizerStateInitializerRHI& Initializer);

	static void ResetCreateInfo(VkPipelineRasterizationStateCreateInfo& OutInfo)
	{
		FMemory::Memzero(OutInfo);
		OutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		OutInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		OutInfo.lineWidth = 1.0f;
	}

	VkPipelineRasterizationStateCreateInfo RasterizerState;
};

class FVulkanDepthStencilState : public FRHIDepthStencilState
{
public:
	FVulkanDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer);

	VkPipelineDepthStencilStateCreateInfo DepthStencilState;
	uint8 FrontStencilKey;
	uint8 BackStencilKey;

protected:
	uint8 StencilStateToKey(const VkStencilOpState& State);
		
	// this tracks blend settings (in a bit flag) into a unique key that uses few bits, for PipelineState MRT setup
	static TMap<uint64, uint8> StencilSettingsToUniqueKeyMap;
	static uint8 NextKey;
};

class FVulkanBlendState : public FRHIBlendState
{
public:
	FVulkanBlendState(const FBlendStateInitializerRHI& Initializer);

	// array the pipeline state can point right to
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
	uint8 BlendStateKeys[MaxSimultaneousRenderTargets];

protected:
	// this tracks blend settings (in a bit flag) into a unique key that uses few bits, for PipelineState MRT setup
	static TMap<uint32, uint8> BlendSettingsToUniqueKeyMap;
	static uint8 NextKey;
};
