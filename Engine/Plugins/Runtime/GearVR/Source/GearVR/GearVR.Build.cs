// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GearVR : ModuleRules
	{
		public GearVR(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[]
				{
					"GearVR/Private",
					"../../../../Source/Runtime/Renderer/Private",
					"../../../../Source/Runtime/Launch/Private",
					"../../../../Source/ThirdParty/Oculus/Common",
					"../../../../Source/Runtime/OpenGLDrv/Private",
				});

			PrivateDependencyModuleNames.AddRange(new string[]
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
					"OpenGLDrv",
					"OculusMobile",
					"UtilityShaders",
				});

			if (UEBuildConfiguration.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GearVR_APL.xml")));
			}

			PublicIncludePathModuleNames.Add("Launch");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		}
	}
}
