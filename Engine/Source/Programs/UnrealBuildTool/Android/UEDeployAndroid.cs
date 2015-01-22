// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool.Android
{
	public class UEDeployAndroid : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			string NDKPath = Environment.GetEnvironmentVariable("ANDROID_HOME");

			// we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				Log.TraceVerbose("        Unable to register Android deployment class because the ANDROID_HOME environment variable isn't set or points to something that doesn't exist");
			}
			else
			{
				UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.Android, this);
			}
		}

		/** Internal usage for GetApiLevel */
		private static List<string> PossibleApiLevels = null;

		/** Simple function to pipe output asynchronously */
		private static void ParseApiLevel(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				string Line = Event.Data;
				if (Line.StartsWith("id:"))
				{
					// the line should look like: id: 1 or "android-19"
					string[] Tokens = Line.Split("\"".ToCharArray());
					if (Tokens.Length >= 2)
					{
						PossibleApiLevels.Add(Tokens[1]);
					}
				}
			}
		}

		static private string CachedSDKLevel = null;
		private static string GetSdkApiLevel()
		{
			if (CachedSDKLevel == null)
			{
				// default to looking on disk for latest API level
				string Target = AndroidPlatform.AndroidSdkApiTarget;

				// if we want to use whatever version the ndk uses, then use that
				if (Target == "matchndk")
				{
					Target = AndroidToolChain.GetNdkApiLevel();
				}

				// run a command and capture output
				if (Target == "latest")
				{
					// we expect there to be one, so use the first one
					string AndroidCommandPath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/tools/android" + (Utils.IsRunningOnMono ? "" : ".bat"));

					var ExeInfo = new ProcessStartInfo(AndroidCommandPath, "list targets");
					ExeInfo.UseShellExecute = false;
					ExeInfo.RedirectStandardOutput = true;
					using (var GameProcess = Process.Start(ExeInfo))
					{
						PossibleApiLevels = new List<string>();
						GameProcess.BeginOutputReadLine();
						GameProcess.OutputDataReceived += ParseApiLevel;
						GameProcess.WaitForExit();
					}

					if (PossibleApiLevels != null && PossibleApiLevels.Count > 0)
					{
						Target = AndroidToolChain.GetLargestApiLevel(PossibleApiLevels.ToArray());
					}
					else
					{
						throw new BuildException("Can't make an APK an API installed (see \"android.bat list targets\")");
					}
				}

				Console.WriteLine("Building Java with SDK API '{0}'", Target);
				CachedSDKLevel = Target;
			}

			return CachedSDKLevel;
		}

		private static string GetAntPath()
		{
			// look up an ANT_HOME env var
			string AntHome = Environment.GetEnvironmentVariable("ANT_HOME");
			if (!string.IsNullOrEmpty(AntHome) && Directory.Exists(AntHome))
			{
				string AntPath = AntHome + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
				// use it if found
				if (File.Exists(AntPath))
				{
					return AntPath;
				}
			}

			// otherwise, look in the eclipse install for the ant plugin (matches the unzipped Android ADT from Google)
			string PluginDir = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/../eclipse/plugins");
			if (Directory.Exists(PluginDir))
			{
				string[] Plugins = Directory.GetDirectories(PluginDir, "org.apache.ant*");
				// use the first one with ant.bat
				if (Plugins.Length > 0)
				{
					foreach (string PluginName in Plugins)
					{
						string AntPath = PluginName + "/bin/ant" + (Utils.IsRunningOnMono ? "" : ".bat");
						// use it if found
						if (File.Exists(AntPath))
						{
							return AntPath;
						}
					}
				}
			}

			throw new BuildException("Unable to find ant.bat (via %ANT_HOME% or %ANDROID_HOME%/../eclipse/plugins/org.apache.ant*");
		}


		private static void CopyFileDirectory(string SourceDir, string DestDir, Dictionary<string,string> Replacements)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				// skip template files
				if (Path.GetExtension(Filename) == ".template")
				{
					continue;
				}

				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));
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
				string Ext = Path.GetExtension(Filename);
				if (Ext == ".xml")
				{
					string Contents = File.ReadAllText(Filename);

					// replace some varaibles
					foreach (var Pair in Replacements)
					{
						Contents = Contents.Replace(Pair.Key, Pair.Value);
					}

					// write out file
					File.WriteAllText(DestFilename, Contents);
				}
				else
				{
					File.Copy(Filename, DestFilename);

					// remove any read only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				}
			}
		}

		private static void DeleteDirectory(string InPath, string SubDirectoryToKeep="")
		{
			// skip the dir we want to
			if (String.Compare(Path.GetFileName(InPath), SubDirectoryToKeep, true) == 0)
			{
				return;
			}
			
			// delete all files in here
            string[] Files;
            try
            {
                Files = Directory.GetFiles(InPath);
            }
            catch (Exception)
            {
                // directory doesn't exist so all is good
                return;
            }
			foreach (string Filename in Files)
			{
				try
				{
					// remove any read only flags
					FileInfo FileInfo = new FileInfo(Filename);
					FileInfo.Attributes = FileInfo.Attributes & ~FileAttributes.ReadOnly;
					FileInfo.Delete();
				}
				catch (Exception)
				{
					Log.TraceInformation("Failed to delete all files in directory {0}. Continuing on...", InPath);
				}
			}

			string[] Dirs = Directory.GetDirectories(InPath, "*.*", SearchOption.TopDirectoryOnly);
			foreach (string Dir in Dirs)
			{
				DeleteDirectory(Dir, SubDirectoryToKeep);
				// try to delete the directory, but allow it to fail (due to SubDirectoryToKeep still existing)
				try
				{
					Directory.Delete(Dir);
				}
				catch (Exception)
				{
					// do nothing
				}
			}
		}

        public string GetUE4BuildFilePath(String EngineDirectory)
        {
            return Path.GetFullPath(Path.Combine(EngineDirectory, "Build/Android/Java"));
        }

        public string GetUE4JavaFilePath(String EngineDirectory)
        {
            return Path.GetFullPath(Path.Combine(GetUE4BuildFilePath(EngineDirectory), "src/com/epicgames/ue4"));
        }

        public string GetUE4JavaBuildSettingsFileName(String EngineDirectory)
        {
            return Path.Combine(GetUE4JavaFilePath(EngineDirectory), "JavaBuildSettings.java");
        }

        public void WriteJavaBuildSettingsFile(string FileName, bool OBBinAPK)
        {
             // (!UEBuildConfiguration.bOBBinAPK ? "PackageType.AMAZON" : /*bPackageForGoogle ? "PackageType.GOOGLE" :*/ "PackageType.DEVELOPMENT") + ";\n");
            string Setting = OBBinAPK ? "AMAZON" : "DEVELOPMENT";
            if (!File.Exists(FileName) || ShouldWriteJavaBuildSettingsFile(FileName, Setting))
            {
				Log.TraceInformation("\n===={0}====WRITING JAVABUILDSETTINGS.JAVA========================================================", DateTime.Now.ToString());
				StringBuilder BuildSettings = new StringBuilder("package com.epicgames.ue4;\npublic class JavaBuildSettings\n{\n");
                BuildSettings.Append("\tpublic enum PackageType {AMAZON, GOOGLE, DEVELOPMENT};\n");
                BuildSettings.Append("\tpublic static final PackageType PACKAGING = PackageType." + Setting + ";\n");
                BuildSettings.Append("}\n");
                File.WriteAllText(FileName, BuildSettings.ToString());
            }
        }

        public bool ShouldWriteJavaBuildSettingsFile(string FileName, string setting)
        {
            var fileContent = File.ReadAllLines(FileName);
            if (fileContent.Length < 5)
                return true;
            var packageLine = fileContent[4]; // We know this to be true... because we write it below...
            int location = packageLine.IndexOf("PACKAGING") + 12 + 12; // + ("PACKAGING = ") + ("PackageType.")
            if (location == -1)
                return true;
            return String.Compare(setting, packageLine.Substring(location, Math.Min(packageLine.Length - location - 1, setting.Length))) != 0;
        }

		private static string GetNDKArch(string UE4Arch)
		{
			switch (UE4Arch)
			{
				case "-armv7": return "armeabi-v7a";
				case "-x86": return "x86";

				default: throw new BuildException("Unknown UE4 architecture {0}", UE4Arch);
			}
		}

		public static string GetUE4Arch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "armeabi-v7a": return "-armv7";
				case "x86":			return "-x86";

				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
			}
		}

		private static void CopySTL(string UE4BuildPath, string UE4Arch)
		{
			string Arch = GetNDKArch(UE4Arch);

			string GccVersion = "4.8";
			if (!Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.6";
			}

			// copy it in!
			string SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/") + GccVersion + "/libs/" + Arch + "/libgnustl_shared.so";
			string FinalSTLSOName = UE4BuildPath + "/libs/" + Arch + "/libgnustl_shared.so";
			Directory.CreateDirectory(Path.GetDirectoryName(FinalSTLSOName));
			File.Copy(SourceSTLSOName, FinalSTLSOName);
		}

		//@TODO: To enable the NVIDIA Gfx Debugger
		private static void CopyGfxDebugger(string UE4BuildPath, string UE4Arch)
		{
//			string Arch = GetNDKArch(UE4Arch);
//			Directory.CreateDirectory(UE4BuildPath + "/libs/" + Arch);
//			File.Copy("E:/Dev2/UE4-Clean/Engine/Source/ThirdParty/NVIDIA/TegraGfxDebugger/libNvPmApi.Core.so", UE4BuildPath + "/libs/" + Arch + "/libNvPmApi.Core.so");
//			File.Copy("E:/Dev2/UE4-Clean/Engine/Source/ThirdParty/NVIDIA/TegraGfxDebugger/libTegra_gfx_debugger.so", UE4BuildPath + "/libs/" + Arch + "/libTegra_gfx_debugger.so");
		}

		private static void RunCommandLineProgramAndThrowOnError(string WorkingDirectory, string Command, string Params, string OverrideDesc=null, bool bUseShellExecute=false)
		{
			if (OverrideDesc == null)
			{
				Log.TraceInformation("\nRunning: " + Command + " " + Params);
			}
			else if (OverrideDesc != "")
			{
				Log.TraceInformation(OverrideDesc);
				Log.TraceVerbose("\nRunning: " + Command + " " + Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			// android bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private void UpdateProjectProperties(string UE4BuildPath, string ProjectName)
		{
			Log.TraceInformation("\n===={0}====UPDATING BUILD CONFIGURATION FILES====================================================", DateTime.Now.ToString());

			// get all of the libs (from engine + project)
			string JavaLibsDir = Path.Combine(UE4BuildPath, "JavaLibs");
			string[] LibDirs = Directory.GetDirectories(JavaLibsDir);

			// get existing project.properties lines (if any)
			string ProjectPropertiesFile = Path.Combine(UE4BuildPath, "project.properties");
			string[] PropertiesLines = new string[] { };
			if (File.Exists(ProjectPropertiesFile))
			{
				PropertiesLines = File.ReadAllLines(ProjectPropertiesFile);
			}

			// figure out how many libraries were already listed (if there were more than this listed, then we need to start the file over, because we need to unreference a library)
			int NumOutstandingAlreadyReferencedLibs = 0;
			foreach (string Line in PropertiesLines)
			{
				if (Line.StartsWith("android.library.reference."))
				{
					NumOutstandingAlreadyReferencedLibs++;
				}
			}

			// now go through each one and verify they are listed in project properties, and if not, add them
			List<string> LibsToBeAdded = new List<string>();
			foreach (string LibDir in LibDirs)
			{
				// put it in terms of the subdirectory that would be in the project.properties
				string RelativePath = "JavaLibs/" + Path.GetFileName(LibDir);

				// now look for this in the existing file
				bool bWasReferencedAlready = false;
				foreach (string Line in PropertiesLines)
				{
					if (Line.StartsWith("android.library.reference.") && Line.EndsWith(RelativePath))
					{
						// this lib was already referenced, don't need to readd
						bWasReferencedAlready = true;
						break;
					}
				}

				if (bWasReferencedAlready)
				{
					// if it was, no further action needed, and count it off
					NumOutstandingAlreadyReferencedLibs--;
				}
				else
				{
					// otherwise, we need to add it to the project properties
					LibsToBeAdded.Add(RelativePath);
				}
			}

			// now at this point, if there are any outstanding already referenced libs, we have too many, so we have to start over
			if (NumOutstandingAlreadyReferencedLibs > 0)
			{
				// @todo android: If a user had a project.properties in the game, NEVER do this
				Log.TraceInformation("There were too many libs already referenced in project.properties, tossing it");
				File.Delete(ProjectPropertiesFile);

				LibsToBeAdded.Clear();
				foreach (string LibDir in LibDirs)
				{
					// put it in terms of the subdirectory that would be in the project.properties
					LibsToBeAdded.Add("JavaLibs/" + Path.GetFileName(LibDir));
				}
			}
			
			// now update the project for each library
			string AndroidCommandPath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/tools/android" + (Utils.IsRunningOnMono ? "" : ".bat"));
			string UpdateCommandLine = "--silent update project --subprojects --name " + ProjectName + " --path . --target " + GetSdkApiLevel();
			foreach (string Lib in LibsToBeAdded)
			{
				string LocalUpdateCommandLine = UpdateCommandLine + " --library " + Lib;

				// make sure each library has a build.xml - --subprojects doesn't create build.xml files, but it will create project.properties
				// and later code needs each lib to have a build.xml
				if (!File.Exists(Path.Combine(Lib, "build.xml")))
				{
					RunCommandLineProgramAndThrowOnError(UE4BuildPath, AndroidCommandPath, "--silent update lib-project --path " + Lib + " --target " + GetSdkApiLevel(), "");
				}
				RunCommandLineProgramAndThrowOnError(UE4BuildPath, AndroidCommandPath, LocalUpdateCommandLine, "Updating project.properties, local.properties, and build.xml...");
			}

		}


		private string GetAllBuildSettings(string BuildPath, bool bForDistribution, bool bMakeSeparateApks, bool bOBBinApk)
		{
			// make the settings string - this will be char by char compared against last time
			StringBuilder CurrentSettings = new StringBuilder();
			CurrentSettings.AppendFormat("NDKROOT={0}{1}", Environment.GetEnvironmentVariable("NDKROOT"), Environment.NewLine);
			CurrentSettings.AppendFormat("ANDROID_HOME={0}{1}", Environment.GetEnvironmentVariable("ANDROID_HOME"), Environment.NewLine);
			CurrentSettings.AppendFormat("ANT_HOME={0}{1}", Environment.GetEnvironmentVariable("ANT_HOME"), Environment.NewLine);
			CurrentSettings.AppendFormat("JAVA_HOME={0}{1}", Environment.GetEnvironmentVariable("JAVA_HOME"), Environment.NewLine);
			CurrentSettings.AppendFormat("SDKVersion={0}{1}", GetSdkApiLevel(), Environment.NewLine);
			CurrentSettings.AppendFormat("bForDistribution={0}{1}", bForDistribution, Environment.NewLine);
			CurrentSettings.AppendFormat("bMakeSeparateApks={0}{1}", bMakeSeparateApks, Environment.NewLine);
			CurrentSettings.AppendFormat("bOBBinApk={0}{1}", bOBBinApk, Environment.NewLine);

			string[] Arches = AndroidToolChain.GetAllArchitectures();
			foreach (string Arch in Arches)
			{
				CurrentSettings.AppendFormat("Arch={0}{1}", Arch, Environment.NewLine);
			}

			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();
			foreach (string GPUArch in GPUArchitectures)
			{
				CurrentSettings.AppendFormat("GPUArch={0}{1}", GPUArch, Environment.NewLine);
			}

			return CurrentSettings.ToString();
		}

		private bool CheckDependencies(string ProjectName, string ProjectDirectory, string UE4BuildFilesPath, string GameBuildFilesPath, string EngineDirectory, string JavaSettingsFile, string CookFlavor, string OutputPath, string UE4BuildPath, bool bMakeSeparateApks)
		{
			string[] Arches = AndroidToolChain.GetAllArchitectures();
			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();

			// check all input files (.so, java files, .ini files, etc)
			bool bAllInputsCurrent = true;
			foreach (string Arch in Arches)
			{
				foreach (string GPUArch in GPUArchitectures)
				{
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArch);
					// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
					string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

					// if we making multiple Apks, we need to put the architecture into the name
					if (bMakeSeparateApks)
					{
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArch);
					}

					// check to see if it's out of date before trying the slow make apk process (look at .so and all Engine and Project build files to be safe)
					List<String> InputFiles = new List<string>();
					InputFiles.Add(SourceSOName);
					InputFiles.AddRange(Directory.EnumerateFiles(UE4BuildFilesPath, "*.*", SearchOption.AllDirectories));
					if (Directory.Exists(GameBuildFilesPath))
					{
						InputFiles.AddRange(Directory.EnumerateFiles(GameBuildFilesPath, "*.*", SearchOption.AllDirectories));
					}

					// rebuild if .ini files change
					// @todo android: programmatically determine if any .ini setting changed?
					InputFiles.Add(Path.Combine(EngineDirectory, "Config\\BaseEngine.ini"));
					InputFiles.Add(Path.Combine(ProjectDirectory, "Config\\DefaultEngine.ini"));

					// make sure changed java settings will rebuild apk
					InputFiles.Add(JavaSettingsFile);

					// rebuild if .pak files exist for OBB in APK case
					if (UEBuildConfiguration.bOBBinAPK)
					{
						string PAKFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + "/" + ProjectName + "/Content/Paks";
						if (Directory.Exists(PAKFileLocation))
						{
							var PakFiles = Directory.EnumerateFiles(PAKFileLocation, "*.pak", SearchOption.TopDirectoryOnly);
							foreach (var Name in PakFiles)
							{
								InputFiles.Add(Name);
							}
						}
					}

					// look for any newer input file
					DateTime ApkTime = File.GetLastWriteTimeUtc(DestApkName);
					foreach (var InputFileName in InputFiles)
					{
						if (File.Exists(InputFileName))
						{
							// skip .log files
							if (Path.GetExtension(InputFileName) == ".log")
							{
								continue;
							}
							DateTime InputFileTime = File.GetLastWriteTimeUtc(InputFileName);
							if (InputFileTime.CompareTo(ApkTime) > 0)
							{
								bAllInputsCurrent = false;
								Log.TraceInformation("{0} is out of date due to newer input file {1}", DestApkName, InputFileName);
								break;
							}
						}
					}
				}
			}

			return bAllInputsCurrent;
		}

		private void MakeApk(string ProjectName, string ProjectDirectory, string OutputPath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bMakeSeparateApks, bool bIncrementalPackage)
		{
			Log.TraceInformation("\n===={0}====PREPARING TO MAKE APK=================================================================", DateTime.Now.ToString());

			// cache some tools paths
			string NDKBuildPath = Environment.ExpandEnvironmentVariables("%NDKROOT%/ndk-build" + (Utils.IsRunningOnMono ? "" : ".cmd"));

			// set up some directory info
			string IntermediateAndroidPath = Path.Combine(ProjectDirectory, "Intermediate/Android/");
			string UE4BuildPath = Path.Combine(IntermediateAndroidPath, "APK");
			string UE4BuildFilesPath = GetUE4BuildFilePath(EngineDirectory);
			string GameBuildFilesPath = Path.Combine(ProjectDirectory, "Build/Android");
	
			string[] Arches = AndroidToolChain.GetAllArchitectures();
			string[] GPUArchitectures = AndroidToolChain.GetAllGPUArchitectures();
//			int NumArches = Arches.Length * GPUArchitectures.Length;


            // See if we need to create a 'default' Java Build settings file if one doesn't exist (if it does exist we have to assume it has been setup correctly)
            string UE4JavaBuildSettingsFileName = GetUE4JavaBuildSettingsFileName(EngineDirectory);
            WriteJavaBuildSettingsFile(UE4JavaBuildSettingsFileName, UEBuildConfiguration.bOBBinAPK);

			// check to see if any "meta information" is newer than last time we build
			string CurrentBuildSettings = GetAllBuildSettings(UE4BuildPath, bForDistribution, bMakeSeparateApks, UEBuildConfiguration.bOBBinAPK);
			string BuildSettingsCacheFile = Path.Combine(UE4BuildPath, "UEBuildSettings.txt");

			// do we match previous build settings?
			bool bBuildSettingsMatch = false;
			if (File.Exists(BuildSettingsCacheFile))
			{
				string PreviousBuildSettings = File.ReadAllText(BuildSettingsCacheFile);
				if (PreviousBuildSettings == CurrentBuildSettings)
				{
					bBuildSettingsMatch = true;
				}
				else
				{
					Log.TraceInformation("Previous .apk file(s) were made with different build settings, forcing repackage.");
				}
			}

			// only check input dependencies if the build settings already match
			if (bBuildSettingsMatch)
			{
				// check if so's are up to date against various inputs
				bool bAllInputsCurrent = CheckDependencies(ProjectName, ProjectDirectory, UE4BuildFilesPath, GameBuildFilesPath, 
					EngineDirectory, UE4JavaBuildSettingsFileName, CookFlavor, OutputPath, UE4BuildPath, bMakeSeparateApks);

				if (bAllInputsCurrent)
				{
					Log.TraceInformation("Output .apk file(s) are up to date (dependencies and build settings are up to date)");
					return;
				}
			}


			// Once for all arches code:

			// make up a dictionary of strings to replace in the Manifest file
			Dictionary<string, string> Replacements = new Dictionary<string, string>();
			Replacements.Add("${EXECUTABLE_NAME}", ProjectName);

			// distribution apps can't be debuggable, so if it was set to true, set it to false:
			if (bForDistribution)
			{
				Replacements.Add("android:debuggable=\"true\"", "android:debuggable=\"false\"");
			}

			if (!bIncrementalPackage)
			{
				// Wipe the Intermediate/Build/APK directory first, except for dexedLibs, because Google Services takes FOREVER to predex, and it almost never changes
				// so allow the ANT checking to win here - if this grows a bit with extra libs, it's fine, it _should_ only pull in dexedLibs it needs
				Log.TraceInformation("Performing complete package - wiping {0}, except for predexedLibs", UE4BuildPath);
				DeleteDirectory(UE4BuildPath, "dexedLibs");
			}

			// If we are packaging for Amazon then we need to copy the OBB file to the correct location
			Log.TraceInformation("UEBuildConfiguration.bOBBinAPK = {0}", UEBuildConfiguration.bOBBinAPK);
			if (UEBuildConfiguration.bOBBinAPK)
			{
				string ObbFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".obb";
				Console.WriteLine("Obb location {0}", ObbFileLocation);
				string ObbFileDestination = UE4BuildPath + "/assets";
				Console.WriteLine("Obb destination location {0}", ObbFileDestination);
				if (File.Exists(ObbFileLocation))
				{
					Directory.CreateDirectory(UE4BuildPath);
					Directory.CreateDirectory(ObbFileDestination);
					Console.WriteLine("Obb file exists...");
					var DestFileName = Path.Combine(ObbFileDestination, "main.obb.png"); // Need a rename to turn off compression
					var SrcFileName = ObbFileLocation;
					if (!File.Exists(DestFileName) || File.GetLastWriteTimeUtc(DestFileName) < File.GetLastWriteTimeUtc(SrcFileName))
					{
						Console.WriteLine("Copying {0} to {1}", SrcFileName, DestFileName);
						File.Copy(SrcFileName, DestFileName);
					}
				}
			}

			//Copy build files to the intermediate folder in this order (later overrides earlier):
			//	- Shared Engine
			//  - Shared Engine NoRedist (for Epic secret files)
			//  - Game
			//  - Game NoRedist (for Epic secret files)
			CopyFileDirectory(UE4BuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(UE4BuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath, UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NotForLicensees", UE4BuildPath, Replacements);
			CopyFileDirectory(GameBuildFilesPath + "/NoRedist", UE4BuildPath, Replacements);

			// update metadata files (like project.properties, build.xml) if we are missing a build.xml or if we just overwrote project.properties with a bad version in it (from game/engine dir)
			UpdateProjectProperties(UE4BuildPath, ProjectName);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			// now make the apk(s)
			string FinalNdkBuildABICommand = "";
			for (int ArchIndex = 0; ArchIndex < Arches.Length; ArchIndex++)
			{
				string Arch = Arches[ArchIndex];
				for (int GPUArchIndex = 0; GPUArchIndex < GPUArchitectures.Length; GPUArchIndex++)
				{
					Log.TraceInformation("\n===={0}====PREPARING NATIVE CODE=================================================================", DateTime.Now.ToString());

					string GPUArchitecture = GPUArchitectures[GPUArchIndex];
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArchitecture);
					// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
					string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

					// if we making multiple Apks, we need to put the architecture into the name
					if (bMakeSeparateApks)
					{
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArchitecture);
					}

					// Copy the generated .so file from the binaries directory to the jni folder
					if (!File.Exists(SourceSOName))
					{
						throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
					}
					if (!Directory.Exists(UE4BuildPath + "/jni"))
					{
						throw new BuildException("Can't make an APK without the jni directory [{0}/jni]", UE4BuildFilesPath);
					}

					// Use ndk-build to do stuff and move the .so file to the lib folder (only if NDK is installed)
					string FinalSOName = "";
					if (File.Exists(NDKBuildPath))
					{
						string LibDir = UE4BuildPath + "/jni/" + GetNDKArch(Arch);
						Directory.CreateDirectory(LibDir);

						// copy the binary to the standard .so location
						FinalSOName = LibDir + "/libUE4.so";
						File.Copy(SourceSOName, FinalSOName, true);

						FinalNdkBuildABICommand += GetNDKArch(Arch) + " ";
					}
					else
					{
						// if no NDK, we don't need any of the debugger stuff, so we just copy the .so to where it will end up
						FinalSOName = UE4BuildPath + "/libs/" + GetNDKArch(Arch) + "/libUE4.so";
						Directory.CreateDirectory(Path.GetDirectoryName(FinalSOName));
						File.Copy(SourceSOName, FinalSOName);
					}

					// now do final stuff per apk (or after all .so's for a shared .apk)
					if (bMakeSeparateApks || (ArchIndex == Arches.Length - 1 && GPUArchIndex == GPUArchitectures.Length - 1))
					{
						// always delete libs up to this point so fat binaries and incremental builds work together (otherwise we might end up with multiple
						// so files in an apk that doesn't want them)
						// note that we don't want to delete all libs, just the ones we copied (
						foreach (string Lib in Directory.EnumerateFiles(UE4BuildPath + "/libs", "libUE4*.so", SearchOption.AllDirectories))
						{
							File.Delete(Lib);
						}

						// if we need to run ndk-build, do it now (if making a shared .apk, we need to wait until all .libs exist)
						if (!string.IsNullOrEmpty(FinalNdkBuildABICommand))
						{
							string CommandLine = "APP_ABI=\"" + FinalNdkBuildABICommand + "\"";
							if (!bForDistribution)
							{
								CommandLine += " NDK_DEBUG=1";
							}
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, NDKBuildPath, CommandLine, "Preparing native code for debugging...", true);

							// next loop we don't want to redo these ABIs
							FinalNdkBuildABICommand = "";
						}
	
						// after ndk-build is called, we can now copy in the stl .so (ndk-build deletes old files)
						// copy libgnustl_shared.so to library (use 4.8 if possible, otherwise 4.6)
						if (bMakeSeparateApks)
						{
							CopySTL(UE4BuildPath, Arch);
							CopyGfxDebugger(UE4BuildPath, Arch);
						}
						else
						{
							foreach (string InnerArch in Arches)
							{
								CopySTL(UE4BuildPath, InnerArch);
								CopyGfxDebugger(UE4BuildPath, InnerArch);
							}
						}


						// remove any read only flags
						FileInfo DestFileInfo = new FileInfo(FinalSOName);
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;

						Log.TraceInformation("\n===={0}====PERFORMING FINAL APK PACKAGE OPERATION================================================", DateTime.Now.ToString());

						string AntBuildType = "debug";
						string AntOutputSuffix = "-debug";
						if (bForDistribution)
						{
							// this will write out ant.properties with info needed to sign a distribution build
							PrepareToSignApk(UE4BuildPath);
							AntBuildType = "release";
							AntOutputSuffix = "-release";
						}

						// Use ant to build the .apk file
						if (Utils.IsRunningOnMono)
						{
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, "/bin/sh", "-c '\"" + GetAntPath() + "\" -quiet " + AntBuildType + "'", "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)");
						}
						else
						{
							RunCommandLineProgramAndThrowOnError(UE4BuildPath, "cmd.exe", "/c \"" + GetAntPath() + "\" -quiet " + AntBuildType, "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)");
						}

						// make sure destination exists
						Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

						// now copy to the final location
						File.Copy(UE4BuildPath + "/bin/" + ProjectName + AntOutputSuffix + ".apk", DestApkName, true);
					}
				}
			}
		}

		private void PrepareToSignApk(string BuildPath)
		{
			string ConfigFilePath = Path.Combine(BuildPath, "SigningConfig.xml");
			if (!File.Exists(ConfigFilePath))
			{
				throw new BuildException("Unable to sign for Shipping without signing config file: '{0}", ConfigFilePath);
			}

			// open an Xml parser for the config file
			string ConfigFile = File.ReadAllText(ConfigFilePath);
			XmlReader Xml = XmlReader.Create(new StringReader(ConfigFile));

			string Alias = "UESigningKey";
			string KeystorePassword = "";
			string KeyPassword = "_sameaskeystore_";
			string Keystore = "UE.keystore";

			Xml.ReadToFollowing("ue-signing-config");
			bool bFinishedSection = false;
			while (Xml.Read() && !bFinishedSection)
			{
				switch (Xml.NodeType)
				{
					case XmlNodeType.Element:
						if (Xml.Name == "keyalias")
						{
							Alias = Xml.ReadElementContentAsString();
						}
						else if (Xml.Name == "keystorepassword")
						{
							KeystorePassword = Xml.ReadElementContentAsString();
						}
						else if (Xml.Name == "keypassword")
						{
							KeyPassword = Xml.ReadElementContentAsString();
						}
						else if (Xml.Name == "keystore")
						{
							Keystore = Xml.ReadElementContentAsString();
						}
						break;
					case XmlNodeType.EndElement:
						if (Xml.Name == "ue-signing-config")
						{
							bFinishedSection = true;
						}
						break;
				}
			}


			string[] AntPropertiesLines = new string[4];
			AntPropertiesLines[0] = "key.store=" + Keystore;
			AntPropertiesLines[1] = "key.alias=" + Alias;
			AntPropertiesLines[2] = "key.store.password=" + KeystorePassword;
			AntPropertiesLines[3] = "key.alias.password=" + ((KeyPassword == "_sameaskeystore_") ? KeystorePassword : KeyPassword);

			// now write out the properties
			File.WriteAllLines(Path.Combine(BuildPath, "ant.properties"), AntPropertiesLines);
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			// we need to strip architecture from any of the output paths
			string BaseSoName = AndroidToolChain.RemoveArchName(InTarget.OutputPaths[0]);

			// make an apk at the end of compiling, so that we can run without packaging (debugger, cook on the fly, etc)
			MakeApk(InTarget.AppName, InTarget.ProjectDirectory, BaseSoName, BuildConfiguration.RelativeEnginePath, bForDistribution: false, CookFlavor: "", bMakeSeparateApks:ShouldMakeSeparateApks(), bIncrementalPackage:true);

			// if we made any non-standard .apk files, the generated debugger settings may be wrong
			if (ShouldMakeSeparateApks() && (InTarget.OutputPaths.Length > 1 || !InTarget.OutputPaths[0].Contains("-armv7-es2")))
			{
				Console.WriteLine("================================================================================================================================");
				Console.WriteLine("Non-default apk(s) have been made: If you are debugging, you will need to manually select one to run in the debugger properties!");
				Console.WriteLine("================================================================================================================================");
			}
			return true;
		}

		public static bool ShouldMakeSeparateApks()
		{
			// @todo android fat binary: Currently, there isn't much utility in merging multiple .so's into a single .apk except for debugging,
			// but we can't properly handle multiple GPU architectures in a single .apk, so we are disabling the feature for now
			// The user will need to manually select the apk to run in their Visual Studio debugger settings (see Override APK in TADP, for instance)
			// If we change this, pay attention to <OverrideAPKPath> in AndroidProjectGenerator
			return true;
			
			// check to see if the project wants separate apks
// 			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
// 			bool bSeparateApks = false;
// 			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSplitIntoSeparateApks", out bSeparateApks);
// 
// 			return bSeparateApks;
		}

		public override bool PrepForUATPackageOrDeploy(string ProjectName, string ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor)
		{
			MakeApk(ProjectName, ProjectDirectory, ExecutablePath, EngineDirectory, bForDistribution, CookFlavor, ShouldMakeSeparateApks(), bIncrementalPackage:false);
			return true;
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Log.TraceInformation(Line.Data);
			}
		}
	}
}
