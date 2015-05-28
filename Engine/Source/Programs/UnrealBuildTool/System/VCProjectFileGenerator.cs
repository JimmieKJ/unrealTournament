// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a folder within the master project (e.g. Visual Studio solution)
	/// </summary>
	public class VisualStudioSolutionFolder : MasterProjectFolder
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public VisualStudioSolutionFolder( ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName )
			: base(InitOwnerProjectFileGenerator, InitFolderName )
		{
			// Generate a unique GUID for this folder
			// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
			//       matches identically with the pre-existing file
			FolderGUID = Guid.NewGuid();
		}


		/// GUID for this folder
		public Guid FolderGUID
		{
			get;
			private set;
		}
	}


	/// <summary>
	/// Visual C++ project file generator implementation
	/// </summary>
	public class VCProjectFileGenerator : ProjectFileGenerator
	{
		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".vcxproj";
			}
		}

		/// <summary>
		/// </summary>
		public override void CleanProjectFiles(string InMasterProjectRelativePath, string InMasterProjectName, string InIntermediateProjectFilesPath)
		{
			string MasterProjectFile = Path.Combine(InMasterProjectRelativePath, InMasterProjectName);
			string MasterProjDeleteFilename = MasterProjectFile + ".sln";
			if (File.Exists(MasterProjDeleteFilename))
			{
				File.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".sdf";
			if (File.Exists(MasterProjDeleteFilename))
			{
				File.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".suo";
			if (File.Exists(MasterProjDeleteFilename))
			{
				File.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".v11.suo";
			if (File.Exists(MasterProjDeleteFilename))
			{
				File.Delete(MasterProjDeleteFilename);
			}

			// Delete the project files folder
			if (Directory.Exists(InIntermediateProjectFilesPath))
			{
				try
				{
					Directory.Delete(InIntermediateProjectFilesPath, true);
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Error while trying to clean project files path {0}. Ignored.", InIntermediateProjectFilesPath);
					Log.TraceInformation("\t" + Ex.Message);
				}
			}
		}

		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile( string InitFilePath )
		{
			return new VCProjectFile( InitFilePath );
		}


		/// ProjectFileGenerator interface
		public override MasterProjectFolder AllocateMasterProjectFolder( ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName )
		{
			return new VisualStudioSolutionFolder( InitOwnerProjectFileGenerator, InitFolderName );
		}


		public enum VCProjectFileFormat
		{
			VisualStudio2012,
			VisualStudio2013
		}

		/// Which version of Visual Studio we should generate project files for
		static public VCProjectFileFormat ProjectFileFormat = 
			( WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2012 ) ? VCProjectFileFormat.VisualStudio2012 : VCProjectFileFormat.VisualStudio2013;


		/// This is the platform name that Visual Studio is always guaranteed to support.  We'll use this as
		/// a platform for any project configurations where our actual platform is not supported by the
		/// installed version of Visual Studio (e.g, "iOS")
		static public readonly string DefaultPlatformName = "Win32";

		/// The platform name that must be used for .NET projects
		static public readonly string DotNetPlatformName = "Any CPU";


		/// <summary>
		/// Configures project generator based on command-line options
		/// </summary>
		/// <param name="Arguments">Arguments passed into the program</param>
		/// <param name="IncludeAllPlatforms">True if all platforms should be included</param>
		protected override void ConfigureProjectFileGeneration( String[] Arguments, ref bool IncludeAllPlatforms )
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration( Arguments, ref IncludeAllPlatforms );

			// Default the project file format to whatever UBT is using
			{
				switch( WindowsPlatform.Compiler )
				{
					case WindowsCompiler.VisualStudio2012:
						ProjectFileFormat = VCProjectFileFormat.VisualStudio2012;
						break;

					case WindowsCompiler.VisualStudio2013:
						ProjectFileFormat = VCProjectFileFormat.VisualStudio2013;
						break;
				}
			}

			foreach( var CurArgument in Arguments )
			{
				switch( CurArgument.ToUpperInvariant() )
				{
					case "-2012":
						ProjectFileFormat = VCProjectFileFormat.VisualStudio2012;
						break;

					case "-2013":
						ProjectFileFormat = VCProjectFileFormat.VisualStudio2013;
						break;
				}
			}
		}


		/// <summary>
		/// Selects which platforms and build configurations we want in the project file
		/// </summary>
		/// <param name="IncludeAllPlatforms">True if we should include ALL platforms that are supported on this machine.  Otherwise, only desktop platforms will be included.</param>
		/// <param name="SupportedPlatformNames">Output string for supported platforms, returned as comma-separated values.</param>
		protected override void SetupSupportedPlatformsAndConfigurations( bool IncludeAllPlatforms, out string SupportedPlatformNames )
		{
			// Call parent implementation to figure out the actual platforms
			base.SetupSupportedPlatformsAndConfigurations( IncludeAllPlatforms, out SupportedPlatformNames );

			// Certain platforms override the project file format because their debugger add-ins may not yet support the latest
			// version of Visual Studio.  This is their chance to override that.
			foreach( var SupportedPlatform in SupportedPlatforms )
			{
				var BuildPlatform = UEBuildPlatform.GetBuildPlatform( SupportedPlatform, true );				
				if( BuildPlatform != null )
				{
					// Don't worry about platforms that we're missing SDKs for
					if (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
					{						
						// Make sure Visual Studio 2013 project files will work...
						if( ProjectFileFormat == VCProjectFileFormat.VisualStudio2013 )
						{
							// ...but only if the user didn't override this via the command-line.
							if( !UnrealBuildTool.CommandLineContains( "-2013" ) )
							{ 
								// Visual Studio 2013 is not supported by Xbox One debugger add-in yet
								if( SupportedPlatform == UnrealTargetPlatform.XboxOne )
								{
									Log.TraceInformation( "Forcing Visual Studio 2012 projects for Xbox One compatibility (use '-2013' to override.)");
									ProjectFileFormat = VCProjectFileFormat.VisualStudio2012;
								}
							}
						}
					}					
				}
			}
		}


		/// <summary>
		/// Used to sort VC solution config names along with the config and platform values
		/// </summary>
		class VCSolutionConfigCombination
		{
			/// Visual Studio solution configuration name for this config+platform
			public string VCSolutionConfigAndPlatformName;

			/// Configuration name
			public UnrealTargetConfiguration Configuration;

			/// Platform name
			public UnrealTargetPlatform Platform;

			/// The target configuration name
			public string TargetConfigurationName;

			public override string ToString()
			{
				return String.Format("{0}={1} {2} {3}", VCSolutionConfigAndPlatformName, Configuration, Platform, TargetConfigurationName);
			}
		}


		/// <summary>
		/// Composes a string to use for the Visual Studio solution configuration, given a build configuration and target rules configuration name
		/// </summary>
		/// <param name="Configuration">The build configuration</param>
		/// <param name="TargetConfigurationName">The target rules configuration name</param>
		/// <returns>The generated solution configuration name</returns>
		string MakeSolutionConfigurationName( UnrealTargetConfiguration Configuration, string TargetConfigurationName )
		{
			var SolutionConfigName = Configuration.ToString();

			// Don't bother postfixing "Game" or "Program" -- that will be the default when using "Debug", "Development", etc.
			// Also don't postfix "RocketGame" when we're building Rocket game projects.  That's the only type of game there is in that case!
			if( !TargetConfigurationName.Equals( TargetRules.TargetType.Game.ToString(), StringComparison.InvariantCultureIgnoreCase ) && 
				!TargetConfigurationName.Equals( TargetRules.TargetType.Program.ToString(), StringComparison.InvariantCultureIgnoreCase ) )
			{
				SolutionConfigName += " " + TargetConfigurationName;
			}

			return SolutionConfigName;
		}



		protected override bool WriteMasterProjectFile( ProjectFile UBTProject )
		{
			bool bSuccess = true;

			var SolutionFileName = MasterProjectName + ".sln";

			// Setup solution file content
			var VCSolutionFileContent = new StringBuilder();

			const string VersionTag = "# UnrealEngineGeneratedSolutionVersion=1.0";

			// Solution file header
			if( ProjectFileFormat == VCProjectFileFormat.VisualStudio2013 )
			{
				VCSolutionFileContent.Append(
					ProjectFileGenerator.NewLine +
					"Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
					"# Visual Studio 2013" + ProjectFileGenerator.NewLine +
					VersionTag + ProjectFileGenerator.NewLine);
				
				/* This is not required by VS 2013 to load the projects
				VCSolutionFileContent.Append(
					"VisualStudioVersion = 12.0.20617.1 PREVIEW" + ProjectFileGenerator.NewLine +
					"MinimumVisualStudioVersion = 10.0.40219.1" + ProjectFileGenerator.NewLine );*/
			}
			else if( ProjectFileFormat == VCProjectFileFormat.VisualStudio2012 )
			{
				VCSolutionFileContent.Append(
					ProjectFileGenerator.NewLine +
					"Microsoft Visual Studio Solution File, Format Version 12.00" + ProjectFileGenerator.NewLine +
					"# Visual Studio 2012" + ProjectFileGenerator.NewLine +
					VersionTag + ProjectFileGenerator.NewLine);
			}
			else
			{
				throw new BuildException( "Unexpected ProjectFileFormat" );
			}


			// Solution folders, files and project entries
			{
				// This the GUID that Visual Studio uses to identify a solution folder
				var SolutionFolderEntryGUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}";

				// Solution folders
				{
					var AllSolutionFolders = new List<MasterProjectFolder>();
					System.Action< List<MasterProjectFolder> /* Folders */ > GatherFoldersFunction = null;
					GatherFoldersFunction = FolderList =>
						{
							AllSolutionFolders.AddRange( FolderList );
							foreach( var CurSubFolder in FolderList )
							{
								GatherFoldersFunction( CurSubFolder.SubFolders );
							}
						};
					GatherFoldersFunction( RootFolder.SubFolders );

					foreach( VisualStudioSolutionFolder CurFolder in AllSolutionFolders )
					{
						var FolderGUIDString = CurFolder.FolderGUID.ToString( "B" ).ToUpperInvariant();
						VCSolutionFileContent.Append(
								"Project(\"" + SolutionFolderEntryGUID + "\") = \"" + CurFolder.FolderName + "\", \"" + CurFolder.FolderName + "\", \"" + FolderGUIDString + "\"" + ProjectFileGenerator.NewLine );

						// Add any files that are inlined right inside the solution folder
						if( CurFolder.Files.Count > 0 )
						{
							VCSolutionFileContent.Append(
									"	ProjectSection(SolutionItems) = preProject" + ProjectFileGenerator.NewLine );
							foreach( var CurFile in CurFolder.Files )
							{
								// Syntax is:  <relative file path> = <relative file path>
								VCSolutionFileContent.Append(
									"		" + CurFile + " = " + CurFile + ProjectFileGenerator.NewLine );
							}
							VCSolutionFileContent.Append(
									"	EndProjectSection" + ProjectFileGenerator.NewLine );
						}
						
						VCSolutionFileContent.Append(
								"EndProject" + ProjectFileGenerator.NewLine
							);
					}
				}


				// Project files
				foreach( MSBuildProjectFile CurProject in AllProjectFiles )
				{
					// Visual Studio uses different GUID types depending on the project type
					string ProjectTypeGUID = CurProject.ProjectTypeGUID;

					// NOTE: The project name in the solution doesn't actually *have* to match the project file name on disk.  However,
					//       we prefer it when it does match so we use the actual file name here.
					var ProjectNameInSolution = Path.GetFileNameWithoutExtension(CurProject.ProjectFilePath);

					// Use the existing project's GUID that's already known to us
					var ProjectGUID = CurProject.ProjectGUID.ToString( "B" ).ToUpperInvariant();

					VCSolutionFileContent.Append(
							"Project(\"" + ProjectTypeGUID + "\") = \"" + ProjectNameInSolution + "\", \"" + CurProject.RelativeProjectFilePath + "\", \"" + ProjectGUID + "\"" + ProjectFileGenerator.NewLine );

					// Setup dependency on UnrealBuildTool, if we need that.  This makes sure that UnrealBuildTool is
					// freshly compiled before kicking off any build operations on this target project
					if ( !CurProject.IsStubProject )
					{
						var Dependencies = new List<ProjectFile>();
						if (CurProject.IsGeneratedProject && UBTProject != null && CurProject != UBTProject)
						{
							Dependencies.Add(UBTProject);
							Dependencies.AddRange(UBTProject.DependsOnProjects);
						}
						Dependencies.AddRange(CurProject.DependsOnProjects);

						if (Dependencies.Count > 0)
						{
							VCSolutionFileContent.Append("\tProjectSection(ProjectDependencies) = postProject" + ProjectFileGenerator.NewLine);

							// Setup any addition dependencies this project has...
							foreach (var DependsOnProject in Dependencies)
							{
								var DependsOnProjectGUID = ((MSBuildProjectFile)DependsOnProject).ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.Append("\t\t" + DependsOnProjectGUID + " = " + DependsOnProjectGUID + ProjectFileGenerator.NewLine);
							}

							VCSolutionFileContent.Append("\tEndProjectSection" + ProjectFileGenerator.NewLine);
						}
					}

					VCSolutionFileContent.Append(
							"EndProject" + ProjectFileGenerator.NewLine
						);
				}
			}

			// Solution configuration platforms.  This is just a list of all of the platforms and configurations that
			// appear in Visual Studio's build configuration selector.
			var SolutionConfigCombinations = new List<VCSolutionConfigCombination>();

			// The "Global" section has source control, solution configurations, project configurations,
			// preferences, and project hierarchy data
			{
				VCSolutionFileContent.Append(
					"Global" + ProjectFileGenerator.NewLine );
				{
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(SolutionConfigurationPlatforms) = preSolution" + ProjectFileGenerator.NewLine );

						var SolutionConfigurationsValidForProjects = new Dictionary<string, Tuple< UnrealTargetConfiguration, string>>();
						var PlatformsValidForProjects = new HashSet<UnrealTargetPlatform>();

						foreach( var CurConfiguration in SupportedConfigurations )
						{
							if (UnrealBuildTool.IsValidConfiguration(CurConfiguration))
							{
								foreach (var CurPlatform in SupportedPlatforms)
								{
									if (UnrealBuildTool.IsValidPlatform(CurPlatform))
									{
										foreach( var CurProject in AllProjectFiles )
										{
											if( !CurProject.IsStubProject )
											{
												if( CurProject.ProjectTargets.Count == 0 )
												{
													throw new BuildException( "Expecting project '" + CurProject.ProjectFilePath + "' to have at least one ProjectTarget associated with it!" );
												}

												// Figure out the set of valid target configuration names
												foreach( var ProjectTarget in CurProject.ProjectTargets )
												{
													if( VCProjectFile.IsValidProjectPlatformAndConfiguration( ProjectTarget, CurPlatform, CurConfiguration ) )
													{
														PlatformsValidForProjects.Add( CurPlatform );

														// Default to a target configuration name of "Game", since that will collapse down to an empty string
														var TargetConfigurationName = TargetRules.TargetType.Game.ToString();
														if( ProjectTarget.TargetRules != null )
														{
															TargetConfigurationName = ProjectTarget.TargetRules.ConfigurationName;
														}

														var SolutionConfigName = MakeSolutionConfigurationName( CurConfiguration, TargetConfigurationName );
														SolutionConfigurationsValidForProjects[ SolutionConfigName ] = new Tuple<UnrealTargetConfiguration, string>( CurConfiguration, TargetConfigurationName );
													}
												}
											}
										}
									}
								}
							}
						}

						foreach( var CurPlatform in PlatformsValidForProjects )
						{
							foreach( var SolutionConfigKeyValue in SolutionConfigurationsValidForProjects )
							{
								// e.g.  "Development|Win64 = Development|Win64"
								var SolutionConfigName = SolutionConfigKeyValue.Key;
								var Configuration = SolutionConfigKeyValue.Value.Item1;
								var TargetConfigurationName = SolutionConfigKeyValue.Value.Item2;

								var SolutionPlatformName = CurPlatform.ToString();

								var SolutionConfigAndPlatformPair = SolutionConfigName + "|" + SolutionPlatformName;
								SolutionConfigCombinations.Add(
										new VCSolutionConfigCombination
										{
											VCSolutionConfigAndPlatformName = SolutionConfigAndPlatformPair,
											Configuration = Configuration,
											Platform = CurPlatform,
											TargetConfigurationName = TargetConfigurationName
										}
									);
							}
						}

						// Sort the list of solution platform strings alphabetically (Visual Studio prefers it)
						SolutionConfigCombinations.Sort(
								new Comparison< VCSolutionConfigCombination >(
									( x, y ) => { return String.Compare( x.VCSolutionConfigAndPlatformName, y.VCSolutionConfigAndPlatformName, StringComparison.InvariantCultureIgnoreCase ); } 
								) 
							);

						var AppendedSolutionConfigAndPlatformNames = new HashSet<string>( StringComparer.InvariantCultureIgnoreCase );
						foreach( var SolutionConfigCombination in SolutionConfigCombinations )
						{
							// We alias "Game" and "Program" to both have the same solution configuration, so we're careful not to add the same combination twice.
							if( !AppendedSolutionConfigAndPlatformNames.Contains( SolutionConfigCombination.VCSolutionConfigAndPlatformName ) )
							{
								VCSolutionFileContent.Append(
									"		" + SolutionConfigCombination.VCSolutionConfigAndPlatformName + " = " + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ProjectFileGenerator.NewLine );
								AppendedSolutionConfigAndPlatformNames.Add( SolutionConfigCombination.VCSolutionConfigAndPlatformName );
							}
						}

						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine );
					}


					// Assign each project's "project configuration" to our "solution platform + configuration" pairs.  This
					// also sets up which projects are actually built when building the solution.
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(ProjectConfigurationPlatforms) = postSolution" + ProjectFileGenerator.NewLine );

						var CombinationsThatWereMatchedToProjects = new List<VCSolutionConfigCombination>();

						foreach( MSBuildProjectFile CurProject in AllProjectFiles )
						{
							// NOTE: We don't emit solution configuration entries for "stub" projects.  Those projects are only
							// built using UnrealBuildTool and don't require a presence in the solution project list

							// NOTE: We also process projects that were "imported" here, hoping to match those to our solution
							//       configurations.  In some cases this may not be successful though.  Imported projects
							//       should always be carefully setup to match our project generator's solution configs.
							if( !CurProject.IsStubProject )
							{
								if( CurProject.ProjectTargets.Count == 0 )
								{
									throw new BuildException( "Expecting project '" + CurProject.ProjectFilePath + "' to have at least one ProjectTarget associated with it!" );
								}
								var IsProgramProject = CurProject.ProjectTargets[ 0 ].TargetRules != null && CurProject.ProjectTargets[ 0 ].TargetRules.Type == TargetRules.TargetType.Program;

								var GameOrProgramConfigsAlreadyMapped = new HashSet<string>();
								foreach( var SolutionConfigCombination in SolutionConfigCombinations )
								{
									// Handle aliasing of Program and Game target configuration names
									if( ( IsProgramProject && GameOrProgramConfigsAlreadyMapped.Add( SolutionConfigCombination.VCSolutionConfigAndPlatformName ) ) ||
										IsProgramProject && SolutionConfigCombination.TargetConfigurationName != TargetRules.TargetType.Game.ToString() ||
										!IsProgramProject && SolutionConfigCombination.TargetConfigurationName != TargetRules.TargetType.Program.ToString() )
									{
										var TargetConfigurationName = SolutionConfigCombination.TargetConfigurationName;
										if( IsProgramProject && TargetConfigurationName == TargetRules.TargetType.Game.ToString() )
										{
											TargetConfigurationName = TargetRules.TargetType.Program.ToString();
										}

										// Now, we want to find a target in this project that maps to the current solution config combination.  Only up to one target should
										// and every solution config combination should map to at least one target in one project (otherwise we shouldn't have added it!).
										ProjectTarget MatchingProjectTarget = null;
										foreach( var ProjectTarget in CurProject.ProjectTargets )
										{
											bool IsMatchingCombination = VCProjectFile.IsValidProjectPlatformAndConfiguration( ProjectTarget, SolutionConfigCombination.Platform, SolutionConfigCombination.Configuration );
											if( ProjectTarget.TargetRules != null )
											{
												if( TargetConfigurationName != ProjectTarget.TargetRules.ConfigurationName )
												{
													// Solution configuration name for this combination doesn't match this target's configuration name.  It's not buildable.
													IsMatchingCombination = false;
												}
											}
											else
											{
												// UBT gets a pass because it is a dependency of every single configuration combination
												if( CurProject != UBTProject &&
													!CurProject.ShouldBuildForAllSolutionTargets &&
													TargetConfigurationName != TargetRules.TargetType.Game.ToString() )
												{
													// Can't build non-generated project in configurations except for the default (Game)
													IsMatchingCombination = false;
												}
											}

											if( IsMatchingCombination )
											{
												if( MatchingProjectTarget != null )
												{
													// Not expecting more than one target to match a single solution configuration per project!
													throw new BuildException( "Not expecting more than one target for project " + CurProject.ProjectFilePath + " to match solution configuration " + SolutionConfigCombination.VCSolutionConfigAndPlatformName );
												}

												MatchingProjectTarget = ProjectTarget;

												// NOTE: For faster perf, we could "break" here and bail out early, but that would circumvent the error checking
												//		 for multiple targets within a project that may map to a single solution configuration.
											}
										}

										var SolutionConfiguration = SolutionConfigCombination.Configuration;
										var SolutionPlatform = SolutionConfigCombination.Platform;


										if( MatchingProjectTarget == null )
										{
											// The current configuration/platform and target configuration name doesn't map to anything our project actually supports.
											// We'll map it to a default config.
											SolutionConfiguration = UnrealTargetConfiguration.Development;

											// Prefer using Win64 as the default, but fall back to a platform the project file actually supports if needed.  This is for
											// projects that can never be compiled in Windows, such as UnrealLaunchDaemon which is an iOS-only program
											SolutionPlatform = UnrealTargetPlatform.Win64;
											if( CurProject.ProjectTargets[0].TargetRules != null )
											{
												var ProjectSupportedPlatforms = new List<UnrealTargetPlatform>();
												CurProject.ProjectTargets[0].TargetRules.GetSupportedPlatforms( ref ProjectSupportedPlatforms );
												if( !ProjectSupportedPlatforms.Contains( SolutionPlatform ) )
												{
													SolutionPlatform = ProjectSupportedPlatforms[ 0 ];
												}
											}


											if( IsProgramProject )
											{
												TargetConfigurationName = TargetRules.TargetType.Program.ToString();
											}
											else 
											{
												TargetConfigurationName = TargetRules.TargetType.Game.ToString();
											}
										}


										// If the project wants to always build in "Development", regardless of what the solution
										// configuration is set to, then we'll do that here.  This is used for projects like
										// UnrealBuildTool and ShaderCompileWorker
										if( MatchingProjectTarget != null )
										{
											if( MatchingProjectTarget.ForceDevelopmentConfiguration )
											{
												SolutionConfiguration = UnrealTargetConfiguration.Development;
											}
										}


										string ProjectConfigName;
										string ProjectPlatformName;
										CurProject.MakeProjectPlatformAndConfigurationNames( SolutionPlatform, SolutionConfiguration, TargetConfigurationName, out ProjectPlatformName, out ProjectConfigName );

										var ProjectConfigAndPlatformPair = ProjectConfigName.ToString() + "|" + ProjectPlatformName.ToString();

										// e.g.  "{4232C52C-680F-4850-8855-DC39419B5E9B}.Debug|iOS.ActiveCfg = iOS_Debug|Win32"
										var CurProjectGUID = CurProject.ProjectGUID.ToString( "B" ).ToUpperInvariant();
										VCSolutionFileContent.Append(
											"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".ActiveCfg = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine );


										// Set whether this project configuration should be built when the user initiates "build solution"
										if( MatchingProjectTarget != null && CurProject.ShouldBuildByDefaultForSolutionTargets )
										{
											VCSolutionFileContent.Append(
													"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".Build.0 = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine );

											var ProjGen = UEPlatformProjectGenerator.GetPlatformProjectGenerator(SolutionConfigCombination.Platform, true);
											if (MatchingProjectTarget.ProjectDeploys ||
												((ProjGen != null) && (ProjGen.GetVisualStudioDeploymentEnabled(SolutionPlatform, SolutionConfiguration) == true)))
											{
												VCSolutionFileContent.Append(
														"		" + CurProjectGUID + "." + SolutionConfigCombination.VCSolutionConfigAndPlatformName + ".Deploy.0 = " + ProjectConfigAndPlatformPair + ProjectFileGenerator.NewLine );
											}
										}

										CombinationsThatWereMatchedToProjects.Add( SolutionConfigCombination );
									}
								}
							}
						}

						// Check for problems
						foreach( var SolutionConfigCombination in SolutionConfigCombinations )
						{
							if( !CombinationsThatWereMatchedToProjects.Contains( SolutionConfigCombination ) )
							{
								throw new BuildException( "Unable to find a ProjectTarget that matches the solution configuration/platform mapping: " + SolutionConfigCombination.Configuration.ToString() + ", " + SolutionConfigCombination.Platform.ToString() + ", " + SolutionConfigCombination.TargetConfigurationName );
							}
						}
						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine );
					}


					// Setup other solution properties
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(SolutionProperties) = preSolution" + ProjectFileGenerator.NewLine );

						// HideSolutionNode sets whether or not the top-level solution entry is completely hidden in the UI.
						// We don't want that, as we need users to be able to right click on the solution tree item.
						VCSolutionFileContent.Append(
							"		HideSolutionNode = FALSE" + ProjectFileGenerator.NewLine );

						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine );
					}



					// Solution directory hierarchy
					{
						VCSolutionFileContent.Append(
							"	GlobalSection(NestedProjects) = preSolution" + ProjectFileGenerator.NewLine );

						// Every entry in this section is in the format "Guid1 = Guid2".  Guid1 is the child project (or solution
						// filter)'s GUID, and Guid2 is the solution filter directory to parent the child project (or solution
						// filter) to.  This sets up the hierarchical solution explorer tree for all solution folders and projects.

						System.Action<StringBuilder /* VCSolutionFileContent */, List<MasterProjectFolder> /* Folders */ > FolderProcessorFunction = null;
						FolderProcessorFunction = ( LocalVCSolutionFileContent, LocalMasterProjectFolders ) =>
							{
								foreach( VisualStudioSolutionFolder CurFolder in LocalMasterProjectFolders )
								{
									var CurFolderGUIDString = CurFolder.FolderGUID.ToString( "B" ).ToUpperInvariant();

									foreach( MSBuildProjectFile ChildProject in CurFolder.ChildProjects )
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.Append(
											"		" + ChildProject.ProjectGUID.ToString( "B" ).ToUpperInvariant() + " = " + CurFolderGUIDString + ProjectFileGenerator.NewLine );
									}

									foreach( VisualStudioSolutionFolder SubFolder in CurFolder.SubFolders )
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.Append(
											"		" + SubFolder.FolderGUID.ToString( "B" ).ToUpperInvariant() + " = " + CurFolderGUIDString + ProjectFileGenerator.NewLine );
									}

									// Recurse into subfolders
									FolderProcessorFunction( LocalVCSolutionFileContent, CurFolder.SubFolders );
								}
							};
						FolderProcessorFunction( VCSolutionFileContent, RootFolder.SubFolders );
						
						VCSolutionFileContent.Append(
							"	EndGlobalSection" + ProjectFileGenerator.NewLine );
					}
				}

				VCSolutionFileContent.Append(
					"EndGlobal" + ProjectFileGenerator.NewLine );
			}


			// Save the solution file
			if( bSuccess )
			{
				var SolutionFilePath = Path.Combine( MasterProjectRelativePath, SolutionFileName );
				bSuccess = WriteFileIfChanged( SolutionFilePath, VCSolutionFileContent.ToString() );
			}


			// Save a solution config file which selects the development editor configuration by default.
			if (bSuccess)
			{
				// Figure out the filename for the SUO file. VS2013 will automatically import the VS2012 options if necessary.
				string SolutionOptionsExtension = (ProjectFileFormat == VCProjectFileFormat.VisualStudio2012)? "v11.suo" : "v12.suo";

				// Check it doesn't exist before overwriting it. Since these files store the user's preferences, it'd be bad form to overwrite them.
				string SolutionOptionsFileName = Path.Combine(MasterProjectRelativePath, Path.ChangeExtension(SolutionFileName, SolutionOptionsExtension));
				if(!File.Exists(SolutionOptionsFileName))
				{
					VCSolutionOptions Options = new VCSolutionOptions();

					// Set the default configuration and startup project
					VCSolutionConfigCombination DefaultConfig = SolutionConfigCombinations.Find(x => x.Configuration == UnrealTargetConfiguration.Development && x.Platform == UnrealTargetPlatform.Win64 && x.TargetConfigurationName == "Editor");
					if (DefaultConfig != null)
					{
						List<VCBinarySetting> Settings = new List<VCBinarySetting>();
						Settings.Add(new VCBinarySetting("ActiveCfg", DefaultConfig.VCSolutionConfigAndPlatformName));
						if(DefaultProject != null)
						{
							Settings.Add(new VCBinarySetting("StartupProject", ((MSBuildProjectFile)DefaultProject).ProjectGUID.ToString("B")));
						}
						Options.SetConfiguration(Settings);
					}

					// Mark all the projects as closed by default, apart from the startup project
					VCSolutionExplorerState ExplorerState = new VCSolutionExplorerState();
					foreach(ProjectFile ProjectFile in AllProjectFiles)
					{
						string ProjectName = Path.GetFileNameWithoutExtension(ProjectFile.ProjectFilePath);
						if(ProjectFile == DefaultProject)
						{
							ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[]{ ProjectName }));
						}
						else
						{
							ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[]{ }));
						}
					}
					if( IncludeEnginePrograms )
					{
						ExplorerState.OpenProjects.Add(new Tuple<string, string[]>("Automation", new string[0]));
					}
					Options.SetExplorerState(ExplorerState);

					// Write the file
					if(Options.Sections.Count > 0)
					{
						Options.Write(SolutionOptionsFileName);
					}
				}
			}

			return bSuccess;
		}


		/// <summary>
		/// Takes a string and "cleans it up" to make it parsable by the Visual Studio source control provider's file format
		/// </summary>
		/// <param name="Str">String to clean up</param>
		/// <returns>The cleaned up string</returns>
		public string CleanupStringForSCC( string Str )
		{
			var Cleaned = Str;

			// SCC is expecting paths to contain only double-backslashes for path separators.  It's a bit weird but we need to do it.
			Cleaned = Cleaned.Replace( Path.DirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString() );
			Cleaned = Cleaned.Replace( Path.AltDirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString() );

			// SCC is expecting not to see spaces in these strings, so we'll replace spaces with "\u0020"
			Cleaned = Cleaned.Replace( " ", "\\u0020" );

			return Cleaned;
		}

	}

}
