// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace AutomationTool
{
	/// <summary>
	/// Stores the context for a build graph definition as it's being parsed
	/// </summary>
	public class BuildGraphContext
	{
		public Dictionary<string, string> Variables;
		public string AgentSharingGroup;

		public BuildGraphContext()
		{
			Variables = new Dictionary<string,string>();
			AgentSharingGroup = "";
		}

		public BuildGraphContext(BuildGraphContext Other)
		{
			Variables = new Dictionary<string,string>(Other.Variables);
			AgentSharingGroup = Other.AgentSharingGroup;
		}

		public string ExpandVariables(string Text)
		{
			return UnrealBuildTool.Utils.ExpandVariables(Text, Variables);
		}
	}

	/// <summary>
	/// Base class for all elements in the build graph
	/// </summary>
	public abstract class ElementDefinition
	{
		public abstract void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions);
	}

	/// <summary>
	/// Definition for a variable. Can be expanded by text elements using the syntax $(Name).
	/// </summary>
    public class VariableDefinition : ElementDefinition
    {
        [XmlAttribute]
        public string Name;

        [XmlAttribute]
        public string Value;

		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			Context.Variables[Name] = Value;
		}
    }

	/// <summary>
	/// Imports part of a build graph from another file
	/// </summary>
    public class ImportDefinition : ElementDefinition
    {
        [XmlAttribute]
        public string Path;

        [XmlAttribute]
        public string Nodes;

        [XmlAttribute]
        public bool IgnoreMissing = false;

		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			throw new NotImplementedException();
		}
    }

	/// <summary>
	/// Parent for all elements that can contain child elements
	/// </summary>
	public abstract class ContainerDefinition : ElementDefinition
	{
        [XmlElement("Node", typeof(TaskNodeDefinition))]
        [XmlElement("Variable", typeof(VariableDefinition))]
        [XmlElement("AgentSharingGroup", typeof(AgentSharingGroupDefinition))]
//        [XmlElement("Trigger", typeof(TriggerDefinition))]
		[XmlElement("LegacyNode", typeof(LegacyNodeDefinition))]
		[XmlElement("Aggregate", typeof(AggregateNodeDefinition))]
        public List<ElementDefinition> Elements = new List<ElementDefinition>();
	}

	/// <summary>
	/// Container for elements in an agent sharing group
	/// </summary>
	public class AgentSharingGroupDefinition : ContainerDefinition
	{
		[XmlAttribute]
		public string Name;

		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			if(!String.IsNullOrEmpty(Context.AgentSharingGroup))
			{
				throw new AutomationException("Cannot define an agent sharing group ({0}) within another agent sharing group ({1})", Name, Context.AgentSharingGroup);
			}

			BuildGraphContext ChildContext = new BuildGraphContext(Context);
			ChildContext.AgentSharingGroup = Name;

			foreach(ElementDefinition Element in Elements)
			{
				Element.AddToGraph(ChildContext, AggregateNodeDefinitions, BuildNodeDefinitions);
			}
		}
	}

	/// <summary>
	/// Root container for a build graph
	/// </summary>
	[XmlRoot("BuildGraph")]
	public class BuildGraphDefinition : ContainerDefinition
	{
        [XmlAttribute]
        public string DefaultInterval = "1h20m";

		static XmlSerializer Serializer = XmlSerializer.FromTypes(new Type[]{ typeof(BuildGraphDefinition) })[0];

		public void AddToGraph(List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			AddToGraph(new BuildGraphContext(), AggregateNodeDefinitions, BuildNodeDefinitions);
		}

		public override void AddToGraph(BuildGraphContext Context, List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions)
		{
			foreach(ElementDefinition Element in Elements)
			{
				Element.AddToGraph(Context, AggregateNodeDefinitions, BuildNodeDefinitions);
			}
		}

		public static BuildGraphDefinition FromNodeDefinitions(List<AggregateNodeDefinition> AggregateNodeDefinitions, List<BuildNodeDefinition> BuildNodeDefinitions, int Interval)
		{
			BuildGraphDefinition GraphDefinition = new BuildGraphDefinition();

			// Add all the sticky build nodes
			AgentSharingGroupDefinition StickyGroup = null;
			foreach(BuildNodeDefinition Definition in BuildNodeDefinitions)
			{
				if(Definition.IsSticky)
				{ 
					if(StickyGroup == null)
					{
						StickyGroup = new AgentSharingGroupDefinition{ Name = "Sticky" };
						GraphDefinition.Elements.Add(StickyGroup);
					}
					StickyGroup.Elements.Add(Definition);
				}
			}

			// Add all the other build nodes
			HashSet<string> AgentSharingGroups = new HashSet<string>();
			foreach(BuildNodeDefinition Definition in BuildNodeDefinitions)
			{
				if(!Definition.IsSticky)
				{
					if(String.IsNullOrEmpty(Definition.AgentSharingGroup))
					{
						GraphDefinition.Elements.Add(Definition);
					}
					else if(AgentSharingGroups.Add(Definition.AgentSharingGroup))
					{
						GraphDefinition.Elements.Add(new AgentSharingGroupDefinition{ Name = Definition.AgentSharingGroup, Elements = new List<ElementDefinition>(BuildNodeDefinitions.Where(x => x.AgentSharingGroup == Definition.AgentSharingGroup)) });
					}
				}
			}

			// Add all the aggregate nodes
			GraphDefinition.Elements.AddRange(AggregateNodeDefinitions);
			return GraphDefinition;
		}

		public static BuildGraphDefinition Read(string FileName)
		{
			using(StreamReader Reader = new StreamReader(FileName))
			{
				return (BuildGraphDefinition)Serializer.Deserialize(Reader);
			}
		}

		public void Write(string FileName)
		{
			using(StreamWriter Writer = new StreamWriter(FileName))
			{
				Serializer.Serialize(Writer, this);
			}
		}
	}
}
