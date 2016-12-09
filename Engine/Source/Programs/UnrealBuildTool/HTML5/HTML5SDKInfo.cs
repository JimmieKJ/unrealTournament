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
		static string SDKVersion = "incoming";
		static string NODE_VER = "4.1.1_64bit";
		static string LLVM_VER = "incoming";
		static string PYTHON_VER = "2.7.5.3_64bit";

		// --------------------------------------------------
		// --------------------------------------------------
//		static string SDKBase
//		{
//			get
//			{
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//				if (Environment.GetEnvironmentVariable("EMSDK") != null) return Environment.GetEnvironmentVariable("EMSDK");
//
//				// Otherwise, use the one embedded in this repository.
//				return Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, "Extras", "ThirdPartyNotUE", "emsdk"));
//			}
//		}
//		static public string EMSCRIPTEN_ROOT
//		{
//			get
//			{
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//				if (Environment.GetEnvironmentVariable("EMSCRIPTEN") != null) return Environment.GetEnvironmentVariable("EMSCRIPTEN");
//
//				// Otherwise, use the one embedded in this repository.		
//				return Path.Combine(SDKBase, "emscripten", SDKVersion);
//			}
//		}
		static string SDKBase { get { return Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, "Extras", "ThirdPartyNotUE", "emsdk")); } }
		static public string EMSCRIPTEN_ROOT { get { return Path.Combine(SDKBase, "emscripten", SDKVersion); } }
		static public string EmscriptenCMakeToolChainFile { get { return Path.Combine(EMSCRIPTEN_ROOT,  "cmake", "Modules", "Platform", "Emscripten.cmake"); } }
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

//		// Reads the contents of the ".emscripten" config file.
//		static string ReadEmscriptenConfigFile()
//		{
//			string config = Environment.GetEnvironmentVariable("EM_CONFIG"); // This is either a string containing the config directly, or points to a file
//			if (File.Exists(config))
//			{
//                config = File.ReadAllText(config);
//			}
//			return config;
//		}
//
//		// Pulls the value of the given option key in .emscripten config file.
//		static string GetEmscriptenConfigVar(string variable)
//		{
//			string config = ReadEmscriptenConfigFile();
//            if (config == null) return "";
//			string[] tokens = config.Split('\n');
//
//			// Parse lines of type "KEY='value'"
//			Regex regex = new Regex(variable + "\\s*=\\s*['\\\"](.*)['\\\"]");
//			foreach(string line in tokens)
//			{
//				Match m = regex.Match(line);
//				if (m.Success)
//				{
//					return m.Groups[1].ToString();
//				}
//			}
//			return "";
//		}
//
//		static string NODE_JS
//		{
//			get
//			{
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//				string node_js = GetEmscriptenConfigVar("NODE_JS");
//				if (node_js != null) return node_js;
//
//				// Otherwise, use the one embedded in this repository.
//				return Path.Combine(SDKBase, CURRENT_PLATFORM, "node", NODE_VER, "bin", "node" + PLATFORM_EXE);
//			}
//		}
//		static string LLVM_ROOT
//		{
//			get
//			{
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//				string llvm_root = GetEmscriptenConfigVar("LLVM_ROOT");
//				if (llvm_root != null) return llvm_root;
//
//				// Otherwise, use the one embedded in this repository.
//                return Path.Combine(SDKBase, CURRENT_PLATFORM, "clang", LLVM_VER);
//			}
//		}
//		static string PYTHON
//		{
//			get
//			{
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//				string python = GetEmscriptenConfigVar("PYTHON");
//				if (python != null) return python;
//
//                switch (BuildHostPlatform.Current.Platform)
//				{
//					case UnrealTargetPlatform.Win64:
//						return Path.Combine(SDKBase, "Win64", "python", PYTHON_VER, "python.exe");
//					case UnrealTargetPlatform.Mac: // python is default installed on mac.
//						return "/usr/bin/python";
//					default:
//						return "error_unknown_platform";
//				}
//			}
//		}
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
//		static public string DOT_EMSCRIPTEN
//        {
//            get
//            {
//				// If user has configured a custom Emscripten toolchain, use that automatically.
//                if (Environment.GetEnvironmentVariable("EMSDK") != null && Environment.GetEnvironmentVariable("EM_CONFIG") != null && File.Exists(Environment.GetEnvironmentVariable("EM_CONFIG")))
//                {
//                    return Environment.GetEnvironmentVariable("EM_CONFIG");
//                }
//
//				// Otherwise, use the one embedded in this repository.
//                return Path.Combine(HTML5Intermediatory, ".emscripten");
//            }
//        }
//		static public string EMSCRIPTEN_CACHE { get { return Path.Combine(HTML5Intermediatory, "EmscriptenCache"); ; } }
		static public string DOT_EMSCRIPTEN { get { return Path.Combine(HTML5Intermediatory, ".emscripten"); } }
		static public string EMSCRIPTEN_CACHE { get { return Path.Combine(HTML5Intermediatory, "EmscriptenCache"); ; } }

		public static string SetupEmscriptenTemp()
		{
//			// If user has configured a custom Emscripten toolchain, use that automatically.
//			if (Environment.GetEnvironmentVariable("EMSDK") != null)
//			{
//				string emscripten_temp = GetEmscriptenConfigVar("TEMP_DIR");
//				if (emscripten_temp != null) return emscripten_temp;
//			}

			// Otherwise, use the one embedded in this repository.
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
//			// If user has configured a custom Emscripten toolchain, use that automatically.
//			if (Environment.GetEnvironmentVariable("EMSDK") != null && Environment.GetEnvironmentVariable("EM_CONFIG") != null && File.Exists(Environment.GetEnvironmentVariable("EM_CONFIG")))
//			{
//				return Environment.GetEnvironmentVariable("EM_CONFIG");
//			}

			// Otherwise, use the one embedded in this repository.

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
				Console.WriteLine("emcc ExitCode: {0}", process.ExitCode);
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
			// the following are needed when CMake is used
			Environment.SetEnvironmentVariable("EMSCRIPTEN", EMSCRIPTEN_ROOT);
			Environment.SetEnvironmentVariable("NODEPATH", Path.GetDirectoryName(NODE_JS));
			Environment.SetEnvironmentVariable("NODE", NODE_JS);
			Environment.SetEnvironmentVariable("LLVM", LLVM_ROOT);

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
		// BuildPhysX.Automation.cs
	}
}
