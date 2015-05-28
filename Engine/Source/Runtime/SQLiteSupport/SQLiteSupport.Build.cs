// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SQLiteSupport : ModuleRules
	{
		public SQLiteSupport(TargetInfo Target)
		{
			string PlatformName = "";
			string ConfigurationName = "";

			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win32:
					PlatformName = "Win32/";
					break;
				case UnrealTargetPlatform.Win64:
					PlatformName = "x64/";
					break;
			
			    case UnrealTargetPlatform.IOS:
                    PlatformName = "IOS/";
                    break;
                case UnrealTargetPlatform.Mac:
                    PlatformName = "Mac/";
                    break;
                case UnrealTargetPlatform.Linux:
                    PlatformName = "Linux/";
                    break;
			}

			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					ConfigurationName = "Debug/";
					break;
				case UnrealTargetConfiguration.DebugGame:
					ConfigurationName = "Debug/";
					break;
				default:
					ConfigurationName = "Release/";
					break;
			}

			if (UnrealBuildTool.RunningRocket())
			{
				throw new BuildException("This module requires a source code build of the engine from Github. Please refer to the Engine/Source/ThirdParty/sqlite/README.txt file prior to enabling this module.");
			}
		
			string LibraryPath = "" + UEBuildConfiguration.UEThirdPartySourceDirectory + "sqlite/lib/" + PlatformName + ConfigurationName;
			string LibraryFilename = Path.Combine(LibraryPath, "sqlite" + UEBuildPlatform.GetBuildPlatform(Target.Platform).GetBinaryExtension(UEBuildBinaryType.StaticLibrary));
			if (!File.Exists(LibraryFilename))
			{
				throw new BuildException("Please refer to the Engine/Source/ThirdParty/sqlite/README.txt file prior to enabling this module.");
			}

			PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "sqlite/sqlite/");

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"DatabaseSupport",
				}
			);

			// Lib file
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryFilename);
		}
	}
}