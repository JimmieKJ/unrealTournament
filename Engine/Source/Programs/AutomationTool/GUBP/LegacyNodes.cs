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
    public abstract class GUBPNode
    {
        public List<string> FullNamesOfDependencies = new List<string>();
        public List<string> FullNamesOfPseudodependencies = new List<string>(); //these are really only used for sorting. We want the editor to fail before the monolithics. Think of it as "can't possibly be useful without".
        public List<string> BuildProducts = null;
        public List<string> AllDependencyBuildProducts = null;
        public string AgentSharingGroup = "";

        public virtual string GetFullName()
        {
            throw new AutomationException("Unimplemented GetFullName.");
        }
		public virtual string GetDisplayGroupName()
		{
			return GetFullName();
		}
        /// <summary>
        /// This function is legacy and deprecated for new usage. It purely exists to maintain an
        /// older hack to determine which <see cref="FullGameAggregateNode"/>s to add.
        /// </summary>
        /// <returns></returns>
        public virtual string GameNameIfAnyForFullGameAggregateNode()
        {
            return "";
        }
		public virtual BuildNodeDefinition GetDefinition(GUBP bp)
		{
			return new LegacyNodeDefinition(bp, this);
		}
        public virtual void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
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
		public virtual bool NotifyOnWarnings()
		{
			return true;
		}
		public virtual bool IsTest()
		{
			return false;
		}
		public virtual string NodeHostPlatform()
		{
			return "";
		}
        public virtual int AgentMemoryRequirement()
        {
            return 0;
        }
        public virtual int TimeoutInMinutes()
        {
            return 90;
        }
		public abstract string[] GetAgentTypes();

        /// <summary>
        /// When triggered by CIS (instead of a person) this dictates how often this node runs.
        /// The builder has a increasing index, specified with -TimeIndex=N
        /// If N mod (1 lshift CISFrequencyQuantumShift()) is not 0, the node is skipped
        /// </summary>
		public virtual int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return 0;
        }

        public virtual float Priority()
        {
            return 100.0f;
        }
        public virtual string ECAgentString()
        {
            return "";
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
            if (!FullNamesOfPseudodependencies.Contains(Node))
            {
                FullNamesOfPseudodependencies.Add(Node);
            }
        }
        public void RemovePseudodependency(string Node)
        {
            if (FullNamesOfPseudodependencies.Contains(Node))
            {
                FullNamesOfPseudodependencies.Remove(Node);
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
        public void RemoveOveralppingBuildProducts()
        {
            foreach (var ToRemove in AllDependencyBuildProducts)
            {
                BuildProducts.RemoveAll(FileName => FileName.Equals(ToRemove, StringComparison.InvariantCultureIgnoreCase));
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

		public override string[] GetAgentTypes()
		{
			return new string[]{ };
		}

        public override void DoBuild(GUBP bp)
        {
			if (CommandUtils.P4Enabled && CommandUtils.AllowSubmit)
			{
				var UE4Build = new UE4Build(bp);
				BuildProducts = UE4Build.UpdateVersionFiles(ActuallyUpdateVersionFiles: true);
			}
			else
			{
				BuildProducts = new List<string>();
				SaveRecordOfSuccessAndAddToBuildProducts();
			}
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
		public override string[] GetAgentTypes()
		{
			return new string[]{ HostPlatform.ToString() };
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
		public override BuildNodeDefinition GetDefinition(GUBP bp)
		{
			BuildNodeDefinition Definition = base.GetDefinition(bp);
			Definition.AgentPlatform = GetAgentPlatform();
			return Definition;
		}
        public virtual UnrealTargetPlatform GetAgentPlatform()
        {
            return HostPlatform;
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
		protected GUBP.GUBPBranchConfig BranchConfig;
		bool bDependentOnCompileTools = true;
        public CompileNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, bool DependentOnCompileTools = true)
            : base(InHostPlatform)
        {
			BranchConfig = InBranchConfig;
			bDependentOnCompileTools = DependentOnCompileTools;
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
        public virtual bool DeleteBuildProducts()
        {
            return false;
        }
		public override string[] GetAgentTypes()
		{
			return new string[]{ "Compile" + HostPlatform.ToString(), HostPlatform.ToString() };
		}
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            var UE4Build = new UE4Build(bp);
            UE4Build.BuildAgenda Agenda = GetAgenda(bp);
            if (Agenda != null)
            {
				bool ReallyDeleteBuildProducts = DeleteBuildProducts() && !BranchConfig.bForceIncrementalCompile;
                Agenda.DoRetries = false; // these would delete build products
				bool UseParallelExecutor = bDependentOnCompileTools && (HostPlatform == UnrealTargetPlatform.Win64);
				UE4Build.Build(Agenda, InDeleteBuildProducts: ReallyDeleteBuildProducts, InUpdateVersionFiles: false, InForceNoXGE: true, InForceUnity: true, InUseParallelExecutor: UseParallelExecutor);
				using(TelemetryStopwatch PostBuildStopwatch = new TelemetryStopwatch("PostBuild"))
				{
					PostBuild(bp, UE4Build);
				}
                UE4Build.CheckBuildProducts(UE4Build.BuildProductFiles);
                foreach (var Product in UE4Build.BuildProductFiles)
                {
                    AddBuildProduct(Product);
                }
                RemoveOveralppingBuildProducts();
            }
			if (Agenda == null || (BuildProducts.Count == 0 && BranchConfig.bForceIncrementalCompile))
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
		bool bHasLauncherParam;
		bool bNoInstalledEngine;

        public ToolsForCompileNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, bool bInHasLauncherParam)
            : base(InBranchConfig, InHostPlatform, false)
        {
			bHasLauncherParam = bInHasLauncherParam;
			bNoInstalledEngine = InBranchConfig.BranchOptions.bNoInstalledEngine;

			if (InHostPlatform != UnrealTargetPlatform.Win64)
            {
				AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(InHostPlatform);
			}
        }
        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "ToolsForCompile" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
        public override bool IsSticky()
        {
            if (HostPlatform == UnrealTargetPlatform.Win64)
            {
                return true;
            }
            return false;
        }
		public override BuildNodeDefinition GetDefinition(GUBP bp)
		{
			BuildNodeDefinition Definition = base.GetDefinition(bp);
			if(HostPlatform == UnrealTargetPlatform.Win64)
			{
				Definition.IsParallelAgentShareEditor = true;
			}
			return Definition;
		}
        public override bool DeleteBuildProducts()
        {
            return true;
        }
		public override int AgentMemoryRequirement()
        {
			if (bHasLauncherParam)
            {
                return base.AgentMemoryRequirement();
            }
            return 32;
        }
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();
			if (HostPlatform == UnrealTargetPlatform.Win64 && !BranchConfig.bForceIncrementalCompile)
            {
                Agenda.DotNetProjects.AddRange(
                    new string[] 
					{
						CombinePaths(@"Engine\Source\Programs\DotNETCommon\DotNETUtilities\DotNETUtilities.csproj"),
						CombinePaths(@"Engine\Source\Programs\RPCUtility\RPCUtility.csproj"),
					}
				);
            }
            string AddArgs = "-CopyAppBundleBackToDevice";

            Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs + (bNoInstalledEngine? "" : " -precompile"));
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
        public RootEditorNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
            : base(InBranchConfig, InHostPlatform)
        {
            if (InHostPlatform != UnrealTargetPlatform.Win64)
            {
				AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(InHostPlatform);
			}
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

			if(!BranchConfig.BranchOptions.bNoInstalledEngine)
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
        public override bool IsSticky()
        {
            if (HostPlatform == UnrealTargetPlatform.Win64)
            {
                return true;
            }
            return false;
        }
		public override BuildNodeDefinition GetDefinition(GUBP bp)
		{
			BuildNodeDefinition Definition = base.GetDefinition(bp);
			if (HostPlatform == UnrealTargetPlatform.Win64)
			{
				Definition.IsParallelAgentShareEditor = true;
			}
			return Definition;
		}
		public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string AddArgs = "-nobuilduht";
			if (!BranchConfig.BranchOptions.bNoInstalledEngine)
			{
				AddArgs += " -precompile";
			}
            Agenda.AddTargets(
                new string[] { BranchConfig.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
                HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
            foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(HostPlatform))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
                }
            }
            return Agenda;
        }
    }
	public class RootEditorCrossCompileLinuxNode : CompileNode
	{
		public RootEditorCrossCompileLinuxNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
			: base(InBranchConfig, InHostPlatform)
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			return base.CISFrequencyQuantumShift(BranchConfig) + 3;
		}
		public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
		{
			var Agenda = new UE4Build.BuildAgenda();

			string AddArgs = "-nobuilduht";
			Agenda.AddTargets(
				new string[] { BranchConfig.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
				UnrealTargetPlatform.Linux, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
			 foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
            {
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithBaseEditor() && ProgramTarget.Rules.SupportsPlatform(UnrealTargetPlatform.Linux))
                {
                    Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, UnrealTargetPlatform.Linux, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
                }
            }
			return Agenda;
		}
	}
    public class ToolsNode : CompileNode
    {
        public ToolsNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
            : base(InBranchConfig, InHostPlatform)
        {
			if(!BranchConfig.BranchOptions.bNoEditorDependenciesForTools)
			{
				AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
			}
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
				if (!BranchConfig.bForceIncrementalCompile)
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
				if (!BranchConfig.bForceIncrementalCompile)
                {
					Agenda.SwarmProject = CombinePaths(@"Engine\Source\Programs\UnrealSwarm\UnrealSwarm.sln");
				}

				bool WithIOS = !BranchConfig.BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.IOS);
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

				bool WithHTML5 = !BranchConfig.BranchOptions.PlatformsToRemove.Contains(UnrealTargetPlatform.HTML5);
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

            foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
            {
                bool bInternalOnly;
                bool SeparateNode;
				bool CrossCompile;
                if (ProgramTarget.Rules.GUBP_AlwaysBuildWithTools(HostPlatform, out bInternalOnly, out SeparateNode, out CrossCompile) && ProgramTarget.Rules.SupportsPlatform(HostPlatform) && !bInternalOnly && !SeparateNode)
                {
					if (!BranchConfig.BranchOptions.ExcludeNodes.Contains(ProgramTarget.TargetName))
                    {
						// Always compile CRC with VS2013 due to requiring the 2015 redist
						string AddTargetArgs = AddArgs;
						if(String.Compare(ProgramTarget.TargetName, "CrashReportClient", true) == 0 || String.Compare(ProgramTarget.TargetName, "UnrealCEFSubProcess", true) == 0)
						{
							AddTargetArgs += " -2013";
						}
						foreach (var Plat in ProgramTarget.Rules.GUBP_ToolPlatforms(HostPlatform))
						{
							foreach (var Config in ProgramTarget.Rules.GUBP_ToolConfigs(HostPlatform))
							{
								Agenda.AddTargets(new string[] { ProgramTarget.TargetName }, Plat, Config, InAddArgs: AddTargetArgs);
							}
						}
					}
				}
            }

            return Agenda;
        }
    }
	public class ToolsCrossCompileNode : CompileNode
	{
		public ToolsCrossCompileNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
			: base(InBranchConfig, InHostPlatform)
		{
			if (!InBranchConfig.BranchOptions.ExcludePlatformsForEditor.Contains(UnrealTargetPlatform.Linux))
			{
				AddPseudodependency(RootEditorCrossCompileLinuxNode.StaticGetFullName());
			}
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

			foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
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
        public SingleToolsNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
            : base(InBranchConfig, InHostPlatform)
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
        public InternalToolsNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
            : base(InBranchConfig, InHostPlatform)
        {
			if(!InBranchConfig.BranchOptions.bNoEditorDependenciesForTools)
			{
				AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
			}
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            int Result = base.CISFrequencyQuantumShift(BranchConfig) + 1;
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
                    CombinePaths(@"Engine\Source\Programs\CrashReporter\CrashReportCommon\CrashReportCommon.csproj"),
					CombinePaths(@"Engine\Source\Programs\CrashReporter\CrashReportReceiver\CrashReportReceiver.csproj"),
					CombinePaths(@"Engine\Source\Programs\CrashReporter\CrashReportProcess\CrashReportProcess.csproj"),
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

			foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
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
        public SingleInternalToolsNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
            : base(InBranchConfig, InHostPlatform)
        {
			SetupSingleInternalToolsNode(InProgramTarget);
        }
		public SingleInternalToolsNode(GUBP bp, GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, SingleTargetProperties InProgramTarget)
			: base(InBranchConfig, InHostPlatform)
        {
			// Don't add rooteditor dependency if it isn't in the graph
			var bRootEditorNodeDoesExit = BranchConfig.HasNode(RootEditorNode.StaticGetFullName(HostPlatform));
			SetupSingleInternalToolsNode(InProgramTarget, !bRootEditorNodeDoesExit && BranchConfig.BranchOptions.bNoEditorDependenciesForTools);
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            int Result = base.CISFrequencyQuantumShift(BranchConfig) + 1;                             
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

    public class EditorGameNode : CompileNode
    {
		List<BranchInfo.BranchUProject> GameProjects = new List<BranchInfo.BranchUProject>();

        public EditorGameNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj)
            : base(InBranchConfig, InHostPlatform)
        {
            if (InHostPlatform != UnrealTargetPlatform.Win64)
            {
				AgentSharingGroup = "Editor"  + StaticGetHostPlatformSuffix(InHostPlatform);
            }
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
        public override bool IsSticky()
        {
            if (HostPlatform == UnrealTargetPlatform.Win64)
	        {
                return true;
            }
            return false;
        }
		public override BuildNodeDefinition GetDefinition(GUBP bp)
		{
			BuildNodeDefinition Definition = base.GetDefinition(bp);
			if (HostPlatform == UnrealTargetPlatform.Win64)
			{
				Definition.IsParallelAgentShareEditor = true;
			}
			return Definition;
		}
		public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProjects[0].Options(HostPlatform).GroupName ?? GameProjects[0].GameName;
        }                    
        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
            var Agenda = new UE4Build.BuildAgenda();

            string Args = "-nobuilduht -skipactionhistory -skipnonhostplatforms -CopyAppBundleBackToDevice -forceheadergeneration";
			
			if(!BranchConfig.BranchOptions.bNoInstalledEngine)
			{
				Args += " -precompile";
			}

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
			bool bHasContents = CommandUtils.FileExists(CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(InGameProj.FilePath.FullName), "contents.txt"));
			bool bHasManifest = CommandUtils.FileExists(CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(InGameProj.FilePath.FullName), "manifest.json"));
			return bHasContents && bHasManifest;
		}

		public static UnrealTargetPlatform GetDefaultBuildPlatform(List<UnrealTargetPlatform> HostPlatforms)
		{
			if(HostPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				return UnrealTargetPlatform.Win64;
			}
			else if(HostPlatforms.Contains(UnrealTargetPlatform.Mac))
			{
				return UnrealTargetPlatform.Mac;
			}
			else
			{
				if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux && HostPlatforms[0] != UnrealTargetPlatform.Linux)
				{
					throw new AutomationException("Linux is not (yet?) able to cross-compile nodes for platform {0}, did you forget -NoPC / -NoMac?", HostPlatforms[0]);
				}

				return HostPlatforms[0];
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

		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			return base.CISFrequencyQuantumShift(BranchConfig) + 2;
		}
		public override void DoBuild(GUBP bp)
        {
			BuildProducts = new List<string>();
			foreach(BranchInfo.BranchUProject Project in Projects)
			{
				string ContentsFileName = CommandUtils.CombinePaths(CommandUtils.GetDirectoryName(Project.FilePath.FullName), "contents.txt");

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
				else if (HostPlatform == UnrealTargetPlatform.Linux)
				{
					UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Linux/UnrealPak");
				}
                else if (HostPlatform == UnrealTargetPlatform.Mac)
                {
                    UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Mac/UnrealPak");
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
		List<UnrealTargetPlatform> ActivePlatforms;
		protected List<TargetRules.TargetType> ActiveMonolithicKinds;
        UnrealTargetPlatform TargetPlatform;
		bool WithXp;
		bool Precompiled; // If true, just builds targets which generate static libraries for the -UsePrecompiled option to UBT. If false, just build those that don't.
		bool EnhanceAgentRequirements;
		public List<UnrealTargetConfiguration> ExcludeConfigurations = new List<UnrealTargetConfiguration>();
		public static readonly Dictionary<UnrealTargetPlatform, string[]> PrecompiledArchitectures = new Dictionary<UnrealTargetPlatform, string[]>
		{
			{UnrealTargetPlatform.Android, new string[] {"armv7", "arm64"/*, "x86", "x64"*/ } }
		};
		public static readonly Dictionary<UnrealTargetPlatform, string[]> PrecompiledGPUArchitectures = new Dictionary<UnrealTargetPlatform, string[]>
		{
			{UnrealTargetPlatform.Android, new string[] {"es2"/*, "es31"*/ } }
		};

        public GamePlatformMonolithicsNode(GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InActivePlatforms, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, bool InWithXp = false, bool InPrecompiled = false)
            : base(InBranchConfig, InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;
			ActivePlatforms = InActivePlatforms;
			WithXp = InWithXp;
			Precompiled = InPrecompiled;
			EnhanceAgentRequirements = BranchConfig.BranchOptions.EnhanceAgentRequirements.Contains(StaticGetFullName(HostPlatform, GameProj, TargetPlatform, WithXp, Precompiled));
			ActiveMonolithicKinds = BranchInfo.MonolithicKinds;

            if (TargetPlatform == UnrealTargetPlatform.PS4 || TargetPlatform == UnrealTargetPlatform.XboxOne)
            {
				// Required for PS4MapFileUtil/XboxOnePDBFileUtil
				AddDependency(ToolsNode.StaticGetFullName(InHostPlatform));
			}

			bool bBehindTrigger = false;
			if(IsSample(BranchConfig, InGameProj))
			{
				AddPseudodependency(WaitToPackageSamplesNode.StaticGetFullName());
				bBehindTrigger = true;
			}

            if (InGameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
				if (!BranchConfig.BranchOptions.ExcludePlatformsForEditor.Contains(InHostPlatform))
				{
					AddPseudodependency(EditorGameNode.StaticGetFullName(InHostPlatform, GameProj));
				}
				if (BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, TargetPlatform)))
                {
					AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, TargetPlatform));
                }
            }
            else
            {
				if (TargetPlatform != InHostPlatform && BranchConfig.HasNode(GamePlatformMonolithicsNode.StaticGetFullName(InHostPlatform, BranchConfig.Branch.BaseEngineProject, InHostPlatform, Precompiled: Precompiled)))
                {
					AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(InHostPlatform, BranchConfig.Branch.BaseEngineProject, InHostPlatform, Precompiled: Precompiled));
                }
            }
            if (InGameProj.Options(InHostPlatform).bTestWithShared)  /// compiling templates is only for testing purposes, and we will group them to avoid saturating the farm
            {
                AddPseudodependency(WaitForTestShared.StaticGetFullName());
				bBehindTrigger = true;
                //AgentSharingGroup = "TemplateMonolithics" + StaticGetHostPlatformSuffix(InHostPlatform);
            }
			if(!InBranchConfig.BranchOptions.bTargetPlatformsInParallel)
			{
				if(bBehindTrigger)
				{
					AgentSharingGroup = "TargetPlatformsTest" + StaticGetHostPlatformSuffix(InHostPlatform);
				}
				else
				{
					AgentSharingGroup = "TargetPlatforms" + StaticGetHostPlatformSuffix(InHostPlatform);
				}
			}
        }

		public static bool IsSample(GUBPBranchConfig BranchConfig, BranchInfo.BranchUProject GameProj)
		{
			return (GameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName && (GameProj.FilePath.FullName.Contains("Samples") || GameProj.FilePath.FullName.Contains("Templates")));
		}

		public override string GetDisplayGroupName()
		{
			return GameProj.GameName + "_Monolithics" + (Precompiled? "_Precompiled" : "");
		}

		public static List<UnrealTargetPlatform> GetMonolithicPlatformsForUProject(UnrealTargetPlatform HostPlatform, List<UnrealTargetPlatform> ActivePlatforms, BranchInfo.BranchUProject GameProj, bool bIncludeHostPlatform, bool bNoIOSOnPC)
		{
			UnrealTargetPlatform AltHostPlatform = GUBP.GetAltHostPlatform(HostPlatform);
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
						if (bNoIOSOnPC && Plat == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64)
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
        public override bool DeleteBuildProducts()
        {
            return true;
        }
		public override int AgentMemoryRequirement()
        {
			if (EnhanceAgentRequirements)
            {
                return 64;
            }
            return base.AgentMemoryRequirement();
        }
        public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {            
            int Result = base.CISFrequencyQuantumShift(BranchConfig);
			if (GameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName || !Precompiled)
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
            if (!ActivePlatforms.Contains(TargetPlatform))
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

				string[] PlatformArchitectures;
				if (PrecompiledArchitectures.TryGetValue(TargetPlatform, out PlatformArchitectures)
					&& PlatformArchitectures.Length > 0)
				{
					Args += " -architectures=" + String.Join("+", PlatformArchitectures);
				}
				if (PrecompiledGPUArchitectures.TryGetValue(TargetPlatform, out PlatformArchitectures)
					&& PlatformArchitectures.Length > 0)
				{
					Args += " -gpuarchitectures=" + String.Join("+", PlatformArchitectures);
				}
			}

			if (WithXp)
			{
				Args += " -winxp";
			}

            foreach (var Kind in ActiveMonolithicKinds)
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
							
							foreach (var Config in Configs.Where(x => !ExcludeConfigurations.Contains(x)))
							{
								if (GameProj.GameName == BranchConfig.Branch.BaseEngineProject.GameName)
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

	public class GamePlatformMonolithicsKindNode : GamePlatformMonolithicsNode
	{
		public GamePlatformMonolithicsKindNode(GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InActivePlatforms, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, TargetRules.TargetType InTargetKind, bool InWithXp = false, bool InPrecompiled = false)
			: base(InBranchConfig, InHostPlatform, InActivePlatforms, InGameProj, InTargetPlatform, InWithXp, InPrecompiled)
		{
			ActiveMonolithicKinds = new List<TargetRules.TargetType>();
			ActiveMonolithicKinds.Add(InTargetKind);
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, TargetRules.TargetType InTargetKind, bool WithXp = false, bool Precompiled = false)
		{
			string Name = GamePlatformMonolithicsNode.StaticGetFullName(InHostPlatform, InGameProj, InTargetPlatform, WithXp, Precompiled);
			return Name + "_" + InTargetKind;
		}

		public override string GetFullName()
		{
			string Name = base.GetFullName();
			return Name + "_" + ActiveMonolithicKinds[0];
		}

		public static bool HasPrecompiledTargets(BranchInfo.BranchUProject Project, UnrealTargetPlatform HostPlatform, UnrealTargetPlatform TargetPlatform, TargetRules.TargetType InTargetKind)
		{
			if (Project.Properties.Targets.ContainsKey(InTargetKind))
			{
				SingleTargetProperties Target = Project.Properties.Targets[InTargetKind];
				if (Target.Rules.GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(HostPlatform, TargetPlatform).Any())
				{
					return true;
				}
			}
			return false;
		}	
	}

    public class SingleTargetNode : CompileNode
    {
        BranchInfo.BranchUProject GameProj;
		string TargetName;
        UnrealTargetPlatform TargetPlatform;
		UnrealTargetConfiguration TargetConfiguration;

		public SingleTargetNode(GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InTargetName, UnrealTargetPlatform InTargetPlatform, UnrealTargetConfiguration InTargetConfiguration)
            : base(InBranchConfig, InHostPlatform)
        {
            GameProj = InGameProj;
			TargetName = InTargetName;
            TargetPlatform = InTargetPlatform;
			TargetConfiguration = InTargetConfiguration;

			AddPseudodependency(RootEditorNode.StaticGetFullName(InHostPlatform));

            if (TargetPlatform == UnrealTargetPlatform.PS4 || TargetPlatform == UnrealTargetPlatform.XboxOne)
            {
				// Required for PS4MapFileUtil/XboxOnePDBFileUtil
				AddDependency(ToolsNode.StaticGetFullName(InHostPlatform));
			}

			if(!InBranchConfig.BranchOptions.bTargetPlatformsInParallel)
			{
				AgentSharingGroup = "TargetPlatforms" + StaticGetHostPlatformSuffix(InHostPlatform);
			}
        }

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, UnrealTargetConfiguration InTargetConfiguration)
        {
			return InGameProj.GameName + "_" + InTargetPlatform + "_" + InTargetConfiguration.ToString() + StaticGetHostPlatformSuffix(InHostPlatform);
        }

        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, TargetPlatform, TargetConfiguration);
        }

        public override bool DeleteBuildProducts()
        {
            return true;
        }

		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
			return 15;
        }

        public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
        {
           string Args = "-nobuilduht -skipactionhistory -CopyAppBundleBackToDevice";

            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			if (GameProj.GameName == BranchConfig.Branch.BaseEngineProject.GameName)
			{
				Agenda.AddTargets(new string[] { TargetName }, TargetPlatform, TargetConfiguration, InAddArgs: Args);
			}
			else
			{
				Agenda.AddTargets(new string[] { TargetName }, TargetPlatform, TargetConfiguration, GameProj.FilePath, InAddArgs: Args);
			}
            return Agenda;
        }
	}

    public class SuccessNode : GUBPNode
    {
        public SuccessNode()
        {
        }

		public override string[] GetAgentTypes()
		{
			return new string[]{ };
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

    public abstract class GUBPAggregateNode
    {
		public List<string> Dependencies = new List<string>();

        public GUBPAggregateNode()
        {
        }

		public void AddDependency(string Name)
		{
			if(!Dependencies.Contains(Name))
			{
				Dependencies.Add(Name);
			}
		}

		public AggregateNodeDefinition GetDefinition()
		{
			AggregateNodeDefinition Definition = new AggregateNodeDefinition();
			Definition.Name = GetFullName();
			Definition.DependsOn = String.Join(";", Dependencies);
			return Definition;
		}

		public abstract string GetFullName();

        public virtual string GameNameIfAnyForFullGameAggregateNode()
		{
			return "";
		}
    }

    public abstract class HostPlatformAggregateNode : GUBPAggregateNode
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

	public class FullGameAggregateNode : GUBPAggregateNode
	{
		string GameName;

		public FullGameAggregateNode(string InGameName, List<string> NodeNames)
		{
			GameName = InGameName;

			foreach (string NodeName in NodeNames)
			{
				AddDependency(NodeName);
			}
		}

		public static string StaticGetFullName(string GameName)
		{
			return "FullGameAggregate_" + GameName;
		}

		public override string GetFullName()
		{
			return StaticGetFullName(GameName);
		}
	}

	public class GenericAggregateNode : GUBPAggregateNode
	{
		string Name;

		public GenericAggregateNode(string InName, IEnumerable<string> NodeNames)
		{
			Name = InName;

			foreach(string NodeName in NodeNames)
			{
				AddDependency(NodeName);
			}
		}

		public override string GetFullName()
		{
			return Name;
		}
	}

    public class WaitForUserInput : GUBPNode
    {
        public WaitForUserInput()
        {
        }

		public override string[] GetAgentTypes()
		{
			return new string[]{ };
		}
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
        public override BuildNodeDefinition GetDefinition(GUBP bp)
        {
			return new TriggerNodeDefinition(bp, this);
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
        public override int TimeoutInMinutes()
        {
            return 0;
        }
    }

    public class WaitToPackageSamplesNode : WaitForUserInput
    {
        public WaitToPackageSamplesNode(IEnumerable<UnrealTargetPlatform> HostPlatforms)
        {
			foreach(UnrealTargetPlatform HostPlatform in HostPlatforms)
			{
				AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
			}
		}

        public static string StaticGetFullName()
        {
            return "WaitToPackageSamples";
        }

        public override string GetFullName()
        {
            return StaticGetFullName();
        }

        public override string GetTriggerDescText()
        {
			return "Ready to package samples";
        }

        public override string GetTriggerActionText()
        {
			return "Package samples";
        }
    }

    public class WaitForTestShared : GUBPAggregateNode
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
    }

    public class CookNode : HostPlatformNode
    {
        public BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;
        string CookPlatform;
        bool bIsMassive;
		
		public String ExtraArgsForCook = "";

        public CookNode(GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, string InCookPlatform)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            TargetPlatform = InTargetPlatform;
            CookPlatform = InCookPlatform;
            bIsMassive = false;
            AddDependency(EditorAndToolsNode.StaticGetFullName(HostPlatform));
            bool bIsShared = false;
            // is this the "base game" or a non code project?
            if (InGameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
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
				if (!BranchConfig.BranchOptions.bNoMonolithicDependenciesForCooks)
				{
					AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, GameProj, TargetPlatform));
				}
            }
            else
            {
                bIsShared = true;				
                AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, TargetPlatform));
            }
            if (bIsShared)
            {
                // add an arc to prevent cooks from running until promotable is labeled
                AddPseudodependency(WaitForTestShared.StaticGetFullName());
                AgentSharingGroup = "SharedCooks" + StaticGetHostPlatformSuffix(HostPlatform);

                // If the cook fails for the base engine, don't bother trying
                if (InGameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName && BranchConfig.HasNode(CookNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, CookPlatform)))
                {
                    AddPseudodependency(CookNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, CookPlatform));
                }

                // If the base cook platform fails, don't bother trying other ones
                string BaseCookedPlatform = Platform.GetPlatform(HostPlatform).GetCookPlatform(false, false);
                if (InGameProj.GameName == BranchConfig.Branch.BaseEngineProject.GameName && CookPlatform != BaseCookedPlatform &&
                    BranchConfig.HasNode(CookNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, BaseCookedPlatform)))
                {
                    AddPseudodependency(CookNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, BaseCookedPlatform));
                }
            }

			if(InGameProj.GameName == BranchConfig.Branch.BaseEngineProject.GameName)
			{
				ExtraArgsForCook += " -WarningsAsErrors";
			}
            ExtraArgsForCook += " " + BranchConfig.BranchOptions.AdditionalCookArgs;

			if(GamePlatformMonolithicsNode.IsSample(BranchConfig, GameProj))
			{
				AddPseudodependency(WaitToPackageSamplesNode.StaticGetFullName());
				AgentSharingGroup = "SampleCooks" + StaticGetHostPlatformSuffix(HostPlatform);
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return base.CISFrequencyQuantumShift(BranchConfig) + 4 + (bIsMassive ? 1 : 0);
        }
        public override float Priority()
        {
            return 10.0f;
        }
		public override int AgentMemoryRequirement()
        {
            return bIsMassive ? 32 : 0;
        }
        public override int TimeoutInMinutes()
        {
            return bIsMassive ? 240 : base.TimeoutInMinutes();
        }
		public override bool NotifyOnWarnings()
		{
			return false;
		}

		public string RootForCook()
		{
			return CombinePaths(Path.GetDirectoryName(GameProj.FilePath.FullName), "Saved", "Cooked", CookPlatform);
		}

		//this lets the extra build products through, but filepaths become too long for windows. revert for now.
