// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TreeMap : ModuleRules
	{
		public TreeMap(TargetInfo Target)
		{
	        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "SlateCore" });
	        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "InputCore", "XmlParser" });
		}
	}
}
