using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.IO;
using System.Threading;
using System.Xml;

namespace Win.Automation
{
	/// <summary>
	/// Parameters for a task that purges data from a symbol store after a given age
	/// </summary>
	public class AgeStoreTaskParameters
	{
		/// <summary>
		/// List of output files. PDBs will be extracted from this list.
		/// </summary>
		[TaskParameter]
		public string StoreDir;

		/// <summary>
		/// Number of days worth of symbols to keep iOutput directory for the compressed symbols.
		/// </summary>
		[TaskParameter]
		public int Days;
	}

	/// <summary>
	/// Task which strips symbols from a set of files. This task is named after the AGESTORE utility that comes with the Microsoft debugger tools SDK, but is actually a separate implementation. The main
	/// difference is that it uses the last modified time rather than last access time to determine which files to delete.
	/// </summary>
	[TaskElement("AgeStore", typeof(AgeStoreTaskParameters))]
	public class AgeStoreTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		AgeStoreTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public AgeStoreTask(AgeStoreTaskParameters InParameters)
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
			// Get the symbol store directory
			DirectoryInfo StoreDir = new DirectoryInfo(ResolveDirectory(Parameters.StoreDir).FullName);

			// Get the time at which to expire files
			DateTime ExpireTimeUtc = DateTime.UtcNow - TimeSpan.FromDays((double)Parameters.Days);
			CommandUtils.Log("Expiring all files before {0}...", ExpireTimeUtc);

			// Scan the store directory
			foreach (DirectoryInfo PdbDir in StoreDir.EnumerateDirectories("*.pdb"))
			{
				foreach (DirectoryInfo HashDir in PdbDir.EnumerateDirectories())
				{
					if (HashDir.EnumerateFiles().All(x => x.LastWriteTimeUtc < ExpireTimeUtc))
					{
						try
						{
							HashDir.Delete(true);
							CommandUtils.Log("Removed '{0}'", HashDir.FullName);
						}
						catch
						{
							CommandUtils.LogWarning("Couldn't delete '{0}' - skipping", HashDir.FullName);
						}
					}
				}
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
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
