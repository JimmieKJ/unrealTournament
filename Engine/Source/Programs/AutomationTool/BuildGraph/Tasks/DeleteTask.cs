using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a copy task
	/// </summary>
	public class DeleteTaskParameters
	{
		/// <summary>
		/// The directory to delete from
		/// </summary>
		[TaskParameter]
		public string Dir;

		/// <summary>
		/// List of file specifications separated by semicolons (eg. *.cpp;Engine/.../*.bat), or the name of a tag set
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Files;

		/// <summary>
		/// Whether to delete empty directories after deleting the files
		/// </summary>
		public bool DeleteEmptyDirectories = true;
	}

	/// <summary>
	/// Task which copies files from one directory to another
	/// </summary>
	[TaskElement("Delete", typeof(DeleteTaskParameters))]
	public class DeleteTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		DeleteTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public DeleteTask(DeleteTaskParameters InParameters)
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
			DirectoryReference BaseDirectory = ResolveDirectory(Parameters.Dir);

			// Find all the referenced files
			IEnumerable<FileReference> Files;
			if(Parameters.Files == null)
			{
				Files = BaseDirectory.EnumerateFileReferences("*", System.IO.SearchOption.AllDirectories);
			}
			else
			{
				Files = ResolveFilespec(BaseDirectory, Parameters.Files, TagNameToFileSet);
			}

			// Delete them all
			foreach(FileReference File in Files)
			{
				InternalUtils.SafeDeleteFile(File.FullName, true);
			}

			// Try to delete all the parent directories. Keep track of the directories we've already deleted to avoid hitting the disk.
			if(Parameters.DeleteEmptyDirectories)
			{
				// Find all the directories that we're touching
				HashSet<DirectoryReference> ParentDirectories = new HashSet<DirectoryReference>();
				foreach(FileReference File in Files)
				{
					ParentDirectories.Add(File.Directory);
				}

				// Recurse back up from each of those directories to the root folder
				foreach(DirectoryReference ParentDirectory in ParentDirectories)
				{
					for(DirectoryReference CurrentDirectory = ParentDirectory; CurrentDirectory != BaseDirectory; CurrentDirectory = CurrentDirectory.ParentDirectory)
					{
						if(!TryDeleteEmptyDirectory(CurrentDirectory))
						{
							break;
						}
					}
				}

				// Try to delete the base directory
				TryDeleteEmptyDirectory(BaseDirectory);
			}
			return true;
		}

		/// <summary>
		/// Deletes a directory, if it's empty
		/// </summary>
		/// <param name="Directory">The directory to check</param>
		/// <returns>True if the directory was deleted, false if not</returns>
		static bool TryDeleteEmptyDirectory(DirectoryReference CandidateDirectory)
		{
			// Check if there are any files in it. If there are, don't bother trying to delete it.
			if(Directory.EnumerateFiles(CandidateDirectory.FullName).Any() || Directory.EnumerateDirectories(CandidateDirectory.FullName).Any())
			{
				return false;
			}

			// Try to delete the directory.
			try
			{
				Directory.Delete(CandidateDirectory.FullName);
				return true;
			}
			catch(Exception Ex)
			{
				CommandUtils.LogWarning("Couldn't delete directory {0} ({1})", CandidateDirectory.FullName, Ex.Message);
				return false;
			}
		}
	}
}
