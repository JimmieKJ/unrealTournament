// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;

/// <summary>
/// Helper command used for rebuilding a projects light maps.
/// </summary>
/// <remarks>
/// Command line parameters used by this command:
/// -
/// </remarks>
namespace AutomationScripts.Automation
{
	public class RebuildLightMaps : BuildCommand
	{
		#region RebuildLightMaps Command

		public override void ExecuteBuild()
		{
			var Params = new ProjectParams
			(
				Command: this,
				// Shared
				RawProjectPath: ProjectPath
			);


			Log("********** REBUILD LIGHT MAPS COMMAND STARTED **********");

			string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
			if (!FileExists(UE4EditorExe))
			{
				throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
			}

			try
			{
				var CommandletParams = IsBuildMachine ? "-buildmachine -fileopenlog" : "-fileopenlog";
				RebuildLightMapsCommandlet(Params.RawProjectPath, Params.UE4Exe, Params.MapsToRebuildLightMaps.ToArray(), CommandletParams);
			}
			catch (Exception Ex)
			{
				if (Params.IgnoreLightMapErrors)
				{
					LogWarning("Ignoring Rebuild Light Map failure.");
				}
				else
				{
					// Abandon this run, dont check in any updated files, etc.
					Log("Rebuild Light Maps has failed.");
					//CleanupCookedData(PlatformsToCook.ToList(), Params);
					throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "RebuildLightMaps failed.");
				}
			}
			Log("********** COOK COMMAND COMPLETED **********");
		}

		#endregion


		private FileReference ProjectFullPath;
		public virtual FileReference ProjectPath
		{
			get
			{
				if (ProjectFullPath == null)
				{
					var OriginalProjectName = ParseParamValue("project", "");
					var ProjectName = OriginalProjectName;
					ProjectName = ProjectName.Trim(new char[] { '\"' });
					if (ProjectName.IndexOfAny(new char[] { '\\', '/' }) < 0)
					{
						ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName, ProjectName + ".uproject");
					}
					else if (!FileExists_NoExceptions(ProjectName))
					{
						ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName);
					}
					if (FileExists_NoExceptions(ProjectName))
					{
						ProjectFullPath = new FileReference(ProjectName);
					}
					else
					{
						var Branch = new BranchInfo(new List<UnrealTargetPlatform> { UnrealBuildTool.BuildHostPlatform.Current.Platform });
						var GameProj = Branch.FindGame(OriginalProjectName);
						if (GameProj != null)
						{
							ProjectFullPath = GameProj.FilePath;
						}
						if (!FileExists_NoExceptions(ProjectFullPath.FullName))
						{
							throw new AutomationException("Could not find a project file {0}.", ProjectName);
						}
					}
				}
				return ProjectFullPath;
			}
		}
	}
}