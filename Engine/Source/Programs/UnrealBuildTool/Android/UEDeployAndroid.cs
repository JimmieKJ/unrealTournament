// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Xml.Linq;

namespace UnrealBuildTool
{
	public class UEDeployAndroid : UEBuildDeploy
	{
		/// <summary>
		/// Internal usage for GetApiLevel
		/// </summary>
		private List<string> PossibleApiLevels = null;

		private FileReference ProjectFile;

		public UEDeployAndroid(FileReference InProjectFile)
		{
			ProjectFile = InProjectFile;
		}

		private UnrealPluginLanguage UPL = null;

		public void SetAndroidPluginData(List<string> Architectures, List<string> inPluginExtraData)
		{
			List<string> NDKArches = new List<string>();
			foreach (var Arch in Architectures)
			{
				NDKArches.Add(GetNDKArch(Arch));
			}
			UPL = new UnrealPluginLanguage(ProjectFile, inPluginExtraData, NDKArches, "http://schemas.android.com/apk/res/android", "xmlns:android=\"http://schemas.android.com/apk/res/android\"", UnrealTargetPlatform.Android);
//			APL.SetTrace();
		}

		/// <summary>
		/// Simple function to pipe output asynchronously
		/// </summary>
		private void ParseApiLevel(object Sender, DataReceivedEventArgs Event)
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

		private Dictionary<string, ConfigCacheIni> ConfigCache = null;

		private ConfigCacheIni GetConfigCacheIni(string baseIniName)
		{
			if (ConfigCache == null)
			{
				ConfigCache = new Dictionary<string, ConfigCacheIni>();
			}

			ConfigCacheIni config = null;
			if (!ConfigCache.TryGetValue(baseIniName, out config))
			{
				config = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Android, "Engine", DirectoryReference.FromFile(ProjectFile));
				ConfigCache.Add(baseIniName, config);
			}

			return config;
		}

