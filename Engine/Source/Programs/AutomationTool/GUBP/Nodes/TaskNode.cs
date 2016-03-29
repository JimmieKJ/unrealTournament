// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Describes a node which contains a sequence of tasks to be executed.
	/// </summary>
	public class TaskNodeDefinition : BuildNodeDefinition
	{
		/// <summary>
		/// List of the tasks to be executed.
		/// </summary>
		[XmlElement("Command", typeof(CommandTaskDefinition))]
		[XmlElement("Compile", typeof(CompileTaskDefinition))]
		[XmlElement("Cook", typeof(CookTaskDefinition))]
		[XmlElement("MsBuild", typeof(MsBuildTaskDefinition))]
		[XmlElement("Output", typeof(OutputTaskDefinition))]
		[XmlElement("Script", typeof(ScriptTaskDefinition))]
		public List<BuildTaskDefinition> Tasks = new List<BuildTaskDefinition>();

		/// <summary>
		/// Add this element to the build graph.
		/// </summary>
		/// <param name="Context">Context for the graph traversal</param>
		/// <param name="AggregateNodeDefinitions">List of aggregate nodes defined so far</param>
		/// <param name="BuildNodeDefinitions">List of build nodes defined so far</param>
		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			BuildNodeDefinitions.Add(this);
		}

		/// <summary>
		/// Construct a build node from this definition.
		/// </summary>
		/// <returns>New instance of a build node</returns>
		public override BuildNode CreateNode()
		{
			return new TaskNode(this);
		}
	}

	/// <summary>
	/// Node which contains a sequence of tasks to be executed.
	/// </summary>
	public class TaskNode : BuildNode
	{
		/// <summary>
		/// Array of tasks to be executed.
		/// </summary>
		public BuildTask[] Tasks;

		/// <summary>
		/// Constructs a task node from the given definition.
		/// </summary>
		/// <param name="Definition">Definition of the task</param>
		public TaskNode(TaskNodeDefinition Definition)
			: base(Definition)
		{
			// Create instances of all the tasks
			List<BuildTask> TaskInstances = new List<BuildTask>();
			foreach (BuildTaskDefinition Task in Definition.Tasks)
			{
				TaskInstances.Add(Task.CreateTask());
			}

			// Merge them together, removing from the above list as we go.
			List<BuildTask> MergedTaskInstances = new List<BuildTask>();
			while (TaskInstances.Count > 0)
			{
				BuildTask NextInstance = TaskInstances[0];
				TaskInstances.RemoveAt(0);
				NextInstance.Merge(TaskInstances);
				MergedTaskInstances.Add(NextInstance);
			}

			// Construct the node
			Tasks = MergedTaskInstances.ToArray();
		}

		public override bool DoBuild()
		{
			BuildProducts = new List<string>();
			foreach(BuildTask Task in Tasks)
			{
				if(!Task.Execute(BuildProducts))
				{
					return false;
				}
			}
			return true;
		}

		public override bool DoFakeBuild()
		{
			string FileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "FakeBuilds", Name + ".txt");
			CommandUtils.WriteAllText(FileName, "Completed: " + Name);
			BuildProducts = new List<string>{ FileName };
			return true;
		}
	}
}
