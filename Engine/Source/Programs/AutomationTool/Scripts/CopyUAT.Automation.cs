// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

/// <summary>
/// Copies all UAT and UBT build products to a directory
/// </summary>
public class CopyUAT : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get the output directory
		string TargetDir = ParseParamValue("TargetDir");
		if(TargetDir == null)
		{
			throw new AutomationException("Missing -Target argument to CopyUAT");
		}

		// Construct a dummy UE4Build object to get a list of the UAT and UBT build products
		UE4Build Build = new UE4Build(this);
		Build.AddUATFilesToBuildProducts();
		if(ParseParam("WithLauncher"))
		{
			Build.AddUATLauncherFilesToBuildProducts();
		}
		Build.AddUBTFilesToBuildProducts();

		// Get a list of all the input files
		List<string> FileNames = new List<string>(Build.BuildProductFiles);
		foreach(string FileName in Build.BuildProductFiles)
		{
			string SymbolFileName = Path.ChangeExtension(FileName, ".pdb");
			if(File.Exists(SymbolFileName))
			{
				FileNames.Add(SymbolFileName);
			}
		}

		// Copy all the files over
		foreach(string FileName in FileNames)
		{
			string TargetFileName = Utils.MakeRerootedFilePath(FileName, CommandUtils.CmdEnv.LocalRoot, TargetDir);
			Directory.CreateDirectory(Path.GetDirectoryName(TargetFileName));
			File.Copy(FileName, TargetFileName);
		}

		Log("Copied {0} files to {1}", FileNames.Count, TargetDir);
	}
}