		private string CachedSDKLevel = null;
		private string GetSdkApiLevel(AndroidToolChain ToolChain)
		{
			if (CachedSDKLevel == null)
			{
				// ask the .ini system for what version to use
				ConfigCacheIni Ini = GetConfigCacheIni("Engine");
				string SDKLevel;
				Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "SDKAPILevel", out SDKLevel);

				// if we want to use whatever version the ndk uses, then use that
				if (SDKLevel == "matchndk")
				{
					SDKLevel = ToolChain.GetNdkApiLevel();
				}

				// run a command and capture output
				if (SDKLevel == "latest")
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
						SDKLevel = ToolChain.GetLargestApiLevel(PossibleApiLevels.ToArray());
					}
					else
					{
						throw new BuildException("Can't make an APK without an API installed (see \"android.bat list targets\")");
					}
				}

				Console.WriteLine("Building Java with SDK API level '{0}'", SDKLevel);
				CachedSDKLevel = SDKLevel;
			}

			return CachedSDKLevel;
		}

		private bool IsVulkanSDKAvailable()
		{
			bool bHaveVulkan = false;

			// First look for VulkanSDK (two possible env variables)
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			if (String.IsNullOrEmpty(VulkanSDKPath))
			{
				VulkanSDKPath = Environment.GetEnvironmentVariable("VK_SDK_PATH");
			}

			// Note: header is the same for all architectures so just use arch-arm
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			string NDKVulkanIncludePath = NDKPath + "/android-24/arch-arm/usr/include/vulkan";

			// Use NDK Vulkan header if discovered, or VulkanSDK if available
			if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
			{
				bHaveVulkan = true;
			}
			else
			if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
			}

			return bHaveVulkan;
		}

		public static string GetOBBVersionNumber(int PackageVersion)
		{
			string VersionString = PackageVersion.ToString("0");
			return VersionString;
		}

		public bool PackageDataInsideApk(bool bDisallowPackagingDataInApk, ConfigCacheIni Ini = null)
		{
			if (bDisallowPackagingDataInApk)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni("Engine");
			}

			// we check this a lot, so make it easy 
			bool bPackageDataInsideApk;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageDataInsideApk", out bPackageDataInsideApk);

			return bPackageDataInsideApk;
		}

		public bool DisableVerifyOBBOnStartUp(ConfigCacheIni Ini = null)
		{
			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni("Engine");
			}

			// we check this a lot, so make it easy 
			bool bDisableVerifyOBBOnStartUp;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableVerifyOBBOnStartUp", out bDisableVerifyOBBOnStartUp);

			return bDisableVerifyOBBOnStartUp;
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


		private static void CopyFileDirectory(string SourceDir, string DestDir, Dictionary<string, string> Replacements)
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

		private static void DeleteDirectory(string InPath, string SubDirectoryToKeep = "")
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

		public string GetUE4JavaSrcPath()
		{
			return Path.Combine("src", "com", "epicgames", "ue4");
		}

		public string GetUE4JavaFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(GetUE4BuildFilePath(EngineDirectory), GetUE4JavaSrcPath()));
		}

		public string GetUE4JavaBuildSettingsFileName(String EngineDirectory)
		{
			return Path.Combine(GetUE4JavaFilePath(EngineDirectory), "JavaBuildSettings.java");
		}

		public string GetUE4JavaDownloadShimFileName(string Directory)
		{
			return Path.Combine(Directory, "DownloadShim.java");
		}

		public string GetUE4TemplateJavaSourceDir(string Directory)
		{
			return Path.Combine(GetUE4BuildFilePath(Directory), "JavaTemplates");
		}

		public string GetUE4TemplateJavaDestination(string Directory, string FileName)
		{
			return Path.Combine(Directory, FileName);
		}

		public string GetUE4JavaOBBDataFileName(string Directory)
		{
			return Path.Combine(Directory, "OBBData.java");
		}

		public class TemplateFile
		{
			public string SourceFile;
			public string DestinationFile;
		}

		private void MakeDirectoryIfRequired(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		public void WriteJavaOBBDataFile(string FileName, string PackageName, List<string> ObbSources)
		{

			Log.TraceInformation("\n==== Writing to OBB data file {0} ====", FileName);

			var Ini = GetConfigCacheIni("Engine");
			int StoreVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);

			string[] obbDataFile = File.Exists(FileName) ? File.ReadAllLines(FileName) : null;

			StringBuilder obbData = new StringBuilder("package " + PackageName + ";\n\n");
			obbData.Append("public class OBBData\n{\n");
			obbData.Append("public static class XAPKFile {\npublic final boolean mIsMain;\npublic final String mFileVersion;\n");
			obbData.Append("public final long mFileSize;\nXAPKFile(boolean isMain, String fileVersion, long fileSize) {\nmIsMain = isMain;\nmFileVersion = fileVersion;\nmFileSize = fileSize;\n");
			obbData.Append("}\n}\n\n");

			// write the data here
			obbData.Append("public static final XAPKFile[] xAPKS = {\n");
			// For each obb file... but we only have one... for now anyway.
			bool first = ObbSources.Count > 1;
			foreach (string ObbSource in ObbSources)
			{
				obbData.Append("new XAPKFile(\ntrue, // true signifies a main file\n");
				obbData.AppendFormat("\"{0}\", // the version of the APK that the file was uploaded against\n", GetOBBVersionNumber(StoreVersion));
				obbData.AppendFormat("{0}L // the length of the file in bytes\n", File.Exists(ObbSource) ? new FileInfo(ObbSource).Length : 0);
				obbData.AppendFormat("){0}\n", first ? "," : "");
				first = false;
			}
			obbData.Append("};\n"); // close off data

			//
			obbData.Append("};\n"); // close class definition off

			if (obbDataFile == null || !obbDataFile.SequenceEqual((obbData.ToString()).Split('\n')))
			{
				MakeDirectoryIfRequired(FileName);
				using (StreamWriter outputFile = new StreamWriter(FileName, false))
				{
					var obbSrc = obbData.ToString().Split('\n');
					foreach (var line in obbSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Log.TraceInformation("\n==== OBB data file up to date so not writing. ====");
			}
		}

		public void WriteJavaDownloadSupportFiles(string ShimFileName, IEnumerable<TemplateFile> TemplateFiles, Dictionary<string, string> replacements)
		{
			// Deal with the Shim first as that is a known target and is easy to deal with
			// If it exists then read it
			string[] DestFileContent = File.Exists(ShimFileName) ? File.ReadAllLines(ShimFileName) : null;

			StringBuilder ShimFileContent = new StringBuilder("package com.epicgames.ue4;\n\n");
			ShimFileContent.AppendFormat("import {0}.OBBDownloaderService;\n", replacements["$$PackageName$$"]);
			ShimFileContent.AppendFormat("import {0}.DownloaderActivity;\n", replacements["$$PackageName$$"]);
			ShimFileContent.Append("\n\npublic class DownloadShim\n{\n");
			ShimFileContent.Append("\tpublic static OBBDownloaderService DownloaderService;\n");
			ShimFileContent.Append("\tpublic static DownloaderActivity DownloadActivity;\n");
			ShimFileContent.Append("\tpublic static Class<DownloaderActivity> GetDownloaderType() { return DownloaderActivity.class; }\n");
			ShimFileContent.Append("}\n");

			Log.TraceInformation("\n==== Writing to shim file {0} ====", ShimFileName);

			// If they aren't the same then dump out the settings
			if (DestFileContent == null || !DestFileContent.SequenceEqual((ShimFileContent.ToString()).Split('\n')))
			{
				MakeDirectoryIfRequired(ShimFileName);
				using (StreamWriter outputFile = new StreamWriter(ShimFileName, false))
				{
					var shimSrc = ShimFileContent.ToString().Split('\n');
					foreach (var line in shimSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Log.TraceInformation("\n==== Shim data file up to date so not writing. ====");
			}

			// Now we move on to the template files
			foreach (var template in TemplateFiles)
			{
				string[] templateSrc = File.ReadAllLines(template.SourceFile);
				string[] templateDest = File.Exists(template.DestinationFile) ? File.ReadAllLines(template.DestinationFile) : null;

				for (int i = 0; i < templateSrc.Length; ++i)
				{
					string srcLine = templateSrc[i];
					bool changed = false;
					foreach (var kvp in replacements)
					{
						if (srcLine.Contains(kvp.Key))
						{
							srcLine = srcLine.Replace(kvp.Key, kvp.Value);
							changed = true;
						}
					}
					if (changed)
					{
						templateSrc[i] = srcLine;
					}
				}

				Log.TraceInformation("\n==== Writing to template target file {0} ====", template.DestinationFile);

				if (templateDest == null || templateSrc.Length != templateDest.Length || !templateSrc.SequenceEqual(templateDest))
				{
					MakeDirectoryIfRequired(template.DestinationFile);
					using (StreamWriter outputFile = new StreamWriter(template.DestinationFile, false))
					{
						foreach (var line in templateSrc)
						{
							outputFile.WriteLine(line);
						}
					}
				}
				else
				{
					Log.TraceInformation("\n==== Template target file up to date so not writing. ====");
				}
			}
		}


		private static string GetNDKArch(string UE4Arch)
		{
			switch (UE4Arch)
			{
				case "-armv7":	return "armeabi-v7a";
                case "-arm64":  return "arm64-v8a";
				case "-x64":	return "x86_64";
				case "-x86":	return "x86";

				default: throw new BuildException("Unknown UE4 architecture {0}", UE4Arch);
			}
		}

		public static string GetUE4Arch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "armeabi-v7a": return "-armv7";
                case "arm64-v8a":   return "-arm64";
                case "x86":         return "-x86";
                case "arm64":       return "-arm64";
				case "x86_64":
				case "x64":			return "-x64";
					
//				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
                // future-proof by returning armv7 for unknown
                default:            return "-armv7";
			}
		}

		private static void StripDebugSymbols(string SourceFileName, string TargetFileName, string UE4Arch)
		{
			// Copy the file and remove read-only if necessary
			File.Copy(SourceFileName, TargetFileName, true);
			FileAttributes Attribs = File.GetAttributes(TargetFileName);
			if (Attribs.HasFlag(FileAttributes.ReadOnly))
			{
				File.SetAttributes(TargetFileName, Attribs & ~FileAttributes.ReadOnly);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = AndroidToolChain.GetStripExecutablePath(UE4Arch);
			StartInfo.Arguments = "--strip-debug \"" + TargetFileName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}

		private static void CopySTL(AndroidToolChain ToolChain, string UE4BuildPath, string UE4Arch, string NDKArch, bool bForDistribution)
		{
			string GccVersion = "4.6";
			if (Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.9")))
			{
				GccVersion = "4.9";
			}
			else if (Directory.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/4.8")))
			{
				GccVersion = "4.8";
			}

			// copy it in!
			string SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/gnu-libstdc++/") + GccVersion + "/libs/" + NDKArch + "/libgnustl_shared.so";
			string FinalSTLSOName = UE4BuildPath + "/libs/" + NDKArch + "/libgnustl_shared.so";

			// check to see if libgnustl_shared.so is newer than last time we copied (or needs stripping for distribution)
			bool bFileExists = File.Exists(FinalSTLSOName);
			TimeSpan Diff = File.GetLastWriteTimeUtc(FinalSTLSOName) - File.GetLastWriteTimeUtc(SourceSTLSOName);
			if (bForDistribution || !bFileExists || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				if (bFileExists)
				{
					File.Delete(FinalSTLSOName);
				}
				Directory.CreateDirectory(Path.GetDirectoryName(FinalSTLSOName));
				if (bForDistribution)
				{
					// Strip debug symbols for distribution builds
					StripDebugSymbols(SourceSTLSOName, FinalSTLSOName, UE4Arch);
				}
				else
				{
					File.Copy(SourceSTLSOName, FinalSTLSOName, true);
				}
			}
		}

		//@TODO: To enable the NVIDIA Gfx Debugger - these paths need to be standardized
		private static void CopyGfxDebugger(string UE4BuildPath, string UE4Arch, string NDKArch)
		{
			/*
			Directory.CreateDirectory(UE4BuildPath + "/libs/" + NDKArch);
			File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvPmApi.Core.so", UE4BuildPath + "/libs/" + NDKArch + "/libNvPmApi.Core.so", true);
			File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvidia_gfx_debugger.so", UE4BuildPath + "/libs/" + NDKArch + "/libNvidia_gfx_debugger.so", true);
			*/
		}

		private static int RunCommandLineProgramAndReturnResult(string WorkingDirectory, string Command, string Params, string OverrideDesc = null, bool bUseShellExecute = false)
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

			return Proc.ExitCode;
		}

		private static void RunCommandLineProgramWithException(string WorkingDirectory, string Command, string Params, string OverrideDesc = null, bool bUseShellExecute = false)
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

		private bool CheckApplicationName(string UE4BuildPath, string ProjectName, out string ApplicationDisplayName)
		{
			string StringsXMLPath = Path.Combine(UE4BuildPath, "res/values/strings.xml");

			ApplicationDisplayName = null;
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ApplicationDisplayName", out ApplicationDisplayName);

			// use project name if display name is left blank
			if (String.IsNullOrWhiteSpace(ApplicationDisplayName))
			{
				ApplicationDisplayName = ProjectName;
			}

			// replace escaped characters (note: changes &# pattern before &, then patches back to allow escaped character codes in the string)
			ApplicationDisplayName = ApplicationDisplayName.Replace("&#", "$@#$").Replace("&", "&amp;").Replace("'", "\\'").Replace("\"", "\\\"").Replace("<", "&lt;").Replace(">", "&gt;").Replace("$@#$", "&#");

			// if it doesn't exist, need to repackage
			if (!File.Exists(StringsXMLPath))
			{
				return true;
			}

			// read it and see if needs to be updated
			string Contents = File.ReadAllText(StringsXMLPath);

			// find the key
			string AppNameTag = "<string name=\"app_name\">";
			int KeyIndex = Contents.IndexOf(AppNameTag);

			// if doesn't exist, need to repackage
			if (KeyIndex < 0)
			{
				return true;
			}

			// get the current value
			KeyIndex += AppNameTag.Length;
			int TagEnd = Contents.IndexOf("</string>", KeyIndex);
			if (TagEnd < 0)
			{
				return true;
			}
			string CurrentApplicationName = Contents.Substring(KeyIndex, TagEnd - KeyIndex);

			// no need to do anything if matches
			if (CurrentApplicationName == ApplicationDisplayName)
			{
				// name matches, no need to force a repackage
				return false;
			}

			// need to repackage
			return true;
		}

		private void UpdateProjectProperties(AndroidToolChain ToolChain, string UE4BuildPath, string ProjectName)
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
			string UpdateCommandLine = "--silent update project --subprojects --name " + ProjectName + " --path . --target " + GetSdkApiLevel(ToolChain);
			foreach (string Lib in LibsToBeAdded)
			{
				string LocalUpdateCommandLine = UpdateCommandLine + " --library " + Lib;

				// make sure each library has a build.xml - --subprojects doesn't create build.xml files, but it will create project.properties
				// and later code needs each lib to have a build.xml
				RunCommandLineProgramWithException(UE4BuildPath, AndroidCommandPath, "--silent update lib-project --path " + Lib + " --target " + GetSdkApiLevel(ToolChain), "");
				RunCommandLineProgramWithException(UE4BuildPath, AndroidCommandPath, LocalUpdateCommandLine, "Updating project.properties, local.properties, and build.xml for " + Path.GetFileName(Lib) + "...");
			}

		}


		private string GetAllBuildSettings(AndroidToolChain ToolChain, string BuildPath, bool bForDistribution, bool bMakeSeparateApks, bool bPackageDataInsideApk, bool bDisableVerifyOBBOnStartUp)
		{
			// make the settings string - this will be char by char compared against last time
			StringBuilder CurrentSettings = new StringBuilder();
			CurrentSettings.AppendLine(string.Format("NDKROOT={0}", Environment.GetEnvironmentVariable("NDKROOT")));
			CurrentSettings.AppendLine(string.Format("ANDROID_HOME={0}", Environment.GetEnvironmentVariable("ANDROID_HOME")));
			CurrentSettings.AppendLine(string.Format("ANT_HOME={0}", Environment.GetEnvironmentVariable("ANT_HOME")));
			CurrentSettings.AppendLine(string.Format("JAVA_HOME={0}", Environment.GetEnvironmentVariable("JAVA_HOME")));
			CurrentSettings.AppendLine(string.Format("SDKVersion={0}", GetSdkApiLevel(ToolChain)));
			CurrentSettings.AppendLine(string.Format("bForDistribution={0}", bForDistribution));
			CurrentSettings.AppendLine(string.Format("bMakeSeparateApks={0}", bMakeSeparateApks));
			CurrentSettings.AppendLine(string.Format("bPackageDataInsideApk={0}", bPackageDataInsideApk));
			CurrentSettings.AppendLine(string.Format("bDisableVerifyOBBOnStartUp={0}", bDisableVerifyOBBOnStartUp));

			// all AndroidRuntimeSettings ini settings in here
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			ConfigCacheIni.IniSection Section = Ini.FindSection("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
			if (Section != null)
			{
				foreach (string Key in Section.Keys)
				{
					List<string> Values = Section[Key];
					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			Section = Ini.FindSection("/Script/AndroidPlatformEditor.AndroidSDKSettings");
			if (Section != null)
			{
				foreach (string Key in Section.Keys)
				{
					List<string> Values = Section[Key];
					foreach (string Value in Values)
					{
						CurrentSettings.AppendLine(string.Format("{0}={1}", Key, Value));
					}
				}
			}

			var Arches = ToolChain.GetAllArchitectures();
			foreach (string Arch in Arches)
			{
				CurrentSettings.AppendFormat("Arch={0}{1}", Arch, Environment.NewLine);
			}

			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();
			foreach (string GPUArch in GPUArchitectures)
			{
				CurrentSettings.AppendFormat("GPUArch={0}{1}", GPUArch, Environment.NewLine);
			}

			return CurrentSettings.ToString();
		}

		private bool CheckDependencies(AndroidToolChain ToolChain, string ProjectName, string ProjectDirectory, string UE4BuildFilesPath, string GameBuildFilesPath, string EngineDirectory, List<string> SettingsFiles,
			string CookFlavor, string OutputPath, string UE4BuildPath, bool bMakeSeparateApks, bool bPackageDataInsideApk)
		{
			var Arches = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

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

					// make sure changed java files will rebuild apk
					InputFiles.AddRange(SettingsFiles);

					// rebuild if .pak files exist for OBB in APK case
					if (bPackageDataInsideApk)
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

		private int ConvertDepthBufferIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "bits16":
					return 16;
				case "bits24":
					return 24;
				case "bits32":
					return 32;
				default:
					return 0;
			}
		}

		private string ConvertOrientationIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "portrait":
					return "portrait";
				case "reverseportrait":
					return "reversePortrait";
				case "sensorportrait":
					return "sensorPortrait";
				case "landscape":
					return "landscape";
				case "reverselandscape":
					return "reverseLandscape";
				case "sensorlandscape":
					return "sensorLandscape";
				case "sensor":
					return "sensor";
				case "fullsensor":
					return "fullSensor";
				default:
					return "landscape";
			}
		}

		private void DetermineScreenOrientationRequirements(out bool bNeedPortrait, out bool bNeedLandscape)
		{
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);

			bNeedLandscape = false;
			bNeedPortrait = false;

			switch (Orientation.ToLower())
			{
				case "portrait":
					bNeedPortrait = true;
					break;
				case "reverseportrait":
					bNeedPortrait = true;
					break;
				case "sensorportrait":
					bNeedPortrait = true;
					break;

				case "landscape":
					bNeedLandscape = true;
					break;
				case "reverselandscape":
					bNeedLandscape = true;
					break;
				case "sensorlandscape":
					bNeedLandscape = true;
					break;

				case "sensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
				case "fullsensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;

				default:
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
			}
		}

		private void PickDownloaderScreenOrientation(string UE4BuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			// Remove unused downloader_progress.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string LayoutPath = UE4BuildPath + "/res/layout-port/downloader_progress.xml";
				if (File.Exists(LayoutPath))
				{
					File.Delete(LayoutPath);
				}
			}
			if (!bNeedLandscape)
			{
				string LayoutPath = UE4BuildPath + "/res/layout-land/downloader_progress.xml";
				if (File.Exists(LayoutPath))
				{
					File.Delete(LayoutPath);
				}
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UE4BuildPath + ResolutionPath + "downloadimagev.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Downloader screen source image {0} not available, downloader screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(PortraitFilename))
					{
						File.Delete(PortraitFilename);
					}
				}

				string LandscapeFilename = UE4BuildPath + ResolutionPath + "downloadimageh.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Downloader screen source image {0} not available, downloader screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(LandscapeFilename))
					{
						File.Delete(LandscapeFilename);
					}
				}
			}
		}

		private void PickSplashScreenOrientation(string UE4BuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);
			bool bPackageForGearVR;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageForGearVR", out bPackageForGearVR);

			//override the parameters if we are not showing a launch image or are packaging for GearVR
			if (bPackageForGearVR || !bShowLaunchImage)
			{
				bNeedPortrait = bNeedLandscape = false;
			}

			// Remove unused styles.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string StylesPath = UE4BuildPath + "/res/values-port/styles.xml";
				if (File.Exists(StylesPath))
				{
					File.Delete(StylesPath);
				}
			}
			if (!bNeedLandscape)
			{
				string StylesPath = UE4BuildPath + "/res/values-land/styles.xml";
				if (File.Exists(StylesPath))
				{
					File.Delete(StylesPath);
				}
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UE4BuildPath + ResolutionPath + "splashscreen_portrait.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Splash screen source image {0} not available, splash screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(PortraitFilename))
					{
						File.Delete(PortraitFilename);
					}
				}

				string LandscapeFilename = UE4BuildPath + ResolutionPath + "splashscreen_landscape.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Log.TraceWarning("Warning: Splash screen source image {0} not available, splash screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					if (File.Exists(LandscapeFilename))
					{
						File.Delete(LandscapeFilename);
					}
				}
			}
		}

		private string GetPackageName(string ProjectName)
		{
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string PackageName;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "PackageName", out PackageName);
			// replace some variables
			PackageName = PackageName.Replace("[PROJECT]", ProjectName);
			PackageName = PackageName.Replace("-", "_");
			return PackageName;
		}

		private string GetPublicKey()
		{
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string PlayLicenseKey = "";
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GooglePlayLicenseKey", out PlayLicenseKey);
			return PlayLicenseKey;
		}



		private string GenerateManifest(AndroidToolChain ToolChain, string ProjectName, bool bIsForDistribution, bool bPackageDataInsideApk, string GameBuildFilesPath, bool bHasOBBFiles, bool bDisableVerifyOBBOnStartUp, string UE4Arch, string GPUArch)
		{
			string Arch = GetNDKArch(UE4Arch);
			int NDKLevelInt = ToolChain.GetNdkApiLevelInt();

			// 64-bit targets must be android-21 or higher
			if (NDKLevelInt < 21)
			{
				if (UE4Arch == "-arm64" || UE4Arch == "-x64")
				{
					NDKLevelInt = 21;
				}
			}

			// ini file to get settings from
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string PackageName = GetPackageName(ProjectName);
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);
			string DepthBufferPreference;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "DepthBufferPreference", out DepthBufferPreference);
			int MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
			int TargetSDKVersion = MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);
			int StoreVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);
			string VersionDisplayName;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);
			bool EnableFullScreen;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bFullScreen", out EnableFullScreen);
			List<string> ExtraManifestNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraManifestNodeTags", out ExtraManifestNodeTags);
			List<string> ExtraApplicationNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationNodeTags", out ExtraApplicationNodeTags);
			List<string> ExtraActivityNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivityNodeTags", out ExtraActivityNodeTags);
			string ExtraActivitySettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivitySettings", out ExtraActivitySettings);
			string ExtraApplicationSettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationSettings", out ExtraApplicationSettings);
			List<string> ExtraPermissions;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraPermissions", out ExtraPermissions);
			bool bPackageForGearVR;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageForGearVR", out bPackageForGearVR);
			bool bSupportsVulkan;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);
			if (bSupportsVulkan)
			{
				bSupportsVulkan = IsVulkanSDKAvailable();
			}
			bool bEnableIAP = false;
			Ini.GetBool("OnlineSubsystemGooglePlay.Store", "bSupportsInAppPurchasing", out bEnableIAP);
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);

			string InstallLocation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "InstallLocation", out InstallLocation);
			switch(InstallLocation.ToLower())
			{
				case "preferexternal":
					InstallLocation = "preferExternal";
					break;
				case "auto":
					InstallLocation = "auto";
					break;
				default:
					InstallLocation = "internalOnly";
					break;
			}

			// fix up the MinSdkVersion
			if (NDKLevelInt > 19)
			{
				if (MinSDKVersion < 21)
				{
					MinSDKVersion = 21;
					Log.TraceInformation("Fixing minSdkVersion; NDK level above 19 requires minSdkVersion of 21 (arch={0})", UE4Arch.Substring(1));
				}
			}

			// disable GearVR if not supported platform (in this case only armv7 for now)
			if (UE4Arch != "-armv7")
			{
				if (bPackageForGearVR)
				{
					Log.TraceInformation("Disabling Package For GearVR for unsupported architecture {0}", UE4Arch);
					bPackageForGearVR = false;
				}
			}

			// disable splash screen for GearVR (for now)
			if (bPackageForGearVR)
			{
				if (bShowLaunchImage)
				{
					Log.TraceInformation("Disabling Show Launch Image for GearVR enabled application");
					bShowLaunchImage = false;
				}
			}

			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			Text.AppendLine("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"");
			Text.AppendLine(string.Format("          package=\"{0}\"", PackageName));
			if (ExtraManifestNodeTags != null)
			{
				foreach (string Line in ExtraManifestNodeTags)
				{
					Text.AppendLine("          " + Line);
				}
			}
			Text.AppendLine(string.Format("          android:installLocation=\"{0}\"", InstallLocation));
			Text.AppendLine(string.Format("          android:versionCode=\"{0}\"", StoreVersion));
			Text.AppendLine(string.Format("          android:versionName=\"{0}\">", VersionDisplayName));

			Text.AppendLine("");

			Text.AppendLine("\t<!-- Application Definition -->");
			Text.AppendLine("\t<application android:label=\"@string/app_name\"");
			Text.AppendLine("\t             android:icon=\"@drawable/icon\"");
			if (ExtraApplicationNodeTags != null)
			{
				foreach (string Line in ExtraApplicationNodeTags)
				{
					Text.AppendLine("\t             " + Line);
				}
			}
			Text.AppendLine("\t             android:hasCode=\"true\">");
			if (bShowLaunchImage)
			{
				// normal application settings
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.SplashActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\"");
				Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
				Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
				Text.AppendLine(string.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(string.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
				Text.AppendLine("\t\t</activity>");
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.GameActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\"");
				Text.AppendLine("\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
			}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.ue4.GameActivity\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.Black.NoTitleBar.Fullscreen\"");
				Text.AppendLine("\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
			}
			Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
			Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
			if (ExtraActivityNodeTags != null)
			{
				foreach (string Line in ExtraActivityNodeTags)
				{
					Text.AppendLine("\t\t          " + Line);
				}
			}
			Text.AppendLine(string.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
			Text.AppendLine("\t\t\t<meta-data android:name=\"android.app.lib_name\" android:value=\"UE4\"/>");
			if (!bShowLaunchImage)
			{
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(string.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
			}
			if (!string.IsNullOrEmpty(ExtraActivitySettings))
			{
				ExtraActivitySettings = ExtraActivitySettings.Replace("\\n", "\n");
				foreach (string Line in ExtraActivitySettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			string ActivityAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestActivityAdditions.txt");
			if (File.Exists(ActivityAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ActivityAdditionsFile))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			Text.AppendLine("\t\t</activity>");

			// For OBB download support
			if (bShowLaunchImage)
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\"");
				Text.AppendLine(string.Format("\t\t          android:screenOrientation=\"{0}\"", ConvertOrientationIniValue(Orientation)));
				Text.AppendLine("\t\t          android:configChanges=\"screenSize|orientation|keyboardHidden|keyboard\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UE4SplashTheme\" />");
			}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\" />");
			}

			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.DepthBufferPreference\" android:value=\"{0}\"/>", ConvertDepthBufferIniValue(DepthBufferPreference)));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bPackageDataInsideApk\" android:value=\"{0}\"/>", bPackageDataInsideApk ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp\" android:value=\"{0}\"/>", (bIsForDistribution && !bDisableVerifyOBBOnStartUp) ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bShouldHideUI\" android:value=\"{0}\"/>", EnableFullScreen ? "true" : "false"));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.ProjectName\" android:value=\"{0}\"/>", ProjectName));
			Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bHasOBBFiles\" android:value=\"{0}\"/>", bHasOBBFiles ? "true" : "false"));
            Text.AppendLine(string.Format("\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.bSupportsVulkan\" android:value=\"{0}\"/>", bSupportsVulkan ? "true" : "false"));
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.games.APP_ID\"");
			Text.AppendLine("\t\t           android:value=\"@string/app_id\" />");
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.version\"");
			Text.AppendLine("\t\t           android:value=\"@integer/google_play_services_version\" />");
			Text.AppendLine("\t\t<activity android:name=\"com.google.android.gms.ads.AdActivity\"");
			Text.AppendLine("\t\t          android:configChanges=\"keyboard|keyboardHidden|orientation|screenLayout|uiMode|screenSize|smallestScreenSize\"/>");
			if (!string.IsNullOrEmpty(ExtraApplicationSettings))
			{
				ExtraApplicationSettings = ExtraApplicationSettings.Replace("\\n", "\n");
				foreach (string Line in ExtraApplicationSettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}
			string ApplicationAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestApplicationAdditions.txt");
			if (File.Exists(ApplicationAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ApplicationAdditionsFile))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}

            // Required for OBB download support
            Text.AppendLine("\t\t<service android:name=\"OBBDownloaderService\" />");
            Text.AppendLine("\t\t<receiver android:name=\"AlarmReceiver\" />");

			Text.AppendLine("\t</application>");

			Text.AppendLine("");
			Text.AppendLine("\t<!-- Requirements -->");

			// check for an override for the requirements section of the manifest
			string RequirementsOverrideFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsOverride.txt");
			if (File.Exists(RequirementsOverrideFile))
			{
				foreach (string Line in File.ReadAllLines(RequirementsOverrideFile))
				{
					Text.AppendLine("\t" + Line);
				}
			}
			else
			{
				// need just the number part of the sdk
				Text.AppendLine(string.Format("\t<uses-sdk android:minSdkVersion=\"{0}\" android:targetSdkVersion=\"{1}\"/>", MinSDKVersion, TargetSDKVersion));
				Text.AppendLine("\t<uses-feature android:glEsVersion=\"" + AndroidToolChain.GetGLESVersionFromGPUArch(GPUArch) + "\" android:required=\"true\" />");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.INTERNET\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.WAKE_LOCK\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.READ_PHONE_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.CHECK_LICENSE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_WIFI_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.MODIFY_AUDIO_SETTINGS\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.GET_ACCOUNTS\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.VIBRATE\"/>");
				//			Text.AppendLine("\t<uses-permission android:name=\"android.permission.DISABLE_KEYGUARD\"/>");

				if (bEnableIAP)
				{
					Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.BILLING\"/>");
				}
				if (ExtraPermissions != null)
				{
					foreach (string Permission in ExtraPermissions)
					{
						Text.AppendLine(string.Format("\t<uses-permission android:name=\"{0}\"/>", Permission));
					}
				}
				string RequirementsAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsAdditions.txt");
				if (File.Exists(RequirementsAdditionsFile))
				{
					foreach (string Line in File.ReadAllLines(RequirementsAdditionsFile))
					{
						Text.AppendLine("\t" + Line);
					}
				}
			}

			Text.AppendLine("</manifest>");

			// allow plugins to modify final manifest HERE
			XDocument XDoc;
			try
			{
				XDoc = XDocument.Parse(Text.ToString());
			}
			catch (Exception e)
			{
				throw new BuildException("AndroidManifest.xml is invalid {0}\n{1}", e, Text.ToString());
			}

			UPL.ProcessPluginNode(Arch, "androidManifestUpdates", "", ref XDoc);
			return XDoc.ToString();
		}

		private string GenerateProguard(string Arch, string EngineSourcePath, string GameBuildFilesPath)
		{
			StringBuilder Text = new StringBuilder();

			string ProguardFile = Path.Combine(EngineSourcePath, "proguard-project.txt");
			if (File.Exists(ProguardFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardFile))
				{
					Text.AppendLine(Line);
				}
			}

			string ProguardAdditionsFile = Path.Combine(GameBuildFilesPath, "ProguardAdditions.txt");
			if (File.Exists(ProguardAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardAdditionsFile))
				{
					Text.AppendLine(Line);
				}
			}

			// add plugin additions
			return UPL.ProcessPluginNode(Arch, "proguardAdditions", Text.ToString());
		}

		private void ValidateGooglePlay(string UE4BuildPath)
		{
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);

			if (!bEnableGooglePlaySupport)
			{
				// do not need to do anything; it is fine
				return;
			}

			string IniAppId;
			bool bInvalidIniAppId = false;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GamesAppID", out IniAppId);

			//validate the value found in the AndroidRuntimeSettings
			Int64 Value;
			if (IniAppId.Length == 0 || !Int64.TryParse(IniAppId, out Value))
			{
				bInvalidIniAppId = true;
			}

			bool bInvalid = false;
			string ReplacementId = "";
			String Filename = Path.Combine(UE4BuildPath, "res", "values", "GooglePlayAppID.xml");
			if (File.Exists(Filename))
			{
				string[] FileContent = File.ReadAllLines(Filename);
				int LineIndex = -1;
				foreach (string Line in FileContent)
				{
					++LineIndex;

					int StartIndex = Line.IndexOf("\"app_id\">");
					if (StartIndex < 0)
						continue;

					StartIndex += 9;
					int EndIndex = Line.IndexOf("</string>");
					if (EndIndex < 0)
						continue;

					string XmlAppId = Line.Substring(StartIndex, EndIndex - StartIndex);

					//validate that the AppId matches the .ini value for the GooglePlay AppId, assuming it's valid
					if (!bInvalidIniAppId &&  IniAppId.CompareTo(XmlAppId) != 0)
					{
						Log.TraceInformation("Replacing Google Play AppID in GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");

						bInvalid = true;
						ReplacementId = IniAppId;
						
					}					
					else if(XmlAppId.Length == 0 || !Int64.TryParse(XmlAppId, out Value))
					{
						Log.TraceWarning("\nWARNING: GooglePlay Games App ID is invalid! Replacing it with \"1\"");

						//write file with something which will fail but not cause an exception if executed
						bInvalid = true;
						ReplacementId = "1";
					}	

					if(bInvalid)
					{
						// remove any read only flags if invalid so it can be replaced
						FileInfo DestFileInfo = new FileInfo(Filename);
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;

						//preserve the rest of the file, just fix up this line
						string NewLine = Line.Replace("\"app_id\">" + XmlAppId + "</string>", "\"app_id\">" + ReplacementId + "</string>");
						FileContent[LineIndex] = NewLine;

						File.WriteAllLines(Filename, FileContent);
					}

					break;
				}
			}
			else
			{
				string NewAppId;
				// if we don't have an appID to use from the config, write file with something which will fail but not cause an exception if executed
				if (bInvalidIniAppId)
				{
					Log.TraceWarning("\nWARNING: Creating GooglePlayAppID.xml using a Google Play AppID of \"1\" because there was no valid AppID in AndroidRuntimeSettings!");
					NewAppId = "1";
				}
				else
				{
					Log.TraceInformation("Creating GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");
					NewAppId = IniAppId;
				}

				File.WriteAllText(Filename, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n\t<string name=\"app_id\">" + NewAppId + "</string>\n</resources>\n");
			}
		}

		private bool FilesAreDifferent(string SourceFilename, string DestFilename)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				throw new BuildException("Can't make an APK without file [{0}]", SourceFilename);
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return true;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return true;
			}

			// validate timestamps
			TimeSpan Diff = DestInfo.LastWriteTimeUtc - SourceInfo.LastWriteTimeUtc;
			if (Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				return true;
			}

			// could check actual bytes just to be sure, but good enough
			return false;
		}

        private bool RequiresOBB(bool bDisallowPackageInAPK, string OBBLocation)
        {
            if (bDisallowPackageInAPK)
            {
                Log.TraceInformation("APK contains data.");
                return false;
            }
            else if (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")))
            {
                Log.TraceInformation("On build machine.");
                return true;
            }
            else
            {
                Log.TraceInformation("Looking for OBB.");
                return File.Exists(OBBLocation);
            }
        }

		private void MakeApk(AndroidToolChain ToolChain, string ProjectName, string ProjectDirectory, string OutputPath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bMakeSeparateApks, bool bIncrementalPackage, bool bDisallowPackagingDataInApk)
		{
			Log.TraceInformation("\n===={0}====PREPARING TO MAKE APK=================================================================", DateTime.Now.ToString());

			// cache some tools paths
			string NDKBuildPath = Environment.ExpandEnvironmentVariables("%NDKROOT%/ndk-build" + (Utils.IsRunningOnMono ? "" : ".cmd"));

			// set up some directory info
			string IntermediateAndroidPath = Path.Combine(ProjectDirectory, "Intermediate/Android/");
			string UE4BuildPath = Path.Combine(IntermediateAndroidPath, "APK");
			string UE4JavaFilePath = Path.Combine(ProjectDirectory, "Build", "Android", GetUE4JavaSrcPath());
			string UE4BuildFilesPath = GetUE4BuildFilePath(EngineDirectory);
			string GameBuildFilesPath = Path.Combine(ProjectDirectory, "Build/Android");

			// Generate Java files
			string PackageName = GetPackageName(ProjectName);
			string TemplateDestinationBase = Path.Combine(ProjectDirectory, "Build", "Android", "src", PackageName.Replace('.', Path.DirectorySeparatorChar));
			MakeDirectoryIfRequired(TemplateDestinationBase);

			// We'll be writing the OBB data into the same location as the download service files
			string UE4OBBDataFileName = GetUE4JavaOBBDataFileName(TemplateDestinationBase);
			string UE4DownloadShimFileName = GetUE4JavaDownloadShimFileName(UE4JavaFilePath);

			// Template generated files           
			string JavaTemplateSourceDir = GetUE4TemplateJavaSourceDir(EngineDirectory);
			var templates = from template in Directory.EnumerateFiles(JavaTemplateSourceDir, "*.template")
							let RealName = Path.GetFileNameWithoutExtension(template)
							select new TemplateFile { SourceFile = template, DestinationFile = GetUE4TemplateJavaDestination(TemplateDestinationBase, RealName) };

			// Generate the OBB and Shim files here
			string ObbFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".obb";

			// This is kind of a small hack to get around a rewrite problem
			// We need to make sure the file is there but if the OBB file doesn't exist then we don't want to replace it
			if (File.Exists(ObbFileLocation) || !File.Exists(UE4OBBDataFileName))
			{
				WriteJavaOBBDataFile(UE4OBBDataFileName, PackageName, new List<string> { ObbFileLocation });
			}

			// Make sure any existing proguard file in project is NOT used (back it up)
			string ProjectBuildProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.txt");
			if (File.Exists(ProjectBuildProguardFile))
			{
				string ProjectBackupProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.backup");
				File.Move(ProjectBuildProguardFile, ProjectBackupProguardFile);
			}

			WriteJavaDownloadSupportFiles(UE4DownloadShimFileName, templates, new Dictionary<string, string>{
                { "$$GameName$$", ProjectName },
                { "$$PublicKey$$", GetPublicKey() }, 
                { "$$PackageName$$",PackageName }
            });

			// Sometimes old files get left behind if things change, so we'll do a clean up pass
			{
				string CleanUpBaseDir = Path.Combine(ProjectDirectory, "Build", "Android", "src");
				var files = Directory.EnumerateFiles(CleanUpBaseDir, "*.java", SearchOption.AllDirectories);

				Log.TraceInformation("Cleaning up files based on template dir {0}", TemplateDestinationBase);

				// Make a set of files that are okay to clean up
				var cleanFiles = new HashSet<string>();
				cleanFiles.Add("OBBData.java");
				foreach (var template in templates)
				{
					cleanFiles.Add(Path.GetFileName(template.DestinationFile));
				}

				foreach (var filename in files)
				{
					if (filename == UE4DownloadShimFileName)  // we always need the shim, and it'll get rewritten if needed anyway
						continue;

					string filePath = Path.GetDirectoryName(filename);  // grab the file's path
					if (filePath != TemplateDestinationBase)             // and check to make sure it isn't the same as the Template directory we calculated earlier
					{
						// Only delete the files in the cleanup set
						if (!cleanFiles.Contains(Path.GetFileName(filename)))
							continue;

						Log.TraceInformation("Cleaning up file {0} with path {1}", filename, filePath);
						File.Delete(filename);

						// Check to see if this file also exists in our target destination, and if so nuke it too
						string DestFilename = Path.Combine(UE4BuildPath, Utils.MakePathRelativeTo(filePath, UE4BuildFilesPath));
						if (File.Exists(filename))
						{
							File.Delete(filename);
						}
					}
				}

				// Directory clean up code
				var directories = Directory.EnumerateDirectories(CleanUpBaseDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
				foreach (var directory in directories)
				{
					if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
					{
						Log.TraceInformation("Cleaning Directory {0} as empty.", directory);
						Directory.Delete(directory, true);
					}
				};


			}


			// cache if we want data in the Apk
			bool bPackageDataInsideApk = PackageDataInsideApk(bDisallowPackagingDataInApk);
			bool bDisableVerifyOBBOnStartUp = DisableVerifyOBBOnStartUp();

			// check to see if any "meta information" is newer than last time we build
			string CurrentBuildSettings = GetAllBuildSettings(ToolChain, UE4BuildPath, bForDistribution, bMakeSeparateApks, bPackageDataInsideApk, bDisableVerifyOBBOnStartUp);
			string BuildSettingsCacheFile = Path.Combine(UE4BuildPath, "UEBuildSettings.txt");

			// do we match previous build settings?
			bool bBuildSettingsMatch = true;

			// get application name and whether it changed, needing to force repackage
			string ApplicationDisplayName;
			if (CheckApplicationName(UE4BuildPath, ProjectName, out ApplicationDisplayName))
			{
				bBuildSettingsMatch = false;
				Log.TraceInformation("Application display name is different than last build, forcing repackage.");
			}

			// if the manifest matches, look at other settings stored in a file
			if (bBuildSettingsMatch)
			{
				if (File.Exists(BuildSettingsCacheFile))
				{
					string PreviousBuildSettings = File.ReadAllText(BuildSettingsCacheFile);
					if (PreviousBuildSettings != CurrentBuildSettings)
					{
						bBuildSettingsMatch = false;
						Log.TraceInformation("Previous .apk file(s) were made with different build settings, forcing repackage.");
					}
				}
			}

			// only check input dependencies if the build settings already match
			if (bBuildSettingsMatch)
			{
				// check if so's are up to date against various inputs
				var JavaFiles = new List<string>{
                                                    UE4OBBDataFileName,
                                                    UE4DownloadShimFileName
                                                };
				// Add the generated files too
				JavaFiles.AddRange(from t in templates select t.SourceFile);
				JavaFiles.AddRange(from t in templates select t.DestinationFile);

				bBuildSettingsMatch = CheckDependencies(ToolChain, ProjectName, ProjectDirectory, UE4BuildFilesPath, GameBuildFilesPath,
					EngineDirectory, JavaFiles, CookFlavor, OutputPath, UE4BuildPath, bMakeSeparateApks, bPackageDataInsideApk);

			}

			var Arches = ToolChain.GetAllArchitectures();
			var GPUArchitectures = ToolChain.GetAllGPUArchitectures();

			// Initialize APL contexts for each architecture enabled
			List<string> NDKArches = new List<string>();
			foreach (var Arch in Arches)
			{
				string NDKArch = GetNDKArch(Arch);
				if (!NDKArches.Contains(NDKArch))
				{
					NDKArches.Add(NDKArch);
				}
			}
			UPL.Init(NDKArches, bForDistribution, EngineDirectory, UE4BuildPath);

			IEnumerable<Tuple<string, string, string>> BuildList = null;

			if (!bBuildSettingsMatch)
			{
				BuildList = from Arch in Arches
							from GPUArch in GPUArchitectures
							let manifest = GenerateManifest(ToolChain, ProjectName, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, RequiresOBB(bDisallowPackagingDataInApk, ObbFileLocation), bDisableVerifyOBBOnStartUp, Arch, GPUArch)
							select Tuple.Create(Arch, GPUArch, manifest);
			}
			else
			{
				BuildList = from Arch in Arches
							from GPUArch in GPUArchitectures
							let manifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_" + GPUArch + "_AndroidManifest.xml")
							let manifest = GenerateManifest(ToolChain, ProjectName, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, RequiresOBB(bDisallowPackagingDataInApk, ObbFileLocation), bDisableVerifyOBBOnStartUp, Arch, GPUArch)
							let OldManifest = File.Exists(manifestFile) ? File.ReadAllText(manifestFile) : ""
							where manifest != OldManifest
							select Tuple.Create(Arch, GPUArch, manifest);
			}

			// Now we have to spin over all the arch/gpu combinations to make sure they all match
			int BuildListComboTotal = BuildList.Count();
			if (BuildListComboTotal == 0)
			{
				Log.TraceInformation("Output .apk file(s) are up to date (dependencies and build settings are up to date)");
				return;
			}


			// Once for all arches code:

			// make up a dictionary of strings to replace in xml files (strings.xml)
			Dictionary<string, string> Replacements = new Dictionary<string, string>();
			Replacements.Add("${EXECUTABLE_NAME}", ApplicationDisplayName);

			if (!bIncrementalPackage)
			{
				// Wipe the Intermediate/Build/APK directory first, except for dexedLibs, because Google Services takes FOREVER to predex, and it almost never changes
				// so allow the ANT checking to win here - if this grows a bit with extra libs, it's fine, it _should_ only pull in dexedLibs it needs
				Log.TraceInformation("Performing complete package - wiping {0}, except for predexedLibs", UE4BuildPath);
				DeleteDirectory(UE4BuildPath, "dexedLibs");
			}

			// If we are packaging for Amazon then we need to copy the  file to the correct location
			Log.TraceInformation("bPackageDataInsideApk = {0}", bPackageDataInsideApk);
			if (bPackageDataInsideApk)
			{
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
			else // try to remove the file it we aren't packaging inside the APK
			{
				string ObbFileDestination = UE4BuildPath + "/assets";
				var DestFileName = Path.Combine(ObbFileDestination, "main.obb.png");
				if (File.Exists(DestFileName))
				{
					File.Delete(DestFileName);
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

			//Extract AAR and Jar files with dependencies
			ExtractAARAndJARFiles(EngineDirectory, UE4BuildPath, NDKArches);

			//Now validate GooglePlay app_id if enabled
			ValidateGooglePlay(UE4BuildPath);

			//determine which orientation requirements this app has
			bool bNeedLandscape = false;
			bool bNeedPortrait = false;
			DetermineScreenOrientationRequirements(out bNeedPortrait, out bNeedLandscape);

			//Now keep the splash screen images matching orientation requested
			PickSplashScreenOrientation(UE4BuildPath, bNeedPortrait, bNeedLandscape);

			//Similarly, keep only the downloader screen image matching the orientation requested
			PickDownloaderScreenOrientation(UE4BuildPath, bNeedPortrait, bNeedLandscape);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);

			///////////////
			// in case the game had an AndroidManifest.xml file, we overwrite it now with the generated one
			//File.WriteAllText(ManifestFile, NewManifest);
			///////////////

			Log.TraceInformation("\n===={0}====PREPARING NATIVE CODE=================================================================", DateTime.Now.ToString());
			bool HasNDKPath = File.Exists(NDKBuildPath);

			// get Ant verbosity level
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string AntVerbosity;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AntVerbosity", out AntVerbosity);

			foreach (var build in BuildList)
			{
				string Arch = build.Item1;
				string GPUArchitecture = build.Item2;
				string Manifest = build.Item3;
				string NDKArch = GetNDKArch(Arch);

				string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch, GPUArchitecture);
				// if the source binary was UE4Game, replace it with the new project name, when re-packaging a binary only build
				string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UE4Game", ProjectName);
				string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

				// As we are always making seperate APKs we need to put the architecture into the name
				DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArchitecture);

				// Write the manifest to the correct locations (cache and real)
				String ManifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_" + GPUArchitecture + "_AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);
				ManifestFile = Path.Combine(UE4BuildPath, "AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);

				// copy prebuild plugin files
				UPL.ProcessPluginNode(NDKArch, "prebuildCopies", "");

				// update metadata files (like project.properties, build.xml) if we are missing a build.xml or if we just overwrote project.properties with a bad version in it (from game/engine dir)
				UpdateProjectProperties(ToolChain, UE4BuildPath, ProjectName);

				// update GameActivity.java if out of date
				UpdateGameActivity(Arch, NDKArch, EngineDirectory, UE4BuildPath);

				// Copy the generated .so file from the binaries directory to the jni folder
				if (!File.Exists(SourceSOName))
				{
					throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
				}
				if (!Directory.Exists(UE4BuildPath + "/jni"))
				{
					throw new BuildException("Can't make an APK without the jni directory [{0}/jni]", UE4BuildFilesPath);
				}

				String FinalSOName;

				if (HasNDKPath)
				{
					string LibDir = UE4BuildPath + "/jni/" + GetNDKArch(Arch);
					FinalSOName = LibDir + "/libUE4.so";

					// check to see if libUE4.so needs to be copied
					if (BuildListComboTotal > 1 || FilesAreDifferent(SourceSOName, FinalSOName))
					{
						Log.TraceInformation("\nCopying new .so {0} file to jni folder...", SourceSOName);
						Directory.CreateDirectory(LibDir);
						// copy the binary to the standard .so location
						File.Copy(SourceSOName, FinalSOName, true);
					}
				}
				else
				{
					// if no NDK, we don't need any of the debugger stuff, so we just copy the .so to where it will end up
					FinalSOName = UE4BuildPath + "/libs/" + GetNDKArch(Arch) + "/libUE4.so";

					// check to see if libUE4.so needs to be copied
					if (BuildListComboTotal > 1 || FilesAreDifferent(SourceSOName, FinalSOName))
					{
						Log.TraceInformation("\nCopying .so {0} file to jni folder...", SourceSOName);
						Directory.CreateDirectory(Path.GetDirectoryName(FinalSOName));
						File.Copy(SourceSOName, FinalSOName, true);
					}
				}

				// remove any read only flags
				FileInfo DestFileInfo = new FileInfo(FinalSOName);
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				File.SetLastWriteTimeUtc(FinalSOName, File.GetLastWriteTimeUtc(SourceSOName));

				// if we need to run ndk-build, do it now
				if (HasNDKPath)
				{
					string LibSOName = UE4BuildPath + "/libs/" + GetNDKArch(Arch) + "/libUE4.so";
					// always delete libs up to this point so fat binaries and incremental builds work together (otherwise we might end up with multiple
					// so files in an apk that doesn't want them)
					// note that we don't want to delete all libs, just the ones we copied
					TimeSpan Diff = File.GetLastWriteTimeUtc(LibSOName) - File.GetLastWriteTimeUtc(FinalSOName);
					if (!File.Exists(LibSOName) || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
					{
						foreach (string Lib in Directory.EnumerateFiles(UE4BuildPath + "/libs", "libUE4*.so", SearchOption.AllDirectories))
						{
							File.Delete(Lib);
						}

						string CommandLine = "APP_ABI=\"" + GetNDKArch(Arch) + " " + "\"";
						if (!bForDistribution)
						{
							CommandLine += " NDK_DEBUG=1";
						}
						RunCommandLineProgramWithException(UE4BuildPath, NDKBuildPath, CommandLine, "Preparing native code for debugging...", true);

						File.SetLastWriteTimeUtc(LibSOName, File.GetLastWriteTimeUtc(FinalSOName));
					}
				}

				// after ndk-build is called, we can now copy in the stl .so (ndk-build deletes old files)
				// copy libgnustl_shared.so to library (use 4.8 if possible, otherwise 4.6)
				CopySTL(ToolChain, UE4BuildPath, Arch, NDKArch, bForDistribution);
				CopyGfxDebugger(UE4BuildPath, Arch, NDKArch);

				// copy postbuild plugin files
				UPL.ProcessPluginNode(NDKArch, "resourceCopies", "");

				Log.TraceInformation("\n===={0}====PERFORMING FINAL APK PACKAGE OPERATION================================================", DateTime.Now.ToString());

                string AntBuildType = "debug";
                string AntOutputSuffix = "-debug";
                if (bForDistribution)
                {
					// Generate the Proguard file contents and write it
					string ProguardContents = GenerateProguard(NDKArch, UE4BuildFilesPath, GameBuildFilesPath);
					string ProguardFilename = Path.Combine(UE4BuildPath, "proguard-project.txt");
					if (File.Exists(ProguardFilename))
					{
						File.Delete(ProguardFilename);
					}
					File.WriteAllText(ProguardFilename, ProguardContents);

					// this will write out ant.properties with info needed to sign a distribution build
					PrepareToSignApk(UE4BuildPath);
					AntBuildType = "release";
					AntOutputSuffix = "-release";
				}

				// Use ant to build the .apk file
				string ShellExecutable = Utils.IsRunningOnMono ? "/bin/sh" : "cmd.exe";
				string ShellParametersBegin = Utils.IsRunningOnMono ? "-c '" : "/c ";
				string ShellParametersEnd = Utils.IsRunningOnMono ? "'" : "";
				switch (AntVerbosity.ToLower())
				{
					default:
					case "quiet":
						if (RunCommandLineProgramAndReturnResult(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" -quiet " + AntBuildType + ShellParametersEnd, "Making .apk with Ant... (note: it's safe to ignore javac obsolete warnings)") != 0)
						{
							RunCommandLineProgramAndReturnResult(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" " + AntBuildType + ShellParametersEnd, "Making .apk with Ant again to show errors");
						}
						break;

					case "normal":
						RunCommandLineProgramAndReturnResult(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" " + AntBuildType + ShellParametersEnd, "Making .apk with Ant again to show errors");
						break;

					case "verbose":
						RunCommandLineProgramAndReturnResult(UE4BuildPath, ShellExecutable, ShellParametersBegin + "\"" + GetAntPath() + "\" -verbose " + AntBuildType + ShellParametersEnd, "Making .apk with Ant again to show errors");
						break;
				}

				// make sure destination exists
				Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

				// now copy to the final location
				File.Copy(UE4BuildPath + "/bin/" + ProjectName + AntOutputSuffix + ".apk", DestApkName, true);

			}

		}

		private void PrepareToSignApk(string BuildPath)
		{
			// ini file to get settings from
			ConfigCacheIni Ini = GetConfigCacheIni("Engine");
			string KeyAlias, KeyStore, KeyStorePassword, KeyPassword;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

			if (string.IsNullOrEmpty(KeyAlias) || string.IsNullOrEmpty(KeyStore) || string.IsNullOrEmpty(KeyStorePassword))
			{
				throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Andriod tab of Project Settings");
			}

			string[] AntPropertiesLines = new string[4];
			AntPropertiesLines[0] = "key.store=" + KeyStore;
			AntPropertiesLines[1] = "key.alias=" + KeyAlias;
			AntPropertiesLines[2] = "key.store.password=" + KeyStorePassword;
			AntPropertiesLines[3] = "key.alias.password=" + ((string.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_") ? KeyStorePassword : KeyPassword);

			// now write out the properties
			File.WriteAllLines(Path.Combine(BuildPath, "ant.properties"), AntPropertiesLines);
		}

		private List<string> CollectPluginDataPaths(TargetReceipt Receipt)
		{
			List<string> PluginExtras = new List<string>();
			if (Receipt == null)
			{
				Log.TraceInformation("Receipt is NULL");
				return PluginExtras;
			}

			// collect plugin extra data paths from target receipt
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "AndroidPlugin");
			foreach (var Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					Log.TraceInformation("AndroidPlugin: {0}", PluginPath);
				}
			}
			return PluginExtras;
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			//Log.TraceInformation("$$$$$$$$$$$$$$ PrepTargetForDeployment $$$$$$$$$$$$$$$$$");
			AndroidToolChain ToolChain = new AndroidToolChain(InTarget.ProjectFile); 

			// we need to strip architecture from any of the output paths
			string BaseSoName = ToolChain.RemoveArchName(InTarget.OutputPaths[0].FullName);

			// get the receipt
			UnrealTargetPlatform Platform = InTarget.Platform;
			UnrealTargetConfiguration Configuration = InTarget.Configuration;
			string ProjectBaseName = Path.GetFileName(BaseSoName).Replace("-" + Platform, "").Replace("-" + Configuration, "").Replace(".so", "");
			string ReceiptFilename = TargetReceipt.GetDefaultPath(InTarget.ProjectDirectory.FullName, ProjectBaseName, Platform, Configuration, "");
			Log.TraceInformation("Receipt Filename: {0}", ReceiptFilename);
			SetAndroidPluginData(ToolChain.GetAllArchitectures(), CollectPluginDataPaths(TargetReceipt.Read(ReceiptFilename)));

			// make an apk at the end of compiling, so that we can run without packaging (debugger, cook on the fly, etc)
			MakeApk(ToolChain, InTarget.AppName, InTarget.ProjectDirectory.FullName, BaseSoName, BuildConfiguration.RelativeEnginePath, bForDistribution: false, CookFlavor: "",
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: true, bDisallowPackagingDataInApk: false);

			// if we made any non-standard .apk files, the generated debugger settings may be wrong
			if (ShouldMakeSeparateApks() && (InTarget.OutputPaths.Count > 1 || !InTarget.OutputPaths[0].FullName.Contains("-armv7-es2")))
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
			// 			ConfigCacheIni Ini = nGetConfigCacheIni("Engine");
			// 			bool bSeparateApks = false;
			// 			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSplitIntoSeparateApks", out bSeparateApks);
			// 
			// 			return bSeparateApks;
		}

		public override bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, string ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			//Log.TraceInformation("$$$$$$$$$$$$$$ PrepForUATPackageOrDeploy $$$$$$$$$$$$$$$$$");

			// note that we cannot allow the data packaged into the APK if we are doing something like Launch On that will not make an obb
			// file and instead pushes files directly via deploy
			AndroidToolChain ToolChain = new AndroidToolChain(ProjectFile);
			MakeApk(ToolChain, ProjectName, ProjectDirectory, ExecutablePath, EngineDirectory, bForDistribution: bForDistribution, CookFlavor: CookFlavor,
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: false, bDisallowPackagingDataInApk: bIsDataDeploy);
			return true;
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Log.TraceInformation(Line.Data);
			}
		}

		private void UpdateGameActivity(string UE4Arch, string NDKArch, string EngineDir, string UE4BuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "Android", "Java", "src", "com", "epicgames", "ue4", "GameActivity.java");
			string DestFilename = Path.Combine(UE4BuildPath, "src", "com", "epicgames", "ue4", "GameActivity.java");

			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${gameActivityImportAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityImportAdditions", "")},
				{ "//$${gameActivityClassAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityClassAdditions", "")},
				{ "//$${gameActivityReadMetadataAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityReadMetadataAdditions", "")},
				{ "//$${gameActivityOnCreateAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnCreateAdditions", "")},
				{ "//$${gameActivityOnDestroyAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnDestroyAdditions", "")},
				{ "//$${gameActivityOnStartAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStartAdditions", "")},
				{ "//$${gameActivityOnStopAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStopAdditions", "")},
				{ "//$${gameActivityOnPauseAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnPauseAdditions", "")},
				{ "//$${gameActivityOnResumeAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnResumeAdditions", "")},
				{ "//$${gameActivityOnActivityResultAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnActivityResultAdditions", "")},
				{ "//$${soLoadLibrary}$$", UPL.ProcessPluginNode(NDKArch, "soLoadLibrary", "")}
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : null;

			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (var KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
				}
			}

			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
                Log.TraceInformation("\n==== Writing new GameActivity.java file to {0} ====", DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private void ExtractAARAndJARFiles(string EngineDir, string UE4BuildPath, List<string> NDKArches)
		{
			AndroidAARHandler AARHandler = new AndroidAARHandler();
			string ImportList = "";

			// Get some common paths
			string AndroidHome = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%").TrimEnd('/', '\\');
			EngineDir = EngineDir.TrimEnd('/', '\\');

			// Add the AARs from the default aar-imports.txt
			// format: Package,Name,Version
			string ImportsFile = Path.Combine(UE4BuildPath, "aar-imports.txt");
			if (File.Exists(ImportsFile))
			{
				ImportList = File.ReadAllText(ImportsFile);
			}

			// Run the UPL imports section for each architecture and add any new imports (duplicates will be removed)
			foreach (string NDKArch in NDKArches)
			{
				ImportList = UPL.ProcessPluginNode(NDKArch, "AARImports", ImportList);
			}

			// Add the final list of imports and get dependencies
			foreach (string Line in ImportList.Split('\n'))
			{
				string Trimmed = Line.Trim(' ', '\r');

				if (Trimmed.StartsWith("repository "))
				{
					string DirectoryPath = Trimmed.Substring(11).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepository(DirectoryPath);
				}
				else if (Trimmed.StartsWith("repositories "))
				{
					string DirectoryPath = Trimmed.Substring(13).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepositories(DirectoryPath, "m2repository");
				}
				else
				{
					string[] Sections = Trimmed.Split(',');
					if (Sections.Length == 3)
					{
						string PackageName = Sections[0].Trim(' ');
						string BaseName = Sections[1].Trim(' ');
						string Version = Sections[2].Trim(' ');
						Log.TraceInformation("AARImports: {0}, {1}, {2}", PackageName, BaseName, Version);
						AARHandler.AddNewAAR(PackageName, BaseName, Version);
					}
				}
			}

			// Finally, extract the AARs and copy the JARs
			AARHandler.ExtractAARs(UE4BuildPath);
			AARHandler.CopyJARs(UE4BuildPath);
		}
    }
}
