// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Linq;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

[Help("Builds a plugin, and packages it for distribution")]
[Help("Plugin", "Specify the path to the descriptor file for the plugin that should be packaged")]
[Help("NoHostPlatform", "Prevent compiling for the editor platform on the host")]
[Help("TargetPlatforms", "Specify a list of target platforms to build, separated by '+' characters (eg. -TargetPlatforms=Win32+Win64). Default is all the Rocket target platforms.")]
[Help("Package", "The path which the build artifacts should be packaged to, ready for distribution.")]
class BuildPlugin : BuildCommand
{
	public override void ExecuteBuild()
	{
		// Get the plugin filename
		string PluginParam = ParseParamValue("Plugin");
		if(PluginParam == null)
		{
			throw new AutomationException("Plugin file name was not specified via the -plugin argument");
		}

		// Read the plugin
		FileReference PluginFile = new FileReference(PluginParam);
		DirectoryReference PluginDirectory = PluginFile.Directory;
		PluginDescriptor Plugin = PluginDescriptor.FromFile(PluginFile);

		// Clean the intermediate build directory
		DirectoryReference IntermediateBuildDirectory = DirectoryReference.Combine(PluginDirectory, "Intermediate", "Build");
		if(CommandUtils.DirectoryExists(IntermediateBuildDirectory.FullName))
		{
			CommandUtils.DeleteDirectory(IntermediateBuildDirectory.FullName);
		}

		// Create a host project for the plugin. For script generator plugins, we need to have UHT be able to load it - and that can only happen if it's enabled in a project.
		DirectoryReference HostProjectDirectory = DirectoryReference.Combine(new DirectoryReference(CommandUtils.CmdEnv.LocalRoot), "HostProject");
		if (CommandUtils.DirectoryExists(HostProjectDirectory.FullName))
		{
			CommandUtils.DeleteDirectory(HostProjectDirectory.FullName);
		}

		DirectoryReference HostProjectPluginDirectory = DirectoryReference.Combine(HostProjectDirectory, "Plugins", PluginFile.GetFileNameWithoutExtension());

		string[] CopyPluginFiles = Directory.EnumerateFiles(PluginDirectory.FullName, "*", SearchOption.AllDirectories).ToArray();
		foreach (string CopyPluginFile in CopyPluginFiles)
		{
			CommandUtils.CopyFile(CopyPluginFile, CommandUtils.MakeRerootedFilePath(CopyPluginFile, PluginDirectory.FullName, HostProjectPluginDirectory.FullName));
		}

		FileReference HostProjectPluginFile = FileReference.Combine(HostProjectPluginDirectory, PluginFile.GetFileName());
		FileReference HostProjectFile = FileReference.Combine(HostProjectDirectory, "HostProject.uproject");
		File.WriteAllText(HostProjectFile.FullName, "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"" + PluginFile.GetFileNameWithoutExtension() + "\", \"Enabled\": true } ] }");

		// Get any additional arguments from the commandline
		string AdditionalArgs = "";

		// Build the host platforms
		List<string> ReceiptFileNames = new List<string>();
		UnrealTargetPlatform HostPlatform = BuildHostPlatform.Current.Platform;
		if(!ParseParam("NoHostPlatform"))
		{
			if (Plugin.bCanBeUsedWithUnrealHeaderTool)
			{
				BuildPluginWithUBT(PluginFile, Plugin, null, "UnrealHeaderTool", TargetRules.TargetType.Program, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, String.Format("{0} -plugin {1}", AdditionalArgs, CommandUtils.MakePathSafeToUseWithCommandLine(HostProjectPluginFile.FullName)));
			}
			BuildPluginWithUBT(PluginFile, Plugin, HostProjectFile, "UE4Editor", TargetRules.TargetType.Editor, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, AdditionalArgs);
		}

		// Add the game targets
		List<UnrealTargetPlatform> TargetPlatforms = Rocket.RocketBuild.GetTargetPlatforms(this, HostPlatform);
		foreach(UnrealTargetPlatform TargetPlatform in TargetPlatforms)
		{
			if(Rocket.RocketBuild.IsCodeTargetPlatform(HostPlatform, TargetPlatform))
			{
				BuildPluginWithUBT(PluginFile, Plugin, HostProjectFile, "UE4Game", TargetRules.TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, AdditionalArgs);
				BuildPluginWithUBT(PluginFile, Plugin, HostProjectFile, "UE4Game", TargetRules.TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Shipping, ReceiptFileNames, AdditionalArgs);
			}
		}

		// Package the plugin to the output folder
		string PackageDirectory = ParseParamValue("Package");
		if(PackageDirectory != null)
		{
			List<BuildProduct> BuildProducts = GetBuildProductsFromReceipts(UnrealBuildTool.UnrealBuildTool.EngineDirectory, HostProjectDirectory, ReceiptFileNames);
			PackagePlugin(HostProjectPluginFile, BuildProducts, PackageDirectory);
		}
	}

