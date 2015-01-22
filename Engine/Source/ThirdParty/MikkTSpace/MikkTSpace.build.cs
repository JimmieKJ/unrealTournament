// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MikkTSpace : ModuleRules
{
	public MikkTSpace(TargetInfo Target)
	{
		Type = ModuleType.External;

		string MikkTSpacePath = UEBuildConfiguration.UEThirdPartySourceDirectory + "MikkTSpace/";

		PublicIncludePaths.Add(MikkTSpacePath + "inc/");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(MikkTSpacePath + "lib/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()); 
			PublicAdditionalLibraries.Add("MikkTSpace.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(MikkTSpacePath + "lib/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()); 
			PublicAdditionalLibraries.Add("MikkTSpace.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Linux/" + Target.Architecture + "/libMikkTSpace.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Mac/libMikkTSpace.a");
		}
	}
}
