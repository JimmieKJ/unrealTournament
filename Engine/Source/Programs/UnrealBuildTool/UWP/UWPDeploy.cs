// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;

// @todo UWP: this file is a work in progress and is not yet complete for the F5 scenario for UWP

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	class UWPDeploy : UEBuildDeploy
	{
		/// <summary>
		/// Utility function to delete a file
		/// </summary>
		void DeployHelper_DeleteFile(string InFileToDelete)
		{
			Log.TraceInformation("UWPDeploy.DeployHelper_DeleteFile({0})", InFileToDelete);
			if (File.Exists(InFileToDelete) == true)
			{
				FileAttributes attributes = File.GetAttributes(InFileToDelete);
				if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
				{
					attributes &= ~FileAttributes.ReadOnly;
					File.SetAttributes(InFileToDelete, attributes);
				}
				File.Delete(InFileToDelete);
			}
		}

		/// <summary>
		/// Copy the contents of the given source directory to the given destination directory
		/// </summary>
		bool CopySourceToDestDir(string InSourceDirectory, string InDestinationDirectory, string InWildCard,
			bool bInIncludeSubDirectories, bool bInRemoveDestinationOrphans)
		{
			Log.TraceInformation("UWPDeploy.CopySourceToDestDir({0}, {1}, {2},...)", InSourceDirectory, InDestinationDirectory, InWildCard);
			if (Directory.Exists(InSourceDirectory) == false)
			{
				Log.TraceInformation("Warning: CopySourceToDestDir - SourceDirectory does not exist: {0}", InSourceDirectory);
				return false;
			}

			// Make sure the destination directory exists!
			Directory.CreateDirectory(InDestinationDirectory);

			SearchOption OptionToSearch = bInIncludeSubDirectories ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;

			var SourceDirs = new List<string>(Directory.GetDirectories(InSourceDirectory, "*.*", OptionToSearch));
			foreach (string SourceDir in SourceDirs)
			{
				string SubDir = SourceDir.Replace(InSourceDirectory, "");
				string DestDir = InDestinationDirectory + SubDir;
				Directory.CreateDirectory(DestDir);
			}

			var SourceFiles = new List<string>(Directory.GetFiles(InSourceDirectory, InWildCard, OptionToSearch));
			var DestFiles = new List<string>(Directory.GetFiles(InDestinationDirectory, InWildCard, OptionToSearch));

			// Keep a list of the files in the source directory... without the source path
			List<string> FilesInSource = new List<string>();

			// Copy all the source files that are newer...
			foreach (string SourceFile in SourceFiles)
			{
				string Filename = SourceFile.Replace(InSourceDirectory, "");
				FilesInSource.Add(Filename.ToUpperInvariant());
				string DestFile = InDestinationDirectory + Filename;

				System.DateTime SourceTime = File.GetLastWriteTime(SourceFile);
				System.DateTime DestTime = File.GetLastWriteTime(DestFile);

				if (SourceTime > DestTime)
				{
					try
					{
						DeployHelper_DeleteFile(DestFile);
						File.Copy(SourceFile, DestFile, true);
					}
					catch (Exception exceptionMessage)
					{
						Log.TraceInformation("Failed to copy {0} to deployment: {1}", SourceFile, exceptionMessage);
					}
				}
			}

			if (bInRemoveDestinationOrphans == true)
			{
				// If requested, delete any destination files that do not have a corresponding
				// file in the source directory
				foreach (string DestFile in DestFiles)
				{
					string DestFilename = DestFile.Replace(InDestinationDirectory, "");
					if (FilesInSource.Contains(DestFilename.ToUpperInvariant()) == false)
					{
						Log.TraceInformation("Destination file does not exist in Source - DELETING: {0}", DestFile);
						FileAttributes attributes = File.GetAttributes(DestFile);
						try
						{
							DeployHelper_DeleteFile(DestFile);
						}
						catch (Exception exceptionMessage)
						{
							Log.TraceInformation("Failed to delete {0} from deployment: {1}", DestFile, exceptionMessage);
						}
					}
				}
			}

			return true;
		}


		/// <summary>
		/// Helper function for copying files
		/// </summary>
		void CopyFile(string InSource, string InDest, bool bForce)
		{
			Log.TraceInformation("UWPDeploy.CopyFile({0}, {1}, {2},...)", InSource, InDest, bForce);
			if (File.Exists(InSource) == true)
			{
				if (bForce == true)
				{
					if (File.Exists(InDest) == true)
					{
						DeployHelper_DeleteFile(InDest);
					}
				}
				File.Copy(InSource, InDest, true);
			}
			else
			{
				Log.TraceInformation("UWPDeploy: File didn't exist - {0}", InSource);
			}
		}

		/// <summary>
		/// Helper function for copying a tree files
		/// </summary>
		//void CopyDirectory(string InSource, string InDest, bool bForce, bool bRecurse)
		//{
		//    if (Directory.Exists(InSource))
		//    {
		//        if (!Directory.Exists(InDest))
		//        {
		//            Directory.CreateDirectory(InDest);
		//        }

		//        // Copy all files
		//        string[] FilesInDir = Directory.GetFiles(InSource);
		//        foreach (string FileSourcePath in FilesInDir)
		//        {
		//            string FileDestPath = Path.Combine(InDest, Path.GetFileName(FileSourcePath));
		//            if (bForce == true)
		//            {
		//                if (File.Exists(FileDestPath) == true)
		//                {
		//                    DeployHelper_DeleteFile(FileDestPath);
		//                }
		//            }
		//            File.Copy(FileSourcePath, FileDestPath, true);
		//        }

		//        // Recurse sub directories
		//        string[] DirsInDir = Directory.GetDirectories(InSource);
		//        foreach (string DirSourcePath in DirsInDir)
		//        {
		//            string DirName = Path.GetFileName(DirSourcePath);
		//            string DirDestPath = Path.Combine(InDest, DirName);
		//            CopyDirectory(DirSourcePath, DirDestPath, bForce, bRecurse);
		//        }
		//    }
		//}


		public bool PrepForUATPackageOrDeploy(string InProjectName, string InProjectDirectory, string InExecutablePath, string InEngineDir, bool bForDistribution, string CookFlavor)
		{
			// Much of the UWPProjectGenerator code assumes that the path will be set to Engine/Source
			string PreviousDirectory = Directory.GetCurrentDirectory();
			string PreviousRelativeEnginePath = BuildConfiguration.RelativeEnginePath;

			Directory.SetCurrentDirectory(Path.Combine(InEngineDir, "Binaries"));
			BuildConfiguration.RelativeEnginePath = "../../Engine";

			string ProjectBinaryFolder = new FileInfo(InExecutablePath).DirectoryName;
			string EngineSourceRelativeBinaryPath = Utils.MakePathRelativeTo(ProjectBinaryFolder, Path.Combine(InEngineDir, "Source"));
			bool bIsUE4Game = new FileInfo(InExecutablePath).Name.StartsWith("UE4Game", StringComparison.InvariantCultureIgnoreCase);

			UWPProjectGenerator ProjectGen = new UWPProjectGenerator();
			ProjectGen.GenerateDeployFiles(InProjectName, ProjectBinaryFolder, bIsUE4Game, EngineSourceRelativeBinaryPath);

			// back to root for UAT
			Directory.SetCurrentDirectory(PreviousDirectory);
			BuildConfiguration.RelativeEnginePath = PreviousRelativeEnginePath;

			return true;
		}



		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			string InAppName = InTarget.AppName;
			Log.TraceInformation("Prepping {0} for deployment to {1}", InAppName, InTarget.Platform.ToString());
			System.DateTime PrepDeployStartTime = DateTime.UtcNow;

			string TargetFilename = InTarget.RulesAssembly.GetTargetFileName(InAppName).FullName;
			string ProjectSourceFolder = new FileInfo(TargetFilename).DirectoryName + "/";

			string RelativeTargetDirectory;
			string EngineSourceRelativeBinaryPath;
			UWPProjectGenerator.GetTargetUWPPaths(InTarget.RulesAssembly, InAppName, InTarget.Rules, out EngineSourceRelativeBinaryPath, out RelativeTargetDirectory);

			PrepForUATPackageOrDeploy(InAppName, InTarget.ProjectDirectory.FullName, InTarget.OutputPath.FullName, BuildConfiguration.RelativeEnginePath, false, "");

			// TODO - richiem - restore this if we find that it's needed.
			//Log.TraceInformation("...copying the CELL dll...");
			//string CELLPath = "../../../Engine/Source/" + UEBuildConfiguration.UEThirdPartySourceDirectory + "CELL/lib/win64/";
			//string CELLPathRelease = CELLPath + "Release/";
			//string CELLDllRelease = "CommonEventLoggingLibrary.dll";
			//string CELLPathDebug = CELLPath + "Debug/";
			//string CELLDllDebug = "CommonEventLoggingLibraryd.dll";

			//CopyFile(EngineSourceRelativeBinaryPath + CELLPathRelease + CELLDllRelease, EngineSourceRelativeBinaryPath + CELLDllRelease, true);
			//CopyFile(EngineSourceRelativeBinaryPath + CELLPathDebug + CELLDllDebug, EngineSourceRelativeBinaryPath + CELLDllDebug, true);

			//string XSAPIPath            = EngineSourceRelativeBinaryPath + "../../../Engine/Source/ThirdParty/XSAPI/lib/";
			//string XboxServicesConfig   = "xboxservices.config";
			//string DesktopLogin         = "DesktopLogin.exe";

			//Log.TraceInformation("...copying xboxservices.config");
			//CopyFile(XSAPIPath + XboxServicesConfig, EngineSourceRelativeBinaryPath + XboxServicesConfig, true);
			//Log.TraceInformation("...copying DesktopLogin.exe...");
			//CopyFile(XSAPIPath + DesktopLogin, EngineSourceRelativeBinaryPath + DesktopLogin, true);

			// TODO - richiem - restore this if we find that it's needed.
			//if (InTarget.Configuration == UnrealTargetConfiguration.Development)
			//{
			//    Log.TraceInformation("...copying AutoLogin...");
			//    string AutoLoginPath    = EngineSourceRelativeBinaryPath + "../../../Engine/Source/Tools/AutoLogin/";
			//    string AutoLoginDLL     = "AutoLogin.dll";
			//    string AutoLoginBat     = "AutoLogin.bat";
			//    CopyFile(AutoLoginPath + "bin/Debug/" + AutoLoginDLL, EngineSourceRelativeBinaryPath + AutoLoginDLL, true);
			//    CopyFile(AutoLoginPath + AutoLoginBat, EngineSourceRelativeBinaryPath + AutoLoginBat, true);
			//}



			// Log out the time taken to deploy...
			double PrepDeployDuration = (DateTime.UtcNow - PrepDeployStartTime).TotalSeconds;
			Log.TraceInformation("UWP deployment preparation took {0:0.00} seconds", PrepDeployDuration);

			return true;
		}
	}
}
