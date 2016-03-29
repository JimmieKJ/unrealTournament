using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the error task
	/// </summary>
	public class ErrorTaskParameters
	{
		/// <summary>
		/// Message to display before failing
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Message;
	}

	/// <summary>
	/// Task which outputs and error message and fails the build.
	/// </summary>
	[TaskElement("Error", typeof(ErrorTaskParameters))]
	public class ErrorTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ErrorTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ErrorTask(ErrorTaskParameters InParameters)
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
			if(!String.IsNullOrEmpty(Parameters.Message))
			{
				CommandUtils.LogError(Parameters.Message);
			}
			return false;
		}
	}
}
