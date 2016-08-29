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

partial class GUBP
{
	public class GUBPBranchConfig
	{
		public List<UnrealTargetPlatform> HostPlatforms;
        public string BranchName;
		public BranchInfo Branch;
		public GUBPBranchHacker.BranchOptions BranchOptions;
		public bool bForceIncrementalCompile;
        public JobInfo JobInfo;

		public Dictionary<string, GUBPNode> GUBPNodes = new Dictionary<string, GUBPNode>();
		public Dictionary<string, GUBPAggregateNode> GUBPAggregates = new Dictionary<string, GUBPAggregateNode>();

        public GUBPBranchConfig(IEnumerable<UnrealTargetPlatform> InHostPlatforms, BranchInfo InBranch, string InBranchName, GUBPBranchHacker.BranchOptions InBranchOptions, bool bInForceIncrementalCompile, JobInfo JobInfo)
		{
			HostPlatforms = new List<UnrealTargetPlatform>(InHostPlatforms);
			Branch = InBranch;
			BranchName = InBranchName;
			BranchOptions = InBranchOptions;
			bForceIncrementalCompile = bInForceIncrementalCompile;
			this.JobInfo = JobInfo;
		}

		public string AddNode(GUBPNode Node)
		{
			string Name = Node.GetFullName();
			if (GUBPNodes.ContainsKey(Name) || GUBPAggregates.ContainsKey(Name))
			{
				throw new AutomationException("Attempt to add a duplicate node {0}", Node.GetFullName());
			}
			GUBPNodes.Add(Name, Node);
			return Name;
		}

		public string AddNode(GUBPAggregateNode Node)
		{
			string Name = Node.GetFullName();
			if (GUBPNodes.ContainsKey(Name) || GUBPAggregates.ContainsKey(Name))
			{
				throw new AutomationException("Attempt to add a duplicate node {0}", Node.GetFullName());
			}
			GUBPAggregates.Add(Name, Node);
			return Name;
		}

		public bool HasNode(string Node)
		{
			return GUBPNodes.ContainsKey(Node) || GUBPAggregates.ContainsKey(Node);
		}

		public GUBPNode FindNode(string Node)
		{
			return GUBPNodes[Node];
		}

		public GUBPAggregateNode FindAggregateNode(string Node)
		{
			return GUBPAggregates[Node];
		}

		public GUBPNode TryFindNode(string Node)
		{
			GUBPNode Result;
			GUBPNodes.TryGetValue(Node, out Result);
			return Result;
		}

		public void RemovePseudodependencyFromNode(string Node, string Dep)
		{
			if (!GUBPNodes.ContainsKey(Node))
			{
				throw new AutomationException("Node {0} not found", Node);
			}
			GUBPNodes[Node].RemovePseudodependency(Dep);
		}
	}

    public abstract class GUBPNodeAdder
    {
        public abstract void AddNodes(GUBP bp, GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InActivePlatforms);
    }

	Type[] GetTypesFromAssembly(Assembly Asm)
	{
		Type[] AllTypesWeCanGet;
		try
		{
			AllTypesWeCanGet = Asm.GetTypes();
		}
		catch(ReflectionTypeLoadException Exc)
		{
			AllTypesWeCanGet = Exc.Types;
		}
		return AllTypesWeCanGet;
	}

    private void AddCustomNodes(GUBPBranchConfig BranchConfig, List<UnrealTargetPlatform> HostPlatforms, List<UnrealTargetPlatform> ActivePlatforms)
    {
		List<GUBPNodeAdder> Adders = new List<GUBPNodeAdder>();

		Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
		foreach (Assembly Dll in LoadedAssemblies)
		{
			Type[] AllTypes = GetTypesFromAssembly(Dll);
			foreach (Type PotentialConfigType in AllTypes)
			{
				if (PotentialConfigType != typeof(GUBPNodeAdder) && typeof(GUBPNodeAdder).IsAssignableFrom(PotentialConfigType))
				{
					GUBPNodeAdder Config = Activator.CreateInstance(PotentialConfigType) as GUBPNodeAdder;
					if (Config != null)
					{
						Adders.Add(Config);
					}
				}
			}
		}

        foreach (UnrealTargetPlatform HostPlatform in HostPlatforms)
        {
			foreach(GUBPNodeAdder Adder in Adders)
			{
				Adder.AddNodes(this, BranchConfig, HostPlatform, ActivePlatforms);
			}
        }
    }

    public abstract class GUBPBranchHacker
    {
        public class BranchOptions
        {            
            public List<UnrealTargetPlatform> PlatformsToRemove = new List<UnrealTargetPlatform>();
			public List<TargetRules.TargetType> MonolithicsToRemove = new List<TargetRules.TargetType>();
			public List<string> ExcludeNodes = new List<string>();
			public List<UnrealTargetPlatform> ExcludePlatformsForEditor = new List<UnrealTargetPlatform>();
            public List<UnrealTargetPlatform> RemovePlatformFromPromotable = new List<UnrealTargetPlatform>();
            public List<string> ProjectsToCook = new List<string>();
            public List<string> PromotablesWithoutTools = new List<string>();
            public List<string> NodesToRemovePseudoDependencies = new List<string>();
            public List<string> EnhanceAgentRequirements = new List<string>();
			/// <summary>
			/// Whether to put game builds behind triggers, so that engine branches don't always build them by default.
			/// </summary>
			public bool bGameBuildsBehindTriggers = false;
            /// <summary>
            /// This turns off ALL automated testing in ALL branches by default.
            /// A branch hacker must set this value to false explicitly to use automated testing.
            /// </summary>
			public bool bNoAutomatedTesting = true;
			public bool bNoDocumentation = false;
			public bool bNoInstalledEngine = false;
			public bool bMakeFormalBuildWithoutLabelPromotable = false;
			public bool bNoMonolithicDependenciesForCooks = false;
			public bool bNoEditorDependenciesForTools = false;
            /// <summary>
            /// cap the frequency of each of the member nodes to the specified frequency (given as a sbyte).
            /// </summary>
			public Dictionary<string, sbyte> FrequencyBarriers = new Dictionary<string,sbyte>();
            /// <summary>
            /// Allows overriding the base frequency at which GUBP runs CIS, in minutes.
            /// </summary>
			public int QuantumOverride = 0;
            /// <summary>
            /// Allows a branch to override the build share it uses for temp storage for all nodes. This will override the default value for <see cref="JobInfo.RootNameForTempStorage"/>.
            /// </summary>
            public string RootNameForTempStorage;
            /// <summary>
            /// Stores a mapping from a GameName (determined by the <see cref="BranchInfo.BranchUProject.GameName"/>) to a share name underneath <see cref="CommandUtils.RootBuildStorageDirectory"/>.
            /// This is where formal builds and such will be stored for each game. Instead of tying formal build outputs to the same location as temp storage,
            /// this allows a project to have it's own share for formal build products, but use, say, the UE4 default storage location for temp storage.
            /// If the mapping does not exist for a game, it uses the UE4 folder.
            /// </summary>
            public readonly Dictionary<string, string> GameNameToBuildShareMapping = new Dictionary<string,string>();
			/// <summary>
			/// Controls which branches should build the Engine localization.
			/// If you need localization for your own project, add it in your project specific automation scripts (see BuildLocalizationNode and BuildEngineLocalizationNode as an example).
			/// </summary>
			public bool bBuildEngineLocalization = false;
			public string EngineLocalizationBranchSuffix = "";
            public string AdditionalCookArgs = "";
			/// Whether to build all target platforms in parallel, or serialize them to avoid saturating the farm.
			/// </summary>
			public bool bTargetPlatformsInParallel = true;
        }
        public virtual void ModifyOptions(GUBP bp, ref BranchOptions Options, string Branch)
        {
        }
    }

