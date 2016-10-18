// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Landscape : ModuleRules
{
	public Landscape(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Engine/Private", // for Engine/Private/Collision/PhysXCollision.h
				"Runtime/Landscape/Private"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"DerivedDataCache",
				"Foliage",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore", 
				"RHI",
				"ShaderCore",
				"Renderer",
				"Foliage",
			}
		);

		SetupModulePhysXAPEXSupport(Target);
		if (UEBuildConfiguration.bCompilePhysX && UEBuildConfiguration.bRuntimePhysicsCooking)
		{
			DynamicallyLoadedModuleNames.Add("PhysXFormats");
			PrivateIncludePathModuleNames.Add("PhysXFormats");
		}

		if (UEBuildConfiguration.bBuildDeveloperTools && Target.Type != TargetRules.TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RawMesh"
				}
			);
		}

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			// TODO: Remove all landscape editing code from the Landscape module!!!
			PrivateIncludePathModuleNames.Add("LandscapeEditor");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities", 
					"SlateCore",
					"Slate",
				}
			);

			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
				}
			);
		}
	}
}
