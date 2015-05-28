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
	public class UEDeployIOS : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			// TODO: print debug info and handle any cases that would keep this from registering
			UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.IOS, this);
		}

		class VersionUtilities
		{
			public static string BuildDirectory
			{
				get;
				set;
			}
			public static string GameName
			{
				get;
				set;
			}

			static string RunningVersionFilename
			{
				get { return Path.Combine(BuildDirectory, GameName + ".PackageVersionCounter"); }
			}

			/// <summary>
			/// Reads the GameName.PackageVersionCounter from disk and bumps the minor version number in it
			/// </summary>
			/// <returns></returns>
			public static string ReadRunningVersion()
			{
				string CurrentVersion = "0.0";
				if (File.Exists(RunningVersionFilename))
				{
					CurrentVersion = File.ReadAllText(RunningVersionFilename);
				}

				return CurrentVersion;
			}

			/// <summary>
			/// Pulls apart a version string of one of the two following formats:
			///	  "7301.15 11-01 10:28"   (Major.Minor Date Time)
			///	  "7486.0"  (Major.Minor)
			/// </summary>
			/// <param name="CFBundleVersion"></param>
			/// <param name="VersionMajor"></param>
			/// <param name="VersionMinor"></param>
			/// <param name="TimeStamp"></param>
			public static void PullApartVersion(string CFBundleVersion, out int VersionMajor, out int VersionMinor, out string TimeStamp)
			{
				// Expecting source to be like "7301.15 11-01 10:28" or "7486.0"
				string[] Parts = CFBundleVersion.Split(new char[] { ' ' });

				// Parse the version string
				string[] VersionParts = Parts[0].Split(new char[] { '.' });

				if (!int.TryParse(VersionParts[0], out VersionMajor))
				{
					VersionMajor = 0;
				}

				if ((VersionParts.Length < 2) || (!int.TryParse(VersionParts[1], out VersionMinor)))
				{
					VersionMinor = 0;
				}

				TimeStamp = "";
				if (Parts.Length > 1)
				{
					TimeStamp = String.Join(" ", Parts, 1, Parts.Length - 1);
				}
			}

			public static string ConstructVersion(int MajorVersion, int MinorVersion)
			{
				return String.Format("{0}.{1}", MajorVersion, MinorVersion);
			}

			/// <summary>
			/// Parses the version string (expected to be of the form major.minor or major)
			/// Also parses the major.minor from the running version file and increments it's minor by 1.
			/// 
			/// If the running version major matches and the running version minor is newer, then the bundle version is updated.
			/// 
			/// In either case, the running version is set to the current bundle version number and written back out.
			/// </summary>
			/// <returns>The (possibly updated) bundle version</returns>
			public static string CalculateUpdatedMinorVersionString(string CFBundleVersion)
			{
				// Read the running version and bump it
				int RunningMajorVersion;
				int RunningMinorVersion;

				string DummyDate;
				string RunningVersion = ReadRunningVersion();
				PullApartVersion(RunningVersion, out RunningMajorVersion, out RunningMinorVersion, out DummyDate);
				RunningMinorVersion++;

				// Read the passed in version and bump it
				int MajorVersion;
				int MinorVersion;
				PullApartVersion(CFBundleVersion, out MajorVersion, out MinorVersion, out DummyDate);
				MinorVersion++;

				// Combine them if the stub time is older
				if ((RunningMajorVersion == MajorVersion) && (RunningMinorVersion > MinorVersion))
				{
					// A subsequent cook on the same sync, the only time that we stomp on the stub version
					MinorVersion = RunningMinorVersion;
				}

				// Combine them together
				string ResultVersionString = ConstructVersion(MajorVersion, MinorVersion);

				// Update the running version file
				Directory.CreateDirectory(Path.GetDirectoryName(RunningVersionFilename));
				File.WriteAllText(RunningVersionFilename, ResultVersionString);

				return ResultVersionString;
			}

			/// <summary>
			/// Updates the minor version in the CFBundleVersion key of the specified PList if this is a new package.
			/// Also updates the key EpicAppVersion with the bundle version and the current date/time (no year)
			/// </summary>
			public static string UpdateBundleVersion(string OldPList)
			{
				string CFBundleVersion;
				int Index = OldPList.IndexOf("CFBundleVersion");
				if (Index != -1)
				{
					int Start = OldPList.IndexOf("<string>", Index) + ("<string>").Length;
					CFBundleVersion = OldPList.Substring(Start, OldPList.IndexOf("</string>", Index) - Start);
					CFBundleVersion = CalculateUpdatedMinorVersionString(CFBundleVersion);
				}
				else
				{
					CFBundleVersion = "0.0";
				}

				return CFBundleVersion;
			}
		}

		public static bool GeneratePList(string ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, string InEngineDir, string AppDirectory)
		{
			// generate the Info.plist for future use
			string BuildDirectory = ProjectDirectory + "/Build/IOS";
			bool bSkipDefaultPNGs = false;
			string IntermediateDirectory = (bIsUE4Game ? InEngineDir : ProjectDirectory) + "/Intermediate/IOS";
			string PListFile = IntermediateDirectory + "/" + GameName + "-Info.plist";
			VersionUtilities.BuildDirectory = BuildDirectory;
			VersionUtilities.GameName = GameName;

			// read the old file
			string OldPListData = File.Exists(PListFile) ? File.ReadAllText(PListFile) : "";

			// determine if there is a launch.xib
			string LaunchXib = InEngineDir + "/Build/IOS/Resources/Interface/LaunchScreen.xib";
			if (File.Exists(BuildDirectory + "/Resources/Interface/LaunchScreen.xib"))
			{
				LaunchXib = BuildDirectory + "/Resources/Interface/LaunchScreen.xib";
			}

			// get the settings from the ini file
			// plist replacements
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.IOS, "Engine", UnrealBuildTool.GetUProjectPath());

			// orientations
			string SupportedOrientations = "";
			bool bSupported = true;
			Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsPortraitOrientation", out bSupported);
			SupportedOrientations += bSupported ? "\t\t<string>UIInterfaceOrientationPortrait</string>\n" : "";
			Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsUpsideDownOrientation", out bSupported);
			SupportedOrientations += bSupported ? "\t\t<string>UIInterfaceOrientationPortraitUpsideDown</string>\n" : "";
			Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsLandscapeLeftOrientation", out bSupported);
			SupportedOrientations += bSupported ? "\t\t<string>UIInterfaceOrientationLandscapeLeft</string>\n" : "";
			Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsLandscapeRightOrientation", out bSupported);
			SupportedOrientations += bSupported ? "\t\t<string>UIInterfaceOrientationLandscapeRight</string>\n" : "";

			// bundle display name
			string BundleDisplayName;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleDisplayName", out BundleDisplayName);

			// bundle identifier
			string BundleIdentifier;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleIdentifier);

			// bundle name
			string BundleName;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleName", out BundleName);

			// short version string
			string BundleShortVersion;
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "VersionInfo", out BundleShortVersion);

			// required capabilities
			string RequiredCaps = "\t\t<string>armv7</string>\n";
			Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsOpenGLES2", out bSupported);
			RequiredCaps += bSupported ? "\t\t<string>opengles-2</string>\n" : "";
			if (!bSupported)
			{
				Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsMetal", out bSupported);
				RequiredCaps += bSupported ? "\t\t<string>metal</string>\n" : "";
			}

			// minimum iOS version
			string MinVersion;
			if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumiOSVersion", out MinVersion))
			{
				switch (MinVersion)
				{
					case "IOS_61":
						MinVersion = "6.1";
						break;
					case "IOS_7":
						MinVersion = "7.0";
						break;
					case "IOS_8":
						MinVersion = "8.0";
						break;
				}
			}
			else
			{
				MinVersion = "6.1";
			}

			// extra plist data
			string ExtraData = "";
			Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalPlistData", out ExtraData);

			// generate the plist file
			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
			Text.AppendLine("<plist version=\"1.0\">");
			Text.AppendLine("<dict>");
			Text.AppendLine("\t<key>CFBundleURLTypes</key>");
			Text.AppendLine("\t<array>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>CFBundleURLName</key>");
			Text.AppendLine("\t\t\t<string>com.Epic.Unreal</string>");
			Text.AppendLine("\t\t\t<key>CFBundleURLSchemes</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine(string.Format("\t\t\t\t<string>{0}</string>", bIsUE4Game ? "UE4Game" : GameName));
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</array>");
			Text.AppendLine("\t<key>CFBundleDevelopmentRegion</key>");
			Text.AppendLine("\t<string>English</string>");
			Text.AppendLine("\t<key>CFBundleDisplayName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleDisplayName.Replace("[PROJECT_NAME]", ProjectName).Replace("_","")));
			Text.AppendLine("\t<key>CFBundleExecutable</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", bIsUE4Game ? "UE4Game" : GameName));
			Text.AppendLine("\t<key>CFBundleIdentifier</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleIdentifier.Replace("[PROJECT_NAME]", ProjectName).Replace("_","")));
			Text.AppendLine("\t<key>CFBundleInfoDictionaryVersion</key>");
			Text.AppendLine("\t<string>6.0</string>");
			Text.AppendLine("\t<key>CFBundleName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleName.Replace("[PROJECT_NAME]", ProjectName).Replace("_","")));
			Text.AppendLine("\t<key>CFBundlePackageType</key>");
			Text.AppendLine("\t<string>APPL</string>");
			Text.AppendLine("\t<key>CFBundleSignature</key>");
			Text.AppendLine("\t<string>????</string>");
			Text.AppendLine("\t<key>CFBundleVersion</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", VersionUtilities.UpdateBundleVersion(OldPListData)));
			Text.AppendLine("\t<key>CFBundleShortVersionString</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", BundleShortVersion));
			Text.AppendLine("\t<key>LSRequiresIPhoneOS</key>");
			Text.AppendLine("\t<true/>");
			Text.AppendLine("\t<key>UIStatusBarHidden</key>");
			Text.AppendLine("\t<true/>");
			Text.AppendLine("\t<key>UISupportedInterfaceOrientations</key>");
			Text.AppendLine("\t<array>");
			foreach (string Line in SupportedOrientations.Split("\r\n".ToCharArray()))
			{
				if (!string.IsNullOrWhiteSpace(Line))
				{
					Text.AppendLine(Line);
				}
			}
			Text.AppendLine("\t</array>");
			Text.AppendLine("\t<key>UIRequiredDeviceCapabilities</key>");
			Text.AppendLine("\t<array>");
			foreach (string Line in RequiredCaps.Split("\r\n".ToCharArray()))
			{
				if (!string.IsNullOrWhiteSpace(Line))
				{
					Text.AppendLine(Line);
				}
			}
			Text.AppendLine("\t</array>");
			Text.AppendLine("\t<key>CFBundleIcons</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine("\t\t\t\t<string>Icon29.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon29@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon40.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon40@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon57.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon57@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon60@2x.png</string>");
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
			Text.AppendLine("\t\t\t<true/>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>CFBundleIcons~ipad</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>CFBundlePrimaryIcon</key>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>CFBundleIconFiles</key>");
			Text.AppendLine("\t\t\t<array>");
			Text.AppendLine("\t\t\t\t<string>Icon29.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon29@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon40.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon40@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon50.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon50@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon72.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon72@2x.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon76.png</string>");
			Text.AppendLine("\t\t\t\t<string>Icon76@2x.png</string>");
			Text.AppendLine("\t\t\t</array>");
			Text.AppendLine("\t\t\t<key>UIPrerenderedIcon</key>");
			Text.AppendLine("\t\t\t<true/>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</dict>");
 			if (File.Exists(LaunchXib))
			{
				// TODO: compile the xib via remote tool
				Text.AppendLine("\t<key>UILaunchStoryboardName</key>");
				Text.AppendLine("\t<string>LaunchScreen</string>");
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

				Text.AppendLine("\t<key>UILaunchImages~iphone</key>");
				Text.AppendLine("\t<array>");
				for (int ConfigIndex = 0; ConfigIndex < IPhoneConfigs.Length; ConfigIndex += 3)
				{
					Text.AppendLine("\t\t<dict>");
					Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
					Text.AppendLine("\t\t\t<string>8.0</string>");
					Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
					Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 0]));
					Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
					Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 1]));
					Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
					Text.AppendLine(string.Format("\t\t\t<string>{0}</string>", IPhoneConfigs[ConfigIndex + 2]));
					Text.AppendLine("\t\t</dict>");
				}

				// close it out
				Text.AppendLine("\t</array>");
			}
			Text.AppendLine("\t<key>UILaunchImages~ipad</key>");
			Text.AppendLine("\t<array>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
			Text.AppendLine("\t\t\t<string>7.0</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
			Text.AppendLine("\t\t\t<string>Default-Landscape</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
			Text.AppendLine("\t\t\t<string>Landscape</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
			Text.AppendLine("\t\t\t<string>{768, 1024}</string>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t\t<dict>");
			Text.AppendLine("\t\t\t<key>UILaunchImageMinimumOSVersion</key>");
			Text.AppendLine("\t\t\t<string>7.0</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageName</key>");
			Text.AppendLine("\t\t\t<string>Default-Portrait</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageOrientation</key>");
			Text.AppendLine("\t\t\t<string>Portrait</string>");
			Text.AppendLine("\t\t\t<key>UILaunchImageSize</key>");
			Text.AppendLine("\t\t\t<string>{768, 1024}</string>");
			Text.AppendLine("\t\t</dict>");
			Text.AppendLine("\t</array>");
			Text.AppendLine("\t<key>CFBundleSupportedPlatforms</key>");
			Text.AppendLine("\t<array>");
			Text.AppendLine("\t\t<string>iPhoneOS</string>");
			Text.AppendLine("\t</array>");
			Text.AppendLine("\t<key>MinimumOSVersion</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", MinVersion));
			if (!string.IsNullOrEmpty(ExtraData))
			{
				ExtraData = ExtraData.Replace("\\n", "\n");
				foreach (string Line in ExtraData.Split("\r\n".ToCharArray()))
				{
					if (!string.IsNullOrWhiteSpace(Line))
					{
						Text.AppendLine("\t" + Line);
					}
				}
			}
			Text.AppendLine("</dict>");
			Text.AppendLine("</plist>");

			// write the file out
			if (!Directory.Exists(IntermediateDirectory))
			{
				Directory.CreateDirectory(IntermediateDirectory);
			}
			File.WriteAllText(PListFile, Text.ToString());
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				if (!Directory.Exists(AppDirectory))
				{
					Directory.CreateDirectory(AppDirectory);
				}
				File.WriteAllText(AppDirectory + "/Info.plist", Text.ToString());
			}

			return bSkipDefaultPNGs;
		}

		public override bool PrepForUATPackageOrDeploy(string InProjectName, string InProjectDirectory, string InExecutablePath, string InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
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

			Directory.CreateDirectory(BinaryPath);
			Directory.CreateDirectory(PayloadDirectory);
			Directory.CreateDirectory(AppDirectory);
			Directory.CreateDirectory(BuildDirectory);

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
//			string LaunchXib = InEngineDir + "/Build/IOS/Resources/Interface/LaunchScreen.xib";
//			if (File.Exists(BuildDirectory + "/Resources/Interface/LaunchScreen.xib"))
//			{
//				LaunchXib = BuildDirectory + "/Resources/Interface/LaunchScreen.xib";
//			}

			bool bSkipDefaultPNGs = GeneratePList(InProjectDirectory, bIsUE4Game, GameName, InProjectName, InEngineDir, AppDirectory);

			// ensure the destination is writable
			if (File.Exists(AppDirectory + "/" + GameName))
			{
				FileInfo GameFileInfo = new FileInfo(AppDirectory + "/" + GameName);
				GameFileInfo.Attributes = GameFileInfo.Attributes & ~FileAttributes.ReadOnly;
			}

			// copy the GameName binary
			File.Copy(BinaryPath + "/" + GameExeName, AppDirectory + "/" + GameName, true);

			if (!BuildConfiguration.bCreateStubIPA)
			{
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
				if (Directory.Exists(InProjectDirectory + "/Intermediate/IOS/FrameworkAssets"))
				{
					FrameworkAssetsPath = InProjectDirectory + "/Intermediate/IOS/FrameworkAssets";
				}

				if (Directory.Exists(FrameworkAssetsPath))
				{
					CopyFolder(FrameworkAssetsPath, AppDirectory, true);
				}

				Directory.CreateDirectory(CookedContentDirectory);
				CopyFolder(InEngineDir + "/Content/Stats", AppDirectory + "/cookeddata/engine/content/stats", true);
			}

			return true;
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			string GameName = InTarget.TargetName;
			string BuildPath = (GameName == "UE4Game" ? "../../Engine" : InTarget.ProjectDirectory) + "/Binaries/IOS";
			string ProjectDirectory = InTarget.ProjectDirectory;
			bool bIsUE4Game = GameName.Contains("UE4Game");

			string DecoratedGameName;
			if (InTarget.Configuration == UnrealTargetConfiguration.Development)
			{
				DecoratedGameName = GameName;
			}
			else
			{
				DecoratedGameName = String.Format("{0}-{1}-{2}", GameName, InTarget.Platform.ToString(), InTarget.Configuration.ToString());
			}

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac && Environment.GetEnvironmentVariable("UBT_NO_POST_DEPLOY") != "true")
			{
				return PrepForUATPackageOrDeploy(GameName, ProjectDirectory, BuildPath + "/" + DecoratedGameName, "../../Engine", false, "", false);
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

				GeneratePList(ProjectDirectory, bIsUE4Game, GameName, Path.GetFileNameWithoutExtension(UnrealBuildTool.GetUProjectFile()), "../../Engine", "");
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
