// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SteamVR : ModuleRules
	{
		public SteamVR(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"SteamVR/Private",
					"../../../../../Source/Runtime/Renderer/Private",
					// ... add other private include paths required here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"Slate",
					"SlateCore"
				}
				);
			
            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
				AddThirdPartyPrivateStaticDependencies(Target, "OpenVR");
                PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });     //@todo steamvr: multiplatform
            }
		}
	}
}
