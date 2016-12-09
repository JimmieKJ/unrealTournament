// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class VulkanRHI : ModuleRules
{
	public VulkanRHI(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/Vulkan/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"RHI", 
				"RenderCore", 
				"ShaderCore",
				"UtilityShaders",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			bool bSDKInstalled = !String.IsNullOrEmpty(VulkanSDKPath);
			if (bSDKInstalled)
			{
				// If the user has an installed SDK, use that instead
				PrivateIncludePaths.Add(VulkanSDKPath + "/Include");
				// Older SDKs have an extra subfolder
				PrivateIncludePaths.Add(VulkanSDKPath + "/Include/vulkan");

				if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Source/lib32");
				}
				else
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Source/lib");
				}

				PublicAdditionalLibraries.Add("vulkan-1.lib");
				PublicAdditionalLibraries.Add("vkstatic.1.lib");
			}
			else
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			}
		}
		else
		{
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");

			bool bHaveVulkan = false;
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Note: header is the same for all architectures so just use arch-arm
				string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
				string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

				// Use NDK Vulkan header if discovered, or VulkanSDK if available
				if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
				{
					bHaveVulkan = true;
					PrivateIncludePaths.Add(NDKVulkanIncludePath);
				}
				else
				if (!String.IsNullOrEmpty(VulkanSDKPath))
				{
					// If the user has an installed SDK, use that instead
					bHaveVulkan = true;
					PrivateIncludePaths.Add(VulkanSDKPath + "/Include/vulkan");
				}
				else
				{
					// Fall back to the Windows Vulkan SDK (the headers are the same)
					bHaveVulkan = true;
					PrivateIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Vulkan/Windows/Include/vulkan");
				}
			}
			else if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
				PrivateIncludePaths.Add(VulkanSDKPath + "/Include");

				if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Bin32");
				}
				else
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Bin");
				}

				PublicAdditionalLibraries.Add("vulkan-1.lib");
				PublicAdditionalLibraries.Add("vkstatic.1.lib");
			}

			if (bHaveVulkan)
			{
				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePathModuleNames.AddRange(
						new string[]
						{
                            "TaskGraph",
						}
					);
				}
			}
			else
			{
				PrecompileForTargets = PrecompileTargetsType.None;
			}
		}
	}
}
