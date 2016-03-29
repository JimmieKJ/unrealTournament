// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MetalShaderFormat : ModuleRules
{
	public MetalShaderFormat(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/Apple/MetalRHI/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"HLSLCC"
			);
	}
}