    private static List<GUBPBranchHacker> BranchHackers;
    private GUBPBranchHacker.BranchOptions GetBranchOptions(string Branch)
    {
        if (BranchHackers == null)
        {
            BranchHackers = new List<GUBPBranchHacker>();
            Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
            foreach (Assembly Dll in LoadedAssemblies)
            {
				Type[] AllTypes = GetTypesFromAssembly(Dll);
                foreach (var PotentialConfigType in AllTypes)
                {
                    if (PotentialConfigType != typeof(GUBPBranchHacker) && typeof(GUBPBranchHacker).IsAssignableFrom(PotentialConfigType))
                    {
                        GUBPBranchHacker Config = Activator.CreateInstance(PotentialConfigType) as GUBPBranchHacker;
                        if (Config != null)
                        {
                            BranchHackers.Add(Config);
                        }
                    }
                }
            }
        }
        GUBPBranchHacker.BranchOptions Result = new GUBPBranchHacker.BranchOptions();
        foreach (GUBPBranchHacker Hacker in BranchHackers)
        {
            Hacker.ModifyOptions(this, ref Result, Branch);
        }
        return Result;
    }

    public abstract class GUBPEmailHacker
    {
		public virtual List<string> AddEmails(GUBP bp, string Branch, string NodeName, string EmailHint)
        {
            return new List<string>();
        }
        public virtual List<string> ModifyEmail(string Email, GUBP bp, string Branch, string NodeName)
        {
            return new List<string>{Email};
        }
		public virtual bool VetoEmailingCausers(GUBP bp, string Branch, string NodeName)
		{
			return false; // People who have submitted since last-green will be included unless vetoed by overriding this method. 
		}
    }

	private void FindEmailsForNodes(string BranchName, IEnumerable<BuildNodeDefinition> Nodes)
	{
		List<GUBPEmailHacker> EmailHackers = new List<GUBPEmailHacker>();

		Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
		foreach (var Dll in LoadedAssemblies)
		{
			Type[] AllTypes = GetTypesFromAssembly(Dll);
			foreach (var PotentialConfigType in AllTypes)
			{
				if (PotentialConfigType != typeof(GUBPEmailHacker) && typeof(GUBPEmailHacker).IsAssignableFrom(PotentialConfigType))
				{
					GUBPEmailHacker Config = Activator.CreateInstance(PotentialConfigType) as GUBPEmailHacker;
					if (Config != null)
					{
						EmailHackers.Add(Config);
					}
				}
			}
		}

		foreach (BuildNodeDefinition Node in Nodes)
		{
			Node.Notify = String.Join(";", HackEmails(EmailHackers, BranchName, Node, out Node.AddSubmittersToFailureEmails));
		}
	}

	private string[] HackEmails(List<GUBPEmailHacker> EmailHackers, string Branch, BuildNodeDefinition NodeInfo, out bool bIncludeCausers)
    {
		string OnlyEmail = ParseParamValue("OnlyEmail");
        if (!String.IsNullOrEmpty(OnlyEmail))
        {
			bIncludeCausers = false;
            return new string[]{ OnlyEmail };
        }

		string EmailOnly = ParseParamValue("EmailOnly");
		if (!String.IsNullOrEmpty(EmailOnly))
		{
			bIncludeCausers = false;
			return new string[] { EmailOnly };
		}

        string EmailHint = ParseParamValue("EmailHint");
        if(EmailHint == null)
        {
            EmailHint = "";
        }			

        List<string> Result = new List<string>();

        string AddEmails = ParseParamValue("AddEmails");
        if (!String.IsNullOrEmpty(AddEmails))
        {
			Result.AddRange(AddEmails.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries));
        }

        foreach (var EmailHacker in EmailHackers)
        {
            Result.AddRange(EmailHacker.AddEmails(this, Branch, NodeInfo.Name, EmailHint));
        }
        foreach (var EmailHacker in EmailHackers)
        {
            var NewResult = new List<string>();
            foreach (var Email in Result)
            {
                NewResult.AddRange(EmailHacker.ModifyEmail(Email, this, Branch, NodeInfo.Name));
            }
            Result = NewResult;
        }

