// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Reflection;
using System.Xml;
using System.Linq;
using System.Diagnostics;

[Help("Runs one, several or all of the GUBP nodes")]
[Help(typeof(UE4Build))]
[Help("NoMac", "Toggle to exclude the Mac host platform, default is Win64+Mac+Linux")]
[Help("NoLinux", "Toggle to exclude the Linux (PC, 64-bit) host platform, default is Win64+Mac+Linux")]
[Help("NoPC", "Toggle to exclude the PC host platform, default is Win64+Mac+Linux")]
[Help("CleanLocal", "delete the local temp storage before we start")]
[Help("TimeIndex=", "An integer used to determine subsets to run based on DependentCISFrequencyQuantumShift")]
[Help("UserTimeIndex=", "An integer used to determine subsets to run based on DependentCISFrequencyQuantumShift, this one overrides TimeIndex")]
[Help("PreflightUID=", "A unique integer tag from EC used as part of the tempstorage, builds and label names to distinguish multiple attempts.")]
[Help("Node=", "Nodes to process, -node=Node1+Node2+Node3, if no nodes or games are specified, defaults to all nodes.")]
[Help("TriggerNode=", "Trigger Nodes to process, -triggernode=Node.")]
[Help("Game=", "Games to process, -game=Game1+Game2+Game3, if no games or nodes are specified, defaults to all nodes.")]
[Help("ListOnly", "List Nodes in this branch")]
[Help("CommanderJobSetupOnly", "Set up the EC branch info via ectool and quit")]
[Help("FakeEC", "don't run ectool, rather just do it locally, emulating what EC would have done.")]
[Help("Fake", "Don't actually build anything, just store a record of success as the build product for each node.")]
[Help("AllPlatforms", "Regardless of what is installed on this machine, set up the graph for all platforms; true by default on build machines.")]
[Help("SkipTriggers", "ignore all triggers")]
[Help("CL", "force the CL to something, disregarding the P4 value.")]
[Help("History", "Like ListOnly, except gives you a full history. Must have -P4 for this to work.")]
[Help("Changes", "Like history, but also shows the P4 changes. Must have -P4 for this to work.")]
[Help("AllChanges", "Like changes except includes changes before the last green. Must have -P4 for this to work.")]
[Help("EmailOnly", "Only emails the folks given in the argument.")]
[Help("AddEmails", "Add these space delimited emails too all email lists.")]
[Help("ShowDependencies", "Show node dependencies.")]
[Help("ShowECDependencies", "Show EC node dependencies instead.")]
[Help("ShowECProc", "Show EC proc names.")]
[Help("ECProject", "From EC, the name of the project, used to get a version number.")]
[Help("CIS", "This is a CIS run, assign TimeIndex based on the history.")]
[Help("ForceIncrementalCompile", "make sure all compiles are incremental")]
[Help("AutomatedTesting", "Allow automated testing, currently disabled.")]
[Help("StompCheck", "Look for stomped build products.")]
public partial class GUBP : BuildCommand
{
	const string StartedTempStorageSuffix = "_Started";
	const string FailedTempStorageSuffix = "_Failed";
	const string SucceededTempStorageSuffix = "_Succeeded";

    
    class NodeHistory
	{
		public int LastSucceeded = 0;
		public int LastFailed = 0;
		public List<int> InProgress = new List<int>();
		public string InProgressString = "";
		public List<int> Failed = new List<int>();
		public string FailedString = "";
		public List<int> AllStarted = new List<int>();
		public List<int> AllSucceeded = new List<int>();
		public List<int> AllFailed = new List<int>();
	};

