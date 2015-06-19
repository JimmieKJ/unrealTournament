// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

namespace Rocket
{
	public class RocketBuild : GUBP.GUBPNodeAdder
	{
		static readonly string[] CurrentTemplates = 
		{
			"FP_FirstPerson",
			"FP_FirstPersonBP",
			"TP_FirstPerson",
			"TP_FirstPersonBP",
			"TP_Flying",
			"TP_FlyingBP",
			"TP_Rolling",
			"TP_RollingBP",
			"TP_SideScroller",
			"TP_SideScrollerBP",
			"TP_ThirdPerson",
			"TP_ThirdPersonBP",
			"TP_TopDown",
			"TP_TopDownBP",
			"TP_TwinStick",
			"TP_TwinStickBP",
			"TP_Vehicle",
			"TP_VehicleBP",
			"TP_Puzzle",
			"TP_PuzzleBP",
			"TP_2DSideScroller",
			"TP_2DSideScrollerBP",
			"TP_VehicleAdv",
			"TP_VehicleAdvBP",
		};

		static readonly string[] CurrentFeaturePacks = 
		{
			"FP_FirstPerson",
			"FP_FirstPersonBP",
			"TP_Flying",
			"TP_FlyingBP",
			"TP_Rolling",
			"TP_RollingBP",
			"TP_SideScroller",
			"TP_SideScrollerBP",
			"TP_ThirdPerson",
			"TP_ThirdPersonBP",
			"TP_TopDown",
			"TP_TopDownBP",
			"TP_TwinStick",
			"TP_TwinStickBP",
			"TP_Vehicle",
			"TP_VehicleBP",
			"TP_Puzzle",
			"TP_PuzzleBP",
			"TP_2DSideScroller",
			"TP_2DSideScrollerBP",
			"TP_VehicleAdv",
			"TP_VehicleAdvBP",
			"StarterContent",
			"MobileStarterContent"
		};

		public RocketBuild()
		{
		}

		public override void AddNodes(GUBP bp, UnrealTargetPlatform HostPlatform)
		{
			if(!bp.BranchOptions.bNoInstalledEngine)
			{
				// Find all the target platforms for this host platform.
				List<UnrealTargetPlatform> TargetPlatforms = GetTargetPlatforms(bp, HostPlatform);

				// Remove any platforms that aren't available on this machine
				TargetPlatforms.RemoveAll(x => !bp.ActivePlatforms.Contains(x));

				// Get the temp directory for stripped files for this host
				string StrippedDir = Path.GetFullPath(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Rocket", HostPlatform.ToString()));

				// Strip the host platform
				if (StripRocketNode.IsRequiredForPlatform(HostPlatform))
				{
					bp.AddNode(new StripRocketToolsNode(HostPlatform, StrippedDir));
					bp.AddNode(new StripRocketEditorNode(HostPlatform, StrippedDir));
				}

				// Strip all the target platforms that are built on this host
				foreach (UnrealTargetPlatform TargetPlatform in TargetPlatforms)
				{
					if (GetSourceHostPlatform(bp, HostPlatform, TargetPlatform) == HostPlatform && StripRocketNode.IsRequiredForPlatform(TargetPlatform))
					{
						bp.AddNode(new StripRocketMonolithicsNode(bp, HostPlatform, TargetPlatform, StrippedDir));
					}
				}

				// Build the DDC
				bp.AddNode(new BuildDerivedDataCacheNode(HostPlatform, GetCookPlatforms(HostPlatform, TargetPlatforms), CurrentFeaturePacks));

				// Generate a list of files that needs to be copied for each target platform
				bp.AddNode(new FilterRocketNode(bp, HostPlatform, TargetPlatforms, CurrentFeaturePacks, CurrentTemplates));

				// Copy the install to the output directory
				string LocalOutputDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "LocalBuilds", "Rocket", CommandUtils.GetGenericPlatformName(HostPlatform));
				bp.AddNode(new GatherRocketNode(HostPlatform, TargetPlatforms, LocalOutputDir));

				// Add the aggregate node for the entire install
				GUBP.GUBPNode PromotableNode = bp.FindNode(GUBP.SharedAggregatePromotableNode.StaticGetFullName());
				PromotableNode.AddDependency(FilterRocketNode.StaticGetFullName(HostPlatform));
				PromotableNode.AddDependency(BuildDerivedDataCacheNode.StaticGetFullName(HostPlatform));

				// Add a node for GitHub promotions
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					string GitConfigRelativePath = "Engine/Build/Git/UnrealBot.ini";
					if(CommandUtils.FileExists(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, GitConfigRelativePath)))
					{
						bp.AddNode(new BuildGitPromotable(bp, HostPlatform, GitConfigRelativePath));
						PromotableNode.AddDependency(BuildGitPromotable.StaticGetFullName(HostPlatform));
					}
				}

				// Get the output directory for the build zips
				string PublishedEngineDir;
				if(ShouldDoSeriousThingsLikeP4CheckinAndPostToMCP())
				{
					PublishedEngineDir = CommandUtils.CombinePaths(CommandUtils.RootSharedTempStorageDirectory(), "Rocket", "Automated", GetBuildLabel(), CommandUtils.GetGenericPlatformName(HostPlatform));
				}
				else
				{
					PublishedEngineDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "LocalBuilds", "RocketPublish", CommandUtils.GetGenericPlatformName(HostPlatform));
				}

				// Publish the install to the network
				bp.AddNode(new PublishRocketNode(HostPlatform, LocalOutputDir, PublishedEngineDir));
				bp.AddNode(new PublishRocketSymbolsNode(bp, HostPlatform, TargetPlatforms, PublishedEngineDir + "Symbols"));