		bIncludeCausers = !EmailHackers.Any(x => x.VetoEmailingCausers(this, Branch, NodeInfo.Name));
		return Result.ToArray();
    }
    public abstract class GUBPFrequencyHacker
    {
        public virtual int GetNodeFrequency(GUBP bp, string Branch, string NodeName, int BaseFrequency)
        {
            return new int();
        }
    }

	void FindFrequenciesForNodes(GUBP.GUBPBranchConfig BranchConfig, Dictionary<GUBP.GUBPNode, BuildNodeDefinition> LegacyToNewNodes, Dictionary<string, int> FrequencyOverrides)
	{
        List<GUBPFrequencyHacker> FrequencyHackers = new List<GUBPFrequencyHacker>();

        Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
        foreach (var Dll in LoadedAssemblies)
        {
			Type[] AllTypes = GetTypesFromAssembly(Dll);
            foreach (var PotentialConfigType in AllTypes)
            {
                if (PotentialConfigType != typeof(GUBPFrequencyHacker) && typeof(GUBPFrequencyHacker).IsAssignableFrom(PotentialConfigType))
                {
                    GUBPFrequencyHacker Config = Activator.CreateInstance(PotentialConfigType) as GUBPFrequencyHacker;
                    if (Config != null)
                    {
                        FrequencyHackers.Add(Config);
                    }
                }
            }
        }

		foreach (KeyValuePair<GUBP.GUBPNode, BuildNodeDefinition> LegacyToNewNodePair in LegacyToNewNodes)
		{
			BuildNodeDefinition Node = LegacyToNewNodePair.Value;
			Node.FrequencyShift = LegacyToNewNodePair.Key.CISFrequencyQuantumShift(BranchConfig);

			foreach(GUBPFrequencyHacker FrequencyHacker in FrequencyHackers)
			{
				Node.FrequencyShift = FrequencyHacker.GetNodeFrequency(this, BranchConfig.BranchName, Node.Name, Node.FrequencyShift);            
			}

			int FrequencyOverride;
			if (FrequencyOverrides.TryGetValue(Node.Name, out FrequencyOverride) && Node.FrequencyShift > FrequencyOverride)
			{
				Node.FrequencyShift = FrequencyOverride;
			}
		}
	}

    // when the host is win64, this is win32 because those are also "host platforms"
    static public UnrealTargetPlatform GetAltHostPlatform(UnrealTargetPlatform HostPlatform)
    {
        UnrealTargetPlatform AltHostPlatform = UnrealTargetPlatform.Unknown; // when the host is win64, this is win32 because those are also "host platforms"
        if (HostPlatform == UnrealTargetPlatform.Win64)
        {
            AltHostPlatform = UnrealTargetPlatform.Win32;
        }
        return AltHostPlatform;
    }

    void AddNodesForBranch(List<UnrealTargetPlatform> InitialHostPlatforms, string BranchName, JobInfo JobInfo, GUBPBranchHacker.BranchOptions BranchOptions, out List<BuildNodeDefinition> AllNodeDefinitions, out List<AggregateNodeDefinition> AllAggregateDefinitions, ref int TimeQuantum, bool bNewEC)
	{
		DateTime StartTime = DateTime.UtcNow;

		if (BranchOptions.QuantumOverride != 0)
		{
			TimeQuantum = BranchOptions.QuantumOverride;
		}

		bool bForceIncrementalCompile = ParseParam("ForceIncrementalCompile");
		bool bNoAutomatedTesting = ParseParam("NoAutomatedTesting") || BranchOptions.bNoAutomatedTesting;		

		List<UnrealTargetPlatform> HostPlatforms = new List<UnrealTargetPlatform>(InitialHostPlatforms);
		foreach(UnrealTargetPlatform PlatformToRemove in BranchOptions.PlatformsToRemove)
		{
			HostPlatforms.Remove(PlatformToRemove);
		}

        BranchInfo Branch = new BranchInfo(HostPlatforms);        

		List<UnrealTargetPlatform> ActivePlatforms;
        if (IsBuildMachine || ParseParam("AllPlatforms"))
        {
            ActivePlatforms = new List<UnrealTargetPlatform>();

            Branch.CodeProjects.RemoveAll(Project => BranchOptions.ExcludeNodes.Contains(Project.GameName));

            foreach (var GameProj in new[] { Branch.BaseEngineProject }.Concat(Branch.CodeProjects))
            {
                foreach (var Kind in BranchInfo.MonolithicKinds)
                {
                    if (GameProj.Properties.Targets.ContainsKey(Kind))
                    {
                        var Target = GameProj.Properties.Targets[Kind];
                        foreach (var HostPlatform in HostPlatforms)
                        {
                            var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
							var AdditionalPlatforms = Target.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform);
							var AllPlatforms = Platforms.Union(AdditionalPlatforms);
							foreach (var Plat in AllPlatforms)
                            {
                                if (Target.Rules.SupportsPlatform(Plat) && !ActivePlatforms.Contains(Plat))
                                {
                                    ActivePlatforms.Add(Plat);
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            ActivePlatforms     = new List<UnrealTargetPlatform>(CommandUtils.KnownTargetPlatforms);
        }
        var SupportedPlatforms = new List<UnrealTargetPlatform>();
        foreach(var Plat in ActivePlatforms)
        {
            if(!BranchOptions.PlatformsToRemove.Contains(Plat))
            {
                SupportedPlatforms.Add(Plat);
            }
        }
        ActivePlatforms = SupportedPlatforms;
        foreach (var Plat in ActivePlatforms)
        {
            LogVerbose("Active Platform: {0}", Plat.ToString());
        }

		bool bNoIOSOnPC = HostPlatforms.Contains(UnrealTargetPlatform.Mac);

        if (HostPlatforms.Count >= 2)
        {
            // make sure each project is set up with the right assumptions on monolithics that prefer a platform.
            foreach (var CodeProj in Branch.CodeProjects)
            {
                var OptionsMac = CodeProj.Options(UnrealTargetPlatform.Mac);
                var OptionsPC = CodeProj.Options(UnrealTargetPlatform.Win64);

				var MacMonos = GamePlatformMonolithicsNode.GetMonolithicPlatformsForUProject(UnrealTargetPlatform.Mac, ActivePlatforms, CodeProj, false, bNoIOSOnPC);
				var PCMonos = GamePlatformMonolithicsNode.GetMonolithicPlatformsForUProject(UnrealTargetPlatform.Win64, ActivePlatforms, CodeProj, false, bNoIOSOnPC);

                if (!OptionsMac.bIsPromotable && OptionsPC.bIsPromotable && 
                    (MacMonos.Contains(UnrealTargetPlatform.IOS) || PCMonos.Contains(UnrealTargetPlatform.IOS)))
                {
                    throw new AutomationException("Project {0} is promotable for PC, not promotable for Mac and uses IOS monothics. Since Mac is the preferred platform for IOS, please add Mac as a promotable platform.", CodeProj.GameName);
                }
                if (OptionsMac.bIsPromotable && !OptionsPC.bIsPromotable &&
                    (MacMonos.Contains(UnrealTargetPlatform.Android) || PCMonos.Contains(UnrealTargetPlatform.Android)))
                {
                    throw new AutomationException("Project {0} is not promotable for PC, promotable for Mac and uses Android monothics. Since PC is the preferred platform for Android, please add PC as a promotable platform.", CodeProj.GameName);
                }
            }
        }

		GUBPBranchConfig BranchConfig = new GUBPBranchConfig(HostPlatforms, Branch, BranchName, BranchOptions, bForceIncrementalCompile, JobInfo);

        BranchConfig.AddNode(new VersionFilesNode());


        foreach (var HostPlatform in HostPlatforms)
        {
			BranchConfig.AddNode(new ToolsForCompileNode(BranchConfig, HostPlatform, ParseParam("Launcher")));
			
			if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform))
			{
				BranchConfig.AddNode(new RootEditorNode(BranchConfig, HostPlatform));
			}
			
			BranchConfig.AddNode(new ToolsNode(BranchConfig, HostPlatform));
			
			if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform))
			{
				BranchConfig.AddNode(new InternalToolsNode(BranchConfig, HostPlatform));
			
				if (HostPlatform == UnrealTargetPlatform.Win64 && ActivePlatforms.Contains(UnrealTargetPlatform.Linux) && !BranchConfig.BranchOptions.ExcludeNodes.Contains("LinuxTools"))
				{
					BranchConfig.AddNode(new ToolsCrossCompileNode(BranchConfig, HostPlatform));
				}
				foreach (var ProgramTarget in Branch.BaseEngineProject.Properties.Programs)
				{
                    if (!BranchOptions.ExcludeNodes.Contains(ProgramTarget.TargetName))
                    {
                        bool bInternalOnly;
                        bool SeparateNode;
                        bool CrossCompile;

                        if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && SeparateNode)
                        {
                            if (bInternalOnly)
                            {
                                BranchConfig.AddNode(new SingleInternalToolsNode(BranchConfig, HostPlatform, ProgramTarget));
                            }
                            else
                            {
                                BranchConfig.AddNode(new SingleToolsNode(BranchConfig, HostPlatform, ProgramTarget));
                            }
                        }
                        if (ProgramTarget.Rules.GUBP_IncludeNonUnityToolTest())
                        {
                            BranchConfig.AddNode(new NonUnityToolNode(HostPlatform, ProgramTarget));
                        }
                    }
				}
				foreach(var CodeProj in Branch.CodeProjects)
				{
					foreach(var ProgramTarget in CodeProj.Properties.Programs)
					{
                        if (!BranchOptions.ExcludeNodes.Contains(ProgramTarget.TargetName))
                        {
                            bool bInternalNodeOnly;
                            bool SeparateNode;
                            bool CrossCompile;

                            if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalNodeOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && SeparateNode)
                            {
                                if (bInternalNodeOnly)
                                {
                                    BranchConfig.AddNode(new SingleInternalToolsNode(BranchConfig, HostPlatform, ProgramTarget));
                                }
                                else
                                {
                                    BranchConfig.AddNode(new SingleToolsNode(BranchConfig, HostPlatform, ProgramTarget));
                                }
                            }
                            if (ProgramTarget.Rules.GUBP_IncludeNonUnityToolTest())
                            {
                                BranchConfig.AddNode(new NonUnityToolNode(HostPlatform, ProgramTarget));
                            }
                        }
					}
				}

				BranchConfig.AddNode(new EditorAndToolsNode(this, HostPlatform));
			}

            bool DoASharedPromotable = false;

            int NumSharedCode = 0;
            foreach (var CodeProj in Branch.CodeProjects)
            {
                var Options = CodeProj.Options(HostPlatform);

                if (Options.bIsPromotable && !Options.bSeparateGamePromotion)
                {
                    NumSharedCode++;
                }
            }

            var NonCodeProjectNames = new Dictionary<string, List<UnrealTargetPlatform>>();
            var NonCodeFormalBuilds = new Dictionary<string, List<TargetRules.GUBPFormalBuild>>();
            {
                var Target = Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor];

                foreach (var Codeless in Target.Rules.GUBP_NonCodeProjects_BaseEditorTypeOnly(HostPlatform))
                {
                    var Proj = Branch.FindGame(Codeless.Key);
                    if (Proj == null)
                    {
                        LogVerbose("{0} was listed as a codeless project by GUBP_NonCodeProjects_BaseEditorTypeOnly, however it does not exist in this branch.", Codeless.Key);
                    }
                    else if (Proj.Properties.bIsCodeBasedProject)
                    {
						if (!Branch.NonCodeProjects.Contains(Proj)) 
						{ 
							Branch.NonCodeProjects.Add(Proj);
							NonCodeProjectNames.Add(Codeless.Key, Codeless.Value);
						}
                    }
                    else
                    {
                        NonCodeProjectNames.Add(Codeless.Key, Codeless.Value);
                    }
                }

                var TempNonCodeFormalBuilds = Target.Rules.GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly();
				var HostMonos = GamePlatformMonolithicsNode.GetMonolithicPlatformsForUProject(HostPlatform, ActivePlatforms, Branch.BaseEngineProject, true, bNoIOSOnPC);

                foreach (var Codeless in TempNonCodeFormalBuilds)
                {
                    if (NonCodeProjectNames.ContainsKey(Codeless.Key))
                    {
                        var PlatList = Codeless.Value;
                        var NewPlatList = new List<TargetRules.GUBPFormalBuild>();
                        foreach (var PlatPair in PlatList)
                        {
                            if (HostMonos.Contains(PlatPair.TargetPlatform))
                            {
                                NewPlatList.Add(PlatPair);
                            }
                        }
                        if (NewPlatList.Count > 0)
                        {
                            NonCodeFormalBuilds.Add(Codeless.Key, NewPlatList);
                        }
                    }
                    else
                    {
                        LogVerbose("{0} was listed as a codeless formal build GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly, however it does not exist in this branch.", Codeless.Key);
                    }
                }
            }

            DoASharedPromotable = NumSharedCode > 0 || NonCodeProjectNames.Count > 0 || NonCodeFormalBuilds.Count > 0;

			if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform) && !BranchOptions.ExcludeNodes.Contains("NonUnityTestCompile"))
			{
				BranchConfig.AddNode(new NonUnityTestNode(BranchConfig, HostPlatform));
			}

			if (HostPlatform == UnrealTargetPlatform.Win64 && !BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform) && !BranchOptions.ExcludeNodes.Contains("StaticAnalysis"))
			{
				BranchConfig.AddNode(new StaticAnalysisTestNode(BranchConfig, HostPlatform));
			}

            if (DoASharedPromotable)
            {
                var AgentSharingGroup = "Shared_EditorTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);

                var Options = Branch.BaseEngineProject.Options(HostPlatform);

                if (!Options.bIsPromotable || Options.bSeparateGamePromotion)
                {
                    throw new AutomationException("We assume that if we have shared promotable, the base engine is in it.");
                }


                if (!bNoAutomatedTesting && HostPlatform == UnrealTargetPlatform.Win64) //temp hack till automated testing works on other platforms than Win64
                {

                    var EditorTests = Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].Rules.GUBP_GetEditorTests_EditorTypeOnly(HostPlatform);
                    var EditorTestNodes = new List<string>();
                    foreach (var Test in EditorTests)
                    {
                        EditorTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, Branch.BaseEngineProject, Test.Key, Test.Value, AgentSharingGroup)));
                        
                        foreach (var NonCodeProject in Branch.NonCodeProjects)
                        {
                            if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName))
                            {
                                continue;
                            }
                            if (HostPlatform == UnrealTargetPlatform.Mac) continue; //temp hack till mac automated testing works
                            EditorTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, NonCodeProject, Test.Key, Test.Value, AgentSharingGroup)));
                        }
                    }
                    if (EditorTestNodes.Count > 0)
                    {
                        BranchConfig.AddNode(new GameAggregateNode(HostPlatform, Branch.BaseEngineProject, "AllEditorTests", EditorTestNodes));
                    }
                }


                var ServerPlatforms = new List<UnrealTargetPlatform>();
                var GamePlatforms = new List<UnrealTargetPlatform>();

                foreach (var Kind in BranchInfo.MonolithicKinds)
                {
                    if (Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                    {
                        var Target = Branch.BaseEngineProject.Properties.Targets[Kind];
                        var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                        if (Platforms.Contains(HostPlatform))
                        {
                            // we want the host platform first since some some pseudodependencies look to see if the shared promotable exists.
                            Platforms.Remove(HostPlatform);
                            Platforms.Insert(0, HostPlatform);
                        }
                        foreach (var Plat in Platforms)
                        {
                            if (!Platform.GetPlatform(HostPlatform).CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", Branch.BaseEngineProject.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && (Plat == UnrealTargetPlatform.IOS || Plat == UnrealTargetPlatform.TVOS) && HostPlatform == UnrealTargetPlatform.Win64)
                            {
                                continue;
                            }

                            if (ActivePlatforms.Contains(Plat))
                            {
                                if (Kind == TargetRules.TargetType.Server && !ServerPlatforms.Contains(Plat))
                                {
                                    ServerPlatforms.Add(Plat);
                                }
                                if (Kind == TargetRules.TargetType.Game && !GamePlatforms.Contains(Plat))
                                {
                                    GamePlatforms.Add(Plat);
                                }
                                if (!BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat)))
                                {
									if(GamePlatformMonolithicsNode.HasPrecompiledTargets(Branch.BaseEngineProject, HostPlatform, Plat))
									{
	                                    BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, Branch.BaseEngineProject, Plat, InPrecompiled: true));
									}
                                    BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, Branch.BaseEngineProject, Plat));
                                }
								if (Plat == UnrealTargetPlatform.Win32 && Target.Rules.GUBP_BuildWindowsXPMonolithics() && Kind == TargetRules.TargetType.Game)
								{
									if (!BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat, true)))
									{
										BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, Branch.BaseEngineProject, Plat, true));
									}
	                            }
							}
                        }
                    }
                }

                var CookedSharedAgentSharingGroup = "Shared_CookedTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
                var CookedSampleAgentSharingGroup = "Sample_CookedTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);

                var GameTestNodes = new List<string>();
                var GameCookNodes = new List<string>();
				//var FormalAgentSharingGroup = "Shared_FormalBuilds" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);


                //foreach (var Kind in BranchInfo.MonolithicKinds)//for now, non-code projects don't do client or server.
                {
                    var Kind = TargetRules.TargetType.Game;
                    if (Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                    {
                        var Target = Branch.BaseEngineProject.Properties.Targets[Kind];
                        var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                        foreach (var Plat in Platforms)
                        {
                            if (!Platform.GetPlatform(HostPlatform).CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", Branch.BaseEngineProject.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && (Plat == UnrealTargetPlatform.IOS || Plat == UnrealTargetPlatform.TVOS) && HostPlatform == UnrealTargetPlatform.Win64)
                            {
                                continue;
                            }

                            if (ActivePlatforms.Contains(Plat))
                            {
                                string CookedPlatform = Platform.GetPlatform(Plat).GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client);								
                                if (!BranchConfig.HasNode(CookNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, CookedPlatform)))
                                {
									GameCookNodes.Add(BranchConfig.AddNode(new CookNode(BranchConfig, HostPlatform, Branch.BaseEngineProject, Plat, CookedPlatform)));
                                }
                                if (!BranchConfig.HasNode(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat)))
                                {
									BranchConfig.AddNode(new GamePlatformCookedAndCompiledNode(BranchConfig, HostPlatform, Branch.BaseEngineProject, Plat, false));
                                }
                                var GameTests = Target.Rules.GUBP_GetGameTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), Plat);
                                var RequiredPlatforms = new List<UnrealTargetPlatform> { Plat };
                                if (!bNoAutomatedTesting)
                                {
                                    var ThisMonoGameTestNodes = new List<string>();	
                                    
                                    foreach (var Test in GameTests)
                                    {
                                        var TestName = Test.Key + "_" + Plat.ToString();
                                        ThisMonoGameTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, Branch.BaseEngineProject, TestName, Test.Value, CookedSharedAgentSharingGroup, false, RequiredPlatforms)));
                                    }
                                    if (ThisMonoGameTestNodes.Count > 0)
                                    {										
                                        GameTestNodes.Add(BranchConfig.AddNode(new GameAggregateNode(HostPlatform, Branch.BaseEngineProject, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes)));
                                    }
                                }

                                foreach (var NonCodeProject in Branch.NonCodeProjects)
                                {
                                    if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName) || !NonCodeProjectNames[NonCodeProject.GameName].Contains(Plat))
                                    {
                                        continue;
                                    }
                                    if (BranchOptions.ProjectsToCook.Contains(NonCodeProject.GameName) || BranchOptions.ProjectsToCook.Count == 0)
                                    {
										if (!BranchConfig.HasNode(CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform)))
										{
											GameCookNodes.Add(BranchConfig.AddNode(new CookNode(BranchConfig, HostPlatform, NonCodeProject, Plat, CookedPlatform)));
										}
										if (!BranchConfig.HasNode(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, NonCodeProject, Plat)))
										{
											BranchConfig.AddNode(new GamePlatformCookedAndCompiledNode(BranchConfig, HostPlatform, NonCodeProject, Plat, false));

											if (NonCodeFormalBuilds.ContainsKey(NonCodeProject.GameName))
											{
												var PlatList = NonCodeFormalBuilds[NonCodeProject.GameName];
												foreach (var PlatPair in PlatList)
												{
													if (PlatPair.TargetPlatform == Plat)
													{
														var NodeName = BranchConfig.AddNode(new FormalBuildNode(BranchConfig, NonCodeProject, HostPlatform, new List<UnrealTargetPlatform>() { Plat }, new List<UnrealTargetConfiguration>() { PlatPair.TargetConfig }));                                                    
														// we don't want this delayed
														// this would normally wait for the testing phase, we just want to build it right away
														BranchConfig.RemovePseudodependencyFromNode(
															CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform),
															WaitForTestShared.StaticGetFullName());
														string BuildAgentSharingGroup = "";
														if (Options.bSeparateGamePromotion)
														{
															BuildAgentSharingGroup = NonCodeProject.GameName + "_MakeFormalBuild_" + Plat.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
															if (Plat == UnrealTargetPlatform.IOS || Plat == UnrealTargetPlatform.Android) // These trash build products, so we need to use different agents
															{
																BuildAgentSharingGroup = "";
															}
															BranchConfig.FindNode(CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform)).AgentSharingGroup = BuildAgentSharingGroup;
															BranchConfig.FindNode(NodeName).AgentSharingGroup = BuildAgentSharingGroup;
														}
														else
														{
															//GUBPNodes[NodeName].AgentSharingGroup = FormalAgentSharingGroup;
																if (Plat == UnrealTargetPlatform.XboxOne)
															{
																BranchConfig.FindNode(NodeName).AgentSharingGroup = "";
															}
														}
														if (PlatPair.bTest)
														{
															BranchConfig.AddNode(new FormalBuildTestNode(BranchConfig, NonCodeProject, HostPlatform, Plat, PlatPair.TargetConfig));
														}
													}
												}
											}
										}

										if (!bNoAutomatedTesting)
                                        {
											if (HostPlatform == UnrealTargetPlatform.Mac || HostPlatform == UnrealTargetPlatform.Linux) continue; //temp hack till Linux and Mac automated testing works
											var ThisMonoGameTestNodes = new List<string>();
											foreach (var Test in GameTests)
											{
												var TestName = Test.Key + "_" + Plat.ToString();
												string CookedAgentSharingGroup = GamePlatformMonolithicsNode.IsSample(BranchConfig, NonCodeProject)? CookedSampleAgentSharingGroup : CookedSharedAgentSharingGroup;
												ThisMonoGameTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, NonCodeProject, TestName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
											}
											if (ThisMonoGameTestNodes.Count > 0)
											{
												GameTestNodes.Add(BranchConfig.AddNode(new GameAggregateNode(HostPlatform, NonCodeProject, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes)));
											}
										}
									}
								}
							}
						}
					}
                }