	void BuildPluginWithUBT(FileReference PluginFile, PluginDescriptor Plugin, FileReference HostProjectFile, string TargetName, TargetRules.TargetType TargetType, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, List<string> ReceiptFileNames, string InAdditionalArgs)
	{
		// Find a list of modules that need to be built for this plugin
		List<string> ModuleNames = new List<string>();
		foreach(ModuleDescriptor Module in Plugin.Modules)
		{
			bool bBuildDeveloperTools = (TargetType == TargetRules.TargetType.Editor || TargetType == TargetRules.TargetType.Program);
			bool bBuildEditor = (TargetType == TargetRules.TargetType.Editor);
			if(Module.IsCompiledInConfiguration(Platform, TargetType, bBuildDeveloperTools, bBuildEditor))
			{
				ModuleNames.Add(Module.Name);
			}
		}

		// Add these modules to the build agenda
		if(ModuleNames.Count > 0)
		{
			string Arguments = "";// String.Format("-plugin {0}", CommandUtils.MakePathSafeToUseWithCommandLine(PluginFile.FullName));
			foreach(string ModuleName in ModuleNames)
			{
				Arguments += String.Format(" -module {0}", ModuleName);
			}

			string Architecture = UEBuildPlatform.GetBuildPlatform(Platform).CreateContext(HostProjectFile).GetActiveArchitecture();

			string ReceiptFileName = TargetReceipt.GetDefaultPath(Path.GetDirectoryName(PluginFile.FullName), TargetName, Platform, Configuration, Architecture);
			Arguments += String.Format(" -receipt {0}", CommandUtils.MakePathSafeToUseWithCommandLine(ReceiptFileName));
			ReceiptFileNames.Add(ReceiptFileName);
			
			if(!String.IsNullOrEmpty(InAdditionalArgs))
			{
				Arguments += InAdditionalArgs;
			}

			CommandUtils.RunUBT(CmdEnv, UE4Build.GetUBTExecutable(), String.Format("{0} {1} {2}{3} {4}", TargetName, Platform, Configuration, (HostProjectFile == null)? "" : String.Format(" -project=\"{0}\"", HostProjectFile.FullName), Arguments));
		}
	}

	static List<BuildProduct> GetBuildProductsFromReceipts(DirectoryReference EngineDir, DirectoryReference ProjectDir, List<string> ReceiptFileNames)
	{
		List<BuildProduct> BuildProducts = new List<BuildProduct>();
		foreach(string ReceiptFileName in ReceiptFileNames)
		{
			TargetReceipt Receipt;
			if(!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
			{
				throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
			}
			Receipt.ExpandPathVariables(EngineDir, ProjectDir);
			BuildProducts.AddRange(Receipt.BuildProducts);
		}
		return BuildProducts;
	}

	static void PackagePlugin(FileReference PluginFile, List<BuildProduct> BuildProducts, string PackageDirectory)
	{
		// Clear the output directory
		CommandUtils.DeleteDirectoryContents(PackageDirectory);

		// Copy all the files to the output directory
		List<string> MatchingFileNames = FilterPluginFiles(PluginFile.FullName, BuildProducts);
		foreach(string MatchingFileName in MatchingFileNames)
		{
			string SourceFileName = Path.Combine(Path.GetDirectoryName(PluginFile.FullName), MatchingFileName);
			string TargetFileName = Path.Combine(PackageDirectory, MatchingFileName);
			CommandUtils.CopyFile(SourceFileName, TargetFileName);
			CommandUtils.SetFileAttributes(TargetFileName, ReadOnly: false);
		}

		// Get the output plugin filename
		FileReference TargetPluginFile = FileReference.Combine(new DirectoryReference(PackageDirectory), PluginFile.GetFileName());
		PluginDescriptor NewDescriptor = PluginDescriptor.FromFile(TargetPluginFile);
		NewDescriptor.bEnabledByDefault = true;
		NewDescriptor.bInstalled = true;
		NewDescriptor.Save(TargetPluginFile.FullName);
	}

	static List<string> FilterPluginFiles(string PluginFileName, List<BuildProduct> BuildProducts)
	{
		string PluginDirectory = Path.GetDirectoryName(PluginFileName);

		// Set up the default filter
		FileFilter Filter = new FileFilter();
		Filter.AddRuleForFile(PluginFileName, PluginDirectory, FileFilterType.Include);
		Filter.AddRuleForFiles(BuildProducts.Select(x => x.Path), PluginDirectory, FileFilterType.Include);
		Filter.Include("/Binaries/ThirdParty/...");
		Filter.Include("/Resources/...");
		Filter.Include("/Content/...");
		Filter.Include("/Intermediate/Build/.../Inc/...");
		Filter.Include("/Source/...");

		// Add custom rules for each platform
		string FilterFileName = Path.Combine(Path.GetDirectoryName(PluginFileName), "Config", "FilterPlugin.ini");
		if(File.Exists(FilterFileName))
		{
			Filter.ReadRulesFromFile(FilterFileName, "FilterPlugin");
		}

		// Apply the standard exclusion rules
		Filter.ExcludeConfidentialFolders();
		Filter.ExcludeConfidentialPlatforms();

		// Apply the filter to the plugin directory
		return Filter.ApplyToDirectory(PluginDirectory, true);
	}
}