	/// <summary>
	/// Main entry point for GUBP
	/// </summary>
    public override ExitCode Execute()
    {
		Log("************************* GUBP");

		List<UnrealTargetPlatform> HostPlatforms = new List<UnrealTargetPlatform>();
        if (!ParseParam("NoPC"))
        {
            HostPlatforms.Add(UnrealTargetPlatform.Win64);
        }
        if (!ParseParam("NoMac"))
        {
			HostPlatforms.Add(UnrealTargetPlatform.Mac);
        }
		if(!ParseParam("NoLinux") && UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
		{
			HostPlatforms.Add(UnrealTargetPlatform.Linux);
		}

	    if(ParseParam("CleanLocal"))
        {
            TempStorage.DeleteLocalTempStorage();
        }

		bool bNewEC = ParseParam("NewEC");
        bool bSkipTriggers = ParseParam("SkipTriggers");
        bool bFake = ParseParam("fake");
        bool bFakeEC = ParseParam("FakeEC");

        bool bSaveSharedTempStorage = P4Enabled && (IsBuildMachine || GlobalCommandLine.UseLocalBuildStorage);

        // Get the branch hacker options.
        string BranchName = ParseParamValue("BranchName", P4Enabled ? P4Env.BuildRootP4 : "");
        GUBPBranchHacker.BranchOptions BranchOptions = GetBranchOptions(BranchName);

		foreach (var Kind in BranchOptions.MonolithicsToRemove)
		{
			BranchInfo.MonolithicKinds.Remove(Kind);
		}

		string BranchNameForTempStorage = P4Enabled ? P4Env.BuildRootEscaped : "NoP4";
		string RootNameForTempStorage = BranchOptions.RootNameForTempStorage ?? "UE4";
		string PreflightSuffix = GetPreflightSuffix();
		int ChangeNumber = P4Enabled ? ParseParamInt("CL", P4Env.Changelist) : 0;
		bool bIsPreflight = PreflightSuffix != null;
		string BuildFolder = string.Format("{0}{1}", ChangeNumber, PreflightSuffix);

		string TempStorageDir;
		if(bNewEC)
		{
			TempStorageDir = ParseParamValue("TempStorageDir");
		}
		else
		{
			TempStorageDir = GetTempStorageDir(bSaveSharedTempStorage, bFakeEC, BranchNameForTempStorage, RootNameForTempStorage, BuildFolder);
		}
		
        bool CommanderSetup = ParseParam("CommanderJobSetupOnly");

		string ExplicitTriggerName = "";
		if (CommanderSetup)
		{
			ExplicitTriggerName = ParseParamValue("TriggerNode", "");
		}

		// Create the build graph
		int TimeQuantum = 20;
		List<AggregateNodeDefinition> AggregateNodeDefinitions = new List<AggregateNodeDefinition>();
		List<BuildNodeDefinition> BuildNodeDefinitions = new List<BuildNodeDefinition>();

		// Add all the nodes to it
		string ScriptName = ParseParamValue("Script");
		if(ScriptName == null)
		{
			// Automatically configure it from whatever is in the branch
			string BuildName = "CL-" + (P4Enabled? P4Env.ChangelistString : "Unknown") + (PreflightSuffix ?? "");
			AddNodesForBranch(HostPlatforms, BranchName, new JobInfo(BuildName, PreflightSuffix != null), BranchOptions, out BuildNodeDefinitions, out AggregateNodeDefinitions, ref TimeQuantum, bNewEC);

			// Check if we're exporting the graph as a starting point for manually configuring it
			string OutputScriptName = ParseParamValue("ExportScript");
			if(OutputScriptName != null)
			{
				BuildGraphDefinition Definition = BuildGraphDefinition.FromNodeDefinitions(AggregateNodeDefinitions, BuildNodeDefinitions, 20);
				Definition.Write(OutputScriptName);
				return ExitCode.Success;
			}
		}
		else
		{
			// Read it from a script
			BuildGraphDefinition Definition = BuildGraphDefinition.Read(ScriptName);
			Definition.AddToGraph(AggregateNodeDefinitions, BuildNodeDefinitions);
		}

		BuildGraph Graph = new BuildGraph(AggregateNodeDefinitions, BuildNodeDefinitions);
		FindCompletionState(Graph.BuildNodes, TempStorageDir);
		ComputeDependentFrequencies(Graph.BuildNodes);
		//ValidateTriggerDependencies(Graph.BuildNodes);

		int TimeIndex = ParseParamInt("UserTimeIndex", -1);
		if (TimeIndex == -1)
		{
			TimeIndex = ParseParamInt("TimeIndex", -1);
		}
		if (ParseParam("CIS") && ExplicitTriggerName == "" && CommanderSetup) // explicit triggers will already have a time index assigned
		{
			TimeIndex = UpdateCISCounter(TimeQuantum);
			Log("Setting TimeIndex to {0}", TimeIndex);
		}

		// Find the list of nodes which we're actually going to execute
		HashSet<BuildNode> NodesToDo = ParseNodesToDo(Graph.BuildNodes, Graph.AggregateNodes);
		CullNodesForTimeIndex(NodesToDo, TimeIndex);
        if (bIsPreflight)
        {
            CullNodesForPreflight(NodesToDo);
        }

		// Mark all the nodes whose build products need to be copied to temp storage. Nodes which appear in the same agent sharing group do not need to be 
		// copied; we don't clean between builds on the same agent.
		foreach(BuildNode NodeToDo in NodesToDo)
		{
			foreach(BuildNode InputDependency in NodeToDo.InputDependencies)
			{
				if(String.IsNullOrEmpty(InputDependency.AgentSharingGroup) || InputDependency.AgentSharingGroup != NodeToDo.AgentSharingGroup || NodeToDo.IsSticky != InputDependency.IsSticky)
				{
					InputDependency.CopyToSharedStorage = true;
				}
			}
		}
		foreach(TriggerNode TriggerNode in NodesToDo.OfType<TriggerNode>())
		{
			TriggerNode.CopyToSharedStorage = true;
		}

		// From each of those nodes, remove any order dependencies that we're not going to run. There are no circumstances in which they should be 
		// considered dependencies from this point on.
		foreach(BuildNode NodeToDo in NodesToDo)
		{
			NodeToDo.OrderDependencies.RemoveWhere(x => !NodesToDo.Contains(x));
		}

		TriggerNode ExplicitTrigger = null;
		if (CommanderSetup)
        {
            if (!String.IsNullOrEmpty(ExplicitTriggerName))
            {
                foreach (TriggerNode Trigger in Graph.BuildNodes.OfType<TriggerNode>())
                {
					if (Trigger.Name.Equals(ExplicitTriggerName, StringComparison.InvariantCultureIgnoreCase))
                    {
						Trigger.Activate();
						ExplicitTrigger = Trigger;
                        break;
                    }
                }
                if (ExplicitTrigger == null)
                {
                    throw new AutomationException("Could not find trigger node named {0}", ExplicitTriggerName);
                }
            }
            else
            {
                if (bSkipTriggers)
                {
                    foreach (TriggerNode Trigger in Graph.BuildNodes.OfType<TriggerNode>())
                    {
						Trigger.Activate();
                    }
                }
            }
        }

        List<BuildNode> OrderedToDo = TopologicalSort(NodesToDo, ExplicitTrigger, false, false);

		List<TriggerNode> UnfinishedTriggers = FindUnfinishedTriggers(bSkipTriggers, ExplicitTrigger, OrderedToDo);

		PrintNodes(this, OrderedToDo, Graph.AggregateNodes, UnfinishedTriggers, TimeQuantum);

        //check sorting
		CheckSortOrder(OrderedToDo);

		ElectricCommander EC = new ElectricCommander(this);

		string ShowHistoryParam = ParseParamValue("ShowHistory", null);
		if(ShowHistoryParam != null)
		{
			BuildNode Node = Graph.BuildNodes.FirstOrDefault(x => x.Name.Equals(ShowHistoryParam, StringComparison.InvariantCultureIgnoreCase));
			if(Node == null)
			{
				throw new AutomationException("Couldn't find node {0}", ShowHistoryParam);
			}

            NodeHistory History = FindNodeHistory(Node.Name, ChangeNumber, TempStorageDir);
			if(History == null)
			{
				throw new AutomationException("Couldn't get history for {0}", ShowHistoryParam);
			}

			PrintDetailedChanges(History, P4Env.Changelist);
			return ExitCode.Success;
		}
		else
		{
			if(CommanderSetup)
			{
				return DoCommanderSetup(EC, Graph.BuildNodes, Graph.AggregateNodes, OrderedToDo, TimeIndex, TimeQuantum, bSkipTriggers, bNewEC, bFake, bFakeEC, ExplicitTrigger, UnfinishedTriggers, bIsPreflight);
			}
			else if(ParseParam("ListOnly"))
			{
				return ExitCode.Success;
			}
			else
			{
				return ExecuteNodes(EC, OrderedToDo, bNewEC, bFake, bFakeEC, TempStorageDir, bSaveSharedTempStorage, ChangeNumber);
			}
		}
	}

	/// <summary>
	/// Get a suffix used to identify preflight builds.
	/// </summary>
	/// <returns>Suffix to uniquely identify this preflight. Null if the current build is not a preflight</returns>
	string GetPreflightSuffix()
	{
        int PreflightShelveCL = 0;
        int PreflightUID = 0;
        string PreflightShelveCLString = GetEnvVar("uebp_PreflightShelveCL");
        // We must be passed a valid shelve and be a build machine (or be running a preflight test) to be a valid preflight.
        if (!string.IsNullOrEmpty(PreflightShelveCLString) && (IsBuildMachine || ParseParam("PreflightTest")))
        {
            Log("**** Preflight shelve {0}", PreflightShelveCLString);
            PreflightShelveCL = int.Parse(PreflightShelveCLString);
            if (PreflightShelveCL < 2000000)
            {
                throw new AutomationException("{0} does not look like a CL", PreflightShelveCL);
            }
            PreflightUID = ParseParamInt("PreflightUID", 0);
        }
		return (PreflightShelveCL > 0)? string.Format("-PF-{0}-{1}", PreflightShelveCL, PreflightUID) : null;
	}

	/// <summary>
	/// Determine the directory shared build products should be saved to.
	/// </summary>
	string GetTempStorageDir(bool bSaveSharedTempStorage, bool bFakeEC, string BranchNameForTempStorage, string RootNameForTempStorage, string BuildFolder)
	{
        bool LocalOnly = !P4Enabled || bFakeEC;

        if (bSaveSharedTempStorage)
        {
            if (!TempStorage.IsSharedTempStorageAvailable(true))
            {
                throw new AutomationException("Request to save to shared temp storage, but shared temp storage is unavailable or does not exist.");
            }
        }
        else if (!LocalOnly && !TempStorage.IsSharedTempStorageAvailable(false))
        {
            LogWarning("Looks like we want to use shared temp storage, but since we don't have it, we won't use it.");
            LocalOnly = true;
        }

		string TempStorageDir = null;
		if(!LocalOnly)
		{
			string RootTempStorageDir = TempStorage.ResolveSharedTempStorageDirectory(RootNameForTempStorage);
			TempStorageDir = CommandUtils.CombinePaths(RootTempStorageDir, BranchNameForTempStorage, BuildFolder);
		}
		return TempStorageDir;
	}

	/// <summary>
	/// Get a string describing a time interval, in the format "XhYm".
	/// </summary>
	/// <param name="Minutes">The time interval, in minutes</param>
	/// <returns>String for how frequently the node should be built</returns>
    static string GetTimeIntervalString(int Minutes)
    {
        if (Minutes < 60)
        {
            return String.Format("{0}m", Minutes);
        }
        else
        {
            return String.Format("{0}h{1}m", Minutes / 60, Minutes % 60);
        }
    }

	/// <summary>
	/// Update the frequency shift for all build nodes to ensure that each node's frequency is at least that of its dependencies.
	/// </summary>
	/// <param name="Nodes">Sequence of nodes to compute frequencies for</param>
	static void ComputeDependentFrequencies(IEnumerable<BuildNode> Nodes)
	{
		foreach(BuildNode Node in Nodes)
		{
			foreach(BuildNode OrderDependency in Node.OrderDependencies)
			{
				Node.FrequencyShift = Math.Max(Node.FrequencyShift, OrderDependency.FrequencyShift);
			}
		}
	}

	/// <summary>
	/// Check whether any nodes reference triggers through input dependencies. We're deprecating the treatment of trigger nodes as
	/// build nodes, because they don't do anything useful when they run. The new EC framework bypasses the execution of trigger nodes
	/// entirely, and just sends notifications once all the dependencies are met.
	/// </summary>
	/// <param name="Nodes">List of nodes to check trigger dependencies for</param>
	static void ValidateTriggerDependencies(IReadOnlyList<BuildNode> Nodes)
	{
		foreach(BuildNode NodeToDo in Nodes)
		{
			foreach(TriggerNode TriggerNodeToDo in NodeToDo.InputDependencies.OfType<TriggerNode>())
			{
				LogWarning("'{0}' has an input dependency on '{1}'. Treating trigger nodes as build nodes is deprecated.", NodeToDo.Name, TriggerNodeToDo.Name);
			}
		}
		foreach(TriggerNode TriggerNodeToDo in Nodes.OfType<TriggerNode>())
		{
			foreach(BuildNode NodeToDo in TriggerNodeToDo.InputDependencies)
			{
				LogWarning("'{0}' has an input dependency on '{1}'. Treating trigger nodes as build nodes is deprecated.", TriggerNodeToDo.Name, NodeToDo.Name);
			}
		}
	}

    static void FindCompletionState(IEnumerable<BuildNode> NodesToDo, string TempStorageDir)
	{
		foreach(BuildNode NodeToDo in NodesToDo)
		{
            // Construct the full temp storage node info. Check both the block name and the succeeded record; we may have skipped copying the build products themselves because they weren't input dependencies.
			NodeToDo.IsComplete = TempStorage.TempStorageExists(NodeToDo.Name, TempStorageDir, bQuiet: true) || TempStorage.TempStorageExists(NodeToDo.Name + SucceededTempStorageSuffix, TempStorageDir, bQuiet: true);

            LogVerbose("** {0}", NodeToDo.Name);
			if (!NodeToDo.IsComplete)
			{
                LogVerbose("***** GUBP Trigger Node was already triggered {0}", NodeToDo.Name);
			}
			else
			{
                LogVerbose("***** GUBP Trigger Node was NOT yet triggered {0}", NodeToDo.Name);
			}
		}
    }

	/// <summary>
	/// Finds all the source code changes between the given range.
	/// </summary>
	/// <param name="MinimumCL">The minimum (inclusive) changelist to include</param>
	/// <param name="MaximumCL">The maximum (inclusive) changelist to include</param>
	/// <returns>Lists of changelist records in the given range</returns>
    static List<P4Connection.ChangeRecord> GetSourceChangeRecords(int MinimumCL, int MaximumCL)
    {
        if (MinimumCL < 1990000)
        {
            throw new AutomationException("That CL looks pretty far off {0}", MinimumCL);
		}

		// Query the changes from Perforce
		StringBuilder FileSpec = new StringBuilder();
		FileSpec.AppendFormat("{0}@{1},{2} ", CombinePaths(PathSeparator.Slash, P4Env.BuildRootP4, "...", "Source", "..."), MinimumCL, MaximumCL);
		FileSpec.AppendFormat("{0}@{1},{2} ", CombinePaths(PathSeparator.Slash, P4Env.BuildRootP4, "...", "Build", "..."), MinimumCL, MaximumCL);

        List<P4Connection.ChangeRecord> ChangeRecords;
		if (!P4.Changes(out ChangeRecords, FileSpec.ToString(), false, true, LongComment: true))
        {
            throw new AutomationException("Could not get changes; cmdline: p4 changes {0}", FileSpec.ToString());
		}

		// Filter out all the changes by the buildmachine user; the CIS counter or promoted builds aren't of interest to us.
		ChangeRecords.RemoveAll(x => x.User.Equals("buildmachine", StringComparison.InvariantCultureIgnoreCase));
		return ChangeRecords;
    }

	/// <summary>
	/// Print a list of source changes, along with the success state for other builds of this node.
	/// </summary>
	/// <param name="History">History for this node</param>
    static void PrintDetailedChanges(NodeHistory History, int CurrentCL)
    {
		Log("**** Changes since last succeeded *****************************");
		Log("");

		// Find all the changelists that we're interested in
		SortedSet<int> BuildChanges = new SortedSet<int>();
		BuildChanges.UnionWith(History.AllStarted.Where(x => x >= History.LastSucceeded));
		BuildChanges.Add(CurrentCL);

		// Find all the changelists that we're interested in
        List<P4Connection.ChangeRecord> ChangeRecords = GetSourceChangeRecords(BuildChanges.First(), BuildChanges.Last());
		ChangeRecords.Reverse();

		// Print all the changes in the set
		int[] BuildChangesArray = BuildChanges.ToArray();
		for(int Idx = BuildChangesArray.Length - 1; Idx >= 0; Idx--)
		{
			int BuildCL = BuildChangesArray[Idx];

			// Show the status of this build
			string BuildStatus;
			if(BuildCL == CurrentCL)
			{
				BuildStatus = "THIS CHANGE";
			}
            else if (History.AllFailed.Contains(BuildCL))
            {
				BuildStatus = "FAILED";
            }
            else if (History.AllSucceeded.Contains(BuildCL))
            {
				BuildStatus = "SUCCEEDED";
            }
			else
			{
				BuildStatus = "still running";
			}
			Log(" {0} {1} {2}", (BuildCL == CurrentCL)? ">>>>" : "    ", BuildCL, BuildStatus.ToString());

			// Show all the changes in this range
			if(Idx > 0)
			{
				int PrevCL = BuildChangesArray[Idx - 1];
				foreach(P4Connection.ChangeRecord Record in ChangeRecords.Where(x => x.CL > PrevCL && x.CL <= BuildCL))
				{
					string[] Lines = Record.Summary.Split(new char[]{ '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
					Log("             {0} {1} {2}", Record.CL, Record.UserEmail, (Lines.Length > 0)? Lines[0] : "");
				}
			}
		}

		Log("");
		Log("***************************************************************");
    }

    public int GetLastSucceededCL(string NodeName)
    {
        if (P4Enabled)
        {
            bool bNewEC = ParseParam("NewEC");
            bool bFakeEC = ParseParam("FakeEC");

            bool bSaveSharedTempStorage = P4Enabled && (IsBuildMachine || GlobalCommandLine.UseLocalBuildStorage);

            // Get the branch hacker options.
            string BranchName = P4Enabled ? P4Env.BuildRootP4 : ParseParamValue("BranchName", "");
            GUBPBranchHacker.BranchOptions BranchOptions = GetBranchOptions(BranchName);

            string BranchNameForTempStorage = P4Enabled ? P4Env.BuildRootEscaped : "NoP4";
            string RootNameForTempStorage = BranchOptions.RootNameForTempStorage ?? "UE4";
            string PreflightSuffix = GetPreflightSuffix();
            int ChangeNumber = P4Enabled ? ParseParamInt("CL", P4Env.Changelist) : 0;
            string BuildFolder = string.Format("{0}{1}", ChangeNumber, PreflightSuffix);

            string TempStorageDir;
            if (bNewEC)
            {
                TempStorageDir = ParseParamValue("TempStorageDir");
            }
            else
            {
                TempStorageDir = GetTempStorageDir(bSaveSharedTempStorage, bFakeEC, BranchNameForTempStorage, RootNameForTempStorage, BuildFolder);
            }

            NodeHistory History = FindNodeHistory(NodeName, P4Env.Changelist, TempStorageDir);
            if (History != null)
            {
                return History.LastSucceeded;
            }
        }

        return -1;
    }

    void PrintNodes(GUBP bp, List<BuildNode> Nodes, IEnumerable<AggregateNode> Aggregates, List<TriggerNode> UnfinishedTriggers, int TimeQuantum)
    {
		AggregateNode[] MatchingAggregates = Aggregates.Where(x => x.Dependencies.All(y => Nodes.Contains(y))).ToArray();
		if (MatchingAggregates.Length > 0)
		{
			Log("*********** Aggregates");
			foreach (AggregateNode Aggregate in MatchingAggregates.OrderBy(x => x.Name))
			{
				Log("    " + Aggregate.Name);
			}
		}

        bool bShowDependencies = bp.ParseParam("ShowDependencies");
        bool AddEmailProps = bp.ParseParam("ShowEmails");
        string LastControllingTrigger = "";
        string LastAgentGroup = "";

		Log("*********** Desired And Dependent Nodes, in order.");
        foreach (BuildNode NodeToDo in Nodes)
        {
            string MyControllingTrigger = NodeToDo.ControllingTriggerDotName;
            if (MyControllingTrigger != LastControllingTrigger)
            {
                LastControllingTrigger = MyControllingTrigger;
                if (MyControllingTrigger != "")
                {
                    string Finished = "";
                    if (UnfinishedTriggers != null)
                    {
                        if (NodeToDo.ControllingTriggers.Length > 0 && UnfinishedTriggers.Contains(NodeToDo.ControllingTriggers.Last()))
                        {
                            Finished = "(not yet triggered)";
                        }
                        else
                        {
                            Finished = "(already triggered)";
                        }
                    }
					Log("  Controlling Trigger: {0}    {1}", MyControllingTrigger, Finished);
                }
            }

			if (NodeToDo.AgentSharingGroup != LastAgentGroup && NodeToDo.AgentSharingGroup != "")
            {
				Log("    Agent Group: {0}", NodeToDo.AgentSharingGroup);
            }
            LastAgentGroup = NodeToDo.AgentSharingGroup;

			StringBuilder Builder = new StringBuilder("    ");
			if(LastAgentGroup != "")
			{
				Builder.Append("    ");
			}
			Builder.AppendFormat("{0} ({1})", NodeToDo.Name, (NodeToDo.FrequencyShift >= BuildNode.ExplicitFrequencyShift)? "explicit" : GetTimeIntervalString(TimeQuantum << NodeToDo.FrequencyShift));
			if(NodeToDo.IsComplete)
			{
				Builder.Append(" - (Completed)");
			}
			if(NodeToDo is TriggerNode)
			{
				Builder.Append(" - (TriggerNode)");
			}
			if(NodeToDo.IsSticky)
			{
				Builder.Append(" - (Sticky)");
			}
			if(!NodeToDo.CopyToSharedStorage)
			{
				Builder.Append(" - (Not Archived)");
			}

            string Agent = NodeToDo.AgentRequirements;
			if(ParseParamValue("AgentOverride") != "" && !NodeToDo.Name.Contains("Mac"))
			{
				Agent = ParseParamValue("AgentOverride");
			}
            if (!String.IsNullOrEmpty(Agent))
            {
				Builder.AppendFormat(" [{0}]", Agent);
            }
			if(NodeToDo.AgentMemoryRequirement != 0)
			{
				Builder.AppendFormat(" [{0}gb]", NodeToDo.AgentMemoryRequirement);
			}
            if (AddEmailProps)
            {
				Builder.AppendFormat(" {0}", String.Join(" ", NodeToDo.RecipientsForFailureEmails));
            }
			Log(Builder.ToString());

            if (bShowDependencies)
            {
				foreach (BuildNode Dep in NodeToDo.InputDependencies.OrderBy(x => x.Name))
                {
					Log("            dep> {0}", Dep.Name);
                }
				foreach (BuildNode Dep in NodeToDo.OrderDependencies.OrderBy(x => x.Name))
				{
					if (!NodeToDo.InputDependencies.Contains(Dep))
					{
						Log("           pdep> {0}", Dep.Name);
					}
				}
            }
        }
    }

    int CountZeros(int Num)
    {
        if (Num < 0)
        {
            throw new AutomationException("Bad CountZeros");
        }
        if (Num == 0)
        {
            return 31;
        }
        int Result = 0;
        while ((Num & 1) == 0)
        {
            Result++;
            Num >>= 1;
        }
        return Result;
    }

    static List<BuildNode> TopologicalSort(HashSet<BuildNode> NodesToDo, BuildNode ExplicitTrigger, bool SubSort, bool DoNotConsiderCompletion)
    {
        List<BuildNode> OrderedToDo = new List<BuildNode>();

        Dictionary<string, List<BuildNode>> SortedAgentGroupChains = new Dictionary<string, List<BuildNode>>();
        if (!SubSort)
        {
            Dictionary<string, List<BuildNode>> AgentGroupChains = new Dictionary<string, List<BuildNode>>();
            foreach (BuildNode NodeToDo in NodesToDo)
            {
                string MyAgentGroup = NodeToDo.AgentSharingGroup;
                if (MyAgentGroup != "")
                {
                    if (!AgentGroupChains.ContainsKey(MyAgentGroup))
                    {
                        AgentGroupChains.Add(MyAgentGroup, new List<BuildNode> { NodeToDo });
                    }
                    else
                    {
                        AgentGroupChains[MyAgentGroup].Add(NodeToDo);
                    }
                }
            }
            foreach (KeyValuePair<string, List<BuildNode>> Chain in AgentGroupChains)
            {
                SortedAgentGroupChains.Add(Chain.Key, TopologicalSort(new HashSet<BuildNode>(Chain.Value), ExplicitTrigger, true, DoNotConsiderCompletion));
            }
			foreach(KeyValuePair<string, List<BuildNode>> Chain in SortedAgentGroupChains)
			{
				string[] ControllingTriggers = Chain.Value.Select(x => x.ControllingTriggerDotName).Distinct().OrderBy(x => x).ToArray();
				if(ControllingTriggers.Length > 1)
				{
					string Triggers = String.Join(", ", ControllingTriggers.Select(x => String.Format("'{0}' ({1})", x, String.Join("+", Chain.Value.Where(y => y.ControllingTriggerDotName == x).Select(y => y.Name)))));
					throw new AutomationException("Agent sharing group '{0}' has multiple controlling triggers: {1}", Chain.Key, Triggers);
				}
			}
        }

        // here we do a topological sort of the nodes, subject to a lexographical and priority sort
        while (NodesToDo.Count > 0)
        {
            bool bProgressMade = false;
            float BestPriority = -1E20f;
            BuildNode BestNode = null;
            bool BestPseudoReady = false;
            HashSet<string> NonReadyAgentGroups = new HashSet<string>();
            HashSet<string> NonPeudoReadyAgentGroups = new HashSet<string>();
            HashSet<string> ExaminedAgentGroups = new HashSet<string>();
            foreach (BuildNode NodeToDo in NodesToDo)
            {
                bool bReady = true;
				bool bPseudoReady = true;
				bool bCompReady = true;
                if (!SubSort && NodeToDo.AgentSharingGroup != "")
                {
                    if (ExaminedAgentGroups.Contains(NodeToDo.AgentSharingGroup))
                    {
                        bReady = !NonReadyAgentGroups.Contains(NodeToDo.AgentSharingGroup);
                        bPseudoReady = !NonPeudoReadyAgentGroups.Contains(NodeToDo.AgentSharingGroup); //this might not be accurate if bReady==false
                    }
                    else
                    {
                        ExaminedAgentGroups.Add(NodeToDo.AgentSharingGroup);
                        foreach (BuildNode ChainNode in SortedAgentGroupChains[NodeToDo.AgentSharingGroup])
                        {
                            foreach (BuildNode Dep in ChainNode.InputDependencies)
                            {
                                if (!SortedAgentGroupChains[NodeToDo.AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    bReady = false;
                                    break;
                                }
                            }
                            if (!bReady)
                            {
                                NonReadyAgentGroups.Add(NodeToDo.AgentSharingGroup);
                                break;
                            }
                            foreach (BuildNode Dep in ChainNode.OrderDependencies)
                            {
                                if (!SortedAgentGroupChains[NodeToDo.AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    bPseudoReady = false;
                                    NonPeudoReadyAgentGroups.Add(NodeToDo.AgentSharingGroup);
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    foreach (BuildNode Dep in NodeToDo.InputDependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            bReady = false;
                            break;
                        }
                    }
                    foreach (BuildNode Dep in NodeToDo.OrderDependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            bPseudoReady = false;
                            break;
                        }
                    }
                }
                float Priority = NodeToDo.Priority;

                if (bReady && BestNode != null)
                {
                    if (String.Compare(BestNode.ControllingTriggerDotName, NodeToDo.ControllingTriggerDotName) < 0) //sorted by controlling trigger
                    {
                        bReady = false;
                    }
                    else if (String.Compare(BestNode.ControllingTriggerDotName, NodeToDo.ControllingTriggerDotName) == 0) //sorted by controlling trigger
                    {
                        if (BestNode.IsSticky && !NodeToDo.IsSticky) //sticky nodes first
                        {
                            bReady = false;
                        }
                        else if (BestNode.IsSticky == NodeToDo.IsSticky)
                        {
                            if (BestPseudoReady && !bPseudoReady)
                            {
                                bReady = false;
                            }
                            else if (BestPseudoReady == bPseudoReady)
                            {
                                bool IamLateTrigger = !DoNotConsiderCompletion && (NodeToDo is TriggerNode) && NodeToDo != ExplicitTrigger && !NodeToDo.IsComplete;
                                bool BestIsLateTrigger = !DoNotConsiderCompletion && (BestNode is TriggerNode) && BestNode != ExplicitTrigger && !BestNode.IsComplete;
                                if (BestIsLateTrigger && !IamLateTrigger)
                                {
                                    bReady = false;
                                }
                                else if (BestIsLateTrigger == IamLateTrigger)
                                {

                                    if (Priority < BestPriority)
                                    {
                                        bReady = false;
                                    }
                                    else if (Priority == BestPriority)
                                    {
                                        if (BestNode.Name.CompareTo(NodeToDo.Name) < 0)
                                        {
                                            bReady = false;
                                        }
                                    }
									if (!bCompReady)
									{
										bReady = false;
									}
                                }
                            }
                        }
                    }
                }
                if (bReady)
                {
                    BestPriority = Priority;
                    BestNode = NodeToDo;
                    BestPseudoReady = bPseudoReady;
                    bProgressMade = true;
                }
            }
            if (bProgressMade)
            {
                if (!SubSort && BestNode.AgentSharingGroup != "")
                {
                    foreach (BuildNode ChainNode in SortedAgentGroupChains[BestNode.AgentSharingGroup])
                    {
                        OrderedToDo.Add(ChainNode);
                        NodesToDo.Remove(ChainNode);
                    }
                }
                else
                {
                    OrderedToDo.Add(BestNode);
                    NodesToDo.Remove(BestNode);
                }
            }

            if (!bProgressMade && NodesToDo.Count > 0)
            {
				Log("Cycle in GUBP, could not resolve:");
                foreach (BuildNode NodeToDo in NodesToDo)
                {
                    string Deps = "";
                    if (!SubSort && NodeToDo.AgentSharingGroup != "")
                    {
                        foreach (BuildNode ChainNode in SortedAgentGroupChains[NodeToDo.AgentSharingGroup])
                        {
                            foreach (BuildNode Dep in ChainNode.InputDependencies)
                            {
                                if (!SortedAgentGroupChains[NodeToDo.AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    Deps = Deps + Dep.Name + "[" + ChainNode.Name + "->" + NodeToDo.AgentSharingGroup + "]" + " ";
                                }
                            }
                        }
                    }
                    foreach (BuildNode Dep in NodeToDo.InputDependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            Deps = Deps + Dep.Name + " ";
                        }
                    }
                    foreach (BuildNode Dep in NodeToDo.OrderDependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            Deps = Deps + Dep.Name + " ";
                        }
                    }
					Log("  {0}    deps: {1}", NodeToDo.Name, Deps);
                }
                throw new AutomationException("Cycle in GUBP");
            }
        }

        return OrderedToDo;
    }

    static NodeHistory FindNodeHistory(string NodeName, int CurrentChangeNumber, string TempStorageDir)
    {
		NodeHistory History = null;
        if (CurrentChangeNumber > 0)
        {
			History = new NodeHistory();

			// Assume that the shared storage directory is a pattern containing the changelist for the current build, and look for directories with a similar pattern.
			string SharedStorageLastName = Path.GetFileName(TempStorageDir);
			string ChangeString = CurrentChangeNumber.ToString();
			int ChangeIndex = SharedStorageLastName.LastIndexOf(ChangeString);
			if(ChangeIndex != -1)
			{
				string SharedStoragePattern = SharedStorageLastName.Substring(0, ChangeIndex) + "*" + SharedStorageLastName.Substring(ChangeIndex + ChangeString.Length);
				foreach(DirectoryInfo Directory in new DirectoryInfo(TempStorageDir).Parent.EnumerateDirectories(SharedStoragePattern))
				{
					int OtherCL;
					if(int.TryParse(Directory.Name.Substring(ChangeIndex, Directory.Name.Length - (SharedStorageLastName.Length - ChangeIndex - ChangeString.Length)), out OtherCL))
					{
						if(TempStorage.SharedTempStorageManifestExists(Directory.FullName, NodeName + StartedTempStorageSuffix, true))
						{
							History.AllStarted.Add(OtherCL);
						}
						if(TempStorage.SharedTempStorageManifestExists(Directory.FullName, NodeName + SucceededTempStorageSuffix, true))
						{
							History.AllSucceeded.Add(OtherCL);
						}
						if(TempStorage.SharedTempStorageManifestExists(Directory.FullName, NodeName + FailedTempStorageSuffix, true))
						{
							History.AllFailed.Add(OtherCL);
						}
					}
				}
			}
			History.AllStarted.Sort();
			History.AllSucceeded.Sort();
			History.AllFailed.Sort();

			// Filter everything relative to the current CL
            if (History.AllFailed.Count > 0)
            {
                History.LastFailed = History.AllFailed.LastOrDefault(x => x < CurrentChangeNumber);
            }
            if (History.AllSucceeded.Count > 0)
            {
                History.LastSucceeded = History.AllSucceeded.LastOrDefault(x => x < CurrentChangeNumber);

                foreach (int Failed in History.AllFailed)
                {
                    if (Failed > History.LastSucceeded)
                    {
                        History.Failed.Add(Failed);
                        History.FailedString = GUBPNode.MergeSpaceStrings(History.FailedString, String.Format("{0}", Failed));
                    }
                }
                foreach (int Started in History.AllStarted)
                {
                    if (Started > History.LastSucceeded && !History.Failed.Contains(Started))
                    {
                        History.InProgress.Add(Started);
                        History.InProgressString = GUBPNode.MergeSpaceStrings(History.InProgressString, String.Format("{0}", Started));
                    }
                }
			}
        }
		return History;
    }
    void GetFailureEmails(ElectricCommander EC, BuildNode NodeToDo, NodeHistory History, string TempStorageDir, bool OnlyLateUpdates = false)
	{
        string FailCauserEMails = "";
        string EMailNote = "";
        bool SendSuccessForGreenAfterRed = false;
        int NumPeople = 0;
        if (History != null)
        {
            if (History.LastSucceeded > 0 && History.LastSucceeded < P4Env.Changelist)
            {
                int LastNonDuplicateFail = P4Env.Changelist;
                try
                {
                    if (OnlyLateUpdates)
                    {
                        LastNonDuplicateFail = FindLastNonDuplicateFail(NodeToDo, History, TempStorageDir);
                        if (LastNonDuplicateFail < P4Env.Changelist)
                        {
							Log("*** Red-after-red spam reduction, changed CL {0} to CL {1} because the errors didn't change.", P4Env.Changelist, LastNonDuplicateFail);
                        }
                    }
                }
                catch (Exception Ex)
                {
                    LastNonDuplicateFail = P4Env.Changelist;
                    LogWarning("Failed to FindLastNonDuplicateFail.");
                    LogWarning(LogUtils.FormatException(Ex));
                }

				if(LastNonDuplicateFail > History.LastSucceeded)
				{
					List<P4Connection.ChangeRecord> ChangeRecords = GetSourceChangeRecords(History.LastSucceeded + 1, LastNonDuplicateFail);
					foreach (P4Connection.ChangeRecord Record in ChangeRecords)
					{
						FailCauserEMails = GUBPNode.MergeSpaceStrings(FailCauserEMails, Record.UserEmail);
					}
				}

                if (!String.IsNullOrEmpty(FailCauserEMails))
                {
                    NumPeople++;
                    foreach (char AChar in FailCauserEMails.ToCharArray())
                    {
                        if (AChar == ' ')
                        {
                            NumPeople++;
                        }
                    }
                    if (NumPeople > 50)
                    {
                        EMailNote = String.Format("This step has been broken for more than 50 changes. It last succeeded at CL {0}. ", History.LastSucceeded);
                    }
                }
            }
            else if (History.LastSucceeded <= 0)
            {
                EMailNote = String.Format("This step has been broken for more than a few days, so there is no record of it ever succeeding. ");
            }
            if (EMailNote != "" && !String.IsNullOrEmpty(History.FailedString))
            {
                EMailNote += String.Format("It has failed at CLs {0}. ", History.FailedString);
            }
            if (EMailNote != "" && !String.IsNullOrEmpty(History.InProgressString))
            {
                EMailNote += String.Format("These CLs are being built right now {0}. ", History.InProgressString);
            }
            if (History.LastSucceeded > 0 && History.LastSucceeded < P4Env.Changelist && History.LastFailed > History.LastSucceeded && History.LastFailed < P4Env.Changelist)
            {
                SendSuccessForGreenAfterRed = ParseParam("CIS");
            }
		}

		if (History == null)
		{
			EC.UpdateEmailProperties(NodeToDo, 0, "", FailCauserEMails, EMailNote, SendSuccessForGreenAfterRed);
		}
		else
		{
			EC.UpdateEmailProperties(NodeToDo, History.LastSucceeded, History.FailedString, FailCauserEMails, EMailNote, SendSuccessForGreenAfterRed);
		}
	}

    bool HashSetEqual(HashSet<string> A, HashSet<string> B)
    {
        if (A.Count != B.Count)
        {
            return false;
        }
        foreach (string Elem in A)
        {
            if (!B.Contains(Elem))
            {
                return false;
            }
        }
        foreach (string Elem in B)
        {
            if (!A.Contains(Elem))
            {
                return false;
            }
        }
        return true;
    }

    int FindLastNonDuplicateFail(BuildNode NodeToDo, NodeHistory History, string TempStorageDir)
    {
        int Result = P4Env.Changelist;

        var TempStorageNodeInfo = NodeToDo.Name + FailedTempStorageSuffix;

        List<int> BackwardsFails = new List<int>(History.AllFailed);
        BackwardsFails.Add(P4Env.Changelist);
        BackwardsFails.Sort();
        BackwardsFails.Reverse();
        HashSet<string> CurrentErrors = null;
        foreach (int CL in BackwardsFails)
        {
            if (CL > P4Env.Changelist)
            {
                continue;
            }
            if (CL <= History.LastSucceeded)
            {
                break;
            }
            // Find any local temp storage manifest for this changelist and delete it.
            var ThisTempStorageNodeInfo = TempStorageNodeInfo;
            TempStorage.DeleteLocalTempStorageManifest(ThisTempStorageNodeInfo, true); // these all clash locally, which is fine we just retrieve them from shared

            List<string> Files = null;
            try
            {
                Files = TempStorage.RetrieveFromTempStorage(TempStorageDir, ThisTempStorageNodeInfo); // this will fail on our CL if we didn't fail or we are just setting up the branch
            }
            catch (Exception)
            {
            }
            if (Files == null)
            {
                continue;
            }
            if (Files.Count != 1)
            {
                throw new AutomationException("Unexpected number of files for fail record {0}", Files.Count);
            }
            string ErrorFile = Files[0];
            HashSet<string> ThisErrors = ECJobPropsUtils.ErrorsFromProps(ErrorFile);
            if (CurrentErrors == null)
            {
                CurrentErrors = ThisErrors;
            }
            else
            {
                if (CurrentErrors.Count == 0 || !HashSetEqual(CurrentErrors, ThisErrors))
                {
                    break;
                }
                Result = CL;
            }
        }
        return Result;
    }

	/// <summary>
	/// Determine which nodes to build from the command line, or return everything if there's nothing specified explicitly.
	/// </summary>
	/// <param name="PotentialNodes">The nodes in the graph</param>
	/// <param name="PotentialAggregates">The aggregates in the graph</param>
	/// <returns>Set of all nodes to execute, recursively including their dependencies.</returns>
	private HashSet<BuildNode> ParseNodesToDo(IEnumerable<BuildNode> PotentialNodes, IEnumerable<AggregateNode> PotentialAggregates)
	{
		// Parse all the node names that we want
		List<string> NamesToDo = new List<string>();

		string NodeParam = ParseParamValue("Node", null);
		if (NodeParam != null)
		{
			NamesToDo.AddRange(NodeParam.Split('+'));
		}

		string GameParam = ParseParamValue("Game", null);
		if (GameParam != null)
		{
			NamesToDo.AddRange(GameParam.Split('+').Select(x => FullGameAggregateNode.StaticGetFullName(x)));
		}

		// Resolve all those node names to a list of build nodes to execute
		HashSet<BuildNode> NodesToDo = new HashSet<BuildNode>();
		foreach (string NameToDo in NamesToDo)
		{
			int FoundNames = 0;
			foreach (BuildNode PotentialNode in PotentialNodes)
			{
				if (String.Compare(PotentialNode.Name, NameToDo, StringComparison.InvariantCultureIgnoreCase) == 0 || String.Compare(PotentialNode.AgentSharingGroup, NameToDo, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					NodesToDo.Add(PotentialNode);
					FoundNames++;
				}
			}
			foreach (AggregateNode PotentialAggregate in PotentialAggregates)
			{
				if (String.Compare(PotentialAggregate.Name, NameToDo, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					NodesToDo.UnionWith(PotentialAggregate.Dependencies);
					FoundNames++;
				}
			}
			if (FoundNames == 0)
			{
				throw new AutomationException("Could not find node named {0}", NameToDo);
			}
		}

		// Add everything if there was nothing specified explicitly
		if (NodesToDo.Count == 0)
		{
			NodesToDo.UnionWith(PotentialNodes);
		}

		// Recursively find the complete set of all nodes we want to execute
		HashSet<BuildNode> RecursiveNodesToDo = new HashSet<BuildNode>(NodesToDo);
		foreach(BuildNode NodeToDo in NodesToDo)
		{
			RecursiveNodesToDo.UnionWith(NodeToDo.InputDependencies);
			RecursiveNodesToDo.UnionWith(NodeToDo.ControllingTriggers);
		}

		return RecursiveNodesToDo;
	}

	/// <summary>
	/// Culls the list of nodes to do depending on the current time index.
	/// </summary>
	/// <param name="NodesToDo">The list of nodes to do</param>
	/// <param name="TimeIndex">The current time index. All nodes are run for TimeIndex=0, otherwise they are culled based on their FrequencyShift parameter.</param>
	private void CullNodesForTimeIndex(HashSet<BuildNode> NodesToDo, int TimeIndex)
	{
		if (TimeIndex >= 0)
		{
			List<BuildNode> NodesToCull = new List<BuildNode>();
			foreach (BuildNode NodeToDo in NodesToDo)
			{
				if (NodeToDo.FrequencyShift >= BuildNode.ExplicitFrequencyShift || (TimeIndex % (1 << NodeToDo.FrequencyShift)) != 0)
				{
					NodesToCull.Add(NodeToDo);
				}
			}
			NodesToDo.ExceptWith(NodesToCull);
		}
	}

	/// <summary>
	/// Culls everything downstream of a trigger if we're running a preflight build.
	/// </summary>
	/// <param name="NodesToDo">The current list of nodes to do</param>
	private void CullNodesForPreflight(HashSet<BuildNode> NodesToDo)
	{
		HashSet<BuildNode> NodesToCull = new HashSet<BuildNode>();
		foreach(BuildNode NodeToDo in NodesToDo)
		{
			if((NodeToDo is TriggerNode) || NodeToDo.ControllingTriggers.Length > 0)
			{
				LogVerbose(" Culling {0} due to being downstream of trigger in preflight", NodeToDo.Name);
				NodesToCull.Add(NodeToDo);
			}
		}
		NodesToDo.ExceptWith(NodesToCull);
	}

	private int UpdateCISCounter(int TimeQuantum)
	{
		if (!P4Enabled)
		{
			throw new AutomationException("Can't have -CIS without P4 support");
		}
		string P4IndexFileP4 = CombinePaths(PathSeparator.Slash, CommandUtils.P4Env.BuildRootP4, "Engine", "Build", "CISCounter.txt");
		string P4IndexFileLocal = CombinePaths(CmdEnv.LocalRoot, "Engine", "Build", "CISCounter.txt");
		for(int Retry = 0; Retry < 20; Retry++)
		{
			int NowMinutes = (int)((DateTime.UtcNow - new DateTime(2014, 1, 1)).TotalMinutes);
			if (NowMinutes < 3 * 30 * 24)
			{
				throw new AutomationException("bad date calc");
			}
			if (!FileExists_NoExceptions(P4IndexFileLocal))
			{
				LogVerbose("{0} doesn't exist, checking in a new one", P4IndexFileP4);
				WriteAllText(P4IndexFileLocal, "-1 0");
				int WorkingCL = -1;
				try
				{
					WorkingCL = P4.CreateChange(P4Env.Client, "Adding new CIS Counter");
					P4.Add(WorkingCL, P4IndexFileP4);
					int SubmittedCL;
					P4.Submit(WorkingCL, out SubmittedCL);
				}
				catch (Exception)
				{
					LogWarning("Add of CIS counter failed, assuming it now exists.");
					if (WorkingCL > 0)
					{
						P4.DeleteChange(WorkingCL);
					}
				}
			}
			P4.Sync("-f " + P4IndexFileP4 + "#head");
			if (!FileExists_NoExceptions(P4IndexFileLocal))
			{
				LogVerbose("{0} doesn't exist, checking in a new one", P4IndexFileP4);
				WriteAllText(P4IndexFileLocal, "-1 0");
				int WorkingCL = -1;
				try
				{
					WorkingCL = P4.CreateChange(P4Env.Client, "Adding new CIS Counter");
					P4.Add(WorkingCL, P4IndexFileP4);
					int SubmittedCL;
					P4.Submit(WorkingCL, out SubmittedCL);
				}
				catch (Exception)
				{
					LogWarning("Add of CIS counter failed, assuming it now exists.");
					if (WorkingCL > 0)
					{
						P4.DeleteChange(WorkingCL);
					}
				}
			}
			string Data = ReadAllText(P4IndexFileLocal);
			string[] Parts = Data.Split(" ".ToCharArray());
			int Index = int.Parse(Parts[0]);
			int Minutes = int.Parse(Parts[1]);

			int DeltaMinutes = NowMinutes - Minutes;

			int NewIndex = Index + 1;

			if (DeltaMinutes > TimeQuantum * 2)
			{
				if (DeltaMinutes > TimeQuantum * (1 << 8))
				{
					// it has been forever, lets just start over
					NewIndex = 0;
				}
				else
				{
					int WorkingIndex = NewIndex + 1;
					for (int WorkingDelta = DeltaMinutes - TimeQuantum; WorkingDelta > 0; WorkingDelta -= TimeQuantum, WorkingIndex++)
					{
						if (CountZeros(NewIndex) < CountZeros(WorkingIndex))
						{
							NewIndex = WorkingIndex;
						}
					}
				}
			}
			if(ParseParam("ResetCounter"))
			{
				NewIndex = 0;
			}
			{
				string Line = String.Format("{0} {1}", NewIndex, NowMinutes);
				LogVerbose("Attempting to write {0} with {1}", P4IndexFileP4, Line);
				int WorkingCL = -1;
				try
				{
					WorkingCL = P4.CreateChange(P4Env.Client, "Updating CIS Counter");
					P4.Edit(WorkingCL, P4IndexFileP4);
					WriteAllText(P4IndexFileLocal, Line);
					int SubmittedCL;
					P4.Submit(WorkingCL, out SubmittedCL);
					return NewIndex;
				}
				catch (Exception)
				{
					LogWarning("Edit of CIS counter failed, assuming someone else checked in, retrying.");
					if (WorkingCL > 0)
					{
						P4.DeleteChange(WorkingCL);
					}
					System.Threading.Thread.Sleep(30000);
				}
			}
		}
		throw new AutomationException("Failed to update the CIS counter after 20 tries.");
	}

	private List<TriggerNode> FindUnfinishedTriggers(bool bSkipTriggers, BuildNode ExplicitTrigger, List<BuildNode> OrderedToDo)
	{
		// find all unfinished triggers, excepting the one we are triggering right now
		List<TriggerNode> UnfinishedTriggers = new List<TriggerNode>();
		if (!bSkipTriggers)
		{
			foreach (TriggerNode NodeToDo in OrderedToDo.OfType<TriggerNode>())
			{
				if (!NodeToDo.IsComplete && ExplicitTrigger != NodeToDo)
				{
					UnfinishedTriggers.Add(NodeToDo);
				}
			}
		}
		return UnfinishedTriggers;
	}

	/// <summary>
	/// Validates that the given nodes are sorted correctly, so that all dependencies are met first
	/// </summary>
	/// <param name="OrderedToDo">The sorted list of nodes</param>
	private void CheckSortOrder(List<BuildNode> OrderedToDo)
	{
		foreach (BuildNode NodeToDo in OrderedToDo)
		{
			if ((NodeToDo is TriggerNode) && (NodeToDo.IsSticky || NodeToDo.IsComplete)) // these sticky triggers are ok, everything is already completed anyway
			{
				continue;
			}

			int MyIndex = OrderedToDo.IndexOf(NodeToDo);
			foreach (BuildNode OrderDependency in NodeToDo.OrderDependencies)
			{
				int OrderDependencyIdx = OrderedToDo.IndexOf(OrderDependency);
				if(OrderDependencyIdx >= MyIndex)
				{
					throw new AutomationException("Topological sort error, node {0} has an order dependency on {1} which sorted after it.", NodeToDo.Name, OrderDependency.Name);
				}
			}
		}
	}

    private ExitCode DoCommanderSetup(ElectricCommander EC, IEnumerable<BuildNode> AllNodes, IEnumerable<AggregateNode> AllAggregates, List<BuildNode> OrderedToDo, int TimeIndex, int TimeQuantum, bool bSkipTriggers, bool bNewEC, bool bFake, bool bFakeEC, TriggerNode ExplicitTrigger, List<TriggerNode> UnfinishedTriggers, bool bPreflightBuild)
	{
		List<BuildNode> SortedNodes = TopologicalSort(new HashSet<BuildNode>(AllNodes), null, SubSort: false, DoNotConsiderCompletion: true);
		Log("******* {0} GUBP Nodes", SortedNodes.Count);

		List<BuildNode> FilteredOrderedToDo = new List<BuildNode>();
		using(TelemetryStopwatch StartFilterTimer = new TelemetryStopwatch("FilterNodes"))
		{
			// remove nodes that have unfinished triggers
			foreach (BuildNode NodeToDo in OrderedToDo)
			{
				if (NodeToDo.ControllingTriggers.Length == 0 || !UnfinishedTriggers.Contains(NodeToDo.ControllingTriggers.Last()))
				{
					// if we are triggering, then remove nodes that are not controlled by the trigger or are dependencies of this trigger
					if (ExplicitTrigger != null && ExplicitTrigger != NodeToDo && !ExplicitTrigger.DependsOn(NodeToDo) && !NodeToDo.DependsOn(ExplicitTrigger))
					{
						continue; // this wasn't on the chain related to the trigger we are triggering, so it is not relevant
					}

					// in preflight builds, we are either skipping triggers (and running things downstream) or we just stop at triggers and don't make them available for triggering.
					if (bPreflightBuild && !bSkipTriggers && (NodeToDo is TriggerNode))
					{
						continue;
					}

					FilteredOrderedToDo.Add(NodeToDo);
				}
			}
		}
		using(TelemetryStopwatch PrintNodesTimer = new TelemetryStopwatch("SetupCommanderPrint"))
		{
			Log("*********** EC Nodes, in order.");
			PrintNodes(this, FilteredOrderedToDo, AllAggregates, UnfinishedTriggers, TimeQuantum);
		}

		if(bNewEC)
		{
			ElectricCommander.DoNewCommanderSetup(FilteredOrderedToDo, SortedNodes, ExplicitTrigger, bSkipTriggers);
		}
		else
		{
			EC.DoCommanderSetup(AllNodes, AllAggregates, FilteredOrderedToDo, SortedNodes, TimeIndex, TimeQuantum, bSkipTriggers, bFake, bFakeEC, ExplicitTrigger, UnfinishedTriggers);
		}
		return ExitCode.Success;
	}

    ExitCode ExecuteNodes(ElectricCommander EC, List<BuildNode> OrderedToDo, bool bNewEC, bool bFake, bool bFakeEC, string TempStorageDir, bool bSaveSharedTempStorage, int ChangeNumber)
	{
        Dictionary<string, BuildNode> BuildProductToNodeMap = new Dictionary<string, BuildNode>();
		foreach (BuildNode NodeToDo in OrderedToDo)
        {
            if (NodeToDo.BuildProducts != null)
            {
                throw new AutomationException("topological sort error");
            }

            if (NodeToDo.IsComplete)
            {
				// Just fetch the build products from temp storage
				Log("***** Retrieving GUBP Node {0}", NodeToDo.Name);
				NodeToDo.RetrieveBuildProducts(TempStorageDir);
            }
            else
            {
				// this is kinda complicated
				bool SaveSuccessRecords = (IsBuildMachine || bFakeEC || bNewEC) && // no real reason to make these locally except for fakeEC tests
					(!(NodeToDo is TriggerNode) || NodeToDo.IsSticky); // trigger nodes are run twice, one to start the new workflow and once when it is actually triggered, we will save reconds for the latter

                // Record that the node has started. We save our status to a new temp storage location specifically named with a suffix so we can find it later.
                if (SaveSuccessRecords) 
                {
                    EC.SaveStatus(TempStorageDir, NodeToDo.Name + StartedTempStorageSuffix, bSaveSharedTempStorage);
                }

				// Execute the node
				DateTime StartTime = DateTime.UtcNow;
				bool bResult = ExecuteNode(NodeToDo, bFake);
				TimeSpan ElapsedTime = DateTime.UtcNow - StartTime;

				// Archive the build products if the node succeeded
				if(bResult)
				{
					NodeToDo.ArchiveBuildProducts(TempStorageDir, (bSaveSharedTempStorage && !ParseParam("NoCopyToSharedStorage")));
				}

				// Record that the node has finished
				NodeHistory History = null;
                if (SaveSuccessRecords)
                {
					if(!bNewEC)
					{
						using(TelemetryStopwatch UpdateNodeHistoryStopwatch = new TelemetryStopwatch("UpdateNodeHistory"))
						{
							if(!(NodeToDo is TriggerNode))
							{
								History = FindNodeHistory(NodeToDo.Name, ChangeNumber, TempStorageDir);
							}
						}
					}
					using(TelemetryStopwatch SaveNodeStatusStopwatch = new TelemetryStopwatch("SaveNodeStatus"))
					{
                        EC.SaveStatus(TempStorageDir, NodeToDo.Name + (bResult? SucceededTempStorageSuffix : FailedTempStorageSuffix), bSaveSharedTempStorage, ParseParamValue("MyJobStepId"));
					}
					if(!bNewEC)
					{
						using(TelemetryStopwatch UpdateECPropsStopwatch = new TelemetryStopwatch("UpdateECProps"))
						{
							EC.UpdateECProps(NodeToDo);
						}
						if (IsBuildMachine)
						{
							using(TelemetryStopwatch GetFailEmailsStopwatch = new TelemetryStopwatch("GetFailEmails"))
							{
								GetFailureEmails(EC, NodeToDo, History, TempStorageDir);
							}
						}
						EC.UpdateECBuildTime(NodeToDo, ElapsedTime.TotalSeconds);
					}
                }

				// If it failed, print the diagnostic info and quit
				if(!bResult)
				{
                    if (History != null)
                    {
                        PrintDetailedChanges(History, P4Env.Changelist);
                    }

					WriteFailureLog(CombinePaths(CmdEnv.LogFolder, "LogTailsAndChanges.log"));
					return ExitCode.Error_Unknown;
                }
            }
            foreach (string Product in NodeToDo.BuildProducts)
            {
                if (BuildProductToNodeMap.ContainsKey(Product))
                {
                    throw new AutomationException("Overlapping build product: {0} and {1} both produce {2}", BuildProductToNodeMap[Product].Name, NodeToDo.Name, Product);
                }
                BuildProductToNodeMap.Add(Product, NodeToDo);
            }
        }
		return ExitCode.Success;
    }

	bool ExecuteNode(BuildNode NodeToDo, bool bFake)
	{
		try
		{
			if (bFake)
			{
				return NodeToDo.DoFakeBuild();
			}
			else
			{
				return NodeToDo.DoBuild();
			}
		}
		catch(Exception Ex)
		{
			Log("{0}", Ex.ToString());
			return false;
		}
	}

	static void WriteFailureLog(string FileName)
	{
		string FailInfo = "";
		FailInfo += "********************************* Main log file";
		FailInfo += Environment.NewLine + Environment.NewLine;
		FailInfo += LogUtils.GetLogTail();
		FailInfo += Environment.NewLine + Environment.NewLine + Environment.NewLine;

		string OtherLog = "See logfile for details: '";
		if (FailInfo.Contains(OtherLog))
		{
			string LogFile = FailInfo.Substring(FailInfo.IndexOf(OtherLog) + OtherLog.Length);
			if (LogFile.Contains("'"))
			{
				LogFile = CombinePaths(CmdEnv.LogFolder, LogFile.Substring(0, LogFile.IndexOf("'")));
				if (FileExists_NoExceptions(LogFile))
				{
					FailInfo += "********************************* Sub log file " + LogFile;
					FailInfo += Environment.NewLine + Environment.NewLine;

					FailInfo += LogUtils.GetLogTail(LogFile);
					FailInfo += Environment.NewLine + Environment.NewLine + Environment.NewLine;
				}
			}
		}

		WriteAllText(FileName, FailInfo);
	}
}
