// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OculusLibrary : ModuleRules
	{
		public OculusLibrary(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"OculusLibrary/Private",
 					"../../../../Source/Runtime/Renderer/Private",
 					"../../../../Source/ThirdParty/Oculus/Common",
					"../../OculusRift/Source/OculusRift/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicIncludePathModuleNames.Add("OculusRift");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"Slate",
					"SlateCore",
					"UtilityShaders",
				}
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}