#if false
                //for now, non-code projects don't do client or server.
                foreach (var ServerPlatform in ServerPlatforms)
                {
                    var ServerTarget = Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Server];
                    foreach (var GamePlatform in GamePlatforms)
                    {
                        var Target = Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Game];

                        foreach (var NonCodeProject in Branch.NonCodeProjects)
                        {
                            if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName) || !NonCodeProjectNames.ContainsKey(NonCodeProject.GameName) ||
                                    !NonCodeProjectNames[NonCodeProject.GameName].Contains(ServerPlatform)  || !NonCodeProjectNames[NonCodeProject.GameName].Contains(GamePlatform) )
                            {
                                continue;
                            }

                            var ClientServerTests = Target.Rules.GUBP_GetClientServerTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), ServerPlatform, GamePlatform);
                            var RequiredPlatforms = new List<UnrealTargetPlatform> { ServerPlatform };
                            if (ServerPlatform != GamePlatform)
                            {
                                RequiredPlatforms.Add(GamePlatform);
                            }
                            foreach (var Test in ClientServerTests)
                            {
                                GameTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, NonCodeProject, Test.Key + "_" + GamePlatform.ToString() + "_" + ServerPlatform.ToString(), Test.Value, false, RequiredPlatforms, true)));
                            }
                        }
                    }
                }
