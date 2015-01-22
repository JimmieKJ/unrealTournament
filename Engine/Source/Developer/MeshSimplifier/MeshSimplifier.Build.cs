// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshSimplifier : ModuleRules
{
	public MeshSimplifier(TargetInfo Target)
	{
		PublicIncludePaths.Add("Developer/MeshSimplifier/Public");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("RenderCore");
		PrivateDependencyModuleNames.Add("RawMesh");
		PrivateIncludePathModuleNames.Add("MeshUtilities");
	}
}