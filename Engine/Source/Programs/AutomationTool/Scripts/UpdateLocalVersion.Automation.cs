// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

[Help("Updates your local versions based on your P4 sync")]
[Help("CL", "Overrides the automatically disovered changelist number with the specified one")]
[Help("Licensee", "When updating version files, store the changelist number in licensee format")]
[RequireP4]
public class UpdateLocalVersion : BuildCommand
{
	public override void ExecuteBuild()
	{
		UE4Build UE4Build = new UE4Build(this);
		int? ChangelistOverride = ParseParamNullableInt("cl");
		int? CompatibleChangelistOverride = ParseParamNullableInt("compatiblecl");
		string Build = ParseParamValue("Build", null);
		UE4Build.UpdateVersionFiles(ChangelistNumberOverride: ChangelistOverride, CompatibleChangelistNumberOverride: CompatibleChangelistOverride, Build: Build);
	}
}
