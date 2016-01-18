// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayDebuggerModule : ModuleRules
	{
        public GameplayDebuggerModule(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
                    "Runtime/Engine/Public",
                    "Runtime/AssetRegistry/Public",
  				    "Developer/GameplayDebugger/Public",
                    "Developer/Settings/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
                    "Runtime/Engine/Private",
                    "Runtime/AssetRegistry/Private",
					"Developer/GameplayDebugger/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "InputCore",
					"Engine",    
                    "RenderCore",
                    "RHI",
                    "ShaderCore",
                    "Settings",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "AssetRegistry"
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

            if (UEBuildConfiguration.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
                PrivateDependencyModuleNames.Add("LevelEditor");
                PrivateDependencyModuleNames.Add("SlateCore");
                PrivateDependencyModuleNames.Add("Slate");
                PrivateDependencyModuleNames.Add("PropertyEditor");
                PrivateDependencyModuleNames.Add("DetailCustomizations");
                PrivateDependencyModuleNames.Add("SourceControl");
              
                PublicIncludePaths.Add("Editor/LevelEditor/Public");
                PublicIncludePaths.Add("Editor/DetailCustomizations/Public");
                PublicIncludePaths.Add("Editor/PropertyEditor/Public");
                PublicIncludePaths.Add("Developer/SourceControl/Public");

                PrivateIncludePaths.Add("Editor/DetailCustomizations/Private");
                PrivateIncludePaths.Add("Editor/PropertyEditor/Private");
            }
        }
	}
}