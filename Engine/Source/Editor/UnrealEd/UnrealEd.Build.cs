// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealEd : ModuleRules
{
	public UnrealEd(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Editor/UnrealEd/Public/UnrealEd.h";

		PrivateIncludePaths.AddRange(
			new string[] 
			{
				"Editor/UnrealEd/Private",
				"Editor/UnrealEd/Private/Settings",
				"Editor/UnrealEd/Private/FeaturePack",
				"Editor/PackagesDialog/Public",				
				"Developer/DerivedDataCache/Public",
				"Developer/TargetPlatform/Public",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
				"Analytics",
				"AssetRegistry",
				"AssetTools",
                "BehaviorTreeEditor",
				"ClassViewer",
				"ContentBrowser",
				"CrashTracker",
				"DerivedDataCache",
				"DesktopPlatform",
                "EnvironmentQueryEditor",
				"GameProjectGeneration",
				"ProjectTargetPlatformEditor",
				"ImageWrapper",
				"MainFrame",
				"MaterialEditor",
                "MergeActors",
				"MeshUtilities",
				"Messaging",
				"NiagaraEditor",
				"PlacementMode",
				"Settings",
				"SettingsEditor",
				"SoundClassEditor",
				"ViewportSnapping",
				"SourceCodeAccess",
				"ReferenceViewer",
                "IntroTutorials",
                "SuperSearch",
				"OutputLog",
				"Landscape",
                "Niagara",
				"SizeMap",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] 
			{
				"BspMode",
				"Core",
				"CoreUObject",
				"DirectoryWatcher",
				"Documentation",
				"Engine",
				"Json",
				"Projects",
				"SandboxFile",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"UnrealEdMessages",
                "AIModule",
				"BlueprintGraph",
                "Http",
				"UnrealAudio",
                "Niagara",
                "VectorVM",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"AnimGraph",
                "AppFramework",
				"BlueprintGraph",
				"OnlineSubsystem",
				"OnlineBlueprintSupport",
				"DesktopPlatform",
				"EditorStyle",
				"EngineSettings",
				"InputCore",
				"InputBindingEditor",
				"Internationalization",
				"LauncherAutomatedService",
				"LauncherServices",
				"MaterialEditor",
				"MessageLog",
				"NetworkFileSystem",
				"PakFile",
				"PropertyEditor",
				"Projects",
				"RawMesh",
				"RenderCore", 
				"RHI", 
				"ShaderCore", 
				"Sockets",
				"SoundClassEditor",
				"SoundCueEditor",
				"SourceControlWindows", 
				"StatsViewer",
				"SwarmInterface",
				"TargetPlatform",
				"TargetDeviceServices",
                "EditorWidgets",
				"GraphEditor",
				"Kismet",
                "InternationalizationSettings",
                "JsonUtilities",
				"Landscape",
				"HeadMountedDisplay",
				"MeshPaint",
                "Foliage",
                "VectorVM",
				"TreeMap",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] 
			{
				"CrashTracker",
				"MaterialEditor",
				"FontEditor",
				"StaticMeshEditor",
				"TextureEditor",
				"Cascade",
                "UMGEditor",
				"Matinee",
				"AssetRegistry",
				"AssetTools",
				"ClassViewer",
				"CollectionManager",
				"ContentBrowser",
				"CurveTableEditor",
				"DataTableEditor",
				"DestructibleMeshEditor",
				"EditorSettingsViewer",
				"LandscapeEditor",
				"KismetCompiler",
				"DetailCustomizations",
				"ComponentVisualizers",
				"MainFrame",
				"LevelEditor",
				"InputBindingEditor",
				"PackagesDialog",
				"Persona",
				"PhAT",
                "ProjectLauncher",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"Sequencer",
				"SoundClassEditor",
				"GeometryMode",
				"TextureAlignMode",
				"FoliageEdit",
				"PackageDependencyInfo",
				"ImageWrapper",
				"Blutility",
				"IntroTutorials",
                "SuperSearch",
				"DesktopPlatform",
				"WorkspaceMenuStructure",
				"BspMode",
				"PlacementMode",
				"NiagaraEditor",
				"MeshUtilities",
                "MergeActors",
				"GameProjectGeneration",
				"ProjectSettingsViewer",
				"ProjectTargetPlatformEditor",
				"PListEditor",
                "Documentation",
                "BehaviorTreeEditor",
                "EnvironmentQueryEditor",
				"ViewportSnapping",
				"UserFeedback",
				"GameplayTagsEditor",
                "GameplayAbilitiesEditor",
				"UndoHistory",
				"SourceCodeAccess",
				"ReferenceViewer",
				"EditorLiveStreaming",
				"HotReload",
                "IOSPlatformEditor",
				"HTML5PlatformEditor",
				"SizeMap",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicallyLoadedModuleNames.Add("AndroidPlatformEditor");
		}

		CircularlyReferencedDependentModules.AddRange(
			new string[] 
			{
                "GraphEditor",
				"Kismet",
            }
		); 


		// Add include directory for Lightmass
		PublicIncludePaths.Add("Programs/UnrealLightmass/Public");

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"UserFeedback",
             	"CollectionManager",
				"BlueprintGraph",
				"NiagaraEditor",
                "Niagara",
                "VectorVM",
			}
			);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicDependencyModuleNames.Add("XAudio2");
			PublicDependencyModuleNames.Add("UnrealAudioXAudio2");

			AddThirdPartyPrivateStaticDependencies(Target, 
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"DX11Audio"
				);

			
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDependencyModuleNames.Add("UnrealAudioCoreAudio");
		}

        if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicDependencyModuleNames.Add("HTML5Audio");
        }

		AddThirdPartyPrivateStaticDependencies(Target,
            "HACD",
            "VHACD",
			"FBX",
			"FreeType2"
		);

		SetupModulePhysXAPEXSupport(Target);

		if (UEBuildConfiguration.bCompileRecast)
		{
            PrivateDependencyModuleNames.Add("Navmesh");
			Definitions.Add( "WITH_RECAST=1" );
		}
		else
		{
			Definitions.Add( "WITH_RECAST=0" );
		}
	}
}
