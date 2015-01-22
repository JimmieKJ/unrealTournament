// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Simplygon : ModuleRules
{
	public Simplygon(TargetInfo Target)
	{
		Type = ModuleType.External;

        Definitions.Add("SGDEPRECATED_OFF=1");

		string SimplygonPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "NotForLicensees/Simplygon/Simplygon-5.5.2156/";
		PublicIncludePaths.Add(SimplygonPath + "Inc");

		// Simplygon depends on D3DX9.
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicLibraryPaths.Add( UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX/Lib/x64" );
			}
			else
			{
				PublicLibraryPaths.Add( UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX/Lib/x86" );
			}
			PublicAdditionalLibraries.Add(
				(Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d3dx9d.lib" : "d3dx9.lib"
				);

			// Simplygon requires GetProcessMemoryInfo exported by psapi.dll. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683219(v=vs.85).aspx
			PublicAdditionalLibraries.Add("psapi.lib");
		}
	}
}

