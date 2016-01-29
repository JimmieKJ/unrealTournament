// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
	public static string CreateBuildVersion()
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
		return UnrealTournamentAppName.UnrealTournamentBuilds;
	}

	/// <summary>
	/// Enum that defines the MCP backend-compatible platform
	/// </summary>
	public enum UnrealTournamentAppName
	{
		// Dev and release branch builds source app
		UnrealTournamentBuilds,

		// Dev branch promotions
		UnrealTournamentDevTesting,
		//UnrealTournamentDevStage,  // no plans for UT yet
		//UnrealTournamentDevPlaytest,  // no plans for UT yet

		// Release branch promotions
		UnrealTournamentReleaseTesting,
		UnrealTournamentReleaseStage,
		UnrealTournamentPublicTest,

		/// Live branch promotions
		UnrealTournamentLiveTesting,
        UnrealTournamentLiveStage,
		/// Legacy Dev app, used as "Live", displays in Launcher as "UnrealTournament"
		UnrealTournamentDev,
		//UnrealTournamentLive, // Name we wish we could migrate to for live app
	}


    public static UnrealTournamentEditorAppName EditorBranchNameToAppName(string BranchName)
    {
        return UnrealTournamentEditorAppName.UnrealTournamentEditor;
    }

    /// <summary>
    /// Enum that defines the MCP backend-compatible platform
    /// </summary>
    public enum UnrealTournamentEditorAppName
    {
		// Dev and release branch builds
		UnrealTournamentEditorBuilds,

		// Dev branch promotions
		UnrealTournamentEditorDevTesting,
		//UnrealTournamentEditorDevStage,  // no plans for UT yet
		//UnrealTournamentEditorDevPlaytest,  // no plans for UT yet

		// Release branch promotions
		UnrealTournamentEditorReleaseTesting,
		UnrealTournamentEditorReleaseStage,
		UnrealTournamentEditorPublicTest,

		/// Live branch promotions
		UnrealTournamentEditorLiveTesting,
		UnrealTournamentEditorLiveStage,
		UnrealTournamentEditor // live app for the public
    }



	/// GAME VERSIONS OF BUILDPATCHTOOLSTAGINGINFOS ///

	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentAppName AppName, string ManifestFilename = null)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
	}
    
	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName, string ManifestFilename = null)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
	}
     
	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, UnrealTargetPlatform InPlatform, string BranchName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
	}

	// Get a basic StagingInfo based on buildversion of command currently running
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, UnrealTargetPlatform InPlatform, string BranchName)
	{
		return GetUTBuildPatchToolStagingInfo(InOwnerCommand, CreateBuildVersion(), InPlatform, BranchName);
	}



	/// EDITOR VERSIONS OF BUILDPATCHTOOLSTAGINGINFOS ///

	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentEditorAppName AppName, string ManifestFilename = null)
    {
        // Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
        return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
    }

    // Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName, string ManifestFilename = null)
    {
        // Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
        return new BuildPatchToolStagingInfo(InOwnerCommand, EditorBranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
    }

    // Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, UnrealTargetPlatform InPlatform, string BranchName)
    {
        // Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
        return new BuildPatchToolStagingInfo(InOwnerCommand, EditorBranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
    }

    // Get a basic StagingInfo based on buildversion of command currently running
    public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, UnrealTargetPlatform InPlatform, string BranchName)
    {
        return GetUTEditorBuildPatchToolStagingInfo(InOwnerCommand, CreateBuildVersion(), InPlatform, BranchName);
    }

	public static string GetArchiveDir()
	{
		return CommandUtils.CombinePaths(BuildPatchToolStagingInfo.GetBuildRootPath(), "UnrealTournament", CreateBuildVersion());
	}
}

[RequireP4]
class UnrealTournamentProto_ChunkBuild : BuildCommand
{
    public override void ExecuteBuild()
    {
        Log("************************* UnrealTournamentProto_ChunkBuild");
        
        string BranchName = "";
        if (CommandUtils.P4Enabled)
        {
            BranchName = CommandUtils.P4Env.BuildRootP4;
        }

        if (ParseParam("mac"))
        {
            {
                // verify the files we need exist first
                string RawImagePathMac = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "MacNoEditor");
                string RawImageManifestMac = CombinePaths(RawImagePathMac, "Mac_Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifestMac))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifestMac);
                }

                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Mac, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePathMac,
                    AppLaunchCmd = "./Engine/Binaries/Mac/UE4-Mac-Shipping.app",
                    AppLaunchCmdArgs = "UnrealTournament -opengl",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Mac build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting Unreal Tournament for Mac to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Live in MCP.");
                string LabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Live", MCPPlatform.Mac);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelName, McpConfigName);

				// Go ahead and post to Testing app in Launcher as well
				UnrealTournamentBuild.UnrealTournamentAppName TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting;
				if (BranchName.Contains("UT-Releases"))
				{
					TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting;
				}
				CommandUtils.Log("Also posting to app {0} based on branch {1} to automatically make a build available in gamedev launcher", TestingApp, BranchName);
				// Reuse the same staginginfo but to the old app (need to hardcode the manifest to match it being created with the new app)
				BuildPatchToolStagingInfo TestingAppStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, StagingInfo.BuildVersion, StagingInfo.Platform, TestingApp, StagingInfo.ManifestFilename);
				BuildInfoPublisherBase.Get().PostBuildInfo(TestingAppStagingInfo);
				BuildInfoPublisherBase.Get().LabelBuild(TestingAppStagingInfo, LabelName, McpConfigName);
			}
        }
        else
        {
            // GAME BUILD
            {
                // verify the files we need exist first
                string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "WindowsNoEditor");
                string RawImageManifest = CombinePaths(RawImagePath, "Win64_Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
                }

                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Win64, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePath,
                    FileIgnoreList = CommandUtils.CombinePaths(RawImagePath, "Win64_Manifest_DebugFiles.txt"),
                    AppLaunchCmd = @".\Engine\Binaries\Win64\UE4-Win64-Shipping.exe",
                    AppLaunchCmdArgs = "UnrealTournament",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Windows build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting UnrealTournament for Windows to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Live in MCP.");
                string LabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Live", MCPPlatform.Windows);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelName, McpConfigName);

				// Go ahead and post to Testing app in Launcher as well
				UnrealTournamentBuild.UnrealTournamentAppName TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting;
				if (BranchName.Contains("UT-Releases"))
				{
					TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting;
				}
				CommandUtils.Log("Also posting to app {0} based on branch {1} to automatically make a build available in gamedev launcher", TestingApp, BranchName);
				// Reuse the same staginginfo but to the old app (need to hardcode the manifest to match it being created with the new app)
				BuildPatchToolStagingInfo TestingAppStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, StagingInfo.BuildVersion, StagingInfo.Platform, TestingApp, StagingInfo.ManifestFilename);
				BuildInfoPublisherBase.Get().PostBuildInfo(TestingAppStagingInfo);
				BuildInfoPublisherBase.Get().LabelBuild(TestingAppStagingInfo, LabelName, McpConfigName);
			}
            
            // Win32 GAME BUILD
            {
                // verify the files we need exist first
                string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "WindowsNoEditor");
                string RawImageManifest = CombinePaths(RawImagePath, "Win32_Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
                }

                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Win32, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePath,
                    FileIgnoreList = CommandUtils.CombinePaths(RawImagePath, "Win32_Manifest_DebugFiles.txt"),
                    AppLaunchCmd = @".\Engine\Binaries\Win32\UE4-Win32-Shipping.exe",
                    AppLaunchCmdArgs = "UnrealTournament",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Windows build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting UnrealTournament for Windows to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Live in MCP.");
                string LabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Live", MCPPlatform.Win32);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelName, McpConfigName);

				// Go ahead and post to Testing app in Launcher as well
				UnrealTournamentBuild.UnrealTournamentAppName TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting;
				if (BranchName.Contains("UT-Releases"))
				{
					TestingApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting;
				}
				CommandUtils.Log("Also posting to app {0} based on branch {1} to automatically make a build available in gamedev launcher", TestingApp, BranchName);
				// Reuse the same staginginfo but to the old app (need to hardcode the manifest to match it being created with the new app)
				BuildPatchToolStagingInfo TestingAppStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, StagingInfo.BuildVersion, StagingInfo.Platform, TestingApp, StagingInfo.ManifestFilename);
				BuildInfoPublisherBase.Get().PostBuildInfo(TestingAppStagingInfo);
				BuildInfoPublisherBase.Get().LabelBuild(TestingAppStagingInfo, LabelName, McpConfigName);
			}
        }

        PrintRunTime();
    }
}

[RequireP4]
class UnrealTournamentProto_BasicBuild : BuildCommand
{
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

