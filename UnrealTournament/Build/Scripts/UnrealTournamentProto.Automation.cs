// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.MCP.Automation;
using EpicGames.MCP.Config;

public class UnrealTournamentBuild
{
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

    static BuildPatchToolStagingInfo UTBuildPatchToolStagingInfo = null;

    public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand)
    {
        if (UTBuildPatchToolStagingInfo == null)
        {
            UTBuildPatchToolStagingInfo = new BuildPatchToolStagingInfo(InOwnerCommand, "UnrealTournament", "UnrealTournament", 1, CreateBuildVersion(), MCPPlatform.Windows, "UnrealTournament");
        }
        return UTBuildPatchToolStagingInfo;
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
        Platform ClientPlatformInst = Params.ClientTargetPlatformInstances[0];
        string TargetCook = ClientPlatformInst.GetCookPlatform(false, Params.HasDedicatedServerAndClient, "");

        string RawImagePath = CombinePaths(UnrealTournamentBuild.GetArchiveDir(), TargetCook);
        string RawImageManifest = CombinePaths(RawImagePath, "Manifest_NonUFSFiles.txt");

        if (!FileExists(RawImageManifest))
        {
            throw new AutomationException("BUILD FAILED: build is missing or did not complete because this file is missing: {0}", RawImageManifest);
        }

        // run the patch tool
        BuildPatchToolBase.Get().Execute(
        new BuildPatchToolBase.PatchGenerationOptions
        {
            StagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this),
            BuildRoot = RawImagePath,
            FileIgnoreList = CommandUtils.CombinePaths(RawImagePath, "Manifest_DebugFiles.txt"),
            AppLaunchCmd = @".\UnrealTournament\Binaries\Win64\UnrealTournament.exe",
            AppLaunchCmdArgs = "-pak",
            AppChunkType = BuildPatchToolBase.ChunkType.Chunk,
        });

        var BuildLocation = CombinePaths(Params.StageDirectoryParam, TargetCook);
        if (!DirectoryExists(BuildLocation))
        {
            throw new BuildException("Build directory {0} does not exist.", BuildLocation);
        }
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