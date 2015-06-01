// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Reflection;
using System.Xml;
using System.Linq;

public class ECJobPropsUtils
{
    public static HashSet<string> ErrorsFromProps(string Filename)
    {
        var Result = new HashSet<string>();
        XmlDocument Doc = new XmlDocument();
        Doc.Load(Filename);
        foreach (XmlElement ChildNode in Doc.FirstChild.ChildNodes)
        {
            if (ChildNode.Name == "propertySheet")
            {
                foreach (XmlElement PropertySheetChild in ChildNode.ChildNodes)
                {
                    if (PropertySheetChild.Name == "property")
                    {
                        bool IsDiag = false;
                        foreach (XmlElement PropertySheetChildDiag in PropertySheetChild.ChildNodes)
                        {
                            if (PropertySheetChildDiag.Name == "propertyName" && PropertySheetChildDiag.InnerText == "ec_diagnostics")
                            {
                                IsDiag = true;
                            }
                            if (IsDiag && PropertySheetChildDiag.Name == "propertySheet")
                            {
                                foreach (XmlElement PropertySheetChildDiagSheet in PropertySheetChildDiag.ChildNodes)
                                {
                                    if (PropertySheetChildDiagSheet.Name == "property")
                                    {
                                        bool IsError = false;
                                        foreach (XmlElement PropertySheetChildDiagSheetElem in PropertySheetChildDiagSheet.ChildNodes)
                                        {
                                            if (PropertySheetChildDiagSheetElem.Name == "propertyName" && PropertySheetChildDiagSheetElem.InnerText.StartsWith("error-"))
                                            {
                                                IsError = true;
                                            }
                                            if (IsError && PropertySheetChildDiagSheetElem.Name == "propertySheet")
                                            {
                                                foreach (XmlElement PropertySheetChildDiagSheetElemInner in PropertySheetChildDiagSheetElem.ChildNodes)
                                                {
                                                    if (PropertySheetChildDiagSheetElemInner.Name == "property")
                                                    {
                                                        bool IsMessage = false;
                                                        foreach (XmlElement PropertySheetChildDiagSheetElemInner2 in PropertySheetChildDiagSheetElemInner.ChildNodes)
                                                        {
                                                            if (PropertySheetChildDiagSheetElemInner2.Name == "propertyName" && PropertySheetChildDiagSheetElemInner2.InnerText == "message")
                                                            {
                                                                IsMessage = true;
                                                            }
                                                            if (IsMessage && PropertySheetChildDiagSheetElemInner2.Name == "value")
                                                            {
                                                                if (!PropertySheetChildDiagSheetElemInner2.InnerText.Contains("LogTailsAndChanges")
                                                                    && !PropertySheetChildDiagSheetElemInner2.InnerText.Contains("-MyJobStepId=")
                                                                    && !PropertySheetChildDiagSheetElemInner2.InnerText.Contains("CommandUtils.Run: Run: Took ")
                                                                    )
                                                                {
                                                                    Result.Add(PropertySheetChildDiagSheetElemInner2.InnerText);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return Result;
    }
}

public class TestECJobErrorParse : BuildCommand
{
    public override void ExecuteBuild()
    {
        Log("*********************** TestECJobErrorParse");

        string Filename = CombinePaths(@"P:\Builds\UE4\GUBP\++depot+UE4-2104401-RootEditor_Failed\Engine\Saved\Logs", "RootEditor_Failed.log");
        var Errors = ECJobPropsUtils.ErrorsFromProps(Filename);
        foreach (var ThisError in Errors)
        {
            Log("Error: {0}", ThisError);
        }
    }
}


public class GUBP : BuildCommand
{
    public string StoreName = null;
	public string BranchName;
    public int CL = 0;
    public int TimeIndex = 0;
    public bool bSignBuildProducts = false;
    public bool bHasTests = false;
    public List<UnrealTargetPlatform> ActivePlatforms = null;
    public BranchInfo Branch = null;
    public bool bOrthogonalizeEditorPlatforms = false;
    public List<UnrealTargetPlatform> HostPlatforms;
    public bool bFake = false;
    public static bool bNoIOSOnPC = false;
    public static bool bForceIncrementalCompile = false;
    public static string ECProject = null;
    public string EmailHint;
    static public bool bPreflightBuild = false;
    public int PreflightShelveCL = 0;
    static public string PreflightMangleSuffix = "";
    public GUBPBranchHacker.BranchOptions BranchOptions = null;	

    Dictionary<string, GUBPNode> GUBPNodes;
    Dictionary<string, bool> GUBPNodesCompleted;
    Dictionary<string, string> GUBPNodesControllingTrigger;
    Dictionary<string, string> GUBPNodesControllingTriggerDotName;

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

    Dictionary<string, NodeHistory> GUBPNodesHistory;

    public abstract class GUBPNodeAdder
    {
        public virtual void AddNodes(GUBP bp, UnrealTargetPlatform InHostPlatform)
        {
        }
    }

    private static List<GUBPNodeAdder> Adders;

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

    private void AddCustomNodes(UnrealTargetPlatform InHostPlatform)
    {
        if (Adders == null)
        {
            Adders = new List<GUBPNodeAdder>();
            Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
            foreach (var Dll in LoadedAssemblies)
            {
				Type[] AllTypes = GetTypesFromAssembly(Dll);
                foreach (var PotentialConfigType in AllTypes)
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
        }
        foreach(var Adder in Adders)
        {
            Adder.AddNodes(this, InHostPlatform);
        }
    }

    public abstract class GUBPBranchHacker
    {
        public class BranchOptions
        {            
            public List<UnrealTargetPlatform> PlatformsToRemove = new List<UnrealTargetPlatform>();
			public List<string> ExcludeNodes = new List<string>();
			public List<UnrealTargetPlatform> ExcludePlatformsForEditor = new List<UnrealTargetPlatform>();
            public List<UnrealTargetPlatform> RemovePlatformFromPromotable = new List<UnrealTargetPlatform>();
			public bool bNoAutomatedTesting = false;
			public bool bNoDocumentation = false;
			public bool bNoInstalledEngine = false;
			public bool bMakeFormalBuildWithoutLabelPromotable = false;
			public Dictionary<string, sbyte> FrequencyBarriers = new Dictionary<string,sbyte>();
			public int QuantumOverride = 0;
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
            foreach (var Dll in LoadedAssemblies)
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
        var Result = new GUBPBranchHacker.BranchOptions();
        foreach (var Hacker in BranchHackers)
        {
            Hacker.ModifyOptions(this, ref Result, Branch);
        }
        return Result;
    }

    public abstract class GUBPEmailHacker
    {        
        public virtual List<string> AddEmails(GUBP bp, string Branch, string NodeName)
        {
            return new List<string>();
        }
        public virtual List<string> ModifyEmail(string Email, GUBP bp, string Branch, string NodeName)
        {
            return new List<string>{Email};
        }
        public virtual List<string> FinalizeEmails(List<string> Emails, GUBP bp, string Branch, string NodeName)
        {
            return Emails;
        }
		public virtual bool VetoEmailingCausers(GUBP bp, string Branch, string NodeName)
		{
			return false; // People who have submitted since last-green will be included unless vetoed by overriding this method. 
		}
    }

    private static List<GUBPEmailHacker> EmailHackers;
    private string HackEmails(string Emails, string Causers, string Branch, string NodeName)
    {
        string OnlyEmail = ParseParamValue("OnlyEmail");
        if (!String.IsNullOrEmpty(OnlyEmail))
        {
            return OnlyEmail;
        }
        EmailHint = ParseParamValue("EmailHint");
        if(EmailHint == null)
        {
            EmailHint = "";
        }			
        if (EmailHackers == null)
        {
            EmailHackers = new List<GUBPEmailHacker>();
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
        }
        List<string> Result = new List<string>(Emails.Split(' '));
		if(!EmailHackers.Any(x => x.VetoEmailingCausers(this, Branch, NodeName)))
		{
			Result.AddRange(Causers.Split(' '));
		}
        foreach (var EmailHacker in EmailHackers)
        {
            Result.AddRange(EmailHacker.AddEmails(this, Branch, NodeName));
        }
        foreach (var EmailHacker in EmailHackers)
        {
            var NewResult = new List<string>();
            foreach (var Email in Result)
            {
                NewResult.AddRange(EmailHacker.ModifyEmail(Email, this, Branch, NodeName));
            }
            Result = NewResult;
        }
        foreach (var EmailHacker in EmailHackers)
        {
            Result = EmailHacker.FinalizeEmails(Result, this, Branch, NodeName);
        }
        string FinalEmails = "";
        int Count = 0;
        foreach (var Email in Result)
        {
            FinalEmails = GUBPNode.MergeSpaceStrings(FinalEmails, Email);
            Count++;
        }
        return FinalEmails;
    }
    public abstract class GUBPFrequencyHacker
    {
        public virtual int GetNodeFrequency(GUBP bp, string Branch, string NodeName, int BaseFrequency)
        {
            return new int();
        }
    }
    private static List<GUBPFrequencyHacker> FrequencyHackers;
    private int HackFrequency(GUBP bp, string Branch, string NodeName, int BaseFrequency)
    {
        int Frequency = BaseFrequency;
        if (FrequencyHackers == null)
        {
            FrequencyHackers = new List<GUBPFrequencyHacker>();
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
        }
        foreach(var FrequencyHacker in FrequencyHackers)
        {
           Frequency = FrequencyHacker.GetNodeFrequency(bp, Branch, NodeName, BaseFrequency);            
        }
        return Frequency;
    }    
    public abstract class GUBPNode
    {
        public List<string> FullNamesOfDependencies = new List<string>();
        public List<string> FullNamesOfPseudosependencies = new List<string>(); //these are really only used for sorting. We want the editor to fail before the monolithics. Think of it as "can't possibly be useful without".
		public List<string> FullNamesOfDependedOn = new List<string>();
        public List<string> BuildProducts = null;
		public List<string> DependentPromotions = new List<string>();
        public List<string> AllDependencyBuildProducts = null;
        public List<string> AllDependencies = null;		
		public List<string> CompletedDependencies = new List<string>();
        public string AgentSharingGroup = "";
        public int ComputedDependentCISFrequencyQuantumShift = -1;

        public virtual string GetFullName()
        {
            throw new AutomationException("Unimplemented GetFullName.");
        }
		public virtual string GetDisplayGroupName()
		{
			return GetFullName();
		}
        public virtual string GameNameIfAnyForTempStorage()
        {
            return "";
        }
        public virtual string RootIfAnyForTempStorage()
        {
            return "";
        }
        public virtual void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public virtual void PostLoadFromSharedTempStorage(GUBP bp)
        {
        }
        public virtual void DoFakeBuild(GUBP bp) // this is used to more rapidly test a build system, it does nothing but save a record of success as a build product
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public virtual bool IsSticky()
        {
            return false;
        }
        public virtual bool SendSuccessEmail()
        {
            return false;
        }
        public virtual bool RunInEC()
        {
            return true;
        }
		public virtual bool IsTest()
		{
			return false;
		}
        public virtual bool IsAggregate()
        {
            return false;
        }
		public virtual bool IsPromotableAggregate()
		{
			return false;
		}
		public virtual bool IsSeparatePromotable()
		{
			return false;
		}
		public virtual string NodeHostPlatform()
		{
			return "";
		}
        public virtual int AgentMemoryRequirement(GUBP bp)
        {
            return 0;
        }
        public virtual int TimeoutInMinutes()
        {
            return 90;
        }

        /// <summary>
        /// When triggered by CIS (instead of a person) this dictates how often this node runs.
        /// The builder has a increasing index, specified with -TimeIndex=N
        /// If N mod (1 lshift CISFrequencyQuantumShift()) is not 0, the node is skipped
        /// </summary>
        public virtual int CISFrequencyQuantumShift(GUBP bp)
        {
            return 0;
        }
        /// <summary>
        /// As above the maximum of all ancestors and pseudoancestors
        /// </summary>
        public int DependentCISFrequencyQuantumShift()
        {
            if (ComputedDependentCISFrequencyQuantumShift < 0)
            {
                throw new AutomationException("Asked for frequency shift before it was computed.");
            }
            return ComputedDependentCISFrequencyQuantumShift;
        }

        public virtual float Priority()
        {
            return 100.0f;
        }
        public virtual bool TriggerNode()
        {
            return false;
        }
        public virtual void SetAsExplicitTrigger()
        {

        }
        public virtual string ECAgentString()
        {
            return "";
        }
        public virtual string ECProcedureInfix()
        {
            return "";
        }
        public virtual string ECProcedure()
        {
            if (IsSticky() && AgentSharingGroup != "")
            {
                throw new AutomationException("Node {0} is both agent sharing and sitcky.", GetFullName());
            }
            return String.Format("GUBP{0}_UAT_Node{1}{2}", ECProcedureInfix(), IsSticky() ? "" : "_Parallel", AgentSharingGroup != "" ? "_AgentShare" : "");
        }
        public virtual string ECProcedureParams()
        {
            var Result = String.Format(", {{actualParameterName => 'Sticky', value => '{0}'}}", IsSticky() ? 1 : 0);
            if (AgentSharingGroup != "")
            {
                Result += String.Format(", {{actualParameterName => 'AgentSharingGroup', value => '{0}'}}", AgentSharingGroup);
            }
            return Result;
        }
        public static string MergeSpaceStrings(params string[] EmailLists)
        {
            var Emails = new List<string>();
            foreach (var EmailList in EmailLists)
            {
                if (!String.IsNullOrEmpty(EmailList))
                {
                    List<string> Parts = new List<string>(EmailList.Split(' '));
                    foreach (var Email in Parts)
                    {
                        if (!string.IsNullOrEmpty(Email) && !Emails.Contains(Email))
                        {
                            Emails.Add(Email);
                        }
                    }
                }
            }
            string Result = "";
            foreach (var Email in Emails)
            {
                if (Result != "")
                {
                    Result += " ";
                }
                Result += Email;
            }
            return Result;
        }

        public void SaveRecordOfSuccessAndAddToBuildProducts(string Contents = "Just a record of success.")
        {
            string RecordOfSuccess = CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Logs", GetFullName() + "_Success.log");
            CommandUtils.CreateDirectory(Path.GetDirectoryName(RecordOfSuccess));
            CommandUtils.WriteAllText(RecordOfSuccess, Contents);
            AddBuildProduct(RecordOfSuccess);
        }
        public void AddDependency(string Node)
        {
            if (!FullNamesOfDependencies.Contains(Node))
            {
                FullNamesOfDependencies.Add(Node);
            }
        }
        public void AddPseudodependency(string Node)
        {
            if (!FullNamesOfPseudosependencies.Contains(Node))
            {
                FullNamesOfPseudosependencies.Add(Node);
            }
        }
		public void AddCompletedDependency(string Node)
		{
			if(!CompletedDependencies.Contains(Node))
			{
				CompletedDependencies.Add(Node);
			}
		}
        public void RemovePseudodependency(string Node)
        {
            if (FullNamesOfPseudosependencies.Contains(Node))
            {
                FullNamesOfPseudosependencies.Remove(Node);
            }
        }
        public void AddBuildProduct(string Filename)
        {
            if (!CommandUtils.FileExists_NoExceptions(true, Filename))
            {
                throw new AutomationException("Cannot add build product {0} because it does not exist.", Filename);
            }
            FileInfo Info = new FileInfo(Filename);
            if (!BuildProducts.Contains(Info.FullName))
            {
                BuildProducts.Add(Info.FullName);
            }
        }
        public void AddDependentBuildProduct(string Filename)
        {
            if (!CommandUtils.FileExists_NoExceptions(true, Filename))
            {
                throw new AutomationException("Cannot add build product {0} because it does not exist.", Filename);
            }
            FileInfo Info = new FileInfo(Filename);
            if (!AllDependencyBuildProducts.Contains(Info.FullName))
            {
                AllDependencyBuildProducts.Add(Info.FullName);
            }
        }
        public void AddAllDependent(string Node)
        {
            if (!AllDependencies.Contains(Node))
            {
                AllDependencies.Add(Node);
            }
        }
        public void RemoveOveralppingBuildProducts()
        {
            foreach (var ToRemove in AllDependencyBuildProducts)
            {
                BuildProducts.Remove(ToRemove);
            }
        }

    }
    public class VersionFilesNode : GUBPNode
    {
        public static string StaticGetFullName()
        {
            return "VersionFiles";
        }
        public override string GetFullName()
        {
            return StaticGetFullName();
        }

        public override void DoBuild(GUBP bp)
        {
            var UE4Build = new UE4Build(bp);
            BuildProducts = UE4Build.UpdateVersionFiles(ActuallyUpdateVersionFiles: CommandUtils.P4Enabled && CommandUtils.AllowSubmit);
        }
        public override bool IsSticky()
        {
            return true;
        }
    }

    public class HostPlatformNode : GUBPNode
    {
        protected UnrealTargetPlatform HostPlatform;
        public HostPlatformNode(UnrealTargetPlatform InHostPlatform)
        {
            HostPlatform = InHostPlatform;
        }
		public override string GetDisplayGroupName()
		{
			string Name = GetFullName();
			string Suffix = GetHostPlatformSuffix();
			return Name.EndsWith(Suffix)? Name.Substring(0, Name.Length - Suffix.Length) : Name;
		}
        public static string StaticGetHostPlatformSuffix(UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform InAgentPlatform = UnrealTargetPlatform.Unknown)
        {
            if (InHostPlatform == UnrealTargetPlatform.Mac)
            {
                if (InAgentPlatform == UnrealTargetPlatform.Win64)
                {
                    return "_ForMac";
                }
                return "_OnMac";
            }
			else if (InHostPlatform == UnrealTargetPlatform.Linux)
			{
				return "_OnLinux";
			}
            return "";
        }
        public virtual UnrealTargetPlatform GetAgentPlatform()
        {
            return HostPlatform;
        }
        public override string ECProcedureInfix()
        {
            if (GetAgentPlatform() == UnrealTargetPlatform.Mac)
            {
                if (IsSticky())
                {
                    throw new AutomationException("Node {0} is sticky, but Mac hosted. Sticky nodes must be PC hosted.", GetFullName());
                }
                return "_Mac";
            }
			if(GetAgentPlatform() == UnrealTargetPlatform.Linux)
			{
				if(IsSticky())
				{
					throw new AutomationException("Node {0} is sticky, but Linux hosted. Sticky nodes must be PC hosted.", GetFullName());
				}
				return "_Linux";
			}
            return "";
        }
        public virtual string GetHostPlatformSuffix()
        {
            return StaticGetHostPlatformSuffix(HostPlatform, GetAgentPlatform());
        }
        public UnrealTargetPlatform GetAltHostPlatform()
        {
            return GUBP.GetAltHostPlatform(HostPlatform);
        }
        public override int TimeoutInMinutes()
        {
            return base.TimeoutInMinutes() + ((HostPlatform == UnrealTargetPlatform.Mac) ? 30 : 0); // Mac is slower and more resource constrained
        }
    }

    public class CompileNode : HostPlatformNode
    {
        public CompileNode(UnrealTargetPlatform InHostPlatform, bool DependentOnCompileTools = true)
            : base(InHostPlatform)
        {
            if (DependentOnCompileTools)
            {
                AddDependency(ToolsForCompileNode.StaticGetFullName(HostPlatform));
            }
            else
            {
                AddDependency(VersionFilesNode.StaticGetFullName());
            }
        }
        public virtual UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            return null;
        }
        public virtual void PostBuild(GUBP bp, UE4Build UE4Build)
        {
        }
        public virtual void PostBuildProducts(GUBP bp)
        {
        }
        public virtual bool DeleteBuildProducts()
        {
            return false;
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            var UE4Build = new UE4Build(bp);
            UE4Build.BuildAgenda Agenda = GetAgenda(bp);
            if (Agenda != null)
            {
                bool ReallyDeleteBuildProducts = DeleteBuildProducts() && !GUBP.bForceIncrementalCompile;
                Agenda.DoRetries = false; // these would delete build products
                UE4Build.Build(Agenda, InDeleteBuildProducts: ReallyDeleteBuildProducts, InUpdateVersionFiles: false, InForceUnity: true);
				
                PostBuild(bp, UE4Build);

                UE4Build.CheckBuildProducts(UE4Build.BuildProductFiles);
                foreach (var Product in UE4Build.BuildProductFiles)
                {
                    AddBuildProduct(Product);
                }
                RemoveOveralppingBuildProducts();
                if (bp.bSignBuildProducts)
                {
                    // Sign everything we built
                    CodeSign.SignMultipleIfEXEOrDLL(bp, BuildProducts);
                }
                PostBuildProducts(bp);
            }
            if (Agenda == null || (BuildProducts.Count == 0 && GUBP.bForceIncrementalCompile))
            {
                SaveRecordOfSuccessAndAddToBuildProducts("Nothing to actually compile");
            }
        }
        public override int TimeoutInMinutes()
        {
            return base.TimeoutInMinutes() + ((HostPlatform == UnrealTargetPlatform.Mac) ? 30 : 0);
        }
    }

    public class ToolsForCompileNode : CompileNode
    {
        public ToolsForCompileNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform, false)
        {
            AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "ToolsForCompile" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
        public override int AgentMemoryRequirement(GUBP bp)
        {
            if (bp.ParseParam("Launcher") || bp.TimeIndex != 0  && HostPlatform != UnrealTargetPlatform.Mac)
            {
                return base.AgentMemoryRequirement(bp);
            }
            return 32;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();
			if (HostPlatform == UnrealTargetPlatform.Win64 && !GUBP.bForceIncrementalCompile)
            {
                Agenda.DotNetProjects.AddRange(
                    new string[] 
			    {
				    @"Engine\Source\Programs\DotNETCommon\DotNETUtilities\DotNETUtilities.csproj",
                    @"Engine\Source\Programs\RPCUtility\RPCUtility.csproj",
			    }
                    );
            }
            string AddArgs = "-CopyAppBundleBackToDevice";

            Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
            return Agenda;
        }
        public override void PostBuild(GUBP bp, UE4Build UE4Build)
        {
			if (HostPlatform == UnrealTargetPlatform.Win64)
            {
            	UE4Build.AddUATFilesToBuildProducts();
            	UE4Build.AddUBTFilesToBuildProducts();
        	}
    	}
    }

    public class RootEditorNode : CompileNode
    {
        public RootEditorNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "RootEditor" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
		public override void DoBuild(GUBP bp)
		{
			base.DoBuild(bp);

			if(!bp.BranchOptions.bNoInstalledEngine)
			{
				FileFilter Filter = new FileFilter();
				Filter.Include("/Engine/Intermediate/Build/" + HostPlatform.ToString() + "/UE4Editor/Inc/...");
				Filter.Include("/Engine/Plugins/.../Intermediate/Build/" + HostPlatform.ToString() + "/UE4Editor/Inc/...");

				string ZipFileName = StaticGetArchivedHeadersPath(HostPlatform);
				CommandUtils.ZipFiles(ZipFileName, CommandUtils.CmdEnv.LocalRoot, Filter);
				BuildProducts.Add(ZipFileName);
			}
		}
		public static string StaticGetArchivedHeadersPath(UnrealTargetPlatform HostPlatform)
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Precompiled", "Headers-RootEditor" + StaticGetHostPlatformSuffix(HostPlatform) + ".zip");
		}
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string AddArgs = "-nobuilduht -precompile";
            if (bp.bOrthogonalizeEditorPlatforms)
            {
                AddArgs += " -skipnonhostplatforms";
            }
            Agenda.AddTargets(
                new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
                HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(HostPlatform))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
                }
            }
            return Agenda;
        }
        void DeleteStaleDLLs(GUBP bp)
        {
            if (GUBP.bForceIncrementalCompile)
            {
                return;
            }
            var Targets = new List<string>{bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName};
            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(HostPlatform))
                {
                    Targets.Add(ProgramTarget.TargetName);
                }
            }


            foreach (var Target in Targets)
            {
                var EnginePlatformBinaries = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", HostPlatform.ToString());
                var Wildcard = Target + "-*";
                Log("************Deleting stale editor DLLs, path {0} wildcard {1}", EnginePlatformBinaries, Wildcard);
                foreach (var DiskFile in FindFiles(Wildcard, true, EnginePlatformBinaries))
                {
                    bool IsBuildProduct = false;
                    foreach (var Product in BuildProducts)
                    {
                        if (Product.Equals(DiskFile, StringComparison.InvariantCultureIgnoreCase))
                        {
                            IsBuildProduct = true;
                            break;
                        }
                    }
                    if (!IsBuildProduct)
                    {
                        DeleteFile(DiskFile);
                    }
                }
                var EnginePluginBinaries = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Plugins");
                var HostSubstring = CommandUtils.CombinePaths("/", HostPlatform.ToString(), "/");
                Log("************Deleting stale editor DLLs, path {0} wildcard {1} host {2}", EnginePluginBinaries, Wildcard, HostSubstring);
                foreach (var DiskFile in FindFiles(Wildcard, true, EnginePluginBinaries))
                {
                    if (DiskFile.IndexOf(HostSubstring, StringComparison.InvariantCultureIgnoreCase) < 0)
                    {
                        continue;
                    }
                    bool IsBuildProduct = false;
                    foreach (var Product in BuildProducts)
                    {
                        if (Product.Equals(DiskFile, StringComparison.InvariantCultureIgnoreCase))
                        {
                            IsBuildProduct = true;
                            break;
                        }
                    }
                    if (!IsBuildProduct)
                    {
                        DeleteFile(DiskFile);
                    }
                }
            }
        }
        public override void PostLoadFromSharedTempStorage(GUBP bp)
        {
            DeleteStaleDLLs(bp);
        }
        public override void PostBuildProducts(GUBP bp)
        {
            DeleteStaleDLLs(bp);
        }
    }
	public class RootEditorCrossCompileLinuxNode : CompileNode
	{
		public RootEditorCrossCompileLinuxNode(UnrealTargetPlatform InHostPlatform)
			: base(InHostPlatform)
		{
			AddDependency(RootEditorNode.StaticGetFullName(UnrealTargetPlatform.Win64));
			AddDependency(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
		}
		public static string StaticGetFullName()
		{
			return "RootEditor_Linux";
		}
		public override string GetFullName()
		{
			return StaticGetFullName();
		}
		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 3;
		}
		public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
		{
			var Agenda = new UE4Build.BuildAgenda();

			string AddArgs = "-nobuilduht";
			Agenda.AddTargets(
				new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
				UnrealTargetPlatform.Linux, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
			 foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(UnrealTargetPlatform.Linux))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, UnrealTargetPlatform.Linux, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
                }
            }
			return Agenda;
		}
        void DeleteStaleDLLs(GUBP bp)
        {
            if (GUBP.bForceIncrementalCompile)
            {
                return;
            }
            var Targets = new List<string> { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName };
            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(UnrealTargetPlatform.Linux))
                {
                    Targets.Add(ProgramTarget.TargetName);
                }
            }


            foreach (var Target in Targets)
            {
                var EnginePlatformBinaries = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Binaries", UnrealTargetPlatform.Linux.ToString());
                var Wildcard = Target + "-*";
                Log("************Deleting stale editor DLLs, path {0} wildcard {1}", EnginePlatformBinaries, Wildcard);
                foreach (var DiskFile in FindFiles(Wildcard, true, EnginePlatformBinaries))
                {
                    bool IsBuildProduct = false;
                    foreach (var Product in BuildProducts)
                    {
                        if (Product.Equals(DiskFile, StringComparison.InvariantCultureIgnoreCase))
                        {
                            IsBuildProduct = true;
                            break;
                        }
                    }
                    if (!IsBuildProduct)
                    {
                        DeleteFile(DiskFile);
                    }
                }
                var EnginePluginBinaries = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Plugins");
                var HostSubstring = CommandUtils.CombinePaths("/", UnrealTargetPlatform.Linux.ToString(), "/");
                Log("************Deleting stale editor DLLs, path {0} wildcard {1} host {2}", EnginePluginBinaries, Wildcard, HostSubstring);
                foreach (var DiskFile in FindFiles(Wildcard, true, EnginePluginBinaries))
                {
                    if (DiskFile.IndexOf(HostSubstring, StringComparison.InvariantCultureIgnoreCase) < 0)
                    {
                        continue;
                    }
                    bool IsBuildProduct = false;
                    foreach (var Product in BuildProducts)
                    {
                        if (Product.Equals(DiskFile, StringComparison.InvariantCultureIgnoreCase))
                        {
                            IsBuildProduct = true;
                            break;
                        }
                    }
                    if (!IsBuildProduct)
                    {
                        DeleteFile(DiskFile);
                    }
                }
            }
        }
        public override void PostLoadFromSharedTempStorage(GUBP bp)
        {
            DeleteStaleDLLs(bp);
        }
        public override void PostBuildProducts(GUBP bp)
        {
            DeleteStaleDLLs(bp);        
		}
	}
    public class ToolsNode : CompileNode
    {
        public ToolsNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
            AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(HostPlatform);
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "Tools" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override float Priority()
        {
            return base.Priority() - 1;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

			if (HostPlatform == UnrealTargetPlatform.Win64)
            {
				if (!GUBP.bForceIncrementalCompile)
				{
					Agenda.DotNetProjects.AddRange(
						new string[] 
						{
							CombinePaths(@"Engine\Source\Programs\UnrealControls\UnrealControls.csproj"),
						}
					);
				}
                Agenda.DotNetSolutions.AddRange(
						new string[] 
						{
							CombinePaths(@"Engine\Source\Programs\NetworkProfiler\NetworkProfiler.sln"),   
						}
                    );
                if (!GUBP.bForceIncrementalCompile)
                {
					Agenda.SwarmProject = CombinePaths(@"Engine\Source\Programs\UnrealSwarm\UnrealSwarm.sln");
				}
				
				bool WithIOS = !bp.BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.IOS);
				if ( WithIOS )
				{
					Agenda.IOSDotNetProjects.AddRange(
                        new string[]
						{
							CombinePaths(@"Engine\Source\Programs\IOS\iPhonePackager\iPhonePackager.csproj"),
							CombinePaths(@"Engine\Source\Programs\IOS\DeploymentServer\DeploymentServer.csproj"),
							CombinePaths(@"Engine\Source\Programs\IOS\MobileDeviceInterface\MobileDeviceInterface.csproj"),
							CombinePaths(@"Engine\Source\Programs\IOS\DeploymentInterface\DeploymentInterface.csproj"),
						}
                    );
				}

				bool WithHTML5 = !bp.BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.HTML5);
				if (WithHTML5)
				{
					Agenda.HTML5DotNetProjects.AddRange(
						new string[]
						{
							CombinePaths(@"Engine\Source\Programs\HTML5\HTML5LaunchHelper\HTML5LaunchHelper.csproj"),
						}
					);
				}
            }

            string AddArgs = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                bool bInternalOnly;
                bool SeparateNode;
				bool CrossCompile;
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && !bInternalOnly && !SeparateNode)
                {
                    foreach (var Plat in ProgramTarget.Rules.GUBP_ToolPlatforms(HostPlatform))
                    {
                        foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
                        {
                            Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, Plat, Config, InAddArgs: AddArgs);
                        }
                    }
                }
            }

            return Agenda;
        }
    }
	public class ToolsCrossCompileNode : CompileNode
	{
		public ToolsCrossCompileNode(UnrealTargetPlatform InHostPlatform)
			: base(InHostPlatform)
		{
			AddPseudodependency(RootEditorCrossCompileLinuxNode.StaticGetFullName());
			AgentSharingGroup = "ToolsCrossCompileGroup" + StaticGetHostPlatformSuffix(HostPlatform);
		}
		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
		{
			return "LinuxTools" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}
		public override float Priority()
		{
			return base.Priority() - 1;
		}
		public override bool DeleteBuildProducts()
		{
			return true;
		}
		public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
		{
			var Agenda = new UE4Build.BuildAgenda();

			string AddArgs = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

			foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
			{
				bool bInternalOnly;
				bool SeparateNode;
				bool CrossCompile;
				if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && !bInternalOnly && !SeparateNode && CrossCompile)
				{
					foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
					{
						Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, UnrealTargetPlatform.Linux, Config, InAddArgs: AddArgs);
					}					
				}
			}

			return Agenda;
		}
	}
    public class SingleToolsNode : CompileNode
    {
        SingleTargetProperties ProgramTarget;
        public SingleToolsNode(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
            : base(InHostPlatform)
        {
            ProgramTarget = InProgramTarget;
            AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
            AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(HostPlatform);
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
        {
            return "Tools_" + InProgramTarget.TargetName + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, ProgramTarget);
        }
        public override float Priority()
        {
            return base.Priority() + 2;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string AddArgs = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

            foreach (var Plat in ProgramTarget.Rules.GUBP_ToolPlatforms(HostPlatform))
            {
                foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, Plat, Config, InAddArgs: AddArgs);
                }
            }
            return Agenda;
        }
    }

    public class InternalToolsNode : CompileNode
    {
        public InternalToolsNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
            AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(HostPlatform);
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "InternalTools" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override float Priority()
        {
            return base.Priority() - 2;
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            int Result = base.CISFrequencyQuantumShift(bp) + 1;
            return Result;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            bool bAnyAdded = false;
            var Agenda = new UE4Build.BuildAgenda();

			if (HostPlatform == UnrealTargetPlatform.Win64)
            {
                bAnyAdded = true;
                Agenda.DotNetProjects.AddRange(
                    new string[] 
			    {
                    CombinePaths(@"Engine\Source\Programs\NotForLicensees\CrashReportServer\CrashReportCommon\CrashReportCommon.csproj"),
					CombinePaths(@"Engine\Source\Programs\NotForLicensees\CrashReportServer\CrashReportReceiver\CrashReportReceiver.csproj"),
					CombinePaths(@"Engine\Source\Programs\NotForLicensees\CrashReportServer\CrashReportProcess\CrashReportProcess.csproj"),
                    CombinePaths(@"Engine\Source\Programs\CrashReporter\RegisterPII\RegisterPII.csproj"),
			    });
                Agenda.DotNetSolutions.AddRange(
                    new string[] 
			        {
				        CombinePaths(@"Engine\Source\Programs\UnrealDocTool\UnrealDocTool\UnrealDocTool.sln"),
			        }
                    );
                Agenda.ExtraDotNetFiles.AddRange(
                    new string[] 
			        {
				        "Interop.IWshRuntimeLibrary",
				        "UnrealMarkdown",
				        "CommonUnrealMarkdown",
			        }
                    );
            }
            string AddArgs = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                bool bInternalOnly;
                bool SeparateNode;
				bool CrossCompile;
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && bInternalOnly && !SeparateNode)
                {
                    foreach (var Plat in ProgramTarget.Rules.GUBP_ToolPlatforms(HostPlatform))
                    {
                        foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
                        {
                            Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, Plat, Config, InAddArgs: AddArgs);
                            bAnyAdded = true;
                        }
                    }
                }
            }
            if (bAnyAdded)
            {
                return Agenda;
            }
            return null;
        }
    }

    public class SingleInternalToolsNode : CompileNode
    {
        SingleTargetProperties ProgramTarget;
        public SingleInternalToolsNode(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
            : base(InHostPlatform)
        {
			SetupSingleInternalToolsNode(InProgramTarget);
        }
		public SingleInternalToolsNode(GUBP bp, UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
            : base(InHostPlatform)
        {
			// Don't add rooteditor dependency if it isn't in the graph
			var bRootEditorNodeDoesExit = bp.HasNode(RootEditorNode.StaticGetFullName(HostPlatform));
			SetupSingleInternalToolsNode(InProgramTarget, !bRootEditorNodeDoesExit);
        }
		private void SetupSingleInternalToolsNode(SingleTargetProperties InProgramTarget, bool bSkipRootEditorPsuedoDependency = true)
		{
			ProgramTarget = InProgramTarget;
			AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(HostPlatform);
			
			if (!bSkipRootEditorPsuedoDependency) 
			{
				AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
			}
		}
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
        {
            return "InternalTools_" + InProgramTarget.TargetName + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, ProgramTarget);
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            int Result = base.CISFrequencyQuantumShift(bp) + 1;                             
            return Result;
        }
        public override float Priority()
        {
            return base.Priority() + 3;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string AddArgs = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

            foreach (var Plat in ProgramTarget.Rules.GUBP_ToolPlatforms(HostPlatform))
            {
                foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, Plat, Config, InAddArgs: AddArgs);
                }
            }
            return Agenda;
        }
    }


    public class EditorPlatformNode : CompileNode
    {
        UnrealTargetPlatform EditorPlatform;

        public EditorPlatformNode(UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform Plat)
            : base(InHostPlatform)
        {
            AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(InHostPlatform);
            EditorPlatform = Plat;
            AddDependency(RootEditorNode.StaticGetFullName(InHostPlatform));
        }

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform Plat)
        {
            return Plat.ToString() + "_EditorPlatform" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, EditorPlatform);
        }
        public override float Priority()
        {
            return base.Priority() + 1;
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            int Result = base.CISFrequencyQuantumShift(bp);            
            return Result;
        }

        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            if (!bp.bOrthogonalizeEditorPlatforms)
            {
                throw new AutomationException("EditorPlatformNode node should not be used unless we are orthogonalizing editor platforms.");
            }
            var Agenda = new UE4Build.BuildAgenda();

            Agenda.AddTargets(
                new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
                HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice -onlyplatformspecificfor=" + EditorPlatform.ToString());
            foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && ProgramTarget.Rules.GUBP_NeedsPlatformSpecificDLLs())
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice -onlyplatformspecificfor=" + EditorPlatform.ToString());
                }
            }
            return Agenda;
        }

    }

    public class EditorGameNode : CompileNode
    {
		List<BranchInfo.BranchUProject> GameProjects = new List<BranchInfo.BranchUProject>();

        public EditorGameNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj)
            : base(InHostPlatform)
        {
            AgentSharingGroup = "Editor"  + StaticGetHostPlatformSuffix(InHostPlatform);
            GameProjects.Add(InGameProj);
			AddDependency(RootEditorNode.StaticGetFullName(InHostPlatform));						
		}
		public void AddProject(BranchInfo.BranchUProject InGameProj)
		{
			if(InGameProj.Options(HostPlatform).GroupName != GameProjects[0].Options(HostPlatform).GroupName)
			{
				throw new AutomationException("Attempt to merge projects with different group names");
			}
			GameProjects.Add(InGameProj);
		}
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj)
        {
            return (InGameProj.Options(InHostPlatform).GroupName ?? InGameProj.GameName) + "_EditorGame" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProjects[0]);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProjects[0].Options(HostPlatform).GroupName ?? GameProjects[0].GameName;
        }		
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string Args = "-nobuilduht -skipactionhistory -skipnonhostplatforms -CopyAppBundleBackToDevice -forceheadergeneration -precompile";

			foreach(BranchInfo.BranchUProject GameProj in GameProjects)
			{
				Agenda.AddTargets(
					new string[] { GameProj.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
					HostPlatform, UnrealTargetConfiguration.Development, GameProj.FilePath, InAddArgs: Args);
			}

            return Agenda;
        }
    }

	public class MakeFeaturePacksNode : HostPlatformNode
	{
		List<BranchInfo.BranchUProject> Projects;

        public MakeFeaturePacksNode(UnrealTargetPlatform InHostPlatform, IEnumerable<BranchInfo.BranchUProject> InProjects)
            : base(InHostPlatform)
        {
			Projects = new List<BranchInfo.BranchUProject>(InProjects);
			AddDependency(ToolsNode.StaticGetFullName(InHostPlatform)); // for UnrealPak
			AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(HostPlatform);
        }

		public static string GetOutputFile(BranchInfo.BranchUProject Project)
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "FeaturePacks", Path.GetFileNameWithoutExtension(Project.GameName) + ".upack");
		}

		public static bool IsFeaturePack(BranchInfo.BranchUProject InGameProj)
		{
			// No obvious way to store this in the project options; it's a property of non-code projects too.
			if(InGameProj.GameName == "StarterContent" || InGameProj.GameName == "MobileStarterContent" || InGameProj.GameName.StartsWith("FP_"))
			{
				return true;
			}
			if(InGameProj.GameName.StartsWith("TP_"))
			{
				return CommandUtils.FileExists(CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(InGameProj.FilePath), "contents.txt"));
			}
			return false;
		}

		public static UnrealTargetPlatform GetDefaultBuildPlatform(GUBP bp)
		{
			if(bp.HostPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				return UnrealTargetPlatform.Win64;
			}
			else if(bp.HostPlatforms.Contains(UnrealTargetPlatform.Mac))
			{
				return UnrealTargetPlatform.Mac;
			}
			else
			{
				if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux && bp.HostPlatforms[0] != UnrealTargetPlatform.Linux)
				{
					throw new AutomationException("Linux is not (yet?) able to cross-compile nodes for platform {0}, did you forget -NoPC / -NoMac?", bp.HostPlatforms[0]);
				}

				return bp.HostPlatforms[0];
			}
		}

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "MakeFeaturePacks" + StaticGetHostPlatformSuffix(InHostPlatform);
        }

		public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }

		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 2;
		}
		public override void DoBuild(GUBP bp)
        {
			BuildProducts = new List<string>();
			foreach(BranchInfo.BranchUProject Project in Projects)
			{
				string ContentsFileName = CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(Project.FilePath), "contents.txt");

				// Make sure we delete the output file. It may be read-only.
				string OutputFileName = GetOutputFile(Project);
				CommandUtils.DeleteFile(OutputFileName);

				// Get the command line
				string CmdLine = CommandUtils.MakePathSafeToUseWithCommandLine(OutputFileName) + " " + CommandUtils.MakePathSafeToUseWithCommandLine("-create=" + ContentsFileName);
				if (GlobalCommandLine.Installed)
				{
					CmdLine += " -installed";
				}
				if (GlobalCommandLine.UTF8Output)
				{
					CmdLine += " -UTF8Output";
				}

				// Run UnrealPak
				string UnrealPakExe;
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");
				}
				else
				{
					throw new AutomationException("Unknown path to UnrealPak for host platform ({0})", HostPlatform);
				}
				RunAndLog(CmdEnv, UnrealPakExe, CmdLine, Options: ERunOptions.Default | ERunOptions.AllowSpew | ERunOptions.UTF8Output);

				// Add the build products
				BuildProducts.Add(OutputFileName);
			}
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}

    public class GamePlatformMonolithicsNode : CompileNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;
		bool WithXp;
		bool Precompiled; // If true, just builds targets which generate static libraries for the -UsePrecompiled option to UBT. If false, just build those that don't.

        public GamePlatformMonolithicsNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, bool InWithXp = false, bool InPrecompiled = false)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;
			WithXp = InWithXp;
			Precompiled = InPrecompiled;

            if (TargetPlatform == UnrealTargetPlatform.PS4 || TargetPlatform == UnrealTargetPlatform.XboxOne)
            {
				// Required for PS4MapFileUtil/XboxOnePDBFileUtil
				AddDependency(ToolsNode.StaticGetFullName(InHostPlatform));
            }

            if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
				if (!bp.BranchOptions.ExcludePlatformsForEditor.Contains(InHostPlatform))
				{
					AddPseudodependency(EditorGameNode.StaticGetFullName(InHostPlatform, GameProj));
				}
                if (bp.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform)))
                {
                    AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform));
                }
            }
            else
            {
                if (TargetPlatform != InHostPlatform && bp.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(InHostPlatform, bp.Branch.BaseEngineProject, InHostPlatform, Precompiled: Precompiled)))
                {
                    AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(InHostPlatform, bp.Branch.BaseEngineProject, InHostPlatform, Precompiled: Precompiled));
                }
            }
            if (InGameProj.Options(InHostPlatform).bTestWithShared)  /// compiling templates is only for testing purposes, and we will group them to avoid saturating the farm
            {
                AddPseudodependency(WaitForTestShared.StaticGetFullName());
                AgentSharingGroup = "TemplateMonolithics" + StaticGetHostPlatformSuffix(InHostPlatform);
            }
        }

		public override string GetDisplayGroupName()
		{
			return GameProj.GameName + "_Monolithics" + (Precompiled? "_Precompiled" : "");
		}

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, bool WithXp = false, bool Precompiled = false)
        {
			string Name = InGameProj.GameName;
			if(WithXp)
			{
				Name += "_WinXP_Mono";
			}
			else
			{
				Name += "_" + InTargetPlatform + "_Mono";
			}
			if(Precompiled)
			{
				Name += "_Precompiled";
			}
			return Name + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, TargetPlatform, WithXp, Precompiled);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }  

        public override int CISFrequencyQuantumShift(GUBP bp)
        {            
            int Result = base.CISFrequencyQuantumShift(bp);
            if(GameProj.GameName != bp.Branch.BaseEngineProject.GameName || !Precompiled)
            {
                Result += 3; //only every 80m
            }
            else if (TargetPlatform != HostPlatform)
            {
                Result += 2; //only every 40m
            }                 
            return Result;
        }

		public static bool HasPrecompiledTargets(BranchInfo.BranchUProject Project, UnrealTargetPlatform HostPlatform, UnrealTargetPlatform TargetPlatform)
		{
            foreach(TargetRules.TargetType Kind in BranchInfo.MonolithicKinds)
            {
                if (Project.Properties.Targets.ContainsKey(Kind))
                {
                    SingleTargetProperties Target = Project.Properties.Targets[Kind];
					if(Target.Rules.GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(HostPlatform, TargetPlatform).Any())
					{
						return true;
					}
				}
			}
			return false;
		}

		public override float Priority()
		{
			float Result = base.Priority();
			if(Precompiled)
			{
				Result += 1.0f;
			}
			return Result;
		}

		public override void DoBuild(GUBP bp)
		{
			base.DoBuild(bp);

			if(Precompiled)
			{
				// Get a list of all the build dependencies
				UE4Build.BuildAgenda Agenda = GetAgenda(bp);
				string FileListPath = new UE4Build(bp).GenerateExternalFileList(Agenda);
				UnrealBuildTool.ExternalFileList FileList = UnrealBuildTool.Utils.ReadClass<UnrealBuildTool.ExternalFileList>(FileListPath);

				// Make all the paths relative to the root
				string FilterPrefix = CommandUtils.CombinePaths(PathSeparator.Slash, CommandUtils.CmdEnv.LocalRoot).TrimEnd('/') + "/";
				for(int Idx = 0; Idx < FileList.FileNames.Count; Idx++)
				{
					if(FileList.FileNames[Idx].StartsWith(FilterPrefix, StringComparison.InvariantCultureIgnoreCase))
					{
						FileList.FileNames[Idx] = FileList.FileNames[Idx].Substring(FilterPrefix.Length);
					}
					else
					{
						CommandUtils.LogError("Referenced external file is not under local root: {0}", FileList.FileNames[Idx]);
					}
				}

				// Write the resulting file list out to disk
				string OutputFileListPath = StaticGetBuildDependenciesPath(HostPlatform, GameProj, TargetPlatform);
				UnrealBuildTool.Utils.WriteClass<UnrealBuildTool.ExternalFileList>(FileList, OutputFileListPath, "");
				AddBuildProduct(OutputFileListPath);

				// Archive all the headers
				FileFilter Filter = new FileFilter();
				Filter.Include("/Engine/Intermediate/Build/" + TargetPlatform.ToString() + "/UE4/Inc/...");
				Filter.Include("/Engine/Plugins/.../Intermediate/Build/" + TargetPlatform.ToString() + "/UE4/Inc/...");

				string ZipFileName = StaticGetArchivedHeadersPath(HostPlatform, GameProj, TargetPlatform);
				CommandUtils.ZipFiles(ZipFileName, CommandUtils.CmdEnv.LocalRoot, Filter);
				BuildProducts.Add(ZipFileName);
			}
		}

        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            if (!bp.ActivePlatforms.Contains(TargetPlatform))
            {
                throw new AutomationException("{0} is not a supported platform for {1}", TargetPlatform.ToString(), GetFullName());
            }
            var Agenda = new UE4Build.BuildAgenda();

            string Args = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

			if(Precompiled)
			{
				Args += " -precompile";

				// MSVC doesn't provide a way to strip symbols from static libraries - you have to use PDBs, but that causes random OOM 
				// exceptions with the /FS arg because mspdbsrv is 32-bit. Just disable compiler debug info manually for now.
				if(TargetPlatform == UnrealTargetPlatform.Win32 || TargetPlatform == UnrealTargetPlatform.Win64)
				{
	                Args += " -nodebuginfo";
				}
			}

			if (WithXp)
			{
				Args += " -winxp";
			}

            foreach (var Kind in BranchInfo.MonolithicKinds)
            {
                if (GameProj.Properties.Targets.ContainsKey(Kind))
                {
                    var Target = GameProj.Properties.Targets[Kind];
					var AllowXp = Target.Rules.GUBP_BuildWindowsXPMonolithics();
					if (!WithXp || (AllowXp && WithXp))
					{
						var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
						var AdditionalPlatforms = Target.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform);
						var AllPlatforms = Platforms.Union(AdditionalPlatforms);
						if (AllPlatforms.Contains(TargetPlatform) && Target.Rules.SupportsPlatform(TargetPlatform))
						{
							List<UnrealTargetConfiguration> Configs;
							if(Precompiled)
							{
								Configs = Target.Rules.GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(HostPlatform, TargetPlatform);
							}
							else
							{
								Configs = Target.Rules.GUBP_GetConfigs_MonolithicOnly(HostPlatform, TargetPlatform).Except(Target.Rules.GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(HostPlatform, TargetPlatform)).ToList();
							}
							
							foreach (var Config in Configs)
							{
								if (GameProj.GameName == bp.Branch.BaseEngineProject.GameName)
								{
									Agenda.AddTargets(new string[] { Target.TargetName }, TargetPlatform, Config, InAddArgs: Args);
								}
								else
								{
									Agenda.AddTargets(new string[] { Target.TargetName }, TargetPlatform, Config, GameProj.FilePath, InAddArgs: Args);
								}
							}
						}
					}
				}
            }

            return Agenda;
        }

		public static string StaticGetArchivedHeadersPath(UnrealTargetPlatform HostPlatform, BranchInfo.BranchUProject GameProj, UnrealTargetPlatform TargetPlatform)
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Precompiled", "Headers-" + StaticGetFullName(HostPlatform, GameProj, TargetPlatform) + ".zip");
		}

		public static string StaticGetBuildDependenciesPath(UnrealTargetPlatform HostPlatform, BranchInfo.BranchUProject GameProj, UnrealTargetPlatform TargetPlatform)
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Precompiled", "BuildDependencies-" + StaticGetFullName(HostPlatform, GameProj, TargetPlatform) + ".xml");
		}
    }

    public class SuccessNode : GUBPNode
    {
        public SuccessNode()
        {
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public override bool SendSuccessEmail()
        {
            return true;
        }
    }

    public class GeneralSuccessNode : GUBP.SuccessNode
    {
        string MyName;
        public GeneralSuccessNode(string InMyName)
        {
            MyName = InMyName;
        }
        public static string StaticGetFullName(string InMyName)
        {
            return InMyName + "_Success";
        }
        public override string GetFullName()
        {
            return StaticGetFullName(MyName);
        }
    }

    public class AggregateNode : GUBPNode
    {
        public AggregateNode()
        {
        }
        public override bool RunInEC()
        {
            return false;
        }		
        public override bool IsAggregate()
        {
            return true;
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
        }
        public override void DoFakeBuild(GUBP bp) // this is used to more rapidly test a build system, it does nothing but save a record of success as a build product
        {
            BuildProducts = new List<string>();
        }
    }

    public class HostPlatformAggregateNode : AggregateNode
    {
        protected UnrealTargetPlatform HostPlatform;
        public HostPlatformAggregateNode(UnrealTargetPlatform InHostPlatform)
        {
            HostPlatform = InHostPlatform;
        }
        public static string StaticGetHostPlatformSuffix(UnrealTargetPlatform InHostPlatform)
        {
            return HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public virtual string GetHostPlatformSuffix()
        {
            return StaticGetHostPlatformSuffix(HostPlatform);
        }
        public UnrealTargetPlatform GetAltHostPlatform()
        {
            return GUBP.GetAltHostPlatform(HostPlatform);
        }
    }

    public class EditorAndToolsNode : HostPlatformAggregateNode
    {
        public EditorAndToolsNode(GUBP bp, UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AddDependency(RootEditorNode.StaticGetFullName(HostPlatform));
            AddDependency(ToolsNode.StaticGetFullName(HostPlatform));
            AddDependency(InternalToolsNode.StaticGetFullName(HostPlatform));
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "BaseEditorAndTools" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
    }


    public class AggregatePromotableNode : AggregateNode
    {
        protected List<UnrealTargetPlatform> HostPlatforms;
        string PromotionLabelPrefix;		

        public AggregatePromotableNode(List<UnrealTargetPlatform> InHostPlatforms, string InPromotionLabelPrefix)
        {
            HostPlatforms = InHostPlatforms;
            foreach (var HostPlatform in HostPlatforms)
            {
                AddDependency(EditorAndToolsNode.StaticGetFullName(HostPlatform));
            }
            PromotionLabelPrefix = InPromotionLabelPrefix; 
        }
        public static string StaticGetFullName(string InPromotionLabelPrefix)
        {
            return InPromotionLabelPrefix + "_Promotable_Aggregate";
        }
		public override bool IsPromotableAggregate()
		{
			return true;
		}
        public override string GetFullName()
        {
            return StaticGetFullName(PromotionLabelPrefix);
        }
    }

	public class SharedCookAggregateNode : AggregateNode
	{
		public SharedCookAggregateNode(GUBP bp, List<UnrealTargetPlatform> InHostPlatforms, Dictionary<string, List<UnrealTargetPlatform>> NonCodeProjectNames, Dictionary<string, List<TargetRules.GUBPFormalBuild>> NonCodeFormalBuilds)
		{			
			foreach (var HostPlatform in InHostPlatforms)
			{
				{
					var Options = bp.Branch.BaseEngineProject.Options(HostPlatform);
					if (Options.bIsPromotable && !Options.bSeparateGamePromotion)
					{
						var Kind = TargetRules.TargetType.Game;						
						if (bp.Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
						{
							var Target = bp.Branch.BaseEngineProject.Properties.Targets[Kind];
							var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
							foreach (var Plat in Platforms)
							{
								if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
								{
									continue;
								}
								if (bp.ActivePlatforms.Contains(Plat))
								{
									string CookedPlatform = Platform.Platforms[Plat].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");										
									
									foreach (var NonCodeProject in bp.Branch.NonCodeProjects)
									{										
										if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName) || !NonCodeProjectNames[NonCodeProject.GameName].Contains(Plat))
										{
											continue;
										}
										if(NonCodeFormalBuilds.ContainsKey(NonCodeProject.GameName))
										{
											var PlatList = NonCodeFormalBuilds[NonCodeProject.GameName];
											foreach (var PlatPair in PlatList)
											{
												if(PlatPair.TargetPlatform == Plat)
												{													
													AddCompletedDependency(CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform));
												}
											}
										}
									}
								}
							}								
						}						
					}					
				}
				foreach (var CodeProj in bp.Branch.CodeProjects)
				{
					var Options = CodeProj.Options(HostPlatform);
					if (!Options.bSeparateGamePromotion)
					{
						if (Options.bIsPromotable)
						{
							foreach (var Kind in BranchInfo.MonolithicKinds)
							{
								if (CodeProj.Properties.Targets.ContainsKey(Kind))
								{
									var Target = CodeProj.Properties.Targets[Kind];
									var Platforms = new List<UnrealTargetPlatform>();
									var PossPlats = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
									var FormalBuilds = Target.Rules.GUBP_GetConfigsForFormalBuilds_MonolithicOnly(HostPlatform);
									foreach (var FormalBuild in FormalBuilds)
									{
										var Platform = FormalBuild.TargetPlatform;
										if(!Platforms.Contains(Platform) && PossPlats.Contains(Platform))
										{
											Platforms.Add(Platform);
										}
									}
									foreach (var Plat in Platforms)
									{
										if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
										{
											continue;
										}
										if (bp.ActivePlatforms.Contains(Plat))
										{
											string CookedPlatform = Platform.Platforms[Plat].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");
											AddCompletedDependency(CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform));
										}
									}
								}
							}
						}
					}
				}				
			}
		}
		public static string StaticGetFullName()
		{
			return "SharedCookAggregate";
		}
		public override string GetFullName()
		{
			return StaticGetFullName();
		}
	}

    public class GameAggregatePromotableNode : AggregatePromotableNode
    {
        BranchInfo.BranchUProject GameProj;		

        public GameAggregatePromotableNode(GUBP bp, List<UnrealTargetPlatform> InHostPlatforms, BranchInfo.BranchUProject InGameProj, bool IsSeparate)
            : base(InHostPlatforms, InGameProj.GameName)
        {
            GameProj = InGameProj;
			
            foreach (var HostPlatform in HostPlatforms)
            {
                AddDependency(RootEditorNode.StaticGetFullName(HostPlatform));				
                if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
                {
                    AddDependency(EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
                }				
                // add all of the platforms I use
                {
                    var Platforms = bp.GetMonolithicPlatformsForUProject(HostPlatform, InGameProj, false);
                    if (bp.bOrthogonalizeEditorPlatforms)
                    {
                        foreach (var Plat in Platforms)
                        {
                            AddDependency(EditorPlatformNode.StaticGetFullName(HostPlatform, Plat));
                        }
                    }
                }
                {
                    if (!GameProj.Options(HostPlatform).bPromoteEditorOnly)
                    {
                        var Platforms = bp.GetMonolithicPlatformsForUProject(HostPlatform, InGameProj, true);
                        foreach (var Plat in Platforms)
                        {
                            AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, Plat));
                            if (Plat == UnrealTargetPlatform.Win32 && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Game))
                            {
                                if (GameProj.Properties.Targets[TargetRules.TargetType.Game].Rules.GUBP_BuildWindowsXPMonolithics())
                                {
                                    AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, Plat, true));
                                }
                            }
                        }
                    }
				}
			}
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj)
        {
            return AggregatePromotableNode.StaticGetFullName(InGameProj.GameName);
        }

        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
		public override bool IsSeparatePromotable()
		{
			bool IsSeparate = false;
			foreach(UnrealTargetPlatform HostPlatform in HostPlatforms)
			{
				if(GameProj.Options(HostPlatform).bSeparateGamePromotion)
				{
					IsSeparate = true;
				}
			}
			return IsSeparate;
		}
    }

    public class SharedAggregatePromotableNode : AggregatePromotableNode
    {

        public SharedAggregatePromotableNode(GUBP bp, List<UnrealTargetPlatform> InHostPlatforms)
            : base(InHostPlatforms, "Shared")
        {
            foreach (var HostPlatform in HostPlatforms)
            {
                {
                    var Options = bp.Branch.BaseEngineProject.Options(HostPlatform);
                    if (Options.bIsPromotable && !Options.bSeparateGamePromotion)
                    {
                        AddDependency(GameAggregatePromotableNode.StaticGetFullName(bp.Branch.BaseEngineProject));
                    }
                }
                foreach (var CodeProj in bp.Branch.CodeProjects)
                {
                    var Options = CodeProj.Options(HostPlatform);
                    if (!Options.bSeparateGamePromotion)
                    {
                        if (Options.bIsPromotable)
                        {
                            AddDependency(GameAggregatePromotableNode.StaticGetFullName(CodeProj));
                        }
                        else if (Options.bTestWithShared)
                        {
							if (!Options.bIsNonCode)
							{
                                AddDependency(EditorGameNode.StaticGetFullName(HostPlatform, CodeProj)); // if we are just testing, we will still include the editor stuff
                            }
                        }
                    }
                }
                if(HostPlatform == UnrealTargetPlatform.Win64 && bp.ActivePlatforms.Contains(UnrealTargetPlatform.Linux))
                {
                    AddDependency(RootEditorCrossCompileLinuxNode.StaticGetFullName());
                    AddDependency(ToolsCrossCompileNode.StaticGetFullName(HostPlatform));
                }
            }
			AddDependency(MakeFeaturePacksNode.StaticGetFullName(MakeFeaturePacksNode.GetDefaultBuildPlatform(bp)));
        }
		public override bool IsSeparatePromotable()
		{
			return true;
		}
        public static string StaticGetFullName()
        {
            return AggregatePromotableNode.StaticGetFullName("Shared");
        }
    }


    public class WaitForUserInput : GUBPNode
    {
        protected bool bTriggerWasTriggered;
        public WaitForUserInput()
        {
            bTriggerWasTriggered = false;
        }

        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public override bool TriggerNode()
        {
            return true;
        }
        public override void SetAsExplicitTrigger()
        {
            bTriggerWasTriggered = true;
        }
        public override bool IsSticky()
        {
            return bTriggerWasTriggered;
        }
        public virtual string GetTriggerStateName()
        {
            return "GenericTrigger";
        }
        public virtual string GetTriggerDescText()
        {
            return "GenericTrigger no description text available";
        }
        public virtual string GetTriggerActionText()
        {
            return "GenericTrigger no action text available";
        }
        public virtual bool TriggerRequiresRecursiveWorkflow()
        {
            return true;
        }
        public override string ECProcedure()
        {
            if (bTriggerWasTriggered)
            {
                return base.ECProcedure(); // after this user hits the trigger, we want to run this as an ordinary node
            }
            if (TriggerRequiresRecursiveWorkflow())
            {
                return String.Format("GUBP_UAT_Trigger"); //here we run a recursive workflow to wait for the trigger
            }
            return String.Format("GUBP_Hardcoded_Trigger"); //here we advance the state in the hardcoded workflow so folks can approve
        }
        public override string ECProcedureParams()
        {
            var Result = base.ECProcedureParams();
            if (!bTriggerWasTriggered)
            {
                Result += String.Format(", {{actualParameterName => 'TriggerState', value => '{0}'}}, {{actualParameterName => 'ActionText', value =>\"{1}\"}}, {{actualParameterName => 'DescText', value =>\"{2}\"}}", GetTriggerStateName(), GetTriggerActionText(), GetTriggerDescText());
                //Result += String.Format(" --actualParameter TriggerState={0} --actualParameter ActionText=\"{1}\" --actualParameter DescText=\"{2}\"", GetTriggerStateName(), GetTriggerActionText(), GetTriggerDescText());
            }
            return Result;
        }
        public override int TimeoutInMinutes()
        {
            return 0;
        }
    }

	public class WaitForFormalUserInput : WaitForUserInput
	{
		
		public WaitForFormalUserInput(GUBP bp)
		{		
			AddDependency(SharedCookAggregateNode.StaticGetFullName());
			AddCompletedDependency(SharedCookAggregateNode.StaticGetFullName());
		}
		public static string StaticGetFullName()
		{
			return "Shared_WaitToMakeBuilds";
		}
		public override string GetFullName()
		{
			return StaticGetFullName();
		}
	}
    public class WaitForPromotionUserInput : WaitForUserInput
    {
        string PromotionLabelPrefix;
        string PromotionLabelSuffix;
        protected bool bLabelPromoted; // true if this is the promoted version

        public WaitForPromotionUserInput(string InPromotionLabelPrefix, string InPromotionLabelSuffix, bool bInLabelPromoted)
        {
            PromotionLabelPrefix = InPromotionLabelPrefix;
            PromotionLabelSuffix = InPromotionLabelSuffix;
            bLabelPromoted = bInLabelPromoted;
            if (bLabelPromoted)
            {
                AddDependency(LabelPromotableNode.StaticGetFullName(PromotionLabelPrefix, false));
            }
            else
            {
                AddDependency(AggregatePromotableNode.StaticGetFullName(PromotionLabelPrefix));
            }
        }
        public static string StaticGetFullName(string InPromotionLabelPrefix, string InPromotionLabelSuffix, bool bInLabelPromoted)
        {
            return InPromotionLabelPrefix + (bInLabelPromoted ? "_WaitForPromotion" : "_WaitForPromotable") + InPromotionLabelSuffix;
        }
        public override string GetFullName()
        {
            return StaticGetFullName(PromotionLabelPrefix, PromotionLabelSuffix, bLabelPromoted);
        }
    }

    public class WaitForGamePromotionUserInput : WaitForPromotionUserInput
    {
        BranchInfo.BranchUProject GameProj;
        bool bCustomWorkflow;        
        public WaitForGamePromotionUserInput(GUBP bp, BranchInfo.BranchUProject InGameProj, bool bInLabelPromoted)
            : base(InGameProj.GameName, "", bInLabelPromoted)
        {
            GameProj = InGameProj;
            var Options = InGameProj.Options(UnrealTargetPlatform.Win64);
            bCustomWorkflow = Options.bCustomWorkflowForPromotion;       
        }
        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, bool bInLabelPromoted)
        {
            return WaitForPromotionUserInput.StaticGetFullName(InGameProj.GameName, "", bInLabelPromoted);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override string GetTriggerDescText()
        {
            if (bLabelPromoted)
            {
                return GameProj.GameName + " is ready for promotion.";
            }
            return GameProj.GameName + " is ready to make a promotable label and begin testing.";
        }
        public override string GetTriggerActionText()
        {
            if (bLabelPromoted)
            {
                return "Promote " + GameProj.GameName + ".";
            }
            return "Make a promotable label for " + GameProj.GameName + " and begin testing.";
        }
        public override string GetTriggerStateName()
        {
            if (bCustomWorkflow)
            {
                return GetFullName();
            }
            return base.GetTriggerStateName();
        }
        public override bool TriggerRequiresRecursiveWorkflow()
        {
            if (bCustomWorkflow)
            {
                return !bLabelPromoted; // the promotable starts the hardcoded chain
            }
            return base.TriggerRequiresRecursiveWorkflow();
        }
    }

    public class WaitForSharedPromotionUserInput : WaitForPromotionUserInput
    {
        
        public WaitForSharedPromotionUserInput(GUBP bp, bool bInLabelPromoted)
            : base("Shared", IsMainBranch(), bInLabelPromoted)
        {
        }
        public override string GetTriggerDescText()
        {
            if (bLabelPromoted)
            {
                return "The shared promotable is ready for promotion.";
            }
            return "The shared promotable is ready to make a promotable label.";
        }
        public override string GetTriggerActionText()
        {
            if (bLabelPromoted)
            {
                return "Promote the shared promotable.";
            }
            return "Make the shared promotable label.";
        }
        public static string StaticGetFullName(bool bInLabelPromoted)
        {            
            return WaitForPromotionUserInput.StaticGetFullName("Shared", IsMainBranch(), bInLabelPromoted);
        }
        public static string IsMainBranch()
        {
            string isMain = "";
            if (P4Enabled)
            {
                string CurrentBranch = P4Env.BuildRootP4;
                if (CurrentBranch == "//depot/UE4")
                {
                    isMain = "_WithNightlys";
                }
            }
            return isMain;
        }
        public override string GetTriggerStateName()
        {
            return GetFullName();
        }
        public override bool TriggerRequiresRecursiveWorkflow()
        {
            return !bLabelPromoted;
        }
    }   	

    public class LabelPromotableNode : GUBPNode
    {
        string PromotionLabelPrefix;        
        protected bool bLabelPromoted; // true if this is the promoted version

        public LabelPromotableNode(string InPromotionLabelPrefix, string InPromotionLabelSuffix, bool bInLabelPromoted)
        {
            PromotionLabelPrefix = InPromotionLabelPrefix;            
            bLabelPromoted = bInLabelPromoted;
            AddDependency(WaitForPromotionUserInput.StaticGetFullName(PromotionLabelPrefix, InPromotionLabelSuffix, bLabelPromoted));
        }
        string LabelName(bool bLocalLabelPromoted)
        {
            string LabelPrefix = PromotionLabelPrefix;            
            string CompleteLabelPrefix = (bLocalLabelPromoted ? "Promoted-" : "Promotable-") + LabelPrefix;
            if (LabelPrefix == "Shared" && bLocalLabelPromoted)
            {
                // shared promotion has a shorter name
                CompleteLabelPrefix = "Promoted";
            }
            if (LabelPrefix == "Shared" && !bLocalLabelPromoted)
            {
                //shared promotable has a shorter name
                CompleteLabelPrefix = "Promotable";
            }                   
            if (GUBP.bPreflightBuild)
            {
                CompleteLabelPrefix = CompleteLabelPrefix + PreflightMangleSuffix;
            }
            return CompleteLabelPrefix;
        }

        public override bool IsSticky()
        {
            return true;
        }
        public override bool SendSuccessEmail()
        {
            return true;
        }


        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();

            if (P4Enabled)
            {
                if (AllDependencyBuildProducts.Count == 0)
                {
                    throw new AutomationException("{0} has no build products", GetFullName());

                }

                if (bLabelPromoted)
                {
					P4.MakeDownstreamLabelFromLabel(P4Env, LabelName(true), LabelName(false));
                }
                else
                {
					int WorkingCL = P4.CreateChange(P4Env.Client, String.Format("GUBP Node {0} built from changelist {1}", GetFullName(), bp.CL));
                    Log("Build from {0}    Working in {1}", bp.CL, WorkingCL);

                    var ProductsToSubmit = new List<String>();

                    foreach (var Product in AllDependencyBuildProducts)
                    {
                        // hacks to keep certain things out of P4
                        if (
                            !Product.EndsWith("version.h", StringComparison.InvariantCultureIgnoreCase) && 
                            !Product.EndsWith("version.cpp", StringComparison.InvariantCultureIgnoreCase) &&
							!Product.Replace('\\', '/').EndsWith("DotNetCommon/MetaData.cs", StringComparison.InvariantCultureIgnoreCase) &&
                            !Product.EndsWith("_Success.log", StringComparison.InvariantCultureIgnoreCase) &&
							!Product.Replace('\\', '/').Contains("/Intermediate/") &&
							!Product.Replace('\\', '/').Contains("/Engine/Saved/") &&
							!Product.Replace('\\', '/').Contains("/DerivedDataCache/") &&
							!Product.EndsWith(".lib") &&
							!Product.EndsWith(".a") && 
							!Product.EndsWith(".bc")
                            )
                        {
                            ProductsToSubmit.Add(Product);
                        }
                    }

                    // Open files for add or edit
                    UE4Build.AddBuildProductsToChangelist(WorkingCL, ProductsToSubmit);

                    // Check everything in!
                    int SubmittedCL;
					P4.Submit(WorkingCL, out SubmittedCL, true, true);

                    // Label it       
					P4.MakeDownstreamLabel(P4Env, LabelName(false), null);
                }
            }
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public static string StaticGetFullName(string InPromotionLabelPrefix, bool bInLabelPromoted)
        {
            return InPromotionLabelPrefix + (bInLabelPromoted ? "_LabelPromoted" : "_LabelPromotable");
        }
        public override string GetFullName()
        {
            return StaticGetFullName(PromotionLabelPrefix, bLabelPromoted);
        }
    }

    public class GameLabelPromotableNode : LabelPromotableNode
    {
        BranchInfo.BranchUProject GameProj;        
        public GameLabelPromotableNode(GUBP bp, BranchInfo.BranchUProject InGameProj, bool bInLabelPromoted)
            : base(InGameProj.GameName, "", bInLabelPromoted)
        {
            GameProj = InGameProj;
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, bool bInLabelPromoted)
        {
            return LabelPromotableNode.StaticGetFullName(InGameProj.GameName, bInLabelPromoted);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
    }

    public class SharedLabelPromotableNode : LabelPromotableNode
    {        
        public SharedLabelPromotableNode(GUBP bp, bool bInLabelPromoted)
            : base("Shared", IsMainBranch(), bInLabelPromoted)
        {
        }

        public static string StaticGetFullName(bool bInLabelPromoted)
        {
            return LabelPromotableNode.StaticGetFullName("Shared", bInLabelPromoted);
        }
        public static string IsMainBranch()
        {
            string isMain = "";
            if (P4Enabled)
            {
                string CurrentBranch = P4Env.BuildRootP4;
                if (CurrentBranch == "//depot/UE4")
                {
                    isMain = "_WithNightlys";
                }
            }
            return isMain;
        }
    }

    public class SharedLabelPromotableSuccessNode : AggregateNode
    {        
        public SharedLabelPromotableSuccessNode()
        {
			AddDependency(SharedLabelPromotableNode.StaticGetFullName(false));
        }

        public static string StaticGetFullName()
        {
            return SharedLabelPromotableNode.StaticGetFullName(false) + "Aggregate";
        }

		public override string GetFullName()
		{
			return StaticGetFullName();
		}
    }

    public class WaitForTestShared : AggregateNode
    {
        public WaitForTestShared(GUBP bp)
        {
        }
        public static string StaticGetFullName()
        {
            return "Shared_TestingAggregate";
        }
        public override string GetFullName()
        {
            return StaticGetFullName();
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 5;
        }
    }

    public class CookNode : HostPlatformNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;
        string CookPlatform;
        bool bIsMassive;
		

        public CookNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, string InCookPlatform)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;
            CookPlatform = InCookPlatform;
            bIsMassive = false;
            AddDependency(EditorAndToolsNode.StaticGetFullName(HostPlatform));
            if (bp.bOrthogonalizeEditorPlatforms)
            {
                if (TargetPlatform != HostPlatform && TargetPlatform != GUBP.GetAltHostPlatform(HostPlatform))
                {
                    AddDependency(EditorPlatformNode.StaticGetFullName(HostPlatform, TargetPlatform));
                }
            }			
            bool bIsShared = false;
            // is this the "base game" or a non code project?
            if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                var Options = InGameProj.Options(HostPlatform);
                bIsMassive = Options.bIsMassive;				
                AddDependency(EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
                // add an arc to prevent cooks from running until promotable is labeled
                if (Options.bIsPromotable)
                {
                    if (!Options.bSeparateGamePromotion)
                    {
                        bIsShared = true;
                    }
                }
                else if (Options.bTestWithShared)
                {
                    bIsShared = true;
                }
                AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, TargetPlatform));
            }
            else
            {
                bIsShared = true;				
                AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform));
            }
            if (bIsShared)
            {
                // add an arc to prevent cooks from running until promotable is labeled
                AddPseudodependency(WaitForTestShared.StaticGetFullName());
                AgentSharingGroup = "SharedCooks" + StaticGetHostPlatformSuffix(HostPlatform);

                // If the cook fails for the base engine, don't bother trying
                if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && bp.HasNode(CookNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, CookPlatform)))
                {
                    AddPseudodependency(CookNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, CookPlatform));
                }

                // If the base cook platform fails, don't bother trying other ones
                string BaseCookedPlatform = Platform.Platforms[HostPlatform].GetCookPlatform(false, false, "");
                if (InGameProj.GameName == bp.Branch.BaseEngineProject.GameName && CookPlatform != BaseCookedPlatform &&
                    bp.HasNode(CookNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, BaseCookedPlatform)))
                {
                    AddPseudodependency(CookNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, BaseCookedPlatform));
                }
            }			
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InCookPlatform)
        {
            return InGameProj.GameName + "_" + InCookPlatform + "_Cook" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, CookPlatform);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 4 + (bIsMassive ? 1 : 0);
        }
        public override float Priority()
        {
            return 10.0f;
        }
        public override int AgentMemoryRequirement(GUBP bp)
        {
            return bIsMassive ? 32 : 0;
        }
        public override int TimeoutInMinutes()
        {
            return bIsMassive ? 240 : base.TimeoutInMinutes();
        }

        public override string RootIfAnyForTempStorage()
        {
            return CombinePaths(Path.GetDirectoryName(GameProj.FilePath), "Saved", "Cooked", CookPlatform);
        }
        public override void DoBuild(GUBP bp)
        {
            if (HostPlatform == UnrealTargetPlatform.Mac)
            {
                // not sure if we need something here or if the cook commandlet will automatically convert the exe name
            }

			CommandUtils.CookCommandlet(GameProj.FilePath, "UE4Editor-Cmd.exe", null, null, null, null, CookPlatform);
		
            var CookedPath = RootIfAnyForTempStorage();
            var CookedFiles = CommandUtils.FindFiles("*", true, CookedPath);
            if (CookedFiles.GetLength(0) < 1)
            {
                throw new AutomationException("CookedPath {1} did not produce any files.", CookedPath);
            }

            BuildProducts = new List<string>();
            foreach (var CookedFile in CookedFiles)
            {
                AddBuildProduct(CookedFile);
            }
        }
    }


    public class DDCNode : HostPlatformNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;
        string CookPlatform;
        bool bIsMassive;

        public DDCNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, string InCookPlatform)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;
            CookPlatform = InCookPlatform;
            bIsMassive = false;
            AddDependency(RootEditorNode.StaticGetFullName(HostPlatform));
            if (bp.bOrthogonalizeEditorPlatforms)
            {
                if (TargetPlatform != HostPlatform && TargetPlatform != GUBP.GetAltHostPlatform(HostPlatform))
                {
                    AddDependency(EditorPlatformNode.StaticGetFullName(HostPlatform, TargetPlatform));
                }
            }

            bool bIsShared = false;
            // is this the "base game" or a non code project?
            if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                var Options = InGameProj.Options(HostPlatform);
                bIsMassive = Options.bIsMassive;

                AddDependency(EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
                // add an arc to prevent DDCNode from running until promotable is labeled
                if (Options.bIsPromotable)
                {
                    if (Options.bSeparateGamePromotion)
                    {
                       // AddPseudodependency(GameLabelPromotableNode.StaticGetFullName(GameProj, false));
                    }
                    else
                    {
                        bIsShared = true;
                    }
                }
                else if (Options.bTestWithShared)
                {
                    bIsShared = true;
                }
                //AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, TargetPlatform));
            }
            else
            {
                bIsShared = true;
                //AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform));
            }
            if (bIsShared)
            {
                // add an arc to prevent cooks from running until promotable is labeled
                //AddPseudodependency(WaitForTestShared.StaticGetFullName());
                //AgentSharingGroup = "SharedCooks" + StaticGetHostPlatformSuffix(HostPlatform);

                // If the cook fails for the base engine, don't bother trying
                if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && bp.HasNode(DDCNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, CookPlatform)))
                {
                    //AddPseudodependency(DDCNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, CookPlatform));
                }

                // If the base cook platform fails, don't bother trying other ones
                string BaseCookedPlatform = Platform.Platforms[HostPlatform].GetCookPlatform(false, false, "");
                if (InGameProj.GameName == bp.Branch.BaseEngineProject.GameName && CookPlatform != BaseCookedPlatform &&
                    bp.HasNode(DDCNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, BaseCookedPlatform)))
                {
                    //AddPseudodependency(DDCNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, BaseCookedPlatform));
                }
            }
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InCookPlatform)
        {
            return InGameProj.GameName + "_" + InCookPlatform.Replace("+", "_") + "_DDC" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, CookPlatform);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override float Priority()
        {
            return base.Priority() + 10.0f;
        }
        public override int AgentMemoryRequirement(GUBP bp)
        {
            return bIsMassive ? 32 : 0;
        }
        public override int TimeoutInMinutes()
        {
            return bIsMassive ? 240 : base.TimeoutInMinutes();
        }

        public override void DoBuild(GUBP bp)
        {
            if (HostPlatform == UnrealTargetPlatform.Mac)
            {
                // not sure if we need something here or if the cook commandlet will automatically convert the exe name
            }
            CommandUtils.DDCCommandlet(GameProj.FilePath, "UE4Editor-Cmd.exe", null, CookPlatform, "-fill");

            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }

    public class GamePlatformCookedAndCompiledNode : HostPlatformAggregateNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;

        public GamePlatformCookedAndCompiledNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, bool bCodeProject)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;

            foreach (var Kind in BranchInfo.MonolithicKinds)
            {
                if (bCodeProject)
                {
                    if (GameProj.Properties.Targets.ContainsKey(Kind))
                    {
                        var Target = GameProj.Properties.Targets[Kind];
                        var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                        if (Platforms.Contains(TargetPlatform) && Target.Rules.SupportsPlatform(TargetPlatform))
                        {
                            //@todo how do we get the client target platform?
                            string CookedPlatform = Platform.Platforms[TargetPlatform].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");
							if (Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform) != "")
							{
								CookedPlatform = Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform);
							}
                            AddDependency(CookNode.StaticGetFullName(HostPlatform, GameProj, CookedPlatform));
                            AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, TargetPlatform));
							if(Target.Rules.GUBP_BuildWindowsXPMonolithics())
							{
								AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, TargetPlatform, true));
                        }
                    }
                }
                }
                else
                {
                    if (Kind == TargetRules.TargetType.Game) //for now, non-code projects don't do client or server.
                    {
                        if (bp.Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                        {
                            var Target = bp.Branch.BaseEngineProject.Properties.Targets[Kind];
                            var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                            if (Platforms.Contains(TargetPlatform) && Target.Rules.SupportsPlatform(TargetPlatform))
                            {
                                //@todo how do we get the client target platform?
                                string CookedPlatform = Platform.Platforms[TargetPlatform].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");
                                AddDependency(CookNode.StaticGetFullName(HostPlatform, GameProj, CookedPlatform));
                                AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform));
                            }
                        }
                    }
                }
            }

            // put these in the right agent group, even though they aren't exposed to EC to sort right.
            if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                var Options = InGameProj.Options(HostPlatform);
                if ((Options.bIsPromotable || Options.bTestWithShared) && !Options.bSeparateGamePromotion)
                {
                    AgentSharingGroup = "SharedCooks" + StaticGetHostPlatformSuffix(HostPlatform);
                }
            }
            else
            {
                AgentSharingGroup = "SharedCooks" + StaticGetHostPlatformSuffix(HostPlatform);
            }

        }

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform)
        {
            return InGameProj.GameName + "_" + InTargetPlatform + "_CookedAndCompiled" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, TargetPlatform);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
    };

    public class FormalBuildNode : HostPlatformNode
    {
        BranchInfo.BranchUProject GameProj;

        //CAUTION, these are lists, but it isn't clear that lists really work on all platforms, so we stick to one node per build
        List<UnrealTargetPlatform> ClientTargetPlatforms;
        List<UnrealTargetPlatform> ServerTargetPlatforms;
        List<UnrealTargetConfiguration> ClientConfigs;
        List<UnrealTargetConfiguration> ServerConfigs;
        bool ClientNotGame;
		bool bIsCode;
        UnrealBuildTool.TargetRules.TargetType GameOrClient;

        public FormalBuildNode(GUBP bp,
            BranchInfo.BranchUProject InGameProj,
            UnrealTargetPlatform InHostPlatform,
            List<UnrealTargetPlatform> InClientTargetPlatforms = null,
            List<UnrealTargetConfiguration> InClientConfigs = null,
            List<UnrealTargetPlatform> InServerTargetPlatforms = null,
            List<UnrealTargetConfiguration> InServerConfigs = null,
            bool InClientNotGame = false
            )
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            ClientTargetPlatforms = InClientTargetPlatforms;
            ServerTargetPlatforms = InServerTargetPlatforms;
            ClientConfigs = InClientConfigs;
            ServerConfigs = InServerConfigs;
            ClientNotGame = InClientNotGame;

            GameOrClient = TargetRules.TargetType.Game;

            if (ClientNotGame)
            {
                GameOrClient = TargetRules.TargetType.Client;
            }
			if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
			{
				bIsCode = true;
			}
			else
			{
				bIsCode = false;
			}

            // verify we actually built these
            var WorkingGameProject = InGameProj;
            if (!WorkingGameProject.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                // this is a codeless project, use the base project
                WorkingGameProject = bp.Branch.BaseEngineProject;
            }

            var AllTargetPlatforms = new List<UnrealTargetPlatform>();
			var Options = InGameProj.Options(HostPlatform);
			if(!Options.bSeparateGamePromotion && !bp.BranchOptions.bMakeFormalBuildWithoutLabelPromotable)
			{
				AddPseudodependency(WaitForFormalUserInput.StaticGetFullName());				
			}
            if (ClientTargetPlatforms != null)
            {
                if (!WorkingGameProject.Properties.Targets.ContainsKey(GameOrClient))
                {
                    throw new AutomationException("Can't make a game build for {0} because it doesn't have a {1} target.", WorkingGameProject.GameName, GameOrClient.ToString());
                }

                foreach (var Plat in ClientTargetPlatforms)
                {
                    if (!AllTargetPlatforms.Contains(Plat))
                    {
                        AllTargetPlatforms.Add(Plat);
                    }
                }
                if (ClientConfigs == null)
                {
                    ClientConfigs = new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development };
                }
                foreach (var Plat in ClientTargetPlatforms)
                {
                    if (!WorkingGameProject.Properties.Targets[GameOrClient].Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform).Contains(Plat))
                    {
                        throw new AutomationException("Can't make a game/client build for {0} because we didn't build platform {1}.", WorkingGameProject.GameName, Plat.ToString());
                    }
                    foreach (var Config in ClientConfigs)
                    {
                        if (!WorkingGameProject.Properties.Targets[GameOrClient].Rules.GUBP_GetConfigs_MonolithicOnly(HostPlatform, Plat).Contains(Config))
                        {
                            throw new AutomationException("Can't make a game/client build for {0} because we didn't build platform {1} config {2}.", WorkingGameProject.GameName, Plat.ToString(), Config.ToString());
                        }						
                    }
                }
            }
            if (ServerTargetPlatforms != null)
            {
                if (!WorkingGameProject.Properties.Targets.ContainsKey(TargetRules.TargetType.Server) && ServerTargetPlatforms != null)
                {
                    throw new AutomationException("Can't make a server build for {0} because it doesn't have a server target.", WorkingGameProject.GameName);
                }
                foreach (var Plat in ServerTargetPlatforms)
                {
                    if (!AllTargetPlatforms.Contains(Plat))
                    {
                        AllTargetPlatforms.Add(Plat);
                    }
                }
                if (ServerConfigs == null)
                {
                    ServerConfigs = new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development };
                }
                foreach (var Plat in ServerTargetPlatforms)
                {
                    if (!WorkingGameProject.Properties.Targets[TargetRules.TargetType.Server].Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform).Contains(Plat))
                    {
                        throw new AutomationException("Can't make a server build for {0} because we didn't build platform {1}.", WorkingGameProject.GameName, Plat.ToString());
                    }
                    foreach (var Config in ServerConfigs)
                    {
                        if (!WorkingGameProject.Properties.Targets[TargetRules.TargetType.Server].Rules.GUBP_GetConfigs_MonolithicOnly(HostPlatform, Plat).Contains(Config))
                        {
                            throw new AutomationException("Can't make a server build for {0} because we didn't build platform {1} config {2}.", WorkingGameProject.GameName, Plat.ToString(), Config.ToString());
                        }
                    }
                }
            }

            // add dependencies for cooked and compiled
            foreach (var Plat in AllTargetPlatforms)
            {
                AddDependency(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, Plat));
            }
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InClientTargetPlatforms = null, List<UnrealTargetConfiguration> InClientConfigs = null, List<UnrealTargetPlatform> InServerTargetPlatforms = null, List<UnrealTargetConfiguration> InServerConfigs = null, bool InClientNotGame = false)
        {
            string Infix = "";
            if (InClientNotGame)
            {
                if (InClientTargetPlatforms != null && InClientTargetPlatforms.Count == 1)
                {
                    Infix = "_Client_" + InClientTargetPlatforms[0].ToString();
                }
                if (InClientConfigs != null && InClientConfigs.Count == 1)
                {
                    Infix += "_Client_" + InClientConfigs[0].ToString();
                }
            }
            else
            {
                if (InClientTargetPlatforms != null && InClientTargetPlatforms.Count == 1)
                {
                    Infix = "_" + InClientTargetPlatforms[0].ToString();
                }
                if (InClientConfigs != null && InClientConfigs.Count == 1)
                {
                    Infix += "_" + InClientConfigs[0].ToString();
                }
            }
            if (InServerTargetPlatforms != null && InServerTargetPlatforms.Count == 1)
            {
                Infix = "_Serv_" + InServerTargetPlatforms[0].ToString();
            }
            if (InServerConfigs != null && InServerConfigs.Count == 1)
            {
                Infix += "_Serv_" + InServerConfigs[0].ToString();
            }
            return InGameProj.GameName + Infix + "_MakeBuild" + HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform, ClientTargetPlatforms, ClientConfigs, ServerTargetPlatforms, ServerConfigs, ClientNotGame);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override bool SendSuccessEmail()
        {
            return true;
        }
        public override string ECAgentString()
        {
            string Result = base.ECAgentString();
			if (ClientTargetPlatforms != null)
			{
				if (!ClientNotGame)
				{
					foreach (UnrealTargetPlatform Plat in ClientTargetPlatforms)
					{
						if (Plat == UnrealTargetPlatform.XboxOne)
						{
							Result = MergeSpaceStrings(Result, Plat.ToString());
						}
					}
				}
			}
            return Result;
        }      
        public override float Priority()
        {
            return base.Priority() + 20.0f;
        }
		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 3;
		}
        public static string GetArchiveDirectory(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InClientTargetPlatforms = null, List<UnrealTargetConfiguration> InClientConfigs = null, List<UnrealTargetPlatform> InServerTargetPlatforms = null, List<UnrealTargetConfiguration> InServerConfigs = null, bool InClientNotGame = false)
        {
            string BaseDir = ResolveSharedBuildDirectory(InGameProj.GameName);
            string NodeName = StaticGetFullName(InGameProj, InHostPlatform, InClientTargetPlatforms, InClientConfigs, InServerTargetPlatforms, InServerConfigs, InClientNotGame);
            string Inner = P4Env.BuildRootEscaped + "-CL-" + P4Env.ChangelistString;
            if (GUBP.bPreflightBuild)
            {
                Inner = Inner + PreflightMangleSuffix;
            }
            string ArchiveDirectory = CombinePaths(BaseDir, NodeName, Inner);
            return ArchiveDirectory;
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            string ProjectArg = "";
            if (!String.IsNullOrEmpty(GameProj.FilePath))
            {
                ProjectArg = " -project=\"" + GameProj.FilePath + "\"";
            }
            string Args = String.Format("BuildCookRun{0} -SkipBuild -SkipCook -Stage -Pak -Package -NoSubmit", ProjectArg);

			bool bXboxOneTarget = false;
            if (ClientTargetPlatforms != null)
            {
                bool bFirstClient = true;
                foreach (var Plat in ClientTargetPlatforms)
                {
					if (Plat == UnrealTargetPlatform.XboxOne)
					{
						bXboxOneTarget = true;
					}
                    if (bFirstClient)
                    {
                        bFirstClient = false;
                        Args += String.Format(" -platform={0}", Plat.ToString());
                    }
                    else
                    {
                        Args += String.Format("+{0}", Plat.ToString());
                    }
					if(bIsCode)
					{
						var Target =  GameProj.Properties.Targets[TargetRules.TargetType.Game];
						if(ClientNotGame)
						{
							Target = GameProj.Properties.Targets[TargetRules.TargetType.Client];						
						}					
						if (Target.Rules.GUBP_AdditionalPackageParameters(HostPlatform, Plat) != "")
						{
							Args += " " + Target.Rules.GUBP_AdditionalPackageParameters(HostPlatform, Plat);
						}
					}
                }
                bool bFirstClientConfig = true;
                foreach (var Config in ClientConfigs)
                {
                    if (bFirstClientConfig)
                    {
                        bFirstClientConfig = false;
                        Args += String.Format(" -clientconfig={0}", Config.ToString());
                    }
                    else
                    {
                        Args += String.Format("+{0}", Config.ToString());
                    }
                }
                if (ClientNotGame)
                {
                    Args += " -client";					
                }
            }
            else
            {
                Args += " -noclient";
            }
            if (ServerTargetPlatforms != null)
            {
                Args += " -server";
                bool bFirstServer = true;
                foreach (var Plat in ServerTargetPlatforms)
                {
					if (Plat == UnrealTargetPlatform.XboxOne)
					{
						bXboxOneTarget = true;
					}
					if (bFirstServer)
                    {
                        bFirstServer = false;
                        Args += String.Format(" -serverplatform={0}", Plat.ToString());
                    }
                    else
                    {
                        Args += String.Format("+{0}", Plat.ToString());
                    }
					if (bIsCode)
					{
						var Target = GameProj.Properties.Targets[TargetRules.TargetType.Server];
						if (Target.Rules.GUBP_AdditionalPackageParameters(HostPlatform, Plat) != "")
						{
							Args += " " + Target.Rules.GUBP_AdditionalPackageParameters(HostPlatform, Plat);
						}
					}
                }
                bool bFirstServerConfig = true;
                foreach (var Config in ServerConfigs)
                {
                    if (bFirstServerConfig)
                    {
                        bFirstServerConfig = false;
                        Args += String.Format(" -serverconfig={0}", Config.ToString());
                    }
                    else
                    {
                        Args += String.Format("+{0}", Config.ToString());
                    }
                }
            }

            string FinalArchiveDirectory = "";
			string IntermediateArchiveDirectory = FinalArchiveDirectory;
            if (P4Enabled)
            {
                FinalArchiveDirectory = GetArchiveDirectory(GameProj, HostPlatform, ClientTargetPlatforms, ClientConfigs, ServerTargetPlatforms, ServerConfigs, ClientNotGame);
				IntermediateArchiveDirectory = FinalArchiveDirectory;
				// Xbox One packaging does not function with remote file systems. Use a temp local directory to package and then move files into final location.
				if (bXboxOneTarget)
				{
					IntermediateArchiveDirectory = Path.Combine(Path.GetTempPath(), "GUBP.XboxOne");
					if (DirectoryExists_NoExceptions(IntermediateArchiveDirectory))
					{
						DeleteDirectory_NoExceptions(IntermediateArchiveDirectory);
					}
					CreateDirectory_NoExceptions(IntermediateArchiveDirectory);
				}
				CleanFormalBuilds(FinalArchiveDirectory);
				if (DirectoryExists_NoExceptions(FinalArchiveDirectory))
                {
                    if (IsBuildMachine)
                    {
						throw new AutomationException("Archive directory already exists {0}", FinalArchiveDirectory);
                    }
					DeleteDirectory_NoExceptions(FinalArchiveDirectory);
                }
				Args += String.Format(" -Archive -archivedirectory={0}", CommandUtils.MakePathSafeToUseWithCommandLine(IntermediateArchiveDirectory));
            }
			
            string LogFile = CommandUtils.RunUAT(CommandUtils.CmdEnv, Args);

            if (P4Enabled)
            {
				if (!FinalArchiveDirectory.Equals(IntermediateArchiveDirectory, StringComparison.InvariantCultureIgnoreCase))
				{
					CopyDirectory_NoExceptions(IntermediateArchiveDirectory, FinalArchiveDirectory);
					DeleteDirectory_NoExceptions(IntermediateArchiveDirectory);
				}
			}

            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));
        }
    }


    public class TestNode : HostPlatformNode
    {
        public TestNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
        }
        public override float Priority()
        {
            return 0.0f;
        }
        public virtual void DoTest(GUBP bp)
        {
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            DoTest(bp);
        }
    }

    public class FormalBuildTestNode : TestNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform ClientTargetPlatform;
        UnrealTargetConfiguration ClientConfig;
        UnrealBuildTool.TargetRules.TargetType GameOrClient;

        public FormalBuildTestNode(GUBP bp,
            BranchInfo.BranchUProject InGameProj,
            UnrealTargetPlatform InHostPlatform,
            UnrealTargetPlatform InClientTargetPlatform,
            UnrealTargetConfiguration InClientConfig
            )
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            ClientTargetPlatform = InClientTargetPlatform;
            ClientConfig = InClientConfig;
            GameOrClient = TargetRules.TargetType.Game;

            // verify we actually built these
            var WorkingGameProject = InGameProj;
            if (!WorkingGameProject.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                // this is a codeless project, use the base project
                WorkingGameProject = bp.Branch.BaseEngineProject;
            }
            if (!WorkingGameProject.Properties.Targets.ContainsKey(GameOrClient))
            {
                throw new AutomationException("Can't make a game build for {0} because it doesn't have a {1} target.", WorkingGameProject.GameName, GameOrClient.ToString());
            }

            if (!WorkingGameProject.Properties.Targets[GameOrClient].Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform).Contains(ClientTargetPlatform))
            {
                throw new AutomationException("Can't make a game/client build for {0} because we didn't build platform {1}.", WorkingGameProject.GameName, ClientTargetPlatform.ToString());
            }
            if (!WorkingGameProject.Properties.Targets[GameOrClient].Rules.GUBP_GetConfigs_MonolithicOnly(HostPlatform, ClientTargetPlatform).Contains(ClientConfig))
            {
                throw new AutomationException("Can't make a game/client build for {0} because we didn't build platform {1} config {2}.", WorkingGameProject.GameName, ClientTargetPlatform.ToString(), ClientConfig.ToString());
            }
            AddDependency(FormalBuildNode.StaticGetFullName(GameProj, HostPlatform, new List<UnrealTargetPlatform>() { ClientTargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { ClientConfig }, InClientNotGame: GameOrClient == TargetRules.TargetType.Client));
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform InClientTargetPlatform, UnrealTargetConfiguration InClientConfig)
        {
            string Infix = "_" + InClientTargetPlatform.ToString();
            Infix += "_" + InClientConfig.ToString();
            return InGameProj.GameName + Infix + "_TestBuild" + HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform, ClientTargetPlatform, ClientConfig);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override float Priority()
        {
            return base.Priority() - 20.0f;
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 3;
        }
        public override void DoTest(GUBP bp)
        {
            string ProjectArg = "";
            if (!String.IsNullOrEmpty(GameProj.FilePath))
            {
                ProjectArg = " -project=\"" + GameProj.FilePath + "\"";
            }

            string ArchiveDirectory = FormalBuildNode.GetArchiveDirectory(GameProj, HostPlatform, new List<UnrealTargetPlatform>() { ClientTargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { ClientConfig }, InClientNotGame: GameOrClient == TargetRules.TargetType.Client);
            if (!DirectoryExists_NoExceptions(ArchiveDirectory))
            {
                throw new AutomationException("Archive directory does not exist {0}, so we can't test the build.", ArchiveDirectory);
            }

            string WorkingCommandline = String.Format("TestFormalBuild {0} -Archive -alldevices -archivedirectory={1} -platform={2} -clientconfig={3} -runtimeoutseconds=300",
                ProjectArg, CommandUtils.MakePathSafeToUseWithCommandLine(ArchiveDirectory), ClientTargetPlatform.ToString(), ClientConfig.ToString());

            if (WorkingCommandline.Contains("-project=\"\""))
            {
                throw new AutomationException("Command line {0} contains -project=\"\" which is doomed to fail", WorkingCommandline);
            }
            string LogFile = RunUAT(CommandUtils.CmdEnv, WorkingCommandline);
            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));
        }
        public override string ECAgentString()
        {
            string Result = base.ECAgentString();
            if (ClientTargetPlatform != HostPlatform)
            {
                Result = MergeSpaceStrings(Result, ClientTargetPlatform.ToString());
            }
            return Result;
        }
    }

	public class NonUnityToolNode : TestNode
	{
		SingleTargetProperties ProgramTarget;
		public NonUnityToolNode(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
			: base(InHostPlatform)
		{
			ProgramTarget = InProgramTarget;
			AddPseudodependency(SingleInternalToolsNode.StaticGetFullName(HostPlatform, ProgramTarget));
			AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
		}
		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, SingleTargetProperties InGameProj)
		{
			return InGameProj.TargetName + "_NonUnityTestCompile" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform, ProgramTarget);
		}
		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			int Result = base.CISFrequencyQuantumShift(bp) + 2;
			if (HostPlatform == UnrealTargetPlatform.Mac)
			{
				Result += 1;
			}		
			return Result;
		}
		public override int AgentMemoryRequirement(GUBP bp)
		{
			int Result = base.AgentMemoryRequirement(bp);
			if (HostPlatform == UnrealTargetPlatform.Mac)
			{
				Result = 32;
			}
			return Result;
		}
		public override void DoTest(GUBP bp)
		{
			var Build = new UE4Build(bp);
			var Agenda = new UE4Build.BuildAgenda();

			Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, HostPlatform, UnrealTargetConfiguration.Development);
			Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-skipnonhostplatforms");			

			Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false, InForceNonUnity: true);

			UE4Build.CheckBuildProducts(Build.BuildProductFiles);
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}

    public class NonUnityTestNode : TestNode
    {
        public NonUnityTestNode(UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "NonUnityTestCompile" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            int Result = base.CISFrequencyQuantumShift(bp) + 2;
            if(HostPlatform == UnrealTargetPlatform.Mac)
            {
                Result += 1;
            }
            return Result;
        }
        public override int AgentMemoryRequirement(GUBP bp)
        {
            int Result = base.AgentMemoryRequirement(bp);
            if(HostPlatform == UnrealTargetPlatform.Mac)
            {
                Result = 32;
            }
            return Result;
        }
        public override void DoTest(GUBP bp)
        {
            var Build = new UE4Build(bp);
            var Agenda = new UE4Build.BuildAgenda();
            
            Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, HostPlatform, UnrealTargetConfiguration.Development);
            Agenda.AddTargets(
                new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
                HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-skipnonhostplatforms");

            foreach (var Kind in BranchInfo.MonolithicKinds)
            {
                if (bp.Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                {
                    var Target = bp.Branch.BaseEngineProject.Properties.Targets[Kind];
                    Agenda.AddTargets(new string[] { Target.TargetName }, HostPlatform, UnrealTargetConfiguration.Development);
                }
            }

            Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false, InForceNonUnity: true);

            UE4Build.CheckBuildProducts(Build.BuildProductFiles);
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }

    public class IOSOnPCTestNode : TestNode
    {
        public IOSOnPCTestNode(GUBP bp)
            : base(UnrealTargetPlatform.Win64)
        {
            AddDependency(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
            AddDependency(ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64));
            AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(UnrealTargetPlatform.Mac, bp.Branch.BaseEngineProject, UnrealTargetPlatform.IOS));
        }
        public static string StaticGetFullName()
        {
            return "IOSOnPCTestCompile";
        }
        public override string GetFullName()
        {
            return StaticGetFullName();
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 3;
        }
        public override void DoTest(GUBP bp)
        {
            var Build = new UE4Build(bp);
            var Agenda = new UE4Build.BuildAgenda();

            Agenda.AddTargets(new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Game].TargetName }, UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Development);

            Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false);

            UE4Build.CheckBuildProducts(Build.BuildProductFiles);
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }
	
	public class VSExpressTestNode : TestNode
	{
		public VSExpressTestNode(GUBP bp)
			: base(UnrealTargetPlatform.Win64)
		{
			AddDependency(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
			AddDependency(RootEditorNode.StaticGetFullName(UnrealTargetPlatform.Win64));
		}
		public static string StaticGetFullName()
		{
			return "VSExpressTestCompile";
		}
		public override string GetFullName()
		{
			return StaticGetFullName();
		}
		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 3;
		}
		public override string ECAgentString()
		{
			return "VCTestAgent";
		}
		public override void DoTest(GUBP bp)
		{
			var Build = new UE4Build(bp);
			var Agenda = new UE4Build.BuildAgenda();

			string AddArgs = "-nobuilduht";
			if (bp.bOrthogonalizeEditorPlatforms)
			{
				AddArgs += " -skipnonhostplatforms";
			}

			Agenda.AddTargets(
				new string[] { bp.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
				HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
			foreach (var ProgramTarget in bp.Branch.BaseEngineProject.Properties.Programs)
			{
				if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(HostPlatform))
				{
					Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
				}
			}
			Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false);

			UE4Build.CheckBuildProducts(Build.BuildProductFiles);
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}
    public class UATTestNode : TestNode
    {
        string TestName;
        BranchInfo.BranchUProject GameProj;
        string UATCommandLine;
        bool DependsOnEditor;
        List<UnrealTargetPlatform> DependsOnCooked;
        float ECPriority;

        public UATTestNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InTestName, string InUATCommandLine, string InAgentSharingGroup, bool InDependsOnEditor = true, List<UnrealTargetPlatform> InDependsOnCooked = null, float InECPriority = 0.0f)
            : base(InHostPlatform)
        {
            AgentSharingGroup = InAgentSharingGroup;
            ECPriority = InECPriority;
            GameProj = InGameProj;
            TestName = InTestName;
            UATCommandLine = InUATCommandLine;
            bool bWillCook = InUATCommandLine.IndexOf("-cook") >= 0;
            DependsOnEditor = InDependsOnEditor || bWillCook;
            if (InDependsOnCooked != null)
            {
                DependsOnCooked = InDependsOnCooked;
            }
            else
            {
                DependsOnCooked = new List<UnrealTargetPlatform>();
            }
            if (DependsOnEditor)
            {
                AddDependency(EditorAndToolsNode.StaticGetFullName(HostPlatform));
                if (GameProj.GameName != bp.Branch.BaseEngineProject.GameName)
                {
                    if (GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
                    {
                        AddDependency(EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
                    }
                }
            }
            foreach (var Plat in DependsOnCooked)
            {
                AddDependency(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, Plat));
            }         
           
            AddPseudodependency(WaitForTestShared.StaticGetFullName());
            // If the same test fails for the base engine, don't bother trying
            if (InGameProj.GameName != bp.Branch.BaseEngineProject.GameName)
            {
                if (bp.HasNode(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TestName)))
                {
                    AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, InTestName));
                }
                else
                {
                    bool bFoundACook = false;
                    foreach (var Plat in DependsOnCooked)
                    {
                        var PlatTestName = "CookedGameTest_"  + Plat.ToString();
                        if (bp.HasNode(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, PlatTestName)))
                        {
                            AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, PlatTestName));
                            bFoundACook = true;
                        }
                    }

                    if (!bFoundACook && bp.HasNode(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, "EditorTest")))
                    {
                        AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, "EditorTest"));
                    }

                }
            }

            if (InGameProj.GameName == bp.Branch.BaseEngineProject.GameName)
            {
                ECPriority = ECPriority + 1.0f;
            }
            if (UATCommandLine.IndexOf("-RunAutomationTests", StringComparison.InvariantCultureIgnoreCase) >= 0)
            {
                ECPriority = ECPriority - 4.0f;
                if (UATCommandLine.IndexOf("-EditorTest", StringComparison.InvariantCultureIgnoreCase) >= 0)
                {
                    ECPriority = ECPriority - 4.0f;
                }
            }
            else if (UATCommandLine.IndexOf("-EditorTest", StringComparison.InvariantCultureIgnoreCase) >= 0)
            {
                ECPriority = ECPriority + 2.0f;
            }

        }
        public override float Priority()
        {
            return ECPriority;
        }
		public override bool IsTest()
		{
			return true;
		}
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 5;
        }
        public override string ECAgentString()
        {
            string Result = base.ECAgentString();
            foreach (UnrealTargetPlatform platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
            {
                if (platform != HostPlatform && platform != GetAltHostPlatform())
                {
                    if (UATCommandLine.IndexOf("-platform=" + platform.ToString(), StringComparison.InvariantCultureIgnoreCase) >= 0)
                    {
                        Result = MergeSpaceStrings(Result, platform.ToString());
                    }
                }
            }
            return Result;
        }      

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InTestName)
        {
            return InGameProj.GameName + "_" + InTestName + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, TestName);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override void DoTest(GUBP bp)
        {
            string ProjectArg = "";
            if (!String.IsNullOrEmpty(GameProj.FilePath) && UATCommandLine.IndexOf("-project=", StringComparison.InvariantCultureIgnoreCase) < 0)
            {
                ProjectArg = " -project=\"" + GameProj.FilePath + "\"";
            }
            string WorkingCommandline = UATCommandLine + ProjectArg + " -NoSubmit -addcmdline=\"-DisablePS4TMAPI\"";
            if (WorkingCommandline.Contains("-project=\"\""))
            {
                throw new AutomationException("Command line {0} contains -project=\"\" which is doomed to fail", WorkingCommandline);
            }
            string LogFile = RunUAT(CommandUtils.CmdEnv, WorkingCommandline);
            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));
        }
    }

    public class GameAggregateNode : HostPlatformAggregateNode
    {
        BranchInfo.BranchUProject GameProj;
        string AggregateName;
        float ECPriority;

        public GameAggregateNode(GUBP bp, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InAggregateName, List<string> Dependencies, float InECPriority = 100.0f)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            AggregateName = InAggregateName;
            ECPriority = InECPriority;

            foreach (var Dep in Dependencies)
            {
                AddDependency(Dep);
            }
        }
        public override float Priority()
        {
            return ECPriority;
        }

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InAggregateName)
        {
            return InGameProj.GameName + "_" + InAggregateName + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, AggregateName);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
    };

    public class CleanSharedTempStorageNode : GUBPNode
    {
        public CleanSharedTempStorageNode(GUBP bp)
        {
            var ToolsNode = bp.FindNode(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
            AgentSharingGroup = ToolsNode.AgentSharingGroup;
        }
        public override float Priority()
        {
            return -1E15f;
        }

        public static string StaticGetFullName()
        {
            return "CleanSharedTempStorage";
        }
        public override string GetFullName()
        {
            return StaticGetFullName();
        }
        public override void DoBuild(GUBP bp)
        {
            {
                var StartTime = DateTime.UtcNow;
                foreach (var NodeToDo in bp.GUBPNodes)
                {
                    CleanSharedTempStorageDirectory(NodeToDo.Value.GameNameIfAnyForTempStorage());
                }
                var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
                Log("Took {0}s to clear temp storage of old files.", BuildDuration / 1000);
            }

            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public override int CISFrequencyQuantumShift(GUBP bp)
        {
            return base.CISFrequencyQuantumShift(bp) + 3;
        }
    };

    public string AddNode(GUBPNode Node)
    {
        string Name = Node.GetFullName();
        if (GUBPNodes.ContainsKey(Name))
        {
            throw new AutomationException("Attempt to add a duplicate node {0}", Node.GetFullName());
        }
        GUBPNodes.Add(Name, Node);
        return Name;
    }

    public bool HasNode(string Node)
    {
        return GUBPNodes.ContainsKey(Node);
    }

    public GUBPNode FindNode(string Node)
    {
        return GUBPNodes[Node];
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

    public void RemoveAllPseudodependenciesFromNode(string Node)
    {
        if (!GUBPNodes.ContainsKey(Node))
        {
            throw new AutomationException("Node {0} not found", Node);
        }
        GUBPNodes[Node].FullNamesOfPseudosependencies.Clear();
    }

    List<string> GetDependencies(string NodeToDo, bool bFlat = false, bool ECOnly = false)
    {
        var Result = new List<string>();
        foreach (var Node in GUBPNodes[NodeToDo].FullNamesOfDependencies)
        {
            bool Usable = GUBPNodes[Node].RunInEC() || !ECOnly;
            if (Usable)
            {
                if (!Result.Contains(Node))
                {
                    Result.Add(Node);
                }
            }
            if (bFlat || !Usable)
            {
                foreach (var RNode in GetDependencies(Node, bFlat, ECOnly))
                {
                    if (!Result.Contains(RNode))
                    {
                        Result.Add(RNode);
                    }
                }
            }
        }
        foreach (var Node in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
        {
            bool Usable = GUBPNodes[Node].RunInEC() || !ECOnly;
            if (Usable)
            {
                if (!Result.Contains(Node))
                {
                    Result.Add(Node);
                }
            }
            if (bFlat || !Usable)
            {
                foreach (var RNode in GetDependencies(Node, bFlat, ECOnly))
                {
                    if (!Result.Contains(RNode))
                    {
                        Result.Add(RNode);
                    }
                }
            }
        }
        return Result;
    }

	List<string> GetCompletedOnlyDependencies(string NodeToDo, bool bFlat = false, bool ECOnly = true)
	{
		var Result = new List<string>();
		foreach (var Node in GUBPNodes[NodeToDo].CompletedDependencies)
		{
			bool Usable = GUBPNodes[Node].RunInEC() || !ECOnly;
			if(Usable)
			{
				if(!Result.Contains(Node))
				{
					Result.Add(Node);
				}
			}
			if (bFlat || !Usable)
			{
				foreach (var RNode in GetCompletedOnlyDependencies(Node, bFlat, ECOnly))
				{
					if (!Result.Contains(RNode))
					{
						Result.Add(RNode);
					}
				}
			}
		}
		return Result;
	}
    List<string> GetECDependencies(string NodeToDo, bool bFlat = false)
    {
        return GetDependencies(NodeToDo, bFlat, true);
    }
    bool NodeDependsOn(string Rootward, string Leafward)
    {
        var Deps = GetDependencies(Leafward, true);
        return Deps.Contains(Rootward);
    }

    string GetControllingTrigger(string NodeToDo)
    {
        if (GUBPNodesControllingTrigger.ContainsKey(NodeToDo))
        {
            return GUBPNodesControllingTrigger[NodeToDo];
        }
        var Result = "";
        foreach (var Node in GUBPNodes[NodeToDo].FullNamesOfDependencies)
        {
            if (!GUBPNodes.ContainsKey(Node))
            {
                throw new AutomationException("Dependency {0} in {1} not found.", Node, NodeToDo);
            }
            bool IsTrigger = GUBPNodes[Node].TriggerNode();
            if (IsTrigger)
            {
                if (Node != Result && !string.IsNullOrEmpty(Result))
                {
                    throw new AutomationException("Node {0} has two controlling triggers {1} and {2}.", NodeToDo, Node, Result);
                }
                Result = Node;
            }
            else
            {
                string NewResult = GetControllingTrigger(Node);
                if (!String.IsNullOrEmpty(NewResult))
                {
                    if (NewResult != Result && !string.IsNullOrEmpty(Result))
                    {
                        throw new AutomationException("Node {0} has two controlling triggers {1} and {2}.", NodeToDo, NewResult, Result);
                    }
                    Result = NewResult;
                }
            }
        }
        foreach (var Node in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
        {
            if (!GUBPNodes.ContainsKey(Node))
            {
                throw new AutomationException("Pseudodependency {0} in {1} not found.", Node, NodeToDo);
            }
            bool IsTrigger = GUBPNodes[Node].TriggerNode();
            if (IsTrigger)
            {
                if (Node != Result && !string.IsNullOrEmpty(Result))
                {
                    throw new AutomationException("Node {0} has two controlling triggers {1} and {2}.", NodeToDo, Node, Result);
                }
                Result = Node;
            }
            else
            {
                string NewResult = GetControllingTrigger(Node);
                if (!String.IsNullOrEmpty(NewResult))
                {
                    if (NewResult != Result && !string.IsNullOrEmpty(Result))
                    {
                        throw new AutomationException("Node {0} has two controlling triggers {1} and {2}.", NodeToDo, NewResult, Result);
                    }
                    Result = NewResult;
                }
            }
        }
        GUBPNodesControllingTrigger.Add(NodeToDo, Result);
        return Result;
    }
    string GetControllingTriggerDotName(string NodeToDo)
    {
        if (GUBPNodesControllingTriggerDotName.ContainsKey(NodeToDo))
        {
            return GUBPNodesControllingTriggerDotName[NodeToDo];
        }
        string Result = "";
        string WorkingNode = NodeToDo;
        while (true)
        {
            string ThisResult = GetControllingTrigger(WorkingNode);
            if (ThisResult == "")
            {
                break;
            }
            if (Result != "")
            {
                Result = "." + Result;
            }
            Result = ThisResult + Result;
            WorkingNode = ThisResult;
        }
        GUBPNodesControllingTriggerDotName.Add(NodeToDo, Result);
        return Result;
    }

    public string CISFrequencyQuantumShiftString(string NodeToDo)
    {
        string FrequencyString = "";
        int Quantum = GUBPNodes[NodeToDo].DependentCISFrequencyQuantumShift();
        if (Quantum > 0)
        {
			int TimeQuantum = 20;
			if(BranchOptions.QuantumOverride != 0)
			{
				TimeQuantum = BranchOptions.QuantumOverride;
			}
            int Minutes = TimeQuantum * (1 << Quantum);
            if (Minutes < 60)
            {
                FrequencyString = string.Format(" ({0}m)", Minutes);
            }
            else
            {
                FrequencyString = string.Format(" ({0}h{1}m)", Minutes / 60, Minutes % 60);
            }
        }
        return FrequencyString;
    }

    public int ComputeDependentCISFrequencyQuantumShift(string NodeToDo, Dictionary<string, int> FrequencyOverrides)
    {
        int Result = GUBPNodes[NodeToDo].ComputedDependentCISFrequencyQuantumShift;        
        if (Result < 0)
        {
            Result = GUBPNodes[NodeToDo].CISFrequencyQuantumShift(this);
            Result = GetFrequencyForNode(this, GUBPNodes[NodeToDo].GetFullName(), Result);

			int FrequencyOverride;
			if(FrequencyOverrides.TryGetValue(NodeToDo, out FrequencyOverride) && Result > FrequencyOverride)
			{
				Result = FrequencyOverride;
			}

            foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
            {
                Result = Math.Max(ComputeDependentCISFrequencyQuantumShift(Dep, FrequencyOverrides), Result);
            }
            foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
            {
                Result = Math.Max(ComputeDependentCISFrequencyQuantumShift(Dep, FrequencyOverrides), Result);
            }
			foreach (var Dep in GUBPNodes[NodeToDo].CompletedDependencies)
			{
				Result = Math.Max(ComputeDependentCISFrequencyQuantumShift(Dep, FrequencyOverrides), Result);
			}
            if (Result < 0)
            {
                throw new AutomationException("Failed to compute shift.");
            }
            GUBPNodes[NodeToDo].ComputedDependentCISFrequencyQuantumShift = Result;
        }
        return Result;
    }

    bool NodeIsAlreadyComplete(string NodeToDo, bool LocalOnly)
    {
        if (GUBPNodesCompleted.ContainsKey(NodeToDo))
        {
            return GUBPNodesCompleted[NodeToDo];
        }
        string NodeStoreName = StoreName + "-" + GUBPNodes[NodeToDo].GetFullName();
        string GameNameIfAny = GUBPNodes[NodeToDo].GameNameIfAnyForTempStorage();
        bool Result;
        if (LocalOnly)
        {
            Result = LocalTempStorageExists(CmdEnv, NodeStoreName, bQuiet : true);
        }
        else
        {
            Result = TempStorageExists(CmdEnv, NodeStoreName, GameNameIfAny, bQuiet: true);
			if(GameNameIfAny != "" && Result == false)
			{
				Result = TempStorageExists(CmdEnv, NodeStoreName, "", bQuiet: true);
			}
        }
        if (Result)
        {
            Log("***** GUBP Trigger Node was already triggered {0} -> {1} : {2}", GUBPNodes[NodeToDo].GetFullName(), GameNameIfAny, NodeStoreName);
        }
        else
        {
            Log("***** GUBP Trigger Node was NOT yet triggered {0} -> {1} : {2}", GUBPNodes[NodeToDo].GetFullName(), GameNameIfAny, NodeStoreName);
        }
        GUBPNodesCompleted.Add(NodeToDo, Result);
        return Result;
    }
    string RunECTool(string Args, bool bQuiet = false)
    {
        if (ParseParam("FakeEC"))
        {
            Log("***** Would have ran ectool {0}", Args);
            return "We didn't actually run ectool";
        }
        else
        {
            ERunOptions Opts = ERunOptions.Default;
            if (bQuiet)
            {
                Opts = (Opts & ~ERunOptions.AllowSpew) | ERunOptions.NoLoggingOfRunCommand;
            }
            return RunAndLog("ectool", "--timeout 900 " + Args, Options: Opts);
        }
    }
    void WriteECPerl(List<string> Args)
    {
        Args.Add("$batch->submit();");
        string ECPerlFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "jobsteps.pl");
        WriteAllLines_NoExceptions(ECPerlFile, Args.ToArray());
    }
    string GetEMailListForNode(GUBP bp, string NodeToDo, string Emails, string Causers)
    {        
        var BranchForEmail = "";
        if (P4Enabled)
        {
            BranchForEmail = P4Env.BuildRootP4;
        }
        return HackEmails(Emails, Causers, BranchForEmail, NodeToDo);
    }
    int GetFrequencyForNode(GUBP bp, string NodeToDo, int BaseFrequency)
    {
        return HackFrequency(bp, BranchName, NodeToDo, BaseFrequency);
    }

    List<P4Connection.ChangeRecord> GetChanges(int LastOutputForChanges, int TopCL, int LastGreen)
    {
        var Result = new List<P4Connection.ChangeRecord>();
        if (TopCL > LastGreen)
        {
            if (LastOutputForChanges > 1990000)
            {
                string Cmd = String.Format("{0}@{1},{2} {3}@{4},{5}",
                    CombinePaths(PathSeparator.Slash, P4Env.BuildRootP4, "*", "Source", "..."), LastOutputForChanges + 1, TopCL,
                    CombinePaths(PathSeparator.Slash, P4Env.BuildRootP4, "*", "Build", "..."), LastOutputForChanges + 1, TopCL
                    );
                List<P4Connection.ChangeRecord> ChangeRecords;
				if (P4.Changes(out ChangeRecords, Cmd, false, true, LongComment: true))
                {
                    foreach (var Record in ChangeRecords)
                    {
                        if (!Record.User.Equals("buildmachine", StringComparison.InvariantCultureIgnoreCase))
                        {
                            Result.Add(Record);
                        }
                    }
                }
                else
                {
                    throw new AutomationException("Could not get changes; cmdline: p4 changes {0}", Cmd);
                }
            }
            else
            {
                throw new AutomationException("That CL looks pretty far off {0}", LastOutputForChanges);
            }
        }
        return Result;
    }

    int PrintChanges(int LastOutputForChanges, int TopCL, int LastGreen)
    {
        var ChangeRecords = GetChanges(LastOutputForChanges, TopCL, LastGreen);
        foreach (var Record in ChangeRecords)
        {
            var Summary = Record.Summary.Replace("\r", "\n");
            if (Summary.IndexOf("\n") > 0)
            {
                Summary = Summary.Substring(0, Summary.IndexOf("\n"));
            }
            Log("         {0} {1} {2}", Record.CL, Record.UserEmail, Summary);
        }
        return TopCL;
    }

    void PrintDetailedChanges(NodeHistory History, bool bShowAllChanges = false)
    {
        var StartTime = DateTime.UtcNow;

        string Me = String.Format("{0}   <<<< local sync", P4Env.Changelist);
        int LastOutputForChanges = 0;
        int LastGreen = History.LastSucceeded;
        if (bShowAllChanges)
        {
            if (History.AllStarted.Count > 0)
            {
                LastGreen = History.AllStarted[0];
            }
        }
        foreach (var cl in History.AllStarted)
        {
            if (cl < LastGreen)
            {
                continue;
            }
            if (P4Env.Changelist < cl && Me != "")
            {
                LastOutputForChanges = PrintChanges(LastOutputForChanges, P4Env.Changelist, LastGreen);
                Log("         {0}", Me);
                Me = "";
            }
            string Status = "In Process";
            if (History.AllSucceeded.Contains(cl))
            {
                Status = "ok";
            }
            if (History.AllFailed.Contains(cl))
            {
                Status = "FAIL";
            }
            LastOutputForChanges = PrintChanges(LastOutputForChanges, cl, LastGreen);
            Log("         {0}   {1}", cl, Status);
        }
        if (Me != "")
        {
            LastOutputForChanges = PrintChanges(LastOutputForChanges, P4Env.Changelist, LastGreen);
            Log("         {0}", Me);
        }
        var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
        Log("Took {0}s to get P4 history", BuildDuration / 1000);

    }
    void PrintNodes(GUBP bp, List<string> Nodes, bool LocalOnly, List<string> UnfinishedTriggers = null)
    {
        bool bShowAllChanges = bp.ParseParam("AllChanges") && GUBPNodesHistory != null;
        bool bShowChanges = (bp.ParseParam("Changes") && GUBPNodesHistory != null) || bShowAllChanges;
        bool bShowDetailedHistory = (bp.ParseParam("History") && GUBPNodesHistory != null) || bShowChanges;
        bool bShowDependencies = bp.ParseParam("ShowDependencies");
		bool bShowDependednOn = bp.ParseParam("ShowDependedOn");
		bool bShowDependentPromotions = bp.ParseParam("ShowDependentPromotions");
        bool bShowECDependencies = bp.ParseParam("ShowECDependencies");
        bool bShowHistory = !bp.ParseParam("NoHistory") && GUBPNodesHistory != null;
        bool AddEmailProps = bp.ParseParam("ShowEmails");
        bool ECProc = bp.ParseParam("ShowECProc");
        bool ECOnly = bp.ParseParam("ShowECOnly");
        bool bShowTriggers = true;
        string LastControllingTrigger = "";
        string LastAgentGroup = "";
        foreach (var NodeToDo in Nodes)
        {
            if (ECOnly && !GUBPNodes[NodeToDo].RunInEC())
            {
                continue;
            }
            string EMails = "";
            if (AddEmailProps)
            {
                EMails = GetEMailListForNode(bp, NodeToDo, "", "");
            }
            if (bShowTriggers)
            {
                string MyControllingTrigger = GetControllingTriggerDotName(NodeToDo);
                if (MyControllingTrigger != LastControllingTrigger)
                {
                    LastControllingTrigger = MyControllingTrigger;
                    if (MyControllingTrigger != "")
                    {
                        string Finished = "";
                        if (UnfinishedTriggers != null)
                        {
                            string MyShortControllingTrigger = GetControllingTrigger(NodeToDo);
                            if (UnfinishedTriggers.Contains(MyShortControllingTrigger))
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
            }
            if (GUBPNodes[NodeToDo].AgentSharingGroup != LastAgentGroup && GUBPNodes[NodeToDo].AgentSharingGroup != "")
            {
                Log("    Agent Group: {0}", GUBPNodes[NodeToDo].AgentSharingGroup);
            }
            LastAgentGroup = GUBPNodes[NodeToDo].AgentSharingGroup;

            string Agent = GUBPNodes[NodeToDo].ECAgentString();
			if(ParseParamValue("AgentOverride") != "" && !GUBPNodes[NodeToDo].GetFullName().Contains("Mac"))
			{
				Agent = ParseParamValue("AgentOverride");
			}
            if (Agent != "")
            {
                Agent = "[" + Agent + "]";
            }
            string MemoryReq = "[" + GUBPNodes[NodeToDo].AgentMemoryRequirement(bp).ToString() + "]";            
            if(MemoryReq == "[0]")
            {
                MemoryReq = "";                
            }
            string FrequencyString = CISFrequencyQuantumShiftString(NodeToDo);

            Log("      {0}{1}{2}{3}{4}{5}{6} {7}  {8}",
                (LastAgentGroup != "" ? "  " : ""),
                NodeToDo,
                FrequencyString,
                NodeIsAlreadyComplete(NodeToDo, LocalOnly) ? " - (Completed)" : "",
                GUBPNodes[NodeToDo].TriggerNode() ? " - (TriggerNode)" : "",
                GUBPNodes[NodeToDo].IsSticky() ? " - (Sticky)" : "",				
                Agent,
                MemoryReq,
                EMails,
                ECProc ? GUBPNodes[NodeToDo].ECProcedure() : ""
                );
            if (bShowHistory && GUBPNodesHistory.ContainsKey(NodeToDo))
            {
                var History = GUBPNodesHistory[NodeToDo];

                if (bShowDetailedHistory)
                {
                    PrintDetailedChanges(History, bShowAllChanges);
                }
                else
                {
                    Log("         Last Success: {0}", History.LastSucceeded);
                    Log("         Last Fail   : {0}", History.LastFailed);
                    Log("          Fails Since: {0}", History.FailedString);
                    Log("     InProgress Since: {0}", History.InProgressString);					
                }
            }
            if (bShowDependencies)
            {
                foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
                {
                    Log("            dep> {0}", Dep);
                }
                foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
                {
                    Log("           pdep> {0}", Dep);
                }
				foreach (var Dep in GUBPNodes[NodeToDo].CompletedDependencies)
				{
					Log("			cdep>{0}", Dep);
				}
            }
            if (bShowECDependencies)
            {
                foreach (var Dep in GetECDependencies(NodeToDo))
                {
                    Log("           {0}", Dep);
                }
				foreach (var Dep in GetCompletedOnlyDependencies(NodeToDo))
				{
					Log("           compDep> {0}", Dep);
				}
            }

			if(bShowDependednOn)
			{
				foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependedOn)
				{
					Log("            depOn> {0}", Dep);
				}
			}
			if (bShowDependentPromotions)
			{
				foreach (var Dep in GUBPNodes[NodeToDo].DependentPromotions)
				{
					Log("            depPro> {0}", Dep);
				}
			}			
        }
    }
    public void SaveGraphVisualization(List<string> Nodes)
    {
        var GraphNodes = new List<GraphNode>();

        var NodeToGraphNodeMap = new Dictionary<string, GraphNode>();

        for (var NodeIndex = 0; NodeIndex < Nodes.Count; ++NodeIndex)
        {
            var Node = Nodes[NodeIndex];

            var GraphNode = new GraphNode()
            {
                Id = GraphNodes.Count,
                Label = Node
            };
            GraphNodes.Add(GraphNode);
            NodeToGraphNodeMap.Add(Node, GraphNode);
        }

        // Connect everything together
        var GraphEdges = new List<GraphEdge>();

        for (var NodeIndex = 0; NodeIndex < Nodes.Count; ++NodeIndex)
        {
            var Node = Nodes[NodeIndex];
            GraphNode NodeGraphNode = NodeToGraphNodeMap[Node];

            foreach (var Dep in GUBPNodes[Node].FullNamesOfDependencies)
            {
                GraphNode PrerequisiteFileGraphNode;
                if (NodeToGraphNodeMap.TryGetValue(Dep, out PrerequisiteFileGraphNode))
                {
                    // Connect a file our action is dependent on, to our action itself
                    var NewGraphEdge = new GraphEdge()
                    {
                        Id = GraphEdges.Count,
                        Source = PrerequisiteFileGraphNode,
                        Target = NodeGraphNode,
                        Color = new GraphColor() { R = 0.0f, G = 0.0f, B = 0.0f, A = 0.75f }
                    };

                    GraphEdges.Add(NewGraphEdge);
                }

            }
            foreach (var Dep in GUBPNodes[Node].FullNamesOfPseudosependencies)
            {
                GraphNode PrerequisiteFileGraphNode;
                if (NodeToGraphNodeMap.TryGetValue(Dep, out PrerequisiteFileGraphNode))
                {
                    // Connect a file our action is dependent on, to our action itself
                    var NewGraphEdge = new GraphEdge()
                    {
                        Id = GraphEdges.Count,
                        Source = PrerequisiteFileGraphNode,
                        Target = NodeGraphNode,
                        Color = new GraphColor() { R = 0.0f, G = 0.0f, B = 0.0f, A = 0.25f }
                    };

                    GraphEdges.Add(NewGraphEdge);
                }

            }
        }

        string Filename = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "GubpGraph.gexf");
        Log("Writing graph to {0}", Filename);
        GraphVisualization.WriteGraphFile(Filename, "GUBP Nodes", GraphNodes, GraphEdges);
        Log("Wrote graph to {0}", Filename);
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

    public List<UnrealTargetPlatform> GetMonolithicPlatformsForUProject(UnrealTargetPlatform HostPlatform, BranchInfo.BranchUProject GameProj, bool bIncludeHostPlatform)
    {
        UnrealTargetPlatform AltHostPlatform = GetAltHostPlatform(HostPlatform);
        var Result = new List<UnrealTargetPlatform>();
        foreach (var Kind in BranchInfo.MonolithicKinds)
        {
            if (GameProj.Properties.Targets.ContainsKey(Kind))
            {
                var Target = GameProj.Properties.Targets[Kind];
                var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
				var AdditionalPlatforms = Target.Rules.GUBP_GetBuildOnlyPlatforms_MonolithicOnly(HostPlatform);
				var AllPlatforms = Platforms.Union(AdditionalPlatforms);
				foreach (var Plat in AllPlatforms)
                {
                    if (GUBP.bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
                    {
                        continue;
                    }

                    if (ActivePlatforms.Contains(Plat) && Target.Rules.SupportsPlatform(Plat) &&
                        ((Plat != HostPlatform && Plat != AltHostPlatform) || bIncludeHostPlatform))
                    {
                        Result.Add(Plat);
                    }
                }
            }
        }
        return Result;
    }

    List<int> ConvertCLToIntList(List<string> Strings)
    {
        var Result = new List<int>();
        foreach (var ThisString in Strings)
        {
            int ThisInt = int.Parse(ThisString);
            if (ThisInt < 1960000 || ThisInt > 3000000)
            {
                Log("CL {0} appears to be out of range", ThisInt);
            }
            Result.Add(ThisInt);
        }
        Result.Sort();
        return Result;
    }
    void SaveStatus(string NodeToDo, string Suffix, string NodeStoreName, bool bSaveSharedTempStorage, string GameNameIfAny, string JobStepIDForFailure = null)
    {
        string Contents = "Just a status record: " + Suffix;
        if (!String.IsNullOrEmpty(JobStepIDForFailure) && IsBuildMachine)
        {
            try
            {
                Contents = RunECTool(String.Format("getProperties --jobStepId {0} --recurse 1", JobStepIDForFailure), true);
            }
            catch (Exception Ex)
            {
                Log(System.Diagnostics.TraceEventType.Warning, "Failed to get properties for jobstep to save them.");
                Log(System.Diagnostics.TraceEventType.Warning, LogUtils.FormatException(Ex));
            }
        }
        string RecordOfSuccess = CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Logs", NodeToDo + Suffix +".log");
        CreateDirectory(Path.GetDirectoryName(RecordOfSuccess));
        WriteAllText(RecordOfSuccess, Contents);		
		StoreToTempStorage(CmdEnv, NodeStoreName + Suffix, new List<string> { RecordOfSuccess }, !bSaveSharedTempStorage, GameNameIfAny);		
    }
	string GetPropertyFromStep(string PropertyPath)
	{
		string Property = "";
		Property = RunECTool("getProperty \"" + PropertyPath + "\"");
		Property = Property.TrimEnd('\r', '\n');
		return Property;
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

    List<string> TopologicalSort(HashSet<string> NodesToDo, string ExplicitTrigger = "", bool LocalOnly = false, bool SubSort = false, bool DoNotConsiderCompletion = false)
    {
        var StartTime = DateTime.UtcNow;

        var OrdereredToDo = new List<string>();

        var SortedAgentGroupChains = new Dictionary<string, List<string>>();
        if (!SubSort)
        {
            var AgentGroupChains = new Dictionary<string, List<string>>();
            foreach (var NodeToDo in NodesToDo)
            {
                string MyAgentGroup = GUBPNodes[NodeToDo].AgentSharingGroup;
                if (MyAgentGroup != "")
                {
                    if (!AgentGroupChains.ContainsKey(MyAgentGroup))
                    {
                        AgentGroupChains.Add(MyAgentGroup, new List<string> { NodeToDo });
                    }
                    else
                    {
                        AgentGroupChains[MyAgentGroup].Add(NodeToDo);
                    }
                }
            }
            foreach (var Chain in AgentGroupChains)
            {
                SortedAgentGroupChains.Add(Chain.Key, TopologicalSort(new HashSet<string>(Chain.Value), ExplicitTrigger, LocalOnly, true, DoNotConsiderCompletion));
            }
            Log("***************Done with recursion");
        }

        // here we do a topological sort of the nodes, subject to a lexographical and priority sort
        while (NodesToDo.Count > 0)
        {
            bool bProgressMade = false;
            float BestPriority = -1E20f;
            string BestNode = "";
            bool BestPseudoReady = false;
            var NonReadyAgentGroups = new HashSet<string>();
            var NonPeudoReadyAgentGroups = new HashSet<string>();
            var ExaminedAgentGroups = new HashSet<string>();
            foreach (var NodeToDo in NodesToDo)
            {
                bool bReady = true;
				bool bPseudoReady = true;
				bool bCompReady = true;
                if (!SubSort && GUBPNodes[NodeToDo].AgentSharingGroup != "")
                {
                    if (ExaminedAgentGroups.Contains(GUBPNodes[NodeToDo].AgentSharingGroup))
                    {
                        bReady = !NonReadyAgentGroups.Contains(GUBPNodes[NodeToDo].AgentSharingGroup);
                        bPseudoReady = !NonPeudoReadyAgentGroups.Contains(GUBPNodes[NodeToDo].AgentSharingGroup); //this might not be accurate if bReady==false
                    }
                    else
                    {
                        ExaminedAgentGroups.Add(GUBPNodes[NodeToDo].AgentSharingGroup);
                        foreach (var ChainNode in SortedAgentGroupChains[GUBPNodes[NodeToDo].AgentSharingGroup])
                        {
                            foreach (var Dep in GUBPNodes[ChainNode].FullNamesOfDependencies)
                            {
                                if (!GUBPNodes.ContainsKey(Dep))
                                {
                                    throw new AutomationException("Dependency {0} node found.", Dep);
                                }
                                if (!SortedAgentGroupChains[GUBPNodes[NodeToDo].AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    bReady = false;
                                    break;
                                }
                            }
                            if (!bReady)
                            {
                                NonReadyAgentGroups.Add(GUBPNodes[NodeToDo].AgentSharingGroup);
                                break;
                            }
                            foreach (var Dep in GUBPNodes[ChainNode].FullNamesOfPseudosependencies)
                            {
                                if (!GUBPNodes.ContainsKey(Dep))
                                {
                                    throw new AutomationException("Pseudodependency {0} node found.", Dep);
                                }
                                if (!SortedAgentGroupChains[GUBPNodes[NodeToDo].AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    bPseudoReady = false;
                                    NonPeudoReadyAgentGroups.Add(GUBPNodes[NodeToDo].AgentSharingGroup);
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
                    {
                        if (!GUBPNodes.ContainsKey(Dep))
                        {
                            throw new AutomationException("Dependency {0} node found.", Dep);
                        }
                        if (NodesToDo.Contains(Dep))
                        {
                            bReady = false;
                            break;
                        }
                    }
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
                    {
                        if (!GUBPNodes.ContainsKey(Dep))
                        {
                            throw new AutomationException("Pseudodependency {0} node found.", Dep);
                        }
                        if (NodesToDo.Contains(Dep))
                        {
                            bPseudoReady = false;
                            break;
                        }
                    }
					foreach (var Dep in GUBPNodes[NodeToDo].CompletedDependencies)
					{
						if (!GUBPNodes.ContainsKey(Dep))
						{
							throw new AutomationException("Completed Dependency {0} node found.", Dep);
						}
						if (NodesToDo.Contains(Dep))
						{
							bCompReady = false;
							break;
						}
					}
                }
                var Priority = GUBPNodes[NodeToDo].Priority();

                if (bReady && BestNode != "")
                {
                    if (String.Compare(GetControllingTriggerDotName(BestNode), GetControllingTriggerDotName(NodeToDo)) < 0) //sorted by controlling trigger
                    {
                        bReady = false;
                    }
                    else if (String.Compare(GetControllingTriggerDotName(BestNode), GetControllingTriggerDotName(NodeToDo)) == 0) //sorted by controlling trigger
                    {
                        if (GUBPNodes[BestNode].IsSticky() && !GUBPNodes[NodeToDo].IsSticky()) //sticky nodes first
                        {
                            bReady = false;
                        }
                        else if (GUBPNodes[BestNode].IsSticky() == GUBPNodes[NodeToDo].IsSticky())
                        {
                            if (BestPseudoReady && !bPseudoReady)
                            {
                                bReady = false;
                            }
                            else if (BestPseudoReady == bPseudoReady)
                            {
                                bool IamLateTrigger = !DoNotConsiderCompletion && GUBPNodes[NodeToDo].TriggerNode() && NodeToDo != ExplicitTrigger && !NodeIsAlreadyComplete(NodeToDo, LocalOnly);
                                bool BestIsLateTrigger = !DoNotConsiderCompletion && GUBPNodes[BestNode].TriggerNode() && BestNode != ExplicitTrigger && !NodeIsAlreadyComplete(BestNode, LocalOnly);
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
                                        if (BestNode.CompareTo(NodeToDo) < 0)
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
                if (!SubSort && GUBPNodes[BestNode].AgentSharingGroup != "")
                {
                    foreach (var ChainNode in SortedAgentGroupChains[GUBPNodes[BestNode].AgentSharingGroup])
                    {
                        OrdereredToDo.Add(ChainNode);
                        NodesToDo.Remove(ChainNode);
                    }
                }
                else
                {
                    OrdereredToDo.Add(BestNode);
                    NodesToDo.Remove(BestNode);
                }
            }

            if (!bProgressMade && NodesToDo.Count > 0)
            {
                Log("Cycle in GUBP, could not resolve:");
                foreach (var NodeToDo in NodesToDo)
                {
                    string Deps = "";
                    if (!SubSort && GUBPNodes[NodeToDo].AgentSharingGroup != "")
                    {
                        foreach (var ChainNode in SortedAgentGroupChains[GUBPNodes[NodeToDo].AgentSharingGroup])
                        {
                            foreach (var Dep in GUBPNodes[ChainNode].FullNamesOfDependencies)
                            {
                                if (!SortedAgentGroupChains[GUBPNodes[NodeToDo].AgentSharingGroup].Contains(Dep) && NodesToDo.Contains(Dep))
                                {
                                    Deps = Deps + Dep + "[" + ChainNode + "->" + GUBPNodes[NodeToDo].AgentSharingGroup + "]" + " ";
                                }
                            }
                        }
                    }
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            Deps = Deps + Dep + " ";
                        }
                    }
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
                    {
                        if (NodesToDo.Contains(Dep))
                        {
                            Deps = Deps + Dep + " ";
                        }
                    }
                    Log("  {0}    deps: {1}", NodeToDo, Deps);
                }
                throw new AutomationException("Cycle in GUBP");
            }
        }
        if (!SubSort)
        {
            var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
            Log("Took {0}s to sort {1} nodes", BuildDuration / 1000, OrdereredToDo.Count);
        }

        return OrdereredToDo;
    }

    string GetJobStepPath(string Dep)
    {
        if (Dep != "Noop" && GUBPNodes[Dep].AgentSharingGroup != "")
        {
            return "jobSteps[" + GUBPNodes[Dep].AgentSharingGroup + "]/jobSteps[" + Dep + "]";
        }
        return "jobSteps[" + Dep + "]";
    }
    string GetJobStep(string ParentPath, string Dep)
    {
        return ParentPath + "/" + GetJobStepPath(Dep);
    }

    void UpdateNodeHistory(string Node, string CLString)
    {
        if (GUBPNodes[Node].RunInEC() && !GUBPNodes[Node].TriggerNode() && CLString != "")
        {
            string GameNameIfAny = GUBPNodes[Node].GameNameIfAnyForTempStorage();
            string NodeStoreWildCard = StoreName.Replace(CLString, "*") + "-" + GUBPNodes[Node].GetFullName();
            var History = new NodeHistory();

            History.AllStarted = ConvertCLToIntList(FindTempStorageManifests(CmdEnv, NodeStoreWildCard + StartedTempStorageSuffix, false, true, GameNameIfAny));
            History.AllSucceeded = ConvertCLToIntList(FindTempStorageManifests(CmdEnv, NodeStoreWildCard + SucceededTempStorageSuffix, false, true, GameNameIfAny));
            History.AllFailed = ConvertCLToIntList(FindTempStorageManifests(CmdEnv, NodeStoreWildCard + FailedTempStorageSuffix, false, true, GameNameIfAny));

            if (History.AllFailed.Count > 0)
            {
                History.LastFailed = History.AllFailed[History.AllFailed.Count - 1];
            }
            if (History.AllSucceeded.Count > 0)
            {
                History.LastSucceeded = History.AllSucceeded[History.AllSucceeded.Count - 1];

                foreach (var Failed in History.AllFailed)
                {
                    if (Failed > History.LastSucceeded)
                    {
                        History.Failed.Add(Failed);
                        History.FailedString = GUBPNode.MergeSpaceStrings(History.FailedString, String.Format("{0}", Failed));
                    }
                }
                foreach (var Started in History.AllStarted)
                {
                    if (Started > History.LastSucceeded && !History.Failed.Contains(Started))
                    {
                        History.InProgress.Add(Started);
                        History.InProgressString = GUBPNode.MergeSpaceStrings(History.InProgressString, String.Format("{0}", Started));
                    }
                }                
            }
			if (GUBPNodesHistory.ContainsKey(Node))
			{
				GUBPNodesHistory.Remove(Node);
			}
			GUBPNodesHistory.Add(Node, History);
        }
    }
	void GetFailureEmails(string NodeToDo, string CLString, bool OnlyLateUpdates = false)
	{
		var StartTime = DateTime.UtcNow;
        string EMails;
        string FailCauserEMails = "";
        string EMailNote = "";
        bool SendSuccessForGreenAfterRed = false;
        int NumPeople = 0;
        if (GUBPNodesHistory.ContainsKey(NodeToDo))
        {
            var History = GUBPNodesHistory[NodeToDo];
			RunECTool(String.Format("setProperty \"/myWorkflow/LastGreen/{0}\" \"{1}\"", NodeToDo, History.LastSucceeded), true);
			RunECTool(String.Format("setProperty \"/myWorkflow/LastGreen/{0}\" \"{1}\"", NodeToDo, History.FailedString), true);

            if (History.LastSucceeded > 0 && History.LastSucceeded < P4Env.Changelist)
            {
                int LastNonDuplicateFail = P4Env.Changelist;
                try
                {
                    if (OnlyLateUpdates)
                    {
                        LastNonDuplicateFail = FindLastNonDuplicateFail(NodeToDo, CLString);
                        if (LastNonDuplicateFail < P4Env.Changelist)
                        {
                            Log("*** Red-after-red spam reduction, changed CL {0} to CL {1} because the errors didn't change.", P4Env.Changelist, LastNonDuplicateFail);
                        }
                    }
                }
                catch (Exception Ex)
                {
                    LastNonDuplicateFail = P4Env.Changelist;
                    Log(System.Diagnostics.TraceEventType.Warning, "Failed to FindLastNonDuplicateFail.");
                    Log(System.Diagnostics.TraceEventType.Warning, LogUtils.FormatException(Ex));
                }

                var ChangeRecords = GetChanges(History.LastSucceeded, LastNonDuplicateFail, History.LastSucceeded);
                foreach (var Record in ChangeRecords)
                {
                    FailCauserEMails = GUBPNode.MergeSpaceStrings(FailCauserEMails, Record.UserEmail);
                }
                if (!String.IsNullOrEmpty(FailCauserEMails))
                {
                    NumPeople++;
                    foreach (var AChar in FailCauserEMails.ToCharArray())
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
        else
        {
			RunECTool(String.Format("setProperty \"/myWorkflow/LastGreen/{0}\" \"{1}\"", NodeToDo, "0"));
			RunECTool(String.Format("setProperty \"/myWorkflow/RedsSince/{0}\" \"{1}\"", NodeToDo, ""));
        }
		RunECTool(String.Format("setProperty \"/myWorkflow/FailCausers/{0}\" \"{1}\"", NodeToDo, FailCauserEMails));
		RunECTool(String.Format("setProperty \"/myWorkflow/EmailNotes/{0}\" \"{1}\"", NodeToDo, EMailNote));
        {
            var AdditionalEmails = "";
			string Causers = "";
            if (ParseParam("CIS") && !GUBPNodes[NodeToDo].SendSuccessEmail() && !GUBPNodes[NodeToDo].TriggerNode())
            {
				Causers = FailCauserEMails;
            }
            string AddEmails = ParseParamValue("AddEmails");
            if (!String.IsNullOrEmpty(AddEmails))
            {
                AdditionalEmails = GUBPNode.MergeSpaceStrings(AddEmails, AdditionalEmails);
            }
			EMails = GetEMailListForNode(this, NodeToDo, AdditionalEmails, Causers);
            RunECTool(String.Format("setProperty \"/myWorkflow/FailEmails/{0}\" \"{1}\"", NodeToDo, EMails));            
        }
		if (GUBPNodes[NodeToDo].SendSuccessEmail() || SendSuccessForGreenAfterRed)
		{
			RunECTool(String.Format("setProperty \"/myWorkflow/SendSuccessEmail/{0}\" \"{1}\"", NodeToDo, "1"));
		}
		else
		{
			RunECTool(String.Format("setProperty \"/myWorkflow/SendSuccessEmail/{0}\" \"{1}\"", NodeToDo, "0"));
		}
	}

    bool HashSetEqual(HashSet<string> A, HashSet<string> B)
    {
        if (A.Count != B.Count)
        {
            return false;
        }
        foreach (var Elem in A)
        {
            if (!B.Contains(Elem))
            {
                return false;
            }
        }
        foreach (var Elem in B)
        {
            if (!A.Contains(Elem))
            {
                return false;
            }
        }
        return true;
    }

    int FindLastNonDuplicateFail(string NodeToDo, string CLString)
    {
        var History = GUBPNodesHistory[NodeToDo];
        int Result = P4Env.Changelist;

        string GameNameIfAny = GUBPNodes[NodeToDo].GameNameIfAnyForTempStorage();
        string NodeStore = StoreName + "-" + GUBPNodes[NodeToDo].GetFullName() + FailedTempStorageSuffix;

        var BackwardsFails = new List<int>(History.AllFailed);
        BackwardsFails.Add(P4Env.Changelist);
        BackwardsFails.Sort();
        BackwardsFails.Reverse();
        HashSet<string> CurrentErrors = null;
        foreach (var CL in BackwardsFails)
        {
            if (CL > P4Env.Changelist)
            {
                continue;
            }
            if (CL <= History.LastSucceeded)
            {
                break;
            }
            var ThisNodeStore = NodeStore.Replace(CLString, String.Format("{0}", CL));
            DeleteLocalTempStorage(CmdEnv, ThisNodeStore, true); // these all clash locally, which is fine we just retrieve them from shared

            List<string> Files = null;
            try
            {
                bool WasLocal;
                Files = RetrieveFromTempStorage(CmdEnv, ThisNodeStore, out WasLocal, GameNameIfAny); // this will fail on our CL if we didn't fail or we are just setting up the branch
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
            var ThisErrors = ECJobPropsUtils.ErrorsFromProps(ErrorFile);
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
    List<string> GetECPropsForNode(string NodeToDo, string CLString, out string EMails, bool OnlyLateUpdates = false)
    {
        var StartTime = DateTime.UtcNow;

        var ECProps = new List<string>();
        EMails = "";		
		var AdditonalEmails = "";
		string Causers = "";				
		string AddEmails = ParseParamValue("AddEmails");
		if (!String.IsNullOrEmpty(AddEmails))
		{
			AdditonalEmails = GUBPNode.MergeSpaceStrings(AddEmails, AdditonalEmails);
		}
		EMails = GetEMailListForNode(this, NodeToDo, AdditonalEmails, Causers);
		ECProps.Add("FailEmails/" + NodeToDo + "=" + EMails);
	
		if (!OnlyLateUpdates)
		{
			string AgentReq = GUBPNodes[NodeToDo].ECAgentString();
			if(ParseParamValue("AgentOverride") != "" && !GUBPNodes[NodeToDo].GetFullName().Contains("OnMac"))
			{
				AgentReq = ParseParamValue("AgentOverride");
			}
			ECProps.Add(string.Format("AgentRequirementString/{0}={1}", NodeToDo, AgentReq));
			ECProps.Add(string.Format("RequiredMemory/{0}={1}", NodeToDo, GUBPNodes[NodeToDo].AgentMemoryRequirement(this)));
			ECProps.Add(string.Format("Timeouts/{0}={1}", NodeToDo, GUBPNodes[NodeToDo].TimeoutInMinutes()));
			ECProps.Add(string.Format("JobStepPath/{0}={1}", NodeToDo, GetJobStepPath(NodeToDo)));
		}
		
        var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
        return ECProps;
    }

    void UpdateECProps(string NodeToDo, string CLString)
    {
        try
        {
            Log("Updating node props for node {0}", NodeToDo);
			string EMails = "";
            var Props = GetECPropsForNode(NodeToDo, CLString, out EMails, true);
            foreach (var Prop in Props)
            {
                var Parts = Prop.Split("=".ToCharArray());
                RunECTool(String.Format("setProperty \"/myWorkflow/{0}\" \"{1}\"", Parts[0], Parts[1]), true);
            }			
        }
        catch (Exception Ex)
        {
            Log(System.Diagnostics.TraceEventType.Warning, "Failed to UpdateECProps.");
            Log(System.Diagnostics.TraceEventType.Warning, LogUtils.FormatException(Ex));
        }
    }
	void UpdateECBuildTime(string NodeToDo, double BuildDuration)
	{
		try
		{
			Log("Updating duration prop for node {0}", NodeToDo);
			RunECTool(String.Format("setProperty \"/myWorkflow/NodeDuration/{0}\" \"{1}\"", NodeToDo, BuildDuration.ToString()));
			RunECTool(String.Format("setProperty \"/myJobStep/NodeDuration\" \"{0}\"", BuildDuration.ToString()));
		}
		catch (Exception Ex)
		{
			Log(System.Diagnostics.TraceEventType.Warning, "Failed to UpdateECBuildTime.");
			Log(System.Diagnostics.TraceEventType.Warning, LogUtils.FormatException(Ex));
		}
	}

    [Help("Runs one, several or all of the GUBP nodes")]
    [Help(typeof(UE4Build))]
    [Help("NoMac", "Toggle to exclude the Mac host platform, default is Win64+Mac+Linux")]
	[Help("NoLinux", "Toggle to exclude the Linux (PC, 64-bit) host platform, default is Win64+Mac+Linux")]
	[Help("NoPC", "Toggle to exclude the PC host platform, default is Win64+Mac+Linux")]
    [Help("CleanLocal", "delete the local temp storage before we start")]
    [Help("Store=", "Sets the name of the temp storage block, normally, this is built for you.")]
    [Help("StoreSuffix=", "Tacked onto a store name constructed from CL, branch, etc")]
    [Help("TimeIndex=", "An integer used to determine subsets to run based on DependentCISFrequencyQuantumShift")]
    [Help("UserTimeIndex=", "An integer used to determine subsets to run based on DependentCISFrequencyQuantumShift, this one overrides TimeIndex")]
    [Help("PreflightUID=", "A unique integer tag from EC used as part of the tempstorage, builds and label names to distinguish multiple attempts.")]
    [Help("Node=", "Nodes to process, -node=Node1+Node2+Node3, if no nodes or games are specified, defaults to all nodes.")]
    [Help("SetupNode=", "Like -Node, but only applies with CommanderJobSetupOnly")]
    [Help("RelatedToNode=", "Nodes to process, -RelatedToNode=Node1+Node2+Node3, use all nodes that either depend on these nodes or these nodes depend on them.")]
    [Help("SetupRelatedToNode=", "Like -RelatedToNode, but only applies with CommanderJobSetupOnly")]
    [Help("OnlyNode=", "Nodes to process NO dependencies, -OnlyNode=Node1+Node2+Node3, if no nodes or games are specified, defaults to all nodes.")]
    [Help("TriggerNode=", "Trigger Nodes to process, -triggernode=Node.")]
    [Help("Game=", "Games to process, -game=Game1+Game2+Game3, if no games or nodes are specified, defaults to all nodes.")]
    [Help("ListOnly", "List Nodes in this branch")]
    [Help("SaveGraph", "Save graph as an xml file")]
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
    [Help("BuildRocket", "Build in rocket mode.")]
    [Help("ShowECOnly", "Only show EC nodes.")]
    [Help("ECProject", "From EC, the name of the project, used to get a version number.")]
    [Help("CIS", "This is a CIS run, assign TimeIndex based on the history.")]
    [Help("ForceIncrementalCompile", "make sure all compiles are incremental")]
    [Help("AutomatedTesting", "Allow automated testing, currently disabled.")]
    [Help("StompCheck", "Look for stomped build products.")]

    public override void ExecuteBuild()
    {
        Log("************************* GUBP");
        string PreflightShelveCLString = GetEnvVar("uebp_PreflightShelveCL");
        if ((!String.IsNullOrEmpty(PreflightShelveCLString) && IsBuildMachine) || ParseParam("PreflightTest"))
        {
            Log("**** Preflight shelve {0}", PreflightShelveCLString);
            if (!String.IsNullOrEmpty(PreflightShelveCLString))
            {
                PreflightShelveCL = int.Parse(PreflightShelveCLString);
                if (PreflightShelveCL < 2000000)
                {
                    throw new AutomationException(String.Format( "{0} does not look like a CL", PreflightShelveCL));
                }
            }
            bPreflightBuild = true;
        }
        
        ECProject = ParseParamValue("ECProject");
        if (ECProject == null)
        {
            ECProject = "";
        }		
        HostPlatforms = new List<UnrealTargetPlatform>();
        if (!ParseParam("NoPC"))
        {
            HostPlatforms.Add(UnrealTargetPlatform.Win64);
        }
        if (P4Enabled)
        {
            BranchName = P4Env.BuildRootP4;
        }
		else
		{ 
			BranchName = ParseParamValue("BranchName", "");
        }
        BranchOptions = GetBranchOptions(BranchName);
        bool WithMac = !BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.Mac);
        if (ParseParam("NoMac"))
        {
            WithMac = false;
        }
        if (WithMac)
        {
            HostPlatforms.Add(UnrealTargetPlatform.Mac);
        }

		bool WithLinux = !BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.Linux);
		bool WithoutLinux = ParseParam("NoLinux");
		// @TODO: exclude temporarily unless running on a Linux machine to prevent spurious GUBP failures
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux || ParseParam("NoLinux"))
		{
			WithLinux = false;
		}
		if (WithLinux)
		{
			HostPlatforms.Add(UnrealTargetPlatform.Linux);
		}

        bForceIncrementalCompile = ParseParam("ForceIncrementalCompile");
        bool bNoAutomatedTesting = ParseParam("NoAutomatedTesting") || BranchOptions.bNoAutomatedTesting;		
        StoreName = ParseParamValue("Store");
        string StoreSuffix = ParseParamValue("StoreSuffix", "");

        if (bPreflightBuild)
        {
            int PreflightUID = ParseParamInt("PreflightUID", 0);
            PreflightMangleSuffix = String.Format("-PF-{0}-{1}", PreflightShelveCL, PreflightUID);
            StoreSuffix = StoreSuffix + PreflightMangleSuffix;
        }
        CL = ParseParamInt("CL", 0);
        bool bCleanLocalTempStorage = ParseParam("CleanLocal");
        bool bChanges = ParseParam("Changes") || ParseParam("AllChanges");
        bool bHistory = ParseParam("History") || bChanges;
        bool bListOnly = ParseParam("ListOnly") || bHistory;
        bool bSkipTriggers = ParseParam("SkipTriggers");
        bFake = ParseParam("fake");
        bool bFakeEC = ParseParam("FakeEC");
        TimeIndex = ParseParamInt("TimeIndex", 0);
        if (TimeIndex == 0)
        {
            TimeIndex = ParseParamInt("UserTimeIndex", 0);
        }

        bNoIOSOnPC = HostPlatforms.Contains(UnrealTargetPlatform.Mac);

        bool bSaveSharedTempStorage = false;

        if (bHistory && !P4Enabled)
        {
            throw new AutomationException("-Changes and -History require -P4.");
        }
        bool LocalOnly = true;
        string CLString = "";
        if (String.IsNullOrEmpty(StoreName))
        {
            if (P4Enabled)
            {
                if (CL == 0)
                {
                    CL = P4Env.Changelist;
                }
                CLString = String.Format("{0}", CL);
                StoreName = P4Env.BuildRootEscaped + "-" + CLString;
                bSaveSharedTempStorage = CommandUtils.IsBuildMachine;
                LocalOnly = false;
            }
            else
            {
                StoreName = "TempLocal";
                bSaveSharedTempStorage = false;
            }
        }
        StoreName = StoreName + StoreSuffix;
        if (bFakeEC)
        {
            LocalOnly = true;
        } 
        if (bSaveSharedTempStorage)
        {
            if (!HaveSharedTempStorage(true))
            {
                throw new AutomationException("Request to save to temp storage, but {0} is unavailable.", UE4TempStorageDirectory());
            }
            bSignBuildProducts = true;
        }
        else if (!LocalOnly && !HaveSharedTempStorage(false))
        {
            Log("Looks like we want to use shared temp storage, but since we don't have it, we won't use it.");
            LocalOnly = true;
        }

        bool CommanderSetup = ParseParam("CommanderJobSetupOnly");
        string ExplicitTrigger = "";
        if (CommanderSetup)
        {
            ExplicitTrigger = ParseParamValue("TriggerNode");
            if (ExplicitTrigger == null)
            {
                ExplicitTrigger = "";
            }
        }

        if (ParseParam("CIS") && ExplicitTrigger == "" && CommanderSetup) // explicit triggers will already have a time index assigned
        {
            if (!P4Enabled)
            {
                throw new AutomationException("Can't have -CIS without P4 support");
            }
            var P4IndexFileP4 = CombinePaths(PathSeparator.Slash, CommandUtils.P4Env.BuildRootP4, "Engine", "Build", "CISCounter.txt");
            var P4IndexFileLocal = CombinePaths(CmdEnv.LocalRoot, "Engine", "Build", "CISCounter.txt");
            int Retry = 0;
            bool bDone = false;
            while (++Retry < 20 && !bDone)
            {
                int NowMinutes = (int)((DateTime.UtcNow - new DateTime(2014, 1, 1)).TotalMinutes);
                if (NowMinutes < 3 * 30 * 24)
                {
                    throw new AutomationException("bad date calc");
                }
                if (!FileExists_NoExceptions(P4IndexFileLocal))
                {
                    Log("{0} doesn't exist, checking in a new one", P4IndexFileP4);
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
                        Log("Add of CIS counter failed, assuming it now exists.");
                        if (WorkingCL > 0)
                        {
							P4.DeleteChange(WorkingCL);
                        }
                    }
                }
				P4.Sync("-f " + P4IndexFileP4 + "#head");
                if (!FileExists_NoExceptions(P4IndexFileLocal))
                {
                    Log("{0} doesn't exist, checking in a new one", P4IndexFileP4);
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
                        Log("Add of CIS counter failed, assuming it now exists.");
                        if (WorkingCL > 0)
                        {
                            P4.DeleteChange(WorkingCL);
                        }
                    }
                }
                var Data = ReadAllText(P4IndexFileLocal);
                var Parts = Data.Split(" ".ToCharArray());
                int Index = int.Parse(Parts[0]);
                int Minutes = int.Parse(Parts[1]);

                int DeltaMinutes = NowMinutes - Minutes;

                int TimeQuantum = 20;
				if(BranchOptions.QuantumOverride != 0)
				{
					TimeQuantum = BranchOptions.QuantumOverride;
				}
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
                {
                    var Line = String.Format("{0} {1}", NewIndex, NowMinutes);
                    Log("Attempting to write {0} with {1}", P4IndexFileP4, Line);
                    int WorkingCL = -1;
                    try
                    {
						WorkingCL = P4.CreateChange(P4Env.Client, "Updating CIS Counter");
						P4.Edit(WorkingCL, P4IndexFileP4);
                        WriteAllText(P4IndexFileLocal, Line);
                        int SubmittedCL;
						P4.Submit(WorkingCL, out SubmittedCL);
                        bDone = true;
                        TimeIndex = NewIndex;
                    }
                    catch (Exception)
                    {
                        Log("Edit of CIS counter failed, assuming someone else checked in, retrying.");
                        if (WorkingCL > 0)
                        {
							P4.DeleteChange(WorkingCL);
                        }
                        System.Threading.Thread.Sleep(30000);
                    }
                }
            }
            if (!bDone)
            {
                throw new AutomationException("Failed to update the CIS counter after 20 tries.");
            }
            Log("Setting TimeIndex to {0}", TimeIndex);
        }

        Log("************************* CL:			                    {0}", CL); 
        Log("************************* P4Enabled:			            {0}", P4Enabled);
        foreach (var HostPlatform in HostPlatforms)
        {
            Log("*************************   HostPlatform:		            {0}", HostPlatform.ToString()); 
        }
        Log("************************* StoreName:		                {0}", StoreName.ToString());
        Log("************************* bCleanLocalTempStorage:		    {0}", bCleanLocalTempStorage);
        Log("************************* bSkipTriggers:		            {0}", bSkipTriggers);
        Log("************************* bSaveSharedTempStorage:		    {0}", bSaveSharedTempStorage);
        Log("************************* bSignBuildProducts:		        {0}", bSignBuildProducts);
        Log("************************* bFake:           		        {0}", bFake);
        Log("************************* bFakeEC:           		        {0}", bFakeEC);
        Log("************************* bHistory:           		        {0}", bHistory);
        Log("************************* TimeIndex:           		    {0}", TimeIndex);        


        GUBPNodes = new Dictionary<string, GUBPNode>();        
        Branch = new BranchInfo(HostPlatforms);        
        if (IsBuildMachine || ParseParam("AllPlatforms"))
        {
            ActivePlatforms = new List<UnrealTargetPlatform>();
            
			List<BranchInfo.BranchUProject> BranchCodeProjects = new List<BranchInfo.BranchUProject>();
			BranchCodeProjects.Add(Branch.BaseEngineProject);
			BranchCodeProjects.AddRange(Branch.CodeProjects);
			BranchCodeProjects.RemoveAll(Project => BranchOptions.ExcludeNodes.Contains(Project.GameName));

			foreach (var GameProj in BranchCodeProjects)
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
            Log("Active Platform: {0}", Plat.ToString());
        }

        if (HostPlatforms.Count >= 2)
        {
            // make sure each project is set up with the right assumptions on monolithics that prefer a platform.
            foreach (var CodeProj in Branch.CodeProjects)
            {
                var OptionsMac = CodeProj.Options(UnrealTargetPlatform.Mac);
                var OptionsPC = CodeProj.Options(UnrealTargetPlatform.Win64);

                var MacMonos = GetMonolithicPlatformsForUProject(UnrealTargetPlatform.Mac, CodeProj, false);
                var PCMonos = GetMonolithicPlatformsForUProject(UnrealTargetPlatform.Win64, CodeProj, false);

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


        AddNode(new VersionFilesNode());


        foreach (var HostPlatform in HostPlatforms)
        {
			AddNode(new ToolsForCompileNode(HostPlatform));
			
			if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform))
			{
				AddNode(new RootEditorNode(HostPlatform));			
				AddNode(new ToolsNode(HostPlatform));            
				AddNode(new InternalToolsNode(HostPlatform));
			if (HostPlatform == UnrealTargetPlatform.Win64 && ActivePlatforms.Contains(UnrealTargetPlatform.Linux))
				{
					if (!BranchOptions.ExcludePlatformsForEditor.Contains(UnrealTargetPlatform.Linux))
					{
						AddNode(new ToolsCrossCompileNode(HostPlatform));
					}
				}
				foreach (var ProgramTarget in Branch.BaseEngineProject.Properties.Programs)
				{
					bool bInternalOnly;
					bool SeparateNode;
					bool CrossCompile;

					if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && SeparateNode)
					{
						if (bInternalOnly)
						{
							AddNode(new SingleInternalToolsNode(HostPlatform, ProgramTarget));						
						}
						else
						{
							AddNode(new SingleToolsNode(HostPlatform, ProgramTarget));
						}				
					}
					if (ProgramTarget.Rules.GUBP_IncludeNonUnityToolTest())
					{
						AddNode(new NonUnityToolNode(HostPlatform, ProgramTarget));
					}
				}
				foreach(var CodeProj in Branch.CodeProjects)
				{
					foreach(var ProgramTarget in CodeProj.Properties.Programs)
					{
						bool bInternalNodeOnly;
						bool SeparateNode;
						bool CrossCompile;

						if(ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalNodeOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && SeparateNode)
						{
							if(bInternalNodeOnly)
							{
								AddNode(new SingleInternalToolsNode(HostPlatform, ProgramTarget));
							}
							else
							{
								AddNode(new SingleToolsNode(HostPlatform, ProgramTarget));
							}
						}
						if(ProgramTarget.Rules.GUBP_IncludeNonUnityToolTest())
						{
							AddNode(new NonUnityToolNode(HostPlatform, ProgramTarget));
						}
					}
				}

				AddNode(new EditorAndToolsNode(this, HostPlatform));

				if (bOrthogonalizeEditorPlatforms)
				{
					foreach (var Plat in ActivePlatforms)
					{
						if (Plat != HostPlatform && Plat != GetAltHostPlatform(HostPlatform))
						{
							if (Platform.Platforms[HostPlatform].CanHostPlatform(Plat))
							{
								AddNode(new EditorPlatformNode(HostPlatform, Plat));
							}
						}
					}
				}
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
                        Log(System.Diagnostics.TraceEventType.Information, "{0} was listed as a codeless project by GUBP_NonCodeProjects_BaseEditorTypeOnly, however it does not exist in this branch.", Codeless.Key);
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
                var HostMonos = GetMonolithicPlatformsForUProject(HostPlatform, Branch.BaseEngineProject, true);

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
                        Log(System.Diagnostics.TraceEventType.Information, "{0} was listed as a codeless formal build GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly, however it does not exist in this branch.", Codeless.Key);
                    }
                }
            }

            DoASharedPromotable = NumSharedCode > 0 || NonCodeProjectNames.Count > 0 || NonCodeFormalBuilds.Count > 0;

			AddNode(new NonUnityTestNode(HostPlatform));

            if (DoASharedPromotable)
            {
                var AgentSharingGroup = "Shared_EditorTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);

                var Options = Branch.BaseEngineProject.Options(HostPlatform);

                if (!Options.bIsPromotable || Options.bSeparateGamePromotion)
                {
                    throw new AutomationException("We assume that if we have shared promotable, the base engine is in it.");
                }


				if (HostPlatform == UnrealTargetPlatform.Win64) //temp hack till automated testing works on other platforms than Win64
                {

                    var EditorTests = Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].Rules.GUBP_GetEditorTests_EditorTypeOnly(HostPlatform);
                    var EditorTestNodes = new List<string>();
                    foreach (var Test in EditorTests)
                    {
                        if (!bNoAutomatedTesting)
                        {							
                            EditorTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, Branch.BaseEngineProject, Test.Key, Test.Value, AgentSharingGroup)));
                        
                            foreach (var NonCodeProject in Branch.NonCodeProjects)
                            {
                                if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName))
                                {
                                    continue;
                                }
                                if (HostPlatform == UnrealTargetPlatform.Mac) continue; //temp hack till mac automated testing works
                                EditorTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, NonCodeProject, Test.Key, Test.Value, AgentSharingGroup)));
                            }
                        }
                    }
                    if (EditorTestNodes.Count > 0)
                    {
                        AddNode(new GameAggregateNode(this, HostPlatform, Branch.BaseEngineProject, "AllEditorTests", EditorTestNodes, 0.0f));
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
                            if (!Platform.Platforms[HostPlatform].CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", Branch.BaseEngineProject.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
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
                                if (!GUBPNodes.ContainsKey(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat)))
                                {
									if(GamePlatformMonolithicsNode.HasPrecompiledTargets(Branch.BaseEngineProject, HostPlatform, Plat))
									{
	                                    AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, Branch.BaseEngineProject, Plat, InPrecompiled: true));
									}
                                    AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, Branch.BaseEngineProject, Plat));
                                }
								if (Plat == UnrealTargetPlatform.Win32 && Target.Rules.GUBP_BuildWindowsXPMonolithics() && Kind == TargetRules.TargetType.Game)
								{
									if (!GUBPNodes.ContainsKey(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat, true)))
									{
										AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, Branch.BaseEngineProject, Plat, true));
									}
	                            }
                            }
                        }
                    }
                }

                var CookedAgentSharingGroup = "Shared_CookedTests" + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform);

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
                            if (!Platform.Platforms[HostPlatform].CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", Branch.BaseEngineProject.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
                            {
                                continue;
                            }

                            if (ActivePlatforms.Contains(Plat))
                            {
                                string CookedPlatform = Platform.Platforms[Plat].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");								
                                if (!GUBPNodes.ContainsKey(CookNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, CookedPlatform)))
                                {
                                    GameCookNodes.Add(AddNode(new CookNode(this, HostPlatform, Branch.BaseEngineProject, Plat, CookedPlatform)));
                                }
                                if (!GUBPNodes.ContainsKey(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, Branch.BaseEngineProject, Plat)))
                                {
                                    AddNode(new GamePlatformCookedAndCompiledNode(this, HostPlatform, Branch.BaseEngineProject, Plat, false));
                                }
                                var GameTests = Target.Rules.GUBP_GetGameTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), Plat);
                                var RequiredPlatforms = new List<UnrealTargetPlatform> { Plat };
                                if (!bNoAutomatedTesting)
                                {
                                    var ThisMonoGameTestNodes = new List<string>();	
                                    
                                    foreach (var Test in GameTests)
                                    {
                                        var TestName = Test.Key + "_" + Plat.ToString();
                                        ThisMonoGameTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, Branch.BaseEngineProject, TestName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
                                    }
                                    if (ThisMonoGameTestNodes.Count > 0)
                                    {										
                                        GameTestNodes.Add(AddNode(new GameAggregateNode(this, HostPlatform, Branch.BaseEngineProject, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes, 0.0f)));
                                    }
                                }

                                foreach (var NonCodeProject in Branch.NonCodeProjects)
                                {
                                    if (!NonCodeProjectNames.ContainsKey(NonCodeProject.GameName) || !NonCodeProjectNames[NonCodeProject.GameName].Contains(Plat))
                                    {
                                        continue;
                                    }
                                    if (!GUBPNodes.ContainsKey(CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform)))
                                    {
                                        GameCookNodes.Add(AddNode(new CookNode(this, HostPlatform, NonCodeProject, Plat, CookedPlatform)));
                                    }
                                    if (!GUBPNodes.ContainsKey(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, NonCodeProject, Plat)))
                                    {
                                        AddNode(new GamePlatformCookedAndCompiledNode(this, HostPlatform, NonCodeProject, Plat, false));
                                        if (NonCodeFormalBuilds.ContainsKey(NonCodeProject.GameName))
                                        {
                                            var PlatList = NonCodeFormalBuilds[NonCodeProject.GameName];
                                            foreach (var PlatPair in PlatList)
                                            {
                                                if (PlatPair.TargetPlatform == Plat)
                                                {
													var NodeName = AddNode(new FormalBuildNode(this, NonCodeProject, HostPlatform, new List<UnrealTargetPlatform>() { Plat }, new List<UnrealTargetConfiguration>() {PlatPair.TargetConfig}));
													if(PlatPair.bBeforeTrigger)
													{
														RemovePseudodependencyFromNode(FormalBuildNode.StaticGetFullName(NonCodeProject, HostPlatform, new List<UnrealTargetPlatform>() { Plat }, new List<UnrealTargetConfiguration>() { PlatPair.TargetConfig }), WaitForFormalUserInput.StaticGetFullName());
													}
													// we don't want this delayed
													// this would normally wait for the testing phase, we just want to build it right away
													RemovePseudodependencyFromNode(
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
														GUBPNodes[CookNode.StaticGetFullName(HostPlatform, NonCodeProject, CookedPlatform)].AgentSharingGroup = BuildAgentSharingGroup;
														GUBPNodes[GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, NonCodeProject, Plat)].AgentSharingGroup = BuildAgentSharingGroup;
														GUBPNodes[NodeName].AgentSharingGroup = BuildAgentSharingGroup;
													}
													else
													{
														//GUBPNodes[NodeName].AgentSharingGroup = FormalAgentSharingGroup;
														if(Plat == UnrealTargetPlatform.XboxOne)
														{
															GUBPNodes[NodeName].AgentSharingGroup = "";
														}
													}
													if (PlatPair.bTest)
													{
														AddNode(new FormalBuildTestNode(this, NonCodeProject, HostPlatform, Plat, PlatPair.TargetConfig));
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
											ThisMonoGameTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, NonCodeProject, TestName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
										}
										if (ThisMonoGameTestNodes.Count > 0)
										{											
											GameTestNodes.Add(AddNode(new GameAggregateNode(this, HostPlatform, NonCodeProject, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes, 0.0f)));
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
                    AddNode(new GameAggregateNode(this, HostPlatform, Branch.BaseEngineProject, "AllCookedTests", GameTestNodes));
                }
            }
			if (!GUBPNodes.ContainsKey(SharedCookAggregateNode.StaticGetFullName()))
			{
				AddNode(new SharedCookAggregateNode(this, HostPlatforms, NonCodeProjectNames, NonCodeFormalBuilds));
			}
			
			if(HostPlatform == MakeFeaturePacksNode.GetDefaultBuildPlatform(this))
			{
				AddNode(new MakeFeaturePacksNode(HostPlatform, Branch.AllProjects.Where(x => MakeFeaturePacksNode.IsFeaturePack(x))));
			}

            foreach (var CodeProj in Branch.CodeProjects)
            {
                var Options = CodeProj.Options(HostPlatform);

                if (!Options.bIsPromotable && !Options.bTestWithShared && !Options.bIsNonCode)
                {
                    continue; // we skip things that aren't promotable and aren't tested - except noncode as code situations
                }
                var AgentShareName = CodeProj.GameName;
                if (!Options.bSeparateGamePromotion)
                {
                    AgentShareName = "Shared";
                }

				if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform) && !Options.bIsNonCode)
				{
					EditorGameNode Node = (EditorGameNode)TryFindNode(EditorGameNode.StaticGetFullName(HostPlatform, CodeProj));
					if(Node == null)
					{
						AddNode(new EditorGameNode(this, HostPlatform, CodeProj));
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
							EditorTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, CodeProj, Test.Key, Test.Value, AgentSharingGroup)));
							if (!Options.bTestWithShared || !HasNode(WaitForTestShared.StaticGetFullName()))
							{
								RemovePseudodependencyFromNode((UATTestNode.StaticGetFullName(HostPlatform, CodeProj, Test.Key)), WaitForTestShared.StaticGetFullName());
							}
						}
						if (EditorTestNodes.Count > 0)
						{
							AddNode(new GameAggregateNode(this, HostPlatform, CodeProj, "AllEditorTests", EditorTestNodes, 0.0f));
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
                            if (!Platform.Platforms[HostPlatform].CanHostPlatform(Plat))
                            {
                                throw new AutomationException("Project {0} asked for platform {1} with host {2}, but the host platform cannot build that platform.", CodeProj.GameName, Plat.ToString(), HostPlatform.ToString());
                            }
                            if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
                            {
                                continue;
                            }
							if(Plat == UnrealTargetPlatform.Win32 && Target.Rules.GUBP_BuildWindowsXPMonolithics())
							{
								if(!GUBPNodes.ContainsKey(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, CodeProj, Plat, true)))
								{
									AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, CodeProj, Plat, true));
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
                                if (!GUBPNodes.ContainsKey(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, CodeProj, Plat)))
                                {
									if(GamePlatformMonolithicsNode.HasPrecompiledTargets(CodeProj, HostPlatform, Plat))
									{
	                                    AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, CodeProj, Plat, InPrecompiled: true));
									}
                                    AddNode(new GamePlatformMonolithicsNode(this, HostPlatform, CodeProj, Plat));
                                }
								if (!AdditionalPlatforms.Contains(Plat))
								{
									string CookedPlatform = Platform.Platforms[Plat].GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client, "");
									if (Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform) != "")
									{
										CookedPlatform = Target.Rules.GUBP_AlternateCookPlatform(HostPlatform, CookedPlatform);
									}
									if (!GUBPNodes.ContainsKey(CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform)))
									{
										AddNode(new CookNode(this, HostPlatform, CodeProj, Plat, CookedPlatform));
									}
									if (!GUBPNodes.ContainsKey(GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, CodeProj, Plat)))
									{
										AddNode(new GamePlatformCookedAndCompiledNode(this, HostPlatform, CodeProj, Plat, true));
									}
									var FormalBuildConfigs = Target.Rules.GUBP_GetConfigsForFormalBuilds_MonolithicOnly(HostPlatform);

									foreach (var Config in FormalBuildConfigs)
									{
										string FormalNodeName = null;                                    
										if (Kind == TargetRules.TargetType.Client)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = AddNode(new FormalBuildNode(this, CodeProj, HostPlatform, InClientTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }, InClientNotGame: true));
											}
										}
										else if (Kind == TargetRules.TargetType.Server)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = AddNode(new FormalBuildNode(this, CodeProj, HostPlatform, InServerTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InServerConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }));
											}
										}
										else if (Kind == TargetRules.TargetType.Game)
										{
											if (Plat == Config.TargetPlatform)
											{
												FormalNodeName = AddNode(new FormalBuildNode(this, CodeProj, HostPlatform, InClientTargetPlatforms: new List<UnrealTargetPlatform>() { Config.TargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { Config.TargetConfig }));
											}
										}
										if (FormalNodeName != null)
										{
											if (Config.bBeforeTrigger)
											{
												RemovePseudodependencyFromNode(FormalBuildNode.StaticGetFullName(CodeProj, HostPlatform, new List<UnrealTargetPlatform>() { Config.TargetPlatform }, new List<UnrealTargetConfiguration>() { Config.TargetConfig }), WaitForFormalUserInput.StaticGetFullName());
											}
											// we don't want this delayed
											// this would normally wait for the testing phase, we just want to build it right away
											RemovePseudodependencyFromNode(
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
												GUBPNodes[CookNode.StaticGetFullName(HostPlatform, CodeProj, CookedPlatform)].AgentSharingGroup = BuildAgentSharingGroup;
												GUBPNodes[GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, CodeProj, Plat)].AgentSharingGroup = BuildAgentSharingGroup;
												GUBPNodes[FormalNodeName].AgentSharingGroup = BuildAgentSharingGroup;
											}
											else
											{
												//GUBPNodes[FormalNodeName].AgentSharingGroup = FormalAgentSharingGroup;
												if (Plat == UnrealTargetPlatform.XboxOne)
												{
													GUBPNodes[FormalNodeName].AgentSharingGroup = "";
												}
											}
											if (Config.bTest)
											{
												AddNode(new FormalBuildTestNode(this, CodeProj, HostPlatform, Plat, Config.TargetConfig));
											}																				
										}
									}
									if (!bNoAutomatedTesting)
									{
										if (HostPlatform == UnrealTargetPlatform.Mac || HostPlatform == UnrealTargetPlatform.Linux) continue; //temp hack till Linux and Mac automated testing works
										var GameTests = Target.Rules.GUBP_GetGameTests_MonolithicOnly(HostPlatform, GetAltHostPlatform(HostPlatform), Plat);
										var RequiredPlatforms = new List<UnrealTargetPlatform> { Plat };
										var ThisMonoGameTestNodes = new List<string>();

										foreach (var Test in GameTests)
										{
											var TestNodeName = Test.Key + "_" + Plat.ToString();
											ThisMonoGameTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, CodeProj, TestNodeName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
											if (!Options.bTestWithShared || !HasNode(WaitForTestShared.StaticGetFullName()))
											{
												RemovePseudodependencyFromNode((UATTestNode.StaticGetFullName(HostPlatform, CodeProj, TestNodeName)), WaitForTestShared.StaticGetFullName());
											}
										}
										if (ThisMonoGameTestNodes.Count > 0)
										{
											GameTestNodes.Add(AddNode(new GameAggregateNode(this, HostPlatform, CodeProj, "CookedTests_" + Plat.ToString() + "_" + Kind.ToString() + HostPlatformNode.StaticGetHostPlatformSuffix(HostPlatform), ThisMonoGameTestNodes, 0.0f)));
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
								GameTestNodes.Add(AddNode(new UATTestNode(this, HostPlatform, CodeProj, TestNodeName, Test.Value, CookedAgentSharingGroup, false, RequiredPlatforms)));
							}
						}
					}
					if (GameTestNodes.Count > 0)
					{
						AddNode(new GameAggregateNode(this, HostPlatform, CodeProj, "AllCookedTests", GameTestNodes));
					}
				}
			}
		}

        int NumSharedAllHosts = 0;
        foreach (var CodeProj in Branch.CodeProjects)
        {
            if (CodeProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                bool AnySeparate = false;
                var PromotedHosts = new List<UnrealTargetPlatform>();

                foreach (var HostPlatform in HostPlatforms)
                {
                    if (!BranchOptions.ExcludePlatformsForEditor.Contains(HostPlatform) && !BranchOptions.RemovePlatformFromPromotable.Contains(HostPlatform))
					{
						var Options = CodeProj.Options(HostPlatform);
						AnySeparate = AnySeparate || Options.bSeparateGamePromotion;
						if (Options.bIsPromotable)
						{
							if (!Options.bSeparateGamePromotion)
							{
								NumSharedAllHosts++;
							}
							PromotedHosts.Add(HostPlatform);
						}
					}
                }
                if (PromotedHosts.Count > 0)
                {
                    AddNode(new GameAggregatePromotableNode(this, PromotedHosts, CodeProj, true));
                    if (AnySeparate)
                    {
                        AddNode(new WaitForGamePromotionUserInput(this, CodeProj, false));
                        AddNode(new GameLabelPromotableNode(this, CodeProj, false));
                        AddNode(new WaitForGamePromotionUserInput(this, CodeProj, true));
                        AddNode(new GameLabelPromotableNode(this, CodeProj, true));
                    }
                }
            }
        }
        if (NumSharedAllHosts > 0)
        {
            AddNode(new GameAggregatePromotableNode(this, HostPlatforms, Branch.BaseEngineProject, false));

            AddNode(new SharedAggregatePromotableNode(this, HostPlatforms));
            AddNode(new WaitForSharedPromotionUserInput(this, false));
            AddNode(new SharedLabelPromotableNode(this, false));
            AddNode(new SharedLabelPromotableSuccessNode());
            AddNode(new WaitForTestShared(this));
            AddNode(new WaitForSharedPromotionUserInput(this, true));
            AddNode(new SharedLabelPromotableNode(this, true));
			AddNode(new WaitForFormalUserInput(this));
        }
        foreach (var HostPlatform in HostPlatforms)
        {
            AddCustomNodes(HostPlatform);
        }
        
        if (HasNode(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64)))
        {
			if (HasNode(GamePlatformMonolithicsNode.StaticGetFullName(UnrealTargetPlatform.Mac, Branch.BaseEngineProject, UnrealTargetPlatform.IOS)) && HasNode(ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64)))
			{
				//AddNode(new IOSOnPCTestNode(this)); - Disable IOSOnPCTest until a1011 crash is fixed
			}
			AddNode(new VSExpressTestNode(this));
			if (ActivePlatforms.Contains(UnrealTargetPlatform.Linux) && !BranchOptions.ExcludePlatformsForEditor.Contains(UnrealTargetPlatform.Linux))
			{
				AddNode(new RootEditorCrossCompileLinuxNode(UnrealTargetPlatform.Win64));
			}
            if (!bPreflightBuild)
            {
                AddNode(new CleanSharedTempStorageNode(this));
            }
        }
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
        foreach (var NodeToDo in GUBPNodes)
        {
            foreach (var Dep in GUBPNodes[NodeToDo.Key].FullNamesOfDependencies)
            {
                if (!GUBPNodes.ContainsKey(Dep))
                {
                    throw new AutomationException("Node {0} is not in the full graph. It is a dependency of {1}.", Dep, NodeToDo.Key);
                }
                if (Dep == NodeToDo.Key)
                {
                    throw new AutomationException("Node {0} has a self arc.", NodeToDo.Key);
                }
            }
            foreach (var Dep in GUBPNodes[NodeToDo.Key].FullNamesOfPseudosependencies)
            {
                if (!GUBPNodes.ContainsKey(Dep))
                {
                    throw new AutomationException("Node {0} is not in the full graph. It is a pseudodependency of {1}.", Dep, NodeToDo.Key);
                }
                if (Dep == NodeToDo.Key)
                {
                    throw new AutomationException("Node {0} has a self pseudoarc.", NodeToDo.Key);
                }
            }
        }

        if (bCleanLocalTempStorage)  // shared temp storage can never be wiped
        {
            DeleteLocalTempStorageManifests(CmdEnv);
        }

        GUBPNodesControllingTrigger = new Dictionary<string, string>();
        GUBPNodesControllingTriggerDotName = new Dictionary<string, string>();

        var FullNodeList = new Dictionary<string, string>();
        var FullNodeDirectDependencies = new Dictionary<string, string>();
		var FullNodeDependedOnBy = new Dictionary<string, string>();
		var FullNodeDependentPromotions = new Dictionary<string, string>();		
		var SeparatePromotables = new List<string>();
        {
            foreach (var NodeToDo in GUBPNodes)
            {
                if (GUBPNodes[NodeToDo.Key].IsSeparatePromotable())
                {
                    SeparatePromotables.Add(GUBPNodes[NodeToDo.Key].GetFullName());
                    List<string> Dependencies = new List<string>();
                    Dependencies = GetECDependencies(NodeToDo.Key);
                    foreach (var Dep in Dependencies)
                    {
                        if (!GUBPNodes.ContainsKey(Dep))
                        {
                            throw new AutomationException("Node {0} is not in the graph.  It is a dependency of {1}.", Dep, NodeToDo);
                        }
                        if (!GUBPNodes[Dep].IsPromotableAggregate())
                        {
                            if (!GUBPNodes[Dep].DependentPromotions.Contains(NodeToDo.Key))
                            {
                                GUBPNodes[Dep].DependentPromotions.Add(NodeToDo.Key);
                            }
                        }
                    }
                }
            }

			// Make sure that everything that's listed as a frequency barrier is completed with the given interval
			Dictionary<string, int> FrequencyOverrides = new Dictionary<string,int>();
			foreach (KeyValuePair<string, sbyte> Barrier in BranchOptions.FrequencyBarriers)
			{
				// All the nodes which are dependencies of the barrier node
				HashSet<string> IncludedNodes = new HashSet<string> { Barrier.Key };

				// Find all the nodes which are indirect dependencies of this node
				List<string> SearchNodes = new List<string> { Barrier.Key };
				for (int Idx = 0; Idx < SearchNodes.Count; Idx++)
				{
					GUBPNode Node = GUBPNodes[SearchNodes[Idx]];
					foreach (string DependencyName in Node.FullNamesOfDependencies.Union(Node.FullNamesOfPseudosependencies))
					{
						if (!IncludedNodes.Contains(DependencyName))
						{
							IncludedNodes.Add(DependencyName);
							SearchNodes.Add(DependencyName);
						}
					}
				}

				// Make sure that everything included in this list is before the cap, and everything not in the list is after it
				foreach (KeyValuePair<string, GUBPNode> NodePair in GUBPNodes)
				{
					if (IncludedNodes.Contains(NodePair.Key))
					{
						int Frequency;
						if(FrequencyOverrides.TryGetValue(NodePair.Key, out Frequency))
						{
							Frequency = Math.Min(Frequency, Barrier.Value);
						}
						else
						{
							Frequency = Barrier.Value;
						}
						FrequencyOverrides[NodePair.Key] = Frequency;
					}
				}
			}

			// Compute all the frequencies
            foreach (var NodeToDo in GUBPNodes)
            {
                ComputeDependentCISFrequencyQuantumShift(NodeToDo.Key, FrequencyOverrides);
            }

            foreach (var NodeToDo in GUBPNodes)
            {
                var Deps = GUBPNodes[NodeToDo.Key].DependentPromotions;
                string All = "";
                foreach (var Dep in Deps)
                {
                    if (All != "")
                    {
                        All += " ";
                    }
                    All += Dep;
                }
                FullNodeDependentPromotions.Add(NodeToDo.Key, All);
            }
        }	
        {
            Log("******* {0} GUBP Nodes", GUBPNodes.Count);
            var SortedNodes = TopologicalSort(new HashSet<string>(GUBPNodes.Keys), LocalOnly: true, DoNotConsiderCompletion: true);
            foreach (var Node in SortedNodes)
            {
                string Note = GetControllingTriggerDotName(Node);
                if (Note == "")
                {
                    Note = CISFrequencyQuantumShiftString(Node);
                }
                if (Note == "")
                {
                    Note = "always";
                }
                if (GUBPNodes[Node].RunInEC())
                {
                    var Deps = GetECDependencies(Node);
                    string All = "";
                    foreach (var Dep in Deps)
                    {
                        if (All != "")
                        {
                            All += " ";
                        }
                        All += Dep;
                    }
                    Log("  {0}: {1}      {2}", Node, Note, All);
                    FullNodeList.Add(Node, Note);
                    FullNodeDirectDependencies.Add(Node, All);
                }
                else
                {
                    Log("  {0}: {1} [Aggregate]", Node, Note);
                }
            }
        }
		Dictionary<string, int> FullNodeListSortKey = GetDisplayOrder(FullNodeList.Keys.ToList(), FullNodeDirectDependencies, GUBPNodes);

        bool bOnlyNode = false;
        bool bRelatedToNode = false;
		bool bGraphSubset = false;
        var NodesToDo = new HashSet<string>();

        {
            string NodeSpec = ParseParamValue("Node");
            if (String.IsNullOrEmpty(NodeSpec))
            {
                NodeSpec = ParseParamValue("RelatedToNode");
                if (!String.IsNullOrEmpty(NodeSpec))
                {
                    bRelatedToNode = true;
                }
            }
            if (String.IsNullOrEmpty(NodeSpec) && CommanderSetup)
            {
                NodeSpec = ParseParamValue("SetupNode");
                if (String.IsNullOrEmpty(NodeSpec))
                {
                    NodeSpec = ParseParamValue("SetupRelatedToNode");
                    if (!String.IsNullOrEmpty(NodeSpec))
                    {
                        bRelatedToNode = true;
                    }
                }
            }
            if (String.IsNullOrEmpty(NodeSpec))
            {
                NodeSpec = ParseParamValue("OnlyNode");
                if (!String.IsNullOrEmpty(NodeSpec))
                {
                    bOnlyNode = true;
                }
            }
            if (!String.IsNullOrEmpty(NodeSpec))
            {
				bGraphSubset = true;
                if (NodeSpec.Equals("Noop", StringComparison.InvariantCultureIgnoreCase))
                {
                    Log("Request for Noop node, done.");
                    PrintRunTime();
                    return;
                }
                List<string> Nodes = new List<string>(NodeSpec.Split('+'));
                foreach (var NodeArg in Nodes)
                {
                    var NodeName = NodeArg.Trim();
                    bool bFoundAnything = false;
                    if (!String.IsNullOrEmpty(NodeName))
                    {
                        foreach (var Node in GUBPNodes)
                        {
                            if (Node.Value.GetFullName().Equals(NodeArg, StringComparison.InvariantCultureIgnoreCase) ||
                                Node.Value.AgentSharingGroup.Equals(NodeArg, StringComparison.InvariantCultureIgnoreCase)
                                )
                            {
                                if (!NodesToDo.Contains(Node.Key))
                                {
                                    NodesToDo.Add(Node.Key);
                                }
                                bFoundAnything = true;
                            }
                        }
                        if (!bFoundAnything)
                        {
                            throw new AutomationException("Could not find node named {0}", NodeName);
                        }
                    }
                }
            }
        }
        string GameSpec = ParseParamValue("Game");
        if (!String.IsNullOrEmpty(GameSpec))
        {
			bGraphSubset = true;
            List<string> Games = new List<string>(GameSpec.Split('+'));
            foreach (var GameArg in Games)
            {
                var GameName = GameArg.Trim();
                if (!String.IsNullOrEmpty(GameName))
                {
                    foreach (var GameProj in Branch.CodeProjects)
                    {
                        if (GameProj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                        {

                            NodesToDo.Add(GameAggregatePromotableNode.StaticGetFullName(GameProj));
                            foreach (var Node in GUBPNodes)
                            {
                                if (Node.Value.GameNameIfAnyForTempStorage() == GameProj.GameName)
                                {
                                    NodesToDo.Add(Node.Key);
                                }
                            }
 
                            GameName = null;
                        }
                    }
                    if (GameName != null)
                    {
                        foreach (var GameProj in Branch.NonCodeProjects)
                        {
                            if (GameProj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                            {
                                foreach (var Node in GUBPNodes)
                                {
                                    if (Node.Value.GameNameIfAnyForTempStorage() == GameProj.GameName)
                                    {
                                        NodesToDo.Add(Node.Key);
                                    }
                                }
                                GameName = null;
                            }
                        }
                    }
                    if (GameName != null)
                    {
                        throw new AutomationException("Could not find game named {0}", GameName);
                    }
                }
            }
        }
        if (NodesToDo.Count == 0)
        {
            Log("No nodes specified, adding all nodes");
            foreach (var Node in GUBPNodes)
            {
                NodesToDo.Add(Node.Key);
            }
        }
        else if (TimeIndex != 0)
        {
            Log("Check to make sure we didn't ask for nodes that will be culled by time index");
            foreach (var NodeToDo in NodesToDo)
            {
                if (TimeIndex % (1 << GUBPNodes[NodeToDo].DependentCISFrequencyQuantumShift()) != 0)
                {
                    throw new AutomationException("You asked specifically for node {0}, but it is culled by the time quantum: TimeIndex = {1}, DependentCISFrequencyQuantumShift = {2}.", NodeToDo, TimeIndex, GUBPNodes[NodeToDo].DependentCISFrequencyQuantumShift());
                }
            }
        }

        Log("Desired Nodes");
        foreach (var NodeToDo in NodesToDo)
        {
            Log("  {0}", NodeToDo);			
        }
        // if we are doing related to, then find things that depend on the selected nodes
        if (bRelatedToNode)
        {
            bool bDoneWithDependencies = false;

            while (!bDoneWithDependencies)
            {
                bDoneWithDependencies = true;
                var Fringe = new HashSet<string>();
                foreach (var NodeToDo in GUBPNodes)
                {
                    if (!NodesToDo.Contains(NodeToDo.Key))
                    {
                        foreach (var Dep in GUBPNodes[NodeToDo.Key].FullNamesOfDependencies)
                        {
                            if (!GUBPNodes.ContainsKey(Dep))
                            {
                                throw new AutomationException("Node {0} is not in the graph. It is a dependency of {1}.", Dep, NodeToDo.Key);
                            }
                            if (NodesToDo.Contains(Dep))
                            {
                                Fringe.Add(NodeToDo.Key);
                                bDoneWithDependencies = false;
                            }
                        }
                        foreach (var Dep in GUBPNodes[NodeToDo.Key].FullNamesOfPseudosependencies)
                        {
                            if (!GUBPNodes.ContainsKey(Dep))
                            {
                                throw new AutomationException("Node {0} is not in the graph. It is a pseudodependency of {1}.", Dep, NodeToDo.Key);
                            }
                        }
                    }
                }
                NodesToDo.UnionWith(Fringe);
            }
        }

        // find things that our nodes depend on
        if (!bOnlyNode)
        {
            bool bDoneWithDependencies = false;

            while (!bDoneWithDependencies)
            {
                bDoneWithDependencies = true;
                var Fringe = new HashSet<string>(); 
                foreach (var NodeToDo in NodesToDo)
                {
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
                    {
                        if (!GUBPNodes.ContainsKey(Dep))
                        {
                            throw new AutomationException("Node {0} is not in the graph. It is a dependency of {1}.", Dep, NodeToDo);
                        }
                        if (!NodesToDo.Contains(Dep))
                        {
                            Fringe.Add(Dep);
                            bDoneWithDependencies = false;
                        }
                    }
                    foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
                    {
                        if (!GUBPNodes.ContainsKey(Dep))
                        {
                            throw new AutomationException("Node {0} is not in the graph. It is a pseudodependency of {1}.", Dep, NodeToDo);
                        }
                    }
                }
                NodesToDo.UnionWith(Fringe);
            }
        }		
		if (TimeIndex != 0)
		{
			Log("Culling based on time index");
			var NewNodesToDo = new HashSet<string>();
			foreach (var NodeToDo in NodesToDo)
			{
				if (TimeIndex % (1 << GUBPNodes[NodeToDo].DependentCISFrequencyQuantumShift()) == 0)
				{
					Log("  Keeping {0}", NodeToDo);
					NewNodesToDo.Add(NodeToDo);
				}
				else
				{
					Log("  Rejecting {0}", NodeToDo);
				}
			}
			NodesToDo = NewNodesToDo;
		}
		//Remove Plat if specified
		if(WithoutLinux)
		{
			var NewNodesToDo = new HashSet<string>();
			foreach(var NodeToDo in NodesToDo)
			{
				if(!GUBPNodes[NodeToDo].GetFullName().Contains("Linux"))
				{					
					NewNodesToDo.Add(NodeToDo);
				}
				else
				{
					Log(" Rejecting {0} because -NoLinux was requested", NodeToDo);
				}
			}
			NodesToDo = NewNodesToDo;
		}
		//find things that depend on our nodes and setup commander dictionary
		if (!bOnlyNode)
		{			
			foreach(var NodeToDo in NodesToDo)
			{
				if (!GUBPNodes[NodeToDo].IsAggregate() && !GUBPNodes[NodeToDo].IsTest())
				{
					List<string> ECDependencies = new List<string>();
					ECDependencies = GetECDependencies(NodeToDo);
					foreach (var Dep in ECDependencies)
					{
						if (!GUBPNodes.ContainsKey(Dep))
						{
							throw new AutomationException("Node {0} is not in the graph. It is a dependency of {1}.", Dep, NodeToDo);
						}
						if (!GUBPNodes[Dep].FullNamesOfDependedOn.Contains(NodeToDo))
						{
							GUBPNodes[Dep].FullNamesOfDependedOn.Add(NodeToDo);
						}						
					}
				}				
			}	
			foreach(var NodeToDo in NodesToDo)
			{
				var Deps = GUBPNodes[NodeToDo].FullNamesOfDependedOn;
				string All = "";
				foreach (var Dep in Deps)
				{
					if (All != "")
					{
						All += " ";
					}
					All += Dep;
				}
				FullNodeDependedOnBy.Add(NodeToDo, All);
			}
		}

        if (CommanderSetup)
        {
            if (!String.IsNullOrEmpty(ExplicitTrigger))
            {
                bool bFoundIt = false;
                foreach (var Node in GUBPNodes)
                {
                    if (Node.Value.GetFullName().Equals(ExplicitTrigger, StringComparison.InvariantCultureIgnoreCase))
                    {
                        if (Node.Value.TriggerNode() && Node.Value.RunInEC())
                        {
                            Node.Value.SetAsExplicitTrigger();
                            bFoundIt = true;
                            break;
                        }
                    }
                }
                if (!bFoundIt)
                {
                    throw new AutomationException("Could not find trigger node named {0}", ExplicitTrigger);
                }
            }
            else
            {
                if (bSkipTriggers)
                {
                    foreach (var Node in GUBPNodes)
                    {
                        if (Node.Value.TriggerNode() && Node.Value.RunInEC())
                        {
                            Node.Value.SetAsExplicitTrigger();
                        }
                    }
                }
            }
        }
		if (bPreflightBuild)
		{
			Log("Culling triggers and downstream for preflight builds ");
			var NewNodesToDo = new HashSet<string>();
			foreach (var NodeToDo in NodesToDo)
			{
				var TriggerDot = GetControllingTriggerDotName(NodeToDo);
				if (TriggerDot == "" && !GUBPNodes[NodeToDo].TriggerNode())
				{
					Log("  Keeping {0}", NodeToDo);
					NewNodesToDo.Add(NodeToDo);
				}
				else
				{
					Log("  Rejecting {0}", NodeToDo);
				}
			}
			NodesToDo = NewNodesToDo;
		}

        GUBPNodesCompleted = new Dictionary<string, bool>();
        GUBPNodesHistory = new Dictionary<string, NodeHistory>();

        Log("******* Caching completion");
        {
            var StartTime = DateTime.UtcNow;
            foreach (var Node in NodesToDo)
            {
                Log("** {0}", Node);
                NodeIsAlreadyComplete(Node, LocalOnly); // cache these now to avoid spam later
                GetControllingTriggerDotName(Node);
            }
            var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
            Log("Took {0}s to cache completion for {1} nodes", BuildDuration / 1000, NodesToDo.Count);
        }
        /*if (CLString != "" && StoreName.Contains(CLString) && !ParseParam("NoHistory"))
        {
            Log("******* Updating history");
            var StartTime = DateTime.UtcNow;
            foreach (var Node in NodesToDo)
            {
                if (!NodeIsAlreadyComplete(Node, LocalOnly))
                {
                    UpdateNodeHistory(Node, CLString);
                }				
            }
            var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
            Log("Took {0}s to get history for {1} nodes", BuildDuration / 1000, NodesToDo.Count);
        }*/

        var OrdereredToDo = TopologicalSort(NodesToDo, ExplicitTrigger, LocalOnly);

        // find all unfinished triggers, excepting the one we are triggering right now
        var UnfinishedTriggers = new List<string>();
        if (!bSkipTriggers)
        {
            foreach (var NodeToDo in OrdereredToDo)
            {
                if (GUBPNodes[NodeToDo].TriggerNode() && !NodeIsAlreadyComplete(NodeToDo, LocalOnly))
                {
                    if (String.IsNullOrEmpty(ExplicitTrigger) || ExplicitTrigger != NodeToDo)
                    {
                        UnfinishedTriggers.Add(NodeToDo);
                    }
                }
            }
        }

        Log("*********** Desired And Dependent Nodes, in order.");
        PrintNodes(this, OrdereredToDo, LocalOnly, UnfinishedTriggers);		
        //check sorting
        {
            foreach (var NodeToDo in OrdereredToDo)
            {
                if (GUBPNodes[NodeToDo].TriggerNode() && (GUBPNodes[NodeToDo].IsSticky() || NodeIsAlreadyComplete(NodeToDo, LocalOnly))) // these sticky triggers are ok, everything is already completed anyway
                {
                    continue;
                }
				if(GUBPNodes[NodeToDo].IsTest())
				{
					bHasTests = true;
				}
                int MyIndex = OrdereredToDo.IndexOf(NodeToDo);
                foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
                {
                    int DepIndex = OrdereredToDo.IndexOf(Dep);
                    if (DepIndex >= MyIndex)
                    {
                        throw new AutomationException("Topological sort error, node {0} has a dependency of {1} which sorted after it.", NodeToDo, Dep);
                    }
                }
                foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfPseudosependencies)
                {
                    int DepIndex = OrdereredToDo.IndexOf(Dep);
                    if (DepIndex >= MyIndex)
                    {
                        throw new AutomationException("Topological sort error, node {0} has a pseduodependency of {1} which sorted after it.", NodeToDo, Dep);
                    }
                }
            }
        }

        string FakeFail = ParseParamValue("FakeFail");
        if (CommanderSetup)
        {

            if (OrdereredToDo.Count == 0)
            {
                throw new AutomationException("No nodes to do!");
            }
            var ECProps = new List<string>();
            ECProps.Add(String.Format("TimeIndex={0}", TimeIndex));
            foreach (var NodePair in FullNodeList)
            {
                ECProps.Add(string.Format("AllNodes/{0}={1}", NodePair.Key, NodePair.Value));
            }
            foreach (var NodePair in FullNodeDirectDependencies)
            {
                ECProps.Add(string.Format("DirectDependencies/{0}={1}", NodePair.Key, NodePair.Value));
            }
            foreach (var NodePair in FullNodeListSortKey)
            {
                ECProps.Add(string.Format("SortKey/{0}={1}", NodePair.Key, NodePair.Value));
            }
			foreach (var NodePair in FullNodeDependedOnBy)
            {
				ECProps.Add(string.Format("DependedOnBy/{0}={1}", NodePair.Key, NodePair.Value));
            }
			foreach (var NodePair in FullNodeDependentPromotions)
			{
				ECProps.Add(string.Format("DependentPromotions/{0}={1}", NodePair.Key, NodePair.Value));
			}
			foreach (var Node in SeparatePromotables)
			{
				ECProps.Add(string.Format("PossiblePromotables/{0}={1}", Node, ""));
			}			
            var ECJobProps = new List<string>();
            if (ExplicitTrigger != "")
            {
                ECJobProps.Add("IsRoot=0");
            }
            else
            {
                ECJobProps.Add("IsRoot=1");
            }

            var FilteredOrdereredToDo = new List<string>();
            // remove nodes that have unfinished triggers
            foreach (var NodeToDo in OrdereredToDo)
            {
                string ControllingTrigger = GetControllingTrigger(NodeToDo);
                bool bNoUnfinishedTriggers = !UnfinishedTriggers.Contains(ControllingTrigger);

                if (bNoUnfinishedTriggers)
                {
                    // if we are triggering, then remove nodes that are not controlled by the trigger or are dependencies of this trigger
                    if (!String.IsNullOrEmpty(ExplicitTrigger))
                    {
                        if (ExplicitTrigger != NodeToDo && !NodeDependsOn(NodeToDo, ExplicitTrigger) && !NodeDependsOn(ExplicitTrigger, NodeToDo))
                        {
                            continue; // this wasn't on the chain related to the trigger we are triggering, so it is not relevant
                        }
                    }
					if (bPreflightBuild && !bSkipTriggers && GUBPNodes[NodeToDo].TriggerNode())
					{
						// in preflight builds, we are either skipping triggers (and running things downstream) or we just stop at triggers and don't make them available for triggering.
						continue;
					}
                    FilteredOrdereredToDo.Add(NodeToDo);
                }
            }
            OrdereredToDo = FilteredOrdereredToDo;
            Log("*********** EC Nodes, in order.");
            PrintNodes(this, OrdereredToDo, LocalOnly, UnfinishedTriggers);

            // here we are just making sure everything before the explicit trigger is completed.
            if (!String.IsNullOrEmpty(ExplicitTrigger))
            {
                foreach (var NodeToDo in FilteredOrdereredToDo)
                {
                    if (GUBPNodes[NodeToDo].RunInEC() && !NodeIsAlreadyComplete(NodeToDo, LocalOnly) && NodeToDo != ExplicitTrigger && !NodeDependsOn(ExplicitTrigger, NodeToDo)) // if something is already finished, we don't put it into EC
                    {
                        throw new AutomationException("We are being asked to process node {0}, however, this is an explicit trigger {1}, so everything before it should already be handled. It seems likely that you waited too long to run the trigger. You will have to do a new build from scratch.", NodeToDo, ExplicitTrigger);
                    }
                }
            }

            string LastSticky = "";
            bool HitNonSticky = false;
            bool bHaveECNodes = false;
            List<string> StepList = new List<string>();
            StepList.Add("use strict;");
            StepList.Add("use diagnostics;");
            StepList.Add("use ElectricCommander();");
            StepList.Add("my $ec = new ElectricCommander;");
            StepList.Add("$ec->setTimeout(600);");
            StepList.Add("my $batch = $ec->newBatch(\"serial\");");
            // sticky nodes are ones that we run on the main agent. We run then first and they must not be intermixed with parallel jobs
            foreach (var NodeToDo in OrdereredToDo)
            {
                if (GUBPNodes[NodeToDo].RunInEC() && !NodeIsAlreadyComplete(NodeToDo, LocalOnly)) // if something is already finished, we don't put it into EC
                {
                    bHaveECNodes = true;
                    if (GUBPNodes[NodeToDo].IsSticky())
                    {
                        LastSticky = NodeToDo;
                        if (HitNonSticky && !bSkipTriggers)
                        {
                            throw new AutomationException("Sticky and non-sticky jobs did not sort right.");
                        }
                    }
                    else
                    {
                        HitNonSticky = true;
                    }
                }
            }
			
            string ParentPath = ParseParamValue("ParentPath");
            string BaseArgs = String.Format("$batch->createJobStep({{parentPath => '{0}'", ParentPath);

            bool bHasNoop = false;
            if (LastSticky == "" && bHaveECNodes)
            {
                // if we don't have any sticky nodes and we have other nodes, we run a fake noop just to release the resource 
                string Args = String.Format("{0}, subprocedure => 'GUBP_UAT_Node', parallel => '0', jobStepName => 'Noop', actualParameter => [{{actualParameterName => 'NodeName', value => 'Noop'}}, {{actualParameterName => 'Sticky', value =>'1' }}], releaseMode => 'release'}});", BaseArgs);
                StepList.Add(Args);
                bHasNoop = true;
            }

            var FakeECArgs = new List<string>();
            var AgentGroupChains = new Dictionary<string, List<string>>();
            foreach (var NodeToDo in OrdereredToDo)
            {
                if (GUBPNodes[NodeToDo].RunInEC() && !NodeIsAlreadyComplete(NodeToDo, LocalOnly)) // if something is already finished, we don't put it into EC  
                {
                    string MyAgentGroup = GUBPNodes[NodeToDo].AgentSharingGroup;
                    if (MyAgentGroup != "")
                    {
                        if (!AgentGroupChains.ContainsKey(MyAgentGroup))
                        {
                            AgentGroupChains.Add(MyAgentGroup, new List<string>{NodeToDo});
                        }
                        else
                        {
                            AgentGroupChains[MyAgentGroup].Add(NodeToDo);
                        }
                    }
                }
            }

            foreach (var NodeToDo in OrdereredToDo)
            {
                if (GUBPNodes[NodeToDo].RunInEC() && !NodeIsAlreadyComplete(NodeToDo, LocalOnly)) // if something is already finished, we don't put it into EC  
                {                                      
                    string EMails;
                    var NodeProps = GetECPropsForNode(NodeToDo, CLString, out EMails);
                    ECProps.AddRange(NodeProps);

                    bool Sticky = GUBPNodes[NodeToDo].IsSticky();
                    bool DoParallel = !Sticky;
                    if (Sticky && GUBPNodes[NodeToDo].ECAgentString() != "")
                    {
                        throw new AutomationException("Node {1} is sticky but has agent requirements.", NodeToDo);
                    }
                    string Procedure = GUBPNodes[NodeToDo].ECProcedure();
                    if(GUBPNodes[NodeToDo].IsSticky() && NodeToDo == LastSticky)
                    {
                        Procedure = Procedure + "_Release";
                    }
                    string Args = String.Format("{0}, subprocedure => '{1}', parallel => '{2}', jobStepName => '{3}', actualParameter => [{{actualParameterName => 'NodeName', value =>'{4}'}}",
                        BaseArgs, Procedure, DoParallel ? 1 : 0, NodeToDo, NodeToDo);
                    string ProcedureParams = GUBPNodes[NodeToDo].ECProcedureParams();
                    if (!String.IsNullOrEmpty(ProcedureParams))
                    {
                        Args = Args + ProcedureParams;
                    }

                    if ((Procedure == "GUBP_UAT_Trigger" || Procedure == "GUBP_Hardcoded_Trigger") && !String.IsNullOrEmpty(EMails))
                    {
                        Args = Args + ", {actualParameterName => 'EmailsForTrigger', value => \'" + EMails + "\'}";
                    }
                    Args = Args + "]";
                    string PreCondition = "";
                    string RunCondition = "";
                    var UncompletedEcDeps = new List<string>();
					var UncompletedCompletedDeps = new List<string>();

                    string MyAgentGroup = GUBPNodes[NodeToDo].AgentSharingGroup;
                    bool bDoNestedJobstep = false;
                    bool bDoFirstNestedJobstep = false;

                    
                    string NodeParentPath = ParentPath;
					string PreconditionParentPath;
					if (GUBPNodes[NodeToDo].GetFullName().Contains("MakeBuild") && GUBPNodes[NodeToDo].FullNamesOfPseudosependencies.Contains(WaitForFormalUserInput.StaticGetFullName()) && !bGraphSubset)
                    {
						RemovePseudodependencyFromNode(NodeToDo, WaitForFormalUserInput.StaticGetFullName());
						PreconditionParentPath = GetPropertyFromStep("/myWorkflow/ParentJob");
						UncompletedEcDeps = GetECDependencies(NodeToDo);
					}
					else
					{
						PreconditionParentPath = ParentPath;
                        var EcDeps = GetECDependencies(NodeToDo);
                        foreach (var Dep in EcDeps)
                        {
                            if (GUBPNodes[Dep].RunInEC() && !NodeIsAlreadyComplete(Dep, LocalOnly) && OrdereredToDo.Contains(Dep)) // if something is already finished, we don't put it into EC
                            {
                                if (OrdereredToDo.IndexOf(Dep) > OrdereredToDo.IndexOf(NodeToDo))
                                {
                                    throw new AutomationException("Topological sort error, node {0} has a dependency of {1} which sorted after it.", NodeToDo, Dep);
                                }
                                UncompletedEcDeps.Add(Dep);
                            }
                        }
                    }
                    var PreConditionUncompletedEcDeps = UncompletedEcDeps;
					var CompletedDeps = GetCompletedOnlyDependencies(NodeToDo);					
					foreach (var Dep in CompletedDeps)
					{
						if (GUBPNodes[Dep].RunInEC() && !NodeIsAlreadyComplete(Dep, LocalOnly) && OrdereredToDo.Contains(Dep)) // if something is already finished, we don't put it into EC
						{
							if (OrdereredToDo.IndexOf(Dep) > OrdereredToDo.IndexOf(NodeToDo))
							{
								throw new AutomationException("Topological sort error, node {0} has a dependency of {1} which sorted after it.", NodeToDo, Dep);
							}
							UncompletedCompletedDeps.Add(Dep);
						}
					}
                    if (MyAgentGroup != "")
                    {
                        bDoNestedJobstep = true;
                        NodeParentPath = ParentPath + "/jobSteps[" + MyAgentGroup + "]";

                        PreConditionUncompletedEcDeps = new List<string>();

                        var MyChain = AgentGroupChains[MyAgentGroup];
                        int MyIndex = MyChain.IndexOf(NodeToDo);
                        if (MyIndex  > 0)
                        {
                            PreConditionUncompletedEcDeps.Add(MyChain[MyIndex - 1]);
                        }
                        else
                        {
                            bDoFirstNestedJobstep = bDoNestedJobstep;
                            // to avoid idle agents (and also EC doesn't actually reserve our agent!), we promote all dependencies to the first one
                            foreach (var Chain in MyChain)
                            {
                                var EcDeps = GetECDependencies(Chain);
                                foreach (var Dep in EcDeps)
                                {
                                    if (GUBPNodes[Dep].RunInEC() && !NodeIsAlreadyComplete(Dep, LocalOnly) && OrdereredToDo.Contains(Dep)) // if something is already finished, we don't put it into EC
                                    {
                                        if (OrdereredToDo.IndexOf(Dep) > OrdereredToDo.IndexOf(Chain))
                                        {
                                            throw new AutomationException("Topological sort error, node {0} has a dependency of {1} which sorted after it.", Chain, Dep);
                                        }
                                        if (!MyChain.Contains(Dep) && !PreConditionUncompletedEcDeps.Contains(Dep))
                                        {
                                            PreConditionUncompletedEcDeps.Add(Dep);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (bHasNoop && PreConditionUncompletedEcDeps.Count == 0)
                    {
                        PreConditionUncompletedEcDeps.Add("Noop");
                    }
                    if (PreConditionUncompletedEcDeps.Count > 0)
                    {
                        PreCondition = "\"\\$\" . \"[/javascript if(";
                        // these run "parallel", but we add preconditions to serialize them
                        int Index = 0;
                        foreach (var Dep in PreConditionUncompletedEcDeps)
                        {                            
                            PreCondition = PreCondition + "getProperty('" + GetJobStep(PreconditionParentPath, Dep) + "/status\') == \'completed\'";
                            Index++;
                            if (Index != PreConditionUncompletedEcDeps.Count)
                            {
                                PreCondition = PreCondition + " && ";
                            }
                        }
                        PreCondition = PreCondition + ") true;]\"";
                    }
					if(UncompletedCompletedDeps.Count > 0)
					{
						PreCondition = "\"\\$\" . \"[/javascript if(";
						// these run "parallel", but we add preconditions to serialize them
						int Index = 0;
						foreach (var Dep in CompletedDeps)
						{
							if (GUBPNodes[Dep].RunInEC())
							{
								PreCondition = PreCondition + "getProperty('" + GetJobStep(PreconditionParentPath, Dep) + "/status\') == \'completed\'";
								Index++;
								if (Index != CompletedDeps.Count)
								{
									PreCondition = PreCondition + " && ";
								}
							}
						}
						PreCondition = PreCondition + ") true;]\"";
					}

                    if (UncompletedEcDeps.Count > 0)
                    {
                        RunCondition = "\"\\$\" . \"[/javascript if(";
                        int Index = 0;
                        foreach (var Dep in UncompletedEcDeps)
                        {
							RunCondition = RunCondition + "((\'\\$\" . \"[" + GetJobStep(PreconditionParentPath, Dep) + "/outcome]\' == \'success\') || ";
							RunCondition = RunCondition + "(\'\\$\" . \"[" + GetJobStep(PreconditionParentPath, Dep) + "/outcome]\' == \'warning\'))";

                            Index++;
                            if (Index != UncompletedEcDeps.Count)
                            {
                                RunCondition = RunCondition + " && ";
                            }
                        }
                        RunCondition = RunCondition + ")true; else false;]\"";
                    }

                    if (bDoNestedJobstep)
                    {
                        if (bDoFirstNestedJobstep)
                        {
                            {
                                string NestArgs = String.Format("$batch->createJobStep({{parentPath => '{0}', jobStepName => '{1}', parallel => '1'",
                                    ParentPath, MyAgentGroup);
                                if (!String.IsNullOrEmpty(PreCondition))
                                {
                                    NestArgs = NestArgs + ", precondition => " + PreCondition;
                                }
                                NestArgs = NestArgs + "});";
                                StepList.Add(NestArgs);
                            }
                            {
                                string NestArgs = String.Format("$batch->createJobStep({{parentPath => '{0}/jobSteps[{1}]', jobStepName => '{2}_GetPool', subprocedure => 'GUBP{3}_AgentShare_GetPool', parallel => '1', actualParameter => [{{actualParameterName => 'AgentSharingGroup', value => '{4}'}}, {{actualParameterName => 'NodeName', value => '{5}'}}]",
                                    ParentPath, MyAgentGroup, MyAgentGroup, GUBPNodes[NodeToDo].ECProcedureInfix(), MyAgentGroup, NodeToDo);
                                if (!String.IsNullOrEmpty(PreCondition))
                                {
                                    NestArgs = NestArgs + ", precondition => " + PreCondition;
                                }
                                NestArgs = NestArgs + "});";
                                StepList.Add(NestArgs);
                            }
                            {
                                string NestArgs = String.Format("$batch->createJobStep({{parentPath => '{0}/jobSteps[{1}]', jobStepName => '{2}_GetAgent', subprocedure => 'GUBP{3}_AgentShare_GetAgent', parallel => '1', exclusiveMode => 'call', resourceName => '{4}', actualParameter => [{{actualParameterName => 'AgentSharingGroup', value => '{5}'}}, {{actualParameterName => 'NodeName', value=> '{6}'}}]",
                                    ParentPath, MyAgentGroup, MyAgentGroup, GUBPNodes[NodeToDo].ECProcedureInfix(),
                                    String.Format("$[/myJob/jobSteps[{0}]/ResourcePool]", MyAgentGroup),
                                    MyAgentGroup, NodeToDo);
                                {
                                    NestArgs = NestArgs + ", precondition  => ";
                                    NestArgs = NestArgs + "\"\\$\" . \"[/javascript if(";
                                    NestArgs = NestArgs + "getProperty('" + PreconditionParentPath + "/jobSteps[" + MyAgentGroup + "]/jobSteps[" + MyAgentGroup + "_GetPool]/status') == 'completed'";
                                    NestArgs = NestArgs + ") true;]\"";
                                }
                                NestArgs = NestArgs + "});";
                                StepList.Add(NestArgs);
                            }
                            {
                                PreCondition = "\"\\$\" . \"[/javascript if(";
                                PreCondition = PreCondition + "getProperty('" + PreconditionParentPath + "/jobSteps[" + MyAgentGroup + "]/jobSteps[" + MyAgentGroup + "_GetAgent]/status') == 'completed'";
                                PreCondition = PreCondition + ") true;]\"";
                            }
                        }
                        Args = Args.Replace(String.Format("parentPath => '{0}'", ParentPath), String.Format("parentPath => '{0}'", NodeParentPath));
                        Args = Args.Replace("UAT_Node_Parallel_AgentShare", "UAT_Node_Parallel_AgentShare3");
                    }

                    if (!String.IsNullOrEmpty(PreCondition))
                    {
                        Args = Args + ", precondition => " + PreCondition;
                    }
                    if (!String.IsNullOrEmpty(RunCondition))
                    {
                        Args = Args + ", condition => " + RunCondition;
                    }
#if false
                    // this doesn't work because it includes precondition time
                    if (GUBPNodes[NodeToDo].TimeoutInMinutes() > 0)
                    {
                        Args = Args + String.Format(" --timeLimitUnits minutes --timeLimit {0}", GUBPNodes[NodeToDo].TimeoutInMinutes());
                    }
#endif
                    if (Sticky && NodeToDo == LastSticky)
                    {
                        Args = Args + ", releaseMode => 'release'";
                    }
                    Args = Args + "});";
                    StepList.Add(Args);
                    if (bFakeEC && 
                        !UnfinishedTriggers.Contains(NodeToDo) &&
                        (GUBPNodes[NodeToDo].ECProcedure().StartsWith("GUBP_UAT_Node") || GUBPNodes[NodeToDo].ECProcedure().StartsWith("GUBP_Mac_UAT_Node")) // other things we really can't test
                        ) // unfinished triggers are never run directly by EC, rather it does another job setup
                    {
                        string Arg = String.Format("gubp -Node={0} -FakeEC {1} {2} {3} {4} {5}", 
                            NodeToDo,
                            bFake ? "-Fake" : "" , 
                            ParseParam("AllPlatforms") ? "-AllPlatforms" : "",
                            ParseParam("UnfinishedTriggersFirst") ? "-UnfinishedTriggersFirst" : "",
                            ParseParam("UnfinishedTriggersParallel") ? "-UnfinishedTriggersParallel" : "",
                            ParseParam("WithMac") ? "-WithMac" : ""
                            );

                        string Node = ParseParamValue("-Node");
                        if (!String.IsNullOrEmpty(Node))
                        {
                            Arg = Arg + " -Node=" + Node;
                        }
                        if (!String.IsNullOrEmpty(FakeFail))
                        {
                            Arg = Arg + " -FakeFail=" + FakeFail;
                        } 
                        FakeECArgs.Add(Arg);
                    }

                    if (MyAgentGroup != "" && !bDoNestedJobstep)
                    {
                        var MyChain = AgentGroupChains[MyAgentGroup];
                        int MyIndex = MyChain.IndexOf(NodeToDo);
                        if (MyIndex == MyChain.Count - 1)
                        {
                            var RelPreCondition = "\"\\$\" . \"[/javascript if(";
                            // this runs "parallel", but we a precondition to serialize it
                            RelPreCondition = RelPreCondition + "getProperty('" + PreconditionParentPath + "/jobSteps[" + NodeToDo + "]/status') == 'completed'";
                            RelPreCondition = RelPreCondition + ") true;]\"";
                            // we need to release the resource
                            string RelArgs = String.Format("{0}, subprocedure => 'GUBP_Release_AgentShare', parallel => '1', jobStepName => 'Release_{1}', actualParameter => [{{actualParameterName => 'AgentSharingGroup', valued => '{2}'}}], releaseMode => 'release', precondition => '{3}'",
                                BaseArgs, MyAgentGroup, MyAgentGroup, RelPreCondition);
                            StepList.Add(RelArgs);
                        }
                    }
                }
            }
            WriteECPerl(StepList);
            RunECTool(String.Format("setProperty \"/myWorkflow/HasTests\" \"{0}\"", bHasTests));
            {
                ECProps.Add("GUBP_LoadedProps=1");
                string BranchDefFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "BranchDef.properties");
                CommandUtils.WriteAllLines(BranchDefFile, ECProps.ToArray());
                RunECTool(String.Format("setProperty \"/myWorkflow/BranchDefFile\" \"{0}\"", BranchDefFile.Replace("\\", "\\\\")));
            }
            {
                ECProps.Add("GUBP_LoadedJobProps=1");
                string BranchJobDefFile = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "BranchJobDef.properties");
                CommandUtils.WriteAllLines(BranchJobDefFile, ECProps.ToArray());
                RunECTool(String.Format("setProperty \"/myJob/BranchJobDefFile\" \"{0}\"", BranchJobDefFile.Replace("\\", "\\\\")));
            }
            if (bFakeEC)
            {
                foreach (var Args in FakeECArgs)
                {
                    RunUAT(CmdEnv, Args);
                }
            }
            Log("Commander setup only, done.");
            PrintRunTime();
            return;

        }
        if (ParseParam("SaveGraph"))
        {
            SaveGraphVisualization(OrdereredToDo);
        }
        if (bListOnly)
        {
            Log("List only, done.");
            return;
        }    
        var BuildProductToNodeMap = new Dictionary<string, string>();
		foreach (var NodeToDo in OrdereredToDo)
        {
            if (GUBPNodes[NodeToDo].BuildProducts != null || GUBPNodes[NodeToDo].AllDependencyBuildProducts != null)
            {
                throw new AutomationException("topological sort error");
            }

            GUBPNodes[NodeToDo].AllDependencyBuildProducts = new List<string>();
            GUBPNodes[NodeToDo].AllDependencies = new List<string>();
            foreach (var Dep in GUBPNodes[NodeToDo].FullNamesOfDependencies)
            {
                GUBPNodes[NodeToDo].AddAllDependent(Dep);
                if (GUBPNodes[Dep].AllDependencies == null)
                {
                    if (!bOnlyNode)
                    {
                        throw new AutomationException("Node {0} was not processed yet3?  Processing {1}", Dep, NodeToDo);
                    }
                }
                else
				{
					foreach (var DepDep in GUBPNodes[Dep].AllDependencies)
					{
						GUBPNodes[NodeToDo].AddAllDependent(DepDep);
					}
				}
				
                if (GUBPNodes[Dep].BuildProducts == null)
                {
                    if (!bOnlyNode)
                    {
                        throw new AutomationException("Node {0} was not processed yet? Processing {1}", Dep, NodeToDo);
                    }
                }
                else
                {
                    foreach (var Prod in GUBPNodes[Dep].BuildProducts)
                    {
                        GUBPNodes[NodeToDo].AddDependentBuildProduct(Prod);
                    }
                    if (GUBPNodes[Dep].AllDependencyBuildProducts == null)
                    {
                        throw new AutomationException("Node {0} was not processed yet2?  Processing {1}", Dep, NodeToDo);
                    }
                    foreach (var Prod in GUBPNodes[Dep].AllDependencyBuildProducts)
                    {
                        GUBPNodes[NodeToDo].AddDependentBuildProduct(Prod);
                    }
                }
            }

            string NodeStoreName = StoreName + "-" + GUBPNodes[NodeToDo].GetFullName();
            
            string GameNameIfAny = GUBPNodes[NodeToDo].GameNameIfAnyForTempStorage();
            string StorageRootIfAny = GUBPNodes[NodeToDo].RootIfAnyForTempStorage();
					
            if (bFake)
            {
                StorageRootIfAny = ""; // we don't rebase fake runs since those are entirely "records of success", which are always in the logs folder
            }

            // this is kinda complicated
            bool SaveSuccessRecords = (IsBuildMachine || bFakeEC) && // no real reason to make these locally except for fakeEC tests
                (!GUBPNodes[NodeToDo].TriggerNode() || GUBPNodes[NodeToDo].IsSticky()) // trigger nodes are run twice, one to start the new workflow and once when it is actually triggered, we will save reconds for the latter
                && (GUBPNodes[NodeToDo].RunInEC() || !GUBPNodes[NodeToDo].IsAggregate()); //aggregates not in EC can be "run" multiple times, so we can't track those

            Log("***** Running GUBP Node {0} -> {1} : {2}", GUBPNodes[NodeToDo].GetFullName(), GameNameIfAny, NodeStoreName);
            if (NodeIsAlreadyComplete(NodeToDo, LocalOnly))
            {
                if (NodeToDo == VersionFilesNode.StaticGetFullName() && !IsBuildMachine)
                {
                    Log("***** NOT ****** Retrieving GUBP Node {0} from {1}; it is the version files.", GUBPNodes[NodeToDo].GetFullName(), NodeStoreName);
                    GUBPNodes[NodeToDo].BuildProducts = new List<string>();

                }
                else
                {
                    Log("***** Retrieving GUBP Node {0} from {1}", GUBPNodes[NodeToDo].GetFullName(), NodeStoreName);
                    bool WasLocal;
					try
					{
						GUBPNodes[NodeToDo].BuildProducts = RetrieveFromTempStorage(CmdEnv, NodeStoreName, out WasLocal, GameNameIfAny, StorageRootIfAny);
					}
					catch
					{
						if(GameNameIfAny != "")
						{
							GUBPNodes[NodeToDo].BuildProducts = RetrieveFromTempStorage(CmdEnv, NodeStoreName, out WasLocal, "", StorageRootIfAny);
						}
						else
						{
							throw new AutomationException("Build Products cannot be found for node {0}", NodeToDo);
						}
					}
                    if (!WasLocal)
                    {
                        GUBPNodes[NodeToDo].PostLoadFromSharedTempStorage(this);
                    }
                }
            }
            else
            {
                if (SaveSuccessRecords) 
                {
                    SaveStatus(NodeToDo, StartedTempStorageSuffix, NodeStoreName, bSaveSharedTempStorage, GameNameIfAny);
                }
				var BuildDuration = 0.0;
                try
                {
                    if (!String.IsNullOrEmpty(FakeFail) && FakeFail.Equals(NodeToDo, StringComparison.InvariantCultureIgnoreCase))
                    {
                        throw new AutomationException("Failing node {0} by request.", NodeToDo);
                    }
                    if (bFake)
                    {
                        Log("***** FAKE!! Building GUBP Node {0} for {1}", NodeToDo, NodeStoreName);
                        GUBPNodes[NodeToDo].DoFakeBuild(this);
                    }
                    else
                    {                        
						Log("***** Building GUBP Node {0} for {1}", NodeToDo, NodeStoreName);
						var StartTime = DateTime.UtcNow;
                        GUBPNodes[NodeToDo].DoBuild(this);
						BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000;
						
                    }
                    if (!GUBPNodes[NodeToDo].IsAggregate())
                    {
						var StoreDuration = 0.0;
						var StartTime = DateTime.UtcNow;
                        StoreToTempStorage(CmdEnv, NodeStoreName, GUBPNodes[NodeToDo].BuildProducts, !bSaveSharedTempStorage, GameNameIfAny, StorageRootIfAny);
						StoreDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000;
						Log("Took {0} seconds to store build products", StoreDuration);
						if (IsBuildMachine)
						{
							RunECTool(String.Format("setProperty \"/myJobStep/StoreDuration\" \"{0}\"", StoreDuration.ToString()));
						}
                        if (ParseParam("StompCheck"))
                        {
                            foreach (var Dep in GUBPNodes[NodeToDo].AllDependencies)
                            {
                                try
                                {
                                    bool WasLocal;
									RetrieveFromTempStorage(CmdEnv, NodeStoreName, out WasLocal, GameNameIfAny, StorageRootIfAny);
									if (!WasLocal)
									{
										throw new AutomationException("Retrieve was not local?");
									}
																	                                    
                                }
                                catch(Exception Ex)
                                {
                                    throw new AutomationException("Node {0} stomped Node {1}   Ex: {2}", NodeToDo, Dep, LogUtils.FormatException(Ex));
                                }
                            }
                        }

                    }
                }
                catch (Exception Ex)
                {
                    if (SaveSuccessRecords)
                    {
                        UpdateNodeHistory(NodeToDo, CLString);						
                        SaveStatus(NodeToDo, FailedTempStorageSuffix, NodeStoreName, bSaveSharedTempStorage, GameNameIfAny, ParseParamValue("MyJobStepId"));
                        UpdateECProps(NodeToDo, CLString);
						if (IsBuildMachine)
						{
							GetFailureEmails(NodeToDo, CLString);
						}
						UpdateECBuildTime(NodeToDo, BuildDuration);
                    }

                    Log("{0}", ExceptionToString(Ex));


                    if (GUBPNodesHistory.ContainsKey(NodeToDo))
                    {
                        var History = GUBPNodesHistory[NodeToDo];
                        Log("Changes since last green *********************************");
                        Log("");
                        Log("");
                        Log("");
                        PrintDetailedChanges(History);
                        Log("End changes since last green");
                    }

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

                    string Filename = CombinePaths(CmdEnv.LogFolder, "LogTailsAndChanges.log");
                    WriteAllText(Filename, FailInfo);

                    throw(Ex);
                }
                if (SaveSuccessRecords) 
                {
                    UpdateNodeHistory(NodeToDo, CLString);					
                    SaveStatus(NodeToDo, SucceededTempStorageSuffix, NodeStoreName, bSaveSharedTempStorage, GameNameIfAny);
                    UpdateECProps(NodeToDo, CLString);
					if (IsBuildMachine)
					{
						GetFailureEmails(NodeToDo, CLString);
					}
					UpdateECBuildTime(NodeToDo, BuildDuration);
                }
            }
            foreach (var Product in GUBPNodes[NodeToDo].BuildProducts)
            {
                if (BuildProductToNodeMap.ContainsKey(Product))
                {
                    throw new AutomationException("Overlapping build product: {0} and {1} both produce {2}", BuildProductToNodeMap[Product], NodeToDo, Product);
                }
                BuildProductToNodeMap.Add(Product, NodeToDo);
            }
        }

        PrintRunTime();
    }

	/// <summary>
	/// Sorts a list of nodes to display in EC. The default order is based on execution order and agent groups, whereas this function arranges nodes by
	/// frequency then execution order, while trying to group nodes on parallel paths (eg. Mac/Windows editor nodes) together.
	/// </summary>
	static Dictionary<string, int> GetDisplayOrder(List<string> NodeNames, Dictionary<string, string> InitialNodeDependencyNames, Dictionary<string, GUBPNode> GUBPNodes)
	{
		// Split the nodes into separate lists for each frequency
		SortedDictionary<int, List<string>> NodesByFrequency = new SortedDictionary<int,List<string>>();
		foreach(string NodeName in NodeNames)
		{
			List<string> NodesByThisFrequency;
			if(!NodesByFrequency.TryGetValue(GUBPNodes[NodeName].DependentCISFrequencyQuantumShift(), out NodesByThisFrequency))
			{
				NodesByThisFrequency = new List<string>();
				NodesByFrequency.Add(GUBPNodes[NodeName].DependentCISFrequencyQuantumShift(), NodesByThisFrequency);
			}
			NodesByThisFrequency.Add(NodeName);
		}

		// Build the output list by scanning each frequency in order
		HashSet<string> VisitedNodes = new HashSet<string>();
		Dictionary<string, int> SortedNodes = new Dictionary<string,int>();
		foreach(List<string> NodesByThisFrequency in NodesByFrequency.Values)
		{
			// Find a list of nodes in each display group. If the group name matches the node name, put that node at the front of the list.
			Dictionary<string, string> DisplayGroups = new Dictionary<string,string>();
			foreach(string NodeName in NodesByThisFrequency)
			{
				string GroupName = GUBPNodes[NodeName].GetDisplayGroupName();
				if(!DisplayGroups.ContainsKey(GroupName))
				{
					DisplayGroups.Add(GroupName, NodeName);
				}
				else if(GroupName == NodeName)
				{
					DisplayGroups[GroupName] = NodeName + " " + DisplayGroups[GroupName];
				}
				else
				{
					DisplayGroups[GroupName] = DisplayGroups[GroupName] + " " + NodeName;
				}
			}

			// Build a list of ordering dependencies, putting all Mac nodes after Windows nodes with the same names.
			Dictionary<string, string> NodeDependencyNames = new Dictionary<string,string>(InitialNodeDependencyNames);
			foreach(KeyValuePair<string, string> DisplayGroup in DisplayGroups)
			{
				string[] GroupNodes = DisplayGroup.Value.Split(' ');
				for(int Idx = 1; Idx < GroupNodes.Length; Idx++)
				{
					NodeDependencyNames[GroupNodes[Idx]] += " " + GroupNodes[0];
				}
			}

			// Add nodes for each frequency into the master list, trying to match up different groups along the way
			foreach(string FirstNodeName in NodesByThisFrequency)
			{
				string[] GroupNodeNames = DisplayGroups[GUBPNodes[FirstNodeName].GetDisplayGroupName()].Split(' ');
				foreach(string GroupNodeName in GroupNodeNames)
				{
					AddNodeAndDependencies(GroupNodeName, NodeDependencyNames, VisitedNodes, SortedNodes);
				}
			}
		}
		return SortedNodes;
	}

	static void AddNodeAndDependencies(string NodeName, Dictionary<string, string> NodeDependencyNames, HashSet<string> VisitedNodes, Dictionary<string, int> SortedNodes)
	{
		if(!VisitedNodes.Contains(NodeName))
		{
			VisitedNodes.Add(NodeName);
			foreach(string NodeDependencyName in NodeDependencyNames[NodeName].Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries))
			{
				AddNodeAndDependencies(NodeDependencyName, NodeDependencyNames, VisitedNodes, SortedNodes);
			}
			SortedNodes.Add(NodeName, SortedNodes.Count);
		}
	}

    string StartedTempStorageSuffix = "_Started";
    string FailedTempStorageSuffix = "_Failed";
    string SucceededTempStorageSuffix = "_Succeeded";
}
