// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

// @todo UWP: this file is a work in progress and is not yet complete for the F5 scenario for UWP

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	public class UWPProjectGenerator : UEPlatformProjectGenerator
	{
		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			if (UWPPlatform.bEnableUWPSupport)
			{
				// Register this project generator for UWP
				Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.UWP.ToString());

				UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.UWP, this);
			}
		}

		public static void GetTargetUWPPaths(RulesAssembly InTargetRulesAssembly, string InTargetName, TargetRules InTargetRules,
			out string OutEngineSourceRelativeBinaryPath, out string OutRelativeTargetPath)
		{
			OutEngineSourceRelativeBinaryPath = "";
			OutRelativeTargetPath = "";

			string TargetFilename = InTargetRulesAssembly.GetTargetFileName(InTargetName).FullName;

			string ProjectSourceFolder = new FileInfo(TargetFilename).DirectoryName;

			string EnginePath = Path.Combine(ProjectFileGenerator.EngineRelativePath);
			string EngineSourcePath = Path.Combine(EnginePath, "Source");
			string RelativeTargetFilename = Utils.MakePathRelativeTo(TargetFilename, EngineSourcePath);

			if ((RelativeTargetFilename.StartsWith("..") == false) && (RelativeTargetFilename.Contains(":") == false))
			{
				// This target must be UNDER Engine/Source...
				RelativeTargetFilename = Path.Combine(EngineSourcePath, RelativeTargetFilename);
			}
			RelativeTargetFilename = RelativeTargetFilename.Replace("\\", "/");
			EnginePath = EnginePath.Replace("\\", "/");

			Int32 LastSourceIdx = RelativeTargetFilename.LastIndexOf("Source");
			if (LastSourceIdx != -1)
			{
				RelativeTargetFilename = RelativeTargetFilename.Substring(0, LastSourceIdx);
			}
			else
			{
				RelativeTargetFilename = "";
			}

			OutRelativeTargetPath = RelativeTargetFilename;

			if (InTargetRules.bOutputToEngineBinaries)
			{
				RelativeTargetFilename = EnginePath;
			}
			OutEngineSourceRelativeBinaryPath = Path.Combine(RelativeTargetFilename, "Binaries/UWP/");
			OutEngineSourceRelativeBinaryPath = OutEngineSourceRelativeBinaryPath.Replace("\\", "/");
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Get whether this platform deploys
		/// </summary>
		/// <returns>bool  true if the 'Deploy' option should be enabled</returns>
		public override bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		private Dictionary<string, string> GameToBuildFolder = new Dictionary<string, string>();


		protected void GeneratePackageAppXManifest(string InGameName, bool bIsUE4Game, string InManifestType, string InGameBinaries, string InGameIntermediates, string InOutputPath)
		{
			string BuildFolder = Path.Combine(InOutputPath, "../../Build/UWP");
			string CheckPath;
			if (GameToBuildFolder.TryGetValue(InGameName, out CheckPath) == false)
			{
				GameToBuildFolder.Add(InGameName, BuildFolder);
			}

			string OutputPackageAppXManifestPath = Path.Combine(BuildFolder, InManifestType + ".AppxManifest");


			// first look for a per-game template
			string PackageAppXManifestPath = OutputPackageAppXManifestPath + ".template";
			if (!File.Exists(PackageAppXManifestPath))
			{
				// then fallback to shared template
				PackageAppXManifestPath = "../../Engine/Build/UWP/Templates/" + InManifestType + ".appxmanifest.template";
			}

			GenerateFileFromTemplate(PackageAppXManifestPath, OutputPackageAppXManifestPath, bIsUE4Game, InGameName, "", InGameBinaries, InGameIntermediates);

			// Generate the package name...
			// todo - richiem - this is next to fix
			string PackageName = "foobar"; //= XboxOneDeploy.XboxOnePackageNameHelper_Run(OutputPackageAppXManifestPath);
			if (string.IsNullOrEmpty(PackageName) == false)
			{
				if (GameToBuildFolder.TryGetValue(InGameName, out CheckPath) == false)
				{
					GameToBuildFolder.Add(InGameName, PackageName);
				}
			}
			else
			{
				throw new BuildException("Error: Failed to find package name for {0}", OutputPackageAppXManifestPath);
			}
		}

		protected void GeneratePackagePriConfig(string InGameName, bool bIsUE4Game, string InGameBinaries, string InGameIntermediates, string InOutputPath)
		{
			string BuildFolder = Path.Combine(InOutputPath, "../../Build/UWP");

			string OutputPackagePriConfigPath = Path.Combine(BuildFolder, "Package.PriConfig");

			// first look for a per-game template
			string PackagePriConfigPath = OutputPackagePriConfigPath + ".template";
			if (!File.Exists(PackagePriConfigPath))
			{
				// then fallback to shared template
				PackagePriConfigPath = "../../Engine/Build/UWP/Templates/Package.priconfig.template";
			}

			GenerateFileFromTemplate(PackagePriConfigPath, OutputPackagePriConfigPath, bIsUE4Game, InGameName, "", InGameBinaries, InGameIntermediates);
		}

		protected void GeneratePackageResources(string InGameName, bool bIsUE4Game, string InGameBinaries, string InGameIntermediates, string InOutputPath)
		{
			string BuildFolder = Path.Combine(InOutputPath, "../../Build/UWP");

			string OutputPackageResourcesPath = Path.Combine(BuildFolder, "Resources", "Resources.resw");

			// first look for a per-game template
			string PackageResourcesPath = Path.Combine(BuildFolder, "Resources", "Package.resources.template");
			if (!File.Exists(PackageResourcesPath))
			{
				// then fallback to shared template
				PackageResourcesPath = "../../Engine/Build/UWP/Templates/Package.resources.template";
			}

			// Only generate the example language if the resources haven't been setup at all previously
			if (!File.Exists(OutputPackageResourcesPath))
			{
				GenerateFileFromTemplate(PackageResourcesPath, Path.Combine(BuildFolder, "Resources", "en-US", "Resources.resw"), bIsUE4Game, InGameName, "", InGameBinaries, InGameIntermediates);
			}
			GenerateFileFromTemplate(PackageResourcesPath, OutputPackageResourcesPath, bIsUE4Game, InGameName, "", InGameBinaries, InGameIntermediates);
		}

		protected void GenerateFileFromTemplate(string InTemplateFile, string InOutputFile, bool bIsUE4Game, string InGameName, string InGameConfiguration, string InGameBinaries, string InGameIntermediates, string InGuidString = "", Dictionary<string, string> InExtraReplacements = null)
		{
			String ExecutableName = bIsUE4Game ? "UE4Game" : InGameName;

			StringBuilder outputContents = new StringBuilder();
			using (StreamReader reader = new StreamReader(InTemplateFile))
			{
				string LineStr = null;
				while (reader.Peek() != -1)
				{
					LineStr = reader.ReadLine();
					if (LineStr.Contains("%GAME%"))
					{
						if (LineStr.Contains("Executable") || LineStr.Contains("EntryPoint"))
						{
							LineStr = LineStr.Replace("%GAME%", ExecutableName);
						}
						else
						{
							LineStr = LineStr.Replace("%GAME%", InGameName);
						}
					}
					if (LineStr.Contains("%PROJECT%"))
					{
						if (bIsUE4Game)
						{
							LineStr = LineStr.Replace("%PROJECT%", "Engine");
						}
						else
						{
							LineStr = LineStr.Replace("%PROJECT%", InGameName);
						}
					}

					if (LineStr.Contains("%CONFIG%"))
					{
						LineStr = LineStr.Replace("%CONFIG%", InGameConfiguration);
					}

					if (LineStr.Contains("%DISPLAYCONFIG%"))
					{
						if (string.IsNullOrEmpty(InGameConfiguration))
						{
							LineStr = LineStr.Replace("%DISPLAYCONFIG%", "");
						}
						else
						{
							LineStr = LineStr.Replace("%DISPLAYCONFIG%", " (" + InGameConfiguration + ")");
						}
					}

					if (LineStr.Contains("%GAMEBINARIES%"))
					{
						LineStr = LineStr.Replace("%GAMEBINARIES%", InGameBinaries);
					}

					if (LineStr.Contains("%GAMEBINARIESFOLDER%"))
					{
						LineStr = LineStr.Replace("%GAMEBINARIESFOLDER%", InGameBinaries);
					}

					if (LineStr.Contains("%GAMEINTERMEDIATESFOLDER%"))
					{
						LineStr = LineStr.Replace("%GAMEINTERMEDIATESFOLDER%", InGameIntermediates);
					}

					if (LineStr.Contains("%GUID%"))
					{
						LineStr = LineStr.Replace("%GUID%", InGuidString);
					}

					if (InExtraReplacements != null)
					{
						foreach (KeyValuePair<string, string> Replacement in InExtraReplacements)
						{
							if (LineStr.Contains(Replacement.Key))
							{
								LineStr = LineStr.Replace(Replacement.Key, Replacement.Value);
							}
						}
					}

					outputContents.AppendLine(LineStr);
				}
			}

			if (outputContents.Length > 0)
			{
				bool bFileNeedsSave = true;

				if (File.Exists(InOutputFile))
				{
					// Read in the original file completely
					string LoadedFileContent = null;
					var FileAlreadyExists = File.Exists(InOutputFile);
					if (FileAlreadyExists)
					{
						try
						{
							LoadedFileContent = File.ReadAllText(InOutputFile);
						}
						catch (Exception)
						{
							Log.TraceInformation("Error while trying to load existing file {0}.  Ignored.", InOutputFile);
						}
					}

					// Don't bother saving anything out if the new file content is the same as the old file's content
					if (LoadedFileContent != null)
					{
						var bIgnoreProjectFileWhitespaces = true;
						if (ProjectFileComparer.CompareOrdinalIgnoreCase(LoadedFileContent, outputContents.ToString(), bIgnoreProjectFileWhitespaces) == 0)
						{
							// Exact match!
							bFileNeedsSave = false;
						}

						if (!bFileNeedsSave)
						{
							Log.TraceVerbose("Skipped saving {0} because contents haven't changed.", Path.GetFileName(InOutputFile));
						}
					}
				}

				if (bFileNeedsSave)
				{
					// Save the file
					try
					{
						Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
						File.WriteAllText(InOutputFile, outputContents.ToString(), Encoding.UTF8);
						Log.TraceVerbose("Saved {0}", Path.GetFileName(InOutputFile));
					}
					catch (Exception)
					{
						// Unable to write to the project file.
						Log.TraceInformation("Error while trying to write file {0}.  The file is probably read-only.", InOutputFile);
					}
				}
			}
		}


		protected void CopyDefaultImageFiles(string InOutputPath)
		{
			string OutputImageFolder = Path.Combine(InOutputPath, "../../Build/UWP/Resources");
			if (Directory.Exists(OutputImageFolder) == false)
			{
				Directory.CreateDirectory(OutputImageFolder);
			}

			string[] ImageList = new string[] { "Logo.png", "SmallLogo.png", "SplashScreen.png", "StoreLogo.png", "WideLogo.png" };

			foreach (string ImageName in ImageList)
			{
				string OutputImageFilePath = Path.Combine(OutputImageFolder, ImageName);
				if (File.Exists(OutputImageFilePath) == false)
				{
					string SourceImagePath = "../../Engine/Build/UWP/DefaultImages/" + ImageName;
					File.Copy(SourceImagePath, OutputImageFilePath);

					string OutputEnglishImageFilePath = Path.Combine(OutputImageFolder, "en-US", ImageName);
					if (Directory.Exists(OutputImageFolder + "/en-US") == false)
					{
						Directory.CreateDirectory(OutputImageFolder + "/en-US");
					}
					if (File.Exists(OutputEnglishImageFilePath) == false)
					{
						File.Copy(SourceImagePath, OutputEnglishImageFilePath);
					}
				}
			}
		}


		public void GenerateDeployFiles(string InTargetName, string InTargetFilepath, bool bIsUE4Game, string EngineSourceRelativeBinaryPath)
		{
			const string SourceFolderString = "/Source/";
			string OutTargetPath = InTargetFilepath;
			OutTargetPath = OutTargetPath.Replace("\\", "/");
			if (bIsUE4Game == false)
			{
				int LastSourceIndex = OutTargetPath.LastIndexOf(SourceFolderString);
				if (LastSourceIndex != -1)
				{
					OutTargetPath = OutTargetPath.Substring(0, LastSourceIndex + SourceFolderString.Length);
					OutTargetPath = Path.Combine(OutTargetPath, "UWP");
				}
				else
				{
					// @todo UWP: This is bad... throw a build exception?
				}
			}
			else
			{
				OutTargetPath = Path.Combine((new FileInfo(InTargetFilepath)).DirectoryName, "UWP");
			}

			string GameBinariesFolder = Utils.MakePathRelativeTo(EngineSourceRelativeBinaryPath, OutTargetPath);
			GameBinariesFolder = GameBinariesFolder.Replace("\\", "/");
			if (GameBinariesFolder.EndsWith("/") == false)
			{
				GameBinariesFolder += "/";
			}
			string GameIntermediatesFolder = GameBinariesFolder.Replace("Binaries/WinAUP", "Intermediate/Build/Unused");

			// Make sure the output directory exists...
			if (Directory.Exists(OutTargetPath) == false)
			{
				Log.TraceVerbose("Generating output directory " + OutTargetPath);
				Directory.CreateDirectory(OutTargetPath);
			}

			GeneratePackageAppXManifest(InTargetName, bIsUE4Game, "Package", GameBinariesFolder, GameIntermediatesFolder, OutTargetPath);
			GeneratePackageAppXManifest(InTargetName, bIsUE4Game, "Deploy", GameBinariesFolder, GameIntermediatesFolder, OutTargetPath);
			GeneratePackagePriConfig(InTargetName, bIsUE4Game, GameBinariesFolder, GameIntermediatesFolder, OutTargetPath);
			GeneratePackageResources(InTargetName, bIsUE4Game, GameBinariesFolder, GameIntermediatesFolder, OutTargetPath);
			CopyDefaultImageFiles(OutTargetPath);
		}


		public override void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetRules.TargetType TargetType, DirectoryReference RootDirectory, FileReference TargetFilePath)
		{
			// @todo UWP: This used to be "WINUAP=1".  Need to verify that 'UWP' is the correct define that we want here.
			VCProjectFileContent.Append("		<NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);PLATFORM_UWP=1;UWP=1;</NMakePreprocessorDefinitions>" + ProjectFileGenerator.NewLine);
		}

	}
}
