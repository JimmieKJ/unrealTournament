// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;

namespace UnrealBuildTool
{
	/**
	 * Caches include dependency information to speed up preprocessing on subsequent runs.
	 */
	public class ActionHistory
	{
		/** Path to store the cache data to. */
		private string FilePath;

		/** The command lines used to produce files, keyed by the absolute file paths. */
		private Dictionary<string, string> ProducedItemToPreviousActionCommandLine;

		/** Whether the dependency cache is dirty and needs to be saved. */
		private bool bIsDirty;

		public ActionHistory(string InFilePath)
		{
			FilePath = Path.GetFullPath(InFilePath);

			var bFoundCache = false;
			if (File.Exists(FilePath) == true)
			{
				try
				{
					// Deserialize the history from disk if the file exists.
					using (FileStream Stream = new FileStream(FilePath, FileMode.Open, FileAccess.Read))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						ProducedItemToPreviousActionCommandLine = Formatter.Deserialize(Stream) as Dictionary<string, string>;
					}

					bFoundCache = true;
				}
				catch (Exception)
				{
					// If this fails for any reason just reset. History will be created later.
					ProducedItemToPreviousActionCommandLine = null;
				}
			}

			if (!bFoundCache)
			{
				// Otherwise create a fresh history.
				ProducedItemToPreviousActionCommandLine = new Dictionary<string, string>();
			}

			bIsDirty = false;
		}

		public void Save()
		{
			// Only save if we've made changes to it since load.
			if( bIsDirty )
			{
				// Serialize the cache to disk.
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
					using (FileStream Stream = new FileStream(FilePath, FileMode.Create, FileAccess.Write))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						Formatter.Serialize(Stream, ProducedItemToPreviousActionCommandLine);
					}
				}
				catch (Exception Ex)
				{
					Console.Error.WriteLine("Failed to write dependency cache: {0}", Ex.Message);
				}
			}
		}

		public bool GetProducingCommandLine(FileItem File, out string Result)
		{
			return ProducedItemToPreviousActionCommandLine.TryGetValue(File.AbsolutePath.ToUpperInvariant(), out Result);
		}

		public void SetProducingCommandLine(FileItem File, string CommandLine)
		{
			ProducedItemToPreviousActionCommandLine[File.AbsolutePath.ToUpperInvariant()] = CommandLine;
			bIsDirty = true;
		}

		/// <summary>
		/// Generates a full path to action history file for the specified target.
		/// </summary>
		public static string GeneratePathForTarget(UEBuildTarget Target)
		{
			string Folder = null;
			if (Target.ShouldCompileMonolithic() || Target.TargetType == TargetRules.TargetType.Program)
			{
				// Monolithic configs and programs have their Action History stored in their respective project folders
				// or under engine intermediate folder + program name folder
				string RootDirectory = UnrealBuildTool.GetUProjectPath();
				if (String.IsNullOrEmpty(RootDirectory))
				{
					RootDirectory = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);
				}
				Folder = Path.Combine(RootDirectory, BuildConfiguration.PlatformIntermediateFolder, Target.GetTargetName());
			}
			else
			{
				// Shared action history (unless this is a rocket target)
				Folder = (UnrealBuildTool.RunningRocket() && UnrealBuildTool.HasUProjectFile()) ?
					Path.Combine(UnrealBuildTool.GetUProjectPath(), BuildConfiguration.BaseIntermediateFolder) :
					BuildConfiguration.BaseIntermediatePath;
			}
			return Path.Combine(Folder, "ActionHistory.bin").Replace("\\", "/");
		}
	}
}
