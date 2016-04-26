// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Defines a collection of other nodes.
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public class AggregateNodeDefinition : ElementDefinition
	{
		/// <summary>
		/// The name of this node.
		/// </summary>
		[XmlAttribute]
		public string Name;

		/// <summary>
		/// List of nodes that this aggregate depends on.
		/// </summary>
		[XmlAttribute]
		public string DependsOn;

		/// <summary>
		/// Add this element to the build graph.
		/// </summary>
		/// <param name="Context">Context for the graph traversal</param>
		/// <param name="AggregateNodeDefinitions">List of aggregate nodes defined so far</param>
		/// <param name="BuildNodeDefinitions">List of build nodes defined so far</param>
		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			AggregateNodeDefinitions.Add(this);
		}
	}

	/// <summary>
	/// Collection of other nodes, which can be used as a build target. Aggregate nodes do not exist in EC, but are expanded out when linked into the build graph.
	/// </summary>
	[DebuggerDisplay("{Name}")]
	class AggregateNode
	{
		/// <summary>
		/// The name of this node.
		/// </summary>
		public string Name;

		/// <summary>
		/// Nodes which this aggregate includes.
		/// </summary>
		public BuildNode[] Dependencies;

		/// <summary>
		/// Construct an aggregate node from the given definition
		/// </summary>
		/// <param name="Definition">Definition for this node</param>
		public AggregateNode(AggregateNodeDefinition Definition)
		{
			Name = Definition.Name;
		}
	}
}
