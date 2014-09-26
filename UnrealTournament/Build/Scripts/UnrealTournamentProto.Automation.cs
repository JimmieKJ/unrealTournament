// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using System.Net;
using System.Reflection;
using System.Web.Script.Serialization;
using EpicGames.MCP.Automation;
using EpicGames.MCP.Config;

public class UnrealTournamentBuild
{
	// Use old-style UAT version for UnrealTournament
	// TODO this should probably use the new Engine Version stuff.
	static string CreateBuildVersion()
	{
		string P4Change = "UnknownCL";
		string P4Branch = "UnknownBranch";
		if (CommandUtils.P4Enabled)
		{
			P4Change = CommandUtils.P4Env.ChangelistString;
			P4Branch = CommandUtils.P4Env.BuildRootEscaped;
		}
		return P4Branch + "-CL-" + P4Change;
	}


	public static UnrealTournamentAppName BranchNameToAppName(string BranchName)
	{
		UnrealTournamentAppName AppName = UnrealTournamentAppName.UnrealTournamentDev;

		if (BranchName.Equals("UE4-UT"))
		{
			AppName = UnrealTournamentAppName.UnrealTournamentDev;
		}
		else if (BranchName.Contains("Release"))
		{
			AppName = UnrealTournamentAppName.UnrealTournamentTest;
		}

        /*
        // If a release has been promoted to public then continue pushing builds to public
        List<string> PublicBranches = new List<string>(new string[]
        {
            "//depot/UE4-UT-Release-0.4"
        });

        if (PublicBranches.Contains(BranchName))
        {
            AppName = UnrealTournamentAppName.UnrealTournament;
        }
        */
		
		return AppName;
	}

	/// <summary>
	/// Enum that defines the MCP backend-compatible platform
	/// </summary>
	public enum UnrealTournamentAppName
	{
		/// Public app
		UnrealTournament,

		/// Public test app
		UnrealTournamentTest,

		/// Dev app
		UnrealTournamentDev,
	}


	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentAppName AppName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "");
	}

	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "");
	}

	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, UnrealTargetPlatform InPlatform, string BranchName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "");
	}

	// Get a basic StagingInfo based on buildversion of command currently running
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, UnrealTargetPlatform InPlatform, string BranchName)
	{
		return GetUTBuildPatchToolStagingInfo(InOwnerCommand, CreateBuildVersion(), InPlatform, BranchName);
	}

	public static string GetArchiveDir()
	{
		return CommandUtils.CombinePaths(BuildPatchToolStagingInfo.GetBuildRootPath(), "UnrealTournament", CreateBuildVersion());
	}
}

[RequireP4]
class UnrealTournamentProto_BasicBuild : BuildCommand
{
	static public string NetworkStage = @"P:\Builds\UnrealTournament";

    static List<UnrealTargetPlatform> GetClientTargetPlatforms(BuildCommand Cmd)
    {
        List<UnrealTargetPlatform> ClientPlatforms = new List<UnrealTargetPlatform>();

        ClientPlatforms.Add(UnrealTargetPlatform.Win64);
        ClientPlatforms.Add(UnrealTargetPlatform.Win32);
        ClientPlatforms.Add(UnrealTargetPlatform.Mac);
        if (!Cmd.ParseParam("nolinux"))
        {
            ClientPlatforms.Add(UnrealTargetPlatform.Linux);
        }

        return ClientPlatforms;
    }

    static List<UnrealTargetPlatform> GetServerTargetPlatforms(BuildCommand Cmd)
    {
        List<UnrealTargetPlatform> ServerPlatforms = new List<UnrealTargetPlatform>();

        if (!Cmd.ParseParam("mac"))
        {
            ServerPlatforms.Add(UnrealTargetPlatform.Win64);
            if (!Cmd.ParseParam("nolinux"))
            {
                ServerPlatforms.Add(UnrealTargetPlatform.Linux);
            }
        }

        return ServerPlatforms;
    }

