// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(TargetInfo Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_D3DX_LIBS=1");

		string DirectXSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}

		// If we're targeting Windows XP, then always delay-load D3D11 as it won't exist on that architecture
		if (WindowsPlatform.IsWindowsXPSupported())
		{
			PublicDelayLoadDLLs.AddRange( new string[] {
				"d3d11.dll", 
				"dxgi.dll" 
			} );
		}

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"dxgi.lib",
				"d3d9.lib",
				"d3d11.lib",
				"dxguid.lib",
				"d3dcompiler.lib",
                (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d3dx11d.lib" : "d3dx11.lib",				
				"dinput8.lib",
				"X3DAudio.lib",
				"xapobase.lib",
				"XAPOFX.lib"
			}
			);
	}
}

