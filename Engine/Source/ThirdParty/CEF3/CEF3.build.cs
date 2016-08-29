// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CEF3 : ModuleRules
{
	public CEF3(TargetInfo Target)
	{
		/** Mark the current version of the library */
		string CEFVersion = "3.2357.1291.g47e6d4b";
		string CEFPlatform = "";

		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			CEFPlatform = "windows64";
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			CEFPlatform = "windows32";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			CEFPlatform = "macosx64";
			CEFVersion = "3.2623.1395.g3034273";
		}

		if (CEFPlatform.Length > 0 && UEBuildConfiguration.bCompileCEF3)
		{
			Definitions.Add("WITH_CEF3=1");

			string PlatformPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "CEF3", "cef_binary_" + CEFVersion + "_" + CEFPlatform);

			PublicSystemIncludePaths.Add(PlatformPath);

			string LibraryPath = Path.Combine(PlatformPath, "Release");
            string RuntimePath = Path.Combine(UEBuildConfiguration.UEThirdPartyBinariesDirectory , "CEF3", Target.Platform.ToString());

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
                PublicLibraryPaths.Add(LibraryPath);
                PublicAdditionalLibraries.Add("libcef.lib");

                // There are different versions of the C++ wrapper lib depending on the version of VS we're using
                string VSVersionFolderName = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
                string WrapperLibraryPath = Path.Combine(PlatformPath, VSVersionFolderName, "libcef_dll");

                if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
                {
                    WrapperLibraryPath += "/Debug";
                }
                else
                {
                    WrapperLibraryPath += "/Release";
                }

                PublicLibraryPaths.Add(WrapperLibraryPath);
                PublicAdditionalLibraries.Add("libcef_dll_wrapper.lib");
                
                string[] Dlls = {
                    "d3dcompiler_43.dll",
                    "d3dcompiler_47.dll",
                    "ffmpegsumo.dll",
                    "libcef.dll",
                    "libEGL.dll",
                    "libGLESv2.dll",
                };

				PublicDelayLoadDLLs.AddRange(Dlls);

                // Add the runtime dlls to the build receipt
                foreach (string Dll in Dlls)
                {
                    RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/" + Dll));
                }
                // We also need the icu translations table required by CEF
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/icudtl.dat"));

				// Add the V8 binary data files as well
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/natives_blob.bin"));
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/CEF3/" + Target.Platform.ToString() + "/snapshot_blob.bin"));

				// For Win32 builds, we need a helper executable when running under WOW (32 bit apps under 64 bit windows)
				if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/CEF3/Win32/wow_helper.exe"));
				}

                // And the entire Resources folder. Enunerate the entire directory instead of mentioning each file manually here.
                foreach (string FileName in Directory.EnumerateFiles(Path.Combine(RuntimePath, "Resources"), "*", SearchOption.AllDirectories))
                {
                    string DependencyName = FileName.Substring(UEBuildConfiguration.UEThirdPartyBinariesDirectory.Length).Replace('\\', '/');
                    RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/" + DependencyName));
                }
            }
			// TODO: Ensure these are filled out correctly when adding other platforms
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string WrapperPath = LibraryPath + "/libcef_dll_wrapper.a";
                string FrameworkPath = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "CEF3/Mac/Chromium Embedded Framework.framework";

				PublicAdditionalLibraries.Add(WrapperPath);
                PublicFrameworks.Add(FrameworkPath);

				if(Directory.Exists(LibraryPath + "/locale"))
				{
					var LocaleFolders = Directory.GetFileSystemEntries(LibraryPath + "/locale", "*.lproj");
					foreach (var FolderName in LocaleFolders)
					{
						AdditionalBundleResources.Add(new UEBuildBundleResource(FolderName, bInShouldLog:false));
					}
				}

				// Add contents of framework directory as runtime dependencies
				foreach (string FilePath in Directory.EnumerateFiles(FrameworkPath, "*", SearchOption.AllDirectories))
				{
					RuntimeDependencies.Add(new RuntimeDependency(FilePath));
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				if (Target.IsMonolithic)
				{
					PublicAdditionalLibraries.Add(LibraryPath + "/libcef.a");
				}
				else
				{
					PublicLibraryPaths.Add(LibraryPath);
					PublicAdditionalLibraries.Add("libcef");
				}
			}
		}
	}
}
