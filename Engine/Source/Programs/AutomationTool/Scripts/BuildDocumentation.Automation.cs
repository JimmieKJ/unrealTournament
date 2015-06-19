// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

class ToolsForDocumentationNode : GUBP.CompileNode
{
    public ToolsForDocumentationNode(UnrealTargetPlatform InHostPlatform)
        : base(InHostPlatform, false)
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

	public override int CISFrequencyQuantumShift(GUBP bp)
	{
		return base.CISFrequencyQuantumShift(bp) + 3;
	}

	public override UE4Build.BuildAgenda GetAgenda(GUBP bp)
    {
        var Agenda = new UE4Build.BuildAgenda();
		Agenda.DotNetSolutions.Add("Engine/Source/Programs/UnrealDocTool/APIDocTool/APIDocTool.sln");
		Agenda.DotNetSolutions.Add("Engine/Source/Programs/UnrealDocTool/UnrealDocTool/UnrealDocTool.sln");
        return Agenda;
    }
}
	
class DocumentationNode : GUBP.GUBPNode
{
	public DocumentationNode(UnrealTargetPlatform InHostPlatform)
	{
		AgentSharingGroup = "Documentation" + GUBP.HostPlatformNode.StaticGetHostPlatformSuffix(InHostPlatform);
		AddDependency(ToolsForDocumentationNode.StaticGetFullName(InHostPlatform));
	}

	public override int CISFrequencyQuantumShift(GUBP bp)
	{
		return base.CISFrequencyQuantumShift(bp) + 3;
	}

	protected void ExecuteApiDocTool(string Arguments, string LogName)
	{
		string ApiDocToolPath = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "Engine/Source/Programs/UnrealDocTool/APIDocTool/APIDocTool/bin/x64/Release/APIDocTool.exe");
		string ApiToolCommandLine = Arguments + " -enginedir=\"" + Path.Combine(CommandUtils.CmdEnv.LocalRoot, "Engine") + "\"" + (CommandUtils.IsBuildMachine? " -buildmachine" : "");
		CommandUtils.RunAndLog(CommandUtils.CmdEnv, ApiDocToolPath, ApiToolCommandLine, LogName);
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
	public override void AddNodes(GUBP bp, UnrealTargetPlatform InHostPlatform)
	{
		if(InHostPlatform == UnrealTargetPlatform.Win64 && !bp.BranchOptions.bNoDocumentation)
		{
			bp.AddNode(new ToolsForDocumentationNode(InHostPlatform));
			bp.AddNode(new CodeDocumentationNode(InHostPlatform));
			bp.AddNode(new BlueprintDocumentationNode(InHostPlatform));
		}
	}
}
