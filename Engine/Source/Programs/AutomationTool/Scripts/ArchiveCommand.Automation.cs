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

	#region Utilities

	public static void CreateArchiveManifest(ProjectParams Params, DeploymentContext SC)
	{
		if (!Params.Archive)
		{
			return;
		}
		var ThisPlatform = SC.StageTargetPlatform;

		ThisPlatform.GetFilesToArchive(Params, SC);

		//@todo add any archive meta data files as needed

		if (Params.ArchiveMetaData)
		{
			// archive the build.version file for extra info for testing, etc
			string BuildVersionFile = CombinePaths(SC.LocalRoot, "Engine", "Build", "Build.version");
			SC.ArchiveFiles(Path.GetDirectoryName(BuildVersionFile), Path.GetFileName(BuildVersionFile));
		}
	}

	public static void ApplyArchiveManifest(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.ArchivedFiles.Count > 0)
		{
			foreach (var Pair in SC.ArchivedFiles)
			{
				string Src = Pair.Key;
				string Dest = CombinePaths(SC.ArchiveDirectory, Pair.Value);
				CopyFileIncremental(Src, Dest);
			}
		}
	}

	#endregion

	#region Archive Command

	public static void Archive(ProjectParams Params)
	{
		Params.ValidateAndLog();
		if (!Params.Archive)
		{
			return;
		}

		Log("********** ARCHIVE COMMAND STARTED **********");

		if (!Params.NoClient)
		{
			var DeployContextList = CreateDeploymentContext(Params, false, false);
			foreach ( var SC in DeployContextList )
			{
				CreateArchiveManifest(Params, SC);
				ApplyArchiveManifest(Params, SC);
				SC.StageTargetPlatform.ProcessArchivedProject(Params, SC);
			}
		}
		if (Params.DedicatedServer)
		{
			ProjectParams ServerParams = new ProjectParams(Params);
			ServerParams.Devices = new ParamList<string>(ServerParams.ServerDevice);
			var DeployContextList = CreateDeploymentContext(ServerParams, true, false);
			foreach ( var SC in DeployContextList )
			{
				CreateArchiveManifest(Params, SC);
				ApplyArchiveManifest(Params, SC);
				SC.StageTargetPlatform.ProcessArchivedProject(Params, SC);
			}
		}
		Log("********** ARCHIVE COMMAND COMPLETED **********");
    }

	#endregion
}
