// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Local Build Environment. Used when not running on build machine.
	/// </summary>
	class LocalCommandEnvironment : CommandEnvironment
	{
		internal LocalCommandEnvironment()
			: base()
		{
		}

		/// <summary>
		/// Initializes the environment. Tries to autodetect all source control settings.
		/// </summary>
		/// <param name="CompilationEnv">Compilation environment</param>
		protected override void InitEnvironment()
		{
			SetUATLocation();
			SetLocalRootPath();
			SetUATSavedPath();
			SetAndCreateLogFolder();

			base.InitEnvironment();
		}

		private void SetLocalRootPath()
		{
			var LocalRootPath = CommandUtils.ConvertSeparators(PathSeparator.Slash, RootFromUATLocation(UATExe));
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.LocalRoot, LocalRootPath);
		}

		/// <summary>
		/// Finds the root path of the branch by looking in progressively higher folder locations until a file known to exist in the branch is found.
		/// </summary>
		/// <param name="UATLocation">Location of the currently executing assembly.</param>
		/// <returns></returns>
		private static string RootFromUATLocation(string UATLocation)
		{
			// pick a known file in the depot and try to build a relative path to it. This will tell us where the root path to the branch is.
			var KnownFilePathFromRoot = CommandEnvironment.KnownFileRelativeToRoot;
			var CurrentPath = CommandUtils.ConvertSeparators(PathSeparator.Slash, Path.GetDirectoryName(UATLocation));
			var UATPathRoot = CommandUtils.CombinePaths(Path.GetPathRoot(CurrentPath), KnownFilePathFromRoot);

			// walk parent dirs until we find the file or hit the path root, which means we aren't in a known location.
			while (true)
			{
				var PathToCheck = Path.GetFullPath(CommandUtils.CombinePaths(CurrentPath, KnownFilePathFromRoot));
				Log.TraceVerbose("Checking for {0}", PathToCheck);
				if (!File.Exists(PathToCheck))
				{
					var LastSeparatorIndex = CurrentPath.LastIndexOf('/');
					if (String.Compare(PathToCheck, UATPathRoot, true) == 0 || LastSeparatorIndex < 0)
					{
						throw new AutomationException("{0} does not appear to exist at a valid location with the branch, so the local root cannot be determined.", UATLocation);
					}
					
					// Step back one directory
					CurrentPath = CurrentPath.Substring(0, LastSeparatorIndex);
				}
				else
				{
					return CurrentPath;
				}
			}
		}

		/// <summary>
		/// Gets the log folder used when running from installed build.
		/// </summary>
		/// <returns></returns>
		private string GetInstalledLogFolder()
		{
			var LocalRootPath = CommandUtils.GetEnvVar(EnvVarNames.LocalRoot);
			var LocalRootEscaped = CommandUtils.ConvertSeparators(PathSeparator.Slash, LocalRootPath.Replace(":", ""));
			if (LocalRootEscaped[LocalRootEscaped.Length - 1] == '/')
			{
				LocalRootEscaped = LocalRootEscaped.Substring(0, LocalRootEscaped.Length - 1);
			}
			LocalRootEscaped = CommandUtils.EscapePath(LocalRootEscaped);
			return CommandUtils.CombinePaths(PathSeparator.Slash, HostPlatform.Current.LocalBuildsLogFolder, LocalRootEscaped);
		}

		/// <summary>
		/// Sets up log output folder.
		/// </summary>
		private void SetAndCreateLogFolder()
		{			
			var LocalLogFolder = CommandUtils.GetEnvVar(EnvVarNames.LogFolder);
			if (String.IsNullOrEmpty(LocalLogFolder))
			{
				string RootSavedPath;
				if (!GlobalCommandLine.Installed)
				{					
					RootSavedPath = CommandUtils.CombinePaths(PathSeparator.Slash, CommandUtils.GetEnvVar(EnvVarNames.EngineSavedFolder), "Logs");
				}
				else
				{
					RootSavedPath = GetInstalledLogFolder(); 
				}
				if (TryCreateLogFolder(RootSavedPath) == false)
				{
					var Result = false;
					if (!GlobalCommandLine.Installed)
					{
						RootSavedPath = GetInstalledLogFolder();
						Result = TryCreateLogFolder(RootSavedPath);
					}
					if (!Result)
					{
						throw new AutomationException("Unable to find a usable log folder.");
					}
				}				
			}
		}

		private bool TryCreateLogFolder(string PossibleLogFolder)
		{
			bool Result = true;
			try
			{
				if (Directory.Exists(PossibleLogFolder))
				{
					CommandUtils.DeleteDirectoryContents(PossibleLogFolder);

					// Since the directory existed and might have been empty, we need to make sure
					// we can actually write to it, so create and delete a temporary file.
					var RandomFilename = Path.GetRandomFileName();
					var RandomFilePath = CommandUtils.CombinePaths(PossibleLogFolder, RandomFilename);
					File.WriteAllText(RandomFilePath, RandomFilename);
					File.Delete(RandomFilePath);
				}
				else
				{
					Directory.CreateDirectory(PossibleLogFolder);
				}
			}
			catch (UnauthorizedAccessException Ex)
			{
				Log.TraceInformation("No permission to use LogFolder={0} ({1})", PossibleLogFolder, Ex.Message);
				Result = false;
			}
			if (Result)
			{
				Environment.SetEnvironmentVariable(EnvVarNames.LogFolder, PossibleLogFolder);
			}
			return Result;
		}
	}
}
