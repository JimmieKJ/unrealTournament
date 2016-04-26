using EpicGames.MCP.Automation;
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
	/// Parameters for a task which calls another UAT command
	/// </summary>
	public class ChunkTaskParameters
	{
		/// <summary>
		/// The application name
		/// </summary>
		[TaskParameter]
		public string AppName;

        /// <summary>
        /// Platform we are staging for.
        /// </summary>
		[TaskParameter]
        public MCPPlatform Platform;

        /// <summary>
        /// BuildVersion of the App we are staging.
        /// </summary>
		[TaskParameter]
        public string BuildVersion;

        /// <summary>
        /// Directory that build data will be copied from.
        /// </summary>
		[TaskParameter]
        public string InputDir;

		/// <summary>
		/// Optional list of files that should be considered
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Files;

		/// <summary>
		/// The executable to run to launch this application.
		/// </summary>
		[TaskParameter]
		public string Launch;

		/// <summary>
		/// Parameters that the application should be launched with.
		/// </summary>
		[TaskParameter]
		public string LaunchArgs;

        /// <summary>
        /// Full path to the CloudDir where chunks and manifests should be staged.
        /// </summary>
		[TaskParameter]
        public string CloudDir;
	}

	/// <summary>
	/// Implements a task which calls another UAT command
	/// </summary>
	[TaskElement("Chunk", typeof(ChunkTaskParameters))]
	public class ChunkTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ChunkTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ChunkTask(ChunkTaskParameters InParameters)
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
			// Get the build directory
			DirectoryReference InputDir = ResolveDirectory(Parameters.InputDir);

			// If there's a set of files specified, generate a temporary ignore list.
			FileReference IgnoreList = null;
			if(Parameters.Files != null)
			{
				// Find the files which are to be included
				HashSet<FileReference> IncludeFiles = ResolveFilespec(InputDir, Parameters.Files, TagNameToFileSet);

				// Create a file to store the ignored file list
				IgnoreList = new FileReference(LogUtils.GetUniqueLogName(Path.Combine(CommandUtils.CmdEnv.LogFolder, Parameters.AppName + "-Ignore")));
				using(StreamWriter Writer = new StreamWriter(IgnoreList.FullName))
				{
					DirectoryInfo InputDirInfo = new DirectoryInfo(InputDir.FullName);
					foreach(FileInfo File in InputDirInfo.EnumerateFiles("*", SearchOption.AllDirectories))
					{
						string RelativePath = new FileReference(File).MakeRelativeTo(InputDir);
						const string Iso8601DateTimeFormat = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffZ";
						Writer.WriteLine("\"{0}\"\t{1}", RelativePath, File.LastWriteTimeUtc.ToString(Iso8601DateTimeFormat));
					}
				}
			}

			// Create the staging info
			BuildPatchToolStagingInfo StagingInfo = new BuildPatchToolStagingInfo(Job.OwnerCommand, Parameters.AppName, 1, Parameters.BuildVersion, Parameters.Platform, Parameters.CloudDir);

			// Set the patch generation options
			BuildPatchToolBase.PatchGenerationOptions Options = new BuildPatchToolBase.PatchGenerationOptions();
			Options.StagingInfo = StagingInfo;
			Options.BuildRoot = ResolveDirectory(Parameters.InputDir).FullName;
			Options.FileIgnoreList = (IgnoreList != null)? IgnoreList.FullName : null;
			Options.AppLaunchCmd = Parameters.Launch;
			Options.AppLaunchCmdArgs = Parameters.LaunchArgs;
			Options.AppChunkType = BuildPatchToolBase.ChunkType.Chunk;

			// Run the chunking
			BuildPatchToolBase.Get().Execute(Options);
			return true;
		}
	}
}