        ProjectParams Params = new ProjectParams
		(
			// Shared
            RawProjectPath: new FileReference(CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject")),

			// Build
			EditorTargets: new ParamList<string>("BuildPatchTool"),
			ClientCookedTargets: new ParamList<string>("UnrealTournament"),
			ServerCookedTargets: new ParamList<string>("UnrealTournamentServer"),

            ClientConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Shipping, UnrealTargetConfiguration.Test },
            ServerConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Shipping },
            ClientTargetPlatforms: GetClientTargetPlatforms(Cmd),
            ServerTargetPlatforms: GetServerTargetPlatforms(Cmd),
            Build: !Cmd.ParseParam("skipbuild"),
            Cook: true,
            CulturesToCook: new ParamList<string>("en"),
            InternationalizationPreset: "English",
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
            CreateReleaseVersion: "UTVersion0",
            UnversionedCookedContent: true,
			// if we are running, we assume this is a local test and don't chunk
			Run: Cmd.ParseParam("Run"),
            StageDirectoryParam: UnrealTournamentBuild.GetArchiveDir()
		);
		Params.ValidateAndLog();
		return Params;
	}

    public void CopyAssetRegistryFilesFromSavedCookedToReleases(ProjectParams Params)
    {
        if (P4Enabled && !String.IsNullOrEmpty(Params.CreateReleaseVersion))
        {
            Log("************************* Copying AssetRegistry.bin files from Saved/Cooked to Releases");
            string ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);
            string SavedPath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Saved", "Cooked");

            string[] Platforms = new string[] { "WindowsNoEditor", "MacNoEditor", "LinuxServer", "WindowsServer", "LinuxNoEditor" };

            foreach (string Platform in Platforms)
            {
                string Filename = CombinePaths(ReleasePath, Platform, "AssetRegistry.bin");
                CommandUtils.CopyFile_NoExceptions(CombinePaths(SavedPath, Platform, "Releases", Params.CreateReleaseVersion, "AssetRegistry.bin"), Filename);
            }
        }
    }

    public void SubmitAssetRegistryFilesToPerforce(ProjectParams Params)
    {
        // Submit release version assetregistry.bin
        if (P4Enabled && !String.IsNullOrEmpty(Params.CreateReleaseVersion))
        {
            Log("************************* Submitting AssetRegistry.bin files");
            int AssetRegCL = P4.CreateChange(P4Env.Client, String.Format("UnrealTournamentBuild AssetRegistry build built from changelist {0}", P4Env.Changelist));
            if (AssetRegCL > 0)
            {
                string ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);

                string[] Platforms = new string[] { "WindowsNoEditor", "MacNoEditor", "LinuxServer", "WindowsServer", "LinuxNoEditor" };

                foreach (string Platform in Platforms)
                {
                    string Filename = CombinePaths(ReleasePath, Platform, "AssetRegistry.bin");

                    P4.Sync("-f -k " + Filename + "#head"); // sync the file without overwriting local one

                    if (!FileExists(Filename))
                    {
                        throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", Filename);
                    }

                    P4.ReconcileNoDeletes(AssetRegCL, Filename);
                }
            }
            bool Pending;
            if (P4.ChangeFiles(AssetRegCL, out Pending).Count == 0)
            {
                Log("************************* No files to submit.");
                P4.DeleteChange(AssetRegCL);
            }
            else
            {
                int SubmittedCL;
                P4.Submit(AssetRegCL, out SubmittedCL, true, true);
            }
        }
    }

	public override void ExecuteBuild()
	{
		Log("************************* UnrealTournamentProto_BasicBuild");

		ProjectParams Params = GetParams(this);

		int WorkingCL = -1;
		if (P4Enabled && AllowSubmit)
		{
			WorkingCL = P4.CreateChange(P4Env.Client, String.Format("UnrealTournamentBuild build built from changelist {0}", P4Env.Changelist));
			Log("Build from {0}    Working in {1}", P4Env.Changelist, WorkingCL);
		}

		Project.Build(this, Params, WorkingCL);
		Project.Cook(Params);
		Project.CopyBuildToStagingDirectory(Params);
        CopyAssetRegistryFilesFromSavedCookedToReleases(Params);
        SubmitAssetRegistryFilesToPerforce(Params);
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

			if(!bp.ParseParam("nomac"))
			{
				AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(UnrealTargetPlatform.Mac, GameProj, UnrealTargetPlatform.Mac));
			}
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win64));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win32));
			if(!bp.ParseParam("nolinux"))
			{
	            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Linux));
			}

            
            AddDependency(GUBP.EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
            AddDependency(GUBP.EditorAndToolsNode.StaticGetFullName(HostPlatform));
            //var Chunker = bp.Branch.FindProgram("BuildPatchTool");
            //AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(HostPlatform, Chunker));

            // Make sure we have the mac version of Chunker as well
			if(!bp.ParseParam("nomac"))
			{
				AddDependency(GUBP.EditorGameNode.StaticGetFullName(UnrealTargetPlatform.Mac, GameProj));
				AddDependency(GUBP.EditorAndToolsNode.StaticGetFullName(UnrealTargetPlatform.Mac));
				//AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(UnrealTargetPlatform.Mac, Chunker));
			}
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
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
    }

	public class UnrealTournamentCopyEditorNode : GUBP.HostPlatformNode
	{
        BranchInfo.BranchUProject GameProj;
        string StageDirectory;
        GUBP.GUBPBranchConfig BranchConfig;

        public UnrealTournamentCopyEditorNode(GUBP.GUBPBranchConfig InBranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, string InStageDirectory)
			: base(InHostPlatform)
		{
			GameProj = InGameProj;
			StageDirectory = InStageDirectory;
            BranchConfig = InBranchConfig;

			AddDependency(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
            AddDependency(GUBP.EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
            AddDependency(GUBP.ToolsNode.StaticGetFullName(HostPlatform));
            AddDependency(GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform));
			AddDependency(UnrealTournamentBuildNode.StaticGetFullName(InGameProj));

			AgentSharingGroup = "UnrealTournament_MakeEditorBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
		{
            return "UT_CopyEditor" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public override string GetFullName()
		{
			return StaticGetFullName(GameProj, HostPlatform);
		}
        public override string GameNameIfAnyForFullGameAggregateNode()
		{
			return GameProj.GameName;
		}
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
            return base.CISFrequencyQuantumShift(BranchConfig) + 3;
		}
		public override void DoBuild(GUBP bp)
		{
			// Clear the staging directory
			CommandUtils.DeleteDirectoryContents(StageDirectory);

			// Make a list of files to copy
			SortedSet<string> RequiredFiles = new SortedSet<string>(StringComparer.InvariantCultureIgnoreCase);
            AddEditorBuildProducts(BranchConfig, HostPlatform, GameProj, RequiredFiles);
			AddEditorSupportFiles(HostPlatform, RequiredFiles);
			RemoveUnusedPlugins(RequiredFiles);
			RemoveConfidentialFiles(HostPlatform, RequiredFiles);
            CreateDebugManifest(StageDirectory, HostPlatform, RequiredFiles);

			// Copy them to the staging directory, and add them as build products
			BuildProducts = new List<string>();
			foreach(string RequiredFile in RequiredFiles)
			{
				string SourceFileName = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, RequiredFile);
				string TargetFileName = CommandUtils.CombinePaths(StageDirectory, RequiredFile);
				CommandUtils.CopyFile(SourceFileName, TargetFileName);
				if (BuildProducts.Contains(TargetFileName))
				{
					throw new AutomationException("Overlapping build product: {0}", TargetFileName);
				}
				BuildProducts.Add(TargetFileName);
			}
		}

        static void CreateDebugManifest(string StageDirectory, UnrealTargetPlatform HostPlatform, SortedSet<string> RequiredFiles)
        {
            string ManifestFile = "Manifest_DebugFiles.txt";
            string ManifestPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Logs", ManifestFile);
            CommandUtils.DeleteFile(ManifestPath);
            
            const string Iso8601DateTimeFormat = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffzzz";

            List<string> Lines = new List<string>();
            foreach (string RequiredFile in RequiredFiles)
            {
                if (RequiredFile.EndsWith(".pdb"))
                {
                    string TimeStamp = File.GetLastWriteTimeUtc(RequiredFile).ToString(Iso8601DateTimeFormat);
                    string Dest = RequiredFile + "\t" + TimeStamp;

                    Lines.Add(Dest);
                }
            }
            CommandUtils.WriteAllLines(ManifestPath, Lines.ToArray());
        }

        static void AddEditorBuildProducts(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform HostPlatform, BranchInfo.BranchUProject GameProj, SortedSet<string> RequiredFiles)
		{
			// Build a list of all the nodes we want to copy
			List<string> NodeNames = new List<string>();
			NodeNames.Add(GUBP.RootEditorNode.StaticGetFullName(HostPlatform));
			NodeNames.Add(GUBP.EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
			NodeNames.Add(GUBP.ToolsNode.StaticGetFullName(HostPlatform));
            NodeNames.Add(GUBP.ToolsForCompileNode.StaticGetFullName(HostPlatform));

			// Create a list of all the build products
			List<string> BuildProducts = new List<string>();
			foreach(string NodeName in NodeNames)
			{
                GUBP.GUBPNode Node = InBranchConfig.FindNode(NodeName);
				if(Node == null)
				{
					throw new AutomationException("Couldn't find node '{0}'", NodeName);
				}
				foreach(string BuildProduct in Node.BuildProducts)
				{
					BuildProducts.Add(CommandUtils.StripBaseDirectory(Path.GetFullPath(BuildProduct), CommandUtils.CmdEnv.LocalRoot));
				}
			}

			// Filter them into the output list
			FileFilter BuildProductFilter = new FileFilter();

			BuildProductFilter.Include("...");
			BuildProductFilter.Exclude("*-Simplygon*");
			BuildProductFilter.Exclude("*-DDCUtils*");
			BuildProductFilter.Exclude("*-HTML5*.dylib"); // Manually remove HTML5 dylibs; they crash on startup without having the HTML5 inis
			BuildProductFilter.Exclude("UnrealFrontend*");
			BuildProductFilter.Exclude("UnrealHeaderTool*");

			RequiredFiles.UnionWith(BuildProductFilter.ApplyTo(BuildProducts));
		}
                       
		static void AddEditorSupportFiles(UnrealTargetPlatform Platform, SortedSet<string> RequiredFiles)
		{
			FileFilter Filter = new FileFilter();

			// Engine/Binaries/...
			if(Platform == UnrealTargetPlatform.Win64)
			{
                Filter.Include("/Engine/Binaries/Win64/libfbxsdk.dll");

                Filter.Include("/Engine/Binaries/Win64/embree.dll");
                Filter.Include("/Engine/Binaries/Win64/tbb.dll");
                Filter.Include("/Engine/Binaries/Win64/tbbmalloc.dll");

				Filter.Include("/Engine/Binaries/ThirdParty/CEF3/Win64/...");
				Filter.Include("/Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Win64/VS2013/*.dll");
				Filter.Include("/Engine/Binaries/ThirdParty/PhysX/APEX-1.3/Win64/VS2013/*.dll");
				Filter.Include("/Engine/Binaries/ThirdParty/PhysX/PhysX-3.3/Win64/VS2013/*.dll");
				Filter.Exclude("/Engine/Binaries/ThirdParty/PhysX/.../*DEBUG*");
				Filter.Exclude("/Engine/Binaries/ThirdParty/PhysX/.../*CHECKED*");
				Filter.Include("/Engine/Binaries/ThirdParty/Ogg/Win64/VS2013/*.dll");
				Filter.Include("/Engine/Binaries/ThirdParty/Vorbis/Win64/VS2013/*.dll");
				Filter.Include("/Engine/Binaries/ThirdParty/nvTextureTools/Win64/*.dll");
                Filter.Include("/Engine/Binaries/ThirdParty/Oculus/Audio/Win64/*.dll");
                Filter.Include("/Engine/Binaries/ThirdParty/OpenSSL/Win64/VS2013/*.dll");
			}
			else if(Platform == UnrealTargetPlatform.Mac)
            {
                Filter.Include("/Engine/Binaries/Mac/libembree.2.dylib");
                Filter.Include("/Engine/Binaries/Mac/libtbb.dylib");
                Filter.Include("/Engine/Binaries/Mac/libtbbmalloc.dylib");

				Filter.Include("/Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Mac/*.dylib");
				Filter.Include("/Engine/Binaries/ThirdParty/Mono/Mac/...");
				Filter.Include("/Engine/Binaries/ThirdParty/CEF3/Mac/...");
			}

			// Engine/Plugins/...
			Filter.Include("/Engine/Plugins/....uplugin");
			Filter.Include("/Engine/Plugins/.../Content/...");
			Filter.Include("/Engine/Plugins/.../Resources/...");

            // Engine/...
            Filter.Include("/Engine/Build/Target.cs.template");
			Filter.Include("/Engine/Build/BatchFiles/...");
			Filter.Include("/Engine/Config/...");
			Filter.Include("/Engine/Content/...");
			Filter.Include("/Engine/Documentation/Source/Shared/...");
			Filter.Include("/Engine/Shaders/...");

			// UnrealTournament/...
			Filter.Include("/UnrealTournament/*.uproject");
			Filter.Include("/UnrealTournament/Plugins/...");
            Filter.Exclude("/UnrealTournament/Plugins/.../Binaries/...");
            Filter.Exclude("/UnrealTournament/Plugins/.../Intermediate/...");
			Filter.Include("/UnrealTournament/Content/...");
			Filter.Include("/UnrealTournament/Config/...");
			Filter.Include("/UnrealTournament/Releases/...");

			RequiredFiles.UnionWith(Filter.ApplyToDirectory(CommandUtils.CmdEnv.LocalRoot, true));
		}

		static void RemoveUnusedPlugins(SortedSet<string> RequiredFiles)
		{
			FileFilter UnusedPluginFilter = new FileFilter();

			UnusedPluginFilter.Include("/Engine/Plugins/...");
			UnusedPluginFilter.Exclude("/Engine/Plugins/.../Paper2D/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../ExampleDeviceProfileSelector/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../VisualStudioSourceCodeAccess/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../XCodeSourceCodeAccess/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../AnalyticsBlueprintLibrary/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../UdpMessaging/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../Developer/...");
            UnusedPluginFilter.Exclude("/Engine/Plugins/.../Blendables/...");

			RequiredFiles.RemoveWhere(FileName => UnusedPluginFilter.Matches(FileName));
		}

		static void RemoveConfidentialFiles(UnrealTargetPlatform Platform, SortedSet<string> RequiredFiles)
		{
			FileFilter ConfidentialFilter = new FileFilter();

			ConfidentialFilter.Include(".../NotForLicensees/...");
			ConfidentialFilter.Include(".../NoRedist/...");
            ConfidentialFilter.Include(".../EpicInternal/...");
            ConfidentialFilter.Include(".../Environments/Outside/TestMaps/...");
            ConfidentialFilter.Include(".../Environments/Industrial/TestMaps/...");
            ConfidentialFilter.Include(".../Content/Backend/...");
			foreach (UnrealTargetPlatform PossiblePlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if(PossiblePlatform != Platform && PossiblePlatform != UnrealTargetPlatform.Unknown)
				{
					ConfidentialFilter.Include(String.Format(".../{0}/...", PossiblePlatform.ToString()));
				}
			}
            ConfidentialFilter.Exclude("Engine/Binaries/DotNET/AutomationScripts/NoRedist/UnrealTournament.Automation.dll");
            ConfidentialFilter.Exclude(".../NotForLicensees/UE4Editor-OnlineSubsystemMcp.dylib");
            ConfidentialFilter.Exclude(".../NotForLicensees/UE4Editor-XMPP.dylib");
            ConfidentialFilter.Exclude(".../NotForLicensees/UE4Editor-OnlineSubsystemMcp.dll");
            ConfidentialFilter.Exclude(".../NotForLicensees/UE4Editor-XMPP.dll");

			RequiredFiles.RemoveWhere(x => ConfidentialFilter.Matches(x));
		}
	}

	public class UnrealTournamentEditorDDCNode : GUBP.HostPlatformNode
	{
        BranchInfo.BranchUProject GameProj;
		string TargetPlatforms;
        string StageDirectory;
        GUBP.GUBPBranchConfig BranchConfig;

        public UnrealTournamentEditorDDCNode(GUBP.GUBPBranchConfig InBranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, string InTargetPlatforms, string InStageDirectory)
			: base(InHostPlatform)
		{
            GameProj = InGameProj;
            BranchConfig = InBranchConfig;
			TargetPlatforms = InTargetPlatforms;
			StageDirectory = InStageDirectory;
			AddDependency(UnrealTournamentCopyEditorNode.StaticGetFullName(InGameProj, InHostPlatform));

			AgentSharingGroup = "UnrealTournament_MakeEditorBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
		{
            return "UT_EditorDDC" + StaticGetHostPlatformSuffix(InHostPlatform);
		}
		public override string GetFullName()
		{
			return StaticGetFullName(GameProj, HostPlatform);
		}
        public override string GameNameIfAnyForFullGameAggregateNode()
		{
			return GameProj.GameName;
		}
		public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
		{
            return base.CISFrequencyQuantumShift(BranchConfig) + 3;
		}
		public override float Priority()
		{
			return base.Priority() + 55.0f;
		}
		public override void DoBuild(GUBP bp)
		{
			// Find all the files in the 
			List<string> RequiredFiles = new List<string>(Directory.EnumerateFiles(StageDirectory, "*", SearchOption.AllDirectories));

			// Generate the DDC
			string OutputFileName = CommandUtils.CombinePaths(StageDirectory, "Engine", "DerivedDataCache", "Compressed.ddp");
			CommandUtils.DeleteFile(OutputFileName);
            CommandUtils.DDCCommandlet(new FileReference(CommandUtils.MakeRerootedFilePath(GameProj.FilePath.ToString(), CommandUtils.CmdEnv.LocalRoot, StageDirectory)), CommandUtils.GetEditorCommandletExe(StageDirectory, HostPlatform), null, TargetPlatforms, "-fill -DDC=CreateInstalledEnginePak -Map=Example_Map");
			RequiredFiles.Add(OutputFileName);

			// Clean up the directory from everything else
			CleanDirectory(StageDirectory, RequiredFiles);

			// Return just the new DDP file as build product from this node
			BuildProducts = new List<string>{ OutputFileName };
		}
		static bool RemoveEmptyDirectories(string DirectoryName)
		{
			bool bIsEmpty = true;
			foreach(string ChildDirectoryName in Directory.EnumerateDirectories(DirectoryName))
			{
				if(RemoveEmptyDirectories(ChildDirectoryName) && !Directory.EnumerateFiles(ChildDirectoryName).Any())
				{
					CommandUtils.DeleteDirectory(ChildDirectoryName);
				}
				else
				{
					bIsEmpty = false;
				}
			}
			return bIsEmpty;
		}
		static void CleanDirectory(string DirectoryName, IEnumerable<string> RetainFiles)
		{
			string FullDirectoryName = Path.GetFullPath(DirectoryName);

			// Make a normalized list of files to keep
			HashSet<string> RetainFilesSet = new HashSet<string>(RetainFiles.Select(File => Path.GetFullPath(File)), StringComparer.InvariantCultureIgnoreCase);
			foreach(string FileName in Directory.EnumerateFiles(FullDirectoryName, "*", SearchOption.AllDirectories))
			{
				if(!RetainFilesSet.Contains(FileName))
				{
					CommandUtils.DeleteFile(FileName);
				}
			}

			// Remove all the empty folders
			RemoveEmptyDirectories(DirectoryName);
		}
	}

	public class UnrealTournamentPublishEditorNode : GUBP.HostPlatformNode
	{
		BranchInfo.BranchUProject GameProj;
		string SourceDirectory;
		string TargetDirectory;

		public UnrealTournamentPublishEditorNode(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, string InSourceDirectory, string InTargetDirectory)
            : base(InHostPlatform)
        {
			GameProj = InGameProj;
			SourceDirectory = InSourceDirectory;
			TargetDirectory = InTargetDirectory;

            AddDependency(UnrealTournamentCopyEditorNode.StaticGetFullName(GameProj, InHostPlatform));
            AddDependency(UnrealTournamentEditorDDCNode.StaticGetFullName(GameProj, InHostPlatform));

			AgentSharingGroup = "UnrealTournament_MakeEditorBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
        {
            return "UT_PublishEditor" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform);
        }
        public override string GameNameIfAnyForFullGameAggregateNode()
        {
            return GameProj.GameName;
        }
        public override void DoBuild(GUBP bp)
        {
            BuildProducts = new List<string>();
			CommandUtils.CloneDirectory(SourceDirectory, TargetDirectory);
			SaveRecordOfSuccessAndAddToBuildProducts();
        }
	}

	public class UnrealTournamentChunkEditorNode : GUBP.HostPlatformNode
	{
        BranchInfo.BranchUProject GameProj;
        string StageDirectory;
        GUBP.GUBPBranchConfig BranchConfig;

        public UnrealTournamentChunkEditorNode(GUBP.GUBPBranchConfig InBranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform, string InStageDirectory)
            : base(InHostPlatform)
        {
            BranchConfig = InBranchConfig;
            GameProj = InGameProj;
			StageDirectory = InStageDirectory;

            AddDependency(UnrealTournamentCopyEditorNode.StaticGetFullName(GameProj, InHostPlatform));
            AddDependency(UnrealTournamentEditorDDCNode.StaticGetFullName(GameProj, InHostPlatform));

            //SingleTargetProperties BuildPatchTool = bp.Branch.FindProgram("BuildPatchTool");
            //if (BuildPatchTool.Rules == null)
            //{
            //    throw new AutomationException("Could not find program BuildPatchTool.");
            //}
			//AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(HostPlatform, BuildPatchTool));

			AgentSharingGroup = "UnrealTournament_MakeEditorBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
        {
            return "UT_ChunkEditorBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform);
        }
        public override string GameNameIfAnyForFullGameAggregateNode()
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
            if (BranchConfig.JobInfo.IsPreflight)
            {
                CommandUtils.Log("Things like a real UT build is likely to cause confusion if done in a preflight build, so we are skipping it.");
            }
            else
            {
				string BranchName = "";
				if (CommandUtils.P4Enabled)
				{
					BranchName = CommandUtils.P4Env.BuildRootP4;
				}

				BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(bp, HostPlatform, BranchName);

                string DebugManifest = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Saved", "Logs", "Manifest_DebugFiles.txt");
                if (!CommandUtils.FileExists(DebugManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", DebugManifest);
                }

				// run the patch tool
				BuildPatchToolBase.Get().Execute(
				new BuildPatchToolBase.PatchGenerationOptions
				{
					StagingInfo = StagingInfo,
					BuildRoot = StageDirectory,
                    FileIgnoreList = DebugManifest,
					AppLaunchCmd = GetEditorLaunchCmd(HostPlatform),
					AppLaunchCmdArgs = "UnrealTournament -installed",
					AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
				});

				// post the Editor build to the UnrealTournamentBuilds app
				string McpConfigName = "MainGameDevNet";
				CommandUtils.Log("Posting UnrealTournament Editor for {0} to MCP.", HostPlatform.ToString());
				BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
				CommandUtils.Log("Labeling new build as Live in MCP.");
				string LabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Live", StagingInfo.Platform);
				BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelName, McpConfigName);

				// For back-compat post to branch's Testing app in Launcher as well.  To be deprecated.
				UnrealTournamentBuild.UnrealTournamentEditorAppName TestingApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorDevTesting;
				if (BranchName.Contains("UT-Releases"))
				{
					TestingApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorReleaseTesting;
				}
				CommandUtils.Log("Also posting to app {0} based on branch {1} to automatically make a build available in gamedev launcher", TestingApp, BranchName);
				// Reuse the same staginginfo but to the old app (need to hardcode the manifest to match it being created with the new app)
				BuildPatchToolStagingInfo TestingAppStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(bp, StagingInfo.BuildVersion, StagingInfo.Platform, TestingApp, StagingInfo.ManifestFilename);
				BuildInfoPublisherBase.Get().PostBuildInfo(TestingAppStagingInfo);
				BuildInfoPublisherBase.Get().LabelBuild(TestingAppStagingInfo, LabelName, McpConfigName);
			}
			SaveRecordOfSuccessAndAddToBuildProducts();
        }
		public static string GetEditorLaunchCmd(UnrealTargetPlatform Platform)
		{
			switch(Platform)
			{
				case UnrealTargetPlatform.Win64:
					return @".\Engine\Binaries\Win64\UE4Editor.exe";
				case UnrealTargetPlatform.Mac:
					return @"./Engine/Binaries/Mac/UE4Editor.app";
				default:
					throw new AutomationException("Unknown editor platform '{0}'", Platform);
			}
		}
	}

    public class UnrealTournamentChunkNode : GUBP.HostPlatformNode
    {
        BranchInfo.BranchUProject GameProj;
        GUBP.GUBPBranchConfig BranchConfig;

        public UnrealTournamentChunkNode(GUBP.GUBPBranchConfig InBranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;
            BranchConfig = InBranchConfig;

            AddDependency(UnrealTournamentBuildNode.StaticGetFullName(GameProj));
        }

        public static string StaticGetFullName(BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
        {
            return InGameProj.GameName + "_ChunkBuild" + StaticGetHostPlatformSuffix(InHostPlatform);
        }
        public override string GetFullName()
        {
            return StaticGetFullName(GameProj, HostPlatform);
        }
        public override string GameNameIfAnyForFullGameAggregateNode()
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
            if (BranchConfig.JobInfo.IsPreflight)
            {
                CommandUtils.Log("Things like a real UT build is likely to cause confusion if done in a preflight build, so we are skipping it.");
            }
            else
            {
                if (HostPlatform == UnrealTargetPlatform.Mac)
                {
                    LogFile = CommandUtils.RunUAT(CommandUtils.CmdEnv, "UnrealTournamentProto_ChunkBuild -mac");
                }
                else
                {
                    LogFile = CommandUtils.RunUAT(CommandUtils.CmdEnv, "UnrealTournamentProto_ChunkBuild");
                }
            }
            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));
        }
    }

    public class UnrealTournamentBuildNode : GUBP.GUBPNode
    {
        BranchInfo.BranchUProject GameProj;
        GUBP.GUBPBranchConfig BranchConfig;

        public UnrealTournamentBuildNode(GUBP.GUBPBranchConfig InBranchConfig, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform HostPlatform)
        {
            GameProj = InGameProj;
            BranchConfig = InBranchConfig;

			if (CommandUtils.P4Enabled && CommandUtils.P4Env.BuildRootP4 == "//depot/UE4-UT-Releases")
			{
				AddDependency(GUBP.VersionFilesNode.StaticGetFullName());
			}

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
        public override string GameNameIfAnyForFullGameAggregateNode()
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
            if (BranchConfig.JobInfo.IsPreflight)
            {
                CommandUtils.Log("Things like a real UT build is likely to cause confusion if done in a preflight build, so we are skipping it.");
            }
            else
            {
                LogFile = CommandUtils.RunUAT(CommandUtils.CmdEnv, "UnrealTournamentProto_BasicBuild -SkipBuild -SkipCook -Chunk");
            }
            SaveRecordOfSuccessAndAddToBuildProducts(CommandUtils.ReadAllText(LogFile));

			if (CommandUtils.P4Enabled && CommandUtils.P4Env.BuildRootP4 == "//depot/UE4-UT-Releases")
            {
                SubmitVersionFilesToPerforce();
            }

			string ReleasesDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "UnrealTournament", "Releases");
			BuildProducts.AddRange(CommandUtils.FindFiles("*", true, ReleasesDir));
        }

        public void SubmitVersionFilesToPerforce()
        {
            if (CommandUtils.P4Enabled)
            {
                int VersionFilesCL = CommandUtils.P4.CreateChange(CommandUtils.P4Env.Client, String.Format("UnrealTournamentBuild Version Files from changelist {0}", CommandUtils.P4Env.Changelist));
                if (VersionFilesCL > 0)
                {
                    string[] VersionFiles = new string[4];
                    VersionFiles[0] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "build.properties");
                    VersionFiles[1] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Core", "Private", "UObject", "ObjectVersion.cpp");
                    VersionFiles[2] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Launch", "Resources", "Version.h");
                    VersionFiles[3] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Programs", "DotNETCommon", "MetaData.cs");
                
                    foreach (string VersionFile in VersionFiles)
                    {
                        CommandUtils.P4.Sync("-f -k " + VersionFile + "#head"); // sync the file without overwriting local one

                        if (!CommandUtils.FileExists(VersionFile))
                        {
                            throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", VersionFile);
                        }

                        CommandUtils.P4.ReconcileNoDeletes(VersionFilesCL, VersionFile);                        
                    }
                    bool Pending;
                    if (CommandUtils.P4.ChangeFiles(VersionFilesCL, out Pending).Count == 0)
                    {
                        CommandUtils.Log("************************* No files to submit.");
                        CommandUtils.P4.DeleteChange(VersionFilesCL);
                    }
                    else
                    {
                        int SubmittedCL;
                        CommandUtils.P4.Submit(VersionFilesCL, out SubmittedCL, true, true);
                    }
                }
            }
        }
    }

    public override void AddNodes(GUBP bp, GUBP.GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InActivePlatforms)
    {
        //if (!bp.BranchOptions.ExcludeNodes.Contains("UnrealTournament"))
        {            
            BranchInfo.BranchUProject GameProj = BranchConfig.Branch.FindGame("UnrealTournament");
            if (GameProj != null)
            {
                CommandUtils.Log("*** Adding UT-specific nodes to the GUBP");

				if (InHostPlatform == UnrealTargetPlatform.Win64)
				{
                    AddHostNodes(bp, BranchConfig, GameProj, InHostPlatform, "WindowsEditor", "Windows");
                    BranchConfig.AddNode(new WaitForUnrealTournamentBuildUserInputNode(bp, GameProj, InHostPlatform));
                    BranchConfig.AddNode(new UnrealTournamentBuildNode(BranchConfig, GameProj, InHostPlatform));
                    BranchConfig.AddNode(new UnrealTournamentChunkNode(BranchConfig, GameProj, InHostPlatform));
                }
                else if (InHostPlatform == UnrealTargetPlatform.Mac)
                {
                    AddHostNodes(bp, BranchConfig, GameProj, InHostPlatform, "MacEditor", "Mac");
                    BranchConfig.AddNode(new UnrealTournamentChunkNode(BranchConfig, GameProj, InHostPlatform));
                }
            }
        }
    }

    void AddHostNodes(GUBP bp, GUBP.GUBPBranchConfig BranchConfig, BranchInfo.BranchUProject GameProj, UnrealTargetPlatform HostPlatform, string PlatformName, string TargetPlatformsForDDC)
	{
		string StageDirectory = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "UT-Build", HostPlatform.ToString());

        BranchConfig.AddNode(new UnrealTournamentCopyEditorNode(BranchConfig, GameProj, HostPlatform, StageDirectory));
        BranchConfig.AddNode(new UnrealTournamentEditorDDCNode(BranchConfig, GameProj, HostPlatform, TargetPlatformsForDDC, StageDirectory));
        BranchConfig.AddNode(new UnrealTournamentChunkEditorNode(BranchConfig, GameProj, HostPlatform, StageDirectory));

		string PublishDirectory = CommandUtils.CombinePaths(UnrealTournamentBuild.GetArchiveDir(), PlatformName);

        BranchConfig.AddNode(new UnrealTournamentPublishEditorNode(GameProj, HostPlatform, StageDirectory, PublishDirectory));
	}
}

