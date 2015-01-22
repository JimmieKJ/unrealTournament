// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO; 
using System.Text;
using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace UnrealBuildTool
{
    public class HTML5SDKInfo
    {
        static string EmscriptenSettingsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".emscripten");
		static ConfigCacheIni ConfigCache = null;

        static Dictionary<string, string> ReadEmscriptenSettings()
        {
            // Check HTML5ToolChain.cs for duplicate
            if (!System.IO.File.Exists(EmscriptenSettingsPath))
            {
                return new Dictionary<string, string>();
            }

            Dictionary<string, string> Settings = new Dictionary<string, string>();
            System.IO.StreamReader SettingFile = new System.IO.StreamReader(EmscriptenSettingsPath);
            string EMLine = null;
            while ((EMLine = SettingFile.ReadLine()) != null)
            {
                EMLine = EMLine.Split('#')[0];
                string Pattern1 = @"(\w*)\s*=\s*['\[]?([^'\]\r\n]*)['\]]?";
                Regex Rgx = new Regex(Pattern1, RegexOptions.IgnoreCase);
                MatchCollection Matches = Rgx.Matches(EMLine);
                foreach (Match Matched in Matches)
                {
                    if (Matched.Groups.Count == 3 && Matched.Groups[2].ToString() != "")
                    {
                        Settings[Matched.Groups[1].ToString()] = Matched.Groups[2].ToString();
                    }
                }
            }

            return Settings;
        }

		public static bool ParseProjectPath(string[] Arguments, out string ProjectPath)
		{
			string TargetName = null;
			foreach (var Argument in Arguments)
			{
				// Treat any platform names as such
				UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(Argument);
				if (ParsedPlatform == UnrealTargetPlatform.Unknown)
				{
					var ArgUpper = Argument.ToUpperInvariant();
					switch (ArgUpper)
					{
						// Skip any build configurations 
						case "DEBUG":
						case "DEBUGGAME":
						case "DEVELOPMENT":
						case "SHIPPING":
						case "TEST":
							break;
						default:
							// Ignore any args except -project
							if (!ArgUpper.StartsWith("-") || ArgUpper.StartsWith("-PROJECT="))
							{
								if (ArgUpper.StartsWith("-PROJECT="))
								{
									ArgUpper = ArgUpper.Remove(0, 9).Trim();
								}
								// If running Rocket, the PossibleTargetName could contain a path
								TargetName = ArgUpper;

								// If a project file was not specified see if we can find one
								string CheckProjectFile = UProjectInfo.GetProjectForTarget(TargetName);
								if (string.IsNullOrEmpty(CheckProjectFile))
								{
									CheckProjectFile = UProjectInfo.GetProjectFilePath(TargetName);
								}
								if (string.IsNullOrEmpty(CheckProjectFile) == false)
								{
									Log.TraceVerbose("Found project file for {0} - {1}", TargetName, CheckProjectFile);
									string NewProjectFilename = CheckProjectFile;
									if (Path.IsPathRooted(NewProjectFilename) == false)
									{
										NewProjectFilename = Path.GetFullPath(NewProjectFilename);
									}

									NewProjectFilename = NewProjectFilename.Replace("\\", "/");
									ProjectPath = Path.GetDirectoryName(NewProjectFilename);
									return true;
								} 

								if ((TargetName.Contains("/") || TargetName.Contains("\\")) && TargetName.Contains(".UPROJECT"))
								{
									// Parse off the path
									ProjectPath = Path.GetDirectoryName(TargetName);
									return true;
								}
							}
						break;
					}
				}
			}

			ProjectPath = "";
			return false;
		}

		static void EnsureConfigCacheIsReady()
		{
			if (ConfigCache == null)
			{
				var CmdLine = Environment.GetCommandLineArgs();
				string ProjectPath = UnrealBuildTool.GetUProjectPath();
				if (String.IsNullOrEmpty(ProjectPath))
				{ 
					ParseProjectPath(CmdLine, out ProjectPath);
				}
				ConfigCache = new ConfigCacheIni(UnrealTargetPlatform.HTML5, "Engine", ProjectPath);
				// always pick from the Engine the root directory and NOT the staged engine directory. 
				// This breaks parse order for user & game config inis so disabled but pulling from the staged directory may still be a problem 
				//string IniFile = Path.GetFullPath(Path.GetDirectoryName(UnrealBuildTool.GetUBTPath()) + "/../../") + "Config/HTML5/HTML5Engine.ini";
				//ConfigCache.ParseIniFile(IniFile);
			}
		}

        public static string EmscriptenSDKPath()
        {
			EnsureConfigCacheIsReady();
			// Search the ini file for Emscripten root first
			string EMCCPath;
			bool ok = ConfigCache.GetString("HTML5SDKPaths", "Emscripten", out EMCCPath);
			if (ok && System.IO.Directory.Exists(EMCCPath))
				return EMCCPath;

			// Older method used platform name
            string PlatformName = "";
            if (!Utils.IsRunningOnMono)
            {
                PlatformName = "Windows";
            }
            else
            {
                PlatformName = "Mac";
            }
            ok = ConfigCache.GetString("HTML5SDKPaths", PlatformName, out EMCCPath);

            if (ok && System.IO.Directory.Exists(EMCCPath))
                return EMCCPath;

            // try to find SDK Location from env.
            if (Environment.GetEnvironmentVariable("EMSCRIPTEN") != null
                && System.IO.Directory.Exists(Environment.GetEnvironmentVariable("EMSCRIPTEN"))
                )
            {
                return Environment.GetEnvironmentVariable("EMSCRIPTEN");
            }

            return ""; 
        }

        public  static string PythonPath()
        {
			// find Python.
			EnsureConfigCacheIsReady();
			
			// Check the ini first. Check for Python="path"
			string PythonPath;
			bool ok = ConfigCache.GetString("HTML5SDKPaths", "Python", out PythonPath);
			if (ok && System.IO.File.Exists(PythonPath))
				return PythonPath;
           
            var EmscriptenSettings = ReadEmscriptenSettings();

			// check emscripten generated config file. 
            if (EmscriptenSettings.ContainsKey("PYTHON")
                && System.IO.File.Exists(EmscriptenSettings["PYTHON"])
                )
            {
                return EmscriptenSettings["PYTHON"];
            }

            // It might be setup as a env variable. 
            if (Environment.GetEnvironmentVariable("PYTHON") != null
                &&
                System.IO.File.Exists(Environment.GetEnvironmentVariable("PYTHON"))
                )
            {
                return Environment.GetEnvironmentVariable("PYTHON");
            }

            // it might just be on path. 
            ProcessStartInfo startInfo = new ProcessStartInfo();
            Process PythonProcess = new Process();

            startInfo.CreateNoWindow = true;
            startInfo.RedirectStandardOutput = true;
            startInfo.RedirectStandardInput = true;

            startInfo.Arguments = "python";
			startInfo.UseShellExecute = false;

			if (!Utils.IsRunningOnMono) 
			{
				startInfo.FileName = "C:\\Windows\\System32\\where.exe";
			}
			else 
			{
				startInfo.FileName = "whereis";
			}


			PythonProcess.StartInfo = startInfo;
            PythonProcess.Start();
            PythonProcess.WaitForExit();

            string Apps = PythonProcess.StandardOutput.ReadToEnd();
            Apps = Apps.Replace('\r', '\n');
            string[] locations = Apps.Split('\n');

            return locations[0];
        }

		public static string OverriddenSDKVersion()
		{
			EnsureConfigCacheIsReady();

			// Check the ini first. Check for ForceSDKVersion="x.xx.x"
			string ForcedSDKVersion;
			bool ok = ConfigCache.GetString("HTML5SDKPaths", "ForceSDKVersion", out ForcedSDKVersion);
			if (ok)
				return ForcedSDKVersion;

			return "";
		}

        public static string EmscriptenPackager()
        {
            return Path.Combine(EmscriptenSDKPath(), "tools", "file_packager.py"); 
        }

        public static string EmscriptenCompiler()
        {
            return Path.Combine(EmscriptenSDKPath(), "emcc");
        }

        public static bool IsSDKInstalled()
        {
			return File.Exists(GetVersionInfoPath());
        }

        public static bool IsPythonInstalled()
        {
            if (String.IsNullOrEmpty(PythonPath()))
                return false;
            return true;
        }

		public static bool IsSDKVersionOverridden()
		{
			if (String.IsNullOrEmpty(OverriddenSDKVersion()))
				return false;
			return true;
		}

        public static string EmscriptenVersion()
        {
            string VersionInfo = File.ReadAllText(GetVersionInfoPath());
            VersionInfo = VersionInfo.Trim();
            return VersionInfo; 
        }

		static string GetVersionInfoPath()
		{
            string BaseSDKPath = HTML5SDKInfo.EmscriptenSDKPath(); 
            return Path.Combine(BaseSDKPath,"emscripten-version.txt");
		}
    }
}
 