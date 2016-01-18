// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;

// @todo UWP: this file is a work in progress and is not yet complete for the F5 scenario for UWP

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	class BaseWindowsDeploy : UEBuildDeploy
	{
		public bool PrepForUATPackageOrDeploy(string InProjectName, string InProjectDirectory, string InExecutablePath, string InEngineDir, bool bForDistribution, string CookFlavor)
		{
			// set the icon on the original exe this will be used in the task bar when the bootstrap exe runs
			if (File.Exists(Path.Combine(InProjectDirectory, "Build/Windows/Application.ico")))
			{
				GroupIconResource GroupIcon = null;
				GroupIcon = GroupIconResource.FromIco(Path.Combine(InProjectDirectory, "Build/Windows/Application.ico"));

				// Update the icon on the original exe because this will be used when the game is running in the task bar
				using (ModuleResourceUpdate Update = new ModuleResourceUpdate(InExecutablePath, false))
				{
					const int IconResourceId = 101;
					if (GroupIcon != null)
					{
						Update.SetIcons(IconResourceId, GroupIcon);
					}
				}
			}
			return true;
		}



		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			if ((InTarget.TargetType != TargetRules.TargetType.Editor && InTarget.TargetType != TargetRules.TargetType.Program) && (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64))
			{
				string InAppName = InTarget.AppName;
				Log.TraceInformation("Prepping {0} for deployment to {1}", InAppName, InTarget.Platform.ToString());
				System.DateTime PrepDeployStartTime = DateTime.UtcNow;

				string TargetFilename = InTarget.RulesAssembly.GetTargetFileName(InAppName).FullName;
				string ProjectSourceFolder = new FileInfo(TargetFilename).DirectoryName + "/";

				PrepForUATPackageOrDeploy(InAppName, InTarget.ProjectDirectory.FullName, InTarget.OutputPath.FullName, BuildConfiguration.RelativeEnginePath, false, "");
			}
			return true;
		}
	}
}