    static public ProjectParams GetParams(BuildCommand Cmd)
	{
        string P4Change = "Unknown";
        string P4Branch = "Unknown";
        if (P4Enabled)
        {
            P4Change = P4Env.ChangelistString;
            P4Branch = P4Env.BuildRootEscaped;
        }
				
		var Params = new ProjectParams
		(
			// Shared
            RawProjectPath: CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject"),

			// Build
			EditorTargets: new ParamList<string>("UnrealTournamentEditor", "BuildPatchTool"),
			ClientCookedTargets: new ParamList<string>("UnrealTournament"),
			ServerCookedTargets: new ParamList<string>("UnrealTournamentServer"),

            ClientConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development },
            ServerConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development },
            ClientTargetPlatforms: GetClientTargetPlatforms(Cmd),
            ServerTargetPlatforms: GetServerTargetPlatforms(Cmd),
            Build: !Cmd.ParseParam("skipbuild"),
            Cook: true,
            SkipCook: Cmd.ParseParam("skipcook"),
            Clean: !Cmd.ParseParam("NoClean") && !Cmd.ParseParam("skipcook") && !Cmd.ParseParam("skippak") && !Cmd.ParseParam("skipstage") && !Cmd.ParseParam("skipbuild"),
            DedicatedServer: true,
            Pak: true,
            SkipPak: Cmd.ParseParam("skippak"),
            NoXGE: Cmd.ParseParam("NoXGE"),
            Stage: true,
            SkipStage: Cmd.ParseParam("skipstage"),
            NoDebugInfo: Cmd.ParseParam("NoDebugInfo"),
            CrashReporter: !Cmd.ParseParam("mac"), // @todo Mac: change to true when Mac implementation is ready
            StageNonMonolithic: true,
			// if we are running, we assume this is a local test and don't chunk
			Run: Cmd.ParseParam("Run"),
            StageDirectoryParam: UnrealTournamentBuild.GetArchiveDir()
		);
		Params.ValidateAndLog();
		return Params;
	}
	public override void ExecuteBuild()
	{
		Log("************************* UnrealTournamentProto_BasicBuild");

		var Params = GetParams(this);

		int WorkingCL = -1;
		if (P4Enabled && AllowSubmit)
		{
			WorkingCL = P4.CreateChange(P4Env.Client, String.Format("UnrealTournamentBuild build built from changelist {0}", P4Env.Changelist));
			Log("Build from {0}    Working in {1}", P4Env.Changelist, WorkingCL);
		}

		Project.Build(this, Params, WorkingCL);
		Project.Cook(Params);
		Project.CopyBuildToStagingDirectory(Params);
		PrintRunTime();
		Project.Deploy(Params);
		Project.Run(Params);
        
		if (WorkingCL > 0)
        {
            bool Pending;
            if (P4.ChangeFiles(WorkingCL, out Pending).Count == 0)
            {
                Log("************************* No files to submit.");
                P4.DeleteChange(WorkingCL);
            }
            else
            {
                // Check everything in!
                int SubmittedCL;
                P4.Submit(WorkingCL, out SubmittedCL, true, true);
            }

			P4.MakeDownstreamLabel(P4Env, "UnrealTournamentBuild");
		}
        
        if (ParseParam("Chunk") && !ParseParam("mac"))
		{
		    Chunk(Params);
		}
	}

    public void Chunk(ProjectParams Params)
    {
        // verify the files we need exist first
        string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "Win64", "WindowsNoEditor");
        string RawImageManifest = CombinePaths(RawImagePath, "Manifest_NonUFSFiles.txt");

        if (!FileExists(RawImageManifest))
        {
            throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
        }
		
        var BranchName = "";
        if (CommandUtils.P4Enabled)
        {
            BranchName = CommandUtils.P4Env.BuildRootP4;
        }
		
		var StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Win64, BranchName);

        // run the patch tool
        BuildPatchToolBase.Get().Execute(
        new BuildPatchToolBase.PatchGenerationOptions
        {
            StagingInfo = StagingInfo,
            BuildRoot = RawImagePath,
            FileIgnoreList = CommandUtils.CombinePaths(RawImagePath, "Manifest_DebugFiles.txt"),
            AppLaunchCmd = @".\Engine\Binaries\Win64\UE4.exe",
            AppLaunchCmdArgs = "UnrealTournament",
            AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
        });

		// post the Windows build to build info service on gamedev
		string McpConfigName = "MainGameDevNet";
		CommandUtils.Log("Posting UnrealTournament for Windows to MCP.");
		BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
		CommandUtils.Log("Labeling new build as Latest in MCP.");
		string LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Latest", MCPPlatform.Windows);
		BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
		// For backwards compatibility, also label as Production-Latest
		LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Production-Latest", MCPPlatform.Windows);
		BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);


        // verify the files we need exist first
        string RawImagePathMac = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "MacNoEditor");
        string RawImageManifestMac = CombinePaths(RawImagePathMac, "Manifest_NonUFSFiles.txt");

        if (!FileExists(RawImageManifestMac))
        {
            throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifestMac);
        }

		StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Mac, BranchName);
		
        // run the patch tool
        BuildPatchToolBase.Get().Execute(
        new BuildPatchToolBase.PatchGenerationOptions
        {
			StagingInfo = StagingInfo,
            BuildRoot = RawImagePathMac,
            FileIgnoreList = CommandUtils.CombinePaths(RawImagePathMac, "Manifest_DebugFiles.txt"),
            AppLaunchCmd = "./UnrealTournament/Binaries/Mac/UnrealTournament.app",
            AppLaunchCmdArgs = "",
            AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
        });

		// post the Mac build to build info service on gamedev
		CommandUtils.Log("Posting Unreal Tournament for Mac to MCP.");
		BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
		CommandUtils.Log("Labeling new build as Latest in MCP.");
		LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Latest", MCPPlatform.Mac);
		BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
		// For backwards compatibility, also label as Production-Latest
		LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Production-Latest", MCPPlatform.Mac);
		BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);

        PrintRunTime();
    }
}

