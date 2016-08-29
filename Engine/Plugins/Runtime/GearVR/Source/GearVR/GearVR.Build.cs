// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System.IO;

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
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay"
				}
				);

            PublicIncludePathModuleNames.Add("Launch");

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "OculusMobile" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, BuildConfiguration.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GearVR_APL.xml")));
			}
            if (UEBuildConfiguration.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
            else
            {
                PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
                AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
                PrivateIncludePaths.AddRange(
                    new string[] {
                    "../../../../Source/Runtime/OpenGLDrv/Private",
                        // ... add other private include paths required here ...
                    }
                    );
            }
        }
	}
}
