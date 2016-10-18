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
			throw new AutomationException("Missing -Plugin=... argument");
		}

		// Check it exists
		FileReference PluginFile = new FileReference(PluginParam);
		if (!PluginFile.Exists())
		{
			throw new AutomationException("Plugin '{0}' not found", PluginFile.FullName);
		}

		// Get the output directory
		string PackageParam = ParseParamValue("Package");
		if (PackageParam == null)
		{
			throw new AutomationException("Missing -Package=... argument");
		}

		// Make sure the packaging directory is valid
		DirectoryReference PackageDir = new DirectoryReference(PackageParam);
		if (PluginFile.IsUnderDirectory(PackageDir))
		{
			throw new AutomationException("Packaged plugin output directory must be different to source");
		}
		if (PackageDir.IsUnderDirectory(DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine")))
		{
			throw new AutomationException("Output directory for packaged plugin must be outside engine directory");
		}

		// Clear the output directory of existing stuff
		if (PackageDir.Exists())
		{
			CommandUtils.DeleteDirectoryContents(PackageDir.FullName);
		}
		else
		{
			PackageDir.CreateDirectory();
		}

		// Create a placeholder FilterPlugin.ini with instructions on how to use it
		FileReference SourceFilterFile = FileReference.Combine(PluginFile.Directory, "Config", "FilterPlugin.ini");
		if (!SourceFilterFile.Exists())
		{
			List<string> Lines = new List<string>();
			Lines.Add("[FilterPlugin]");
			Lines.Add("; This section lists additional files which will be packaged along with your plugin. Paths should be listed relative to the root plugin directory, and");
			Lines.Add("; may include \"...\", \"*\", and \"?\" wildcards to match directories, files, and individual characters respectively.");
			Lines.Add(";");
			Lines.Add("; Examples:");
			Lines.Add(";    /README.txt");
			Lines.Add(";    /Extras/...");
			Lines.Add(";    /Binaries/ThirdParty/*.dll");
			SourceFilterFile.Directory.CreateDirectory();
			CommandUtils.WriteAllLines_NoExceptions(SourceFilterFile.FullName, Lines.ToArray());
		}

		// Create a host project for the plugin. For script generator plugins, we need to have UHT be able to load it, which can only happen if it's enabled in a project.
		FileReference HostProjectFile = FileReference.Combine(PackageDir, "HostProject", "HostProject.uproject");
		FileReference HostProjectPluginFile = CreateHostProject(HostProjectFile, PluginFile);

		// Read the plugin
		CommandUtils.Log("Reading plugin from {0}...", HostProjectPluginFile);
		PluginDescriptor Plugin = PluginDescriptor.FromFile(HostProjectPluginFile, false);

		// Compile the plugin for all the target platforms
		List<UnrealTargetPlatform> HostPlatforms = ParseParam("NoHostPlatform")? new List<UnrealTargetPlatform>() : new List<UnrealTargetPlatform> { BuildHostPlatform.Current.Platform };
		List<UnrealTargetPlatform> TargetPlatforms = Rocket.RocketBuild.GetTargetPlatforms(this, BuildHostPlatform.Current.Platform).Where(x => Rocket.RocketBuild.IsCodeTargetPlatform(BuildHostPlatform.Current.Platform, x)).ToList();
		FileReference[] BuildProducts = CompilePlugin(HostProjectFile, HostProjectPluginFile, Plugin, HostPlatforms, TargetPlatforms, "");

		// Package up the final plugin data
		PackagePlugin(HostProjectPluginFile, BuildProducts, PackageDir);

		// Remove the host project
		if(!ParseParam("NoDeleteHostProject"))
		{
			CommandUtils.DeleteDirectory(HostProjectFile.Directory.FullName);
		}
	}

	FileReference CreateHostProject(FileReference HostProjectFile, FileReference PluginFile)
	{
		DirectoryReference HostProjectDir = HostProjectFile.Directory;
		HostProjectDir.CreateDirectory();

		// Create the new project descriptor
		File.WriteAllText(HostProjectFile.FullName, "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"" + PluginFile.GetFileNameWithoutExtension() + "\", \"Enabled\": true } ] }");

		// Get the plugin directory in the host project, and copy all the files in
		DirectoryReference HostProjectPluginDir = DirectoryReference.Combine(HostProjectDir, "Plugins", PluginFile.GetFileNameWithoutExtension());
		CommandUtils.ThreadedCopyFiles(PluginFile.Directory.FullName, HostProjectPluginDir.FullName);
		CommandUtils.DeleteDirectory(true, DirectoryReference.Combine(HostProjectPluginDir, "Intermediate").FullName);

		// Return the path to the plugin file in the host project
		return FileReference.Combine(HostProjectPluginDir, PluginFile.GetFileName());
	}

	FileReference[] CompilePlugin(FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, List<UnrealTargetPlatform> HostPlatforms, List<UnrealTargetPlatform> TargetPlatforms, string AdditionalArgs)
	{
		List<string> ReceiptFileNames = new List<string>();

		// Build the host platforms
		if(HostPlatforms.Count > 0)
		{
			CommandUtils.Log("Building plugin for host platforms: {0}", String.Join(", ", HostPlatforms));
			foreach (UnrealTargetPlatform HostPlatform in HostPlatforms)
			{
				if (Plugin.bCanBeUsedWithUnrealHeaderTool)
				{
					CompilePluginWithUBT(null, HostProjectPluginFile, Plugin, "UnrealHeaderTool", TargetRules.TargetType.Program, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, String.Format("{0} -plugin {1}", AdditionalArgs, CommandUtils.MakePathSafeToUseWithCommandLine(HostProjectPluginFile.FullName)));
				}
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Editor", TargetRules.TargetType.Editor, HostPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, AdditionalArgs);
			}
		}

		// Add the game targets
		if(TargetPlatforms.Count > 0)
		{
			CommandUtils.Log("Building plugin for target platforms: {0}", String.Join(", ", TargetPlatforms));
			foreach (UnrealTargetPlatform TargetPlatform in TargetPlatforms)
			{
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Game", TargetRules.TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Development, ReceiptFileNames, null);
				CompilePluginWithUBT(HostProjectFile, HostProjectPluginFile, Plugin, "UE4Game", TargetRules.TargetType.Game, TargetPlatform, UnrealTargetConfiguration.Shipping, ReceiptFileNames, null);
			}
		}

		// Package the plugin to the output folder
		List<BuildProduct> BuildProducts = GetBuildProductsFromReceipts(UnrealBuildTool.UnrealBuildTool.EngineDirectory, HostProjectFile.Directory, ReceiptFileNames);
		return BuildProducts.Select(x => new FileReference(x.Path)).ToArray();
	}

	void CompilePluginWithUBT(FileReference HostProjectFile, FileReference HostProjectPluginFile, PluginDescriptor Plugin, string TargetName, TargetRules.TargetType TargetType, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, List<string> ReceiptFileNames, string InAdditionalArgs)
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

			string ReceiptFileName = TargetReceipt.GetDefaultPath(HostProjectPluginFile.Directory.FullName, TargetName, Platform, Configuration, Architecture);
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

	static void PackagePlugin(FileReference SourcePluginFile, IEnumerable<FileReference> BuildProducts, DirectoryReference TargetDir)
	{
		DirectoryReference SourcePluginDir = SourcePluginFile.Directory;

		// Copy all the files to the output directory
		FileReference[] SourceFiles = FilterPluginFiles(SourcePluginFile, BuildProducts).ToArray();
		foreach(FileReference SourceFile in SourceFiles)
		{
			FileReference TargetFile = FileReference.Combine(TargetDir, SourceFile.MakeRelativeTo(SourcePluginDir));
			CommandUtils.CopyFile(SourceFile.FullName, TargetFile.FullName);
			CommandUtils.SetFileAttributes(TargetFile.FullName, ReadOnly: false);
		}

		// Get the output plugin filename
		FileReference TargetPluginFile = FileReference.Combine(TargetDir, SourcePluginFile.GetFileName());
		PluginDescriptor NewDescriptor = PluginDescriptor.FromFile(TargetPluginFile, false);
		NewDescriptor.bEnabledByDefault = true;
		NewDescriptor.bInstalled = true;
		NewDescriptor.Save(TargetPluginFile.FullName, false);
	}

	static IEnumerable<FileReference> FilterPluginFiles(FileReference PluginFile, IEnumerable<FileReference> BuildProducts)
	{
		// Set up the default filter
		FileFilter Filter = new FileFilter();
		Filter.AddRuleForFile(PluginFile.FullName, PluginFile.Directory.FullName, FileFilterType.Include);
		Filter.AddRuleForFiles(BuildProducts, PluginFile.Directory, FileFilterType.Include);
		Filter.Include("/Binaries/ThirdParty/...");
		Filter.Include("/Resources/...");
		Filter.Include("/Content/...");
		Filter.Include("/Intermediate/Build/.../Inc/...");
		Filter.Include("/Source/...");

		// Add custom rules for each platform
		FileReference FilterFile = FileReference.Combine(PluginFile.Directory, "Config", "FilterPlugin.ini");
		if(FilterFile.Exists())
		{
			CommandUtils.Log("Reading filter rules from {0}", FilterFile);
			Filter.ReadRulesFromFile(FilterFile.FullName, "FilterPlugin");
		}

		// Apply the standard exclusion rules
		Filter.ExcludeConfidentialFolders();
		Filter.ExcludeConfidentialPlatforms();

		// Apply the filter to the plugin directory
		return Filter.ApplyToDirectory(PluginFile.Directory, true);
	}
}