#endif
                if (GameTestNodes.Count > 0)
                {
                    BranchConfig.AddNode(new GameAggregateNode(HostPlatform, Branch.BaseEngineProject, "AllCookedTests", GameTestNodes));
                }
            }
			
			if(HostPlatform == MakeFeaturePacksNode.GetDefaultBuildPlatform(HostPlatforms) && !BranchOptions.bNoInstalledEngine)
			{
				BranchConfig.AddNode(new MakeFeaturePacksNode(HostPlatform, Branch.AllProjects.Where(x => MakeFeaturePacksNode.IsFeaturePack(x))));
			}

            foreach (var CodeProj in Branch.CodeProjects)
            {
                var Options = CodeProj.Options(HostPlatform);

                if (!Options.bIsPromotable && !Options.bTestWithShared && !Options.bIsNonCode && !Options.bBuildAnyway)
                {
                    continue; // we skip things that aren't promotable and aren't tested - except noncode as code situations
                }
                var AgentShareName = CodeProj.GameName;
                if (!Options.bSeparateGamePromotion)
                {
					if(GamePlatformMonolithicsNode.IsSample(BranchConfig, CodeProj))
					{
						AgentShareName = "Sample";
					}
					else
					{
						AgentShareName = "Shared";
					}
                }

				if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform) && !Options.bIsNonCode)
				{
					EditorGameNode Node = (EditorGameNode)BranchConfig.TryFindNode(EditorGameNode.StaticGetFullName(HostPlatform, CodeProj));
					if(Node == null)
					{
						BranchConfig.AddNode(new EditorGameNode(BranchConfig, HostPlatform, CodeProj));
					}
					else
					{
						Node.AddProject(CodeProj);
					}
				}

				if (!bNoAutomatedTesting && HostPlatform == UnrealTargetPlatform.Win64) //temp hack till automated testing works on other platforms than Win64
                {
					if (CodeProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
					{
						var EditorTests = CodeProj.Properties.Targets[TargetRules.TargetType.Editor].Rules.GUBP_GetEditorTests_EditorTypeOnly(HostPlatform);
						var EditorTestNodes = new List<string>();
						string AgentSharingGroup = "";
						if (EditorTests.Count > 1)
						{
							AgentSharingGroup = AgentShareName + "_EditorTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
						}
						foreach (var Test in EditorTests)
						{
							EditorTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, CodeProj, Test.Key, Test.Value, AgentSharingGroup)));
							if (!Options.bTestWithShared || !BranchConfig.HasNode(WaitForTestShared.StaticGetFullName()))
							{
								BranchConfig.RemovePseudodependencyFromNode((UATTestNode.StaticGetFullName(HostPlatform, CodeProj, Test.Key)), WaitForTestShared.StaticGetFullName());
							}
						}
						if (EditorTestNodes.Count > 0)
						{
							BranchConfig.AddNode(new GameAggregateNode(HostPlatform, CodeProj, "AllEditorTests", EditorTestNodes));
						}
					}
                }

                var CookedAgentSharingGroup = AgentShareName + "_CookedTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
				//var FormalAgentSharingGroup = "Shared_FormalBuilds" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
                var ServerPlatforms = new List<UnrealTargetPlatform>();
                var GamePlatforms = new List<UnrealTargetPlatform>();
                var GameTestNodes = new List<string>();
                foreach (var Kind in BranchInfo.MonolithicKinds)
                {
                    if (CodeProj.Properties.Targets.ContainsKey(Kind))
                    {
                        var Target = CodeProj.Properties.Targets[Kind];
                        var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
						var AdditionalPlatforms = Target.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform);
						var AllPlatforms = Platforms.Union(AdditionalPlatforms);
						foreach (var Plat in AllPlatforms)
                        {
                            if (!Platform.GetPlatform(HostPlatform).CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", CodeProj.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && (Plat == UnrealTargetPlatform.IOS || Plat == UnrealTargetPlatform.TVOS) && HostPlatform == UnrealTargetPlatform.Win64)
                            {
                                continue;
                            }
							if(Plat == UnrealTargetPlatform.Win32 && Target.Rules.GUBP_BuildWindowsXPMonolithics())
							{
								if(!BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, CodeProj, Plat, true)))
								{
									BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, CodeProj, Plat, true));
								}
							}
                            if (ActivePlatforms.Contains(Plat))
                            {
                                if (Kind == TargetRules.TargetType.Server && !ServerPlatforms.Contains(Plat))
                                {
                                    ServerPlatforms.Add(Plat);
                                }
                                if (Kind == TargetRules.TargetType.Game && !GamePlatforms.Contains(Plat))
                                {
                                    GamePlatforms.Add(Plat);
                                }
                                if (!BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, CodeProj, Plat)))
                                {
									if(GamePlatformMonolithicsNode.HasPrecompiledTargets(CodeProj, HostPlatform, Plat))
									{
	                                    BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, CodeProj, Plat, InPrecompiled: true));
									}
                                    BranchConfig.AddNode(new GamePlatformMonolithicsNode(BranchConfig, HostPlatform, ActivePlatforms, CodeProj, Plat));
                                }
								var FormalBuildConfigs = Target.Rules.GUBP_GetConfigsForFormalBuilds_MonolithicOnly(HostPlatform);
								if (!AdditionalPlatforms.Contains(Plat) && (BranchOptions.ProjectsToCook.Contains(CodeProj.GameName) || BranchOptions.ProjectsToCook.Count == 0))
								{
									string CookedPlatform = Platform.GetPlatform(Plat).GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client);
									if (Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform) != "")
									{
										CookedPlatform = Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform);
									}
									if (!BranchConfig.HasNode(CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform)))
									{
										BranchConfig.AddNode(new CookNode(BranchConfig, HostPlatform, CodeProj, Plat, CookedPlatform));
									}
									if (!BranchConfig.HasNode(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, CodeProj, Plat)))
									{
										BranchConfig.AddNode(new GamePlatformCookedAndCompiledNode(BranchConfig, HostPlatform, CodeProj, Plat, true));
									}
									

									foreach (var Config in FormalBuildConfigs)
									{
										string FormalNodeName = null;                                    
										if (Kind == TargetRules.TargetType.Client)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = BranchConfig.AddNode(new FormalBuildNode(BranchConfig, CodeProj, HostPlatform, InClientTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }, InClientNotGame: true));
											}
										}
										else if (Kind == TargetRules.TargetType.Server)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = BranchConfig.AddNode(new FormalBuildNode(BranchConfig, CodeProj, HostPlatform, InServerTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InServerConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }));
											}
										}
										else if (Kind == TargetRules.TargetType.Game)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = BranchConfig.AddNode(new FormalBuildNode(BranchConfig, CodeProj, HostPlatform, InClientTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }));
											}
										}
										if (FormalNodeName != null)
										{
											// we don't want this delayed
											// this would normally wait for the testing phase, we just want to build it right away
											BranchConfig.RemovePseudodependencyFromNode(
												CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform),
												WaitForTestShared.StaticGetFullName());
											string BuildAgentSharingGroup = "";
											
											if (Options.bSeparateGamePromotion)
											{
												BuildAgentSharingGroup = CodeProj.GameName + "_MakeFormalBuild_" + Plat.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);
												if (Plat == UnrealTargetPlatform.IOS || Plat == UnrealTargetPlatform.Android || Plat == UnrealTargetPlatform.XboxOne) // These trash build products, so we need to use different agents
												{
													BuildAgentSharingGroup = "";
												}
												BranchConfig.FindNode(CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform)).AgentSharingGroup = BuildAgentSharingGroup;
												BranchConfig.FindNode(FormalNodeName).AgentSharingGroup = BuildAgentSharingGroup;
											}
											else
											{
												//GUBPNodes[FormalNodeName].AgentSharingGroup = FormalAgentSharingGroup;
												if (Plat == UnrealTargetPlatform.XboxOne)
												{
													BranchConfig.FindNode(FormalNodeName).AgentSharingGroup = "";
												}
											}
											if (Config.bTest)
											{
												BranchConfig.AddNode(new FormalBuildTestNode(BranchConfig, CodeProj, HostPlatform, Plat, Config.TargetConfig));
											}																				
										}
									}
									if (!bNoAutomatedTesting && FormalBuildConfigs.Count > 0)
									{
										if (HostPlatform == UnrealTargetPlatform.Mac || HostPlatform == UnrealTargetPlatform.Linux) continue; //temp hack till Linux and Mac automated testing works
										var GameTests = Target.Rules.GUBP_GetGameTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), Plat);
										var RequiredPlatforms = new List<UnrealTargetPlatform> { Plat };
										var ThisMonoGameTestNodes = new List<string>();

										foreach (var Test in GameTests)
										{
											var TestNodeName = Test.Key + "_" + Plat.ToString();
											ThisMonoGameTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, CodeProj, TestNodeName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
											if (!Options.bTestWithShared || !BranchConfig.HasNode(WaitForTestShared.StaticGetFullName()))
											{
												BranchConfig.RemovePseudodependencyFromNode((UATTestNode.StaticGetFullName(HostPlatform, CodeProj, TestNodeName)), WaitForTestShared.StaticGetFullName());
											}
										}
										if (ThisMonoGameTestNodes.Count > 0)
										{
											GameTestNodes.Add(BranchConfig.AddNode(new GameAggregateNode(HostPlatform, CodeProj, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes)));
										}
									}
								}
							}
						}
					}
				}
				if (!bNoAutomatedTesting)
				{
					foreach (var ServerPlatform in ServerPlatforms)
					{
						foreach (var GamePlatform in GamePlatforms)
						{
							if (HostPlatform == UnrealTargetPlatform.Mac || HostPlatform == UnrealTargetPlatform.Linux) continue; //temp hack till Linux and Mac automated testing works
							var Target = CodeProj.Properties.Targets[TargetRules.TargetType.Game];
							var ClientServerTests = Target.Rules.GUBP_GetClientServerTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), ServerPlatform, GamePlatform);
							var RequiredPlatforms = new List<UnrealTargetPlatform> { ServerPlatform };
							if (ServerPlatform != GamePlatform)
							{
								RequiredPlatforms.Add(GamePlatform);
							}
							foreach (var Test in ClientServerTests)
							{
								var TestNodeName = Test.Key + "_" + GamePlatform.ToString() + "_" + ServerPlatform.ToString();
								GameTestNodes.Add(BranchConfig.AddNode(new UATTestNode(BranchConfig, HostPlatform, CodeProj, TestNodeName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
							}
						}
					}
					if (GameTestNodes.Count > 0)
					{
						BranchConfig.AddNode(new GameAggregateNode(HostPlatform, CodeProj, "AllCookedTests", GameTestNodes));
					}
				}
			}
		}

		if (HostPlatforms.Contains(UnrealTargetPlatform.Win64) && BranchConfig.HasNode(RootEditorNode.StaticGetFullName(UnrealTargetPlatform.Win64)) && BranchOptions.bBuildEngineLocalization)
		{
			BranchConfig.AddNode(new BuildEngineLocalizationNode(BranchOptions.EngineLocalizationBranchSuffix));
		}

		if(BranchConfig.BranchName != null && (BranchConfig.BranchName.StartsWith("//UE4/Dev-", StringComparison.InvariantCultureIgnoreCase) || ParseParam("WithSingleTargetNodes")))
		{
			List<BranchInfo.BranchUProject> CodeProjects = new List<BranchInfo.BranchUProject>();
			CodeProjects.Add(BranchConfig.Branch.BaseEngineProject);
			CodeProjects.AddRange(BranchConfig.Branch.CodeProjects);

			foreach(BranchInfo.BranchUProject CodeProject in CodeProjects)
			{
				SingleTargetProperties Properties;
				if(CodeProject.Properties.Targets.TryGetValue(TargetRules.TargetType.Game, out Properties))
				{
					foreach(UnrealTargetPlatform HostPlatform in HostPlatforms)
					{
						HashSet<UnrealTargetPlatform> TargetPlatforms = new HashSet<UnrealTargetPlatform>();
						TargetPlatforms.UnionWith(Properties.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform));
						TargetPlatforms.UnionWith(Properties.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform));
						foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
						{
							GUBP.SingleTargetNode TargetNode = new SingleTargetNode(BranchConfig, HostPlatform, CodeProject, Properties.TargetName, TargetPlatform, UnrealTargetConfiguration.Development);
							BranchConfig.AddNode(TargetNode);
						}
					}
				}
			}
		}

        BranchConfig.AddNode(new WaitForTestShared(this));
		List<UnrealTargetPlatform> HostPlatformsToWaitFor = BranchConfig.HostPlatforms;
		HostPlatformsToWaitFor.RemoveAll(HostPlatform => BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform));

		foreach(GUBPNode Node in BranchConfig.GUBPNodes.Values)
		{
			if(Node.FullNamesOfDependencies.Contains(WaitToPackageSamplesNode.StaticGetFullName()) || Node.FullNamesOfPseudodependencies.Contains(WaitToPackageSamplesNode.StaticGetFullName()))
			{
				BranchConfig.AddNode(new WaitToPackageSamplesNode(HostPlatformsToWaitFor));
				break;
			}
		}

		AddCustomNodes(BranchConfig, HostPlatforms, ActivePlatforms);
        
        if (BranchConfig.HasNode(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64)))
        {
			if (BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(UnrealTargetPlatform.Mac, Branch.BaseEngineProject, UnrealTargetPlatform.IOS)) && BranchConfig.HasNode(ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64)))
			{
				//AddNode(new IOSOnPCTestNode(this)); - Disable IOSOnPCTest until a1011 crash is fixed
			}
			//AddNode(new VSExpressTestNode(this));
			if (ActivePlatforms.Contains(UnrealTargetPlatform.Linux) && !BranchOptions.ExcludePlatformsForEditor.Contains(UnrealTargetPlatform.Linux))
			{
				BranchConfig.AddNode(new RootEditorCrossCompileLinuxNode(BranchConfig, UnrealTargetPlatform.Win64));
			}
            // Don't run clean on temp shared storage for preflight builds. They might have broken the clean, and it extends to storage beyond this job.
            if (!BranchConfig.JobInfo.IsPreflight && !bNewEC)
            {
                BranchConfig.AddNode(new CleanSharedTempStorageNode(this, BranchConfig));
            }
        }

		// Add an aggregate for all the editors in this branch
		List<GUBPNode> AllEditorNodes = new List<GUBPNode>(BranchConfig.GUBPNodes.Values.Where(x => (x is RootEditorNode) || (x is EditorGameNode)));
		BranchConfig.AddNode(new GenericAggregateNode("AllEditors", AllEditorNodes.Select(x => x.GetFullName())));

		// Add an aggregate for all the tools in the branch
		List<GUBPNode> AllToolNodes = new List<GUBPNode>(BranchConfig.GUBPNodes.Values.Where(x => (x is ToolsNode) || (x is SingleToolsNode) || (x is InternalToolsNode) || (x is SingleInternalToolsNode)));
		BranchConfig.AddNode(new GenericAggregateNode("AllTools", AllToolNodes.Select(x => x.GetFullName())));

		// Add an aggregate for all the tools in the branch
		List<GUBPNode> AllMonolithicNodes = new List<GUBPNode>(BranchConfig.GUBPNodes.Values.Where(x => (x is GamePlatformMonolithicsNode)));
		BranchConfig.AddNode(new GenericAggregateNode("AllMonolithics", AllMonolithicNodes.Select(x => x.GetFullName())));

		// Add an aggregate for all the nodes in the branch
		BranchConfig.AddNode(new GenericAggregateNode("AllNodes", BranchConfig.GUBPNodes.Keys));

