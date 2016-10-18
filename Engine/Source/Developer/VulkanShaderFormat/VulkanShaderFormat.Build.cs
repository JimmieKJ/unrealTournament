// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VulkanShaderFormat : ModuleRules
{
	public VulkanShaderFormat(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		// Do not link the module (as that would require the vulkan dll), only the include paths
		PrivateIncludePaths.Add("Runtime/VulkanRHI/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor",
			}
			);

        // GlsLang has a shadowed variable:
        bEnableShadowVariableWarnings = false;

        AddEngineThirdPartyPrivateStaticDependencies(Target, "HLSLCC");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "GlsLang");

		if (Target.Platform != UnrealTargetPlatform.Win64 || Target.Platform != UnrealTargetPlatform.Win32 || Target.Platform != UnrealTargetPlatform.Android)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
