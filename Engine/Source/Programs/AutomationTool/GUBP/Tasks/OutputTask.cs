// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Definition for a task which modifies the list of build products.
	/// </summary>
	public class OutputTaskDefinition : BuildTaskDefinition
	{
		/// <summary>
		/// P4-style list of rules specifying files to include or exclude, separated by | characters. The rules are applied to the branch root to select which files to add as build products.
		/// Example: "/Engine/Build/...|-*.bat|-/Engine/Build/SomeFile.txt"
		/// </summary>
		[XmlAttribute]
		public string Add;

		/// <summary>
		/// P4-style list of rules to be applied to the existing list of build products, separated by | characters. Example: "-*.pdb|UnrealHeaderTool.pdb"
		/// </summary>
		[XmlAttribute]
		public string Filter;

		/// <summary>
		/// Base directory for the add and filter rules to be applied to, underneath the branch root.
		/// </summary>
		[XmlAttribute]
		public string BaseDirectory;

		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public override BuildTask CreateTask()
		{
			FileFilter AddFiles = null;
			if(Add != null)
			{
				AddFiles = new FileFilter(FileFilterType.Exclude);
				AddFiles.AddRules(Add.Split('|'));
			}

			FileFilter FilterFiles = null;
			if(Filter != null)
			{
				FilterFiles = new FileFilter(FileFilterType.Include);
				FilterFiles.AddRules(Filter.Split('|'));
			}

			return new OutputTask(AddFiles, FilterFiles, BaseDirectory);
		}
	}

	/// <summary>
	/// Task which modifies the list of build products, adding files from the local hard drive or filtering the existing list.
	/// </summary>
	public class OutputTask : BuildTask
	{
		/// <summary>
		/// Filter for the list of files to add.
		/// </summary>
		FileFilter AddFiles;

		/// <summary>
		/// Filter to be applied to the existing list of build products.
		/// </summary>
		FileFilter FilterFiles;
		
		/// <summary>
		/// Base directory, relative to the branch root.
		/// </summary>
		string BaseDirectory;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InAdd"></param>
		/// <param name="InFilter"></param>
		/// <param name="InBaseDirectory"></param>
		public OutputTask(FileFilter InAddFiles, FileFilter InFilterFiles, string InBaseDirectory)
		{
			AddFiles = InAddFiles;
			FilterFiles = InFilterFiles;
			BaseDirectory = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, InBaseDirectory);
		}

		/// <summary>
		/// Runs the task, filtering the list of build products
		/// </summary>
		/// <param name="BuildProducts"></param>
		/// <returns>True if the task succeeds</returns>
		public override bool Execute(List<string> BuildProducts)
		{
			if(AddFiles != null)
			{
				BuildProducts.AddRange(AddFiles.ApplyToDirectory(BaseDirectory, true).Select(x => CommandUtils.CombinePaths(BaseDirectory, x)));
			}
			if(FilterFiles != null)
			{
				BuildProducts.RemoveAll(x => UnrealBuildTool.Utils.IsFileUnderDirectory(x, BaseDirectory) && !FilterFiles.Matches(UnrealBuildTool.Utils.StripBaseDirectory(x, BaseDirectory)));
			}
			return true;
		}
	}
}