#if false
        // this doesn't work for lots of reasons...we can't figure out what the dependencies are until far later
        if (bPreflightBuild)
        {
            GeneralSuccessNode PreflightSuccessNode = new GeneralSuccessNode("Preflight");
            foreach (var NodeToDo in GUBPNodes)
            {
                if (NodeToDo.Value.RunInEC())
                {
                    PreflightSuccessNode.AddPseudodependency(NodeToDo.Key);
                }
            }
            AddNode(PreflightSuccessNode);
        }
#endif

		// Remove all the pseudo-dependencies on nodes specified in the branch options
		foreach(string NodeToRemovePseudoDependencies in BranchOptions.NodesToRemovePseudoDependencies)
		{
			GUBPNode Node = BranchConfig.TryFindNode(NodeToRemovePseudoDependencies);
			if(Node != null)
			{
				Node.FullNamesOfPseudodependencies.Clear();
			}
		}

		// Add aggregate nodes for every project in the branch
		foreach (BranchInfo.BranchUProject GameProj in Branch.AllProjects)
		{
			List<string> NodeNames = new List<string>();
			foreach(GUBP.GUBPNode Node in BranchConfig.GUBPNodes.Values)
			{
				if (Node.GameNameIfAnyForFullGameAggregateNode() == GameProj.GameName)
				{
					NodeNames.Add(Node.GetFullName());
				}
			}
			if (NodeNames.Count > 0)
			{
				BranchConfig.AddNode(new FullGameAggregateNode(GameProj.GameName, NodeNames));
			}
		}

		// Calculate the frequency overrides
		Dictionary<string, int> FrequencyOverrides = ApplyFrequencyBarriers(BranchConfig.GUBPNodes, BranchConfig.GUBPAggregates, BranchOptions.FrequencyBarriers);

		// Get the legacy node to new node mapping
		Dictionary<GUBP.GUBPNode, BuildNodeDefinition> LegacyToNewNodes = BranchConfig.GUBPNodes.Values.ToDictionary(x => x, x => x.GetDefinition(this));

		// Convert the GUBPNodes and GUBPAggregates maps into lists
		AllNodeDefinitions = new List<BuildNodeDefinition>();
		AllNodeDefinitions.AddRange(LegacyToNewNodes.Values);

		AllAggregateDefinitions = new List<AggregateNodeDefinition>();
		AllAggregateDefinitions.AddRange(BranchConfig.GUBPAggregates.Values.Select(x => x.GetDefinition()));

		// Calculate the frequencies for each node
		FindFrequenciesForNodes(BranchConfig, LegacyToNewNodes, FrequencyOverrides);

		// Get the email list for each node
		FindEmailsForNodes(BranchConfig.BranchName, AllNodeDefinitions);

		TimeSpan Span = DateTime.UtcNow - StartTime;
		Log("Time Spent: " + Span.TotalSeconds);
	}

	private static Dictionary<string, int> ApplyFrequencyBarriers(Dictionary<string, GUBPNode> GUBPNodes, Dictionary<string, GUBPAggregateNode> GUBPAggregates, Dictionary<string, sbyte> FrequencyBarriers)
	{
		// Make sure that everything that's listed as a frequency barrier is completed with the given interval
		Dictionary<string, int> FrequencyOverrides = new Dictionary<string,int>();
		foreach (KeyValuePair<string, sbyte> Barrier in FrequencyBarriers)
		{
			// All the nodes which are dependencies of the barrier node
			HashSet<string> IncludedNodes = new HashSet<string>();

			// Find all the nodes which are indirect dependencies of this node
			List<string> SearchNodes = new List<string> { Barrier.Key };
			for (int Idx = 0; Idx < SearchNodes.Count; Idx++)
			{
				if(IncludedNodes.Add(SearchNodes[Idx]))
				{
					GUBPNode Node;
					if(GUBPNodes.TryGetValue(SearchNodes[Idx], out Node))
					{
						SearchNodes.AddRange(Node.FullNamesOfDependencies);
						SearchNodes.AddRange(Node.FullNamesOfPseudodependencies);
					}
					else
					{
						GUBPAggregateNode Aggregate;
						if(GUBPAggregates.TryGetValue(SearchNodes[Idx], out Aggregate))
						{
							SearchNodes.AddRange(Aggregate.Dependencies);
						}
						else
						{
							CommandUtils.LogWarning("Couldn't find referenced frequency barrier node {0}", SearchNodes[Idx]);
						}
					}
				}
			}

			// Make sure that everything included in this list is before the cap, and everything not in the list is after it
			foreach (string NodeName in GUBPNodes.Keys)
			{
				if (IncludedNodes.Contains(NodeName))
				{
					int Frequency;
					if(FrequencyOverrides.TryGetValue(NodeName, out Frequency))
					{
						Frequency = Math.Min(Frequency, Barrier.Value);
					}
					else
					{
						Frequency = Barrier.Value;
					}
					FrequencyOverrides[NodeName] = Frequency;
				}
			}
		}
		return FrequencyOverrides;
	}
}
