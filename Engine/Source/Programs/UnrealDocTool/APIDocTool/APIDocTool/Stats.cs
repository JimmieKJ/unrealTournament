// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public class Stat
	{
		public int NumDocumented;
		public int NumUndocumented;
	}

	public class ModuleStats
	{
		public Dictionary<string, Stat> Stats = new Dictionary<string, Stat>();
	}

	public class Stats
	{
		public Dictionary<Type, int> PageCounts = new Dictionary<Type, int>();
		public Dictionary<string, ModuleStats> Modules = new Dictionary<string, ModuleStats>();
		public Dictionary<string, Stat> Totals = new Dictionary<string, Stat>();

		public Stats(IEnumerable<APIMember> InMembers)
		{
			List<APIMember> Members = new List<APIMember>(InMembers);

			// Generate the page counts
			foreach(APIPage Page in Members)
			{
				Type PageType = Page.GetType();
				int PageCount = PageCounts.ContainsKey(PageType)? PageCounts[PageType] : 0;
				PageCounts.Remove(PageType);
				PageCounts.Add(PageType, PageCount + 1);
			}

			// Check all the records
			foreach(APIRecord Record in Members.OfType<APIRecord>())
			{
				AddStat(Record, "Class descriptions", Record.BriefDescription, Record.FullDescription);
			}

			// Check all the functions
			foreach(APIFunction Function in Members.OfType<APIFunction>())
			{
				AddStat(Function, "Function descriptions", Function.BriefDescription, Function.FullDescription);
			}

			// Check all the constants
			foreach (APIConstant Constant in Members.OfType<APIConstant>())
			{
				AddStat(Constant, "Constant descriptions", Constant.BriefDescription, Constant.FullDescription);
			}

			// Check all the enums
			foreach(APIEnum Enum in Members.OfType<APIEnum>())
			{
				AddStat(Enum, "Enum descriptions", Enum.BriefDescription, Enum.FullDescription);
				foreach (APIEnumValue EnumValue in Enum.Values)
				{
					AddStat(Enum, "Enum value descriptions", EnumValue.BriefDescription, EnumValue.FullDescription);
				}
			}

			// Check all the typedefs
			foreach (APITypeDef TypeDef in Members.OfType<APITypeDef>())
			{
				AddStat(TypeDef, "Typedef descriptions", TypeDef.BriefDescription, TypeDef.FullDescription);
			}

			// Check all the variables
			foreach (APIVariable Variable in Members.OfType<APIVariable>())
			{
				AddStat(Variable, "Variable descriptions", Variable.BriefDescription, Variable.FullDescription);
			}

			// Create the totals
			foreach(KeyValuePair<string, ModuleStats> ModulePair in Modules)
			{
				foreach(KeyValuePair<string, Stat> StatPair in ModulePair.Value.Stats)
				{
					Stat TotalStat;
					if (!Totals.TryGetValue(StatPair.Key, out TotalStat))
					{
						TotalStat = new Stat();
						Totals.Add(StatPair.Key, TotalStat);
					}
					TotalStat.NumDocumented += StatPair.Value.NumDocumented;
					TotalStat.NumUndocumented += StatPair.Value.NumUndocumented;
				}
			}
		}

		public Stat AddStat(APIMember Member, string Name)
		{
			for(APIPage Page = Member; Page != null; Page = Page.Parent)
			{
				APIModule Module = Page as APIModule;
				if(Module != null)
				{
					// Get the stats for this module
					ModuleStats ModuleStatsInst;
					if(!Modules.TryGetValue(Module.Name, out ModuleStatsInst))
					{
						ModuleStatsInst = new ModuleStats();
						Modules.Add(Module.Name, ModuleStatsInst);
					}

					// Find the name of this stat
					Stat StatInst;
					if (!ModuleStatsInst.Stats.TryGetValue(Name, out StatInst))
					{
						StatInst = new Stat();
						ModuleStatsInst.Stats.Add(Name, StatInst);
					}

					// Update the current 
					return StatInst;
				}
			}
			return null;
		}

		public void AddStat(APIMember Member, string Name, string BriefDescription, string DetailedDescription)
		{
			Stat NewStat = AddStat(Member, Name);
			if (NewStat != null)
			{
				if(Utility.IsNullOrWhitespace(BriefDescription) && Utility.IsNullOrWhitespace(DetailedDescription))
				{
					NewStat.NumUndocumented++;
				}
				else
				{
					NewStat.NumDocumented++;
				}
			}
		}

		public void Write(string OutputPath)
		{
			using(StreamWriter Writer = new StreamWriter(OutputPath))
			{
				Writer.WriteLine("h2. Totals");
				Writer.WriteLine();
				WriteStats(Writer, Totals);

				Writer.WriteLine();
				Writer.WriteLine("h2. By Module");
				Writer.WriteLine();
				foreach (KeyValuePair<string, ModuleStats> Module in Modules)
				{
					Writer.WriteLine("h3. {0}", Module.Key);
					Writer.WriteLine();
					WriteStats(Writer, Module.Value.Stats);
				}

				Writer.WriteLine();
				Writer.WriteLine("h2. Page counts");
				Writer.WriteLine();
				Writer.WriteLine("||Type||Count||");
				foreach (KeyValuePair<Type, int> PageCountPair in PageCounts.OrderByDescending(x => x.Value))
				{
					Writer.WriteLine("|{0}|{1}|", PageCountPair.Key.Name, PageCountPair.Value);
				}
				Writer.WriteLine("|{0}|{1}|", "Total", PageCounts.Values.Sum(x => x));
			}
		}

		void WriteStats(StreamWriter Writer, Dictionary<string, Stat> Stats)
		{
			Writer.WriteLine("||Category||Total||Undocumented||Documented||% Documented||");
			foreach (KeyValuePair<string, Stat> StatPair in Stats.OrderBy(x => x.Key))
			{
				int NumDocumented = StatPair.Value.NumDocumented;
				int NumUndocumented = StatPair.Value.NumUndocumented;
				int NumEntities = NumDocumented + NumUndocumented;
				if (NumEntities > 0)
				{
					Writer.WriteLine("|{0}|{1}|{2}|{3}|{4}%|", StatPair.Key, NumEntities, NumUndocumented, NumDocumented, (NumDocumented * 100) / NumEntities);
				}
			}
		}
	}
}
