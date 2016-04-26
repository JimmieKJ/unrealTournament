using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the version task
	/// </summary>
	public class SetVersionTaskParameters
	{
		/// <summary>
		/// The changelist to set in the version files
		/// </summary>
		[TaskParameter]
		public int Change;

		/// <summary>
		/// The branch string
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// Whether to set the IS_LICENSEE_VERSION flag to true
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Licensee;

		/// <summary>
		/// If set, don't actually write to the files - just return the version files that would be updated. Useful for local builds.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool SkipWrite;
	}

	/// <summary>
	/// Task which updates the version files in the current branch
	/// </summary>
	[TaskElement("SetVersion", typeof(SetVersionTaskParameters))]
	public class SetVersionTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		SetVersionTaskParameters Parameters;

		/// <summary>
		/// Construct a version task
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public SetVersionTask(SetVersionTaskParameters InParameters)
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
			List<string> FileNames = UE4Build.StaticUpdateVersionFiles(Parameters.Change, Parameters.Branch, Parameters.Licensee, !Parameters.SkipWrite);
			BuildProducts.UnionWith(FileNames.Select(x => new FileReference(x)));
			return true;
		}
	}
}
