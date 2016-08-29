// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
		static string SDKVersion = "1.35.0";
		static string NODE_VER = "4.1.1_64bit";
		static string LLVM_VER = "e1.35.0_64bit";
		static string PYTHON_VER = "2.7.5.3_64bit";

		// --------------------------------------------------
		// --------------------------------------------------
		static string SDKBase { get { return Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, "Extras", "ThirdPartyNotUE", "emsdk")); } }
		static public string EMSCRIPTEN_ROOT { get { return Path.Combine(SDKBase, "emscripten", SDKVersion); } }
		// --------------------------------------------------
		// --------------------------------------------------
		static string CURRENT_PLATFORM
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return "Win64";
					case UnrealTargetPlatform.Mac:
						return "Mac";
					default:
						return "error_unknown_platform";
				}
			}
		}
		static string PLATFORM_EXE
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return ".exe";
					case UnrealTargetPlatform.Mac:
						return "";
					default:
						return "error_unknown_platform";
				}
			}
		}
		static string NODE_JS { get { return Path.Combine(SDKBase, CURRENT_PLATFORM, "node", NODE_VER, "bin", "node" + PLATFORM_EXE); } }
		static string LLVM_ROOT { get { return Path.Combine(SDKBase, CURRENT_PLATFORM, "clang", LLVM_VER); } }
		static string PYTHON
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return Path.Combine(SDKBase, "Win64", "python", PYTHON_VER, "python.exe");
					case UnrealTargetPlatform.Mac: // python is default installed on mac.
						return "/usr/bin/python";
					default:
						return "error_unknown_platform";
				}
			}
		}
		// --------------------------------------------------
		// --------------------------------------------------
		static string HTML5Intermediatory
		{
			get
			{
				string HTML5IntermediatoryPath = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.BaseIntermediateFolder, "HTML5"));
				if (!Directory.Exists(HTML5IntermediatoryPath))
				{
					Directory.CreateDirectory(HTML5IntermediatoryPath);
				}
				return HTML5IntermediatoryPath;
			}
		}
		static public string DOT_EMSCRIPTEN { get { return Path.Combine(HTML5Intermediatory, ".emscripten"); } }
		static public string EMSCRIPTEN_CACHE { get { return Path.Combine(HTML5Intermediatory, "EmscriptenCache"); ; } }

		public static string SetupEmscriptenTemp()
		{
			string TempPath = Path.Combine(HTML5Intermediatory, "EmscriptenTemp");
			try
			{
				if (Directory.Exists(TempPath))
				{
					Directory.Delete(TempPath, true);
				}

				Directory.CreateDirectory(TempPath);
			}
			catch (Exception Ex)
			{
				Log.TraceErrorOnce(" Recreation of Emscripten Temp folder failed because of " + Ex.ToString());
			}

			return TempPath;
		}

		static string PLATFORM_USER_HOME
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return "USERPROFILE";
					case UnrealTargetPlatform.Mac:
						return "HOME";
					default:
						return "error_unknown_platform";
				}
			}
		}

		public static string SetUpEmscriptenConfigFile()
		{
			// the following are for diff checks for file timestamps
			string SaveDotEmscripten = DOT_EMSCRIPTEN + ".save";
			if (File.Exists(SaveDotEmscripten))
			{
				File.Delete(SaveDotEmscripten);
			}
			string config_old = "";

			// make a fresh .emscripten resource file
			if (File.Exists(DOT_EMSCRIPTEN))
			{
				config_old = File.ReadAllText(DOT_EMSCRIPTEN);
				File.Move(DOT_EMSCRIPTEN, SaveDotEmscripten);
			}

			// the best way to generate .emscripten resource file,
			// is to run "emcc -v" (show version info) without an existing one
			// --------------------------------------------------
			// save a few things
			string PATH_SAVE = Environment.GetEnvironmentVariable("PATH");
			string HOME_SAVE = Environment.GetEnvironmentVariable(PLATFORM_USER_HOME);
			// warm up the .emscripten resource file
			string NODE_ROOT = Path.GetDirectoryName(NODE_JS);
			string PYTHON_ROOT = Path.GetDirectoryName(PYTHON);
			Environment.SetEnvironmentVariable("PATH", NODE_ROOT + Path.PathSeparator + LLVM_ROOT + Path.PathSeparator + PYTHON_ROOT + Path.PathSeparator + EMSCRIPTEN_ROOT + Path.PathSeparator + PATH_SAVE);
			Environment.SetEnvironmentVariable(PLATFORM_USER_HOME, HTML5Intermediatory);
			// --------------------------------------------------
				string cmd = "\"" + Path.Combine(EMSCRIPTEN_ROOT, "emcc") + "\"";
				ProcessStartInfo processInfo = new ProcessStartInfo(PYTHON, cmd + " -v");
				processInfo.CreateNoWindow = true;
				processInfo.UseShellExecute = false;
// jic output dump is needed...
//				processInfo.RedirectStandardError = true;
//				processInfo.RedirectStandardOutput = true;
				Process process = Process.Start(processInfo);
//				process.OutputDataReceived += (object sender, DataReceivedEventArgs e) => Console.WriteLine("output>>" + e.Data);
//				process.BeginOutputReadLine();
//				process.ErrorDataReceived += (object sender, DataReceivedEventArgs e) => Console.WriteLine("error>>" + e.Data);
//				process.BeginErrorReadLine();
				process.WaitForExit();
//				Console.WriteLine("ExitCode: {0}", process.ExitCode);
				process.Close();
				// uncomment OPTIMIZER (GUBP on build machines needs this)
				// and PYTHON (reduce warnings on EMCC_DEBUG=1)
				string pyth = Regex.Replace(PYTHON, @"\\", @"\\");
				string optz = Regex.Replace(Path.Combine(LLVM_ROOT, "optimizer") + PLATFORM_EXE, @"\\", @"\\");
				string txt = Regex.Replace(
							Regex.Replace(File.ReadAllText(DOT_EMSCRIPTEN), "#(PYTHON).*", "$1 = '" + pyth + "'"),
							"# (EMSCRIPTEN_NATIVE_OPTIMIZER).*", "$1 = '" + optz + "'");
				File.WriteAllText(DOT_EMSCRIPTEN, txt);
			// --------------------------------------------------
			if (File.Exists(SaveDotEmscripten))
			{
				if ( config_old.Equals(txt, StringComparison.Ordinal) )
				{	// preserve file timestamp -- otherwise, emscripten "system libs" will always be recompiled
					File.Delete(DOT_EMSCRIPTEN);
					File.Move(SaveDotEmscripten, DOT_EMSCRIPTEN);
				}
				else
				{
					File.Delete(SaveDotEmscripten);
				}
			}
			// --------------------------------------------------
			// --------------------------------------------------
			// restore a few things
			Environment.SetEnvironmentVariable(PLATFORM_USER_HOME, HOME_SAVE);
			Environment.SetEnvironmentVariable("PATH", PATH_SAVE);
			// --------------------------------------------------

			return DOT_EMSCRIPTEN;
		}

		public static string EmscriptenVersion()
		{
			return SDKVersion;
		}

		public static string EmscriptenPackager()
		{
			return Path.Combine(EMSCRIPTEN_ROOT, "tools", "file_packager.py");
		}

		public static string EmscriptenCompiler()
		{
			return "\"" + Path.Combine(EMSCRIPTEN_ROOT, "emcc") + "\"";
		}

		public static string Python()
		{
			return PYTHON;
		}

		public static bool IsSDKInstalled()
		{
			return Directory.Exists(EMSCRIPTEN_ROOT) && File.Exists(NODE_JS) && Directory.Exists(LLVM_ROOT) && File.Exists(PYTHON);
		}

		// this script is used at:
		// HTML5ToolChain.cs
		// UEBuildHTML5.cs
		// HTML5Platform.[PakFiles.]Automation.cs
	}
}
