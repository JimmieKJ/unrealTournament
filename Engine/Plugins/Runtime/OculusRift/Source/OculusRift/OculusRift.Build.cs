// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OculusRift : ModuleRules
	{
		public OculusRift(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"OculusRift/Private",
 					"../../../../Source/Runtime/Renderer/Private",
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
					"HeadMountedDisplay",
					"Slate",
					"SlateCore",
				}
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

            // Currently, the Rift is only supported on windows and mac platforms
            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
				PrivateDependencyModuleNames.AddRange(new string[] { "LibOVR", "OpenGLDrv" });

                // Add direct rendering dependencies on a per-platform basis
                PrivateDependencyModuleNames.AddRange(new string[] { "D3D11RHI" });
                PrivateIncludePaths.AddRange(
                        new string[] {
					        "OculusRift/Private",
 					        "../../../../Source/Runtime/Windows/D3D11RHI/Private",
 					        "../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
					        // ... add other private include paths required here ...
    				        }
                        );

           		AddThirdPartyPrivateStaticDependencies(Target, "OpenGL");

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