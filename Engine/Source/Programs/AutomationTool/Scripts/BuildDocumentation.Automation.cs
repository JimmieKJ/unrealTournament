// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

class ToolsForDocumentationNode : GUBP.HostPlatformNode
{
    public ToolsForDocumentationNode(GUBP.GUBPBranchConfig InBranchConfig, UnrealTargetPlatform InHostPlatform)
        : base(InHostPlatform)
    {
		AgentSharingGroup = "Documentation" + StaticGetHostPlatformSuffix(InHostPlatform);
    }

    public static string StaticGetFullName(UnrealTargetPlatform InHostPlatform)
    {
		return "ToolsForDocumentation" + StaticGetHostPlatformSuffix(InHostPlatform);
    }

	public override string GetFullName()
    {
        return StaticGetFullName(HostPlatform);
    }

	public override float Priority()
	{
		return -100000.0f;
	}

	public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
	{
		return base.CISFrequencyQuantumShift(BranchConfig) + 6;
	}

	public override void DoBuild(GUBP bp)
	{
		CommandUtils.BuildSolution(CommandUtils.CmdEnv, "Engine/Source/Programs/UnrealDocTool/APIDocTool/APIDocTool.sln", "Development", "x64");
		CommandUtils.BuildSolution(CommandUtils.CmdEnv, "Engine/Source/Programs/UnrealDocTool/UnrealDocTool/UnrealDocTool.sln", "Development", "Any CPU");

		BuildProducts = new List<string>();
		SaveRecordOfSuccessAndAddToBuildProducts();
	}
}
	
class DocumentationNode : GUBP.HostPlatformNode
{
	public DocumentationNode(UnrealTargetPlatform InHostPlatform) : base(InHostPlatform)
	{
		AgentSharingGroup = "Documentation" + GUBP.HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
		AddDependency(ToolsForDocumentationNode.StaticGetFullName(InHostPlatform));
	}

	public override int CISFrequencyQuantumShift(GUBP.GUBPBranchConfig BranchConfig)
	{
		return base.CISFrequencyQuantumShift(BranchConfig) + 3;
	}

	protected void ExecuteApiDocTool(string Arguments, string LogName)
	{
		string ApiDocToolPath = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "Engine/Source/Programs/UnrealDocTool/APIDocTool/APIDocTool/bin/x64/Release/APIDocTool.exe");
		string ApiToolCommandLine = Arguments + " -enginedir=\"" + Path.Combine(CommandUtils.CmdEnv.LocalRoot, "Engine") + "\"" + (CommandUtils.IsBuildMachine? " -buildmachine" : "");
		CommandUtils.RunAndLog(CommandUtils.CmdEnv, ApiDocToolPath, ApiToolCommandLine, LogName);
	}

	public override bool NotifyOnWarnings()
	{
		return false;
	}

	public static void SubmitOutputs(string Description, params string[] FileSpecs)
	{
		if (CommandUtils.P4Enabled)
		{
			int Changelist = CommandUtils.P4.CreateChange(CommandUtils.P4Env.Client, String.Format("{0} from CL#{1}", Description, CommandUtils.P4Env.Changelist));

			foreach (string FileSpec in FileSpecs)
			{
				CommandUtils.P4.Reconcile(Changelist, CommandUtils.CombinePaths(PathSeparator.Slash, CommandUtils.P4Env.ClientRoot, FileSpec));
			}

			if (!CommandUtils.P4.TryDeleteEmptyChange(Changelist))
			{
				if (!GlobalCommandLine.NoSubmit)
				{
					int SubmittedChangelist;
					CommandUtils.P4.Submit(Changelist, out SubmittedChangelist, true, true);
				}
			}
		}
	}
}

class CodeDocumentationNode : DocumentationNode
{
	public CodeDocumentationNode(UnrealTargetPlatform InHostPlatform)
		: base(InHostPlatform)
	{
	}

	public override void DoBuild(GUBP bp)
	{
		ExecuteApiDocTool("-rebuildcode", "APIDocTool-Code");
		SubmitOutputs("Code documentation", "Engine/Documentation/Builds/CodeAPI-*", "Engine/Documentation/CHM/API.chm");
		base.DoBuild(bp);
	}

	public override string GetFullName()
	{
		return StaticGetFullName();
	}

	public static string StaticGetFullName()
	{
		return "CodeDocumentation";
	}
}

class BlueprintDocumentationNode : DocumentationNode
{
	public BlueprintDocumentationNode(UnrealTargetPlatform InHostPlatform)
		: base(InHostPlatform)
	{
		AddDependency(GUBP.RootEditorNode.StaticGetFullName(InHostPlatform));
		AddDependency(GUBP.ToolsNode.StaticGetFullName(InHostPlatform));
	}

	public override void DoBuild(GUBP bp)
	{
		ExecuteApiDocTool("-rebuildblueprint", "APIDocTool-Blueprint");
		SubmitOutputs("Blueprint documentation", "Engine/Documentation/Builds/BlueprintAPI-*", "Engine/Documentation/CHM/BlueprintAPI.chm");
		base.DoBuild(bp);
	}

	public override string GetFullName()
	{
		return StaticGetFullName();
	}

	public static string StaticGetFullName()
	{
		return "BlueprintDocumentation";
	}
}

public class DocumentationNodeAdder : GUBP.GUBPNodeAdder
{
	public override void AddNodes(GUBP bp, GUBP.GUBPBranchConfig BranchConfig, UnrealTargetPlatform InHostPlatform, List<UnrealTargetPlatform> InActivePlatforms)
	{
		if(InHostPlatform == UnrealTargetPlatform.Win64 && !BranchConfig.BranchOptions.bNoDocumentation)
		{
			BranchConfig.AddNode(new ToolsForDocumentationNode(BranchConfig, InHostPlatform));
			BranchConfig.AddNode(new CodeDocumentationNode(InHostPlatform));
			BranchConfig.AddNode(new BlueprintDocumentationNode(InHostPlatform));
		}
	}
}