				// Add a dependency on this being published as part of the shared promotable being labeled
				GUBP.SharedLabelPromotableSuccessNode LabelPromotableNode = (GUBP.SharedLabelPromotableSuccessNode)bp.FindNode(GUBP.SharedLabelPromotableSuccessNode.StaticGetFullName());
				LabelPromotableNode.AddDependency(PublishRocketNode.StaticGetFullName(HostPlatform));
				LabelPromotableNode.AddDependency(PublishRocketSymbolsNode.StaticGetFullName(HostPlatform));

				// Add dependencies on a promotable to do these steps too
				GUBP.WaitForSharedPromotionUserInput WaitForPromotionNode = (GUBP.WaitForSharedPromotionUserInput)bp.FindNode(GUBP.WaitForSharedPromotionUserInput.StaticGetFullName(true));
				WaitForPromotionNode.AddDependency(PublishRocketNode.StaticGetFullName(HostPlatform));
				WaitForPromotionNode.AddDependency(PublishRocketSymbolsNode.StaticGetFullName(HostPlatform));

				// Push everything behind the promotion triggers if we're doing things on the build machines
				if(ShouldDoSeriousThingsLikeP4CheckinAndPostToMCP() || bp.ParseParam("WithRocketPromotable"))
				{
					string WaitForTrigger = GUBP.WaitForSharedPromotionUserInput.StaticGetFullName(false);

					GatherRocketNode GatherRocket = (GatherRocketNode)bp.FindNode(GatherRocketNode.StaticGetFullName(HostPlatform));
					GatherRocket.AddDependency(WaitForTrigger);

					PublishRocketSymbolsNode PublishRocketSymbols = (PublishRocketSymbolsNode)bp.FindNode(PublishRocketSymbolsNode.StaticGetFullName(HostPlatform));
					PublishRocketSymbols.AddDependency(WaitForTrigger);
				}
			}
		}

		public static string GetBuildLabel()
		{
			return FEngineVersionSupport.FromVersionFile(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine\Source\Runtime\Launch\Resources\Version.h")).ToString();
		}

		public static List<UnrealTargetPlatform> GetTargetPlatforms(BuildCommand Command, UnrealTargetPlatform HostPlatform)
		{
			List<UnrealTargetPlatform> TargetPlatforms = new List<UnrealTargetPlatform>();
			if(!Command.ParseParam("NoTargetPlatforms"))
			{
				// Always support the host platform
				TargetPlatforms.Add(HostPlatform);

				// Add other target platforms for each host platform
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					TargetPlatforms.Add(UnrealTargetPlatform.Win32);
				}
				if(HostPlatform == UnrealTargetPlatform.Win64 || HostPlatform == UnrealTargetPlatform.Mac)
				{
					TargetPlatforms.Add(UnrealTargetPlatform.Android);
				}
				if(HostPlatform == UnrealTargetPlatform.Win64 || HostPlatform == UnrealTargetPlatform.Mac)
				{
					TargetPlatforms.Add(UnrealTargetPlatform.IOS);
				}
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					TargetPlatforms.Add(UnrealTargetPlatform.Linux);
				}
				if(HostPlatform == UnrealTargetPlatform.Win64 || HostPlatform == UnrealTargetPlatform.Mac )
				{
					TargetPlatforms.Add(UnrealTargetPlatform.HTML5);
				}

				// Remove any platforms that aren't enabled on the command line
				string TargetPlatformFilter = Command.ParseParamValue("TargetPlatforms", null);
				if(TargetPlatformFilter != null)
				{
					List<UnrealTargetPlatform> NewTargetPlatforms = new List<UnrealTargetPlatform>();
					foreach (string TargetPlatformName in TargetPlatformFilter.Split(new char[]{ '+' }, StringSplitOptions.RemoveEmptyEntries))
					{
						UnrealTargetPlatform TargetPlatform;
						if(!Enum.TryParse(TargetPlatformName, out TargetPlatform))
						{
							throw new AutomationException("Unknown target platform '{0}' specified on command line");
						}
						else if(TargetPlatforms.Contains(TargetPlatform))
						{
							NewTargetPlatforms.Add(TargetPlatform);
						}
					}
					TargetPlatforms = NewTargetPlatforms;
				}
			}
			return TargetPlatforms;
		}

		public static string GetCookPlatforms(UnrealTargetPlatform HostPlatform, IEnumerable<UnrealTargetPlatform> TargetPlatforms)
		{
			// Always include the editor platform for cooking
			List<string> CookPlatforms = new List<string>();
			CookPlatforms.Add(Platform.Platforms[HostPlatform].GetEditorCookPlatform());

			// Add all the target platforms
			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				if(TargetPlatform == UnrealTargetPlatform.Android)
				{
					CookPlatforms.Add(Platform.Platforms[TargetPlatform].GetCookPlatform(false, false, "ATC"));
				}
				else
				{
					CookPlatforms.Add(Platform.Platforms[TargetPlatform].GetCookPlatform(false, false, ""));
				}
			}
			return CommandUtils.CombineCommandletParams(CookPlatforms.Distinct().ToArray());
		}

		public static bool ShouldDoSeriousThingsLikeP4CheckinAndPostToMCP()
		{
			return CommandUtils.P4Enabled && CommandUtils.AllowSubmit && !GUBP.bPreflightBuild; // we don't do serious things in a preflight
		}

		public static UnrealTargetPlatform GetSourceHostPlatform(GUBP bp, UnrealTargetPlatform HostPlatform, UnrealTargetPlatform TargetPlatform)
		{
            if (TargetPlatform == UnrealTargetPlatform.HTML5 && HostPlatform == UnrealTargetPlatform.Mac && bp.HostPlatforms.Contains(UnrealTargetPlatform.Win64))
            {
                return UnrealTargetPlatform.Win64;
            }
			if (TargetPlatform == UnrealTargetPlatform.Android && HostPlatform == UnrealTargetPlatform.Mac && bp.HostPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				return UnrealTargetPlatform.Win64;
			}
			if(TargetPlatform == UnrealTargetPlatform.IOS && HostPlatform == UnrealTargetPlatform.Win64 && bp.HostPlatforms.Contains(UnrealTargetPlatform.Mac))
			{
				return UnrealTargetPlatform.Mac;
			}
			return HostPlatform;
		}

		public static bool IsCodeTargetPlatform(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform TargetPlatform)
		{
			if(TargetPlatform == UnrealTargetPlatform.Linux)
			{
				return false;
			}
			if(HostPlatform == UnrealTargetPlatform.Win64 && TargetPlatform == UnrealTargetPlatform.IOS)
			{
				return false;
			}
			return true;
		}
	}

	public class BuildGitPromotable : GUBP.HostPlatformNode
	{
		string ConfigRelativePath;

		public BuildGitPromotable(GUBP bp, UnrealTargetPlatform HostPlatform, string InConfigRelativePath) : base(HostPlatform)
		{
			ConfigRelativePath = InConfigRelativePath;

			foreach(UnrealTargetPlatform OtherHostPlatform in bp.HostPlatforms)
			{
				AddDependency(GUBP.RootEditorNode.StaticGetFullName(OtherHostPlatform));
				AddDependency(GUBP.ToolsNode.StaticGetFullName(OtherHostPlatform));
				AddDependency(GUBP.InternalToolsNode.StaticGetFullName(OtherHostPlatform));
			}
		}

		public static string StaticGetFullName(UnrealTargetPlatform HostPlatform)
		{
			return "BuildGitPromotable" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			// Create a filter for all the promoted binaries
			FileFilter PromotableFilter = new FileFilter();
			PromotableFilter.AddRuleForFiles(AllDependencyBuildProducts, CommandUtils.CmdEnv.LocalRoot, FileFilterType.Include);
			PromotableFilter.ReadRulesFromFile(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, ConfigRelativePath), "promotable");
			PromotableFilter.ExcludeConfidentialFolders();
			
			// Copy everything that matches the filter to the promotion folder
			string PromotableFolder = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "GitPromotable");
			CommandUtils.DeleteDirectoryContents(PromotableFolder);
			string[] PromotableFiles = CommandUtils.ThreadedCopyFiles(CommandUtils.CmdEnv.LocalRoot, PromotableFolder, PromotableFilter, bIgnoreSymlinks: true);
			BuildProducts = new List<string>(PromotableFiles);
		}
	}

	public abstract class StripRocketNode : GUBP.HostPlatformNode
	{
		public UnrealTargetPlatform TargetPlatform;
		public string StrippedDir;
		public List<string> NodesToStrip = new List<string>();

		public StripRocketNode(UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform InTargetPlatform, string InStrippedDir) : base(InHostPlatform)
		{
			TargetPlatform = InTargetPlatform;
			StrippedDir = InStrippedDir;
		}

		public override abstract string GetFullName();

		public void AddNodeToStrip(string NodeName)
		{
			NodesToStrip.Add(NodeName);
			AddDependency(NodeName);
		}

		public static bool IsRequiredForPlatform(UnrealTargetPlatform Platform)
		{
			return Platform != UnrealTargetPlatform.HTML5;
		}

		public override void DoBuild(GUBP bp)
		{
			BuildProducts = new List<string>();

			string InputDir = Path.GetFullPath(CommandUtils.CmdEnv.LocalRoot);
			string RulesFileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "InstalledEngineFilters.ini");

			// Read the filter for files on this platform
			FileFilter StripFilter = new FileFilter();
			StripFilter.ReadRulesFromFile(RulesFileName, "StripSymbols." + TargetPlatform.ToString(), HostPlatform.ToString());

			// Apply the filter to the build products
			List<string> SourcePaths = new List<string>();
			List<string> TargetPaths = new List<string>();
			foreach(string NodeToStrip in NodesToStrip)
			{
				GUBP.GUBPNode Node = bp.FindNode(NodeToStrip);
				foreach(string DependencyBuildProduct in Node.BuildProducts)
				{
					string RelativePath = CommandUtils.StripBaseDirectory(Path.GetFullPath(DependencyBuildProduct), InputDir);
					if(StripFilter.Matches(RelativePath))
					{
						SourcePaths.Add(CommandUtils.CombinePaths(InputDir, RelativePath));
						TargetPaths.Add(CommandUtils.CombinePaths(StrippedDir, RelativePath));
					}
				}
			}

			// Strip the files and add them to the build products
			StripSymbols(TargetPlatform, SourcePaths.ToArray(), TargetPaths.ToArray());
			BuildProducts.AddRange(TargetPaths);

			SaveRecordOfSuccessAndAddToBuildProducts();
		}

		public static void StripSymbols(UnrealTargetPlatform TargetPlatform, string[] SourceFileNames, string[] TargetFileNames)
		{
			IUEBuildPlatform Platform = UEBuildPlatform.GetBuildPlatform(TargetPlatform);
			IUEToolChain ToolChain = UEToolChain.GetPlatformToolChain(Platform.GetCPPTargetPlatform(TargetPlatform));
			for (int Idx = 0; Idx < SourceFileNames.Length; Idx++)
			{
				CommandUtils.CreateDirectory(Path.GetDirectoryName(TargetFileNames[Idx]));
				CommandUtils.Log("Stripping symbols: {0} -> {1}", SourceFileNames[Idx], TargetFileNames[Idx]);
				ToolChain.StripSymbols(SourceFileNames[Idx], TargetFileNames[Idx]);
			}
		}
	}

	public class StripRocketToolsNode : StripRocketNode
	{
		public StripRocketToolsNode(UnrealTargetPlatform InHostPlatform, string InStrippedDir)
			: base(InHostPlatform, InHostPlatform, InStrippedDir)
		{
			AddNodeToStrip(GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform));
			AddNodeToStrip(GUBP.ToolsNode.StaticGetFullName(HostPlatform));
			AgentSharingGroup = "ToolsGroup" + StaticGetHostPlatformSuffix(InHostPlatform);
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
		{
			return "Strip" + GUBP.ToolsNode.StaticGetFullName(InHostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}
	}

	public class StripRocketEditorNode : StripRocketNode
	{
		public StripRocketEditorNode(UnrealTargetPlatform InHostPlatform, string InStrippedDir)
			: base(InHostPlatform, InHostPlatform, InStrippedDir)
		{
			AddNodeToStrip(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
			AgentSharingGroup = "Editor" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public override float Priority()
		{
			return 1000000.0f;
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
		{
			return "Strip" + GUBP.RootEditorNode.StaticGetFullName(InHostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}
	}

	public class StripRocketMonolithicsNode : StripRocketNode
	{
		BranchInfo.BranchUProject Project;
		bool bIsCodeTargetPlatform;

		public StripRocketMonolithicsNode(GUBP bp, UnrealTargetPlatform InHostPlatform, UnrealTargetPlatform InTargetPlatform, string InStrippedDir) : base(InHostPlatform, InTargetPlatform, InStrippedDir)
		{
			Project = bp.Branch.BaseEngineProject;
			bIsCodeTargetPlatform = RocketBuild.IsCodeTargetPlatform(InHostPlatform, InTargetPlatform);

			GUBP.GUBPNode Node = bp.FindNode(GUBP.GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, Project, InTargetPlatform, Precompiled: bIsCodeTargetPlatform));
			if(String.IsNullOrEmpty(Node.AgentSharingGroup))
			{
				Node.AgentSharingGroup = bp.Branch.BaseEngineProject.GameName + "_MonolithicsGroup_" + InTargetPlatform + StaticGetHostPlatformSuffix(InHostPlatform);
			}
			AddNodeToStrip(Node.GetFullName());

			AgentSharingGroup = Node.AgentSharingGroup;
		}

		public override string GetDisplayGroupName()
		{
			return Project.GameName + "_Monolithics" + (bIsCodeTargetPlatform? "_Precompiled" : "");
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform, BranchInfo.BranchUProject InProject, UnrealTargetPlatform InTargetPlatform, bool bIsCodeTargetPlatform)
		{
			string Name = InProject.GameName + "_" + InTargetPlatform + "_Mono";
			if(bIsCodeTargetPlatform)
			{
				Name += "_Precompiled";
			}
			return Name + "_Strip" + StaticGetHostPlatformSuffix(InHostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform, Project, TargetPlatform, bIsCodeTargetPlatform);
		}
	}

	public class FilterRocketNode : GUBP.HostPlatformNode
	{
		List<UnrealTargetPlatform> SourceHostPlatforms;
		List<UnrealTargetPlatform> TargetPlatforms;
		string[] CurrentFeaturePacks;
		string[] CurrentTemplates;
		public readonly string DepotManifestPath;
		public Dictionary<string, string> StrippedNodeManifestPaths = new Dictionary<string, string>();

		public FilterRocketNode(GUBP bp, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InTargetPlatforms, string[] InCurrentFeaturePacks, string[] InCurrentTemplates)
			: base(InHostPlatform)
		{
			TargetPlatforms = new List<UnrealTargetPlatform>(InTargetPlatforms);
			CurrentFeaturePacks = InCurrentFeaturePacks;
			CurrentTemplates = InCurrentTemplates;
			DepotManifestPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Rocket", HostPlatform.ToString(), "Filter.txt");

			// Add the editor
			AddDependency(GUBP.VersionFilesNode.StaticGetFullName());
			AddDependency(GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform));
			AddDependency(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
			AddDependency(GUBP.ToolsNode.StaticGetFullName(HostPlatform));

			// Add all the monolithic builds from their appropriate source host platform
			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				UnrealTargetPlatform SourceHostPlatform = RocketBuild.GetSourceHostPlatform(bp, HostPlatform, TargetPlatform);
				bool bIsCodeTargetPlatform = RocketBuild.IsCodeTargetPlatform(SourceHostPlatform, TargetPlatform);
				AddDependency(GUBP.GamePlatformMonolithicsNode.StaticGetFullName(SourceHostPlatform, bp.Branch.BaseEngineProject, TargetPlatform, Precompiled: bIsCodeTargetPlatform));
			}

			// Also add stripped symbols for all the target platforms that require it
			List<string> StrippedNodeNames = new List<string>();
			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				if(StripRocketNode.IsRequiredForPlatform(TargetPlatform))
				{
					UnrealTargetPlatform SourceHostPlatform = RocketBuild.GetSourceHostPlatform(bp, HostPlatform, TargetPlatform);
					string StripNode = StripRocketMonolithicsNode.StaticGetFullName(SourceHostPlatform, bp.Branch.BaseEngineProject, TargetPlatform, RocketBuild.IsCodeTargetPlatform(SourceHostPlatform, TargetPlatform));
					AddDependency(StripNode);
					StrippedNodeNames.Add(StripNode);
				}
			}

			// Add win64 tools on Mac, to get the win64 build of UBT, UAT and IPP
			if (HostPlatform == UnrealTargetPlatform.Mac && bp.HostPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				AddDependency(GUBP.ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64));
				AddDependency(GUBP.ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64));
			}

			// Add all the feature packs
			AddDependency(GUBP.MakeFeaturePacksNode.StaticGetFullName(GUBP.MakeFeaturePacksNode.GetDefaultBuildPlatform(bp)));

			// Find all the host platforms we need
			SourceHostPlatforms = TargetPlatforms.Select(x => RocketBuild.GetSourceHostPlatform(bp, HostPlatform, x)).Distinct().ToList();
			if(!SourceHostPlatforms.Contains(HostPlatform))
			{
				SourceHostPlatforms.Add(HostPlatform);
			}

			// Add the stripped host platforms
			if(StripRocketNode.IsRequiredForPlatform(HostPlatform))
			{
				AddDependency(StripRocketToolsNode.StaticGetFullName(HostPlatform));
				StrippedNodeNames.Add(StripRocketToolsNode.StaticGetFullName(HostPlatform));

				AddDependency(StripRocketEditorNode.StaticGetFullName(HostPlatform));
				StrippedNodeNames.Add(StripRocketEditorNode.StaticGetFullName(HostPlatform));
			}

			// Set all the stripped manifest paths
			foreach(string StrippedNodeName in StrippedNodeNames)
			{
				StrippedNodeManifestPaths.Add(StrippedNodeName, Path.Combine(Path.GetDirectoryName(DepotManifestPath), "Filter_" + StrippedNodeName + ".txt"));
			}
		}

		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 2;
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
		{
			return "FilterRocket" + StaticGetHostPlatformSuffix(InHostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			BuildProducts = new List<string>();
			FileFilter Filter = new FileFilter();

			// Include all the editor products
			AddRuleForBuildProducts(Filter, bp, GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform), FileFilterType.Include);
			AddRuleForBuildProducts(Filter, bp, GUBP.RootEditorNode.StaticGetFullName(HostPlatform), FileFilterType.Include);
			AddRuleForBuildProducts(Filter, bp, GUBP.ToolsNode.StaticGetFullName(HostPlatform), FileFilterType.Include);

			// Include win64 tools on Mac, to get the win64 build of UBT, UAT and IPP
			if (HostPlatform == UnrealTargetPlatform.Mac && bp.HostPlatforms.Contains(UnrealTargetPlatform.Win64))
			{
				AddRuleForBuildProducts(Filter, bp, GUBP.ToolsNode.StaticGetFullName(UnrealTargetPlatform.Win64), FileFilterType.Include);
				AddRuleForBuildProducts(Filter, bp, GUBP.ToolsForCompileNode.StaticGetFullName(UnrealTargetPlatform.Win64), FileFilterType.Include);
			}

			// Include the editor headers
			UnzipAndAddRuleForHeaders(GUBP.RootEditorNode.StaticGetArchivedHeadersPath(HostPlatform), Filter, FileFilterType.Include);

			// Include the build dependencies for every code platform
			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				if(RocketBuild.IsCodeTargetPlatform(HostPlatform, TargetPlatform))
				{
					UnrealTargetPlatform SourceHostPlatform = RocketBuild.GetSourceHostPlatform(bp, HostPlatform, TargetPlatform);
					string FileListPath = GUBP.GamePlatformMonolithicsNode.StaticGetBuildDependenciesPath(SourceHostPlatform, bp.Branch.BaseEngineProject, TargetPlatform);
					Filter.AddRuleForFiles(UnrealBuildTool.Utils.ReadClass<UnrealBuildTool.ExternalFileList>(FileListPath).FileNames, CommandUtils.CmdEnv.LocalRoot, FileFilterType.Include);
					UnzipAndAddRuleForHeaders(GUBP.GamePlatformMonolithicsNode.StaticGetArchivedHeadersPath(SourceHostPlatform, bp.Branch.BaseEngineProject, TargetPlatform), Filter, FileFilterType.Include);
				}
			}

			// Add the monolithic binaries
			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				UnrealTargetPlatform SourceHostPlatform = RocketBuild.GetSourceHostPlatform(bp, HostPlatform, TargetPlatform);
				bool bIsCodeTargetPlatform = RocketBuild.IsCodeTargetPlatform(SourceHostPlatform, TargetPlatform);
				AddRuleForBuildProducts(Filter, bp, GUBP.GamePlatformMonolithicsNode.StaticGetFullName(SourceHostPlatform, bp.Branch.BaseEngineProject, TargetPlatform, Precompiled: bIsCodeTargetPlatform), FileFilterType.Include);
			}

			// Include the feature packs
			foreach(string CurrentFeaturePack in CurrentFeaturePacks)
			{
				BranchInfo.BranchUProject Project = bp.Branch.FindGameChecked(CurrentFeaturePack);
				Filter.AddRuleForFile(GUBP.MakeFeaturePacksNode.GetOutputFile(Project), CommandUtils.CmdEnv.LocalRoot, FileFilterType.Include);
			}

			// Include all the templates
			foreach (string Template in CurrentTemplates)
			{
				BranchInfo.BranchUProject Project = bp.Branch.FindGameChecked(Template);
				Filter.Include("/" + Utils.StripBaseDirectory(Path.GetDirectoryName(Project.FilePath), CommandUtils.CmdEnv.LocalRoot).Replace('\\', '/') + "/...");
			}

			// Include all the standard rules
			string RulesFileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "InstalledEngineFilters.ini");
			Filter.ReadRulesFromFile(RulesFileName, "CopyEditor", HostPlatform.ToString());
			Filter.ReadRulesFromFile(RulesFileName, "CopyTargetPlatforms", HostPlatform.ToString());

			// Custom rules for each target platform
			foreach(UnrealTargetPlatform TargetPlaform in TargetPlatforms)
			{
				string SectionName = String.Format("CopyTargetPlatform.{0}", TargetPlaform.ToString());
				Filter.ReadRulesFromFile(RulesFileName, SectionName, HostPlatform.ToString());
			}

			// Add the final exclusions for legal reasons.
			Filter.ExcludeConfidentialPlatforms();
			Filter.ExcludeConfidentialFolders();

			// Run the filter on the stripped symbols, and remove those files from the copy filter
			List<string> AllStrippedFiles = new List<string>();
			foreach(KeyValuePair<string, string> StrippedNodeManifestPath in StrippedNodeManifestPaths)
			{
				List<string> StrippedFiles = new List<string>();

				StripRocketNode StripNode = (StripRocketNode)bp.FindNode(StrippedNodeManifestPath.Key);
				foreach(string BuildProduct in StripNode.BuildProducts)
				{
					if(Utils.IsFileUnderDirectory(BuildProduct, StripNode.StrippedDir))
					{
						string RelativePath = CommandUtils.StripBaseDirectory(Path.GetFullPath(BuildProduct), StripNode.StrippedDir);
						if(Filter.Matches(RelativePath))
						{
							StrippedFiles.Add(RelativePath);
							AllStrippedFiles.Add(RelativePath);
							Filter.Exclude("/" + RelativePath);
						}
					}
				}

				WriteManifest(StrippedNodeManifestPath.Value, StrippedFiles);
				BuildProducts.Add(StrippedNodeManifestPath.Value);
			}

			// Write the filtered list of depot files to disk, removing any symlinks
			List<string> DepotFiles = Filter.ApplyToDirectory(CommandUtils.CmdEnv.LocalRoot, true).ToList();
			WriteManifest(DepotManifestPath, DepotFiles);
			BuildProducts.Add(DepotManifestPath);

			// Sort the list of output files
			SortedDictionary<string, bool> SortedFiles = new SortedDictionary<string,bool>(StringComparer.InvariantCultureIgnoreCase);
			foreach(string DepotFile in DepotFiles)
			{
				SortedFiles.Add(DepotFile, false);
			}
			foreach(string StrippedFile in AllStrippedFiles)
			{
				SortedFiles.Add(StrippedFile, true);
			}

			// Write the list to the log
			CommandUtils.Log("Files to be included in Rocket build:");
			foreach(KeyValuePair<string, bool> SortedFile in SortedFiles)
			{
				CommandUtils.Log("  {0}{1}", SortedFile.Key, SortedFile.Value? " (stripped)" : "");
			}
		}

		static void AddRuleForBuildProducts(FileFilter Filter, GUBP bp, string NodeName, FileFilterType Type)
		{
			GUBP.GUBPNode Node = bp.FindNode(NodeName);
			if(Node == null)
			{
				throw new AutomationException("Couldn't find node '{0}'", NodeName);
			}
			Filter.AddRuleForFiles(Node.BuildProducts, CommandUtils.CmdEnv.LocalRoot, Type);
		}

		static void UnzipAndAddRuleForHeaders(string ZipFileName, FileFilter Filter, FileFilterType Type)
		{
			IEnumerable<string> FileNames = CommandUtils.UnzipFiles(ZipFileName, CommandUtils.CmdEnv.LocalRoot);
			Filter.AddRuleForFiles(FileNames, CommandUtils.CmdEnv.LocalRoot, FileFilterType.Include);
		}

		public static void WriteManifest(string FileName, IEnumerable<string> Files)
		{
			CommandUtils.CreateDirectory(Path.GetDirectoryName(FileName));
			CommandUtils.WriteAllLines(FileName, Files.ToArray());
		}
	}

	public class GatherRocketNode : GUBP.HostPlatformNode
	{
		public readonly string OutputDir;
		public List<UnrealTargetPlatform> CodeTargetPlatforms;

		public GatherRocketNode(UnrealTargetPlatform HostPlatform, List<UnrealTargetPlatform> InCodeTargetPlatforms, string InOutputDir) : base(HostPlatform)
		{
			OutputDir = InOutputDir;
			CodeTargetPlatforms = new List<UnrealTargetPlatform>(InCodeTargetPlatforms);

			AddDependency(FilterRocketNode.StaticGetFullName(HostPlatform));
			AddDependency(BuildDerivedDataCacheNode.StaticGetFullName(HostPlatform));

			AgentSharingGroup = "RocketGroup" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public static string StaticGetFullName(UnrealTargetPlatform HostPlatform)
		{
			return "GatherRocket" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			CommandUtils.DeleteDirectoryContents(OutputDir);

			// Extract the editor headers
			CommandUtils.UnzipFiles(GUBP.RootEditorNode.StaticGetArchivedHeadersPath(HostPlatform), CommandUtils.CmdEnv.LocalRoot);

			// Extract all the headers for code target platforms
			foreach(UnrealTargetPlatform CodeTargetPlatform in CodeTargetPlatforms)
			{
				UnrealTargetPlatform SourceHostPlatform = RocketBuild.GetSourceHostPlatform(bp, HostPlatform, CodeTargetPlatform);
				string ZipFileName = GUBP.GamePlatformMonolithicsNode.StaticGetArchivedHeadersPath(SourceHostPlatform, bp.Branch.BaseEngineProject, CodeTargetPlatform);
				CommandUtils.UnzipFiles(ZipFileName, CommandUtils.CmdEnv.LocalRoot);
			}

			// Copy the depot files to the output directory
			FilterRocketNode FilterNode = (FilterRocketNode)bp.FindNode(FilterRocketNode.StaticGetFullName(HostPlatform));
			CopyManifestFilesToOutput(FilterNode.DepotManifestPath, CommandUtils.CmdEnv.LocalRoot, OutputDir);

			// Copy the stripped files to the output directory
			foreach(KeyValuePair<string, string> StrippedManifestPath in FilterNode.StrippedNodeManifestPaths)
			{
				StripRocketNode StripNode = (StripRocketNode)bp.FindNode(StrippedManifestPath.Key);
				CopyManifestFilesToOutput(StrippedManifestPath.Value, StripNode.StrippedDir, OutputDir);
			}

			// Copy the DDC to the output directory
			BuildDerivedDataCacheNode DerivedDataCacheNode = (BuildDerivedDataCacheNode)bp.FindNode(BuildDerivedDataCacheNode.StaticGetFullName(HostPlatform));
			CopyManifestFilesToOutput(DerivedDataCacheNode.SavedManifestPath, DerivedDataCacheNode.SavedDir, OutputDir);

			// Write the Rocket.txt file with the 
			string RocketFile = CommandUtils.CombinePaths(OutputDir, "Engine/Build/Rocket.txt");
			CommandUtils.WriteAllText(RocketFile, "-installedengine -rocket");

			// Create a dummy build product
			BuildProducts = new List<string>();
			SaveRecordOfSuccessAndAddToBuildProducts();
		}

		static void CopyManifestFilesToOutput(string ManifestPath, string InputDir, string OutputDir)
		{
			// Read the files from the manifest
			CommandUtils.Log("Reading manifest: '{0}'", ManifestPath);
			string[] Files = CommandUtils.ReadAllLines(ManifestPath).Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();

			// Create lists of source and target files
			CommandUtils.Log("Preparing file lists...");
			string[] SourceFiles = Files.Select(x => CommandUtils.CombinePaths(InputDir, x)).ToArray();
			string[] TargetFiles = Files.Select(x => CommandUtils.CombinePaths(OutputDir, x)).ToArray();

			// Copy everything
			CommandUtils.ThreadedCopyFiles(SourceFiles, TargetFiles);
		}
	}

	public class PublishRocketNode : GUBP.HostPlatformNode
	{
		string LocalDir;
		string PublishDir;

		public PublishRocketNode(UnrealTargetPlatform HostPlatform, string InLocalDir, string InPublishDir) : base(HostPlatform)
		{
			LocalDir = InLocalDir;
			PublishDir = InPublishDir;

			AddDependency(GatherRocketNode.StaticGetFullName(HostPlatform));

			AgentSharingGroup = "RocketGroup" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public static string StaticGetFullName(UnrealTargetPlatform HostPlatform)
		{
			return "PublishRocket" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			// Create a zip file containing the install
			string FullZipFileName = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "FullInstall" + StaticGetHostPlatformSuffix(HostPlatform) + ".zip");
			CommandUtils.Log("Creating {0}...", FullZipFileName);
			CommandUtils.ZipFiles(FullZipFileName, LocalDir, new FileFilter(FileFilterType.Include));

			// Create a filter for the files we need just to run the editor
			FileFilter EditorFilter = new FileFilter(FileFilterType.Include);
			EditorFilter.Exclude("/Engine/Binaries/...");
			EditorFilter.Include("/Engine/Binaries/DotNET/...");
			EditorFilter.Include("/Engine/Binaries/ThirdParty/...");
			EditorFilter.Include("/Engine/Binaries/" + HostPlatform.ToString() + "/...");
			EditorFilter.Exclude("/Engine/Binaries/.../*.lib");
			EditorFilter.Exclude("/Engine/Binaries/.../*.a");
			EditorFilter.Exclude("/Engine/Extras/...");
			EditorFilter.Exclude("/Engine/Source/.../Private/...");
			EditorFilter.Exclude("/FeaturePacks/...");
			EditorFilter.Exclude("/Samples/...");
			EditorFilter.Exclude("/Templates/...");
			EditorFilter.Exclude("*.pdb");

			// Create a zip file containing the editor install
			string EditorZipFileName = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "EditorInstall" + StaticGetHostPlatformSuffix(HostPlatform) + ".zip");
			CommandUtils.Log("Creating {0}...", EditorZipFileName);
			CommandUtils.ZipFiles(EditorZipFileName, LocalDir, EditorFilter);

			// Copy the files to their final location
			CommandUtils.Log("Copying files to {0}", PublishDir);
			CommandUtils.CopyFile(FullZipFileName, Path.Combine(PublishDir, Path.GetFileName(FullZipFileName)));
			CommandUtils.CopyFile(EditorZipFileName, Path.Combine(PublishDir, Path.GetFileName(EditorZipFileName)));
			CommandUtils.DeleteFile(FullZipFileName);
			CommandUtils.DeleteFile(EditorZipFileName);

			// Save a record of success
			BuildProducts = new List<string>();
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}

	public class PublishRocketSymbolsNode : GUBP.HostPlatformNode
	{
		string SymbolsOutputDir;

		public PublishRocketSymbolsNode(GUBP bp, UnrealTargetPlatform HostPlatform, IEnumerable<UnrealTargetPlatform> TargetPlatforms, string InSymbolsOutputDir) : base(HostPlatform)
		{
			SymbolsOutputDir = InSymbolsOutputDir;

			AddDependency(GUBP.WaitForSharedPromotionUserInput.StaticGetFullName(false));
			AddDependency(GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform));
			AddDependency(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
			AddDependency(GUBP.ToolsNode.StaticGetFullName(HostPlatform));

			foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				if(HostPlatform == RocketBuild.GetSourceHostPlatform(bp, HostPlatform, TargetPlatform))
				{
					bool bIsCodeTargetPlatform = RocketBuild.IsCodeTargetPlatform(HostPlatform, TargetPlatform);
					AddDependency(GUBP.GamePlatformMonolithicsNode.StaticGetFullName(HostPlatform, bp.Branch.BaseEngineProject, TargetPlatform, Precompiled: bIsCodeTargetPlatform));
				}
			}

			AgentSharingGroup = "RocketGroup" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public static string StaticGetFullName(UnrealTargetPlatform HostPlatform)
		{
			return "PublishRocketSymbols" + StaticGetHostPlatformSuffix(HostPlatform);
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			if(RocketBuild.ShouldDoSeriousThingsLikeP4CheckinAndPostToMCP())
			{
				// Make a lookup for all the known debug extensions, and filter all the dependency build products against that
				HashSet<string> DebugExtensions = new HashSet<string>(Platform.Platforms.Values.SelectMany(x => x.GetDebugFileExtentions()).Distinct().ToArray(), StringComparer.InvariantCultureIgnoreCase);
				foreach(string InputFileName in AllDependencyBuildProducts)
				{
					string Extension = Path.GetExtension(InputFileName);
					if(DebugExtensions.Contains(Extension) || Extension == ".exe" || Extension == ".dll") // Need all windows build products for crash reporter
					{
						string OutputFileName = CommandUtils.MakeRerootedFilePath(InputFileName, CommandUtils.CmdEnv.LocalRoot, SymbolsOutputDir);
						CommandUtils.CopyFile(InputFileName, OutputFileName);
					}
				}
			}

			// Add a dummy build product
			BuildProducts = new List<string>();
			SaveRecordOfSuccessAndAddToBuildProducts();
		}
	}

	public class BuildDerivedDataCacheNode : GUBP.HostPlatformNode
	{
		string TargetPlatforms;
		string[] ProjectNames;
		public readonly string SavedDir;
		public readonly string SavedManifestPath;

		public BuildDerivedDataCacheNode(UnrealTargetPlatform InHostPlatform, string InTargetPlatforms, string[] InProjectNames)
			: base(InHostPlatform)
		{
			TargetPlatforms = InTargetPlatforms;
			ProjectNames = InProjectNames;
			SavedDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Rocket", HostPlatform.ToString());
			SavedManifestPath = CommandUtils.CombinePaths(SavedDir, "DerivedDataCacheManifest.txt");

			AddDependency(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
			AddDependency(GUBP.ToolsNode.StaticGetFullName(HostPlatform));
		}

		public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
		{
            return "BuildDerivedDataCache" + StaticGetHostPlatformSuffix(InHostPlatform);
		}

		public override int CISFrequencyQuantumShift(GUBP bp)
		{
			return base.CISFrequencyQuantumShift(bp) + 2;
		}

		public override string GetFullName()
		{
			return StaticGetFullName(HostPlatform);
		}

		public override void DoBuild(GUBP bp)
		{
			CommandUtils.CreateDirectory(SavedDir);

			BuildProducts = new List<string>();

			List<string> ManifestFiles = new List<string>();
			if(!bp.ParseParam("NoDDC"))
			{
				// Find all the projects we're interested in
				List<BranchInfo.BranchUProject> Projects = new List<BranchInfo.BranchUProject>();
				foreach(string ProjectName in ProjectNames)
				{
					BranchInfo.BranchUProject Project = bp.Branch.FindGameChecked(ProjectName);
					if(!Project.Properties.bIsCodeBasedProject)
					{
						Projects.Add(Project);
					}
				}

				// Filter out the files we need to build DDC. Removing confidential folders can affect DDC keys, so we want to be sure that we're making DDC with a build that can use it.
				FileFilter Filter = new FileFilter(FileFilterType.Exclude);
				Filter.AddRuleForFiles(AllDependencyBuildProducts, CommandUtils.CmdEnv.LocalRoot, FileFilterType.Include);
				Filter.ReadRulesFromFile(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "InstalledEngineFilters.ini"), "CopyEditor", HostPlatform.ToString());
				Filter.Exclude("/Engine/Build/...");
				Filter.Exclude("/Engine/Extras/...");
				Filter.Exclude("/Engine/DerivedDataCache/...");
				Filter.Exclude("/Samples/...");
				Filter.Exclude("/Templates/...");
				Filter.Exclude(".../Source/...");
				Filter.Exclude(".../Intermediate/...");
				Filter.ExcludeConfidentialPlatforms();
				Filter.ExcludeConfidentialFolders();
				Filter.Include("/Engine/Build/NotForLicensees/EpicInternal.txt");
				Filter.Include("/Engine/Binaries/.../*DDCUtils*"); // Make sure we can use the shared DDC!

				// Copy everything to a temporary directory
				string TempDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "LocalBuilds", "RocketDDC", CommandUtils.GetGenericPlatformName(HostPlatform));
				CommandUtils.DeleteDirectoryContents(TempDir);
				CommandUtils.ThreadedCopyFiles(CommandUtils.CmdEnv.LocalRoot, TempDir, Filter, true);

				// Get paths to everything within the temporary directory
				string EditorExe = CommandUtils.GetEditorCommandletExe(TempDir, HostPlatform);
				string RelativePakPath = "Engine/DerivedDataCache/Compressed.ddp";
				string OutputPakFile = CommandUtils.CombinePaths(TempDir, RelativePakPath);
				string OutputCsvFile = Path.ChangeExtension(OutputPakFile, ".csv");

				// Generate DDC for all the non-code projects. We don't necessarily have editor DLLs for the code projects, but they should be the same as their blueprint counterparts.
				List<string> ProjectPakFiles = new List<string>();
				foreach(BranchInfo.BranchUProject Project in Projects)
				{
					CommandUtils.Log("Generating DDC data for {0} on {1}", Project.GameName, TargetPlatforms);
					CommandUtils.DDCCommandlet(Project.FilePath, EditorExe, null, TargetPlatforms, "-fill -DDC=CreateInstalledEnginePak -ProjectOnly");

					string ProjectPakFile = CommandUtils.CombinePaths(Path.GetDirectoryName(OutputPakFile), String.Format("Compressed-{0}.ddp", Project.GameName));
					CommandUtils.DeleteFile(ProjectPakFile);
					CommandUtils.RenameFile(OutputPakFile, ProjectPakFile);

					string ProjectCsvFile = Path.ChangeExtension(ProjectPakFile, ".csv");
					CommandUtils.DeleteFile(ProjectCsvFile);
					CommandUtils.RenameFile(OutputCsvFile, ProjectCsvFile);

					ProjectPakFiles.Add(Path.GetFileName(ProjectPakFile));
				}

				// Generate DDC for the editor, and merge all the other PAK files in
				CommandUtils.Log("Generating DDC data for engine content on {0}", TargetPlatforms);
				CommandUtils.DDCCommandlet(null, EditorExe, null, TargetPlatforms, "-fill -DDC=CreateInstalledEnginePak " + CommandUtils.MakePathSafeToUseWithCommandLine("-MergePaks=" + String.Join("+", ProjectPakFiles)));

				// Copy the DDP file to the output path
				string SavedPakFile = CommandUtils.CombinePaths(SavedDir, RelativePakPath);
				CommandUtils.CopyFile(OutputPakFile, SavedPakFile);
				BuildProducts.Add(SavedPakFile);

				// Add the pak file to the list of files to copy
				ManifestFiles.Add(RelativePakPath);
			}
			CommandUtils.WriteAllLines(SavedManifestPath, ManifestFiles.ToArray());
			BuildProducts.Add(SavedManifestPath);

			SaveRecordOfSuccessAndAddToBuildProducts();
		}

		public override float Priority()
		{
			return base.Priority() + 55.0f;
		}
	}
}
