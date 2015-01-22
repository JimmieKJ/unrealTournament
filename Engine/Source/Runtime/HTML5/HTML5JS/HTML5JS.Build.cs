// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTML5JS : ModuleRules
{
	// does not depend on any unreal modules.
	// UBT doesn't automatically understand .js code and the fact that it needs to be linked in or not. 
	public HTML5JS(TargetInfo Target)
	{
		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture != "-win32")
		{
			PublicAdditionalLibraries.Add("Runtime/HTML5/HTML5JS/Private/HTML5JavaScriptFx.js");
		}
	}
}
