using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the Tag task.
	/// </summary>
	public class TagTaskParameters
	{
		/// <summary>
		/// Set the base directory to match relative patterns against
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BaseDir;

		/// <summary>
		/// Patterns to filter the list of files by. May include tag names or patterns that apply to the base directory. Defaults to all files if not specified.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Filter;

		/// <summary>
		/// Set of patterns to exclude from the matched list. May include tag names of patterns that apply to the base directory.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Except;

		/// <summary>
		/// Set of source files. Typically a tag name, but paths and wildcards may also be used. This list is expanded prior to applying the 'Files' and 'Except' filters, 
		/// which is slower than just searching against a base directory.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string From;

		/// <summary>
		/// Name of the tag to apply
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.Tag)]
		public string With;
	}

	/// <summary>
	/// Task which applies a tag to a given set of files. Filtering is performed using the following method:
	/// * A set of files is enumerated from the tags and file specifications given by the "Files" parameter
	/// * Any files not matched by the "Filter" parameter are removed.
	/// * Any files matched by the "Except" parameter are removed.
	/// </summary>
	[TaskElement("Tag", typeof(TagTaskParameters))]
	class TagTask : CustomTask
	{
		/// <summary>
		/// Parameters to this task
		/// </summary>
		TagTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters to select which files to match</param>
		/// <param name="Type"></param>
		public TagTask(TagTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Get the base directory
			DirectoryReference BaseDir = ResolveDirectory(Parameters.BaseDir);

			// Resolve the input list
			HashSet<FileReference> Files;
			HashSet<DirectoryReference> Directories;
			if(Parameters.From == null)
			{
				Directories = new HashSet<DirectoryReference>{ CommandUtils.RootDirectory };
				Files = new HashSet<FileReference>();
			}
			else
			{
				Directories = new HashSet<DirectoryReference>();
				Files = ResolveFilespec(CommandUtils.RootDirectory, Parameters.From, Directories, TagNameToFileSet);
			}

			// Create the filter
			FileFilter Filter = new FileFilter();
			AddRules(Filter, FileFilterType.Include, BaseDir, Parameters.Filter ?? "...", TagNameToFileSet);
			AddRules(Filter, FileFilterType.Exclude, BaseDir, Parameters.Except ?? "", TagNameToFileSet);

			// Apply the filter to the list of files, then to the directories
			HashSet<FileReference> MatchingFiles = FindOrAddTagSet(TagNameToFileSet, Parameters.With);
			foreach(DirectoryReference Directory in Directories)
			{
				List<FileReference> FoundFiles = Filter.ApplyToDirectory(Directory, Directory.FullName, true);
				MatchingFiles.UnionWith(FoundFiles);
			}
			foreach(FileReference File in Files)
			{
				if(Filter.Matches(File.FullName))
				{
					MatchingFiles.Add(File);
				}
			}
			return true;
		}

		/// <summary>
		/// Add rules matching a given set of patterns to a file filter. Patterns are added as absolute paths from the root.
		/// </summary>
		/// <param name="Filter">The filter to add to</param>
		/// <param name="RuleType">The type of rule to add; whether to include or exclude files matching these patterns.</param>
		/// <param name="BaseDir">The base directory for relative paths.</param>
		/// <param name="DelimitedPatterns">List of patterns to add, separated by semicolons.</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to a set of files.</param>
		void AddRules(FileFilter Filter, FileFilterType RuleType, DirectoryReference BaseDir, string DelimitedPatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			string[] Patterns = SplitDelimitedList(DelimitedPatterns);
			foreach(string Pattern in Patterns)
			{
				if(Pattern.StartsWith("#"))
				{
					// Add the files in a specific set to the filter
					HashSet<FileReference> Files = FindOrAddTagSet(TagNameToFileSet, Pattern.Substring(1));
					foreach(FileReference File in Files)
					{
						Filter.AddRule(File.FullName, RuleType);
					}
				}
				else
				{
					// Parse a wildcard filter
					if(Pattern.StartsWith("..."))
					{
						Filter.AddRule(Pattern, RuleType);
					}
					else if(!Pattern.Contains("/") && RuleType == FileFilterType.Exclude)
					{
						Filter.AddRule(".../" + Pattern, RuleType);
					}
					else if(!Pattern.StartsWith("/"))
					{
						Filter.AddRule(BaseDir.FullName + "/" + Pattern, RuleType);
					}
					else
					{
						Filter.AddRule(BaseDir.FullName + Pattern, RuleType);
					}
				}
			}
		}
	}
}