// Performs the appropriate labeling and other actions for a given promotion
// See the EC job for an up-to-date description of all parameters
[Help("ToAppName", "The appname to promote the build to.")]
[Help("BuildVersion", "Non-platform-specific BuildVersion to promote.")]
[Help("Platforms", "Comma-separated list of platforms to promote.")]
[Help("AllowLivePromotion", "Optional.  Toggle on to allow promotion to the Live instance/label.")]
[Help("AWSCredentialsFile", @"Optional.  The full path to the Amazon credentials file used to access S3. Defaults to P:\Builds\Utilities\S3Credentials.txt.")]
[Help("AWSCredentialsKey", "Optional. The name of the credentials profile to use when accessing AWS.  Defaults to \"s3_origin_prod\".")]
[Help("AWSRegion", "Optional. The system name of the Amazon region which contains the S3 bucket.  Defaults to \"us-east-1\".")]
[Help("AWSBucket", "Optional. The name of the Amazon S3 bucket to copy a build to. Defaults to \"patcher-origin\".")]
class UnrealTournament_PromoteBuild : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* UnrealTournament_PromoteBuild");

		// New cross-promote apps only ever have a Live label.  Let them by without setting the label dropdown.
		const string LiveLabel = "Live";

		// Determine whether to promote game, editor, or both
		bool bShouldPromoteGameClient = false;
		bool bShouldPromoteEditor = false;
        {
			List<string> AllProducts = new List<string> { "GameClient", "Editor" };
			string ProductsString = ParseParamValue("Products");
			if (string.IsNullOrEmpty(ProductsString))
			{
				throw new AutomationException("Products is a required parameter");
			}
			var Products = ProductsString.Split(',').Select(x => x.Trim()).ToList();
			var InvalidProducts = Products.Except(AllProducts);
			if (InvalidProducts.Any())
			{
				throw new AutomationException(CreateDebugList(InvalidProducts, "The following product names are invalid:"));
			}
			bShouldPromoteGameClient = Products.Contains("GameClient");
			bShouldPromoteEditor = Products.Contains("Editor");
		}

		// Setup the list of dev apps vs. release apps for enforcing branch consistency for each backend.  Editor builds track with game builds, so just use the game builds' app-to-env mappings
		List<UnrealTournamentBuild.UnrealTournamentAppName> DevBranchApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		DevBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting);

		List<UnrealTournamentBuild.UnrealTournamentAppName> ReleaseBranchApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting);
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseStage);
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest);
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLiveTesting);
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLiveStage);
		ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev); // live public app, stuck on confusing legacy name

		// FromApps
		List<UnrealTournamentBuild.UnrealTournamentAppName> FromApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		FromApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentBuilds);

		// All other apps are only for promoting "to"
		// Map which apps are GameDev only
		List<UnrealTournamentBuild.UnrealTournamentAppName> GameDevApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting);
		GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting);
		GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLiveTesting);
		// Map which apps are Stage only
		List<UnrealTournamentBuild.UnrealTournamentAppName> StageApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseStage);
		GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLiveStage);
		// Map which apps are Production only
		List<UnrealTournamentBuild.UnrealTournamentAppName> ProdApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
		ProdApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest);
		ProdApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev);

		// Each environment's mcpconfig string
		string GameDevMcpConfigString = "MainGameDevNet";
		string StagingMcpConfigString = "StageNet";
		string ProdMcpConfigString = "ProdNet";

		string BuildVersion = ParseParamValue("BuildVersion");
		if (String.IsNullOrEmpty(BuildVersion))
		{
			throw new AutomationException("BuildVersion is a required parameter");
		}
		// Strip platform if it's on there, common mistake/confusion to include it in EC because it's included in the admin portal UI
		foreach (String platform in Enum.GetNames(typeof(MCPPlatform)))
		{
			if (BuildVersion.EndsWith("-" + platform))
			{
				BuildVersion = BuildVersion.Substring(0, BuildVersion.Length - ("-" + platform).Length);
				Log("Stripped platform off BuildVersion, resulting in {0}", BuildVersion);
			}
		}
		// Add ++depot+ junk if it's missing
		if (!BuildVersion.StartsWith("++depot+"))
		{
			BuildVersion = "++depot+" + BuildVersion;
		}

		// Enforce some restrictions on destination apps
		UnrealTournamentBuild.UnrealTournamentAppName ToGameApp;
		{
			string ToGameAppName = ParseParamValue("ToAppName");
			if (ToGameAppName == "Select Target App")
			{
				// Scrub the default option out of the param, treat it the same as not passing the param
				ToGameAppName = "";
			}
			if (String.IsNullOrEmpty(ToGameAppName))
			{
				throw new AutomationException("ToAppName is a required parameter");
			}
			if (!Enum.TryParse(ToGameAppName, out ToGameApp))
			{
				throw new AutomationException("Unrecognized ToAppName: {0}", ToGameAppName);
			}
			if (FromApps.Contains(ToGameApp))
			{
				throw new AutomationException("App passed in ToAppName is not valid as a destination app: {0}", ToGameAppName);
			}
		}

		// Setup the editor to app as well
		UnrealTournamentBuild.UnrealTournamentEditorAppName ToEditorApp;
		{
			String ToEditorAppName = ToGameApp.ToString().Replace("UnrealTournament", "UnrealTournamentEditor");

			if (!Enum.TryParse(ToEditorAppName, out ToEditorApp))
			{
				throw new AutomationException("Unable to find an editor app {0} matching game app {1}", ToEditorAppName, ToGameApp.ToString());
			}
		}

		// Setup source game app
		UnrealTournamentBuild.UnrealTournamentAppName FromGameApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentBuilds;
		// UT only has one source app, but setup the editor source as well
		UnrealTournamentBuild.UnrealTournamentEditorAppName FromEditorApp;
		{
			String FromEditorAppName = FromGameApp.ToString().Replace("UnrealTournament", "UnrealTournamentEditor");

			if (!Enum.TryParse(FromEditorAppName, out FromEditorApp))
			{
				throw new AutomationException("Unable to find an editor app {0} matching game app {1}", FromEditorAppName, FromGameApp.ToString());
			}
		}

		// Set some simple flags for identifying the type of promotion
		bool bIsGameDevPromotion = GameDevApps.Contains(ToGameApp);
		bool bIsStagePromotion = StageApps.Contains(ToGameApp);
		bool bIsProdPromotion = ProdApps.Contains(ToGameApp);
		bool bIsLivePromotion = ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev;

		// Determine the environment for the target app
		string TargetAppMcpConfig;
		if (bIsProdPromotion)
		{
			TargetAppMcpConfig = ProdMcpConfigString;
		}
		else if (bIsStagePromotion)
		{
			TargetAppMcpConfig = StagingMcpConfigString;
		}
		else if (bIsGameDevPromotion)
		{
			TargetAppMcpConfig = GameDevMcpConfigString;
		}
		else
		{
			// How the heck did we get here?
			throw new AutomationException("Couldn't decide which environments to apply label {0} of ToApp {1}", LiveLabel, ToGameApp.ToString());
		}

		// Make sure the switch was flipped if promoting to the Live app used by the public
		bool AllowLivePromotion = ParseParam("AllowLivePromotion");
		if (AllowLivePromotion == false && bIsLivePromotion)
		{
			throw new AutomationException("You attempted to promote to Live without toggling on AllowLivePromotions.  Did you mean to promote to Live?");
		}

		// Work out the manifest URL for each platform we're promoting, and check the source builds are available
		Log("-- Verifying source build exists for all platforms");
		var GameStagingInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
		var EditorStagingInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
        {
			// Pull the list of platforms requested for promotion
			string PlatformParam = ParseParamValue("Platforms");
			if (String.IsNullOrEmpty(PlatformParam))
			{
				throw new AutomationException("Platforms list is a required parameter - defaults are set in EC");
			}
			List<MCPPlatform> RequestedPlatforms = PlatformParam.Split(',').Select(PlatformStr => (MCPPlatform)Enum.Parse(typeof(MCPPlatform), PlatformStr)).ToList<MCPPlatform>();
			if (RequestedPlatforms.Count == 0)
			{
				throw new AutomationException("Platforms is a required parameter, unable to parse platforms from {0}", PlatformParam);
			}

			var InvalidPlatforms = new List<string>();
			if (bShouldPromoteGameClient)
			{
				// Setup game staging infos to be posted
				foreach (var Platform in RequestedPlatforms)
				{
					var GameSourceStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromGameApp);
					if (!BuildInfoPublisherBase.Get().BuildExists(GameSourceStagingInfo, GameDevMcpConfigString))
					{
						InvalidPlatforms.Add(Platform.ToString());
						Log("Unable to find platform {0} build with version {3} in app {1} on {2} mcp", GameSourceStagingInfo.Platform, FromGameApp, GameDevMcpConfigString, GameSourceStagingInfo.BuildVersion);
					}
					else
					{
						var ManifestFilename = Path.GetFileName(BuildInfoPublisherBase.Get().GetBuildManifestUrl(GameSourceStagingInfo, GameDevMcpConfigString));
						var GameTargetStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, ToGameApp, ManifestFilename);
						GameStagingInfos.Add(Platform, GameTargetStagingInfo);
					}
				}
				// Make sure we didn't hit any platforms for which the build didn't exist
				if (InvalidPlatforms.Any())
				{
					throw new AutomationException("Source game builds for {0} build {1} do not exist on all specified platforms.", FromGameApp, BuildVersion);
				}
			}
			if (bShouldPromoteEditor)
			{
				// Only do non-win32 editor builds
				List<MCPPlatform> EditorPlatforms = RequestedPlatforms.Where(x => x != MCPPlatform.Win32).ToList();
				// Setup game staging infos to be posted
				foreach (var Platform in EditorPlatforms)
				{
					var EditorSourceStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromEditorApp);
					if (!BuildInfoPublisherBase.Get().BuildExists(EditorSourceStagingInfo, GameDevMcpConfigString))
					{
						InvalidPlatforms.Add(Platform.ToString());
						Log("Unable to find platform {0} build with version {3} in app {1} on {2} mcp", EditorSourceStagingInfo.Platform, FromEditorApp, GameDevMcpConfigString, EditorSourceStagingInfo.BuildVersion);
					}
					else
					{
						var ManifestFilename = Path.GetFileName(BuildInfoPublisherBase.Get().GetBuildManifestUrl(EditorSourceStagingInfo, GameDevMcpConfigString));
						var EditorTargetStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, ToEditorApp, ManifestFilename);
						EditorStagingInfos.Add(Platform, EditorTargetStagingInfo);
					}
				}
				// Make sure we didn't hit any platforms for which the build didn't exist
				if (InvalidPlatforms.Any())
				{
					throw new AutomationException("Source editor builds for {0} build {1} do not exist on all specified platforms.", FromEditorApp, BuildVersion);
				}
			}

		}

		// S3 PARAMETERS (used during staging and operating on ProdCom only)
		CloudStorageBase CloudStorage = null;
		string S3Bucket = null;
		if (bIsProdPromotion || bIsStagePromotion)
		{
			S3Bucket = ParseParamValue("AWSBucket", "patcher-origin");

			var CloudConfiguration = new Dictionary<string, object>
			{
				{ "CredentialsFilePath", ParseParamValue("AWSCredentialsFile", @"P:\Builds\Utilities\S3Credentials.txt") },
				{ "CredentialsKey",      ParseParamValue("AWSCredentialsKey", "s3_origin_prod") },
				{ "AWSRegion",           ParseParamValue("AWSRegion", "us-east-1") }
			};

			CloudStorage = CloudStorageBase.Get();
			CloudStorage.Init(CloudConfiguration);
		}

		// Ensure that the build exists in gamedev Build Info services, regardless of where we're promoting to
		if (GameDevApps.Contains(ToGameApp))
		{
			Log("-- Ensuring all builds are posted to {0} app {1}/{2} for gamedev promotion", GameDevMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
			EnsureBuildIsRegistered(GameDevMcpConfigString, GameStagingInfos);
			EnsureBuildIsRegistered(GameDevMcpConfigString, EditorStagingInfos);
		}

		// If this label will go to Prod, then make sure the build manifests are all staged to the Prod CDN already
		if (bIsProdPromotion)
		{
			// Look for game builds' manifests
			foreach (var Platform in GameStagingInfos.Keys)
			{
				var StagingInfo = GameStagingInfos[Platform]; // We can use the target here, as it's guaranteed to have the correct manifest filename embedded

				// Check for the manifest on the prod CDN
				Log("Verifying manifest for prod promotion of UT Game {0} {1} was already staged to the internal origin server", BuildVersion, Platform);
				bool bWasManifestFound = BuildInfoPublisherBase.Get().IsManifestOnProductionCDN(StagingInfo);
				if (!bWasManifestFound)
				{
					string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
					throw new AutomationException("Promotion to Prod requires the build first be staged to the internal origin server. Manifest {0} not found for promotion to label {1} of app {2}"
						, StagingInfo.ManifestFilename
						, DestinationLabelWithPlatform
						, ToGameApp.ToString());
				}
				Log("Verifying manifest for prod promotion of Ocean {0} {1} was already staged to the S3 origin", BuildVersion, Platform);
				bWasManifestFound = CloudStorage.IsManifestOnCloudStorage(S3Bucket, StagingInfo);
				if (!bWasManifestFound)
				{
					string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
					throw new AutomationException("Promotion to Prod requires the build first be staged to the S3 origin. Manifest {0} not found for promotion to label {1} of app {2}"
						, StagingInfo.ManifestFilename
						, DestinationLabelWithPlatform
						, ToGameApp.ToString());
				}
			}
			// Look for editor builds' manifests
			foreach (var Platform in EditorStagingInfos.Keys)
			{
				var StagingInfo = EditorStagingInfos[Platform]; // We can use the target here, as it's guaranteed to have the correct manifest filename embedded

				// Check for the manifest on the prod CDN
				Log("Verifying manifest for prod promotion of UT Editor {0} {1} was already staged to the internal origin server", BuildVersion, Platform);
				bool bWasManifestFound = BuildInfoPublisherBase.Get().IsManifestOnProductionCDN(StagingInfo);
				if (!bWasManifestFound)
				{
					string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
					throw new AutomationException("Promotion to Prod requires the build first be staged to the internal origin server. Manifest {0} not found for promotion to label {1} of app {2}"
						, StagingInfo.ManifestFilename
						, DestinationLabelWithPlatform
						, ToEditorApp.ToString());
				}
				Log("Verifying manifest for prod promotion of Ocean {0} {1} was already staged to the S3 origin", BuildVersion, Platform);
				bWasManifestFound = CloudStorage.IsManifestOnCloudStorage(S3Bucket, StagingInfo);
				if (!bWasManifestFound)
				{
					string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
					throw new AutomationException("Promotion to Prod requires the build first be staged to the S3 origin. Manifest {0} not found for promotion to label {1} of app {2}"
						, StagingInfo.ManifestFilename
						, DestinationLabelWithPlatform
						, ToEditorApp.ToString());
				}
			}


			// Ensure the build is registered on the prod and staging build info services.
			Log("-- Ensuring builds are posted to {0} app {1}/{2} for prod promotion", StagingMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
			EnsureBuildIsRegistered(StagingMcpConfigString, GameStagingInfos);
			EnsureBuildIsRegistered(StagingMcpConfigString, EditorStagingInfos);
			Log("-- Ensuring build are posted to {0} app {1}/{2} for prod promotion", ProdMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
			EnsureBuildIsRegistered(ProdMcpConfigString, GameStagingInfos);
			EnsureBuildIsRegistered(ProdMcpConfigString, EditorStagingInfos);
		}

		//
		// Execute additional logic required for each promotion
		//

		Log("Performing build promotion actions for promoting to label {0} of app {1}/{2}.", LiveLabel, ToGameApp.ToString(), ToEditorApp.ToString());
        if (bIsStagePromotion)
		{
			// Copy game chunks to CDN
			foreach (var Platform in GameStagingInfos.Keys)
			{
				var StagingInfo = GameStagingInfos[Platform];
				{
					Log("-- Promoting game chunks to internal origin server");
					BuildInfoPublisherBase.Get().CopyChunksToProductionCDN(StagingInfo);
					Log("Promoting game chunks to S3 origin");
					CloudStorage.CopyChunksToCloudStorage(S3Bucket, StagingInfo);
					Log("DONE Promoting game chunks");
				}
			}
			// Copy editor chunks to CDN
			foreach (var Platform in EditorStagingInfos.Keys)
			{
				var StagingInfo = EditorStagingInfos[Platform];
				{
					Log("-- Promoting editor chunks to internal origin server");
					BuildInfoPublisherBase.Get().CopyChunksToProductionCDN(StagingInfo);
					Log("Promoting editor chunks to S3 origin");
					CloudStorage.CopyChunksToCloudStorage(S3Bucket, StagingInfo);
					Log("DONE Promoting editor chunks");
				}
			}

			// Ensure the build is registered on the staging build info services.
			Log("Ensuring build is posted to {0} app {1}/{2} for stage promotion", StagingMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
			EnsureBuildIsRegistered(StagingMcpConfigString, GameStagingInfos);
			EnsureBuildIsRegistered(StagingMcpConfigString, EditorStagingInfos);
		}

		// Apply game rollback label to preserve info about last Live build
		{
			Log("-- Starting Rollback Labeling");
			Dictionary<MCPPlatform, BuildPatchToolStagingInfo> RollbackBuildInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
			// For each platform, get the Live labeled build and create a staging info for posting to the Rollback label
			foreach (var Platform in GameStagingInfos.Keys)
			{
				var TargetStagingInfo = GameStagingInfos[Platform];
				string LiveLabelWithPlatform = LiveLabel + "-" + Platform;
				string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(TargetStagingInfo.AppName, LiveLabelWithPlatform, TargetAppMcpConfig);
				if (String.IsNullOrEmpty(LiveBuildVersionString))
				{
					Log("platform {0} of app {1}: No Live game build found, skipping rollback labeling", Platform, ToGameApp.ToString());
				}
				else if (LiveBuildVersionString.EndsWith("-" + Platform))
				{
					// Take off platform so it can fit in the FEngineVersion struct
					string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
					if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
					{
						// Create a staging info with the rollback BuildVersion and add it to the list
						Log("platform {0} of app {1}: Identified Live game build {2}, queueing for rollback labeling", Platform, ToGameApp.ToString(), LiveBuildVersionString);
						BuildPatchToolStagingInfo RollbackStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, LiveBuildVersionStringWithoutPlatform, Platform, ToGameApp, GameStagingInfos[Platform].ManifestFilename);
						RollbackBuildInfos.Add(Platform, RollbackStagingInfo);
					}
					else
					{
						Log("platform {0} of app {1}: Identified Live game build {2}, skipping rollback labeling as this build was already live, and would cause us to lose track of the previous live buildversion", Platform, ToGameApp.ToString(), LiveBuildVersionString);
					}
				}
				else
				{
					throw new AutomationException("Current Live game buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
				}
			}
			if (RollbackBuildInfos.Count > 0)
			{
				Log("Applying rollback labels for {0} previous Live game build platforms identified above", RollbackBuildInfos.Count);
				LabelBuilds(RollbackBuildInfos, "Rollback", TargetAppMcpConfig);
			}
		}

		// Apply editor rollback label to preserve info about last Live build
		{
			Log("-- Starting Rollback Labeling");
			Dictionary<MCPPlatform, BuildPatchToolStagingInfo> RollbackBuildInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
			// For each platform, get the Live labeled build and create a staging info for posting to the Rollback label
			foreach (var Platform in EditorStagingInfos.Keys)
			{
				var TargetStagingInfo = EditorStagingInfos[Platform];
				string LiveLabelWithPlatform = LiveLabel + "-" + Platform;
				string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(TargetStagingInfo.AppName, LiveLabelWithPlatform, TargetAppMcpConfig);
				if (String.IsNullOrEmpty(LiveBuildVersionString))
				{
					Log("platform {0} of app {1}: No Live editor build found, skipping rollback labeling", Platform, ToEditorApp.ToString());
				}
				else if (LiveBuildVersionString.EndsWith("-" + Platform))
				{
					// Take off platform so it can fit in the FEngineVersion struct
					string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
					if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
					{
						// Create a staging info with the rollback BuildVersion and add it to the list
						Log("platform {0} of app {1}: Identified Live editor build {2}, queueing for rollback labeling", Platform, ToEditorApp.ToString(), LiveBuildVersionString);
						BuildPatchToolStagingInfo RollbackStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, LiveBuildVersionStringWithoutPlatform, Platform, ToEditorApp, EditorStagingInfos[Platform].ManifestFilename);
						RollbackBuildInfos.Add(Platform, RollbackStagingInfo);
					}
					else
					{
						Log("platform {0} of app {1}: Identified Live editor build {2}, skipping rollback labeling as this build was already live, and would cause us to lose track of the previous live buildversion", Platform, ToEditorApp.ToString(), LiveBuildVersionString);
					}
				}
				else
				{
					throw new AutomationException("Current Live editor buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
				}
			}
			if (RollbackBuildInfos.Count > 0)
			{
				Log("Applying rollback labels for {0} previous Live build platforms identified above", RollbackBuildInfos.Count);
				LabelBuilds(RollbackBuildInfos, "Rollback", TargetAppMcpConfig);
			}
		}

		// Apply the LIVE label in the target apps
		Log("-- Labeling build {0} with label {1} on app {2}/{3} across all platforms", BuildVersion, LiveLabel, ToGameApp.ToString(), ToEditorApp.ToString());
		LabelBuilds(GameStagingInfos, LiveLabel, TargetAppMcpConfig);
		LabelBuilds(EditorStagingInfos, LiveLabel, TargetAppMcpConfig);

		Log("************************* Ocean_PromoteBuild completed");
	}

	private void EnsureBuildIsRegistered(string McpConfigNames, Dictionary<MCPPlatform, BuildPatchToolStagingInfo> StagingInfos)
	{
		foreach (string McpConfigName in McpConfigNames.Split(','))
		{
			foreach (var Platform in StagingInfos.Keys)
			{
				BuildPatchToolStagingInfo StagingInfo = StagingInfos[Platform];
				if (!BuildInfoPublisherBase.Get().BuildExists(StagingInfo, McpConfigName))
				{
					Log("Posting build {0} to app {1} of {2} build info service as it does not already exist.", StagingInfo.BuildVersion, StagingInfo.AppName, McpConfigName);
					BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, McpConfigName);
				}
			}
		}
	}

	/// <summary>
	/// Apply the requested label to the requested build in the BuildInfo backend for the requested MCP environment
	/// Repeat for each passed platform, adding the platform to the end of the label that is applied
	/// </summary>
	/// <param name="BuildInfos">The dictionary of metadata about all builds we're working with</param>
	/// <param name="DestinationLabel">Label, WITHOUT platform embedded, to apply</param>
	/// <param name="McpConfigNames">Which BuildInfo backends to label the build in.</param>
	private void LabelBuilds(Dictionary<MCPPlatform, BuildPatchToolStagingInfo> StagingInfos, string DestinationLabel, string McpConfigNames)
	{
		foreach (string McpConfigName in McpConfigNames.Split(','))
		{
			foreach (var Platform in StagingInfos.Keys)
			{
				string LabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
				BuildInfoPublisherBase.Get().LabelBuild(StagingInfos[Platform], LabelWithPlatform, McpConfigName);
			}
		}
	}

	static public string CreateDebugList<T>(IEnumerable<T> objects, string prefix)
	{
		return objects.Aggregate(new StringBuilder(prefix), (sb, obj) => sb.AppendFormat("\n    {0}", obj.ToString())).ToString();
	}
}

public class MakeUTDLC : BuildCommand
{
    public string DLCName;
    public string DLCMaps;
    public string AssetRegistry;
    public string VersionString;

    static public ProjectParams GetParams(BuildCommand Cmd, string DLCName, string AssetRegistry)
    {
        string P4Change = "Unknown";
        string P4Branch = "Unknown";
        if (P4Enabled)
        {
            P4Change = P4Env.ChangelistString;
            P4Branch = P4Env.BuildRootEscaped;
        }

        ProjectParams Params = new ProjectParams
        (
            Command: Cmd,
            // Shared
            Cook: true,
            Stage: true,
            Pak: true,
            BasedOnReleaseVersion: AssetRegistry,
            RawProjectPath: new FileReference(CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject")),
            StageDirectoryParam: CommandUtils.CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Saved", "StagedBuilds", DLCName)
        );

        Params.ValidateAndLog();
        return Params;
    }

    public void Cook(DeploymentContext SC, ProjectParams Params)
    {
        string Parameters = "-newcook -BasedOnReleaseVersion=" + AssetRegistry + " -Compressed";

        if (DLCMaps.Length > 0)
        {
            Parameters += " -map=" + DLCMaps;
        }

        string CookDir = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Plugins", DLCName, "Content");
        RunCommandlet(Params.RawProjectPath, "UE4Editor-Cmd.exe", "Cook", String.Format("-CookDir={0} -TargetPlatform={1} {2} -DLCName={3} -SKIPEDITORCONTENT", CookDir, SC.CookPlatform, Parameters, DLCName));
    }

    public void Stage(DeploymentContext SC, ProjectParams Params)
    {
        // Create the version.txt
        string VersionPath = CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", DLCName + "-" + "version.txt");
        CommandUtils.WriteToFile(VersionPath, VersionString);

        // Rename the asset registry file to DLC name
        CommandUtils.RenameFile(CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", "AssetRegistry.bin"),
                                CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", DLCName + "-" + "AssetRegistry.bin"));

        // Put all of the cooked dir into the staged dir
        SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform), "*", true, new[] { "CookedAssetRegistry.json", "CookedIniVersion.txt" }, "", true, !Params.UsePak(SC.StageTargetPlatform));
        

        // Stage and pak it all
        Project.ApplyStagingManifest(Params, SC);

        CommandUtils.DeleteFile_NoExceptions(CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", DLCName + "-" + SC.CookPlatform + ".pak"), true);

        // Rename the pak file to DLC name
        CommandUtils.RenameFile(CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", "UnrealTournament-" + SC.CookPlatform + ".pak"),
                                CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", DLCName + "-" + SC.CookPlatform + ".pak"));
    }

    public static List<DeploymentContext> CreateDeploymentContext(ProjectParams Params, bool InDedicatedServer, bool DoCleanStage = false)
    {
        ParamList<string> ListToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerCookedTargets : Params.ClientCookedTargets;
        var ConfigsToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerConfigsToBuild : Params.ClientConfigsToBuild;

        List<UnrealTargetPlatform> PlatformsToStage = Params.ClientTargetPlatforms;
        if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
        {
            PlatformsToStage = Params.ServerTargetPlatforms;
        }

        bool prefixArchiveDir = false;
        if (PlatformsToStage.Contains(UnrealTargetPlatform.Win32) && PlatformsToStage.Contains(UnrealTargetPlatform.Win64))
        {
            prefixArchiveDir = true;
        }

        List<DeploymentContext> DeploymentContexts = new List<DeploymentContext>();
        foreach (var StagePlatform in PlatformsToStage)
        {
            // Get the platform to get cooked data from, may differ from the stage platform
            UnrealTargetPlatform CookedDataPlatform = Params.GetCookedDataPlatformForClientTarget(StagePlatform);

            if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
            {
                CookedDataPlatform = Params.GetCookedDataPlatformForServerTarget(StagePlatform);
            }

            List<string> ExecutablesToStage = new List<string>();

            string PlatformName = StagePlatform.ToString();
            string StageArchitecture = !String.IsNullOrEmpty(Params.SpecifiedArchitecture) ? Params.SpecifiedArchitecture : "";
            foreach (var Target in ListToProcess)
            {
                foreach (var Config in ConfigsToProcess)
                {
                    string Exe = Target;
                    if (Config != UnrealTargetConfiguration.Development)
                    {
                        Exe = Target + "-" + PlatformName + "-" + Config.ToString() + StageArchitecture;
                    }
                    ExecutablesToStage.Add(Exe);
                }
            }

            string StageDirectory = (Params.Stage || !String.IsNullOrEmpty(Params.StageDirectoryParam)) ? Params.BaseStageDirectory : "";
            string ArchiveDirectory = (Params.Archive || !String.IsNullOrEmpty(Params.ArchiveDirectoryParam)) ? Params.BaseArchiveDirectory : "";
            if (prefixArchiveDir && (StagePlatform == UnrealTargetPlatform.Win32 || StagePlatform == UnrealTargetPlatform.Win64))
            {
                if (Params.Stage)
                {
                    StageDirectory = CombinePaths(Params.BaseStageDirectory, StagePlatform.ToString());
                }
                if (Params.Archive)
                {
                    ArchiveDirectory = CombinePaths(Params.BaseArchiveDirectory, StagePlatform.ToString());
                }
            }

            List<StageTarget> TargetsToStage = new List<StageTarget>();

            //@todo should pull StageExecutables from somewhere else if not cooked
            DeploymentContext SC = new DeploymentContext(Params.RawProjectPath, CmdEnv.LocalRoot,
                StageDirectory,
                ArchiveDirectory,
                Params.CookFlavor,
                Params.GetTargetPlatformInstance(CookedDataPlatform),
                Params.GetTargetPlatformInstance(StagePlatform),
                ConfigsToProcess,
                TargetsToStage,
                ExecutablesToStage,
                InDedicatedServer,
                Params.Cook || Params.CookOnTheFly,
                Params.CrashReporter && !(StagePlatform == UnrealTargetPlatform.Linux && Params.Rocket), // can't include the crash reporter from binary Linux builds
                Params.Stage,
                Params.CookOnTheFly,
                Params.Archive,
                Params.IsProgramTarget,
                Params.HasDedicatedServerAndClient
                );
            Project.LogDeploymentContext(SC);

            // If we're a derived platform make sure we're at the end, otherwise make sure we're at the front

            if (CookedDataPlatform != StagePlatform)
            {
                DeploymentContexts.Add(SC);
            }
            else
            {
                DeploymentContexts.Insert(0, SC);
            }
        }

        return DeploymentContexts;
    }

    public override void ExecuteBuild()
    {
        VersionString = ParseParamValue("Version", "NOVERSION");

        DLCName = ParseParamValue("DLCName", "PeteGameMode");

        // Maps should be in format -maps=DM-DLCMap1+DM-DLCMap2+DM-DLCMap3
        DLCMaps = ParseParamValue("Maps", "");

        // Right now all platform asset registries seem to be the exact same, this may change in the future
        AssetRegistry = ParseParamValue("ReleaseVersion", "UTVersion0");

        ProjectParams Params = GetParams(this, DLCName, AssetRegistry);

        if (ParseParam("build"))
        {
            Project.Build(this, Params);
        }

        // Cook dedicated server configs
        if (Params.DedicatedServer)
        {
            var DeployContextServerList = CreateDeploymentContext(Params, true, true);
            foreach (var SC in DeployContextServerList)
            {
                if (!ParseParam("skipcook"))
                {
                    Cook(SC, Params);
                }
                Stage(SC, Params);
            }
        }

        // Cook client configs
        var DeployClientContextList = CreateDeploymentContext(Params, false, true);
        foreach (var SC in DeployClientContextList)
        {
            if (!ParseParam("skipcook"))
            {
                Cook(SC, Params);
            }
            Stage(SC, Params);
        }
    }
}
