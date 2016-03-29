// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	class BuildGraph
	{
		[DebuggerDisplay("{Node.Name}")]
		class UnlinkedBuildNode
		{
			public string[] InputDependencyNames;
			public string[] OrderDependencyNames;
			public BuildNode Node;
		}

		[DebuggerDisplay("{Node.Name}")]
		class UnlinkedAggregateNode
		{
			public string[] DependencyNames;
			public AggregateNode Node;
		}

		public List<BuildNode> BuildNodes;
		public List<AggregateNode> AggregateNodes;

		/// <summary>
		/// Constructor. Creates build nodes from the given template.
		/// </summary>
		/// <param name="Template">Template for this build graph.</param>
		public BuildGraph(IEnumerable<AggregateNodeDefinition> AggregateNodeDefinitions, IEnumerable<BuildNodeDefinition> BuildNodeDefinitions)
		{
			LinkGraph(AggregateNodeDefinitions, BuildNodeDefinitions, out AggregateNodes, out BuildNodes);
			FindControllingTriggers(BuildNodes);
			AddTriggerDependencies(BuildNodes);
		}

		/// <summary>
		/// Resolves the names of each node and aggregates' dependencies, and links them together into the build graph.
		/// </summary>
		/// <param name="AggregateNameToInfo">Map of aggregate names to their info objects</param>
		/// <param name="NodeNameToInfo">Map of node names to their info objects</param>
		private static void LinkGraph(IEnumerable<AggregateNodeDefinition> AggregateNodeDefinitions, IEnumerable<BuildNodeDefinition> BuildNodeDefinitions, out List<AggregateNode> AggregateNodes, out List<BuildNode> BuildNodes)
		{
			// Create all the build nodes, and a dictionary to find them by name
			Dictionary<string, UnlinkedBuildNode> NameToBuildNode = new Dictionary<string, UnlinkedBuildNode>(StringComparer.InvariantCultureIgnoreCase);
			foreach (BuildNodeDefinition Definition in BuildNodeDefinitions)
			{
				UnlinkedBuildNode UnlinkedNode = new UnlinkedBuildNode();
				UnlinkedNode.InputDependencyNames = ParseSemicolonDelimitedList(Definition.DependsOn);
				UnlinkedNode.OrderDependencyNames = ParseSemicolonDelimitedList(Definition.RunAfter);
				UnlinkedNode.Node = Definition.CreateNode();
				NameToBuildNode.Add(Definition.Name, UnlinkedNode);
			}

			// Create all the aggregate nodes, and add them to a dictionary too
			Dictionary<string, UnlinkedAggregateNode> NameToAggregateNode = new Dictionary<string, UnlinkedAggregateNode>(StringComparer.InvariantCultureIgnoreCase);
			foreach (AggregateNodeDefinition Definition in AggregateNodeDefinitions)
			{
				UnlinkedAggregateNode UnlinkedNode = new UnlinkedAggregateNode();
				UnlinkedNode.DependencyNames = ParseSemicolonDelimitedList(Definition.DependsOn);
				UnlinkedNode.Node = new AggregateNode(Definition);
				NameToAggregateNode.Add(Definition.Name, UnlinkedNode);
			}

			// Link the graph
			int NumErrors = 0;
			foreach (UnlinkedAggregateNode Pair in NameToAggregateNode.Values)
			{
				LinkAggregate(Pair, NameToAggregateNode, NameToBuildNode, ref NumErrors);
			}
			foreach (UnlinkedBuildNode Pair in NameToBuildNode.Values)
			{
				LinkNode(Pair, new List<UnlinkedBuildNode>(), NameToAggregateNode, NameToBuildNode, ref NumErrors);
			}
			if (NumErrors > 0)
			{
				throw new AutomationException("Failed to link graph ({0} errors).", NumErrors);
			}

			// Set the output lists
			AggregateNodes = NameToAggregateNode.Values.Select(x => x.Node).ToList();
			BuildNodes = NameToBuildNode.Values.Select(x => x.Node).ToList();
		}

		/// <summary>
		/// Resolves the dependency names in an aggregate to NodeInfo instances, filling in the AggregateInfo.Dependenices array. Any referenced aggregates will also be linked, recursively.
		/// </summary>
		/// <param name="UnlinkedNode">The aggregate to link</param>
		/// <param name="NameToAggregateNode">Map of other aggregate names to their corresponding instance.</param>
		/// <param name="NameToBuildNode">Map from node names to their corresponding instance.</param>
		/// <param name="NumErrors">The number of errors output so far. Incremented if resolving this aggregate fails.</param>
		private static void LinkAggregate(UnlinkedAggregateNode UnlinkedNode, Dictionary<string, UnlinkedAggregateNode> NameToAggregateNode, Dictionary<string, UnlinkedBuildNode> NameToBuildNode, ref int NumErrors)
		{
			if (UnlinkedNode.Node.Dependencies == null)
			{
				UnlinkedNode.Node.Dependencies = new BuildNode[0];

				HashSet<BuildNode> Dependencies = new HashSet<BuildNode>();
				foreach (string DependencyName in UnlinkedNode.DependencyNames)
				{
					UnlinkedAggregateNode AggregateNodeDependency;
					if (NameToAggregateNode.TryGetValue(DependencyName, out AggregateNodeDependency))
					{
						LinkAggregate(AggregateNodeDependency, NameToAggregateNode, NameToBuildNode, ref NumErrors);
						Dependencies.UnionWith(AggregateNodeDependency.Node.Dependencies);
						continue;
					}

					UnlinkedBuildNode BuildNodeDependency;
					if (NameToBuildNode.TryGetValue(DependencyName, out BuildNodeDependency))
					{
						Dependencies.Add(BuildNodeDependency.Node);
						continue;
					}

					CommandUtils.LogError("Node {0} is not in the graph. It is a dependency of {1}.", DependencyName, UnlinkedNode.Node.Name);
					NumErrors++;
				}

				UnlinkedNode.Node.Dependencies = Dependencies.ToArray();
			}
		}

		/// <summary>
		/// Resolves the dependencies in a build graph
		/// </summary>
		/// <param name="UnlinkedNode">The build node template and instance to link.</param>
		/// <param name="Stack">Stack of all the build nodes currently being processed. Used to detect cycles.</param>
		/// <param name="NameToAggregateNode">Mapping of aggregate node names to their template and instance.</param>
		/// <param name="NameToBuildNode">Mapping of build node names to their template and instance.</param>
		/// <param name="NumErrors">Number of errors encountered. Will be incremented for each error encountered.</param>
		private static void LinkNode(UnlinkedBuildNode UnlinkedNode, List<UnlinkedBuildNode> Stack, Dictionary<string, UnlinkedAggregateNode> NameToAggregateNode, Dictionary<string, UnlinkedBuildNode> NameToBuildNode, ref int NumErrors)
		{
			if (UnlinkedNode.Node.OrderDependencies == null)
			{
				Stack.Add(UnlinkedNode);

				int StackIdx = Stack.IndexOf(UnlinkedNode);
				if (StackIdx != Stack.Count - 1)
				{
					// There's a cycle in the build graph. Print out the chain.
					CommandUtils.LogError("Build graph contains a cycle ({0})", String.Join(" -> ", Stack.Skip(StackIdx).Select(x => x.Node.Name)));
					NumErrors++;
					UnlinkedNode.Node.OrderDependencies = new HashSet<BuildNode>();
					UnlinkedNode.Node.InputDependencies = new HashSet<BuildNode>();
				}
				else
				{
					// Find all the direct input dependencies
					HashSet<BuildNode> DirectInputDependencies = new HashSet<BuildNode>();
					foreach (string InputDependencyName in UnlinkedNode.InputDependencyNames)
					{
						if (!ResolveDependencies(InputDependencyName, NameToAggregateNode, NameToBuildNode, DirectInputDependencies))
						{
							CommandUtils.LogError("Node {0} is not in the graph. It is an input dependency of {1}.", InputDependencyName, UnlinkedNode.Node.Name);
							NumErrors++;
						}
					}

					// Find all the direct order dependencies. All the input dependencies are also order dependencies.
					HashSet<BuildNode> DirectOrderDependencies = new HashSet<BuildNode>(DirectInputDependencies);
					foreach (string OrderDependencyName in UnlinkedNode.OrderDependencyNames)
					{
						if (!ResolveDependencies(OrderDependencyName, NameToAggregateNode, NameToBuildNode, DirectOrderDependencies))
						{
							CommandUtils.LogError("Node {0} is not in the graph. It is an order dependency of {1}.", OrderDependencyName, UnlinkedNode.Node.Name);
							NumErrors++;
						}
					}

					// Link everything
					foreach (BuildNode DirectOrderDependency in DirectOrderDependencies)
					{
						LinkNode(NameToBuildNode[DirectOrderDependency.Name], Stack, NameToAggregateNode, NameToBuildNode, ref NumErrors);
					}

					// Recursively include all the input dependencies
					UnlinkedNode.Node.InputDependencies = new HashSet<BuildNode>(DirectInputDependencies);
					foreach (BuildNode DirectInputDependency in DirectInputDependencies)
					{
						UnlinkedNode.Node.InputDependencies.UnionWith(DirectInputDependency.InputDependencies);
					}

					// Same for the order dependencies
					UnlinkedNode.Node.OrderDependencies = new HashSet<BuildNode>(DirectOrderDependencies);
					foreach (BuildNode DirectOrderDependency in DirectOrderDependencies)
					{
						UnlinkedNode.Node.OrderDependencies.UnionWith(DirectOrderDependency.OrderDependencies);
					}
				}

				Stack.RemoveAt(Stack.Count - 1);
			}
		}

		/// <summary>
		/// Adds all the nodes matching a given name to a hash set, expanding any aggregates to their dependencices.
		/// </summary>
		/// <param name="Name">The name to look for</param>
		/// <param name="AggregateNameToInfo">Map of other aggregate names to their corresponding info instance.</param>
		/// <param name="NodeNameToInfo">Map from node names to their corresponding info instance.</param>
		/// <param name="Dependencies">The set of dependencies to add to.</param>
		/// <returns>True if the name was found (and the dependencies list was updated), false otherwise.</returns>
		private static bool ResolveDependencies(string Name, Dictionary<string, UnlinkedAggregateNode> AggregateNodeNameToPair, Dictionary<string, UnlinkedBuildNode> BuildNodeNameToPair, HashSet<BuildNode> Dependencies)
		{
			UnlinkedAggregateNode AggregateDependency;
			if (AggregateNodeNameToPair.TryGetValue(Name, out AggregateDependency))
			{
				Dependencies.UnionWith(AggregateDependency.Node.Dependencies);
				return true;
			}

			UnlinkedBuildNode NodeDependency;
			if (BuildNodeNameToPair.TryGetValue(Name, out NodeDependency))
			{
				Dependencies.Add(NodeDependency.Node);
				return true;
			}

			return false;
		}

		/// <summary>
		/// Parses a semi-colon delimited list into a sequence of strings.
		/// </summary>
		/// <param name="StringList">Semicolon delimited list of strings</param>
		/// <returns>Sequence of strings</returns>
		private static string[] ParseSemicolonDelimitedList(string StringList)
		{
			return StringList.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
		}

		/// <summary>
		/// Recursively update the ControllingTriggers array for each of the nodes passed in. 
		/// </summary>
		/// <param name="NodesToDo">Nodes to update</param>
		static void FindControllingTriggers(List<BuildNode> Nodes)
		{
			foreach (BuildNode Node in Nodes)
			{
				FindControllingTriggers(Node);
			}
		}

		/// <summary>
		/// Recursively find the controlling triggers for the given node.
		/// </summary>
		/// <param name="NodeToDo">Node to find the controlling triggers for</param>
		static void FindControllingTriggers(BuildNode Node)
		{
			if (Node.ControllingTriggers == null)
			{
				Node.ControllingTriggers = new TriggerNode[0];

				// Find the immediate trigger controlling this one
				List<TriggerNode> PreviousTriggers = new List<TriggerNode>();
				foreach (BuildNode Dependency in Node.OrderDependencies)
				{
					FindControllingTriggers(Dependency);

					TriggerNode PreviousTrigger = Dependency as TriggerNode;
					if (PreviousTrigger == null && Dependency.ControllingTriggers.Length > 0)
					{
						PreviousTrigger = Dependency.ControllingTriggers.Last();
					}

					if (PreviousTrigger != null && !PreviousTriggers.Contains(PreviousTrigger))
					{
						PreviousTriggers.Add(PreviousTrigger);
					}
				}

				// Remove previous triggers from the list that aren't the last in the chain. If there's a trigger chain of X.Y.Z, and a node has dependencies behind all three, the only trigger we care about is Z.
				PreviousTriggers.RemoveAll(x => PreviousTriggers.Any(y => y.ControllingTriggers.Contains(x)));

				// We only support one direct controlling trigger at the moment (though it may be in a chain with other triggers)
				if (PreviousTriggers.Count > 1)
				{
					throw new AutomationException("Node {0} has multiple controlling triggers: {1}", Node.Name, String.Join(", ", PreviousTriggers.Select(x => x.Name)));
				}

				// Update the list of controlling triggers
				if (PreviousTriggers.Count == 1)
				{
					List<TriggerNode> ControllingTriggers = new List<TriggerNode>();
					ControllingTriggers.AddRange(PreviousTriggers[0].ControllingTriggers);
					ControllingTriggers.Add(PreviousTriggers[0]);
					Node.ControllingTriggers = ControllingTriggers.ToArray();
				}
			}
		}

		/// <summary>
		/// Adds all the dependencies from nodes behind triggers to the nodes in front of a trigger as pseudo-dependencies of the trigger itself.
		/// This prevents situations where the trigger can be activated before all the dependencies behind it are complete.
		/// </summary>
		void AddTriggerDependencies(List<BuildNode> Nodes)
		{
			foreach(BuildNode Node in Nodes)
			{
				foreach(TriggerNode TriggerNode in Node.ControllingTriggers)
				{
					TriggerNode.OrderDependencies.UnionWith(Node.InputDependencies.Where(x => x != TriggerNode && !x.ControllingTriggers.Contains(TriggerNode)));
					TriggerNode.OrderDependencies.UnionWith(Node.OrderDependencies.Where(x => x != TriggerNode && !x.ControllingTriggers.Contains(TriggerNode)));
				}
			}
		}
	}
}
