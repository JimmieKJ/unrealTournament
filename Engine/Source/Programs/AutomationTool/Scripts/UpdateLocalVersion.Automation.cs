// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
		var UE4Build = new UE4Build(this);
		var ChangelistOverride = ParseParamInt("cl", -1);
		UE4Build.UpdateVersionFiles(ChangelistNumberOverride: (ChangelistOverride < 0 ? (int?)null : ChangelistOverride));
	}
}
