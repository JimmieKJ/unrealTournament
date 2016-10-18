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
	/// Parameters for a copy task
	/// </summary>
	public class CopyTaskParameters
	{
		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set. Relative paths are based at FromDir.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The base directory to copy from. 
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.DirectoryName)]
		public string FromDir;

		/// <summary>
		/// The directory to copy to
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.DirectoryName)]
		public string ToDir;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Copies files from one directory to another.
	/// </summary>
	[TaskElement("Copy", typeof(CopyTaskParameters))]
	public class CopyTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CopyTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CopyTask(CopyTaskParameters InParameters)
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
			// Get the source and target directories
			DirectoryReference FromDir = ResolveDirectory(Parameters.FromDir);
			DirectoryReference ToDir = ResolveDirectory(Parameters.ToDir);

			// Copy all the files
			IEnumerable<FileReference> SourceFiles = ResolveFilespec(FromDir, Parameters.Files, TagNameToFileSet);
			IEnumerable<FileReference> TargetFiles = SourceFiles.Select(x => FileReference.Combine(ToDir, x.MakeRelativeTo(FromDir)));
			if(!FromDir.Exists() && !SourceFiles.Any())
			{
				CommandUtils.Log("Skipping copy of files from '{0}' - directory does not exist.", FromDir.FullName);
			}
			else if(FromDir == ToDir)
			{
				CommandUtils.Log("Skipping copy of files in '{0}' - source directory is same as target directory", FromDir.FullName);
			}
			else 
			{
				CommandUtils.ThreadedCopyFiles(SourceFiles.Select(x => x.FullName).ToList(), TargetFiles.Select(x => x.FullName).ToList());
			}
			BuildProducts.UnionWith(TargetFiles);

			// Apply the optional output tag to them
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFiles);
			}
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
