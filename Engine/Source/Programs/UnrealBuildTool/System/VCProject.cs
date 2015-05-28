// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;
using System.Diagnostics;

namespace UnrealBuildTool
{
	public abstract class MSBuildProjectFile : ProjectFile
	{
		/// The project file version string
		static public readonly string VCProjectFileVersionString = "10.0.30319.1";

		/// The build configuration name to use for stub project configurations.  These are projects whose purpose
		/// is to make it easier for developers to find source files and to provide IntelliSense data for the module
		/// to Visual Studio
		static public readonly string StubProjectConfigurationName = "BuiltWithUnrealBuildTool";

		/// The name of the Visual C++ platform to use for stub project configurations
		/// NOTE: We always use Win32 for the stub project's platform, since that is guaranteed to be supported by Visual Studio
		static public readonly string StubProjectPlatformName = "Win32";

		/// override project configuration name for platforms visual studio doesn't natively support
		public string ProjectConfigurationNameOverride = "";

		/// override project platform for platforms visual studio doesn't natively support
		public string ProjectPlatformNameOverride = "";

		/// <summary>
		/// The Guid representing the project type e.g. C# or C++
		/// </summary>
		public virtual string ProjectTypeGUID
		{
			get { throw new BuildException( "Unrecognized type of project file for Visual Studio solution" ); } 
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public MSBuildProjectFile(string InitFilePath)
			: base(InitFilePath)
		{
			// Each project gets its own GUID.  This is stored in the project file and referenced in the solution file.

			// First, check to see if we have an existing file on disk.  If we do, then we'll try to preserve the
			// GUID by loading it from the existing file.
			if (File.Exists(ProjectFilePath))
			{
				try
				{
					LoadGUIDFromExistingProject();
				}
				catch (Exception)
				{
					// Failed to find GUID, so just create a new one
					ProjectGUID = Guid.NewGuid();
				}
			}

			if (ProjectGUID == Guid.Empty)
			{
				// Generate a brand new GUID
				ProjectGUID = Guid.NewGuid();
			}
		}


		/// <summary>
		/// Attempts to load the project's GUID from an existing project file on disk
		/// </summary>
		public override void LoadGUIDFromExistingProject()
		{
			// Only load GUIDs if we're in project generation mode.  Regular builds don't need GUIDs for anything.
			if( ProjectFileGenerator.bGenerateProjectFiles )
			{
				var Doc = new XmlDocument();
				Doc.Load( ProjectFilePath );

				// @todo projectfiles: Ideally we could do a better job about preserving GUIDs when only minor changes are made
				// to the project (such as adding a single new file.) It would make diffing changes much easier!

				// @todo projectfiles: Can we "seed" a GUID based off the project path and generate consistent GUIDs each time?

				var Elements = Doc.GetElementsByTagName( "ProjectGuid" );
				foreach( XmlElement Element in Elements )
				{
					ProjectGUID = Guid.ParseExact( Element.InnerText.Trim( "{}".ToCharArray() ), "D" );
				}
			}
		}


		/// <summary>
		/// Given a target platform and configuration, generates a platform and configuration name string to use in Visual Studio projects.
		/// Unlike with solution configurations, Visual Studio project configurations only support certain types of platforms, so we'll
		/// generate a configuration name that has the platform "built in", and use a default platform type
		/// </summary>
		/// <param name="Platform">Actual platform</param>
		/// <param name="Configuration">Actual configuration</param>
		/// <param name="TargetConfigurationName">The configuration name from the target rules, or null if we don't have one</param>
		/// <param name="ProjectPlatformName">Name of platform string to use for Visual Studio project</param>
		/// <param name="ProjectConfigurationName">Name of configuration string to use for Visual Studio project</param>
		public void MakeProjectPlatformAndConfigurationNames(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string TargetConfigurationName, out string ProjectPlatformName, out string ProjectConfigurationName)
		{
			if (IsStubProject)
			{
				if (Platform != UnrealTargetPlatform.Unknown || Configuration != UnrealTargetConfiguration.Unknown)
				{
					throw new BuildException("Stub project was expecting platform and configuration type to be set to Unknown");
				}
				ProjectPlatformName = StubProjectPlatformName;
				ProjectConfigurationName = StubProjectConfigurationName;
			}
			else
			{
				// If this is a C# project, then the project platform name must always be "Any CPU"
				if (this is VCSharpProjectFile)
				{
					ProjectConfigurationName = Configuration.ToString();
					ProjectPlatformName = VCProjectFileGenerator.DotNetPlatformName;
				}
				else
				{
					var PlatformProjectGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, bInAllowFailure: true);

					// Check to see if this platform is supported directly by Visual Studio projects.
					bool HasActualVSPlatform = (PlatformProjectGenerator != null) ? PlatformProjectGenerator.HasVisualStudioSupport(Platform, Configuration) : false;

					if (HasActualVSPlatform)
					{
						// Great!  Visual Studio supports this platform natively, so we don't need to make up
						// a fake project configuration name.

						// Allow the platform to specify the name used in VisualStudio.
						// Note that the actual name of the platform on the Visual Studio side may be different than what
						// UnrealBuildTool calls it (e.g. "Win64" -> "x64".) GetVisualStudioPlatformName() will figure this out.
						ProjectConfigurationName = Configuration.ToString();
						ProjectPlatformName = PlatformProjectGenerator.GetVisualStudioPlatformName(Platform, Configuration);
					}
					else
					{
						// Visual Studio doesn't natively support this platform, so we fake it by mapping it to
						// a project configuration that has the platform name in that configuration as a suffix,
						// and then using "Win32" as the actual VS platform name
						ProjectConfigurationName = ProjectConfigurationNameOverride == "" ? Platform.ToString() + "_" + Configuration.ToString() : ProjectConfigurationNameOverride;
						ProjectPlatformName = ProjectPlatformNameOverride == "" ? VCProjectFileGenerator.DefaultPlatformName : ProjectPlatformNameOverride;
					}

					if( !String.IsNullOrEmpty( TargetConfigurationName ) )
					{
						ProjectConfigurationName += "_" + TargetConfigurationName;
					}
				}
			}
		}


