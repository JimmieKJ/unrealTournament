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
	/// Parameters for the staging task
	/// </summary>
	public class StageTaskParameters
	{
		/// <summary>
		/// The project that this target belongs to
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Project;

		/// <summary>
		/// Name of the target to stage
		/// </summary>
		[TaskParameter]
		public string Target;

		/// <summary>
		/// Platform to stage
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Configuration to be staged
		/// </summary>
		[TaskParameter]
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Architecture to be staged
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Architecture;

		/// <summary>
		/// Subset of files which should be staged. This filter is run against the source directory.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Files;

		/// <summary>
		/// Files which should not be staged. This filter is run against the source directory.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Exclude;

		/// <summary>
		/// Directory the receipt files should be staged to
		/// </summary>
		[TaskParameter]
		public string To;
	}

	/// <summary>
	/// Stages files from a receipt to an output directory
	/// </summary>
	[TaskElement("Stage", typeof(StageTaskParameters))]
	public class StageTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		StageTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public StageTask(StageTaskParameters InParameters)
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
			// Get the project path, and check it exists
			FileReference ProjectFile = null;
			if(Parameters.Project != null)
			{
				ProjectFile = ResolveFile(Parameters.Project);
				if(!ProjectFile.Exists())
				{
					CommandUtils.LogError("Couldn't find project '{0}'", ProjectFile.FullName);
					return false;
				}
			}

			// Get the directories used for staging this project
			DirectoryReference SourceEngineDir = UnrealBuildTool.UnrealBuildTool.EngineDirectory;
			DirectoryReference SourceProjectDir = (ProjectFile == null)? SourceEngineDir : ProjectFile.Directory;

			// Get the output directories. We flatten the directory structure on output.
			DirectoryReference TargetDir = ResolveDirectory(Parameters.To);
			DirectoryReference TargetEngineDir = DirectoryReference.Combine(TargetDir, "Engine");
			DirectoryReference TargetProjectDir = DirectoryReference.Combine(TargetDir, ProjectFile.GetFileNameWithoutExtension());

			// Get the path to the receipt
			string ReceiptFileName = TargetReceipt.GetDefaultPath(SourceProjectDir.FullName, Parameters.Target, Parameters.Platform, Parameters.Configuration, Parameters.Architecture);

			// Try to load it
			TargetReceipt Receipt;
			if(!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
			{
				CommandUtils.LogError("Couldn't read receipt '{0}'", ReceiptFileName);
				return false;
			}

			// Expand all the paths from the receipt
			Receipt.ExpandPathVariables(SourceEngineDir, SourceProjectDir);
			
			// Stage all the build products needed at runtime
			HashSet<FileReference> SourceFiles = new HashSet<FileReference>();
			foreach(BuildProduct BuildProduct in Receipt.BuildProducts)
			{
				SourceFiles.Add(new FileReference(BuildProduct.Path));
			}
			foreach(RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
			{
				SourceFiles.Add(new FileReference(RuntimeDependency.Path));
			}

			// Copy them all to the output directory
			foreach(FileReference SourceFile in SourceFiles)
			{
				// Get the destination file to copy to, mapping to the new engine and project directories as appropriate
				FileReference TargetFile;
				if(SourceFile.IsUnderDirectory(SourceEngineDir))
				{
					TargetFile = FileReference.Combine(TargetEngineDir, SourceFile.MakeRelativeTo(SourceEngineDir));
				}
				else
				{
					TargetFile = FileReference.Combine(TargetProjectDir, SourceFile.MakeRelativeTo(SourceProjectDir));
				}

				// Only copy the output file if it doesn't already exist. We can stage multiple targets to the same output directory.
				if(!TargetFile.Exists())
				{
					TargetFile.Directory.CreateDirectory();
					CommandUtils.CopyFile(SourceFile.FullName, TargetFile.FullName);
				}
			}
			return true;
		}
	}
}
