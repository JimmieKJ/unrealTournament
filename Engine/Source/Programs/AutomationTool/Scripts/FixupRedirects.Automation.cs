// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

class FixupRedirects : BuildCommand
{
	public override void ExecuteBuild()
	{
		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");
		Log("********** Running FixupRedirects: {0} -run=FixupRedirects -unattended -nopause -buildmachine -forcelogflush", EditorExe);
		var RunResult = Run(EditorExe, String.Format("-run=FixupRedirects -unattended -nopause -buildmachine -forcelogflush -autosubmit"));
		if (RunResult.ExitCode != 0)
		{
			throw new AutomationException("BUILD FAILED: FixupRedirects failed");
		}

	}
}
