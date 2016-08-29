using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.Xml;
using System.IO;

namespace BuildGraph.Tasks
{
	/// <summary>
	/// Parameters for a task which calls another UAT command
	/// </summary>
	public class CommandTaskParameters
	{
		/// <summary>
		/// The command name to execute
		/// </summary>
		[TaskParameter]
		public string Name;

		/// <summary>
		/// Arguments to be passed to the command
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// If non-null, instructs telemetry from the command to be merged into the telemetry for this UAT instance with the given prefix. May be an empty (non-null) string.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string MergeTelemetryWithPrefix;
	}

	/// <summary>
	/// Invokes an AutomationTool child process to run the given command.
	/// </summary>
	[TaskElement("Command", typeof(CommandTaskParameters))]
	public class CommandTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CommandTaskParameters Parameters;

		/// <summary>
		/// Construct a new CommandTask.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CommandTask(CommandTaskParameters InParameters)
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
			// If we're merging telemetry from the child process, get a temp filename for it
			FileReference TelemetryFile = null;
			if (Parameters.MergeTelemetryWithPrefix != null)
			{
				TelemetryFile = FileReference.Combine(CommandUtils.RootDirectory, "Engine", "Intermediate", "UAT", "Telemetry.json");
				TelemetryFile.Directory.CreateDirectory();
			}

			// Run the command
			string CommandLine = Parameters.Name;
			if (!String.IsNullOrEmpty(Parameters.Arguments))
			{
				CommandLine += String.Format(" {0}", Parameters.Arguments);
			}
			if(TelemetryFile != null)
			{
				CommandLine += String.Format(" -Telemetry={0}", CommandUtils.MakePathSafeToUseWithCommandLine(TelemetryFile.FullName));
			}
			try
			{
				CommandUtils.RunUAT(CommandUtils.CmdEnv, CommandLine);
			}
			catch(CommandUtils.CommandFailedException)
			{
				return false;
			}

			// Merge in any new telemetry data that was produced
			if (Parameters.MergeTelemetryWithPrefix != null)
			{
				TelemetryData NewTelemetry;
				if (TelemetryData.TryRead(TelemetryFile.FullName, out NewTelemetry))
				{
					CommandUtils.Telemetry.Merge(Parameters.MergeTelemetryWithPrefix, NewTelemetry);
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
