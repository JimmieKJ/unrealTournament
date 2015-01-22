// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	class WinRTDeploy : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			// Register this deployment handle for WinRT
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.WinRT.ToString());
			UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.WinRT, this);
		}

		/**
		 *  Utility function to delete a file
		 */
		void DeployHelper_DeleteFile(string InFileToDelete)
		{
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

		/**
		 *	Copy the contents of the given source directory to the given destination directory
		 *	
		 */
		bool CopySourceToDestDir(string InSourceDirectory, string InDestinationDirectory, string InWildCard,
			bool bInIncludeSubDirectories, bool bInRemoveDestinationOrphans)
		{
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

		/**
		 *	Copy the game contents via reading in the <GAME>\Saved\DeployList\ContentList.txt file.
		 *	If the file doesn't exist, simply mirror ALL content to the deployment directory.
		 *	
		 *	@param	InAppName			The name of the target app - could be the GameName
		 *	@param	InPlatform			UnrealTargetPlatform of the target
		 *	@param	InConfiguration		UnrealTargetConfiguration of the target
		 *	
		 *	@return	bool				true if successful
		 */
		bool CopyGameContent(string InAppName, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			string DeployDirectory = "../../" + InAppName + "/Binaries/WinRT/Image/Loose/";
			string ContentListName = "../../" + InAppName + "/Saved/DeployList/ContentList.txt";

			bool bProcessedContentList = false;
			if (File.Exists(ContentListName))
			{
				//string FileContents = File.ReadAllText(ContentListName);
				foreach (string Line in File.ReadLines(ContentListName))
				{
					if (
							(Line.StartsWith("/Script", StringComparison.InvariantCultureIgnoreCase) == true)
						|| (Line.StartsWith("/Engine", StringComparison.InvariantCultureIgnoreCase) == true)
						)
					{
						// Skip these.
						// Engine content will have already been copied.
						// Script content doesn't exist.
					}
					else if (Line.StartsWith("/") == false)
					{
						// Skip these as well.
					}
					else if (Line.StartsWith("/Game", StringComparison.InvariantCultureIgnoreCase) == true)
					{
						// These are the ones we want.
						string SourceFilename = Line;
						SourceFilename = SourceFilename.Replace("/Game/", InAppName + "/");
						string SourceGameContent = "../../" + SourceFilename;
						string DestGameContent = DeployDirectory + SourceFilename;

						System.DateTime SourceTime = File.GetLastWriteTime(SourceGameContent);
						System.DateTime DestTime = File.GetLastWriteTime(DestGameContent);

						if (SourceTime > DestTime)
						{
							try
							{
Log.TraceInformation("GameDeployment: Copying {0} to {1}", SourceGameContent, DestGameContent);
								// Need to ensure that the dest directory exists...
								string DirectoryPath = Path.GetDirectoryName(DestGameContent);
								if (!Directory.Exists(DirectoryPath))
								{
									Directory.CreateDirectory(DirectoryPath);
								}
								DeployHelper_DeleteFile(DestGameContent);
								File.Copy(SourceGameContent, DestGameContent, true);
							}
							catch (Exception exceptionMessage)
							{
								Log.TraceInformation("Failed to copy {0} to deployment: {1}", SourceGameContent, exceptionMessage);
							}
						}
					}
					else
					{
						Log.TraceInformation("Unknown entry in the ContentList: {0}", Line);
					}
				}

				bProcessedContentList = true;
			}

			if (bProcessedContentList == false)
			{
				// Copy the Game/Content files
				string SourceDir = "../../" + InAppName + "/Content";
				string DestDir = DeployDirectory + InAppName + "/Content";
				CopySourceToDestDir(SourceDir, DestDir, "*.*", true, true);
			}

			return true;
		}

		/**
		 *	Helper function for copying files
		 */
		void CopyFile(string InSource, string InDest, bool bForce)
		{
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
				Log.TraceInformation("WinRTDeploy: File didn't exist - {0}", InSource);
			}
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			Log.TraceInformation("Prepping {0} for deployment to {1}", InTarget.AppName, InTarget.Platform.ToString());
			//@todo.WinRT: All this would really be done in a cooker/packager step...
			// The only thing that would really be done here would be to copy the built 
			// executable and desired manifest to the proper deployment folder.
			// For now, we simply make sure all required content is present in the 
			// deployment directory prior to launching the debugger.

			System.DateTime PrepDeployStartTime = DateTime.UtcNow;

			// Make sure the deployment directory exists...
			string DeployDirectory = "../../" + InTarget.AppName + "/Binaries/WinRT/AppX/";
			Directory.CreateDirectory(DeployDirectory);

			// The binaries directory
			string BinariesDirectory = "../../" + InTarget.AppName + "/Binaries/WinRT/";
			Directory.CreateDirectory(BinariesDirectory + "AppX");
			Directory.CreateDirectory(BinariesDirectory + "AppX/Assets");
			// The WinRT build directory
			string WinRTBuildDirectory = "../../" + InTarget.AppName + "/Build/WinRT/";
			// The WinRT source directory
			string WinRTSourceDirectory = "../../" + InTarget.AppName + "/Source/WinRT/";

			// Copy the executable, renaming it <Game>WinRTRUN.exe
			Log.TraceInformation("...copying the executable...");
			string FileNameWithoutExt = InTarget.AppName;
			if (InTarget.Configuration != UnrealTargetConfiguration.Development)
			{
				FileNameWithoutExt += "-" + InTarget.Platform.ToString();
				FileNameWithoutExt += "-" + InTarget.Configuration.ToString();
			}
			string ExeFileName = FileNameWithoutExt + ".exe";
			string WinmdFileName = FileNameWithoutExt + ".winmd";
			string BuildRecipeFileName = InTarget.AppName + "WinRTRUN.build.appxrecipe";
			string DestExeName = InTarget.AppName + "WinRTRUN.exe";
			CopyFile(BinariesDirectory + ExeFileName, BinariesDirectory + DestExeName, true);

			// Copy the winmd file
			Log.TraceInformation("...copying the winmd file...");
			string DestWinmdName = InTarget.AppName + "WinRTRUN.winmd";
			CopyFile(BinariesDirectory + WinmdFileName, BinariesDirectory + DestWinmdName, true);

			// Copy the recipe file
			Log.TraceInformation("...copying the build recipe file...");
			CopyFile(WinRTBuildDirectory + BuildRecipeFileName, BinariesDirectory + BuildRecipeFileName, true);

			// Copy the WinRTCmdLine.txt file
 			Log.TraceInformation("...copying the commandline text file...");
 			string CmdLineFile = "WinRTCmdLine.txt";
 			CopyFile(WinRTBuildDirectory + CmdLineFile, BinariesDirectory + CmdLineFile, true);
 			CopyFile(WinRTBuildDirectory + CmdLineFile, DeployDirectory + CmdLineFile, true);

			// Copy the assets folder
			Log.TraceInformation("...copying the asset files...");
			string AssetFile = "Assets/Logo.png";
			CopyFile(WinRTSourceDirectory + AssetFile, DeployDirectory + AssetFile, true);
			AssetFile = "Assets/SmallLogo.png";
			CopyFile(WinRTSourceDirectory + AssetFile, DeployDirectory + AssetFile, true);
			AssetFile = "Assets/SplashScreen.png";
			CopyFile(WinRTSourceDirectory + AssetFile, DeployDirectory + AssetFile, true);
			AssetFile = "Assets/StoreLogo.png";
			CopyFile(WinRTSourceDirectory + AssetFile, DeployDirectory + AssetFile, true);

			// Log out the time taken to deploy...
			double PrepDeployDuration = (DateTime.UtcNow - PrepDeployStartTime).TotalSeconds;
			Log.TraceInformation("WinRT deployment preparation took {0:0.00} seconds", PrepDeployDuration);

			return true;
		}
	}
}
