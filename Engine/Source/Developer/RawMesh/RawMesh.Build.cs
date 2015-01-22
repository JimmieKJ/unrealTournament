// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RawMesh : ModuleRules
{
    public RawMesh(TargetInfo Target)
	{
        PublicIncludePaths.AddRange(new string[] {"Developer/RawMesh/Public"});
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });
	}
}