class UnrealTournamentBuildProcess : GUBP.GUBPNodeAdder
{

    public class WaitForUnrealTournamentBuildUserInputNode : GUBP.WaitForUserInput
    {
        BranchInfo.BranchUProject GameProj;
        public WaitForUnrealTournamentBuildUserInputNode(GUBP bp, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform HostPlatform)
        {
            GameProj = InGameProj;

            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(UnrealTargetPlatform.Mac, GameProj, UnrealTargetPlatform.Mac));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win64));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win32));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Linux));

            var Chunker = bp.Branch.FindProgram("BuildPatchTool");
            AddDependency(GUBP.EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
            AddDependency(GUBP.EditorAndToolsNode.StaticGetFullName(HostPlatform));
            AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(HostPlatform, Chunker));
        }
        public override string GetTriggerDescText()
        {
            return "UT is ready to begin a formal build.";
        }
        public override string GetTriggerActionText()
        {
            return "Make a formal UT build.";
        }
        public override string GetTriggerStateName()
        {
            return GetFullName();
        }
        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj)
        {
            return InGameProj.GameName + "_WaitToMakeBuild";
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
    }

    public class UnrealTournamentBuildNode : GUBP.GUBPNode
    {
        BranchInfo.BranchUProject GameProj;

        public UnrealTournamentBuildNode(GUBP bp, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform HostPlatform)
        {
            GameProj = InGameProj;

            AddDependency(WaitForUnrealTournamentBuildUserInputNode.StaticGetFullName(GameProj));
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj)
        {
            return InGameProj.GameName + "_MakeBuild";
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj);
        }
        public override string GameNameIfAnyForTempStorage()
        {
            return GameProj.GameName;
        }
        public override bool SendSuccessEmail()
        {
            return true;
        }

        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
            string LogFile = "Just a record of success.";
            if (GUBP.bPreflightBuild)
            {
                CommandUtils.Log("Things like a real UT build is likely to cause confusion if done in a preflight build, so we are skipping it.");
            }
            else
            {
                  LogFile = CommandUtils.RunUAT(CommandUtils.CmdEnv, "UnrealTournamentProto_BasicBuild -SkipBuild -SkipCook -Chunk");
            }
            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));
        }
    }

    public override void AddNodes(GUBP bp, UnrealTargetPlatform InHostPlatform)
    {
        //if (!bp.BranchOptions.ExcludeNodes.Contains("UnrealTournament"))
        {            
            var GameProj = bp.Branch.FindGame("UnrealTournament");
            if (GameProj != null && !GUBP.bBuildRocket)
            {
                CommandUtils.Log("*** Adding UT-specific nodes to the GUBP");
                if (InHostPlatform == UnrealTargetPlatform.Win64)
                {
                    bp.AddNode(new WaitForUnrealTournamentBuildUserInputNode(bp, GameProj, InHostPlatform));
                    bp.AddNode(new UnrealTournamentBuildNode(bp, GameProj, InHostPlatform));
                }
            }
        }
    }

}


