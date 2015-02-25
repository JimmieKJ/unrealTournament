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

static class UnrealTournamentIEnumerableExtensions
{
	/// <summary>
	/// Generates a string suitable for debugging a list of objects using ToString(). Lists one per line with the prefix string on the first line.
	/// </summary>
	/// <param name="prefix">Prefix string to print along with the list of BuildInfos.</param>
	/// <param name="buildInfos"></param>
	/// <returns>the resulting debug string</returns>
	static public string CreateDebugList<T>(this IEnumerable<T> objects, string prefix)
	{
		return objects.Aggregate(new StringBuilder(prefix), (sb, obj) => sb.AppendFormat("\n    {0}", obj.ToString())).ToString();
	}
}

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


    public static UnrealTournamentEditorAppName EditorBranchNameToAppName(string BranchName)
    {
        UnrealTournamentEditorAppName AppName = UnrealTournamentEditorAppName.UnrealTournamentEditor;

        if (BranchName.Equals("UE4-UT"))
        {
            AppName = UnrealTournamentEditorAppName.UnrealTournamentEditor;
        }

        return AppName;
    }

    /// <summary>
    /// Enum that defines the MCP backend-compatible platform
    /// </summary>
    public enum UnrealTournamentEditorAppName
    {
        /// Public app
        UnrealTournamentEditor,
    }

	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentAppName AppName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
	}
    
	// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
	public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName)
	{
		// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
		return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
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


    // Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentEditorAppName AppName)
    {
        // Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
        return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
    }

    // Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
    public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName)
    {
        // Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
        return new BuildPatchToolStagingInfo(InOwnerCommand, EditorBranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
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
        
        var BranchName = "";
        if (CommandUtils.P4Enabled)
        {
            BranchName = CommandUtils.P4Env.BuildRootP4;
        }

        if (ParseParam("mac"))
        {
            {
                // verify the files we need exist first
                string RawImagePathMac = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "MacNoEditor");
                string RawImageManifestMac = CombinePaths(RawImagePathMac, "Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifestMac))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifestMac);
                }

                var StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Mac, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePathMac,
                    AppLaunchCmd = "./UnrealTournament/Binaries/Mac/UnrealTournament-Mac-Test.app",
                    AppLaunchCmdArgs = "",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Mac build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting Unreal Tournament for Mac to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Latest in MCP.");
                string LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Latest", MCPPlatform.Mac);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
                // For backwards compatibility, also label as Production-Latest
                LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Production-Latest", MCPPlatform.Mac);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
            }

            // EDITOR BUILD
            {
                // verify the files we need exist first
                string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "MacEditor");
                string RawImageManifest = CombinePaths(RawImagePath, "Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
                }

                var StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Mac, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePath,
                    AppLaunchCmd = @"./Engine/Binaries/Mac/UE4Editor.app",
                    AppLaunchCmdArgs = "UnrealTournament",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Editor build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting UnrealTournament Editor for Mac to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Latest in MCP.");
                string LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Latest", MCPPlatform.Mac);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
                // For backwards compatibility, also label as Production-Latest
                LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Production-Latest", MCPPlatform.Mac);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
            }
        }
        else
        {
            // GAME BUILD
            {
                // verify the files we need exist first
                string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "Win64", "WindowsNoEditor");
                string RawImageManifest = CombinePaths(RawImagePath, "Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
                }

                var StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Win64, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePath,
                    FileIgnoreList = CommandUtils.CombinePaths(RawImagePath, "Manifest_DebugFiles.txt"),
                    AppLaunchCmd = @".\Engine\Binaries\Win64\UE4-Win64-Test.exe",
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
            }

            // EDITOR BUILD
            {
                // verify the files we need exist first
                string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), "WindowsEditor");
                string RawImageManifest = CombinePaths(RawImagePath, "Manifest_NonUFSFiles.txt");

                if (!FileExists(RawImageManifest))
                {
                    throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
                }

                var StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, UnrealTargetPlatform.Win64, BranchName);

                // run the patch tool
                BuildPatchToolBase.Get().Execute(
                new BuildPatchToolBase.PatchGenerationOptions
                {
                    StagingInfo = StagingInfo,
                    BuildRoot = RawImagePath,
                    AppLaunchCmd = @".\Engine\Binaries\Win64\UE4Editor.exe",
                    AppLaunchCmdArgs = "UnrealTournament",
                    AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
                });

                // post the Editor build to build info service on gamedev
                string McpConfigName = "MainGameDevNet";
                CommandUtils.Log("Posting UnrealTournament Editor for Windows to MCP.");
                BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo);
                CommandUtils.Log("Labeling new build as Latest in MCP.");
                string LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Latest", MCPPlatform.Windows);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
                // For backwards compatibility, also label as Production-Latest
                LatestLabelName = BuildInfoPublisherBase.Get().GetLabelWithPlatform("Production-Latest", MCPPlatform.Windows);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LatestLabelName, McpConfigName);
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
				
		var Params = new ProjectParams
		(
			// Shared
            RawProjectPath: CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject"),

			// Build
			EditorTargets: new ParamList<string>("UnrealTournamentEditor", "BuildPatchTool"),
			ClientCookedTargets: new ParamList<string>("UnrealTournament"),
			ServerCookedTargets: new ParamList<string>("UnrealTournamentServer"),

            ClientConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Test },
            ServerConfigsToBuild: new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development },
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
            StageNonMonolithic: true,
            CreateReleaseVersion: "UTVersion0",
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
            var ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);
            var SavedPath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Saved", "Cooked");

            var Platforms = new string[] { "WindowsNoEditor", "MacNoEditor", "LinuxServer", "WindowsServer", "LinuxNoEditor" };

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
                var ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);

                var Platforms = new string[] { "WindowsNoEditor", "MacNoEditor", "LinuxServer", "WindowsServer", "LinuxNoEditor" };

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
        CopyAssetRegistryFilesFromSavedCookedToReleases(Params);
        SubmitAssetRegistryFilesToPerforce(Params);
        EditorProject.CopyEditorBuildToStagingDirectory(Params);
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

            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(UnrealTargetPlatform.Mac, GameProj, UnrealTargetPlatform.Mac));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win64));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Win32));
            AddDependency(GUBP.GamePlatformCookedAndCompiledNode.StaticGetFullName(HostPlatform, GameProj, UnrealTargetPlatform.Linux));

            var Chunker = bp.Branch.FindProgram("BuildPatchTool");
            AddDependency(GUBP.EditorGameNode.StaticGetFullName(HostPlatform, GameProj));
            AddDependency(GUBP.EditorAndToolsNode.StaticGetFullName(HostPlatform));
            AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(HostPlatform, Chunker));

            // Make sure we have the mac version of Chunker as well
            AddDependency(GUBP.EditorGameNode.StaticGetFullName(UnrealTargetPlatform.Mac, GameProj));
            AddDependency(GUBP.EditorAndToolsNode.StaticGetFullName(UnrealTargetPlatform.Mac));
            AddDependency(GUBP.SingleInternalToolsNode.StaticGetFullName(UnrealTargetPlatform.Mac, Chunker));
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

    public class UnrealTournamentChunkNode : GUBP.HostPlatformNode
    {
        BranchInfo.BranchUProject GameProj;

        public UnrealTournamentChunkNode(GUBP bp, BranchInfo.BranchUProject InGameProj, UnrealTargetPlatform InHostPlatform)
            : base(InHostPlatform)
        {
            GameProj = InGameProj;

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
                    bp.AddNode(new UnrealTournamentChunkNode(bp, GameProj, InHostPlatform));
                }
                else if (InHostPlatform == UnrealTargetPlatform.Mac)
                {
                    bp.AddNode(new UnrealTournamentChunkNode(bp, GameProj, InHostPlatform));
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
[Help("Products", "Comma-separated list of products to promote. Allowable values are \"GameClient\" and \"Editor\".")]
[Help("Platforms", "Optional.  Comma-separated list of platforms to promote.  Default is all platforms.")]
[Help("SkipLabeling", "Optional.  Perform the promotion step but don't apply the new label to the build.")]
[Help("OnlyLabel", "Optional.  Perform the labeling step but don't perform any additional promotion actions.")]
[Help("TestLivePromotion", "Optional.  Use fake production labels and don't perform any actions that would go out to the public, eg. installer redirect.")]
// NOTE: PROMOTION JOB IS ONLY EVER RUN OUT OF UE4-UT, REGARDLESS OF WHICH BRANCH'S BUILD IS BEING PROMOTED
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
			throw new AutomationException(InvalidProducts.CreateDebugList("The following product names are invalid:"));
		}
		var bShouldPromoteGameClient = Products.Contains("GameClient");
		var bShouldPromoteEditor = Products.Contains("Editor");

        UnrealTournamentBuild.UnrealTournamentAppName FromApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev;
        UnrealTournamentBuild.UnrealTournamentAppName ToApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev;
		
		if (bShouldPromoteGameClient)
		{
			// We're promoting the game client, so we'll work out the app names of the game client to promote from and to

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

		// Determine the name for the editor app (currently hardcoded, but we may want to parameterize this later)
		var EditorAppName = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditor;

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
		string StagingMcpConfigString = "StageNet";
		string ProdMcpConfigString = "ProdNet";
		string ProdAndStagingMcpConfigString = StagingMcpConfigString + "," + ProdMcpConfigString;
		string ProdStagingAndGameDevMcpConfigString = ProdMcpConfigString + "," + GameDevMcpConfigString;

		// Verify build meets prior state criteria for this promotion
		// If this label will go to Prod, then make sure the build manifests are all staged to the Prod CDN already
		if (DestinationLabel != "Staged" && (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel))
		{
			// Look for each build's manifest
			foreach (var Platform in Platforms)
			{
				if (bShouldPromoteGameClient)
				{
					BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);

					// Check for the manifest on the prod CDN
					Log("Verifying manifest for prod promotion of Unreal Tournament {0} {1} was already staged to the Prod CDN", BuildVersion, Platform);
					bool bWasManifestFound = BuildInfoPublisherBase.Get().IsManifestOnProductionCDN(StagingInfo);
					if (!bWasManifestFound)
					{
						string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
						throw new AutomationException("Promotion to Prod requires the build first be staged to the Prod CDN. Manifest {0} not found for promotion to label {1}"
							, StagingInfo.ManifestFilename
							, DestinationLabelWithPlatform);
					}
				}
				if (bShouldPromoteEditor)
				{
					BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, EditorAppName);

					// Check for the manifest on the prod CDN
					Log("Verifying manifest for prod promotion of Unreal Tournament Editor {0} {1} was already staged to the Prod CDN", BuildVersion, Platform);
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
					if (bShouldPromoteGameClient)
					{
						BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
						// Publish staging info up to production BuildInfo service
						{
							CommandUtils.Log("Posting {0} to MCP.", FromApp);
							foreach (var McpConfigString in ProdAndStagingMcpConfigString.Split(','))
							{
								BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, McpConfigString);
							}
						}
						// Copy chunks to production CDN
						{
							Log("Promoting game chunks to production CDN");
							BuildInfoPublisherBase.Get().CopyChunksToProductionCDN(StagingInfo);
							Log("DONE Promoting game chunks to production CDN");
						}
					}
					if (bShouldPromoteEditor)
					{
						BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, EditorAppName);
						// Publish staging info up to production BuildInfo service
						{
							CommandUtils.Log("Posting {0} to MCP.", EditorAppName);
							foreach (var McpConfigString in ProdAndStagingMcpConfigString.Split(','))
							{
								BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, McpConfigString);
							}
						}
						// Copy chunks to production CDN
						{
							Log("Promoting editor chunks to production CDN");
							BuildInfoPublisherBase.Get().CopyChunksToProductionCDN(StagingInfo);
							Log("DONE Promoting editor chunks to production CDN");
						}
					}
				}
			}
			else if (DestinationLabel == "Live")
			{
				// Apply rollback label to the build currently labeled Live
				foreach (var Platform in Platforms)
				{
					if (bShouldPromoteGameClient)
					{
						BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
						string LiveLabelWithPlatform = "Live" + "-" + Platform;
						// Request Live-[platform] label for this app (actually Production-[platform] until deprecated)
						string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(StagingInfo.AppName, LiveLabelWithPlatform, ProdMcpConfigString);
						if (String.IsNullOrEmpty(LiveBuildVersionString))
						{
							Log("No current Live game build found for {0}, continuing without applying a Rollback label.", LiveLabelWithPlatform);
						}
						else if (LiveBuildVersionString.EndsWith("-" + Platform))
						{
							Log("Identified current Live game build as {0}", LiveBuildVersionString);
							// Take off platform so it can fit in the FEngineVersion struct
							string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
							if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
							{
								// Label the old Live build as "Rollback"
								LabelBuildForBackwardsCompat(LiveBuildVersionStringWithoutPlatform, "Rollback", new List<MCPPlatform>() { Platform }, ProdStagingAndGameDevMcpConfigString, FromApp);
							}
							else
							{
								Log("Would relabel current Live {0} build as {1} again.  Not applying a Rollback label to avoid losing track of current Rollback build.", StagingInfo.AppName, LiveLabelWithPlatform);
							}
						}
						else
						{
							throw new AutomationException("Current live game buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
						}
					}
					if (bShouldPromoteEditor)
					{
						BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, EditorAppName);
						string LiveLabelWithPlatform = "Live" + "-" + Platform;
						// Request Live-[platform] label for this app (actually Production-[platform] until deprecated)
						string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(StagingInfo.AppName, LiveLabelWithPlatform, ProdMcpConfigString);
						if (String.IsNullOrEmpty(LiveBuildVersionString))
						{
							Log("No current Live editor build found for {0}, continuing without applying a Rollback label.", LiveLabelWithPlatform);
						}
						else if (LiveBuildVersionString.EndsWith("-" + Platform))
						{
							Log("Identified current Live editor build as {0}", LiveBuildVersionString);
							// Take off platform so it can fit in the FEngineVersion struct
							string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
							if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
							{
								// Label the old Live build as "Rollback"
								LabelBuildForBackwardsCompat(LiveBuildVersionStringWithoutPlatform, "Rollback", new List<MCPPlatform>() { Platform }, ProdStagingAndGameDevMcpConfigString, EditorAppName);
							}
							else
							{
								Log("Would relabel current Live {0} build as {1} again.  Not applying a Rollback label to avoid losing track of current Rollback build.", StagingInfo.AppName, LiveLabelWithPlatform);
							}
						}
						else
						{
							throw new AutomationException("Current live editor buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
						}
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
			if (bShouldPromoteGameClient)
			{
				Log("Labeling game build {0} with label {1} across all platforms", BuildVersion, DestinationLabel);

				// For non-prod builds, do gamedev only
				string LabelInMcpConfigs = GameDevMcpConfigString;
				if (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel)
				{
					// For prod labels, do both BI services and also dual-label with the entitlement prefix
					LabelInMcpConfigs = ProdStagingAndGameDevMcpConfigString;
				}
				LabelBuildForBackwardsCompat(BuildVersion, DestinationLabel, Platforms, LabelInMcpConfigs, FromApp);
				// If labeling as the Live build, also create a new archive label based on the date
				if (DestinationLabel == "Live")
				{
					string DateFormatString = "yyyy.MM.dd.HH.mm";
					string ArchiveDateString = DateTime.Now.ToString(DateFormatString);
					string ArchiveLabel = "Archived" + ArchiveDateString;
					LabelBuildForBackwardsCompat(BuildVersion, ArchiveLabel, Platforms, LabelInMcpConfigs, FromApp);
				}
			}
			if (bShouldPromoteEditor)
			{
				Log("Labeling editor build {0} with label {1} across all platforms", BuildVersion, DestinationLabel);

				// For non-prod builds, do gamedev only
				string LabelInMcpConfigs = GameDevMcpConfigString;
				if (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel)
				{
					// For prod labels, do both BI services and also dual-label with the entitlement prefix
					LabelInMcpConfigs = ProdStagingAndGameDevMcpConfigString;
				}
				LabelBuildForBackwardsCompat(BuildVersion, DestinationLabel, Platforms, LabelInMcpConfigs, EditorAppName);
				// If labeling as the Live build, also create a new archive label based on the date
				if (DestinationLabel == "Live")
				{
					string DateFormatString = "yyyy.MM.dd.HH.mm";
					string ArchiveDateString = DateTime.Now.ToString(DateFormatString);
					string ArchiveLabel = "Archived" + ArchiveDateString;
					LabelBuildForBackwardsCompat(BuildVersion, ArchiveLabel, Platforms, LabelInMcpConfigs, EditorAppName);
				}
			}
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


	private void LabelBuildForBackwardsCompat(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentEditorAppName AppName)
	{
		// Label it normally
		LabelBuild(BuildVersion, DestinationLabel, Platforms, McpConfigNames, AppName);

		// If the label is Live, also apply the empty label for backwards-compat until this can be deprecated in favor of "Live"
		if (DestinationLabel.Equals("Live"))
		{
			string Label = "Production";
			LabelBuild(BuildVersion, Label, Platforms, McpConfigNames, AppName);
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
	/// <param name="AppName">Which appname is associated with this build</param>
	private void LabelBuild(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentEditorAppName AppName)
	{
		foreach (string McpConfigName in McpConfigNames.Split(','))
		{
			foreach (var Platform in Platforms)
			{
				BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, AppName);
				string LabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
				BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelWithPlatform, McpConfigName);
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
// NOTE: PROMOTION JOB IS ONLY EVER RUN OUT OF UE4-UT, REGARDLESS OF WHICH BRANCH'S BUILD IS BEING PROMOTED
class UnrealTournamentEditor_PromoteBuild : BuildCommand
{
    public override void ExecuteBuild()
    {
        Log("************************* UnrealTournamentEditor_PromoteBuild");
        string BuildVersion = ParseParamValue("BuildVersion");
        if (String.IsNullOrEmpty(BuildVersion))
        {
            throw new AutomationException("BuildVersion is a required parameter");
        }

        UnrealTournamentBuild.UnrealTournamentEditorAppName FromApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditor;
        UnrealTournamentBuild.UnrealTournamentEditorAppName ToApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditor;
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
        string StagingMcpConfigString = "StageNet";
        string ProdMcpConfigString = "ProdNet";
        string ProdAndStagingMcpConfigString = StagingMcpConfigString + "," + ProdMcpConfigString;
        string ProdStagingAndGameDevMcpConfigString = ProdMcpConfigString + "," + GameDevMcpConfigString;

        // Verify build meets prior state criteria for this promotion
        // If this label will go to Prod, then make sure the build manifests are all staged to the Prod CDN already
        if (DestinationLabel != "Staged" && (ProdLabels.Contains(DestinationLabel) || bUseProdCustomLabel))
        {
            // Look for each build's manifest
            foreach (var Platform in Platforms)
            {
                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);

                // Check for the manifest on the prod CDN
                Log("Verifying manifest for prod promotion of Unreal Tournament Editor {0} {1} was already staged to the Prod CDN", BuildVersion, Platform);
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
                    BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
                    // Publish staging info up to production BuildInfo service
                    {
                        CommandUtils.Log("Posting {0} to MCP.", FromApp);
                        foreach (var McpConfigString in ProdAndStagingMcpConfigString.Split(','))
                        {
                            BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, McpConfigString);
                        }
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
                    BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
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
                            LabelBuildForBackwardsCompat(LiveBuildVersionStringWithoutPlatform, "Rollback", new List<MCPPlatform>() { Platform }, ProdStagingAndGameDevMcpConfigString, FromApp);
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
                LabelInMcpConfigs = ProdStagingAndGameDevMcpConfigString;
            }
            LabelBuildForBackwardsCompat(BuildVersion, DestinationLabel, Platforms, LabelInMcpConfigs, FromApp);
            // If labeling as the Live build, also create a new archive label based on the date
            if (DestinationLabel == "Live")
            {
                string DateFormatString = "yyyy.MM.dd.HH.mm";
                string ArchiveDateString = DateTime.Now.ToString(DateFormatString);
                string ArchiveLabel = "Archived" + ArchiveDateString;
                LabelBuildForBackwardsCompat(BuildVersion, ArchiveLabel, Platforms, LabelInMcpConfigs, FromApp);
            }
        }

        Log("************************* UnrealTournament_PromoteBuild completed");
    }


    private void LabelBuildForBackwardsCompat(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentEditorAppName FromApp)
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
    private void LabelBuild(string BuildVersion, string DestinationLabel, List<MCPPlatform> Platforms, string McpConfigNames, UnrealTournamentBuild.UnrealTournamentEditorAppName FromApp)
    {
        foreach (string McpConfigName in McpConfigNames.Split(','))
        {
            foreach (var Platform in Platforms)
            {
                BuildPatchToolStagingInfo StagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromApp);
                string LabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
                BuildInfoPublisherBase.Get().LabelBuild(StagingInfo, LabelWithPlatform, McpConfigName);
            }
        }
    }
}

public class MakeUTDLC : BuildCommand
{
    public string DLCName;
    public string DLCMaps;
    public string AssetRegistry;

    static public ProjectParams GetParams(BuildCommand Cmd, string DLCName)
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
            Command: Cmd,
            // Shared
            Cook: true,
            Stage: true,
            Pak: true,
            StageNonMonolithic: true,
            RawProjectPath: CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject"),
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
        RunCommandlet("UnrealTournament", "UE4Editor-Cmd.exe", "Cook", String.Format("-CookDir={0} -TargetPlatform={1} {2} -DLCName={3}", CookDir, SC.CookPlatform, Parameters, DLCName));
    }

    public void Stage(DeploymentContext SC, ProjectParams Params)
    {
        // Rename the asset registry file to DLC name
        CommandUtils.RenameFile(CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", "AssetRegistry.bin"),
                                CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", DLCName + "-" + "AssetRegistry.bin"));

        // Put all of the cooked dir into the staged dir
        SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform), "*", true, new[] { "CookedAssetRegistry.json", "CookedIniVersion.txt" }, "", true, !Params.UsePak(SC.StageTargetPlatform));
        

        // Stage and pak it all
        Project.ApplyStagingManifest(Params, SC);

        // Rename the pak file to DLC name
        CommandUtils.RenameFile(CombinePaths(SC.ProjectRoot, "Saved", "StagedBuilds", DLCName, SC.CookPlatform, "UnrealTournament", "Content", "Paks", "UnrealTournament-" + SC.CookPlatform + ".pak"),
                                CombinePaths(SC.ProjectRoot, "Saved", "StagedBuilds", DLCName, SC.CookPlatform, "UnrealTournament", "Content", "Paks", DLCName + "-" + SC.CookPlatform + ".pak"));
    }

    public override void ExecuteBuild()
    {
        DLCName = ParseParamValue("DLCName", "PeteGameMode");

        // Maps should be in format -maps=DM-DLCMap1+DM-DLCMap2+DM-DLCMap3
        DLCMaps = ParseParamValue("Maps", "");

        // Right now all platform asset registries seem to be the exact same, this may change in the future
        AssetRegistry = ParseParamValue("ReleaseVersion", "UTVersion0");

        var Params = GetParams(this, DLCName);

        if (ParseParam("build"))
        {
            Project.Build(this, Params);
        }

        // Cook dedicated server configs
        if (Params.DedicatedServer)
        {
            var DeployContextServerList = Project.CreateDeploymentContext(Params, true, true);
            foreach (var SC in DeployContextServerList)
            {
                Cook(SC, Params);
                Stage(SC, Params);
            }
        }

        // Cook client configs
        var DeployClientContextList = Project.CreateDeploymentContext(Params, false, true);
        foreach (var SC in DeployClientContextList)
        {
            Cook(SC, Params);
            Stage(SC, Params);
        }
    }
}

public class StageUTEditor : BuildCommand
{
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
            Command: Cmd,
            // Shared
            Cook: true,
            Stage: true,
            Pak: true,
            StageNonMonolithic: true,
            RawProjectPath: CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject"),
            StageDirectoryParam: UnrealTournamentBuild.GetArchiveDir()
        );

        Params.ValidateAndLog();
        return Params;
    }

    public override void ExecuteBuild()
    {
        var Params = GetParams(this);

        EditorProject.CopyEditorBuildToStagingDirectory(Params, ParseParam("nomac"));
    }
}

public partial class EditorProject : Project
{
    public static void CopyEditorBuildToStagingDirectory(ProjectParams Params, bool bNoMac = false)
    {
        if (Params.Stage && !Params.SkipStage)
        {
            var DeployContextList = CreateEditorDeploymentContext(Params, false, true, bNoMac);
            foreach (var SC in DeployContextList)
            {
                CreateEditorStagingManifest(Params, SC);
                ApplyStagingManifest(Params, SC);
            }
        }
    }

    public static List<DeploymentContext> CreateEditorDeploymentContext(ProjectParams Params, bool InDedicatedServer, bool DoCleanStage = false, bool bNoMac = false)
    {
        ParamList<string> ListToProcess = new ParamList<string>("UnrealTournament");
        var ConfigsToProcess = new List<UnrealTargetConfiguration>() { UnrealTargetConfiguration.Development };

        List<UnrealTargetPlatform> PlatformsToStage = new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win64 };

        if (!bNoMac)
        {
            PlatformsToStage.Add(UnrealTargetPlatform.Mac);
        }
            
        List<DeploymentContext> DeploymentContexts = new List<DeploymentContext>();
        foreach (var StagePlatform in PlatformsToStage)
        {
            // Get the platform to get cooked data from, may differ from the stage platform
            UnrealTargetPlatform CookedDataPlatform = Params.GetCookedDataPlatformForClientTarget(StagePlatform);

            List<string> ExecutablesToStage = new List<string>();

            string PlatformName = StagePlatform.ToString();
            foreach (var Target in ListToProcess)
            {
                foreach (var Config in ConfigsToProcess)
                {
                    string Exe = Target;
                    if (Config != UnrealTargetConfiguration.Development)
                    {
                        Exe = Target + "-" + PlatformName + "-" + Config.ToString();
                    }
                    ExecutablesToStage.Add(Exe);
                }
            }

            string StageDirectory = (Params.Stage || !String.IsNullOrEmpty(Params.StageDirectoryParam)) ? Params.BaseStageDirectory : "";
            string ArchiveDirectory = (Params.Archive || !String.IsNullOrEmpty(Params.ArchiveDirectoryParam)) ? Params.BaseArchiveDirectory : "";

            if (StagePlatform == UnrealTargetPlatform.Mac)
            {
                StageDirectory = CommandUtils.CombinePaths(StageDirectory, "MacEditor");
            }
            else
            {
                StageDirectory = CommandUtils.CombinePaths(StageDirectory, "WindowsEditor");
            }

            //@todo should pull StageExecutables from somewhere else if not cooked
            var SC = new DeploymentContext(Params.RawProjectPath, CmdEnv.LocalRoot,
                StageDirectory,
                ArchiveDirectory,
                Params.CookFlavor,
                Params.GetTargetPlatformInstance(CookedDataPlatform),
                Params.GetTargetPlatformInstance(StagePlatform),
                ConfigsToProcess,
                ExecutablesToStage,
                InDedicatedServer,
                false,
                Params.CrashReporter && !(StagePlatform == UnrealTargetPlatform.Linux && Params.Rocket), // can't include the crash reporter from binary Linux builds
                Params.Stage,
                Params.CookOnTheFly,
                Params.Archive,
                false,
                Params.HasDedicatedServerAndClient
                );
            LogDeploymentContext(SC);

            // If we're a derived platform make sure we're at the end, otherwise make sure we're at the front

            //DeploymentContexts.Add(SC);
            DeploymentContexts.Insert(0, SC);
        }

        return DeploymentContexts;
    }

    public static void CreateEditorStagingManifest(ProjectParams Params, DeploymentContext SC)
    {
        if (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Mac)
        {
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Mac/UE4Editor.app"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Mac"), "ShaderCompileWorker*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Mac/CrashReportClient.app"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Mac"), "UnrealPak", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Mac"), "UnrealLightmass*", true, new string[] { }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/DotNet/"), "*", true, new string[] { "*.pdb" }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/ICU/icu4c-53_1", SC.PlatformDir), "*.dylib", true, null, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Mono/", SC.PlatformDir), "*", true, null, null, false, false, null, false);
                        
            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/CEF3/", SC.PlatformDir), "*", true, null, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins/Runtime/ExampleDeviceProfileSelector"), "*", true, new string[] { "*-Debug.dylib" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins/2D/Paper2D"), "*", true, new string[] { "*-Debug.dylib" }, null, false, false, null, false);
            
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Build/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Config/"), "*", true, new string[] { }, null, false, false, null, false, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Content/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Shaders/"), "*", true, new string[] { }, null, false, false, null, false);
            
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/"), "*.uproject", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Binaries/Mac/"), "UE4Editor-*dylib", true, new string[] { "*-Debug.dylib" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Plugins/"), "*", true, new string[] { "*-Debug.dylib" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Build/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Config/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/Localization/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/RestrictedAssets/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/Splash/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Releases/"), "*", true, new string[] { }, null, false, false, null, false);

            // THIS IS THE ONE EXCEPTION TO NO REDIST AND NOT FOR LICENSEES
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/DotNET/AutomationScripts/NoRedist/"), "UnrealTournament.Automation.dll", true, new string[] { "*.pdb" }, null, false, false, null, true);
        }
        else
        {
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "UE4Editor*", true, new string[] { "*-Debug.dll", "*.pdb", "*Simplygon*" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "AgentInterface.dll", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "libfbxsdk.dll", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "ShaderCompileWorker*", true, new string[] { "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "CrashReportClient*", true, new string[] { "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "UnrealPak*", true, new string[] { "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/Win64"), "UnrealLightmass*", true, new string[] { "*.pdb" }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/DotNet"), "*", true, new string[] { "*.pdb" }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/ICU/icu4c-53_1", SC.PlatformDir, "VS2013"), "*.dll", true, null, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/APEX-1.3", SC.PlatformDir, "VS2013"), "*.dll", true, new string[] { "*DEBUG*.*", "*CHECKED*.*" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/PhysX/PhysX-3.3", SC.PlatformDir, "VS2013"), "*.dll", true, new string[] { "*DEBUG*.*", "*CHECKED*.*" }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Ogg", SC.PlatformDir), "*.dll", true, null, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Vorbis", SC.PlatformDir), "*.dll", true, null, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/nvTextureTools", SC.PlatformDir), "*.dll", true, null, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins/Runtime/ExampleDeviceProfileSelector/"), "*", true, new string[] { "*-Debug.dll", "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Plugins/2D/Paper2D/"), "*", true, new string[] { "*-Debug.dll", "*.pdb" }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Build/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Config/"), "*", true, new string[] { }, null, false, false, null, false, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Content/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Shaders/"), "*", true, new string[] { }, null, false, false, null, false);

            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/"), "*.uproject", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Binaries/Win64/"), "UE4Editor-*", true, new string[] { "", "*-Debug.dll", "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Plugins/"), "*", true, new string[] { "*-Debug.dll", "*.pdb" }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Build/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Config/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/Localization/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/RestrictedAssets/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Content/Splash/"), "*", true, new string[] { }, null, false, false, null, false);
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "UnrealTournament/Releases/"), "*", true, new string[] { }, null, false, false, null, false);

            // THIS IS THE ONE EXCEPTION TO NO REDIST AND NOT FOR LICENSEES
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/DotNET/AutomationScripts/NoRedist/"), "UnrealTournament.Automation.dll", true, new string[] { "*.pdb" }, null, false, false, null, true);
        }
    }
}