// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
				"Editor/PackagesDialog/Public",				
				"Developer/DerivedDataCache/Public",
				"Developer/TargetPlatform/Public",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
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
				"MovieSceneCapture",
				"NiagaraEditor",
				"PlacementMode",
				"Settings",
				"SettingsEditor",
                "SuperSearch",
                "SoundClassEditor",
				"ViewportSnapping",
				"SourceCodeAccess",
				"ReferenceViewer",
                "IntroTutorials",
				"OutputLog",
				"Landscape",
                "Niagara",
				"SizeMap",
                "LocalizationService",
                "HierarchicalLODUtilities",
                "MessagingRpc",
                "PortalRpc",
                "PortalServices",
                "BlueprintNativeCodeGen",
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
				"GameplayDebugger",
				"BlueprintGraph",
                "Http",
				"UnrealAudio",
                "FunctionalTesting",
				"AutomationController",
				"Internationalization",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
                "LevelSequence",
				"AnimGraph",
                "AppFramework",
				"BlueprintGraph",
				"DesktopPlatform",
				"EditorStyle",
				"EngineSettings",
				"InputCore",
				"InputBindingEditor",
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
                "MaterialUtilities",
                "LocalizationService",
				"AddContentDialog",
				"GameProjectGeneration",
                "HierarchicalLODUtilities",
				"Analytics",
                "AnalyticsET",
                "PluginWarden",
                "PixelInspectorModule",
				"MovieScene",
				"MovieSceneTracks",
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] 
			{
				"CrashTracker",
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
				"PackagesDialog",
				"Persona",
				"PhAT",
                "ProjectLauncher",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"Sequencer",
                "SuperSearch",
                "GeometryMode",
				"TextureAlignMode",
				"FoliageEdit",
				"PackageDependencyInfo",
				"ImageWrapper",
				"Blutility",
				"IntroTutorials",
				"WorkspaceMenuStructure",
				"PlacementMode",
				"NiagaraEditor",
				"MeshUtilities",
                "MergeActors",
				"ProjectSettingsViewer",
				"ProjectTargetPlatformEditor",
				"PListEditor",
                "BehaviorTreeEditor",
                "EnvironmentQueryEditor",
				"ViewportSnapping",
				"UserFeedback",
				"GameplayTagsEditor",
                "GameplayTasksEditor",
                "GameplayAbilitiesEditor",
				"UndoHistory",
				"SourceCodeAccess",
				"ReferenceViewer",
				"EditorLiveStreaming",
				"HotReload",
                "IOSPlatformEditor",
				"HTML5PlatformEditor",
				"SizeMap",
                "PortalProxies",
                "PortalServices",
                "GeometryCacheEd",
                "BlueprintNativeCodeGen",
				"VREditor",
                "EditorAutomation",
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
				"AddContentDialog",                
                "MeshUtilities"
			}
			);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicDependencyModuleNames.Add("XAudio2");
			PublicDependencyModuleNames.Add("UnrealAudioXAudio2");

			AddEngineThirdPartyPrivateStaticDependencies(Target, 
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
			PublicDependencyModuleNames.Add("ALAudio");
        }

		AddEngineThirdPartyPrivateStaticDependencies(Target,
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
