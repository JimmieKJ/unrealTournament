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
/// -project	Absolute path to a .uproject file
/// </remarks>
namespace AutomationScripts.Automation
{
	public class RebuildLightMaps : BuildCommand
	{
		#region RebuildLightMaps Command

		/**
		 * Parse the P4 output for any errors that we really care about.
		 * e.g. umaps and assets are exclusive checkout files, if we cant check out a map for this reason
		 *		then we need to stop.
		 */
		private bool FoundCheckOutErrorInP4Output(string Output)
		{
			bool bHadAnError = false;

			var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			foreach (string Line in Lines)
			{
				// Check for log spew that matches exclusive checkout failure
				// http://answers.perforce.com/articles/KB/3114
				if (Line.Contains("can't edit exclusive file already opened"))
				{
					bHadAnError = true;
					break;
				}
			}

			return bHadAnError;
		}

		/**
		 * Cleanup anything this build may leave behind
		 */
		private void HandleFailure()
		{
			if (WorkingCL != -1)
			{
				P4.RevertAll(WorkingCL);
				P4.DeleteChange(WorkingCL);
			}
		}

		public override void ExecuteBuild()
		{
			Log("********** REBUILD LIGHT MAPS COMMAND STARTED **********");

			var Params = new ProjectParams
			(
				Command: this,
				// Shared
				RawProjectPath: ProjectPath
			);

			// Setup our P4 changelist so we can submit the new lightmaps after the commandlet has completed.
			if (P4Enabled)
			{
				WorkingCL = P4.CreateChange(P4Env.Client, String.Format("{0} rebuilding lightmaps from changelist {1}", Params.ShortProjectName, P4Env.Changelist));
				Log("Working in {0}", WorkingCL);
				try
				{
					string AllMapsWildcardCmdline = String.Format("{0}\\...\\*.umap", Params.RawProjectPath.Directory);
					string Output;
					P4.P4Output(out Output, String.Format("edit -c {0} {1}", WorkingCL, AllMapsWildcardCmdline));

					// We need to ensure that any error in the output log is observed.
					// P4 is still successful if it manages to run the operation.
					if (FoundCheckOutErrorInP4Output(Output) == true)
					{
						throw new Exception();
					}
				}
				catch (Exception)
				{
					HandleFailure();
					throw new AutomationException("Failed to check out every one of the project maps.");
				}
			}
			else
			{
				throw new AutomationException("This command needs to run with P4.");
			}

			string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
			if (!FileExists(UE4EditorExe))
			{
				throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
			}

			// Now let's rebuild lightmaps for the project
			try
			{
				var CommandletParams = IsBuildMachine ? "-buildmachine -fileopenlog" : "-fileopenlog";
				RebuildLightMapsCommandlet(Params.RawProjectPath, Params.UE4Exe, Params.MapsToRebuildLightMaps.ToArray(), CommandletParams);
			}
			catch (Exception Ex)
			{
				// Something went wrong with the commandlet. Abandon this run, don't check in any updated files, etc.
				HandleFailure();

				Log("Rebuild Light Maps has failed.");
				throw new AutomationException(ExitCode.Error_Unknown, Ex, "RebuildLightMaps failed.");
			}

			// Check everything in!
			if (WorkingCL != -1)
			{
				int SubmittedCL;
				P4.Submit(WorkingCL, out SubmittedCL, true, true);
				Log("New lightmaps have been submitted in changelist {0}", SubmittedCL);
			}

			Log("********** REBUILD LIGHT MAPS COMMAND COMPLETED **********");
		}

		#endregion

		// The Chanelist used when doing the work.
		private int WorkingCL = -1;

		// Process command-line and find a project file. This is necessary for the commandlet to run successfully

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