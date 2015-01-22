// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NullDrv : ModuleRules
{
	public NullDrv(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Runtime/NullDrv/Private");

        PrivateDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("RHI");
        PrivateDependencyModuleNames.Add("RenderCore");
	}
}
