using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Stores a list of nodes which can be executed on a single agent
	/// </summary>
	[DebuggerDisplay("{Name}")]
	class AgentGroup
	{
		/// <summary>
		/// Name of this agent group. Used for display purposes in a build system.
		/// </summary>
		public string Name;

		/// <summary>
		/// Array of valid agent types that these nodes may run on. When running in the build system, this determines the class of machine that should
		/// be selected to run these nodes. The first defined agent type for this branch will be used.
		/// </summary>
		public string[] AgentTypes;

		/// <summary>
		/// List of nodes in this agent group.
		/// </summary>
		public List<Node> Nodes = new List<Node>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of this agent group</param>
		/// <param name="InAgentTypes">Array of valid agent types. See comment for AgentTypes member.</param>
		public AgentGroup(string InName, string[] InAgentTypes)
		{
			Name = InName;
			AgentTypes = InAgentTypes;
		}
	}
}
