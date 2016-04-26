// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Base class for the definition of a build task in an XML script
	/// </summary>
	public abstract class BuildTaskDefinition
	{
		/// <summary>
		/// Creates a task from this definition
		/// </summary>
		/// <returns>New instance of a build task</returns>
		public abstract BuildTask CreateTask();
	}

	/// <summary>
	/// Base class for all build tasks
	/// </summary>
	public abstract class BuildTask
	{
		/// <summary>
		/// Allow this task to merge with other tasks within the same node. This can be useful to allow tasks to execute in parallel and reduce overheads.
		/// </summary>
		/// <param name="OtherTasks">Other tasks that this task can merge with. If a merge takes place, the other tasks should be removed from the list.</param>
		public virtual void Merge(List<BuildTask> OtherTasks)
		{
		}

		/// <summary>
		/// Execute this node.
		/// </summary>
		/// <param name="BuildProducts">List of build products created after executing tasks so far. Includes previous tasks.</param>
		/// <returns>Whether the node succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public abstract bool Execute(List<string> BuildProducts);
	}
}