#if false
        public override string RootIfAnyForTempStorage()
        {
            return CombinePaths(Path.GetDirectoryName(GameProj.FilePath), "Saved");
        }

		private string RootForCookedFiles()
		{
			return CombinePaths(RootIfAnyForTempStorage(), "Cooked", CookPlatform);
		}

		private string RootForChunkFiles()
		{
			return CombinePaths(RootIfAnyForTempStorage(), "TmpPackaging", CookPlatform);
		}
#endif

        public override void DoBuild(GUBP bp)
        {
            if (HostPlatform == UnrealTargetPlatform.Mac)
            {
                // not sure if we need something here or if the cook commandlet will automatically convert the exe name
            }
			using(TelemetryStopwatch CookStopwatch = new TelemetryStopwatch("Cook.{0}.{1}", GameProj.GameName, CookPlatform))
			{
				String CookArgs = String.Format("-Unversioned {0}", ExtraArgsForCook);
				CommandUtils.CookCommandlet(GameProj.FilePath, "UE4Editor-Cmd.exe", null, null, null, null, CookPlatform, CookArgs);
			}
			var CookedPath = RootForCook();
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

			//can't add these until we straighten out the storage root.
#if false
			//add in any chunk data files if they exist.
			String TmpPackagingPath = RootForChunkFiles();
			var PackagingFiles = CommandUtils.FindFiles("*", true, TmpPackagingPath);
			foreach (var PackageDataFile in PackagingFiles)
			{
				AddBuildProduct(PackageDataFile);
			}
#endif
        }
    }

    public class GamePlatformCookedAndCompiledNode : HostPlatformAggregateNode
    {
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform TargetPlatform;

        public GamePlatformCookedAndCompiledNode(GUBP.GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InTargetPlatform, bool bCodeProject)
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
                            string CookedPlatform = Platform.GetPlatform(TargetPlatform).GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client);
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
                        if (BranchConfig.Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                        {
                            var Target = BranchConfig.Branch.BaseEngineProject.Properties.Targets[Kind];
                            var Platforms = Target.Rules.GUBP_GetPlatforms_MonolithicOnly(HostPlatform);
                            if (Platforms.Contains(TargetPlatform) && Target.Rules.SupportsPlatform(TargetPlatform))
                            {
                                //@todo how do we get the client target platform?
                                string CookedPlatform = Platform.GetPlatform(TargetPlatform).GetCookPlatform(Kind == TargetRules.TargetType.Server, Kind == TargetRules.TargetType.Client);
                                AddDependency(CookNode.StaticGetFullName(HostPlatform, GameProj, CookedPlatform));
                                AddDependency(GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, TargetPlatform));
                            }
                        }
                    }
                }
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
    };

    public class FormalBuildNode : HostPlatformNode
    {
		GUBP.GUBPBranchConfig BranchConfig;
        BranchInfo.BranchUProject GameProj;

        //CAUTION, these are lists, but it isn't clear that lists really work on all platforms, so we stick to one node per build
        List<UnrealTargetPlatform> ClientTargetPlatforms;
        List<UnrealTargetPlatform> ServerTargetPlatforms;
        List<UnrealTargetConfiguration> ClientConfigs;
        List<UnrealTargetConfiguration> ServerConfigs;
        bool ClientNotGame;
		bool bIsCode;
        UnrealBuildTool.TargetRules.TargetType GameOrClient;

        public FormalBuildNode(GUBP.GUBPBranchConfig InBranchConfig,
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
			BranchConfig = InBranchConfig;
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
			if (InGameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName && GameProj.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
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
                WorkingGameProject = BranchConfig.Branch.BaseEngineProject;
            }

            var AllTargetPlatforms = new List<UnrealTargetPlatform>();					
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

		public static string StaticGetBaseName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InClientTargetPlatforms = null, List<UnrealTargetConfiguration> InClientConfigs = null, List<UnrealTargetPlatform> InServerTargetPlatforms = null, List<UnrealTargetConfiguration> InServerConfigs = null, bool InClientNotGame = false)
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
			return InGameProj.GameName + Infix;
        }
		public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InClientTargetPlatforms = null, List<UnrealTargetConfiguration> InClientConfigs = null, List<UnrealTargetPlatform> InServerTargetPlatforms = null, List<UnrealTargetConfiguration> InServerConfigs = null, bool InClientNotGame = false)
		{
			return StaticGetBaseName(InGameProj, InHostPlatform, InClientTargetPlatforms, InClientConfigs, InServerTargetPlatforms, InServerConfigs, InClientNotGame) + "_MakeBuild" + HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
		}

		public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform, ClientTargetPlatforms, ClientConfigs, ServerTargetPlatforms, ServerConfigs, ClientNotGame);
        }
        public override string GameNameIfAnyForFullGameAggregateNode()
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			return base.CISFrequencyQuantumShift(BranchConfig) + 3;
		}

		public static string GetBranchArchiveDirectory(GUBP.GUBPBranchConfig BranchConfig, BranchInfo.BranchUProject InGameProj)
		{
			// Find the build share where formal builds will be placed for this game.
			string BuildShareName;
			if (!BranchConfig.BranchOptions.GameNameToBuildShareMapping.TryGetValue(InGameProj.GameName, out BuildShareName))
			{
				BuildShareName = "UE4";
			}
			return CommandUtils.CombinePaths(CommandUtils.RootBuildStorageDirectory(), BuildShareName, "PackagedBuilds", P4Env.BuildRootEscaped);
		}

		public static string GetArchiveDirectory(GUBP.GUBPBranchConfig BranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InClientTargetPlatforms = null, List<UnrealTargetConfiguration> InClientConfigs = null, List<UnrealTargetPlatform> InServerTargetPlatforms = null, List<UnrealTargetConfiguration> InServerConfigs = null, bool InClientNotGame = false)
        {
			string BranchArchiveDir = GetBranchArchiveDirectory(BranchConfig, InGameProj);
            string BaseDir = CommandUtils.CombinePaths(BranchArchiveDir, BranchConfig.JobInfo.BuildName);
			string NodeName = StaticGetBaseName(InGameProj, InHostPlatform, InClientTargetPlatforms, InClientConfigs, InServerTargetPlatforms, InServerConfigs, InClientNotGame);
			string ArchiveDirectory = CombinePaths(BaseDir, NodeName);
            return ArchiveDirectory;
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            string ProjectArg = "";
            if (GameProj.FilePath != null)
            {
                ProjectArg = " -project=\"" + GameProj.FilePath.FullName + "\"";
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
                FinalArchiveDirectory = GetArchiveDirectory(BranchConfig, GameProj, HostPlatform, ClientTargetPlatforms, ClientConfigs, ServerTargetPlatforms, ServerConfigs, ClientNotGame);
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
				CleanFormalBuilds(GetBranchArchiveDirectory(BranchConfig, GameProj), "CL-*");
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
		GUBP.GUBPBranchConfig BranchConfig;
        BranchInfo.BranchUProject GameProj;
        UnrealTargetPlatform ClientTargetPlatform;
        UnrealTargetConfiguration ClientConfig;
        UnrealBuildTool.TargetRules.TargetType GameOrClient;

        public FormalBuildTestNode(GUBP.GUBPBranchConfig InBranchConfig,
            BranchInfo.BranchUProject InGameProj,
            UnrealTargetPlatform InHostPlatform,
            UnrealTargetPlatform InClientTargetPlatform,
            UnrealTargetConfiguration InClientConfig
            )
            : base(InHostPlatform)
        {
			BranchConfig = InBranchConfig;
            GameProj = InGameProj;
            ClientTargetPlatform = InClientTargetPlatform;
            ClientConfig = InClientConfig;
            GameOrClient = TargetRules.TargetType.Game;

            // verify we actually built these
            var WorkingGameProject = InGameProj;
            if (!WorkingGameProject.Properties.Targets.ContainsKey(TargetRules.TargetType.Editor))
            {
                // this is a codeless project, use the base project
                WorkingGameProject = BranchConfig.Branch.BaseEngineProject;
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
        public override float Priority()
        {
            return base.Priority() - 20.0f;
        }
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return base.CISFrequencyQuantumShift(BranchConfig) + 3;
        }
        public override void DoTest(GUBP bp)
        {
            string ProjectArg = "";
            if (GameProj.FilePath != null)
            {
                ProjectArg = " -project=\"" + GameProj.FilePath + "\"";
            }

            string ArchiveDirectory = FormalBuildNode.GetArchiveDirectory(BranchConfig, GameProj, HostPlatform, new List<UnrealTargetPlatform>() { ClientTargetPlatform }, InClientConfigs: new List<UnrealTargetConfiguration>() { ClientConfig }, InClientNotGame: GameOrClient == TargetRules.TargetType.Client);
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			int Result = base.CISFrequencyQuantumShift(BranchConfig) + 2;
			if (HostPlatform == UnrealTargetPlatform.Mac)
			{
				Result += 1;
			}		
			return Result;
		}
		public override int AgentMemoryRequirement()
		{
			int Result = base.AgentMemoryRequirement();
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

			Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false, InForceNonUnity: true, InForceNoXGE: true);

			UE4Build.CheckBuildProducts(Build.BuildProductFiles);
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}

    public class NonUnityTestNode : TestNode
    {
		GUBP.GUBPBranchConfig BranchConfig;

        public NonUnityTestNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
			BranchConfig = InBranchConfig;
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            int Result = base.CISFrequencyQuantumShift(BranchConfig) + 2;
            if(HostPlatform == UnrealTargetPlatform.Mac)
            {
                Result += 1;
            }
            return Result;
        }
		public override int AgentMemoryRequirement()
        {
            int Result = base.AgentMemoryRequirement();
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
                new string[] { BranchConfig.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
                HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-skipnonhostplatforms -shadowvariableerrors");

            foreach (var Kind in BranchInfo.MonolithicKinds)
            {
                if (BranchConfig.Branch.BaseEngineProject.Properties.Targets.ContainsKey(Kind))
                {
                    var Target = BranchConfig.Branch.BaseEngineProject.Properties.Targets[Kind];
                    Agenda.AddTargets(new string[] { Target.TargetName }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-shadowvariableerrors");
                }
            }

            Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false, InForceNonUnity: true, InForceNoXGE: true);

            UE4Build.CheckBuildProducts(Build.BuildProductFiles);
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }

    public class StaticAnalysisTestNode : TestNode
    {
        public StaticAnalysisTestNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            AddDependency(ToolsForCompileNode.StaticGetFullName(HostPlatform));
            AddPseudodependency(RootEditorNode.StaticGetFullName(HostPlatform));
			AgentSharingGroup = "TargetPlatforms" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
		public override string[] GetAgentTypes()
		{
			return new string[]{ "CompileWin64", "Win64" };
		}
		public override float Priority()
		{
			return -100000.0f;
		}
		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
        {
            return "UE4_Win64_StaticAnalysis" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform);
        }
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            int Result = base.CISFrequencyQuantumShift(BranchConfig) + 2;
            if(HostPlatform == UnrealTargetPlatform.Mac)
            {
                Result += 1;
            }
            return Result;
        }
		public override int AgentMemoryRequirement()
        {
            int Result = base.AgentMemoryRequirement();
            if(HostPlatform == UnrealTargetPlatform.Mac)
            {
                Result = 32;
            }
            return Result;
        }
        public override void DoTest(GUBP bp)
        {
            UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
            Agenda.AddTargets(new string[] { "UE4Game" }, HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: "-enablecodeanalysis");

			UE4Build Build = new UE4Build(bp);
            Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false, InForceNoXGE: true);

            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }

    public class IOSOnPCTestNode : TestNode
    {
		GUBP.GUBPBranchConfig BranchConfig;

        public IOSOnPCTestNode(GUBP.GUBPBranchConfig InBranchConfig)
            : base(UnrealTargetPlatform.Win64)
        {
			BranchConfig = InBranchConfig;
            AddDependency(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
            AddDependency(ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64));
			AddPseudodependency(GamePlatformMonolithicsNode.StaticGetFullName(UnrealTargetPlatform.Mac, BranchConfig.Branch.BaseEngineProject, UnrealTargetPlatform.IOS));
        }
        public static string StaticGetFullName()
        {
            return "IOSOnPCTestCompile";
        }
        public override string GetFullName()
        {
            return StaticGetFullName();
        }
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return base.CISFrequencyQuantumShift(BranchConfig) + 3;
        }
        public override void DoTest(GUBP bp)
        {
            var Build = new UE4Build(bp);
            var Agenda = new UE4Build.BuildAgenda();

            Agenda.AddTargets(new string[] { BranchConfig.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Game].TargetName }, UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Development);

            Build.Build(Agenda, InDeleteBuildProducts: true, InUpdateVersionFiles: false);

            UE4Build.CheckBuildProducts(Build.BuildProductFiles);
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
    }
	
	public class VSExpressTestNode : TestNode
	{
		GUBP.GUBPBranchConfig BranchConfig;

		public VSExpressTestNode(GUBP.GUBPBranchConfig InBranchConfig)
			: base(UnrealTargetPlatform.Win64)
		{
			BranchConfig = InBranchConfig;
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			return base.CISFrequencyQuantumShift(BranchConfig) + 3;
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

			Agenda.AddTargets(
				new string[] { BranchConfig.Branch.BaseEngineProject.Properties.Targets[TargetRules.TargetType.Editor].TargetName },
				HostPlatform, UnrealTargetConfiguration.Development, InAddArgs: AddArgs);
			foreach (var ProgramTarget in BranchConfig.Branch.BaseEngineProject.Properties.Programs)
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

        public UATTestNode(GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InTestName, string InUATCommandLine, string InAgentSharingGroup, bool InDependsOnEditor = true, List<UnrealTargetPlatform> InDependsOnCooked = null, float InECPriority = 0.0f)
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
                if (GameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName)
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
            if (InGameProj.GameName != BranchConfig.Branch.BaseEngineProject.GameName)
            {
                if (BranchConfig.HasNode(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, TestName)))
                {
                    AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, InTestName));
                }
                else
                {
                    bool bFoundACook = false;
                    foreach (var Plat in DependsOnCooked)
                    {
                        var PlatTestName = "CookedGameTest_"  + Plat.ToString();
                        if (BranchConfig.HasNode(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, PlatTestName)))
                        {
                            AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, PlatTestName));
                            bFoundACook = true;
                        }
                    }

                    if (!bFoundACook && BranchConfig.HasNode(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, "EditorTest")))
                    {
                        AddPseudodependency(UATTestNode.StaticGetFullName(HostPlatform, BranchConfig.Branch.BaseEngineProject, "EditorTest"));
                    }

                }
            }

            if (InGameProj.GameName == BranchConfig.Branch.BaseEngineProject.GameName)
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
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return base.CISFrequencyQuantumShift(BranchConfig) + 5;
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
        public override void DoTest(GUBP bp)
        {
            string ProjectArg = "";
            if (GameProj.FilePath != null && UATCommandLine.IndexOf("-project=", StringComparison.InvariantCultureIgnoreCase) < 0)
            {
                ProjectArg = " -project=\"" + GameProj.FilePath.FullName + "\"";
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

	public abstract class BuildLocalizationNode : HostPlatformNode
	{
		public BuildLocalizationNode(string InLocalizationBranchSuffix)
			: base(UnrealTargetPlatform.Win64)
		{
			LocalizationBranchSuffix = InLocalizationBranchSuffix;
            AddDependency(ToolsNode.StaticGetFullName(HostPlatform));
		}

		public override bool NotifyOnWarnings()
		{
			return false;
		}

		public override void DoBuild(GUBP bp)
		{
			var UEProjectDirectory = GetUEProjectDirectory();
			var UEProjectName = GetUEProjectName();
			var LocalizationProjectNames = GetLocalizationProjectNames();
			var LocalizationProviderName = GetLocalizationProviderName();
			var LocalizationProviderArgs = GetLocalizationProviderArgs();
			var AdditionalCommandletArguments = GetAdditionalCommandletArguments();

			// Build the correct command line arguments.
			var CommandLineArguments = "";

			if (!String.IsNullOrEmpty(UEProjectDirectory))
			{
				CommandLineArguments += " -UEProjectDirectory=\"" + UEProjectDirectory + "\"";
			}

			if (!String.IsNullOrEmpty(UEProjectName))
			{
				CommandLineArguments += " -UEProjectName=\"" + UEProjectName + "\"";
			}

			if (!String.IsNullOrEmpty(LocalizationProjectNames))
			{
				CommandLineArguments += " -LocalizationProjectNames=\"" + LocalizationProjectNames + "\"";
			}

			if (!String.IsNullOrEmpty(LocalizationBranchSuffix))
			{
				CommandLineArguments += " -LocalizationBranch=\"" + LocalizationBranchSuffix + "\"";
			}

			if (!String.IsNullOrEmpty(LocalizationProviderName))
			{
				CommandLineArguments += " -LocalizationProvider=\"" + LocalizationProviderName + "\"";
			}

			if (!String.IsNullOrEmpty(LocalizationProviderArgs))
			{
				CommandLineArguments += " " + LocalizationProviderArgs;
			}

			if (!String.IsNullOrEmpty(AdditionalCommandletArguments))
			{
				CommandLineArguments += " -AdditionalCommandletArguments=\"" + AdditionalCommandletArguments + "\"";
			}

			// Run the localise script.
			CommandUtils.RunUAT(CommandUtils.CmdEnv, "Localise" + CommandLineArguments);

			BuildProducts = new List<string>();

			var SuccessMessage = "Updated the following localization build products:\n";
			try
			{
				// Just add all of the localization content as a build product, since UAT should have updated all of it
				var LocalizationContentDirectory = CombinePaths(CommandUtils.CmdEnv.LocalRoot, UEProjectDirectory, "Content", "Localization");
				foreach (string FilePath in Directory.EnumerateFiles(LocalizationContentDirectory, "*", SearchOption.AllDirectories))
				{
					AddBuildProduct(FilePath);
					SuccessMessage += "    " + FilePath + "\n";
				}
			}
			catch (Exception e)
			{
				LogWarning("Failed to add build products for localization node. The follow exception was raised '%s'", e);
			}

			SaveRecordOfSuccessAndAddToBuildProducts(SuccessMessage);
		}

		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
			return base.CISFrequencyQuantumShift(BranchConfig) + 6;
		}

		protected virtual string GetUEProjectDirectory()
		{
			throw new AutomationException("Unimplemented GetUEProjectDirectory.");
		}

		protected virtual string GetUEProjectName()
		{
			throw new AutomationException("Unimplemented GetUEProjectName.");
		}

		protected virtual string GetLocalizationProjectNames()
		{
			throw new AutomationException("Unimplemented GetLocalizationProjectNames.");
		}

		protected virtual string GetLocalizationProviderName()
		{
			throw new AutomationException("Unimplemented GetLocalizationProviderName.");
		}

		protected virtual string GetLocalizationProviderArgs()
		{
			return "";
		}

		protected virtual string GetAdditionalCommandletArguments()
		{
			return "";
		}

		protected string LocalizationBranchSuffix;
	}

	public class BuildEngineLocalizationNode : BuildLocalizationNode
	{
		public BuildEngineLocalizationNode(string InLocalizationBranchSuffix)
			: base(InLocalizationBranchSuffix)
		{
			AddDependency(RootEditorNode.StaticGetFullName(HostPlatform));
		}

		public static string StaticGetFullName()
		{
			return "BuildEngineLocalization";
		}

		public override string GetFullName()
		{
			return StaticGetFullName();
		}

		protected override string GetUEProjectDirectory()
		{
			return "Engine";
		}

		protected override string GetUEProjectName()
		{
			return "";
		}

		protected override string GetLocalizationProjectNames()
		{
			return "Engine,Editor,EditorTutorials,PropertyNames,ToolTips,Category,Keywords";
		}

		protected override string GetLocalizationProviderName()
		{
			return "OneSky";
		}

		protected override string GetLocalizationProviderArgs()
		{
			return "-OneSkyConfigName=\"OneSkyConfig_EpicGames\" -OneSkyProjectGroupName=\"Unreal Engine\"";
		}
	}

    public class GameAggregateNode : HostPlatformAggregateNode
    {
        BranchInfo.BranchUProject GameProj;
        string AggregateName;

        public GameAggregateNode(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InAggregateName, List<string> Dependencies)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            AggregateName = InAggregateName;

            foreach (var Dep in Dependencies)
            {
                AddDependency(Dep);
            }
        }

        public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InGameProj, string InAggregateName)
        {
            return InGameProj.GameName + "_" + InAggregateName + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(HostPlatform, GameProj, AggregateName);
        }
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
    };

    public class CleanSharedTempStorageNode : GUBPNode
    {
        string RootNameForTempStorage;

        public CleanSharedTempStorageNode(GUBP bp, GUBPBranchConfig BranchConfig)
        {
            var ToolsNode = BranchConfig.FindNode(ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
            RootNameForTempStorage = BranchConfig.BranchOptions.RootNameForTempStorage ?? "UE4";
            AgentSharingGroup = ToolsNode.AgentSharingGroup;
        }
        public override float Priority()
        {
            return -1E15f;
        }

		public override string[] GetAgentTypes()
		{
			return new string[]{ "CompileWin64", "Win64" };
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
                TempStorage.CleanSharedTempStorageDirectory(RootNameForTempStorage);
                var BuildDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
                Log("Took {0}s to clear temp storage of old files.", BuildDuration / 1000);
            }

            BuildProducts = new List<string>();
            SaveRecordOfSuccessAndAddToBuildProducts();
        }
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
        {
            return base.CISFrequencyQuantumShift(BranchConfig) + 3;
        }
    };
}
