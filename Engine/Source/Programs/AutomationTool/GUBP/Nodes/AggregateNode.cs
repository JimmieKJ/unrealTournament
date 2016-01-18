using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	[DebuggerDisplay("{Name}")]
	public class AggregateNodeTemplate
	{
		public string Name;
		public bool IsPromotableAggregate;
		public bool IsSeparatePromotable;
		public string DependencyNames;
	}

	[DebuggerDisplay("{Name}")]
	public class AggregateNode
	{
		public string Name;
		public bool IsPromotableAggregate;
		public bool IsSeparatePromotable;
		public BuildNode[] Dependencies;

		public AggregateNode(AggregateNodeTemplate Template)
		{
			Name = Template.Name;
			IsPromotableAggregate = Template.IsPromotableAggregate;
			IsSeparatePromotable = Template.IsSeparatePromotable;
		}
	}

	[DebuggerDisplay("{Definition.Name}")]
	class AggregateNodePair
	{
		public readonly AggregateNodeTemplate Template;
		public readonly AggregateNode Node;

		public AggregateNodePair(AggregateNodeTemplate InTemplate)
		{
			Template = InTemplate;
			Node = new AggregateNode(Template);
		}
	}
}
