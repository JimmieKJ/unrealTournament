// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Launch : ModuleRules
{
	public Launch(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AutomationController",
				"TaskGraph",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"MediaAssets",
                "MoviePlayer",
				"Networking",
				"PakFile",
				"Projects",
				"RenderCore",
				"RHI",
				"SandboxFile",
				"Serialization",
				"ShaderCore",
				"Slate",
				"SlateCore",
				"Sockets",
			}
		);

		// Enable the LauncherCheck module to be used for platforms that support the Launcher.
        // Projects should set UEBuildConfiguration.bUseLauncherChecks in their Target.cs to enable the functionality.
        if (UEBuildConfiguration.bUseLauncherChecks &&
            ((Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Mac)))
		{
            PrivateDependencyModuleNames.Add("LauncherCheck");
            Definitions.Add("WITH_LAUNCHERCHECK=1");
		}

		if (Target.Type != TargetRules.TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"HeadMountedDisplay",
				}
			);

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("D3D12RHI");
				DynamicallyLoadedModuleNames.Add("D3D11RHI");
				DynamicallyLoadedModuleNames.Add("XAudio2");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.Add("CoreAudio");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				DynamicallyLoadedModuleNames.Add("ALAudio");
				PrivateDependencyModuleNames.Add("Json");
			}

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

		// UFS clients are not available in shipping builds
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"NetworkFile",
					"StreamingFile",
    				"AutomationWorker",
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
                "Media",
				"Renderer",
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateIncludePathModuleNames.Add("Messaging");
			PublicDependencyModuleNames.Add("SessionServices");
			PrivateIncludePaths.Add("Developer/DerivedDataCache/Public");

            // LaunchEngineLoop.cpp will still attempt to load XMPP but not all projects require it so it will silently fail unless referenced by the project's build.cs file.
            // DynamicallyLoadedModuleNames.Add("XMPP");
            DynamicallyLoadedModuleNames.Add("HTTP");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicIncludePathModuleNames.Add("ProfilerService");
			DynamicallyLoadedModuleNames.AddRange(new string[] { "TaskGraph", "RealtimeProfiler", "ProfilerService" });
		}
		
		// The engine can use AutomationController in any connfiguration besides shipping.  This module is loaded
		// dynamically in LaunchEngineLoop.cpp in non-shipping configurations
		if (UEBuildConfiguration.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] { "AutomationController" });
		}

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			PublicIncludePathModuleNames.Add("ProfilerClient");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SourceControl",
					"UnrealEd",
					"DesktopPlatform"
				}
			);


			// ExtraModules that are loaded when WITH_EDITOR=1 is true
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AutomationWindow",
					"ProfilerClient",
					"Toolbox",
					"GammaUI",
					"ModuleUI",
					"OutputLog",
					"TextureCompressor",
					"MeshUtilities",
					"SourceCodeAccess"
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("MainFrame");
				PrivateDependencyModuleNames.Add("Settings");
			}
			else
			{
				DynamicallyLoadedModuleNames.Add("MainFrame");
			}
		}

		if (Target.Platform == UnrealTargetPlatform.IOS ||
			Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PrivateDependencyModuleNames.Add("OpenGLDrv");
			PrivateDependencyModuleNames.Add("IOSAudio");
			DynamicallyLoadedModuleNames.Add("IOSRuntimeSettings");
			DynamicallyLoadedModuleNames.Add("IOSLocalNotification");
			PublicFrameworks.Add("OpenGLES");
			// this is weak for IOS8 support for CAMetalLayer that is in QuartzCore
			PublicWeakFrameworks.Add("QuartzCore");

			PrivateDependencyModuleNames.Add("LaunchDaemonMessages");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("OpenGLDrv"); 
			PrivateDependencyModuleNames.Add("AndroidAudio");
			DynamicallyLoadedModuleNames.Add("AndroidRuntimeSettings");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.Linux && Target.Type != TargetRules.TargetType.Server))
		{
            // TODO: re-enable after implementing resource tables for OpenGL.
			DynamicallyLoadedModuleNames.Add("OpenGLDrv");
		}

        if (Target.Platform == UnrealTargetPlatform.HTML5 )
        {
			PrivateDependencyModuleNames.Add("ALAudio");
			if (Target.Architecture == "-win32")
			{
                PrivateDependencyModuleNames.Add("HTML5Win32");
                PublicIncludePathModuleNames.Add("HTML5Win32");
			}
            AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
        }

		// @todo ps4 clang bug: this works around a PS4/clang compiler bug (optimizations)
		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			bFasterWithoutUnity = true;
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"LinuxCommonStartup"
				}
			);
		}
	}
}
