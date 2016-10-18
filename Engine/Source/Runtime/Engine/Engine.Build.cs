// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Engine : ModuleRules
{
	public Engine(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Runtime/Engine/Public/Engine.h";

		PublicIncludePathModuleNames.AddRange(new string[] { "Renderer", "PacketHandler" });

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DerivedDataCache/Public",
                "Runtime/SynthBenchmark/Public",
                "Runtime/Engine/Private",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {				
				"TargetPlatform",
				"ImageWrapper",
				"HeadMountedDisplay",
				"Advertising",
				"NetworkReplayStreaming",
				"MovieSceneCapture",
				"AutomationWorker",
				"MovieSceneCapture",
				"DesktopPlatform",
            }
		);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);
		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Json",
				"SlateCore",
				"Slate",
				"InputCore",
				"Messaging",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"AssetRegistry", // Here until FAssetData is moved to engine
				"EngineMessages",
				"EngineSettings",
				"SynthBenchmark",
                "AIModule",
				"DatabaseSupport",
                "PacketHandler",
				"HardwareSurvey",
            }
        );

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "CrashTracker" });
			DynamicallyLoadedModuleNames.AddRange(new string[] { "CrashTracker" });
			PublicDependencyModuleNames.AddRange(
				new string[] {
			}
			);
		}


		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Networking",
				"Sockets",
				"Landscape",
                "UMG",
				"Projects",
                "Internationalization",
                "MaterialShaderQualitySettings",
                "CinematicCamera",
				"Analytics",
				"AnalyticsET",
            }
		);

        // to prevent "causes WARNING: Non-editor build cannot depend on non-redistributable modules."
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            // for now we depend on this 
            PrivateDependencyModuleNames.Add("RawMesh");
        }

        bool bVariadicTemplatesSupported = true;
		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				if (VersionName.ToString().Equals("2012"))
				{
					bVariadicTemplatesSupported = false;
				}
			}
		}

		if (bVariadicTemplatesSupported)
        {
            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "MessagingRpc",
                    "PortalRpc",
                    "PortalServices",
                }
            );

            if (Target.Type == TargetRules.TargetType.Editor)
            {
            // these modules require variadic templates
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "MessagingRpc",
                    "PortalRpc",
                    "PortalServices",
                }
            );
        }
        }

        CircularlyReferencedDependentModules.Add("AIModule");
		CircularlyReferencedDependentModules.Add("Landscape");
        CircularlyReferencedDependentModules.Add("UMG");
        CircularlyReferencedDependentModules.Add("MaterialShaderQualitySettings");
        CircularlyReferencedDependentModules.Add("CinematicCamera");

        // The AnimGraphRuntime module is not needed by Engine proper, but it is loaded in LaunchEngineLoop.cpp,
        // and needs to be listed in an always-included module in order to be compiled into standalone games
        DynamicallyLoadedModuleNames.Add("AnimGraphRuntime");

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"MovieScene",
				"MovieSceneCapture",
				"MovieSceneTracks",
				"HeadMountedDisplay",
				"StreamingPauseRendering",
			}
		);

        PrivateIncludePathModuleNames.Add("LightPropagationVolumeRuntime");

		if (Target.Type != TargetRules.TargetType.Server)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);
		}

		if (Target.Type == TargetRules.TargetType.Server || Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("PerfCounters");
		}

		if (UEBuildConfiguration.bBuildDeveloperTools)
		{
			// Add "BlankModule" so that it gets compiled as an example and will be maintained and tested.  This can be removed
			// at any time if needed.  The module isn't actually loaded by the engine so there is no runtime cost.
			DynamicallyLoadedModuleNames.Add("BlankModule");

			if (Target.Type != TargetRules.TargetType.Server)
			{
				PrivateIncludePathModuleNames.Add("MeshUtilities");
				DynamicallyLoadedModuleNames.Add("MeshUtilities");

				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"ImageCore",
						"RawMesh"
					});
			}

			if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test && Target.Type != TargetRules.TargetType.Server)
			{
				PrivateDependencyModuleNames.Add("CollisionAnalyzer");
				CircularlyReferencedDependentModules.Add("CollisionAnalyzer");

                PrivateDependencyModuleNames.Add("LogVisualizer");
                CircularlyReferencedDependentModules.Add("LogVisualizer");
            }

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"WindowsTargetPlatform",
						"WindowsNoEditorTargetPlatform",
						"WindowsServerTargetPlatform",
						"WindowsClientTargetPlatform",
						"AllDesktopTargetPlatform",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
					    "MacTargetPlatform",
					    "MacNoEditorTargetPlatform",
						"MacServerTargetPlatform",
						"MacClientTargetPlatform",
						"AllDesktopTargetPlatform",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"LinuxTargetPlatform",
						"LinuxNoEditorTargetPlatform",
						"LinuxServerTargetPlatform",
						"LinuxClientTargetPlatform",
						"AllDesktopTargetPlatform",
					}
				);
			}
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"NetworkReplayStreaming",
				"NullNetworkReplayStreaming",
				"HttpNetworkReplayStreaming",
				"Advertising"
			}
		);

		if (Target.Type.Value != TargetRules.TargetType.Server)
        {
		    DynamicallyLoadedModuleNames.AddRange(
			    new string[] {
				    "ImageWrapper",
					"GameLiveStreaming"
			    }
		    );
        }

		if (!UEBuildConfiguration.bBuildRequiresCookedData && UEBuildConfiguration.bCompileAgainstEngine)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"DerivedDataCache", 
					"TargetPlatform",
                    "DesktopPlatform"
				}
			);
		}

		if (UEBuildConfiguration.bBuildEditor == true)
		{
                
			PublicDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd", 
                    "Kismet"
                }
            );	// @todo api: Only public because of WITH_EDITOR and UNREALED_API

			CircularlyReferencedDependentModules.AddRange(
                new string[] {
                    "UnrealEd",
                    "Kismet"
                }
            );

			PrivateIncludePathModuleNames.Add("TextureCompressor");
			PrivateIncludePaths.Add("Developer/TextureCompressor/Public");

            PrivateIncludePathModuleNames.Add("HierarchicalLODUtilities");
            DynamicallyLoadedModuleNames.Add("HierarchicalLODUtilities");
        }

		SetupModulePhysXAPEXSupport(Target);
        if(UEBuildConfiguration.bCompilePhysX && UEBuildConfiguration.bRuntimePhysicsCooking)
        {
            DynamicallyLoadedModuleNames.Add("PhysXFormats");
            PrivateIncludePathModuleNames.Add("PhysXFormats");
        }
            

		SetupModuleBox2DSupport(Target);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"libOpus",
			    "OpenSubdiv"
				);

			if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectShow");
			}

            // Head Mounted Display support