		/// <summary>
		/// Checks to see if the specified solution platform and configuration is able to map to this project
		/// </summary>
		/// <param name="ProjectTarget">The target that we're checking for a valid platform/config combination</param>
		/// <param name="Platform">Platform</param>
		/// <param name="Configuration">Configuration</param>
		/// <returns>True if this is a valid combination for this project, otherwise false</returns>
		public static bool IsValidProjectPlatformAndConfiguration(ProjectTarget ProjectTarget, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
		{
			var PlatformProjectGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
			if (PlatformProjectGenerator == null)
			{
				return false;
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
			if (BuildPlatform == null)
			{
				return false;
			}

			if (BuildPlatform.HasRequiredSDKsInstalled() != SDKStatus.Valid)
			{
				return false;
			}


			var SupportedConfigurations = new List<UnrealTargetConfiguration>();
			var SupportedPlatforms = new List<UnrealTargetPlatform>();
			if (!ProjectFileGenerator.bCreateDummyConfigsForUnsupportedPlatforms)
			{
				if( ProjectTarget.TargetRules != null )
				{
					ProjectTarget.TargetRules.GetSupportedPlatforms(ref SupportedPlatforms);
				}
			}
			else
			{
				UnrealBuildTool.GetAllPlatforms(ref SupportedPlatforms);
			}
			bool bIncludeTestAndShippingConfigs = ProjectFileGenerator.bIncludeTestAndShippingConfigs || ProjectFileGenerator.bGeneratingRocketProjectFiles;
			if( ProjectTarget.TargetRules != null )
			{
				// Rocket projects always get shipping configs
				ProjectTarget.TargetRules.GetSupportedConfigurations(ref SupportedConfigurations, bIncludeTestAndShippingConfigs:bIncludeTestAndShippingConfigs);
			}

			// Add all of the extra platforms/configurations for this target
			{
				foreach( var ExtraPlatform in ProjectTarget.ExtraSupportedPlatforms )
				{
					if( !SupportedPlatforms.Contains( ExtraPlatform ) )
					{
						SupportedPlatforms.Add( ExtraPlatform );
					}
				}
				foreach( var ExtraConfig in ProjectTarget.ExtraSupportedConfigurations )
				{
					if( bIncludeTestAndShippingConfigs || ( ExtraConfig != UnrealTargetConfiguration.Shipping && ExtraConfig != UnrealTargetConfiguration.Test ) )
					{
						if( !SupportedConfigurations.Contains( ExtraConfig ) )
						{
							SupportedConfigurations.Add( ExtraConfig );
						}
					}
				}
			}

			// Only build for supported platforms
			if (SupportedPlatforms.Contains(Platform) == false)
			{
				return false;
			}

			// Only build for supported configurations
			if (SupportedConfigurations.Contains(Configuration) == false)
			{
				return false;
			}

			return true;
		}

		/// <summary>
		/// GUID for this Visual C++ project file
		/// </summary>
		public Guid ProjectGUID
		{
			get;
			protected set;
		}
	}
			
	public class VCProjectFile : MSBuildProjectFile
	{
		// This is the GUID that Visual Studio uses to identify a C++ project file in the solution
		public override string ProjectTypeGUID
		{
			get { return "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public VCProjectFile(string InitFilePath)
			: base( InitFilePath )
		{
		}


		class ProjectConfigAndTargetCombination
		{
			public UnrealTargetPlatform Platform;
			public UnrealTargetConfiguration Configuration;
			public string ProjectPlatformName;
			public string ProjectConfigurationName;
			public ProjectTarget ProjectTarget;

			public ProjectConfigAndTargetCombination(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InProjectPlatformName, string InProjectConfigurationName, ProjectTarget InProjectTarget)
			{
				Platform = InPlatform;
				Configuration = InConfiguration;
				ProjectPlatformName = InProjectPlatformName;
				ProjectConfigurationName = InProjectConfigurationName;
				ProjectTarget = InProjectTarget;
			}

			public string ProjectConfigurationAndPlatformName
			{
				get { return (ProjectPlatformName == null)? null : (ProjectConfigurationName + "|" + ProjectPlatformName); }
			}

			public override string ToString()
			{
				return String.Format("{0} {1} {2}", ProjectTarget, Platform, Configuration);
			}
		}


		/// Implements Project interface
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			string ProjectName = Path.GetFileNameWithoutExtension(ProjectFilePath);

			bool bSuccess = true;

			// NOTE: We intentionally do not SORT the source file list, as we want the order they're written to disk to be consistent
			//       with how they are stored in memory.  This makes for more consistent Unity compiles when alternating between
			//       using "auto" projects and on-disk projects for builds.
			var ShouldSortSourceFiles = false;
			if( ShouldSortSourceFiles )
			{
				// Source our list of source files
				Comparison<SourceFile> SourceFileComparer = ( FileA, FileB ) => { return FileA.FilePath.CompareTo( FileB.FilePath ); };
				SourceFiles.Sort( SourceFileComparer );
			}

			// Build up the new include search path string
			var VCIncludeSearchPaths = new StringBuilder();
			{ 
				foreach( var CurPath in IntelliSenseIncludeSearchPaths )
				{
					VCIncludeSearchPaths.Append( CurPath + ";" );
				}
				foreach( var CurPath in IntelliSenseSystemIncludeSearchPaths )
				{
					VCIncludeSearchPaths.Append( CurPath + ";" );
				}
				if(InPlatforms.Contains(UnrealTargetPlatform.Win64))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CPPTargetPlatform.Win64) + ";");
				}
				else if(InPlatforms.Contains(UnrealTargetPlatform.Win32))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CPPTargetPlatform.Win32) + ";");
				}
			}

			var VCPreprocessorDefinitions = new StringBuilder();
			foreach( var CurDef in IntelliSensePreprocessorDefinitions )
			{
				if( VCPreprocessorDefinitions.Length > 0 )
				{
					VCPreprocessorDefinitions.Append( ';' );
				}
				VCPreprocessorDefinitions.Append( CurDef );
			}

			// Setup VC project file content
			var VCProjectFileContent = new StringBuilder();
			var VCFiltersFileContent = new StringBuilder();
			var VCUserFileContent = new StringBuilder();

			// Visual Studio doesn't require a *.vcxproj.filters file to even exist alongside the project unless 
			// it actually has something of substance in it.  We'll avoid saving it out unless we need to.
			var FiltersFileIsNeeded = false;

			// Project file header
			var ToolsVersion = VCProjectFileGenerator.ProjectFileFormat == VCProjectFileGenerator.VCProjectFileFormat.VisualStudio2013 ? "12.0" : "4.0";
			VCProjectFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				ProjectFileGenerator.NewLine +
				"<Project DefaultTargets=\"Build\" ToolsVersion=\"" + ToolsVersion + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

			bool bGenerateUserFileContent = UEPlatformProjectGenerator.PlatformRequiresVSUserFileGeneration(InPlatforms, InConfigurations);
			if (bGenerateUserFileContent)
			{
				VCUserFileContent.Append(
					"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine + 
					ProjectFileGenerator.NewLine +
					"<Project ToolsVersion=\"" + ToolsVersion + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine
					);
			}

			// Build up a list of platforms and configurations this project will support.  In this list, Unknown simply
			// means that we should use the default "stub" project platform and configuration name.
			var ProjectConfigAndTargetCombinations = new List< ProjectConfigAndTargetCombination >();

			// If this is a "stub" project, then only add a single configuration to the project
			if( IsStubProject )
			{
				ProjectConfigAndTargetCombination StubCombination = new ProjectConfigAndTargetCombination(UnrealTargetPlatform.Unknown, UnrealTargetConfiguration.Unknown, StubProjectPlatformName, StubProjectConfigurationName, null);
				ProjectConfigAndTargetCombinations.Add(StubCombination);
			}
			else
			{
				// Figure out all the desired configurations
				foreach (var Configuration in InConfigurations)
				{
					//@todo.Rocket: Put this in a commonly accessible place?
					if (UnrealBuildTool.IsValidConfiguration(Configuration) == false)
					{
						continue;
					}
					foreach (var Platform in InPlatforms)
					{
						if (UnrealBuildTool.IsValidPlatform(Platform) == false)
						{
							continue;
						}
						var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
						if ((BuildPlatform != null) && (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid))
						{
							// Now go through all of the target types for this project
							if( ProjectTargets.Count == 0 )
							{
								throw new BuildException( "Expecting at least one ProjectTarget to be associated with project '{0}' in the TargetProjects list ", ProjectFilePath );
							}

							foreach( var ProjectTarget in ProjectTargets )
							{
								if(IsValidProjectPlatformAndConfiguration( ProjectTarget, Platform, Configuration ))
								{
									string ProjectPlatformName, ProjectConfigurationName;
									MakeProjectPlatformAndConfigurationNames(Platform, Configuration, ProjectTarget.TargetRules.ConfigurationName, out ProjectPlatformName, out ProjectConfigurationName);

									ProjectConfigAndTargetCombination Combination = new ProjectConfigAndTargetCombination(Platform, Configuration, ProjectPlatformName, ProjectConfigurationName, ProjectTarget);
									ProjectConfigAndTargetCombinations.Add( Combination );
								}
							}
						}
					}
				}
			}

			VCProjectFileContent.Append(
				"	<ItemGroup Label=\"ProjectConfigurations\">" + ProjectFileGenerator.NewLine);

			// Make a list of the platforms and configs as project-format names
			var ProjectPlatforms = new List<UnrealTargetPlatform>();
			var ProjectPlatformNames = new List<string>();
			var ProjectConfigNames = new List<string>();
			foreach( var Combination in ProjectConfigAndTargetCombinations )
			{
				if( !ProjectPlatforms.Contains( Combination.Platform ) )
				{
					ProjectPlatforms.Add( Combination.Platform );
				}
				if( !ProjectPlatformNames.Contains( Combination.ProjectPlatformName ) )
				{
					ProjectPlatformNames.Add(Combination.ProjectPlatformName);
				}
				if( !ProjectConfigNames.Contains( Combination.ProjectConfigurationName ) )
				{
					ProjectConfigNames.Add(Combination.ProjectConfigurationName);
				}
			}

			// Output ALL the project's config-platform permutations (project files MUST do this)
			foreach (var ProjectConfigName in ProjectConfigNames)
			{
				foreach (var ProjectPlatformName in ProjectPlatformNames)
				{
					VCProjectFileContent.Append(
							"		<ProjectConfiguration Include=\"" + ProjectConfigName + "|" + ProjectPlatformName + "\">" + ProjectFileGenerator.NewLine +
							"			<Configuration>" + ProjectConfigName + "</Configuration>" + ProjectFileGenerator.NewLine +
							"			<Platform>" + ProjectPlatformName + "</Platform>" + ProjectFileGenerator.NewLine +
							"		</ProjectConfiguration>" + ProjectFileGenerator.NewLine);
				}              
			}

			VCProjectFileContent.Append(
				"	</ItemGroup>" + ProjectFileGenerator.NewLine);

			VCFiltersFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				ProjectFileGenerator.NewLine +
				"<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

			// Platform specific PropertyGroups, etc.
			StringBuilder AdditionalPropertyGroups = new StringBuilder();
			if (!IsStubProject)
			{
				foreach (var Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development))
					{
						AdditionalPropertyGroups.Append(ProjGenerator.GetAdditionalVisualStudioPropertyGroups(Platform));
					}
				}

				VCProjectFileContent.Append( AdditionalPropertyGroups );
			}

			// Source folders and files
			{
				var LocalAliasedFiles = new List<AliasedFile>(AliasedFiles);

				foreach( var CurFile in SourceFiles )
				{
					// We want all source file and directory paths in the project files to be relative to the project file's
					// location on the disk.  Convert the path to be relative to the project file directory
					var ProjectRelativeSourceFile = Utils.MakePathRelativeTo( CurFile.FilePath, Path.GetDirectoryName( ProjectFilePath ) );

					// By default, files will appear relative to the project file in the solution.  This is kind of the normal Visual
					// Studio way to do things, but because our generated project files are emitted to intermediate folders, if we always
					// did this it would yield really ugly paths int he solution explorer
					string FilterRelativeSourceDirectory = Path.GetDirectoryName( ProjectRelativeSourceFile );

					// Use the specified relative base folder
					if( CurFile.RelativeBaseFolder != null )	// NOTE: We are looking for null strings, not empty strings!
					{
						FilterRelativeSourceDirectory = Path.GetDirectoryName( Utils.MakePathRelativeTo( CurFile.FilePath, CurFile.RelativeBaseFolder ) );
					}

					LocalAliasedFiles.Add(new AliasedFile(ProjectRelativeSourceFile, FilterRelativeSourceDirectory));
				}

				VCFiltersFileContent.Append(
					"	<ItemGroup>" + ProjectFileGenerator.NewLine);

				VCProjectFileContent.Append(
					"	<ItemGroup>" + ProjectFileGenerator.NewLine);

				// Add all file directories to the filters file as solution filters
				var FilterDirectories = new HashSet<string>();
				foreach (var AliasedFile in LocalAliasedFiles)
				{
					// No need to add the root directory relative to the project (it would just be an empty string!)
					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						FiltersFileIsNeeded = EnsureFilterPathExists(AliasedFile.ProjectPath, VCFiltersFileContent, FilterDirectories);
					}

					var VCFileType = GetVCFileType(AliasedFile.FileSystemPath);

					VCProjectFileContent.Append(
						"		<" + VCFileType + " Include=\"" + AliasedFile.FileSystemPath + "\" />" + ProjectFileGenerator.NewLine);

					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						VCFiltersFileContent.Append(
							"		<" + VCFileType + " Include=\"" + AliasedFile.FileSystemPath + "\">" + ProjectFileGenerator.NewLine +
							"			<Filter>" + Utils.CleanDirectorySeparators(AliasedFile.ProjectPath) + "</Filter>" + ProjectFileGenerator.NewLine +
							"		</" + VCFileType + " >" + ProjectFileGenerator.NewLine);

						FiltersFileIsNeeded = true;
					}
					else
					{
						// No need to specify the root directory relative to the project (it would just be an empty string!)
						VCFiltersFileContent.Append(
							"		<" + VCFileType + " Include=\"" + AliasedFile.FileSystemPath + "\" />" + ProjectFileGenerator.NewLine);
					}
				}

				VCProjectFileContent.Append(
					"	</ItemGroup>" + ProjectFileGenerator.NewLine );

				VCFiltersFileContent.Append(
					"	</ItemGroup>" + ProjectFileGenerator.NewLine );
			}


			// Project globals (project GUID, project type, SCC bindings, etc)
			{
				VCProjectFileContent.Append(
					"	<PropertyGroup Label=\"Globals\">" + ProjectFileGenerator.NewLine +
					"		<ProjectGuid>" + ProjectGUID.ToString( "B" ).ToUpperInvariant() + "</ProjectGuid>" + ProjectFileGenerator.NewLine +
					"		<Keyword>MakeFileProj</Keyword>" + ProjectFileGenerator.NewLine +
					"		<RootNamespace>" + ProjectName + "</RootNamespace>" + ProjectFileGenerator.NewLine);

				VCProjectFileContent.Append(
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine);
			}

			// Write each project configuration PreDefaultProps section
			foreach( var Combination in ProjectConfigAndTargetCombinations )
			{
				WritePreDefaultPropsConfiguration( Combination, VCProjectFileContent );
			}

			VCProjectFileContent.Append(
				"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />" + ProjectFileGenerator.NewLine +
				"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />" + ProjectFileGenerator.NewLine +
				"	<ImportGroup Label=\"ExtensionSettings\" />" + ProjectFileGenerator.NewLine +
				"	<PropertyGroup Label=\"UserMacros\" />" + ProjectFileGenerator.NewLine
				);

			// Write each project configuration
			foreach( var Combination in ProjectConfigAndTargetCombinations )
			{
				WriteConfiguration( ProjectName, Combination, VCProjectFileContent, bGenerateUserFileContent? VCUserFileContent : null );
			}

			// For Rocket, include engine source in the source search paths. We never build it locally, so the debugger can't find it.
			if(UnrealBuildTool.RunningRocket() && !IsStubProject)
			{
				VCProjectFileContent.Append("	<PropertyGroup>" + ProjectFileGenerator.NewLine);
				VCProjectFileContent.Append("		<SourcePath>");
				foreach(string DirectoryName in Directory.EnumerateDirectories(Path.GetFullPath(Path.Combine(ProjectFileGenerator.EngineRelativePath, "Source")), "*", SearchOption.AllDirectories))
				{
					if(Directory.EnumerateFiles(DirectoryName, "*.cpp").Any())
					{
						VCProjectFileContent.Append(DirectoryName);
						VCProjectFileContent.Append(";");
					}
				}
				VCProjectFileContent.Append("</SourcePath>" + ProjectFileGenerator.NewLine);
				VCProjectFileContent.Append("	</PropertyGroup>" + ProjectFileGenerator.NewLine);
			}

			// Write IntelliSense info
			{
				// @todo projectfiles: Currently we are storing defines/include paths for ALL configurations rather than using ConditionString and storing
				//      this data uniquely for each target configuration.  IntelliSense may behave better if we did that, but it will result in a LOT more
				//      data being stored into the project file, and might make the IDE perform worse when switching configurations!
				VCProjectFileContent.Append(
					"	<PropertyGroup>" + ProjectFileGenerator.NewLine +
					"		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions)" + ( VCPreprocessorDefinitions.Length > 0 ? ( ";" + VCPreprocessorDefinitions ) : "" ) + "</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine +
					"		<NMakeIncludeSearchPath>$(NMakeIncludeSearchPath)" + ( VCIncludeSearchPaths.Length > 0 ? ( ";" + VCIncludeSearchPaths ) : "" ) + "</NMakeIncludeSearchPath>" + ProjectFileGenerator.NewLine +
					"		<NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>" + ProjectFileGenerator.NewLine +
					"		<NMakeAssemblySearchPath>$(NMakeAssemblySearchPath)</NMakeAssemblySearchPath>" + ProjectFileGenerator.NewLine +
					"		<NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies)</NMakeForcedUsingAssemblies>" + ProjectFileGenerator.NewLine +
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine );
			}

			// look for additional import lines for all platforms for non stub projects
			StringBuilder AdditionalImportSettings = new StringBuilder();
			if (!IsStubProject)
			{
				foreach (var Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development))
					{
						AdditionalImportSettings.Append(ProjGenerator.GetAdditionalVisualStudioImportSettings(Platform));
					}
				}
			}

			string OutputManifestString = "";
			if (!IsStubProject)
			{
				foreach (var Platform in ProjectPlatforms)
				{
					UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development))
					{
						// @todo projectfiles: Serious hacks here because we are trying to emit one-time platform-specific sections that need information
						//    about a target type, but the project file may contain many types of targets!  Some of this logic will need to move into
						//    the per-target configuration writing code.
						var HackTargetType = TargetRules.TargetType.Game;
						string HackTargetFilePath = null;
						foreach( var Combination in ProjectConfigAndTargetCombinations )
						{
							if( Combination.Platform == Platform &&
								Combination.ProjectTarget.TargetRules != null && 
								Combination.ProjectTarget.TargetRules.Type == HackTargetType )
							{
								HackTargetFilePath = Combination.ProjectTarget.TargetFilePath;// ProjectConfigAndTargetCombinations[0].ProjectTarget.TargetFilePath;
								break;										
							}
						}

						if( !String.IsNullOrEmpty( HackTargetFilePath ) )
						{
							OutputManifestString += ProjGenerator.GetVisualStudioOutputManifestSection(Platform, HackTargetType, HackTargetFilePath, ProjectFilePath);
						}
					}
				}
			}

			VCProjectFileContent.Append(
					OutputManifestString +	// output manifest must come before the Cpp.targets file.
					"	<ItemDefinitionGroup>" + ProjectFileGenerator.NewLine +
					"	</ItemDefinitionGroup>" + ProjectFileGenerator.NewLine +
					"	<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />" + ProjectFileGenerator.NewLine +
					AdditionalImportSettings.ToString() +
					"	<ImportGroup Label=\"ExtensionTargets\">" + ProjectFileGenerator.NewLine +
					"	</ImportGroup>" + ProjectFileGenerator.NewLine +
					"</Project>" + ProjectFileGenerator.NewLine );

			VCFiltersFileContent.Append(
				"</Project>" + ProjectFileGenerator.NewLine );

			if (bGenerateUserFileContent)
			{
				VCUserFileContent.Append(
					"</Project>" + ProjectFileGenerator.NewLine
					);
			}

			// Save the project file
			if( bSuccess )
			{
				bSuccess = ProjectFileGenerator.WriteFileIfChanged( ProjectFilePath, VCProjectFileContent.ToString() );
			}


			// Save the filters file
			if( bSuccess )
			{
				// Create a path to the project file's filters file
				var VCFiltersFilePath = ProjectFilePath + ".filters";
				if( FiltersFileIsNeeded )
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged( VCFiltersFilePath, VCFiltersFileContent.ToString() );
				}
				else
				{
					Log.TraceVerbose( "Deleting Visual C++ filters file which is no longer needed: " + VCFiltersFilePath );

					// Delete the filters file, if one exists.  We no longer need it
					try
					{
						File.Delete( VCFiltersFilePath );
					}
					catch( Exception )
					{
						Log.TraceInformation( "Error deleting filters file (file may not be writable): " + VCFiltersFilePath );
					}
				}
			}

			// Save the user file, if required
			if (VCUserFileContent.Length > 0)
			{
				// Create a path to the project file's user file
				var VCUserFilePath = ProjectFilePath + ".user";
				// Never overwrite the existing user path as it will cause them to lose their settings
				if (File.Exists(VCUserFilePath) == false)
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(VCUserFilePath, VCUserFileContent.ToString());
				}
			}

			return bSuccess;
		}

		private static bool EnsureFilterPathExists(string FilterRelativeSourceDirectory, StringBuilder VCFiltersFileContent, HashSet<string> FilterDirectories)
		{
			// We only want each directory to appear once in the filters file
			var PathRemaining = Utils.CleanDirectorySeparators( FilterRelativeSourceDirectory );
			var FiltersFileIsNeeded = false;
			if( !FilterDirectories.Contains( PathRemaining ) )
			{
				// Make sure all subdirectories leading up to this directory each have their own filter, too!
				var AllDirectoriesInPath = new List<string>();
				var PathSoFar = "";
				for( ; ; )
				{
					if( PathRemaining.Length > 0 )
					{
						var SlashIndex = PathRemaining.IndexOf( Path.DirectorySeparatorChar );
						string SplitDirectory;
						if( SlashIndex != -1 )
						{
							SplitDirectory = PathRemaining.Substring( 0, SlashIndex );
							PathRemaining = PathRemaining.Substring( SplitDirectory.Length + 1 );
						}
						else
						{
							SplitDirectory = PathRemaining;
							PathRemaining = "";
						}
						if( !String.IsNullOrEmpty( PathSoFar ) )
						{
							PathSoFar += Path.DirectorySeparatorChar;
						}
						PathSoFar += SplitDirectory;

						AllDirectoriesInPath.Add( PathSoFar );
					}
					else
					{
						break;
					}
				}

				foreach( var LeadingDirectory in AllDirectoriesInPath )
				{
					if( !FilterDirectories.Contains( LeadingDirectory ) )
					{
						FilterDirectories.Add( LeadingDirectory );

						// Generate a unique GUID for this folder
						// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
						//       matches identically with the pre-existing file
						var FilterGUID = Guid.NewGuid().ToString( "B" ).ToUpperInvariant();

						VCFiltersFileContent.Append(
							"		<Filter Include=\"" + LeadingDirectory + "\">" + ProjectFileGenerator.NewLine +
							"			<UniqueIdentifier>" + FilterGUID + "</UniqueIdentifier>" + ProjectFileGenerator.NewLine +
							"		</Filter>" + ProjectFileGenerator.NewLine);

						FiltersFileIsNeeded = true;
					}
				}
			}

			return FiltersFileIsNeeded;
		}

		/// <summary>
		/// Returns the VCFileType element name based on the file path.
		/// </summary>
		/// <param name="Path">The path of the file to return type for.</param>
		/// <returns>Name of the element in MSBuild project file for this file.</returns>
		private string GetVCFileType(string Path)
		{
			// What type of file is this?
			if (Path.EndsWith(".h", StringComparison.InvariantCultureIgnoreCase) ||
				Path.EndsWith(".inl", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClInclude";
			}
			else if (Path.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClCompile";
			}
			else if (Path.EndsWith(".rc", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ResourceCompile";
			}
			else if (Path.EndsWith(".manifest", StringComparison.InvariantCultureIgnoreCase))
			{
				return "Manifest";
			}
			else
			{
				return "None";
			}
		}

		// Anonymous function that writes pre-Default.props configuration data
		private void WritePreDefaultPropsConfiguration(ProjectConfigAndTargetCombination Combination, StringBuilder VCProjectFileContent)
		{
			UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Combination.Platform, true);
			if (((ProjGenerator == null) && (Combination.Platform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}

			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + Combination.ProjectConfigurationAndPlatformName + "'\"";

			string PlatformToolsetString = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioPlatformToolsetString(Combination.Platform, Combination.Configuration, this) : "";
			if( String.IsNullOrEmpty( PlatformToolsetString ) )
			{
				if( VCProjectFileGenerator.ProjectFileFormat == VCProjectFileGenerator.VCProjectFileFormat.VisualStudio2013 )
				{
					PlatformToolsetString = "		<PlatformToolset>v120</PlatformToolset>" + ProjectFileGenerator.NewLine;
				}
				else if( VCProjectFileGenerator.ProjectFileFormat == VCProjectFileGenerator.VCProjectFileFormat.VisualStudio2012 )
				{
					PlatformToolsetString = "		<PlatformToolset>v110</PlatformToolset>" + ProjectFileGenerator.NewLine;
				}
			}

			string PlatformConfigurationType = (ProjGenerator == null)? "Makefile" : ProjGenerator.GetVisualStudioPlatformConfigurationType(Combination.Platform);	
			VCProjectFileContent.Append(
				"	<PropertyGroup " + ConditionString + " Label=\"Configuration\">" + ProjectFileGenerator.NewLine +
				"		<ConfigurationType>" + PlatformConfigurationType + "</ConfigurationType>" + ProjectFileGenerator.NewLine +
						PlatformToolsetString +
				"	</PropertyGroup>" + ProjectFileGenerator.NewLine 
				);
		}

		// Anonymous function that writes project configuration data
		private void WriteConfiguration(string ProjectName, ProjectConfigAndTargetCombination Combination, StringBuilder VCProjectFileContent, StringBuilder VCUserFileContent)
		{
			UnrealTargetPlatform Platform = Combination.Platform;
			UnrealTargetConfiguration Configuration = Combination.Configuration;

			UEPlatformProjectGenerator ProjGenerator = UEPlatformProjectGenerator.GetPlatformProjectGenerator(Platform, true);
			if (((ProjGenerator == null) && (Platform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}
	
			string UProjectPath = "";
			if (IsForeignProject)
			{
				UProjectPath = "\"$(SolutionDir)$(ProjectName).uproject\"";
			}

			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + Combination.ProjectConfigurationAndPlatformName + "'\"";

			{
				VCProjectFileContent.Append(
					"	<ImportGroup " + ConditionString + " Label=\"PropertySheets\">" + ProjectFileGenerator.NewLine +
					"		<Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />" + ProjectFileGenerator.NewLine +
					"	</ImportGroup>" + ProjectFileGenerator.NewLine);

				if (IsStubProject)
				{
					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(Path.Combine(ProjectFileGenerator.EngineRelativePath, BuildConfiguration.BaseIntermediateFolder, "Unused"));

					VCProjectFileContent.Append(
					    "	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine +
						"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
						"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine +
						"		<NMakeBuildCommandLine>@rem Nothing to do.</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeReBuildCommandLine>@rem Nothing to do.</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeCleanCommandLine>@rem Nothing to do.</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeOutput/>" + ProjectFileGenerator.NewLine +
						"	</PropertyGroup>" + ProjectFileGenerator.NewLine);
				}
				else if(ProjectFileGenerator.bGeneratingRocketProjectFiles && Combination.ProjectTarget != null && Combination.ProjectTarget.TargetRules != null && !Combination.ProjectTarget.TargetRules.SupportsPlatform(Combination.Platform))
				{
					List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
					Combination.ProjectTarget.TargetRules.GetSupportedPlatforms(ref SupportedPlatforms);

					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(Path.Combine(ProjectFileGenerator.EngineRelativePath, BuildConfiguration.BaseIntermediateFolder, "Unused"));

					VCProjectFileContent.AppendFormat(
						"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine +
						"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
						"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine +
						"		<NMakeBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeReBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeCleanCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine +
						"		<NMakeOutput/>" + ProjectFileGenerator.NewLine +
						"	</PropertyGroup>" + ProjectFileGenerator.NewLine, Combination.Platform, Utils.GetFilenameWithoutAnyExtensions(Combination.ProjectTarget.TargetFilePath), String.Join(", ", SupportedPlatforms.Select(x => x.ToString())));
				}
				else
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;
					string TargetFilePath = Combination.ProjectTarget.TargetFilePath;
					string TargetName = Utils.GetFilenameWithoutAnyExtensions(TargetFilePath);
					var UBTPlatformName = IsStubProject ? StubProjectPlatformName : Platform.ToString();
					var UBTConfigurationName = IsStubProject ? StubProjectConfigurationName : Configuration.ToString();

					// Setup output path
					var BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

					// Figure out if this is a monolithic build
					bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);
					bShouldCompileMonolithic |= TargetRulesObject.ShouldCompileMonolithic(Platform, Configuration);

					// Get the output directory
					string EngineRootDirectory = Path.GetFullPath(ProjectFileGenerator.EngineRelativePath);
					string RootDirectory = EngineRootDirectory;
                    if ((TargetRules.IsAGame(TargetRulesObject.Type) || TargetRulesObject.Type == TargetRules.TargetType.Server) && bShouldCompileMonolithic && !TargetRulesObject.bOutputToEngineBinaries)
					{
						if (UnrealBuildTool.HasUProjectFile() && Utils.IsFileUnderDirectory(TargetFilePath, UnrealBuildTool.GetUProjectPath()))
						{
							RootDirectory = Path.GetFullPath(UnrealBuildTool.GetUProjectPath());
						}
						else
						{
							string UnrealProjectPath = UProjectInfo.GetProjectFilePath(ProjectName);
							if (!String.IsNullOrEmpty(UnrealProjectPath))
							{
								RootDirectory = Path.GetDirectoryName(Path.GetFullPath(UnrealProjectPath));
							}
						}
					}

					if(TargetRulesObject.Type == TargetRules.TargetType.Program && !TargetRulesObject.bOutputToEngineBinaries)
					{
						string UnrealProjectPath = UProjectInfo.GetProjectForTarget(TargetName);
						if (!String.IsNullOrEmpty(UnrealProjectPath))
						{
							RootDirectory = Path.GetDirectoryName(Path.GetFullPath(UnrealProjectPath));
						}
					}

					// Get the output directory
					string OutputDirectory = Path.Combine(RootDirectory, "Binaries", UBTPlatformName);

					// Get the executable name (minus any platform or config suffixes)
					string BaseExeName = TargetName;
					if (!bShouldCompileMonolithic && TargetRulesObject.Type != TargetRules.TargetType.Program)
					{
						// Figure out what the compiled binary will be called so that we can point the IDE to the correct file
						string TargetConfigurationName = TargetRulesObject.ConfigurationName;
						if (TargetConfigurationName != TargetRules.TargetType.Game.ToString() && TargetConfigurationName != TargetRules.TargetType.Program.ToString())
						{
							BaseExeName = "UE4" + TargetConfigurationName;
						}
					}

					// Make the output file path
					string NMakePath = Path.Combine(OutputDirectory, BaseExeName);
					if (Configuration != UnrealTargetConfiguration.Development && (Configuration != UnrealTargetConfiguration.DebugGame || bShouldCompileMonolithic))
					{
						NMakePath += "-" + UBTPlatformName + "-" + UBTConfigurationName;
					}
					NMakePath += BuildPlatform.GetActiveArchitecture();
					NMakePath += BuildPlatform.GetBinaryExtension(UEBuildBinaryType.Executable);
					NMakePath = (BuildPlatform as UEBuildPlatform).ModifyNMakeOutput(NMakePath);

                    VCProjectFileContent.Append(
                        "	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine);

					string PathStrings = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioPathsEntries(Platform, Configuration, TargetRulesObject.Type, TargetFilePath, ProjectFilePath, NMakePath) : "";
					if (string.IsNullOrEmpty(PathStrings) || (PathStrings.Contains("<IntDir>") == false))
					{
						string ProjectRelativeUnusedDirectory = "$(ProjectDir)..\\Build\\Unused";
						VCProjectFileContent.Append(
							PathStrings +
							"		<OutDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</OutDir>" + ProjectFileGenerator.NewLine +
							"		<IntDir>" + ProjectRelativeUnusedDirectory + Path.DirectorySeparatorChar + "</IntDir>" + ProjectFileGenerator.NewLine);
					}
					else
					{
						VCProjectFileContent.Append(PathStrings);
					}

					if (TargetRules.IsGameType(TargetRulesObject.Type) &&
						(TargetRules.IsEditorType(TargetRulesObject.Type) == false))
					{
						// Allow platforms to add any special properties they require... like aumid override for Xbox One
						UEPlatformProjectGenerator.GenerateGamePlatformSpecificProperties(Platform, Configuration, TargetRulesObject.Type, VCProjectFileContent, RootDirectory, TargetFilePath);
					}

					// This is the standard UE4 based project NMake build line:
					//	..\..\Build\BatchFiles\Build.bat <TARGETNAME> <PLATFORM> <CONFIGURATION>
					//	ie ..\..\Build\BatchFiles\Build.bat BlankProgram Win64 Debug

					string BuildArguments = " " + TargetName + " " + UBTPlatformName + " " + UBTConfigurationName;
					if(ProjectFileGenerator.bUsePrecompiled)
					{
						BuildArguments += " -useprecompiled";
					}
					if (IsForeignProject)
					{
						BuildArguments += " " + UProjectPath + (UnrealBuildTool.RunningRocket() ? " -rocket" : "");
					}
					string BatchFilesDirectoryName = Path.Combine(ProjectFileGenerator.EngineRelativePath, "Build", "BatchFiles");

					// NMake Build command line
					VCProjectFileContent.Append("		<NMakeBuildCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(Path.Combine(BatchFilesDirectoryName, "Build.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeBuildCommandLine>" + ProjectFileGenerator.NewLine);

					// NMake ReBuild command line
					VCProjectFileContent.Append("		<NMakeReBuildCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(Path.Combine(BatchFilesDirectoryName, "Rebuild.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeReBuildCommandLine>" + ProjectFileGenerator.NewLine);

					// NMake Clean command line
					VCProjectFileContent.Append("		<NMakeCleanCommandLine>");
					VCProjectFileContent.Append(EscapePath(NormalizeProjectPath(Path.Combine(BatchFilesDirectoryName, "Clean.bat"))) + BuildArguments.ToString());
					VCProjectFileContent.Append("</NMakeCleanCommandLine>" + ProjectFileGenerator.NewLine);

					VCProjectFileContent.Append("		<NMakeOutput>");
					VCProjectFileContent.Append(NormalizeProjectPath(NMakePath));
					VCProjectFileContent.Append("</NMakeOutput>" + ProjectFileGenerator.NewLine);
				    VCProjectFileContent.Append("	</PropertyGroup>" + ProjectFileGenerator.NewLine);

                    string LayoutDirString = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioLayoutDirSection(Platform, Configuration, ConditionString, Combination.ProjectTarget.TargetRules.Type, Combination.ProjectTarget.TargetFilePath, ProjectFilePath, NMakePath) : "";
                    VCProjectFileContent.Append(LayoutDirString);
                }

				if (VCUserFileContent != null && Combination.ProjectTarget != null)
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;

					if ((Platform == UnrealTargetPlatform.Win32) || (Platform == UnrealTargetPlatform.Win64))
					{
						VCUserFileContent.Append(
							"	<PropertyGroup " + ConditionString + ">" + ProjectFileGenerator.NewLine);
						if (TargetRulesObject.Type != TargetRules.TargetType.Game)
						{
							string DebugOptions = "";
							
							if(IsForeignProject)
							{
								DebugOptions += UProjectPath;
							}
							else if(TargetRulesObject.Type == TargetRules.TargetType.Editor && ProjectName != "UE4")
							{
								DebugOptions += ProjectName;
							}

							if (Configuration == UnrealTargetConfiguration.Debug || Configuration == UnrealTargetConfiguration.DebugGame)
							{
								DebugOptions += " -debug";
							}
							else if (Configuration == UnrealTargetConfiguration.Shipping)
							{
								DebugOptions += " -shipping";
							}

							VCUserFileContent.Append(
								"		<LocalDebuggerCommandArguments>" + DebugOptions + "</LocalDebuggerCommandArguments>" + ProjectFileGenerator.NewLine
								);
						}
						VCUserFileContent.Append(
							"		<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>" + ProjectFileGenerator.NewLine
							);
						VCUserFileContent.Append(
							"	</PropertyGroup>" + ProjectFileGenerator.NewLine
							);
					}

					string PlatformUserFileStrings = (ProjGenerator != null) ? ProjGenerator.GetVisualStudioUserFileStrings(Platform, Configuration, ConditionString, TargetRulesObject, Combination.ProjectTarget.TargetFilePath, ProjectFilePath) : "";
					VCUserFileContent.Append(PlatformUserFileStrings);
				}
			}
		}
	}


	/** A Visual C# project. */
	public class VCSharpProjectFile : MSBuildProjectFile
	{
		// This is the GUID that Visual Studio uses to identify a C# project file in the solution
		public override string ProjectTypeGUID
		{
			get { return "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public VCSharpProjectFile( string InitFilePath )
			: base( InitFilePath )
		{
		}


		/** Reads the list of dependencies from the specified project file. */
		public List<string> GetCSharpDependencies()
		{
			var RelativeFilePaths = new List<string>();
			var Doc = new XmlDocument();
			Doc.Load( ProjectFilePath );

			var Tags = new string[] { "Compile", "Page", "Resource" };
			foreach( var Tag in Tags )
			{
				var Elements = Doc.GetElementsByTagName( Tag );
				foreach( XmlElement Element in Elements )
				{
					RelativeFilePaths.Add( Element.GetAttribute( "Include" ) );
				}
			}

			return RelativeFilePaths;
		}

		/**
		 * Adds a C# dot net (system) assembly reference to this project
		 *
		 * @param	AssemblyReference	The full path to the assembly file on disk
		 */
		public void AddDotNetAssemblyReference(string AssemblyReference)
		{
			if (!DotNetAssemblyReferences.Contains(AssemblyReference))
			{
				DotNetAssemblyReferences.Add(AssemblyReference);
			}
		}

		/**
		 * Adds a C# assembly reference to this project, such as a third party assembly needed for this project to compile
		 *
		 * @param	AssemblyReference	The full path to the assembly file on disk
		 */
		public void AddAssemblyReference( string AssemblyReference )
		{
			AssemblyReferences.Add( AssemblyReference );
		}

		/// <summary>
		/// Basic csproj file support. Generates C# library project with one build config.
		/// </summary>
		/// <param name="InPlatforms">Not used.</param>
		/// <param name="InConfigurations">Not Used.</param>
		/// <returns>true if the opration was successful, false otherwise</returns>
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			bool bSuccess = true;

			// Setup C# project file content.
			var ProjectFileContent = new StringBuilder();

			// Project file header.
			ProjectFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				"<Project DefaultTargets=\"Build\" ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

			ProjectFileContent.Append(
				"<Import Project=\"$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props\" Condition=\"Exists('$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props')\" />" +
 					ProjectFileGenerator.NewLine);

			// Support single configuration only (for now).
			ProjectFileContent.Append(
					"<PropertyGroup Condition=\" '$(Configuration)|$(Platform)' == 'Development|AnyCPU' \">" + ProjectFileGenerator.NewLine +
					"\t<DebugType>pdbonly</DebugType>" + ProjectFileGenerator.NewLine +
					"\t<Optimize>true</Optimize>" + ProjectFileGenerator.NewLine +
					"\t<OutputPath>bin\\Release\\</OutputPath>" + ProjectFileGenerator.NewLine +
					"\t<DefineConstants>TRACE</DefineConstants>" + ProjectFileGenerator.NewLine +
					"\t<ErrorReport>prompt</ErrorReport>" + ProjectFileGenerator.NewLine +
					"\t<WarningLevel>4</WarningLevel>" + ProjectFileGenerator.NewLine +
                    "\t<TreatWarningsAsErrors>true</TreatWarningsAsErrors>" + ProjectFileGenerator.NewLine +
				"</PropertyGroup>" + ProjectFileGenerator.NewLine);

			ProjectFileContent.Append(
				"<PropertyGroup>" + ProjectFileGenerator.NewLine +
					"\t<OutputType>Library</OutputType>" + ProjectFileGenerator.NewLine +
					"\t<ProjectGuid>" + ProjectGUID.ToString("B").ToUpperInvariant() + "</ProjectGuid>" + ProjectFileGenerator.NewLine +
					"\t<TargetFrameworkVersion>v4.0</TargetFrameworkVersion>" + ProjectFileGenerator.NewLine +
				"\t</PropertyGroup>" + ProjectFileGenerator.NewLine);

			// Basic .Net references
			if (DotNetAssemblyReferences.Count > 0)
			{
				ProjectFileContent.Append("<ItemGroup>" + ProjectFileGenerator.NewLine);
				foreach (var CurReference in DotNetAssemblyReferences)
				{
					ProjectFileContent.Append("\t<Reference Include=\"" + CurReference + "\" />" + ProjectFileGenerator.NewLine);
				}
				ProjectFileContent.Append("</ItemGroup>" + ProjectFileGenerator.NewLine);
			}
			// External or third party assembly references
			if( AssemblyReferences.Count > 0 )
			{
				ProjectFileContent.Append( "<ItemGroup>" + ProjectFileGenerator.NewLine );
				foreach( var CurReference in AssemblyReferences )
				{
					ProjectFileContent.Append( "\t<Reference Include=\"" + Path.GetFileNameWithoutExtension( CurReference ) + "\" >" + ProjectFileGenerator.NewLine );
					ProjectFileContent.Append( "\t\t<HintPath>" + Utils.MakePathRelativeTo( CurReference, Path.GetDirectoryName(ProjectFilePath) ) + "</HintPath>" + ProjectFileGenerator.NewLine );
					ProjectFileContent.Append( "\t</Reference>" + ProjectFileGenerator.NewLine );
				}
				ProjectFileContent.Append( "</ItemGroup>" + ProjectFileGenerator.NewLine );
			}

			// Other references (note it's assumed all references here are at least of MSBuildProjectFile type.
			foreach (var Project in DependsOnProjects)
			{
				var RelativePath = Utils.MakePathRelativeTo(Path.GetDirectoryName(Project.ProjectFilePath), Path.GetDirectoryName(ProjectFilePath));
				RelativePath = Path.Combine(RelativePath, Path.GetFileName(Project.ProjectFilePath));
				ProjectFileContent.Append(
					"<ItemGroup>" + ProjectFileGenerator.NewLine +
						"<ProjectReference Include=\"" + RelativePath + "\">" + ProjectFileGenerator.NewLine +
							"<Project>" + ((MSBuildProjectFile)Project).ProjectGUID.ToString("B").ToUpperInvariant() + "</Project>" + ProjectFileGenerator.NewLine +
							"<Name>" + Path.GetFileNameWithoutExtension(RelativePath) + "</Name>" + ProjectFileGenerator.NewLine +
						"</ProjectReference>" + ProjectFileGenerator.NewLine +
					"</ItemGroup>" + ProjectFileGenerator.NewLine);
			}

			// Source files.
			ProjectFileContent.Append(
				"	<ItemGroup>" + ProjectFileGenerator.NewLine );
			// Add all files to the project.
			foreach( var CurFile in SourceFiles )
			{
				var ProjectRelativeSourceFile = Utils.MakePathRelativeTo( CurFile.FilePath, Path.GetDirectoryName( ProjectFilePath ) );
				ProjectFileContent.Append(
					"		<Compile Include=\"" + ProjectRelativeSourceFile + "\" />" + ProjectFileGenerator.NewLine);
			}
			ProjectFileContent.Append(
				"	</ItemGroup>" + ProjectFileGenerator.NewLine );

			ProjectFileContent.Append(
				"<Import Project=\"$(MSBuildToolsPath)\\Microsoft.CSharp.targets\" />" + ProjectFileGenerator.NewLine);

			ProjectFileContent.Append(
				"</Project>" + ProjectFileGenerator.NewLine );

			// Save the project file
			if (bSuccess)
			{
				bSuccess = ProjectFileGenerator.WriteFileIfChanged(ProjectFilePath, ProjectFileContent.ToString());
			}

			return bSuccess;
		}

		/// Assemblies this project is dependent on
		protected readonly List<string> AssemblyReferences = new List<string>();
		/// System assemblies this project is dependent on
		protected readonly List<string> DotNetAssemblyReferences = new List<string>() { "System", "System.Core", "System.Data", "System.Xml" };
	}


	/// <summary>
	/// A Sandcastle Help File Builder project
	/// </summary>
	public class VSHFBProjectFile : MSBuildProjectFile
	{
		// This is the GUID that Visual Studio uses to identify a Sandcastle Help File project file in the solution - note the lack of {}
		public override string ProjectTypeGUID
		{
			get { return "0074e5b6-dd35-4f2c-9ede-f6259f61e92c"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public VSHFBProjectFile( string InitFilePath )
			: base( InitFilePath )
		{
		}
	}
}
