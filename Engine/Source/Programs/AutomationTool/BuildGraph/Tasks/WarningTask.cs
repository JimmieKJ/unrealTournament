using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the warning task
	/// </summary>
	public class WarningTaskParameters
	{
		/// <summary>
		/// Message to display before failing
		/// </summary>
		[TaskParameter]
		public string Message;
	}

	/// <summary>
	/// Task which outputs a warning message
	/// </summary>
	[TaskElement("Warning", typeof(WarningTaskParameters))]
	public class WarningTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		WarningTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public WarningTask(WarningTaskParameters InParameters)
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
			CommandUtils.LogWarning(Parameters.Message);
			return true;
		}
	}
}