//            PrivateIncludePathModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
//            DynamicallyLoadedModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
		}

		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
			AddEngineThirdPartyPrivateStaticDependencies(Target, 
                    "UEOgg",
                    "Vorbis",
                    "VorbisFile"
                    );
        }
		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture != "-win32")
		{
			PublicDependencyModuleNames.Add("HTML5JS");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"UEOgg",
				"Vorbis",
				"libOpus"
				);
			PublicFrameworks.AddRange(new string[] { "AVFoundation", "CoreVideo", "CoreMedia" });
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
        {
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile"
				);
        }

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"libOpus"
				);
		}

		if (UEBuildConfiguration.bCompileRecast)
		{
            PrivateDependencyModuleNames.Add("Navmesh");
            Definitions.Add("WITH_RECAST=1");
		}
		else
		{
			// Because we test WITH_RECAST in public Engine header files, we need to make sure that modules
			// that import this also have this definition set appropriately.  Recast is a private dependency
			// module, so it's definitions won't propagate to modules that import Engine.
			Definitions.Add("WITH_RECAST=0");
		}

		// Add a reference to the stats HTML files referenced by UEngine::DumpFPSChartToHTML. Previously staged by CopyBuildToStagingDirectory.
		if(UEBuildConfiguration.bBuildEditor || Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			RuntimeDependencies.Add("$(EngineDir)/Content/Stats/...", StagedFileType.UFS);
		}

        DynamicallyLoadedModuleNames.Add("Niagara");
    }
}
