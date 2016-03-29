using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Specifies validation that should be performed on a task parameter.
	/// </summary>
	public enum TaskParameterValidationType
	{
		/// <summary>
		/// Allow any valid values for the field type.
		/// </summary>
		Default,

		/// <summary>
		/// A standard name; alphanumeric characters, plus underscore and space. Spaces at the start or end, or more than one in a row are prohibited.
		/// </summary>
		Name,

		/// <summary>
		/// A list of names separated by semicolons
		/// </summary>
		NameList,

		/// <summary>
		/// A tag name (a regular name with an optional '#' prefix)
		/// </summary>
		Tag,

		/// <summary>
		/// A list of tag names separated by semicolons
		/// </summary>
		TagList,
	}

	/// <summary>
	/// Attribute to mark parameters to a task, which should be read as XML attributes from the script file.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class TaskParameterAttribute : Attribute
	{
		/// <summary>
		/// Whether the parameter can be omitted
		/// </summary>
		public bool Optional
		{
			get;
			set;
		}

		/// <summary>
		/// Sets additional restrictions on how this field is validated in the schema. Default is to allow any valid field type.
		/// </summary>
		public TaskParameterValidationType ValidationType
		{
			get;
			set;
		}
	}

	/// <summary>
	/// Attribute used to associate an XML element name with a parameter block that can be used to construct tasks
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public class TaskElementAttribute : Attribute
	{
		/// <summary>
		/// Name of the XML element that can be used to denote this class
		/// </summary>
		public string Name;

		/// <summary>
		/// Type to be constructed from the deserialized element
		/// </summary>
		public Type ParametersType;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the XML element used to denote this object</param>
		/// <param name="InParametersType">Type to be constructed from this object</param>
		public TaskElementAttribute(string InName, Type InParametersType)
		{
			Name = InName;
			ParametersType =  InParametersType;
		}
	}

	/// <summary>
	/// Base class for all custom build tasks
	/// </summary>
	public abstract class CustomTask
	{
		/// <summary>
		/// Allow this task to merge with other tasks within the same node if it can. This can be useful to allow tasks to execute in parallel.
		/// </summary>
		/// <param name="OtherTasks">Other tasks that this task can merge with. If a merge takes place, the other tasks should be removed from the list.</param>
		public virtual void Merge(List<CustomTask> OtherTasks)
		{
		}

		/// <summary>
		/// Execute this node.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public abstract bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet);

		/// <summary>
		/// Resolves a single name to a file reference, resolving relative paths to the root of the current path.
		/// </summary>
		/// <param name="Name">Name of the file</param>
		/// <returns>Fully qualified file reference</returns>
		public static FileReference ResolveFile(string Name)
		{
			if(Path.IsPathRooted(Name))
			{
				return new FileReference(Name);
			}
			else
			{
				return new FileReference(Path.Combine(CommandUtils.CmdEnv.LocalRoot, Name));
			}
		}

		/// <summary>
		/// Resolves a directory reference from the given string. Assumes the root directory is the root of the current branch.
		/// </summary>
		/// <param name="Name">Name of the directory. May be null or empty.</param>
		/// <returns>The resolved directory</returns>
		public static DirectoryReference ResolveDirectory(string Name)
		{
			if(String.IsNullOrEmpty(Name))
			{
				return CommandUtils.RootDirectory;
			}
			else if(Path.IsPathRooted(Name))
			{
				return new DirectoryReference(Name);
			}
			else
			{
				return DirectoryReference.Combine(CommandUtils.RootDirectory, Name);
			}
		}

		/// <summary>
		/// Finds or adds a set containing files with the given tag
		/// </summary>
		/// <param name="Name">The tag name to return a set for. An leading '#' character is optional.</param>
		/// <returns>Set of files</returns>
		public static HashSet<FileReference> FindOrAddTagSet(Dictionary<string, HashSet<FileReference>> TagNameToFileSet, string Name)
		{
			// Get the clean tag name, without the leading '#' character
			string TagName = Name.StartsWith("#")? Name.Substring(1) : Name;

			// Find the files which match this tag
			HashSet<FileReference> Files;
			if(!TagNameToFileSet.TryGetValue(TagName, out Files))
			{
				Files = new HashSet<FileReference>();
				TagNameToFileSet.Add(TagName, Files);
			}

			// If we got a null reference, it's because the tag is not listed as an input for this node (see RunGraph.BuildSingleNode). Fill it in, but only with an error.
			if(Files == null)
			{
				CommandUtils.LogError("Attempt to reference tag '{0}', which is not listed as a dependency of this node.", Name);
				Files = new HashSet<FileReference>();
				TagNameToFileSet.Add(TagName, Files);
			}
			return Files;
		}

		/// <summary>
		/// Resolve a list of files, tag names or file specifications separated by semicolons. Supported entries may be:
		///   a) The name of a tag set (eg. #CompiledBinaries)
		///   b) Relative or absolute filenames
		///   c) A simple file pattern (eg. Foo/*.cpp)
		///   d) A full directory wildcard (eg. Engine/...)
		/// Note that wildcards may only match the last fragment in a pattern, so matches like "/*/Foo.txt" and "/.../Bar.txt" are illegal.
		/// </summary>
		/// <param name="DefaultDirectory">The default directory to resolve relative paths to</param>
		/// <param name="DelimitedPatterns">List of files, tag names, or file specifications to include separated by semicolons.</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to fileset, as passed to the Execute() method</param>
		/// <returns>Set of matching files.</returns>
		public static HashSet<FileReference> ResolveFilespec(DirectoryReference DefaultDirectory, string DelimitedPatterns, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Find all the files and directories that are referenced
			HashSet<DirectoryReference> Directories = new HashSet<DirectoryReference>();
			HashSet<FileReference> Files = ResolveFilespec(DefaultDirectory, DelimitedPatterns, Directories, TagNameToFileSet);

			// Include all the files underneath the directories
			foreach(DirectoryReference Directory in Directories)
			{
				Files.UnionWith(Directory.EnumerateFileReferences("*", SearchOption.AllDirectories));
			}
			return Files;
		}

		/// <summary>
		/// Resolve a list of files, tag names or file specifications separated by semicolons as above, but preserves any directory references for further processing.
		/// </summary>
		/// <param name="DefaultDirectory">The default directory to resolve relative paths to</param>
		/// <param name="DelimitedPatterns">List of files, tag names, or file specifications to include separated by semicolons.</param>
		/// <param name="Directories">Set of directories which are referenced using directory wildcards. Files under these directories are not added to the output set.</param>
		/// <param name="TagNameToFileSet">Mapping of tag name to fileset, as passed to the Execute() method</param>
		/// <returns>Set of matching files.</returns>
		public static HashSet<FileReference> ResolveFilespec(DirectoryReference DefaultDirectory, string DelimitedPatterns, HashSet<DirectoryReference> Directories, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Split the argument into a list of patterns
			string[] Patterns = SplitDelimitedList(DelimitedPatterns);

			// Parse each of the patterns, and add the results into the given sets
			HashSet<FileReference> Files = new HashSet<FileReference>();
			foreach(string Pattern in Patterns)
			{
				// Check if it's a tag name
				if(Pattern.StartsWith("#"))
				{
					Files.UnionWith(FindOrAddTagSet(TagNameToFileSet, Pattern.Substring(1)));
					continue;
				}

				// List of all wildcards
				string[] Wildcards = { "?", "*", "..." };

				// Find the first index of a wildcard in the given pattern
				int WildcardIdx = -1;
				foreach(string Wildcard in Wildcards)
				{
					int Idx = Pattern.IndexOf(Wildcard);
					if(Idx != -1 && (WildcardIdx == -1 || Idx < WildcardIdx))
					{
						WildcardIdx = Idx;
					}
				}

				// If we didn't find any wildcards, we can just add the pattern directly.
				if(WildcardIdx == -1)
				{
					Files.Add(FileReference.Combine(DefaultDirectory, Pattern));
					continue;
				}

				// Check the wildcard is in the last fragment of the path.
				int LastDirectoryIdx = Pattern.LastIndexOfAny(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar });
				if(LastDirectoryIdx != -1 && LastDirectoryIdx > WildcardIdx)
				{
					CommandUtils.LogWarning("Invalid pattern '{0}': Path wildcard can only match files");
					continue;
				}

				// Check if it's a directory reference (ends with ...) or file reference.
				int PathWildcardIdx = Pattern.IndexOf("...");
				if(PathWildcardIdx != -1)
				{
					// Add the base directory to the list of results
					if(PathWildcardIdx + 3 != Pattern.Length)
					{
						CommandUtils.LogWarning("Invalid pattern '{0}': Path wildcard must appear at end of pattern.");
					}
					else if(PathWildcardIdx != LastDirectoryIdx + 1)
					{
						CommandUtils.LogWarning("Invalid pattern '{0}': Path wildcard cannot partially match a name.");
					}
					else
					{
						Directories.Add(DirectoryReference.Combine(DefaultDirectory, Pattern.Substring(0, PathWildcardIdx)));
					}
				}
				else
				{
					// Construct a file filter and apply it to files in the directory. Don't use the default file enumeration logic for consistency; search patterns
					// passed to Directory.EnumerateFiles et al have special cases for backwards compatibility that we don't want.
					FileFilter Filter = new FileFilter();
					Filter.AddRule("/" + Pattern.Substring(LastDirectoryIdx + 1), FileFilterType.Include);
					DirectoryReference BaseDir = DirectoryReference.Combine(DefaultDirectory, Pattern.Substring(0, LastDirectoryIdx + 1));
					Files.UnionWith(Filter.ApplyToDirectory(BaseDir, true));
				}
			}
			return Files;
		}

		/// <summary>
		/// Splits a string separated by semicolons into a list, removing empty entries
		/// </summary>
		/// <param name="Text">The input string</param>
		/// <returns>Array of the parsed items</returns>
		public static string[] SplitDelimitedList(string Text)
		{
			return Text.Split(';').Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();
		}
	}
}
