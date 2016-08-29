// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Linq;

[Help("Lists TPS files associated with any source used to build a specified target(s). Grabs TPS files associated with source modules, content, and engine shaders.")]
[Help("Target", "one or more Project|Config|Platform values describing all the targets to get associated TPS files for.")]
[Help("Debug", "Log more detailed info for debugging")]
class ListThirdPartySoftware : BuildCommand
{
	public override void ExecuteBuild()
	{
		CommandUtils.Log("************************* List Third Party Software");
		bool DebugRun = ParseParam("Debug");

		// first get a list of all TPS files available (as a lookup of Dir -> List of TPS files in Dir).
		var AllTPSFiles = 
			Directory.EnumerateFiles(CombinePaths(CmdEnv.LocalRoot), "*.tps", SearchOption.AllDirectories)
			.ToLookup(TPSFile => Path.GetDirectoryName(TPSFile), StringComparer.InvariantCultureIgnoreCase);

		ParseParamValues(Params, "Target") // Grab all the Target params.
		.Select(TargetArg => TargetArg.Split(new[] { '|' }, 3)) // split the arg up by |
		.Where(Target => Target.Length == 3) // ensure it has three parts
		.Select(Target => new // strongly name the parts of the target and 
		{
			Name          = Target[0],
			Configuration = Target[1],
			Platform      = Target[2],
			// create a list of unsupported platform strings for that target, which we'll use later to cull out irrelvant TPS files
			UnsupportedSubstrings =
				Utils.MakeListOfUnsupportedPlatforms(new List<UnrealTargetPlatform> { (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), Target[2]) })
				.Select(PlatformName => Path.DirectorySeparatorChar + PlatformName + Path.DirectorySeparatorChar) // convert to /Platform/
				.ToList(), // project to list
		})
		.SelectMany(Target =>
			// run UBT on each target to get the build folders, add a few hard-coded paths, then search for TPS files in relevant folders to them
			RunAndLog(UE4Build.GetUBTExecutable(), string.Format("{0} {1} {2} -listbuildfolders", Target.Name, Target.Configuration, Target.Platform), Options: DebugRun ? ERunOptions.Default : ERunOptions.NoLoggingOfRunCommand) // run UBT to get the raw output
			.EnumerateLines() // split it into separate lines
			.Where(Line => Line.StartsWith("BuildFolder:", StringComparison.InvariantCultureIgnoreCase)) // find the output lines that we need
			.Select(Line => Line.Substring("BuildFolder:".Length)) // chop it down to the path name
			// tack on the target content directory, which we could infer from one of a few places
			.Concat(new[] 
				{
					Path.Combine(Path.GetDirectoryName(Target.Name), "Content"), // The target could be a .uproject, so look for a content folder there
					Path.Combine(CmdEnv.LocalRoot, Target.Name, "Content"), // the target could be a root project, so look for a content folder there
				}.Where(Folder => Directory.Exists(Folder)) // only accept Content folders that actually exist.
			)
			// tack on hard-coded engine shaders and content folder
			.Concat(new List<string> { "Shaders", "Content", }.Select(Folder => Path.Combine(CmdEnv.LocalRoot, "Engine", Folder)))
			// canonicalize the path and convert separators. We do string compares below, so we need separators to be in consistent formats.
			.Select(FolderToScan => CombinePaths(FolderToScan))
			// scan each folder for TPS files that appear relevant to that folder
			.SelectMany(FolderToScan =>
				AllTPSFiles
				// A relevant folder is one that is in a subfolders or parent folder to that folder
				.Where(TPSFileDir => FolderToScan.StartsWith(TPSFileDir.Key, StringComparison.InvariantCultureIgnoreCase) || TPSFileDir.Key.StartsWith(FolderToScan, StringComparison.InvariantCultureIgnoreCase))
				// flatten the list of all the TPS files in any folders we found (remember we were just looking at folders above, which could have a list of TPS files)
				.SelectMany(TPSFileDir => TPSFileDir)
				// filter out TPS from unwanted platforms for the current target.
				// This is a bit hard to follow, but we are only looking for unsupported platform names INSIDE the folder to search.
				// for instance, if the folder is Foo/PS4/Plugins/PadSupport and we are building for windows, we don't want to filter it out even though it has PS4 in the name.
				// but if the TPS file is Foo/PS4/Plugins/PadSupport/Android/TPS.TPS, then we do want to filter out because of the Android folder.
				// this basically checks if an unsupported platform string is found AFTER the original folder we are looking in.
				.Where(TpsFile => Target.UnsupportedSubstrings.Max(PlatformDir => TpsFile.IndexOf(PlatformDir, StringComparison.InvariantCultureIgnoreCase)) < FolderToScan.Length)
			)
		)
		.Distinct() // remove duplicate TPS files.
		// pull out the redirect if present (but leave a breadcrumb to where the redirect came from)
		.Select(TPSFile => 
			// read all the lines in the file
			File.ReadAllLines(TPSFile)
			// look for a line with a redirect
			.Where(Line => Line.IndexOf("Redirect:", StringComparison.InvariantCultureIgnoreCase) >= 0)
			// grab the redirect file and canonicalize it, tacking on some info on where the redirect was from.
			.Select(Line => CombinePaths(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(TPSFile), Line.Split(new[] { ':' },2)[1].Trim()))) + " (redirect from " + TPSFile + ")")
			// take the first one found, or just use the TPS file directly if no redirect was present.
			.SingleOrDefault() ?? TPSFile)
		.OrderBy(TPSFile => TPSFile) // sort them.
		.ToList().ForEach(TPSFile => UnrealBuildTool.Log.WriteLine(0, string.Empty, LogEventType.Console, TPSFile)); // log them.
	}
}
