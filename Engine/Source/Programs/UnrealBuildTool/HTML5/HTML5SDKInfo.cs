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
		static string SDKBase { get { return Path.GetFullPath(Path.Combine(new string[] { BuildConfiguration.RelativeEnginePath, "Source", "ThirdParty", "HTML5", "emsdk" })); } }
		// A GUID as a string. Allows updates to flush the emscripten install without bumping the SDK version number. Useful if a programming-error causes a bogus install.
		static string SDKInfoGUID = "49CA9678-2667-48BC-A6A9-25D6FB341F08";
		static string SDKVersion = "1.30.0";
		static string EMSCRIPTEN_ROOT { get { return Path.Combine(SDKBase, "emscripten", SDKVersion); } }

		static string PYTHON_WIN { get { return Path.Combine(SDKBase, "Win64", "python", "2.7.5.3_64bit", "python.exe"); } }
		// python is default installed on mac. 
		static string PYTHON_MAC { get { return Path.Combine("/usr/bin/python"); } }

		static string PYTHON
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return PYTHON_WIN;
					case UnrealTargetPlatform.Mac:
						return PYTHON_MAC;
					default:
						return "";
				}
			}
		}

		static string LLVM_ROOT_WIN { get { return Path.Combine(SDKBase, "Win64", "clang", "e" + SDKVersion + "_64bit"); } }
		static string LLVM_ROOT_MAC { get { return Path.Combine(SDKBase, "Mac", "clang", "e" + SDKVersion + "_64bit"); } }

		static string LLVM_ROOT
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return LLVM_ROOT_WIN;
					case UnrealTargetPlatform.Mac:
						return LLVM_ROOT_MAC;
					default:
						return "";
				}
			}
		}

		static string NODE_JS_WIN { get { return Path.Combine(SDKBase, "Win64", "node", "0.12.2_64bit", "node.exe"); } }
		static string NODE_JS_MAC { get { return Path.Combine(SDKBase, "Mac", "node", "0.12.2_64bit", "bin", "node"); } }

		static string NODE_JS
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return NODE_JS_WIN;
					case UnrealTargetPlatform.Mac:
						return NODE_JS_MAC;
					default:
						return "";
				}
			}
		}

		static string OPTIMIZER_NAME
		{
			get
			{
				switch (BuildHostPlatform.Current.Platform)
				{
					case UnrealTargetPlatform.Win64:
						return "optimizer.exe";
					case UnrealTargetPlatform.Mac:
						return "optimizer";
					default:
						return "";
				}
			}
		}

		static public string DOT_EMSCRIPTEN
		{
			get
			{
				var TempPath = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.BaseIntermediateFolder, "HTML5"));
				if (!Directory.Exists(TempPath))
				{
					Directory.CreateDirectory(TempPath);
				}
				return Path.Combine(TempPath, ".emscripten");
			}
		}

		static public string EMSCRIPTEN_CACHE
		{
			get
			{
				var TempPath = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.BaseIntermediateFolder, "HTML5"));
				if (!Directory.Exists(TempPath))
				{
					Directory.CreateDirectory(TempPath);
				}
				return Path.Combine(TempPath, "EmscriptenCache"); ;
			}
		}

		public static string SetupEmscriptenTemp()
		{
			string HTML5Intermediatory = Path.GetFullPath(Path.Combine(BuildConfiguration.RelativeEnginePath, BuildConfiguration.BaseIntermediateFolder, "HTML5"));
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

		public static string SetUpEmscriptenConfigFile()
		{
			string ConfigFile = DOT_EMSCRIPTEN;

			if (!File.Exists(ConfigFile) || !File.ReadAllText(ConfigFile).Contains("GENERATEDBYUE4='" + SDKVersion + "+" + SDKInfoGUID + "'"))
			{
				var ConfigString = String.Join(
								Environment.NewLine,
								"import os",
								"SPIDERMONKEY_ENGINE = ''",
								"LLVM_ROOT='" + LLVM_ROOT + "'",
								"NODE_JS= '" + NODE_JS + "'",
								"PYTHON= '" + PYTHON + "'",
								"EMSCRIPTEN_NATIVE_OPTIMIZER='" + Path.Combine(LLVM_ROOT, OPTIMIZER_NAME) + "'",
								"EMSCRIPTEN_ROOT= '" + EMSCRIPTEN_ROOT + "'",
								"TEMP_DIR= '" + SetupEmscriptenTemp() + "'",
								"COMPILER_ENGINE = NODE_JS",
								"JS_ENGINES = [NODE_JS]",
								"V8_ENGINE = ''",
								"GENERATEDBYUE4='" + SDKVersion + "+" + SDKInfoGUID + "'"
								);
				File.WriteAllText(ConfigFile, ConfigString.Replace("\\", "/"));
			}
			return ConfigFile;
		}

		public static string EmscriptenBase()
		{
			return EMSCRIPTEN_ROOT;
		}

		public static string EmscriptenVersion()
		{
			return SDKVersion;
		}

		public static string Python()
		{
			return PYTHON;
		}

		public static string EmscriptenPackager()
		{
			return Path.Combine(EmscriptenBase(), "tools", "file_packager.py");
		}

		public static string EmscriptenCompiler()
		{
			return "\"" + Path.Combine(EmscriptenBase(), "emcc") + "\"";
		}

		public static bool IsSDKInstalled()
		{
			bool SDK = File.Exists(GetVersionInfoPath());

			switch (BuildHostPlatform.Current.Platform)
			{
				case UnrealTargetPlatform.Win64:
					return SDK && Directory.Exists(Path.Combine(SDKBase, "Win64"));
				case UnrealTargetPlatform.Mac:
					return SDK && Directory.Exists(Path.Combine(SDKBase, "Mac")) && File.Exists(PYTHON);
				default:
					return false;
			}
		}

		static string GetVersionInfoPath()
		{
			return Path.Combine(EmscriptenBase(), "emscripten-version.txt");
		}
	}
}
