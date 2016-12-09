// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrossCompilerTool : ModuleRules
{
	public CrossCompilerTool(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Developer/Apple/MetalShaderFormat/Private");		// For Metal includes
		PrivateIncludePaths.Add("Developer/ShaderFormatOpenGL/Private");		// For GLSL includes
        PrivateIncludePaths.Add("Developer/ShaderCompilerCommon/Private");      // For Lexer includes
		PrivateIncludePaths.Add("Runtime/VulkanRHI/Public");      // For Vulkan definitions/includes

		PrivateDependencyModuleNames.AddRange(new string []
			{
				"Core",
				"Projects",
				"ShaderCompilerCommon",
				"MetalShaderFormat",
				"ShaderFormatOpenGL",
				"ShaderPreprocessor",

				//@todo-rco: Remove me!
				"ShaderCore",
			});

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"HLSLCC"
		);

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
        {
            PrivateIncludePaths.Add("Developer/VulkanShaderFormat/Private");
			PrivateIncludePaths.Add("Runtime/VulkanRHI/Public");
			PrivateDependencyModuleNames.Add("VulkanShaderFormat");
        }
	}
}
