// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GearVR : ModuleRules
	{
		public GearVR(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"GearVR/Private",
					"../../../../Source/Runtime/Renderer/Private",
					"../../../../Source/Runtime/Launch/Private",
 					"../../../../Source/ThirdParty/Oculus/Common",
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
					"HeadMountedDisplay"
				}
				);

			PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
			AddThirdPartyPrivateStaticDependencies(Target, "OpenGL");
            PrivateIncludePaths.AddRange(
				new string[] {
					"../../../../Source/Runtime/OpenGLDrv/Private",
					// ... add other private include paths required here ...
					}
				);
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "LibOVRMobile" });
			}
		}
	}
}
