// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool.IOS
{
	class UEDeployIOS : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			// TODO: print debug info and handle any cases that would keep this from registering
			UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.IOS, this);
		}

		private static void CopyFileWithReplacements(string SourceFilename, string DestFilename, Dictionary<string, string> Replacements, List<string> AdditionalLines)
		{
			if (!File.Exists(SourceFilename))
			{
				return;
			}

			// make the dst filename with the same structure as it was in SourceDir
			if (File.Exists(DestFilename))
			{
				File.Delete(DestFilename);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(DestFilename);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// some files are handled specially
			string Ext = Path.GetExtension(SourceFilename);
			if (Ext == ".plist")
			{
				string[] Contents = File.ReadAllLines(SourceFilename);
				StringBuilder NewContents = new StringBuilder();

				int LastDictLine = 0;
				for (int LineIndex = Contents.Length - 1; LineIndex >= 0; LineIndex--)
				{
					if (Contents[LineIndex].Trim() == "</dict>")
					{
						LastDictLine = LineIndex;
						break;
					}
				}

				// replace some varaibles
				for (int LineIndex = 0; LineIndex < Contents.Length; LineIndex++)
				{
					string Line = Contents[LineIndex];

					// inject before the last line
					if (LineIndex == LastDictLine)
					{
						foreach (string ExtraLine in AdditionalLines)
						{
							string FixedLine = ExtraLine;
							foreach (var Pair in Replacements)
							{
								FixedLine = FixedLine.Replace(Pair.Key, Pair.Value);
							}
							NewContents.Append(FixedLine + Environment.NewLine);
						}
					}

					foreach (var Pair in Replacements)
					{
						Line = Line.Replace(Pair.Key, Pair.Value);
					}

					NewContents.Append(Line + Environment.NewLine);

				}

				File.WriteAllText(DestFilename, NewContents.ToString());
			}
			else
			{
				File.Copy(SourceFilename, DestFilename);

				// remove any read only flags
				FileInfo DestFileInfo = new FileInfo(DestFilename);
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
			}
		}


		public override bool PrepForUATPackageOrDeploy(string InProjectName, string InProjectDirectory, string InExecutablePath, string InEngineDir, bool bForDistribution, string CookFlavor)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				throw new BuildException("UEDeployIOS.PrepForUATPackageOrDeploy only supports running on the Mac");
			}

			bool bIsUE4Game = InExecutablePath.Contains ("UE4Game");
			string BinaryPath = Path.GetDirectoryName (InExecutablePath);
			string GameExeName = Path.GetFileName (InExecutablePath);
			string GameName = bIsUE4Game ? "UE4Game" : InProjectName;
			string PayloadDirectory =  BinaryPath + "/Payload";
			string AppDirectory = PayloadDirectory + "/" + GameName + ".app";
			string CookedContentDirectory = AppDirectory + "/cookeddata";
			string BuildDirectory = InProjectDirectory + "/Build/IOS";
			string IntermediateDirectory = (bIsUE4Game ? InEngineDir : InProjectDirectory) + "/Intermediate/IOS";

			// don't delete the payload directory, just update the data in it
			//if (Directory.Exists(PayloadDirectory))
			//{
			//	Directory.Delete(PayloadDirectory, true);
			//}

			Directory.CreateDirectory(BinaryPath);
			Directory.CreateDirectory(PayloadDirectory);
			Directory.CreateDirectory(AppDirectory);
			Directory.CreateDirectory(BuildDirectory);
			Directory.CreateDirectory(CookedContentDirectory);

			// create the entitlements file
			WriteEntitlementsFile(Path.Combine(IntermediateDirectory, GameName + ".entitlements"));

			// delete some old files if they exist
			if (Directory.Exists(AppDirectory + "/_CodeSignature"))
			{
				Directory.Delete(AppDirectory + "/_CodeSignature", true);
			}
			if (File.Exists(AppDirectory + "/CustomResourceRules.plist"))
			{
				File.Delete(AppDirectory + "/CustomResourceRules.plist");
			}
			if (File.Exists(AppDirectory + "/embedded.mobileprovision"))
			{
				File.Delete(AppDirectory + "/embedded.mobileprovision");
			}
			if (File.Exists(AppDirectory + "/PkgInfo"))
			{
				File.Delete(AppDirectory + "/PkgInfo");
			}

			// install the provision
			FileInfo DestFileInfo;
			string ProvisionWithPrefix = InEngineDir + "/Build/IOS/UE4Game.mobileprovision";
			if (File.Exists(BuildDirectory + "/" + InProjectName + ".mobileprovision"))
			{
				ProvisionWithPrefix = BuildDirectory + "/" + InProjectName + ".mobileprovision";
			}
			else
			{
				if (File.Exists(BuildDirectory + "/NotForLicensees/" + InProjectName + ".mobileprovision"))
				{
					ProvisionWithPrefix = BuildDirectory + "/NotForLicensees/" + InProjectName + ".mobileprovision";
				}
				else if (!File.Exists(ProvisionWithPrefix))
				{
					ProvisionWithPrefix = InEngineDir + "/Build/IOS/NotForLicensees/UE4Game.mobileprovision";
				}
			}
			if (File.Exists (ProvisionWithPrefix))
			{
				Directory.CreateDirectory (Environment.GetEnvironmentVariable ("HOME") + "/Library/MobileDevice/Provisioning Profiles/");
				if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + ".mobileprovision"))
				{
					DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + ".mobileprovision");
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
				File.Copy (ProvisionWithPrefix, Environment.GetEnvironmentVariable ("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + ".mobileprovision", true);
				DestFileInfo = new FileInfo (Environment.GetEnvironmentVariable ("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + ".mobileprovision");
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
			}
			else
			{
				// copy all provisions from the game directory, the engine directory, and the notforlicensees directory
				// copy all of the provisions from the game directory to the library
				{
					if (Directory.Exists(BuildDirectory))
					{
						foreach (string Provision in Directory.EnumerateFiles(BuildDirectory, "*.mobileprovision", SearchOption.AllDirectories))
						{
							if (!File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)) || File.GetLastWriteTime(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)) < File.GetLastWriteTime(Provision))
							{
								if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)))
								{
									DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision));
									DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
								}
								File.Copy(Provision, Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision), true);
								DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision));
								DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
							}
						}
					}
				}

				// copy all of the provisions from the engine directory to the library
				{
					if (Directory.Exists(InEngineDir + "/Build/IOS"))
					{
						foreach (string Provision in Directory.EnumerateFiles(InEngineDir + "/Build/IOS", "*.mobileprovision", SearchOption.AllDirectories))
						{
							if (!File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)) || File.GetLastWriteTime(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)) < File.GetLastWriteTime(Provision))
							{
								if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision)))
								{
									DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision));
									DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
								}
								File.Copy(Provision, Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision), true);
								DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Path.GetFileName(Provision));
								DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
							}
						}
					}
				}
			}

			// install the distribution provision
			ProvisionWithPrefix = InEngineDir + "/Build/IOS/UE4Game_Distro.mobileprovision";
			if (File.Exists(BuildDirectory + "/" + InProjectName + "_Distro.mobileprovision"))
			{
				ProvisionWithPrefix = BuildDirectory + "/" + InProjectName + "_Distro.mobileprovision";
			}
			else
			{
				if (File.Exists(BuildDirectory + "/NotForLicensees/" + InProjectName + "_Distro.mobileprovision"))
				{
					ProvisionWithPrefix = BuildDirectory + "/NotForLicensees/" + InProjectName + "_Distro.mobileprovision";
				}
				else if (!File.Exists(ProvisionWithPrefix))
				{
					ProvisionWithPrefix = InEngineDir + "/Build/IOS/NotForLicensees/UE4Game_Distro.mobileprovision";
				}
			}
			if (File.Exists(ProvisionWithPrefix))
			{
				if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + "_Distro.mobileprovision"))
				{
					DestFileInfo = new FileInfo(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + "_Distro.mobileprovision");
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
				File.Copy(ProvisionWithPrefix, Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + "_Distro.mobileprovision", true);
				DestFileInfo = new FileInfo (Environment.GetEnvironmentVariable ("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + InProjectName + "_Distro.mobileprovision");
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
			}

			// compile the launch .xib
			string LaunchXib = InEngineDir + "/Build/IOS/Resources/Interface/LaunchScreen.xib";
			if (File.Exists(BuildDirectory + "/Resources/Interface/LaunchScreen.xib"))
			{
				LaunchXib = BuildDirectory + "/Resources/Interface/LaunchScreen.xib";
			}

			List<string> PListAdditionalLines = new List<string>();
			Dictionary<string, string> Replacements = new Dictionary<string, string>();

			bool bSkipDefaultPNGs = false;
			if (File.Exists(LaunchXib))
			{
				string CommandLine = string.Format("--target-device iphone --target-device ipad --errors --warnings --notices --module {0} --minimum-deployment-target 8.0 --auto-activate-custom-fonts --output-format human-readable-text --compile {1}/LaunchScreen.nib {2}", InProjectName, Path.GetFullPath(AppDirectory), Path.GetFileName(LaunchXib));

				// now we need to zipalign the apk to the final destination (to 4 bytes, must be 4)
				ProcessStartInfo IBToolStartInfo = new ProcessStartInfo();
				IBToolStartInfo.WorkingDirectory = Path.GetDirectoryName(LaunchXib);
				IBToolStartInfo.FileName = "/Applications/Xcode.app/Contents/Developer/usr/bin/ibtool";
				IBToolStartInfo.Arguments = CommandLine;
				IBToolStartInfo.UseShellExecute = false;
				Process CallIBTool = new Process();
				CallIBTool.StartInfo = IBToolStartInfo;
				CallIBTool.Start();
				CallIBTool.WaitForExit();

				PListAdditionalLines.Add("\t<key>UILaunchStoryboardName</key>");
				PListAdditionalLines.Add("\t<string>LaunchScreen</string>");

				bSkipDefaultPNGs = true;
			}
			else
			{
				// this is a temp way to inject the iphone 6 images without needing to upgrade everyone's plist
				// eventually we want to generate this based on what the user has set in the project settings
				string[] IPhoneConfigs =  
				{ 
					"Default-IPhone6", "Landscape", "{375, 667}", 
					"Default-IPhone6", "Portrait", "{375, 667}", 
					"Default-IPhone6Plus-Landscape", "Landscape", "{414, 736}", 
					"Default-IPhone6Plus-Portrait", "Portrait", "{414, 736}", 
					"Default", "Landscape", "{320, 480}",
					"Default", "Portrait", "{320, 480}",
					"Default-568h", "Landscape", "{320, 568}",
					"Default-568h", "Portrait", "{320, 568}",
				};

				StringBuilder NewLaunchImagesString = new StringBuilder("<key>UILaunchImages~iphone</key>\n\t\t<array>\n");
				for (int ConfigIndex = 0; ConfigIndex < IPhoneConfigs.Length; ConfigIndex += 3)
				{
					NewLaunchImagesString.Append("\t\t\t<dict>\n");
					NewLaunchImagesString.Append("\t\t\t\t<key>UILaunchImageMinimumOSVersion</key>\n");
					NewLaunchImagesString.Append("\t\t\t\t<string>8.0</string>\n");
					NewLaunchImagesString.Append("\t\t\t\t<key>UILaunchImageName</key>\n");
					NewLaunchImagesString.AppendFormat("\t\t\t\t<string>{0}</string>\n", IPhoneConfigs[ConfigIndex + 0]);
					NewLaunchImagesString.Append("\t\t\t\t<key>UILaunchImageOrientation</key>\n");
					NewLaunchImagesString.AppendFormat("\t\t\t\t<string>{0}</string>\n", IPhoneConfigs[ConfigIndex + 1]);
					NewLaunchImagesString.Append("\t\t\t\t<key>UILaunchImageSize</key>\n");
					NewLaunchImagesString.AppendFormat("\t\t\t\t<string>{0}</string>\n", IPhoneConfigs[ConfigIndex + 2]);
					NewLaunchImagesString.Append("\t\t\t</dict>\n");
				}

				// close it out
				NewLaunchImagesString.Append("\t\t\t</array>\n\t\t<key>UILaunchImages~ipad</key>");
				Replacements.Add("<key>UILaunchImages~ipad</key>", NewLaunchImagesString.ToString());
			}

			// copy plist file
			string PListFile = InEngineDir + "/Build/IOS/UE4Game-Info.plist";
			if (File.Exists(BuildDirectory + "/Info.plist"))
			{
				PListFile = BuildDirectory + "/Info.plist";
			}
			else if (File.Exists(BuildDirectory + "/" + InProjectName + "-Info.plist"))
			{
				PListFile = BuildDirectory + "/" + InProjectName + "-Info.plist";
			}

			// plist replacements
			Replacements.Add("${EXECUTABLE_NAME}", GameName);
			Replacements.Add("${BUNDLE_IDENTIFIER}", InProjectName.Replace("_", ""));
			CopyFileWithReplacements(PListFile, AppDirectory + "/Info.plist", Replacements, PListAdditionalLines);
			CopyFileWithReplacements(PListFile, IntermediateDirectory + "/" + GameName + "-Info.plist", Replacements, PListAdditionalLines);

			// ensure the destination is writable
			if (File.Exists(AppDirectory + "/" + GameName))
			{
				FileInfo GameFileInfo = new FileInfo(AppDirectory + "/" + GameName);
				GameFileInfo.Attributes = GameFileInfo.Attributes & ~FileAttributes.ReadOnly;
			}

			// copy the GameName binary
			File.Copy(BinaryPath + "/" + GameExeName, AppDirectory + "/" + GameName, true);

			// copy engine assets in
			if (bSkipDefaultPNGs)
			{
				// we still want default icons
				CopyFiles(InEngineDir + "/Build/IOS/Resources/Graphics", AppDirectory, "Icon*.png", true);
			}
			else
			{
				CopyFiles(InEngineDir + "/Build/IOS/Resources/Graphics", AppDirectory, "*.png", true);
			}
			// merge game assets on top
			if (Directory.Exists(BuildDirectory + "/Resources/Graphics"))
			{
				CopyFiles(BuildDirectory + "/Resources/Graphics", AppDirectory, "*.png", true);
			}
			
			// copy additional engine framework assets in
			string FrameworkAssetsPath = InEngineDir + "/Intermediate/IOS/FrameworkAssets";

			// Let project override assets if they exist
			if ( Directory.Exists( InProjectDirectory + "/Intermediate/IOS/FrameworkAssets" ) )
			{
				FrameworkAssetsPath = InProjectDirectory + "/Intermediate/IOS/FrameworkAssets";
			}

			if ( Directory.Exists( FrameworkAssetsPath ) )
			{
				CopyFolder( FrameworkAssetsPath, AppDirectory, true );
			}

			//CopyFiles(BuildDirectory, PayloadDirectory, null, "iTunesArtwork", null);
			CopyFolder(InEngineDir + "/Content/Stats", AppDirectory + "/cookeddata/engine/content/stats", true);

			return true;
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			string GameName = InTarget.AppName;
			string BuildPath = (GameName == "UE4Game" ? "../../Engine" : InTarget.ProjectDirectory) + "/Binaries/IOS";
			string ProjectDirectory = InTarget.ProjectDirectory;

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac && Environment.GetEnvironmentVariable("UBT_NO_POST_DEPLOY") != "true")
			{
				string DecoratedGameName;
				if (InTarget.Configuration == UnrealTargetConfiguration.Development)
				{
					DecoratedGameName = GameName;
				}
				else
				{
					DecoratedGameName = String.Format("{0}-{1}-{2}", GameName, InTarget.Platform.ToString(), InTarget.Configuration.ToString());
				}

				return PrepForUATPackageOrDeploy(GameName, ProjectDirectory, BuildPath + "/" + DecoratedGameName, "../../Engine", false, "");
			}
			else
			{
				// If it is requested, send the app bundle back to the platform executing these commands.
				if (BuildConfiguration.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					IOSToolChain Toolchain = UEToolChain.GetPlatformToolChain(CPPTargetPlatform.IOS) as IOSToolChain;

					try
					{
						string BinaryDir = Path.GetDirectoryName(InTarget.OutputPath) + "\\";
						if (BinaryDir.EndsWith(InTarget.AppName + "\\Binaries\\IOS\\") && InTarget.TargetType != TargetRules.TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(InTarget.TargetType.ToString(), "Game");
						}

						// Get the app bundle's name
						string AppFullName = InTarget.AppName;
						if (InTarget.Configuration != UnrealTargetConfiguration.Development)
						{
							AppFullName += "-" + InTarget.Platform.ToString();
							AppFullName += "-" + InTarget.Configuration.ToString();
						}

						foreach (string BinaryPath in Toolchain.BuiltBinaries)
						{
							if (!BinaryPath.Contains("Dummy"))
							{
								RPCUtilHelper.CopyFile(Toolchain.ConvertPath(BinaryPath), BinaryPath, false);
							}
						}
						Log.TraceInformation("Copied binaries successfully.");
					}
					catch (Exception)
					{
						Log.TraceInformation("Copying binaries back to this device failed.");
					}
				}
			}
			return true;
		}

		private void WriteEntitlementsFile(string OutputFilename)
		{
			Directory.CreateDirectory(Path.GetDirectoryName(OutputFilename));
			// we need to have something so Xcode will compile, so we just set the get-task-allow, since we know the value, 
			// which is based on distribution or not (true means debuggable)
			File.WriteAllText(OutputFilename, string.Format("<plist><dict><key>get-task-allow</key><{0}/></dict></plist>",
				/*Config.bForDistribution ? "false" : */"true"));
		}

		static void SafeFileCopy(FileInfo SourceFile, string DestinationPath, bool bOverwrite)
		{
			FileInfo DI = new FileInfo(DestinationPath);
			if (DI.Exists && bOverwrite)
			{
				DI.IsReadOnly = false;
				DI.Delete();
			}

			SourceFile.CopyTo(DestinationPath, bOverwrite);

			FileInfo DI2 = new FileInfo(DestinationPath);
			if (DI2.Exists)
			{
				DI2.IsReadOnly = false;
			}
		}

		private void CopyFiles(string SourceDirectory, string DestinationDirectory, string TargetFiles, bool bOverwrite = false)
		{
			DirectoryInfo SourceFolderInfo = new DirectoryInfo(SourceDirectory);
			FileInfo[] SourceFiles = SourceFolderInfo.GetFiles(TargetFiles);
			foreach (FileInfo SourceFile in SourceFiles)
			{
				string DestinationPath = Path.Combine(DestinationDirectory, SourceFile.Name);
				SafeFileCopy(SourceFile, DestinationPath, bOverwrite);
			}
		}

		private void CopyFolder(string SourceDirectory, string DestinationDirectory, bool bOverwrite = false)
		{
			Directory.CreateDirectory(DestinationDirectory);
			RecursiveFolderCopy(new DirectoryInfo(SourceDirectory), new DirectoryInfo(DestinationDirectory), bOverwrite);
		}

		static private void RecursiveFolderCopy(DirectoryInfo SourceFolderInfo, DirectoryInfo DestFolderInfo, bool bOverwrite = false)
		{
			foreach (FileInfo SourceFileInfo in SourceFolderInfo.GetFiles())
			{
				string DestinationPath = Path.Combine(DestFolderInfo.FullName, SourceFileInfo.Name);
				SafeFileCopy(SourceFileInfo, DestinationPath, bOverwrite);
			}

			foreach (DirectoryInfo SourceSubFolderInfo in SourceFolderInfo.GetDirectories())
			{
				string DestFolderName = Path.Combine(DestFolderInfo.FullName, SourceSubFolderInfo.Name);
				Directory.CreateDirectory(DestFolderName);
				RecursiveFolderCopy(SourceSubFolderInfo, new DirectoryInfo(DestFolderName), bOverwrite);
			}
		}
	}
}
