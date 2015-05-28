// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D11RHI : ModuleRules
{
	public D3D11RHI(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders",
			}
			);

		AddThirdPartyPrivateStaticDependencies(Target, "DX11");
        AddThirdPartyPrivateStaticDependencies(Target, "NVAPI");
        AddThirdPartyPrivateStaticDependencies(Target, "AMD");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}
	}
}
