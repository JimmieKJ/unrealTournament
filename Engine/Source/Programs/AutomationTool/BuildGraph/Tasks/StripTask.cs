using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a task that strips symbols from a set of files
	/// </summary>
	public class StripTaskParameters
	{
		/// <summary>
		/// The platform toolchain to strip binaries
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// The directory to find files in
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.DirectoryName)]
		public string BaseDir;

		/// <summary>
		/// List of file specifications separated by semicolons (eg. Engine/.../*.pdb), or the name of a tag set
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Output directory for the stripped files. Defaults to the input path (overwriting the input files).
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.DirectoryName)]
		public string OutputDir;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Strips debugging information from a set of files.
	/// </summary>
	[TaskElement("Strip", typeof(StripTaskParameters))]
	public class StripTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		StripTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public StripTask(StripTaskParameters InParameters)
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

			// Get the output directory
			DirectoryReference OutputDir = ResolveDirectory(Parameters.OutputDir);

			// Make sure the source and destination directories don't overlap. We can't strip in-place at the moment.
			if(BaseDir == OutputDir)
			{
				CommandUtils.LogError("Output directory for stripped files is the same as source directory ({0})", BaseDir.FullName);
				return false;
			}

			// Find the matching files
			FileReference[] SourceFiles = ResolveFilespec(BaseDir, Parameters.Files, TagNameToFileSet).OrderBy(x => x.FullName).ToArray();

			// Create the matching target files
			FileReference[] TargetFiles = SourceFiles.Select(x => FileReference.Combine(OutputDir, x.MakeRelativeTo(BaseDir))).ToArray();

			// Run the stripping command
			UEBuildPlatform Platform = UEBuildPlatform.GetBuildPlatform(Parameters.Platform);
			UEToolChain ToolChain = Platform.CreateContext(null).CreateToolChainForDefaultCppPlatform();
			for (int Idx = 0; Idx < SourceFiles.Length; Idx++)
			{
				TargetFiles[Idx].Directory.CreateDirectory();
				CommandUtils.Log("Stripping symbols: {0} -> {1}", SourceFiles[Idx].FullName, TargetFiles[Idx].FullName);
				ToolChain.StripSymbols(SourceFiles[Idx].FullName, TargetFiles[Idx].FullName);
			}

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFiles);
			}

			// Add the target files to the set of build products
			BuildProducts.UnionWith(TargetFiles);
			return true;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Files);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.Tag);
		}
	}
}
