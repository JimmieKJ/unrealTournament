// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WinRTSDK : ModuleRules
{
	public WinRTSDK(TargetInfo Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.WinRT) || (Target.Platform == UnrealTargetPlatform.WinRT_ARM))
		{
			string WindowsKits8Path = "c:/Program Files (x86)/Windows Kits/8.0/";
			PublicIncludePaths.AddRange(
				new string[] {
					Path.Combine(WindowsKits8Path, "Include", "shared"),
					Path.Combine(WindowsKits8Path, "Include", "um"),
					Path.Combine(WindowsKits8Path, "Include", "WinRT"),
				}
				);

			PublicLibraryPaths.Add(Path.Combine(WindowsKits8Path, "Lib", "win8", "um", "x64"));

			PublicAdditionalLibraries.AddRange(
				new string[] {
					"dxgi.lib",
					"dxguid.lib",
					"d3dcompiler.lib",
					"xapobase.lib",
				}
				);

			//@todo.WinRT: Add a profiling configuration type?
			// By default, use standard d3d11
			PublicAdditionalLibraries.Add("d3d11.lib");
			// Include this (d3d11i) to enable PIX captures
			//PublicAdditionalLibraries.Add("d3d11i.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DirectXSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";

			// Just include the DX11 stuff
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
			PublicAdditionalLibraries.AddRange(
				new string[] {
					"dxgi.lib",
					"d3d9.lib",
					"d3d11.lib",
					"dxguid.lib",
					"d3dcompiler.lib",
					//(Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d3dx11d.lib" : "d3dx11.lib",
					"dinput8.lib",
					"X3DAudio.lib",
					"xapobase.lib",
					"XAPOFX.lib"
				}
				);
		}
	}
}

