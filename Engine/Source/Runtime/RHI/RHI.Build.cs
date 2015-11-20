// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class RHI : ModuleRules
{
	public RHI(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
            DynamicallyLoadedModuleNames.Add("NullDrv");

			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("D3D11RHI");

				//#todo-rco: D3D12 requires different SDK headers not compatible with WinXP
				DynamicallyLoadedModuleNames.Add("D3D12RHI");

//#todo-rco: Remove when public
				string VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
				if (!String.IsNullOrEmpty(VulkanSDKPath))
				{
					DynamicallyLoadedModuleNames.Add("VulkanRHI");
				}
			}

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Mac)   ||
                (Target.Platform == UnrealTargetPlatform.Linux && Target.Type != TargetRules.TargetType.Server) ||  // @todo should servers on all platforms skip this?
                (Target.Platform == UnrealTargetPlatform.HTML5))
			{
				DynamicallyLoadedModuleNames.Add("OpenGLDrv");
			}
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}
	}
}
