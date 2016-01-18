// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
 
public class HTML5Win32 : ModuleRules
{
	public HTML5Win32(TargetInfo Target)
	{
        // Don't depend on UE types or modules.  
		AddThirdPartyPrivateStaticDependencies(Target, "libcurl");
	}
}
