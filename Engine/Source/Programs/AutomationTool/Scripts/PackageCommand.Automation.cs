// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

public partial class Project : CommandUtils
{
	#region Package Command

	public static void Package(ProjectParams Params, int WorkingCL=-1)
	{
		Params.ValidateAndLog();
		List<DeploymentContext> DeployContextList = new List<DeploymentContext>();
		if (!Params.NoClient)
		{
			DeployContextList.AddRange(CreateDeploymentContext(Params, false, false));
		}
		if (Params.DedicatedServer)
		{
			DeployContextList.AddRange(CreateDeploymentContext(Params, true, false));
		}

		// before we package up the build, allow a symbol upload (this isn't actually tied to packaging, 
		// but logically it's a package-time thing)
		foreach (var SC in DeployContextList)
		{
			if (Params.UploadSymbols)
			{
				SC.StageTargetPlatform.UploadSymbols(Params, SC);
			}
		}

		if (DeployContextList.Count > 0 && (!Params.SkipStage || Params.Package))
		{
			Log("********** PACKAGE COMMAND STARTED **********");

			foreach (var SC in DeployContextList)
			{
				if (Params.Package || (SC.StageTargetPlatform.RequiresPackageToDeploy && Params.Deploy))
				{
					SC.StageTargetPlatform.Package(Params, SC, WorkingCL);
				}
			}

			Log("********** PACKAGE COMMAND COMPLETED **********");
		}
	}

	#endregion
}
