// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class FBX : ModuleRules
{
	public FBX(TargetInfo Target)
	{
		Type = ModuleType.External;

		string FBXSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "FBX/2014.2.1/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					FBXSDKDir + "include",
					FBXSDKDir + "include/fbxsdk",
				}
			);


		if ( Target.Platform == UnrealTargetPlatform.Win64 )
		{
			string FBxLibPath = FBXSDKDir + "lib/vs2012/"; // VS2013 libs are not readily available for FBX, so let's just use the 2012 ones whilst we can.

			FBxLibPath += "x64/release/";
			PublicLibraryPaths.Add(FBxLibPath);

			PublicAdditionalLibraries.Add("libfbxsdk.lib");

			// We are using DLL versions of the FBX libraries
			Definitions.Add("FBXSDK_SHARED"); 
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDir = FBXSDKDir + "lib/mac/";
			PublicAdditionalLibraries.Add(LibDir + "libfbxsdk.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string LibDir = FBXSDKDir + "lib/linux/" + Target.Architecture;
			if (!Directory.Exists(LibDir))
			{
				string Err = string.Format("FBX SDK not found in {0}", LibDir);
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			PublicAdditionalLibraries.Add(LibDir + "/libfbxsdk.a");
			/* There is a bug in fbxarch.h where is doesn't do the check
			 * for clang under linux */
			Definitions.Add("FBXSDK_COMPILER_CLANG");
		}
	}
}