// Performs the appropriate labeling and other actions for a given promotion
// See the EC job for an up-to-date description of all parameters
[Help("PromotionStep", "New label for build.  Dropdown with Latest, Testing, Approved, Staged, Live")]
[Help("CustomLabel", "Custom label to apply to this build.  Requires a custom label step be selected for PromotionStep")]
[Help("BuildVersion", "Non-platform-specific BuildVersion to promote.")]
[Help("Platforms", "Optional.  Comma-separated list of platforms to promote.  Default is all platforms.")]
[Help("SkipLabeling", "Optional.  Perform the promotion step but don't apply the new label to the build.")]
[Help("OnlyLabel", "Optional.  Perform the labeling step but don't perform any additional promotion actions.")]
[Help("TestLivePromotion", "Optional.  Use fake production labels and don't perform any actions that would go out to the public, eg. installer redirect.")]
// NOTE: PROMOTION JOB IS ONLY EVER RUN OUT OF UE4-FORTNITE, REGARDLESS OF WHICH BRANCH'S BUILD IS BEING PROMOTED
class UnrealTournament_PromoteBuild : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* UnrealTournament_PromoteBuild");
		string BuildVersion = ParseParamValue("BuildVersion");
		if (String.IsNullOrEmpty(BuildVersion))
		{
			throw new AutomationException("BuildVersion is a required parameter");
		}

        UnrealTournamentBuild.UnrealTournamentAppName FromApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev;
        UnrealTournamentBuild.UnrealTournamentAppName ToApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev;
		{
			string FromAppName = ParseParamValue("FromAppName");
			if (String.IsNullOrEmpty(FromAppName))
			{
				throw new AutomationException("FromAppName is a required parameter");
			}
			Enum.TryParse(FromAppName, out FromApp);

			// evaluate ToApp param if promoting cross app
			bool bShouldPromoteCrossApp = false;
			Boolean.TryParse(ParseParamValue("PromoteCrossApp"), out bShouldPromoteCrossApp);
			if (!bShouldPromoteCrossApp)
			{
				// normal case is promoting within same app
				ToApp = FromApp;
			}
			else
			{
				string ToAppName = ParseParamValue("ToAppName");
				if (String.IsNullOrEmpty(ToAppName))
				{
					throw new AutomationException("ToAppName is a required parameter when PromoteCrossApp is checked");
				}
				if (FromApp == ToApp)
				{
					throw new AutomationException("ToAppName is the same as FromAppName.  This doesn't make sense with PromoteCrossApp checked.");
				}
				else
				{
					throw new AutomationException("Promoting from FromApp: {0} to ToApp: {1} has not been implemented yet.", FromApp.ToString(), ToApp.ToString());
				}
			}
		}

		// Pull promotion step parameter
		string DestinationLabel = ParseParamValue("PromotionStep");
		bool bUseProdCustomLabel = false;
		// Check for custom labels, and scope out extra temp vars
		{
			bool bUseDevCustomLabel = (DestinationLabel == "DevCustomLabel");
			bUseProdCustomLabel = (DestinationLabel == "ProdCustomLabel");
			string CustomLabel = ParseParamValue("CustomLabel");
			if (!bUseDevCustomLabel && !bUseProdCustomLabel && !String.IsNullOrEmpty(CustomLabel))
			{
				throw new AutomationException("Custom label was filled out, but promotion step for custom label was not selected.");
			}
			if ((bUseDevCustomLabel || bUseProdCustomLabel) && String.IsNullOrEmpty(CustomLabel))
			{
				throw new AutomationException("Custom label promotion step was selected, but custom label textbox was empty.");
			}
			if (bUseDevCustomLabel || bUseProdCustomLabel)
			{
				DestinationLabel = CustomLabel;
			}
		}

		// OPTIONAL PARAMETERS
		string PlatformParam = ParseParamValue("Platforms");
		if (String.IsNullOrEmpty(PlatformParam))
		{
			PlatformParam = "Windows";
		}
		List<MCPPlatform> Platforms = PlatformParam.Split(',').Select(PlatformStr => (MCPPlatform)Enum.Parse(typeof(MCPPlatform), PlatformStr)).ToList<MCPPlatform>();
		bool SkipLabeling = ParseParam("SkipLabeling");
		bool OnlyLabel = ParseParam("OnlyLabel");
		bool TestLivePromotion = ParseParam("TestLivePromotion");
		if (TestLivePromotion == true && DestinationLabel.Equals("Live"))
		{
			throw new AutomationException("You attempted to promote to Live without toggling off TestLivePromotion in EC.  Did you mean to promote to Live?");
		}

		// Map which labels are GameDev-only vs. also being in Prod
		List<string> ProdLabels = new List<string>();
		ProdLabels.Add("Staged");
		ProdLabels.Add("InternalTest");
		ProdLabels.Add("PatchTestProd");
		ProdLabels.Add("QFE1");
		ProdLabels.Add("QFE2");
		ProdLabels.Add("QFE3");
		ProdLabels.Add("Rollback");
		ProdLabels.Add("Live");

		// Strings for dev and prod mcp BuildInfo list
		string GameDevMcpConfigString = "MainGameDevNet";
		string ProdMcpConfigString = "ProdNet";
		string ProdAndGameDevMcpConfigString = ProdMcpConfigString + "," + GameDevMcpConfigString;

		// Verify build meets prior state criteria for this promotion
		// If this label will go to Prod, then make sure the build manifests are all staged to the Prod CDN already
		if (DestinationLabel != "Staged" && (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel))
		{
			// Look for each build's manifest
			foreach (var Platform in Platforms)
			{
				BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);

				// Check for the manifest on the prod CDN
				Log("Verifying manifest for prod promotion of Fortnite {0} {1} was already staged to the Prod CDN", BuildVersion, Platform);
				bool bWasManifestFound = BuildInfoPublisherBase.Get().IsManifestOnProductionCDN(StagingInfo);
				if (!bWasManifestFound)
				{
					string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
					throw new AutomationException("Promotion to Prod requires the build first be staged to the Prod CDN. Manifest {0} not found for promotion to label {1}"
						, StagingInfo.ManifestFilename
						, DestinationLabelWithPlatform);
				}
			}
		}

		// Execute any additional logic required for each promotion
		if (OnlyLabel == true)
		{
			Log("Not performing promotion logic due to -OnlyLabel parameter");
		}
		else
		{
			Log("Performing build promotion actions for promoting to label {0}.", DestinationLabel);
			if (DestinationLabel == "Staged")
			{
				// Copy chunks and installer to production CDN
				foreach (var Platform in Platforms)
				{
                    BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
					// Publish staging info up to production BuildInfo service
					{
						CommandUtils.Log("Posting portal to MCP.");
						BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, ProdMcpConfigString);
					}
					// Copy chunks to production CDN
					{
						Log("Promoting chunks to production CDN");
						BuildInfoPublisherBase.Get().CopyChunksToProductionCDN(StagingInfo);
						Log("DONE Promoting chunks to production CDN");
					}
				}
			}
			else if (DestinationLabel == "Live")
			{
				// Apply rollback label to the build currently labeled Live
				foreach (var Platform in Platforms)
				{
                    BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
					string LiveLabelWithPlatform = "Live" + "-" + Platform;
					// Request Live-[platform] label for this app (actually Production-[platform] until deprecated)
					string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(StagingInfo.AppName, LiveLabelWithPlatform, ProdMcpConfigString);
					if (String.IsNullOrEmpty(LiveBuildVersionString))
					{
						Log("No current Live build found for {0}, continuing without applying a Rollback label.", LiveLabelWithPlatform);
					}
					else if (LiveBuildVersionString.EndsWith("-" + Platform))
					{
						Log("Identified current Live build as {0}", LiveBuildVersionString);
						// Take off platform so it can fit in the FEngineVersion struct
						string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
						if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
						{
							// Label the old Live build as "Rollback"
							LabelBuildForBackwardsCompat(LiveBuildVersionStringWithoutPlatform, "Rollback", new List<MCPPlatform>() { Platform }, ProdAndGameDevMcpConfigString, FromApp);
						}
						else
						{
							Log("Would relabel current Live {0} build as {1} again.  Not applying a Rollback label to avoid losing track of current Rollback build.", StagingInfo.AppName, LiveLabelWithPlatform);
						}
					}
					else
					{
						throw new AutomationException("Current live buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
					}
				}
			}
		}

		// Apply the label
		if (SkipLabeling == true)
		{
			Log("Not labeling build due to -SkipLabeling parameter");
		}
		else
		{
			Log("Labeling build {0} with label {1} across all platforms", BuildVersion, DestinationLabel);

			// For non-prod builds, do gamedev only
			string LabelInMcpConfigs = GameDevMcpConfigString;
			if (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel)
			{
				// For prod labels, do both BI services and also dual-label with the entitlement prefix
				LabelInMcpConfigs = ProdAndGameDevMcpConfigString;
			}
			LabelBuildForBackwardsCompat(BuildVersion, DestinationLabel, Platforms, LabelInMcpConfigs, FromApp);
		}

        Log("************************* UnrealTournament_PromoteBuild completed");
	}


    private void LabelBuildForBackwardsCompat(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentAppName FromApp)
	{
		// Label it normally
		LabelBuild(BuildVersion, DestinationLabel, Platforms, McpConfigNames, FromApp);

		// Apply the label again with entitlement prefix for backwards-compat until this can be deprecated
		string Label = "Production-" + DestinationLabel;
		// Don't do backwards compat labeling until/unless we determine this is still needed
		//LabelBuild(BuildVersion, Label, Platforms, McpConfigNames);

		// If the label is Live, also apply the empty label for backwards-compat until this can be deprecated in favor of "Live"
		if (DestinationLabel.Equals("Live"))
		{
			Label = "Production";
			LabelBuild(BuildVersion, Label, Platforms, McpConfigNames, FromApp);
		}
	}

	/// <summary>
	/// Apply the requested label to the requested build in the BuildInfo backend for the requested MCP environment
	/// Repeat for each passed platform, adding the platform to the end of the label that is applied
	/// </summary>
	/// <param name="BuildVersion">Build version to label builds for, WITHOUT a platform string embedded.</param>
	/// <param name="DestinationLabel">Label, WITHOUT platform embedded, to apply</param>
	/// <param name="Platforms">Array of platform strings to post labels for</param>
	/// <param name="McpConfigNames">Which BuildInfo backends to label the build in.</param>
	/// <param name="FromApp">Which appname is associated with this build</param>
    private void LabelBuild(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentAppName FromApp)
	{
		foreach (string McpConfigName in McpConfigNames.Split(','))
		{
			foreach (var Platform in Platforms)
			{
                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
				string LabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
				BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelWithPlatform, McpConfigName);
			}
		}
	}
}
