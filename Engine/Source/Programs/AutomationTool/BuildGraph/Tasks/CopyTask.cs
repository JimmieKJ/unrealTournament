using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a copy task
	/// </summary>
	public class CopyTaskParameters
	{
		/// <summary>
		/// The directory to copy from
		/// </summary>
		[TaskParameter]
		public string FromDir;

		/// <summary>
		/// The directory to copy to
		/// </summary>
		[TaskParameter]
		public string ToDir;

		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Files;
	}

	/// <summary>
	/// Task which copies files from one directory to another
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
			DirectoryReference FromDir = ResolveDirectory(Parameters.FromDir);
			DirectoryReference ToDir = ResolveDirectory(Parameters.ToDir);
			if(FromDir != ToDir)
			{
				// Get the source files to copy
				IEnumerable<FileReference> SourceFiles;
				if(Parameters.Files == null)
				{
					SourceFiles = FromDir.EnumerateFileReferences("*", System.IO.SearchOption.AllDirectories);
				}
				else
				{
					SourceFiles = ResolveFilespec(FromDir, Parameters.Files, TagNameToFileSet);
				}

				// Figure out matching target files
				IEnumerable<FileReference> TargetFiles = SourceFiles.Select(x => FileReference.Combine(ToDir, x.MakeRelativeTo(FromDir)));
				CommandUtils.ThreadedCopyFiles(SourceFiles.Select(x => x.FullName).ToList(), TargetFiles.Select(x => x.FullName).ToList());
			}
			return true;
		}
	}
}
