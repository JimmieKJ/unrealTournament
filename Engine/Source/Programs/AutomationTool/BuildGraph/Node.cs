using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;
using AutomationTool;

namespace AutomationTool
{
	/// <summary>
	/// Defines a node, a container for tasks and the smallest unit of execution that can be run as part of a build graph.
	/// </summary>
	class Node
	{
		/// <summary>
		/// The node's name
		/// </summary>
		public string Name;

		/// <summary>
		/// Array of input names which this node requires to run
		/// </summary>
		public string[] InputNames;

		/// <summary>
		/// Array of output names produced by this node
		/// </summary>
		public string[] OutputNames;

		/// <summary>
		/// Nodes which this node has input dependencies on
		/// </summary>
		public Node[] InputDependencies;

		/// <summary>
		/// Nodes which this node needs to run after
		/// </summary>
		public Node[] OrderDependencies;

		/// <summary>
		/// The trigger which controls whether this node will be executed
		/// </summary>
		public ManualTrigger ControllingTrigger;

		/// <summary>
		/// List of tasks to execute
		/// </summary>
		public List<CustomTask> Tasks;

		/// <summary>
		/// List of email addresses to notify if this node fails.
		/// </summary>
		public HashSet<string> NotifyUsers = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// If set, anyone that has submitted to one of the given paths will be notified on failure of this node
		/// </summary>
		public HashSet<string> NotifySubmitters = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Whether to ignore warnings produced by this node
		/// </summary>
		public bool bNotifyOnWarnings = true;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">The name of this node</param>
		/// <param name="InInputNames">Names of inputs that this node depends on</param>
		/// <param name="InOutputNames">Names of outputs that this node produces</param>
		/// <param name="InInputDependencies">Nodes which this node is dependent on for its inputs</param>
		/// <param name="InOrderDependencies">Nodes which this node needs to run after. Should include all input dependencies.</param>
		/// <param name="InControllingTrigger">The trigger which this node is behind</param>
		public Node(string InName, string[] InInputNames, string[] InOutputNames, Node[] InInputDependencies, Node[] InOrderDependencies, ManualTrigger InControllingTrigger, List<CustomTask> InTasks)
		{
			Name = InName;
			InputNames = InInputNames;
			OutputNames = InOutputNames;
			InputDependencies = InInputDependencies;
			OrderDependencies = InOrderDependencies;
			ControllingTrigger = InControllingTrigger;
			Tasks = new List<CustomTask>(InTasks);
		}

		/// <summary>
		/// Build all the tasks for this node
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include. Should be set to contain the node inputs on entry.</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public bool Build(JobContext Job, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			bool bResult = true;

			// Allow tasks to merge together
			MergeTasks();

			// Build everything
			HashSet<FileReference> BuildProducts = new HashSet<FileReference>();
			foreach(CustomTask Task in Tasks)
			{
				if(!Task.Execute(Job, BuildProducts, TagNameToFileSet))
				{
					CommandUtils.LogError("Failed to execute task.");
					return false;
				}
			}

			// Build a mapping of build product to the outputs it belongs to, using the filesets created by the tasks.
			Dictionary<FileReference, string> FileToOutputName = new Dictionary<FileReference,string>();
			foreach(string OutputName in OutputNames)
			{
				HashSet<FileReference> FileSet = TagNameToFileSet[OutputName];
				foreach(FileReference File in FileSet)
				{
					string ExistingOutputName;
					if(FileToOutputName.TryGetValue(File, out ExistingOutputName))
					{
						CommandUtils.LogError("Build product is added to multiple outputs; {0} added to {1} and {2}", File.MakeRelativeTo(new DirectoryReference(CommandUtils.CmdEnv.LocalRoot)), ExistingOutputName, OutputName);
						bResult = false;
						continue;
					}
					FileToOutputName.Add(File, OutputName);
				}
			}

			// Any build products left over can be added to the default output.
			TagNameToFileSet[Name].UnionWith(BuildProducts.Where(x => !FileToOutputName.ContainsKey(x)));
			return bResult;
		}

		/// <summary>
		/// Merge tasks which can be combined together
		/// </summary>
		void MergeTasks()
		{
			List<CustomTask> MergedTasks = new List<CustomTask>();
			while(Tasks.Count > 0)
			{
				CustomTask NextTask = Tasks[0];
				Tasks.RemoveAt(0);
				NextTask.Merge(Tasks);
				MergedTasks.Add(NextTask);
			}
			Tasks = MergedTasks;
		}

		/// <summary>
		/// Determines the minimal set of direct input dependencies for this node to run
		/// </summary>
		/// <returns>Sequence of nodes that are direct inputs to this node</returns>
		public IEnumerable<Node> GetDirectInputDependencies()
		{
			HashSet<Node> DirectDependencies = new HashSet<Node>(InputDependencies);
			foreach(Node InputDependency in InputDependencies)
			{
				DirectDependencies.ExceptWith(InputDependency.InputDependencies);
			}
			return DirectDependencies;
		}

		/// <summary>
		/// Determines the minimal set of direct order dependencies for this node to run
		/// </summary>
		/// <returns>Sequence of nodes that are direct order dependencies of this node</returns>
		public IEnumerable<Node> GetDirectOrderDependencies()
		{
			HashSet<Node> DirectDependencies = new HashSet<Node>(OrderDependencies);
			foreach(Node OrderDependency in OrderDependencies)
			{
				DirectDependencies.ExceptWith(OrderDependency.OrderDependencies);
			}
			return DirectDependencies;
		}

		/// <summary>
		/// Checks whether this node is downstream of the given trigger
		/// </summary>
		/// <param name="Trigger">The trigger to check</param>
		/// <returns>True if the node is downstream of the trigger, false otherwise</returns>
		public bool IsControlledBy(ManualTrigger Trigger)
		{
			return Trigger == null || ControllingTrigger == Trigger || (ControllingTrigger != null && ControllingTrigger.IsDownstreamFrom(Trigger));
		}

		/// <summary>
		/// Returns the name of this node
		/// </summary>
		/// <returns>The name of this node</returns>
		public override string ToString()
		{
			return Name;
		}
	}
}
