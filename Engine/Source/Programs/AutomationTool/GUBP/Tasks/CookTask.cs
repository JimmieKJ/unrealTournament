// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Definition for a task that runs the cooker
	/// </summary>
	public class CookTaskDefinition : BuildTaskDefinition
	{
		/// <summary>
		/// The target that should to be cooked
		/// </summary>
		[XmlAttribute]
		public string Target;

		/// <summary>
		/// The cook platform to target (eg. WindowsNoEditor)
		/// </summary>
		[XmlAttribute]
		public string CookPlatform;

		/// <summary>
		/// List of maps to be cooked, separated by '+' characters
		/// </summary>
		[XmlAttribute]
		public string Maps;

		/// <summary>
		/// Additional arguments to be passed to the cooker
		/// </summary>
		[XmlAttribute, DefaultValue("-Unversioned")]
		public string Arguments = "-Unversioned";

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			return new CookTask(Target, CookPlatform, Maps.Split('+'), Arguments);
		}
	}

	/// <summary>
	/// Cook a selection of maps for a certain platform
	/// </summary>
	public class CookTask : BuildTask
	{
		/// <summary>
		/// The target that should to be cooked
		/// </summary>
		string Target;

		/// <summary>
		/// The cook platform to target (eg. WindowsNoEditor)
		/// </summary>
		string CookPlatform;

		/// <summary>
		/// List of maps to be cooked, separated by '+' characters
		/// </summary>
		string[] Maps;

		/// <summary>
		/// Additional arguments to be passed to the cooker
		/// </summary>
		string Arguments;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InTarget">The target that should to be cooked</param>
		/// <param name="InCookPlatform">The cook platform to target (eg. WindowsNoEditor)</param>
		/// <param name="InMaps">List of maps to be cooked, separated by '+' characters</param>
		public CookTask(string InTarget, string InCookPlatform, string[] InMaps, string InArguments)
		{
			Target = InTarget;
			CookPlatform = InCookPlatform;
			Maps = InMaps;
			Arguments = InArguments;
		}

		/// <summary>
		/// Execute the task and run the cooker.
		/// </summary>
		/// <param name="BuildProducts">List of build products for the current node. Cooking build products will be appended to this list.</param>
		/// <returns>True if the node succeeded</returns>
		public override bool Execute(List<string> BuildProducts)
		{
			// Figure out the project that this target belongs to
			FileReference ProjectFile;
			if(UProjectInfo.TryGetProjectForTarget(Target, out ProjectFile))
			{
				ProjectFile = null;
			}

			// Execute the cooker
			using(TelemetryStopwatch CookStopwatch = new TelemetryStopwatch("Cook.{0}.{1}", ProjectFile.GetFileNameWithoutExtension(), CookPlatform))
			{
				CommandUtils.CookCommandlet(ProjectFile, "UE4Editor-Cmd.exe", Maps, null, null, null, CookPlatform, Arguments);
			}

			// Find all the cooked files
			DirectoryReference CookedDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Saved", "Cooked", CookPlatform);
			List<FileReference> CookedFiles = CookedDirectory.EnumerateFileReferences().ToList();
			if(CookedFiles.Count == 0)
			{
				throw new AutomationException("Cooking did not produce any files in {0}", CookedDirectory.FullName);
			}
			BuildProducts.AddRange(CookedFiles.Select(x => x.FullName));
			return true;
		}
	}
}
