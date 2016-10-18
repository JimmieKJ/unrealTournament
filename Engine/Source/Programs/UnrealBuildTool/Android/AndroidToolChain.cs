// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	public class AndroidToolChain : UEToolChain
	{
		private FileReference ProjectFile;

		// the number of the clang version being used to compile 
		private float ClangVersionFloat = 0;

		// the list of architectures we will compile for
		private List<string> Arches = null;
		// the list of GPU architectures we will compile for
		private List<string> GPUArchitectures = null;
		// a list of all architecture+GPUArchitecture names (-armv7-es2, etc)
		private List<string> AllComboNames = null;

		static private Dictionary<string, string[]> AllArchNames = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { "armv7", "armeabi-v7a", } }, 
			{ "-arm64", new string[] { "arm64", "arm64-v8a", } }, 
			{ "-x86",   new string[] { "x86", } }, 
			{ "-x64",   new string[] { "x64", "x86_64", } }, 
		};

		static private Dictionary<string, string[]> LibrariesToSkip = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { } }, 
			{ "-arm64", new string[] { "nvToolsExt", "nvToolsExtStub", "oculus", "vrapi", "ovrkernel", "systemutils", "openglloader", } },
			{ "-x86",   new string[] { "nvToolsExt", "nvToolsExtStub", "oculus", "vrapi", "ovrkernel", "systemutils", "openglloader", } }, 
			{ "-x64",   new string[] { "nvToolsExt", "nvToolsExtStub", "oculus", "vrapi", "ovrkernel", "systemutils", "openglloader", "gpg", } }, 
		};

		static private Dictionary<string, string[]> ModulesToSkip = new Dictionary<string, string[]> {
			{ "-armv7", new string[] {  } }, 
			{ "-arm64", new string[] {  } }, 
			{ "-x86",   new string[] {  } }, 
			{ "-x64",   new string[] { "OnlineSubsystemGooglePlay", } }, 
		};

		public AndroidToolChain(FileReference InProjectFile) 
			: base(CPPTargetPlatform.Android)
		{
			ProjectFile = InProjectFile;

			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");

			// don't register if we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				throw new BuildException("NDKROOT is not specified; cannot use Android toolchain.");
			}

			NDKPath = NDKPath.Replace("\"", "");

			string ClangVersion = "";
			string GccVersion = "";

			string ArchitecturePath = "";
			string ArchitecturePathWindows32 = @"prebuilt/windows";
			string ArchitecturePathWindows64 = @"prebuilt/windows-x86_64";
			string ArchitecturePathMac = @"prebuilt/darwin-x86_64";
			string ExeExtension = ".exe";

			if (Directory.Exists(Path.Combine(NDKPath, ArchitecturePathWindows64)))
			{
				Log.TraceVerbose("        Found Windows 64 bit versions of toolchain");
				ArchitecturePath = ArchitecturePathWindows64;
			}
			else if (Directory.Exists(Path.Combine(NDKPath, ArchitecturePathWindows32)))
			{
				Log.TraceVerbose("        Found Windows 32 bit versions of toolchain");
				ArchitecturePath = ArchitecturePathWindows32;
			}
			else if (Directory.Exists(Path.Combine(NDKPath, ArchitecturePathMac)))
			{
				Log.TraceVerbose("        Found Mac versions of toolchain");
				ArchitecturePath = ArchitecturePathMac;
				ExeExtension = "";
			}
			else
			{
				throw new BuildException("Couldn't find 32-bit or 64-bit versions of the Android toolchain");
			}

			// prefer clang 3.6, but fall back if needed for now
            if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.6")))
			{
				ClangVersionFloat = 3.6f;
				ClangVersion = "-3.6";
				GccVersion = "4.9";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.5")))
			{
				ClangVersionFloat = 3.5f;
				ClangVersion = "-3.5";
				GccVersion = "4.9";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.3")))
			{
				ClangVersionFloat = 3.3f;
				ClangVersion = "-3.3";
				GccVersion = "4.8";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.1")))
			{
				ClangVersionFloat = 3.1f;
				ClangVersion = "-3.1";
				GccVersion = "4.6";
			}
            else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm")))
            {
				// look for version in AndroidVersion.txt (fail if not found)
				string VersionFilename = Path.Combine(NDKPath, @"toolchains/llvm/", ArchitecturePath, @"AndroidVersion.txt");
				if (!File.Exists(VersionFilename))
				{
					throw new BuildException("Cannot find supported Android toolchain");
				}
				string[] VersionFile = File.ReadAllLines(VersionFilename);
				string[] VersionParts = VersionFile[0].Split('.');
				ClangVersionFloat = float.Parse(VersionParts[0] + "." + VersionParts[1], System.Globalization.CultureInfo.InvariantCulture);
				ClangVersion = "";
				GccVersion = "4.9";
			}
			else
			{
				throw new BuildException("Cannot find supported Android toolchain");
			}

			// set up the path to our toolchains
			ClangPath = Path.Combine(NDKPath, @"toolchains/llvm" + ClangVersion, ArchitecturePath, @"bin/clang++" + ExeExtension);
			ArPathArm = Path.Combine(NDKPath, @"toolchains/arm-linux-androideabi-" + GccVersion, ArchitecturePath, @"bin/arm-linux-androideabi-ar" + ExeExtension);		//@todo android: use llvm-ar.exe instead?
			ArPathArm64 = Path.Combine(NDKPath, @"toolchains/aarch64-linux-android-" + GccVersion, ArchitecturePath, @"bin/aarch64-linux-android-ar" + ExeExtension);	//@todo android: use llvm-ar.exe instead?
			ArPathx86 = Path.Combine(NDKPath, @"toolchains/x86-" + GccVersion, ArchitecturePath, @"bin/i686-linux-android-ar" + ExeExtension);							//@todo android: verify x86 toolchain
			ArPathx64 = Path.Combine(NDKPath, @"toolchains/x86_64-" + GccVersion, ArchitecturePath, @"bin/x86_64-linux-android-ar" + ExeExtension);                     //@todo android: verify x64 toolchain

			// NDK setup (use no less than 21 for 64-bit targets)
			int NDKApiLevelInt = GetNdkApiLevelInt();
			string NDKApiLevel32Bit = GetNdkApiLevel();
			string NDKApiLevel64Bit = NDKApiLevel32Bit;
			if (NDKApiLevelInt < 21)
			{
				NDKApiLevel64Bit = "android-21";
			}

			// toolchain params
			ToolchainParamsArm = " -target armv7-none-linux-androideabi" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", NDKApiLevel32Bit, "arch-arm") + "\"" +
								   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains/arm-linux-androideabi-" + GccVersion, ArchitecturePath) + "\"";
			ToolchainParamsArm64 = " -target aarch64-none-linux-android" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", NDKApiLevel64Bit, "arch-arm64") + "\"" +
								   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains/aarch64-linux-android-" + GccVersion, ArchitecturePath) + "\"";
			ToolchainParamsx86 = " -target i686-none-linux-android" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", NDKApiLevel32Bit, "arch-x86") + "\"" +
								   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains/x86-" + GccVersion, ArchitecturePath) + "\"";
			ToolchainParamsx64 = " -target x86_64-none-linux-android" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", NDKApiLevel64Bit, "arch-x86_64") + "\"" +
								   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains\x86_64-" + GccVersion, ArchitecturePath) + "\"";
		}

		public void ParseArchitectures()
		{
			// look in ini settings for what platforms to compile for
			ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Android, "Engine", DirectoryReference.FromFile(ProjectFile));
			Arches = new List<string>();
			bool bBuild = true;
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForArmV7", out bBuild) && bBuild
				|| UEBuildConfiguration.Architectures.Contains("armv7", StringComparer.OrdinalIgnoreCase))
			{
				Arches.Add("-armv7");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForArm64", out bBuild) && bBuild
				|| UEBuildConfiguration.Architectures.Contains("arm64", StringComparer.OrdinalIgnoreCase))
			{
				Arches.Add("-arm64");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForx86", out bBuild) && bBuild
				|| UEBuildConfiguration.Architectures.Contains("x86", StringComparer.OrdinalIgnoreCase))
			{
				Arches.Add("-x86");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForx8664", out bBuild) && bBuild
				|| UEBuildConfiguration.Architectures.Contains("x64", StringComparer.OrdinalIgnoreCase))
			{
				Arches.Add("-x64");
			}

			// force armv7 if something went wrong
			if (Arches.Count == 0)
			{
				Arches.Add("-armv7");
			}

			// Parse selected GPU architectures
			GPUArchitectures = new List<string>();
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForES2", out bBuild) && bBuild
				|| UEBuildConfiguration.GPUArchitectures.Contains("es2", StringComparer.OrdinalIgnoreCase))
			{
				GPUArchitectures.Add("-es2");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForESDeferred", out bBuild) && bBuild
				|| UEBuildConfiguration.GPUArchitectures.Contains("esdeferred", StringComparer.OrdinalIgnoreCase))
			{
				GPUArchitectures.Add("-esdeferred");
			}
			if (GPUArchitectures.Count == 0)
			{
				GPUArchitectures.Add("-es2");
			}

			AllComboNames = (from Arch in Arches
							 from GPUArch in GPUArchitectures
							 select Arch + GPUArch).ToList();
		}

		static public string GetGLESVersionFromGPUArch(string GPUArch)
		{
			GPUArch = GPUArch.Substring(1); // drop the '-' from the start
			string GLESversion = "";
			switch (GPUArch)
			{
				case "es2":
					GLESversion = "0x00020000";
					break;
				case "esdeferred":
					GLESversion = "0x00030001";
					break;
				default:
					GLESversion = "0x00020000";
					break;
			}

			return GLESversion;
		}

		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			ParseArchitectures();
		}

		public List<string> GetAllArchitectures()
		{
			if (Arches == null)
			{
				ParseArchitectures();
			}

			return Arches;
		}

		public List<string> GetAllGPUArchitectures()
		{
			if (GPUArchitectures == null)
			{
				ParseArchitectures();
			}

			return GPUArchitectures;
		}

		public int GetNdkApiLevelInt(int MinNdk = 19)
		{
			string NDKVersion = GetNdkApiLevel();
			int NDKVersionInt = MinNdk;
			if (NDKVersion.Contains("-"))
			{
				int Version;
				if (int.TryParse(NDKVersion.Substring(NDKVersion.LastIndexOf('-') + 1), out Version))
				{
					if (Version > NDKVersionInt)
						NDKVersionInt = Version;
				}
			}
			return NDKVersionInt;
		}

		public string GetNdkApiLevel()
		{
			// ask the .ini system for what version to use
			ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Android, "Engine", DirectoryReference.FromFile(ProjectFile));
			string NDKLevel;
			Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "NDKAPILevel", out NDKLevel);

			if (NDKLevel == "latest")
			{
				// get a list of NDK platforms
				string PlatformsDir = Environment.ExpandEnvironmentVariables("%NDKROOT%/platforms");
				if (!Directory.Exists(PlatformsDir))
				{
					throw new BuildException("No platforms found in {0}", PlatformsDir);
				}

				// return the largest of them
				NDKLevel = GetLargestApiLevel(Directory.GetDirectories(PlatformsDir));
			}

			return NDKLevel;
		}

		public string GetLargestApiLevel(string[] ApiLevels)
		{
			int LargestLevel = 0;
			string LargestString = null;

			// look for largest integer
			foreach (string Level in ApiLevels)
			{
				string LocalLevel = Path.GetFileName(Level);
				string[] Tokens = LocalLevel.Split("-".ToCharArray());
				if (Tokens.Length >= 2)
				{
					try
					{
						int ParsedLevel = int.Parse(Tokens[1]);
						// bigger? remember it
						if (ParsedLevel > LargestLevel)
						{
							LargestLevel = ParsedLevel;
							LargestString = LocalLevel;
						}
					}
					catch (Exception)
					{
						// ignore poorly formed string
					}
				}
			}

			return LargestString;
		}

		string GetCLArguments_Global(CPPEnvironment CompileEnvironment, string Architecture)
		{
			string Result = "";

			switch (Architecture)
			{
				case "-armv7": Result += ToolchainParamsArm; break;
				case "-arm64": Result += ToolchainParamsArm64; break;
				case "-x86": Result += ToolchainParamsx86; break;
				case "-x64": Result += ToolchainParamsx64; break;
				default: Result += ToolchainParamsArm; break;
			}

			// build up the commandline common to C and C++
			Result += " -c";
			Result += " -fdiagnostics-format=msvc";
			Result += " -Wall";

			Result += " -Wno-unused-variable";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			//This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			Result += " -Wno-local-type-template-args";	// engine triggers this
			Result += " -Wno-return-type-c-linkage";	// needed for PhysX
			Result += " -Wno-reorder";					// member initialization order
			Result += " -Wno-unknown-pragmas";			// probably should kill this one, sign of another issue in PhysX?
			Result += " -Wno-invalid-offsetof";			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-logical-op-parentheses";	// needed for external headers we can't change

			if (CompileEnvironment.Config.bEnableShadowVariableWarning)
			{
				Result += " -Wshadow -Wno-error=shadow";
			}

			// new for clang4.5 warnings:
			if (ClangVersionFloat >= 3.5f)
			{
				Result += " -Wno-undefined-bool-conversion"; // 'this' pointer cannot be null in well-defined C++ code; pointer may be assumed to always convert to true (if (this))

				// we use this feature to allow static FNames.
				Result += " -Wno-gnu-string-literal-operator-template";
			}

			if (ClangVersionFloat >= 3.6f)
			{
				Result += " -Wno-unused-local-typedef";				// clang is being overly strict here? PhysX headers trigger this.
				Result += " -Wno-inconsistent-missing-override";	// these have to be suppressed for UE 4.8, should be fixed later.
			}

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
			}

			// debug info
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				Result += " -g2 -gdwarf-4";
			}

			// optimization level
			if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
			{
				Result += " -O0";
			}
			else
			{
				if (UEBuildConfiguration.bCompileForSize)
				{
					Result += " -Oz";
				}
				else
				{
					Result += " -O3";
				}
			}

			//@todo android: these are copied verbatim from UE3 and probably need adjustment
			if (Architecture == "-armv7")
			{
				//		Result += " -mthumb-interwork";			// Generates code which supports calling between ARM and Thumb instructions, w/o it you can't reliability use both together 
				Result += " -funwind-tables";			// Just generates any needed static data, affects no code 
				Result += " -fstack-protector";			// Emits extra code to check for buffer overflows
				//		Result += " -mlong-calls";				// Perform function calls by first loading the address of the function into a reg and then performing the subroutine call
				Result += " -fno-strict-aliasing";		// Prevents unwanted or invalid optimizations that could produce incorrect code
				Result += " -fpic";						// Generates position-independent code (PIC) suitable for use in a shared library
				Result += " -fno-exceptions";			// Do not enable exception handling, generates extra code needed to propagate exceptions
				Result += " -fno-rtti";					// 
				Result += " -fno-short-enums";			// Do not allocate to an enum type only as many bytes as it needs for the declared range of possible values
				//		Result += " -finline-limit=64";			// GCC limits the size of functions that can be inlined, this flag allows coarse control of this limit
				//		Result += " -Wno-psabi";				// Warn when G++ generates code that is probably not compatible with the vendor-neutral C++ ABI

				Result += " -march=armv7-a";
				Result += " -mfloat-abi=softfp";
				Result += " -mfpu=vfpv3-d16";			//@todo android: UE3 was just vfp. arm7a should all support v3 with 16 registers

				// Add flags for on-device debugging	
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
				{
					Result += " -fno-omit-frame-pointer";	// Disable removing the save/restore frame pointer for better debugging
					if (ClangVersionFloat >= 3.6f)
					{
						Result += " -fno-function-sections";	// Improve breakpoint location
					}
				}

				// Some switches interfere with on-device debugging
				if (CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug)
				{
					Result += " -ffunction-sections";   // Places each function in its own section of the output file, linker may be able to perform opts to improve locality of reference
				}

				Result += " -fsigned-char";				// Treat chars as signed //@todo android: any concerns about ABI compatibility with libs here?
			}
			else if (Architecture == "-arm64")
			{
				Result += " -funwind-tables";			// Just generates any needed static data, affects no code 
				Result += " -fstack-protector";			// Emits extra code to check for buffer overflows
				Result += " -fno-strict-aliasing";		// Prevents unwanted or invalid optimizations that could produce incorrect code
				Result += " -fpic";						// Generates position-independent code (PIC) suitable for use in a shared library
				Result += " -fno-exceptions";			// Do not enable exception handling, generates extra code needed to propagate exceptions
				Result += " -fno-rtti";					// 
				Result += " -fno-short-enums";          // Do not allocate to an enum type only as many bytes as it needs for the declared range of possible values

				Result += " -D__arm64__";				// for some reason this isn't defined and needed for PhysX

				Result += " -march=armv8-a";
				//Result += " -mfloat-abi=softfp";
				//Result += " -mfpu=vfpv3-d16";			//@todo android: UE3 was just vfp. arm7a should all support v3 with 16 registers

				// Some switches interfere with on-device debugging
				if (CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug)
				{
					Result += " -ffunction-sections";   // Places each function in its own section of the output file, linker may be able to perform opts to improve locality of reference
				}

				Result += " -fsigned-char";				// Treat chars as signed //@todo android: any concerns about ABI compatibility with libs here?
			}
			else if (Architecture == "-x86")
			{
				Result += " -fstrict-aliasing";
				Result += " -fno-omit-frame-pointer";
				Result += " -fno-strict-aliasing";
				Result += " -fno-short-enums";
				Result += " -fno-exceptions";
				Result += " -fno-rtti";
				Result += " -march=atom";
			}
			else if (Architecture == "-x64")
			{
				Result += " -fstrict-aliasing";
				Result += " -fno-omit-frame-pointer";
				Result += " -fno-strict-aliasing";
				Result += " -fno-short-enums";
				Result += " -fno-exceptions";
				Result += " -fno-rtti";
				Result += " -march=atom";
			}

			return Result;
		}

		static string GetCompileArguments_CPP(bool bDisableOptimizations)
		{
			string Result = "";

			Result += " -x c++";
			Result += " -std=c++11";

			// optimization level
			if (bDisableOptimizations)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O3";
			}

			return Result;
		}

		static string GetCompileArguments_C(bool bDisableOptimizations)
		{
			string Result = "";

			Result += " -x c";

			// optimization level
			if (bDisableOptimizations)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O3";
			}

			return Result;
		}

		static string GetCompileArguments_PCH(bool bDisableOptimizations)
		{
			string Result = "";

			Result += " -x c++-header";
			Result += " -std=c++11";

			// optimization level
			if (bDisableOptimizations)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O3";
			}

			return Result;
		}

		string GetLinkArguments(LinkEnvironment LinkEnvironment, string Architecture)
		{
			string Result = "";

			Result += " -nostdlib";
			Result += " -Wl,-shared,-Bsymbolic";
			Result += " -Wl,--no-undefined";

			if (Architecture == "-arm64")
			{
				Result += ToolchainParamsArm64;
				Result += " -march=armv8-a";
			}
			else if (Architecture == "-x86")
			{
				Result += ToolchainParamsx86;
				Result += " -march=atom";
			}
			else if (Architecture == "-x64")
			{
				Result += ToolchainParamsx64;
				Result += " -march=atom";
			}
			else // if (Architecture == "-armv7")
			{
				Result += ToolchainParamsArm;
				Result += " -march=armv7-a";
				Result += " -Wl,--fix-cortex-a8";		// required to route around a CPU bug in some Cortex-A8 implementations
			}

			if (BuildConfiguration.bUseUnityBuild && ClangVersionFloat >= 3.6f && ClangVersionFloat < 3.8f)
			{
				Result += " -fuse-ld=gold";				// ld.gold is available in r10e (clang 3.6)
			}

			// make sure the DT_SONAME field is set properly (or we can a warning toast at startup on new Android)
			Result += " -Wl,-soname,libUE4.so";

			// verbose output from the linker
			// Result += " -v";

			return Result;
		}

		static string GetArArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -r";

			return Result;
		}

		static bool IsDirectoryForArch(string Dir, string Arch)
		{
			// make sure paths use one particular slash
			Dir = Dir.Replace("\\", "/").ToLowerInvariant();

			// look for other architectures in the Dir path, and fail if it finds it
			foreach (var Pair in AllArchNames)
			{
				if (Pair.Key != Arch)
				{
					foreach (var ArchName in Pair.Value)
					{
						// if there's a directory in the path with a bad architecture name, reject it
						if (Regex.IsMatch(Dir, "/" + ArchName + "$") || Regex.IsMatch(Dir, "/" + ArchName + "/"))
						{
							return false;
						}
					}
				}
			}

			// if nothing was found, we are okay
			return true;
		}

		static bool ShouldSkipModule(string ModuleName, string Arch)
		{
			foreach (var ModName in ModulesToSkip[Arch])
			{
				if (ModName == ModuleName)
				{
					return true;
				}
			}

			// if nothing was found, we are okay
			return false;
		}

		bool ShouldSkipLib(string Lib, string Arch, string GPUArchitecture)
		{
			// reject any libs we outright don't want to link with
			foreach (var LibName in LibrariesToSkip[Arch])
			{
				if (LibName == Lib)
				{
					return true;
				}
			}

			// if another architecture is in the filename, reject it
			foreach (string ComboName in AllComboNames)
			{
				if (ComboName != Arch + GPUArchitecture)
				{
					if (Path.GetFileNameWithoutExtension(Lib).EndsWith(ComboName))
					{
						return true;
					}
				}
			}

			// if nothing was found, we are okay
			return false;
		}

		static void ConditionallyAddNDKSourceFiles(List<FileItem> SourceFiles, string ModuleName)
		{
			// We need to add the extra glue and cpu code only to Launch module.
			if (ModuleName.Equals("Launch"))
			{
				SourceFiles.Add(FileItem.GetItemByPath(Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/native_app_glue/android_native_app_glue.c"));

				// Newer NDK cpu_features.c uses getauxval() which causes a SIGSEGV in libhoudini.so (ARM on Intel translator) in older versions of Houdini
				// so we patch the file to use alternative methods of detecting CPU features if libhoudini.so is detected
				// The basis for this patch is from here: https://android-review.googlesource.com/#/c/110650/
				string CpuFeaturesPath = Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/cpufeatures/";
				string CpuFeaturesPatchedFile = CpuFeaturesPath + "cpu-features-patched.c";
				if (!File.Exists(CpuFeaturesPatchedFile))
				{
					// Either make a copy or patch it
					string[] CpuFeaturesLines = File.ReadAllLines(CpuFeaturesPath + "cpu-features.c");

					// Look for get_elf_hwcap_from_getauxval in the file
					bool NeedsPatch = false;
					int LineIndex;
					for (LineIndex = 0; LineIndex < CpuFeaturesLines.Length; ++LineIndex)
					{
						if (CpuFeaturesLines[LineIndex].Contains("get_elf_hwcap_from_getauxval"))
						{
							NeedsPatch = true;

							// Make sure it doesn't already have the patch (r10c and 10d have it already, but removed in 10e)
							for (int LineIndex2 = LineIndex; LineIndex2 < CpuFeaturesLines.Length; ++LineIndex2)
							{
								if (CpuFeaturesLines[LineIndex2].Contains("has_houdini_binary_translator(void)"))
								{
									NeedsPatch = false;
									break;
								}
							}
							break;
						}
					}

					// Apply patch or write unchanged
					if (NeedsPatch)
					{
						List<string> CpuFeaturesList = new List<string>(CpuFeaturesLines);

						// Skip down to section to add Houdini check function for arm
						while (!CpuFeaturesList[++LineIndex].StartsWith("#if defined(__arm__)")) ;
						CpuFeaturesList.Insert(++LineIndex, "/* Check Houdini Binary Translator is installed on the system.");
						CpuFeaturesList.Insert(++LineIndex, " *");
						CpuFeaturesList.Insert(++LineIndex, " * If this function returns 1, get_elf_hwcap_from_getauxval() function");
						CpuFeaturesList.Insert(++LineIndex, " * will causes SIGSEGV while calling getauxval() function.");
						CpuFeaturesList.Insert(++LineIndex, " */");
						CpuFeaturesList.Insert(++LineIndex, "static int");
						CpuFeaturesList.Insert(++LineIndex, "has_houdini_binary_translator(void) {");
						CpuFeaturesList.Insert(++LineIndex, "    int found = 0;");
						CpuFeaturesList.Insert(++LineIndex, "    if (access(\"/system/lib/libhoudini.so\", F_OK) != -1) {");
						CpuFeaturesList.Insert(++LineIndex, "        D(\"Found Houdini binary translator\\n\");");
						CpuFeaturesList.Insert(++LineIndex, "        found = 1;");
						CpuFeaturesList.Insert(++LineIndex, "    }");
						CpuFeaturesList.Insert(++LineIndex, "    return found;");
						CpuFeaturesList.Insert(++LineIndex, "}");
						CpuFeaturesList.Insert(++LineIndex, "");

						// Add the Houdini check call
						while (!CpuFeaturesList[++LineIndex].Contains("/* Extract the list of CPU features from ELF hwcaps */")) ;
						CpuFeaturesList.Insert(LineIndex++, "        /* Check Houdini binary translator is installed */");
						CpuFeaturesList.Insert(LineIndex++, "        int has_houdini = has_houdini_binary_translator();");
						CpuFeaturesList.Insert(LineIndex++, "");

						// Make the get_elf_hwcap_from_getauxval() calls conditional
						while (!CpuFeaturesList[++LineIndex].Contains("hwcaps = get_elf_hwcap_from_getauxval(AT_HWCAP);")) ;
						CpuFeaturesList.Insert(LineIndex++, "        if (!has_houdini) {");
						CpuFeaturesList.Insert(++LineIndex, "        }");
						while (!CpuFeaturesList[++LineIndex].Contains("hwcaps2 = get_elf_hwcap_from_getauxval(AT_HWCAP2);")) ;
						CpuFeaturesList.Insert(LineIndex++, "        if (!has_houdini) {");
						CpuFeaturesList.Insert(++LineIndex, "        }");

						File.WriteAllLines(CpuFeaturesPatchedFile, CpuFeaturesList.ToArray());
					}
					else
					{
						File.WriteAllLines(CpuFeaturesPatchedFile, CpuFeaturesLines);
					}
				}
				SourceFiles.Add(FileItem.GetItemByPath(CpuFeaturesPatchedFile));
			}
		}

		void GenerateEmptyLinkFunctionsForRemovedModules(List<FileItem> SourceFiles, string ModuleName, DirectoryReference OutputDirectory)
		{
			// Only add to UELinkerFixups module
			if (!ModuleName.Equals("Launch"))
			{
				return;
			}

			string LinkerExceptionsName = "../UELinkerExceptions";
			FileReference LinkerExceptionsCPPFilename = FileReference.Combine(OutputDirectory, LinkerExceptionsName + ".cpp");

			// Create the cpp filename
			if (!LinkerExceptionsCPPFilename.Exists())
			{
				// Create a dummy file in case it doesn't exist yet so that the module does not complain it's not there
				ResponseFile.Create(LinkerExceptionsCPPFilename, new List<string>());
			}

			var Result = new List<string>();
			foreach (string Arch in Arches)
			{
				switch (Arch)
				{
					case "-armv7": Result.Add("#if PLATFORM_ANDROID_ARM"); break;
					case "-arm64": Result.Add("#if PLATFORM_ANDROID_ARM64"); break;
					case "-x86": Result.Add("#if PLATFORM_ANDROID_X86"); break;
					case "-x64": Result.Add("#if PLATFORM_ANDROID_X64"); break;
					default: Result.Add("#if PLATFORM_ANDROID_ARM"); break;
				}

				foreach (var ModName in ModulesToSkip[Arch])
				{
					Result.Add("  void EmptyLinkFunctionForStaticInitialization" + ModName + "(){}");
				}
				Result.Add("#endif");
			}

			// Determine if the file changed. Write it if it either doesn't exist or the contents are different.
			bool bShouldWriteFile = true;
			if (LinkerExceptionsCPPFilename.Exists())
			{
				string[] ExistingExceptionText = File.ReadAllLines(LinkerExceptionsCPPFilename.FullName);
				string JoinedNewContents = string.Join("", Result.ToArray());
				string JoinedOldContents = string.Join("", ExistingExceptionText);
				bShouldWriteFile = (JoinedNewContents != JoinedOldContents);
			}

			// If we determined that we should write the file, write it now.
			if (bShouldWriteFile)
			{
				ResponseFile.Create(LinkerExceptionsCPPFilename, Result);
			}

			SourceFiles.Add(FileItem.GetItemByFileReference(LinkerExceptionsCPPFilename));
		}

		// cache the location of NDK tools
		static string ClangPath;
		static string ToolchainParamsArm;
		static string ToolchainParamsArm64;
		static string ToolchainParamsx86;
		static string ToolchainParamsx64;
		static string ArPathArm;
		static string ArPathArm64;
		static string ArPathx86;
		static string ArPathx64;

		static public string GetStripExecutablePath(string UE4Arch)
		{
			string StripPath;

			switch (UE4Arch)
			{
				case "-armv7": StripPath = ArPathArm; break;
				case "-arm64": StripPath = ArPathArm64; break;
				case "-x86": StripPath = ArPathx86; break;
				case "-x64": StripPath = ArPathx64; break;
				default: StripPath = ArPathArm; break;
			}
			return StripPath.Replace("-ar", "-strip");
		}

		static private bool bHasPrintedApiLevel = false;
		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			if (Arches.Count == 0)
			{
				throw new BuildException("At least one architecture (armv7, x86, etc) needs to be selected in the project settings to build");
			}

			if (!bHasPrintedApiLevel)
			{
				Console.WriteLine("Compiling Native code with NDK API '{0}'", GetNdkApiLevel());
				bHasPrintedApiLevel = true;
			}

			string BaseArguments = "";

			if (CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create)
			{
				BaseArguments += " -Werror";

			}

			// Directly added NDK files for NDK extensions
			ConditionallyAddNDKSourceFiles(SourceFiles, ModuleName);

			// Deal with dynamic modules removed by architecture
			GenerateEmptyLinkFunctionsForRemovedModules(SourceFiles, ModuleName, CompileEnvironment.Config.OutputDirectory);

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				BaseArguments += string.Format(" -D \"{0}\"", Definition);
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);
			var NDKRoot = Environment.GetEnvironmentVariable("NDKROOT").Replace("\\", "/");

			string BasePCHName = "";
			var PCHExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Android).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				BasePCHName = RemoveArchName(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath).Replace(PCHExtension, "");
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (string Arch in Arches)
			{
				if (ShouldSkipModule(ModuleName, Arch))
				{
					continue;
				}

				foreach (string GPUArchitecture in GPUArchitectures)
				{
					// which toolchain to use
					string Arguments = GetCLArguments_Global(CompileEnvironment, Arch) + BaseArguments;

					switch (Arch)
					{
						case "-armv7": Arguments += " -DPLATFORM_64BITS=0 -DPLATFORM_ANDROID_ARM=1"; break;
						case "-arm64": Arguments += " -DPLATFORM_64BITS=1 -DPLATFORM_ANDROID_ARM64=1"; break;
						case "-x86": Arguments += " -DPLATFORM_64BITS=0 -DPLATFORM_ANDROID_X86=1"; break;
						case "-x64": Arguments += " -DPLATFORM_64BITS=1 -DPLATFORM_ANDROID_X64=1"; break;
						default: Arguments += " -DPLATFORM_64BITS=0 -DPLATFORM_ANDROID_ARM=1"; break;
					}

                    if (GPUArchitecture == "-esdeferred")
					{
						Arguments += " -DPLATFORM_ANDROIDESDEFERRED=1";
					}

					// which PCH file to include
					string PCHArguments = "";
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						// Add the precompiled header file's path to the include path so Clang can find it.
						// This needs to be before the other include paths to ensure Clang uses it instead of the source header file.
						PCHArguments += string.Format(" -include \"{0}\"", InlineArchName(BasePCHName, Arch, GPUArchitecture));
					}

					// Add include paths to the argument list (filtered by architecture)
					foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
					{
						if (IsDirectoryForArch(IncludePath, Arch))
						{
							Arguments += string.Format(" -I\"{0}\"", IncludePath);
						}
					}
					foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
					{
						if (IsDirectoryForArch(IncludePath, Arch))
						{
							Arguments += string.Format(" -I\"{0}\"", IncludePath);
						}
					}

					foreach (FileItem SourceFile in SourceFiles)
					{
						Action CompileAction = new Action(ActionType.Compile);
						string FileArguments = "";
						bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";
						bool bDisableShadowWarning = false;

						// should we disable optimizations on this file?
						// @todo android - We wouldn't need this if we could disable optimizations per function (via pragma)
						bool bDisableOptimizations = false;// SourceFile.AbsolutePath.ToUpperInvariant().IndexOf("\\SLATE\\") != -1;
						if (bDisableOptimizations && CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug)
						{
							Log.TraceWarning("Disabling optimizations on {0}", SourceFile.AbsolutePath);
						}

						bDisableOptimizations = bDisableOptimizations || CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug;

						// Add C or C++ specific compiler arguments.
						if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
						{
							FileArguments += GetCompileArguments_PCH(bDisableOptimizations);
						}
						else if (bIsPlainCFile)
						{
							FileArguments += GetCompileArguments_C(bDisableOptimizations);

							// remove shadow variable warnings for NDK files
							if (SourceFile.AbsolutePath.Replace("\\", "/").StartsWith(NDKRoot))
							{
								bDisableShadowWarning = true;
							}
						}
						else
						{
							FileArguments += GetCompileArguments_CPP(bDisableOptimizations);

							// only use PCH for .cpp files
							FileArguments += PCHArguments;
						}

						// Add the C++ source file and its included files to the prerequisite item list.
						AddPrerequisiteSourceFile(Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

						if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
						{
							// Add the precompiled header file to the produced item list.
							FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
								FileReference.Combine(
									CompileEnvironment.Config.OutputDirectory,
									Path.GetFileName(InlineArchName(SourceFile.AbsolutePath, Arch, GPUArchitecture) + PCHExtension)
									)
								);

							CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
							Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

							// Add the parameters needed to compile the precompiled header file to the command-line.
							FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath, false);
						}
						else
						{
							if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
							{
								CompileAction.bIsUsingPCH = true;
								FileItem ArchPrecompiledHeaderFile = FileItem.GetItemByPath(InlineArchName(BasePCHName, Arch, GPUArchitecture) + PCHExtension);
								CompileAction.PrerequisiteItems.Add(ArchPrecompiledHeaderFile);
							}

							var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Android).GetBinaryExtension(UEBuildBinaryType.Object);

							// Add the object file to the produced item list.
							FileItem ObjectFile = FileItem.GetItemByFileReference(
								FileReference.Combine(
									CompileEnvironment.Config.OutputDirectory,
									InlineArchName(Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension, Arch, GPUArchitecture)
									)
								);
							CompileAction.ProducedItems.Add(ObjectFile);
							Result.ObjectFiles.Add(ObjectFile);

							FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath, false);
						}

						// Add the source file path to the command-line.
						FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

						// Build a full argument list
						string AllArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
						AllArguments = ActionThread.ExpandEnvironmentVariables(AllArguments);
						AllArguments = AllArguments.Replace("\\", "/");

						// Remove shadow warning for this file if requested
						if (bDisableShadowWarning)
						{
							int WarningIndex = AllArguments.IndexOf(" -Wshadow");
							if (WarningIndex > 0)
							{
								AllArguments = AllArguments.Remove(WarningIndex, 9);
							}
						}

						// Create the response file
						FileReference ResponseFileName = CompileAction.ProducedItems[0].Reference + ".response";
						string ResponseArgument = string.Format("@\"{0}\"", ResponseFile.Create(ResponseFileName, new List<string> { AllArguments }).FullName);

						CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
						CompileAction.CommandPath = ClangPath;
						CompileAction.CommandArguments = ResponseArgument;
						CompileAction.StatusDescription = string.Format("{0} [{1}-{2}]", Path.GetFileName(SourceFile.AbsolutePath), Arch.Replace("-", ""), GPUArchitecture.Replace("-", ""));

						// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
						CompileAction.bShouldOutputStatusDescription = true;

						// Don't farm out creation of pre-compiled headers as it is the critical path task.
						CompileAction.bCanExecuteRemotely =
							CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
							BuildConfiguration.bAllowRemotelyCompiledPCHs;
					}
				}
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			return null;
		}

		static public string InlineArchName(string Pathname, string Arch, string GPUArchitecture)
		{
			return Path.Combine(Path.GetDirectoryName(Pathname), Path.GetFileNameWithoutExtension(Pathname) + Arch + GPUArchitecture + Path.GetExtension(Pathname));
		}

		public string RemoveArchName(string Pathname)
		{
			// remove all architecture names
			foreach (string Arch in GetAllArchitectures())
			{
				foreach (string GPUArchitecture in GetAllGPUArchitectures())
				{
					Pathname = Path.Combine(Path.GetDirectoryName(Pathname), Path.GetFileName(Pathname).Replace(Arch + GPUArchitecture, ""));
				}
			}
			return Pathname;
		}

		public override FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			List<FileItem> Outputs = new List<FileItem>();

			var NDKRoot = Environment.GetEnvironmentVariable("NDKROOT").Replace("\\", "/");
			int NDKApiLevelInt = GetNdkApiLevelInt();
			string OptionalLinkArguments;

			for (int ArchIndex = 0; ArchIndex < Arches.Count; ArchIndex++)
			{
				string Arch = Arches[ArchIndex];

				// 32-bit ABI may need fixup for removed bsd_signal in NDK11 for android-21+
				OptionalLinkArguments = "";
				if (NDKApiLevelInt >= 21)
				{
					// this file was added in NDK11 so use existence to detect (RELEASE.TXT no longer present)
					if (File.Exists(Path.Combine(NDKRoot, "source.properties")))
					{
						switch (Arch)
						{
							case "-armv7":
								OptionalLinkArguments = string.Format(" \"{0}\"", Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Build/Android/Prebuilt/bsdsignal/lib/armeabi-v7a/libbsdsignal.a"));
								break;

							case "-x86":
								OptionalLinkArguments = string.Format(" \"{0}\"", Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Build/Android/Prebuilt/bsdsignal/lib/x86/libbsdsignal.a"));
								break;
						}
					}
				}

				for (int GPUArchIndex = 0; GPUArchIndex < GPUArchitectures.Count; GPUArchIndex++)
				{
					string GPUArchitecture = GPUArchitectures[GPUArchIndex];
					int OutputPathIndex = ArchIndex * GPUArchitectures.Count + GPUArchIndex;

					// Android will have an array of outputs
					if (LinkEnvironment.Config.OutputFilePaths.Count < OutputPathIndex ||
						!LinkEnvironment.Config.OutputFilePaths[OutputPathIndex].GetFileNameWithoutExtension().EndsWith(Arch + GPUArchitecture))
					{
						throw new BuildException("The OutputFilePaths array didn't match the Arches array in AndroidToolChain.LinkAllFiles");
					}

					// Create an action that invokes the linker.
					Action LinkAction = new Action(ActionType.Link);
					LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;

					if (LinkEnvironment.Config.bIsBuildingLibrary)
					{
						switch (Arch)
						{
							case "-armv7": LinkAction.CommandPath = ArPathArm; break;
							case "-arm64": LinkAction.CommandPath = ArPathArm64; break;
							case "-x86": LinkAction.CommandPath = ArPathx86; ; break;
							case "-x64": LinkAction.CommandPath = ArPathx64; ; break;
							default: LinkAction.CommandPath = ArPathArm; ; break;
						}
					}
					else
					{
						LinkAction.CommandPath = ClangPath;
					}

					string LinkerPath = LinkAction.WorkingDirectory;

					LinkAction.WorkingDirectory = LinkEnvironment.Config.IntermediateDirectory.FullName;

					// Get link arguments.
					LinkAction.CommandArguments = LinkEnvironment.Config.bIsBuildingLibrary ? GetArArguments(LinkEnvironment) : GetLinkArguments(LinkEnvironment, Arch);

					// Add the output file as a production of the link action.
					FileItem OutputFile;
					OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePaths[OutputPathIndex]);
					Outputs.Add(OutputFile);
					LinkAction.ProducedItems.Add(OutputFile);
					LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));

					// LinkAction.bPrintDebugInfo = true;

					// Add the output file to the command-line.
					if (LinkEnvironment.Config.bIsBuildingLibrary)
					{
						LinkAction.CommandArguments += string.Format(" \"{0}\"", OutputFile.AbsolutePath);
					}
					else
					{
						LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);
					}

					// Add the input files to a response file, and pass the response file on the command-line.
					List<string> InputFileNames = new List<string>();
					foreach (FileItem InputFile in LinkEnvironment.InputFiles)
					{
						// make sure it's for current Arch
						if (Path.GetFileNameWithoutExtension(InputFile.AbsolutePath).EndsWith(Arch + GPUArchitecture))
						{
							string AbsolutePath = InputFile.AbsolutePath.Replace("\\", "/");

							AbsolutePath = AbsolutePath.Replace(LinkEnvironment.Config.IntermediateDirectory.FullName.Replace("\\", "/"), "");
							AbsolutePath = AbsolutePath.TrimStart(new char[] { '/' });

							InputFileNames.Add(string.Format("\"{0}\"", AbsolutePath));
							LinkAction.PrerequisiteItems.Add(InputFile);
						}
					}

					FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
					LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFile.Create(ResponseFileName, InputFileNames));

					// libs don't link in other libs
					if (!LinkEnvironment.Config.bIsBuildingLibrary)
					{
						// Add the library paths to the argument list.
						foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
						{
							// LinkerPaths could be relative or absolute
							string AbsoluteLibraryPath = ActionThread.ExpandEnvironmentVariables(LibraryPath);
							if (IsDirectoryForArch(AbsoluteLibraryPath, Arch))
							{
								// environment variables aren't expanded when using the $( style
								if (Path.IsPathRooted(AbsoluteLibraryPath) == false)
								{
									AbsoluteLibraryPath = Path.Combine(LinkerPath, AbsoluteLibraryPath);
								}
								LinkAction.CommandArguments += string.Format(" -L\"{0}\"", AbsoluteLibraryPath);
							}
						}

						// add libraries in a library group
						LinkAction.CommandArguments += string.Format(" -Wl,--start-group");
						foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
						{
							if (!ShouldSkipLib(AdditionalLibrary, Arch, GPUArchitecture))
							{
								if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
								{
									LinkAction.CommandArguments += string.Format(" \"-l{0}\"", AdditionalLibrary);
								}
								else
								{
									// full pathed libs are compiled by us, so we depend on linking them
									LinkAction.CommandArguments += string.Format(" \"{0}\"", Path.GetFullPath(AdditionalLibrary));
									LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
								}
							}
						}
						LinkAction.CommandArguments += OptionalLinkArguments;
						LinkAction.CommandArguments += string.Format(" -Wl,--end-group");
					}

					// Add the additional arguments specified by the environment.
					LinkAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;
					LinkAction.CommandArguments = LinkAction.CommandArguments.Replace("\\", "/");

					// Only execute linking on the local PC.
					LinkAction.bCanExecuteRemotely = false;
				}
			}

			return Outputs.ToArray();
		}

		public override void ModifyBuildProducts(UEBuildBinary Binary, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			// the binary will have all of the .so's in the output files, we need to trim down to the shared apk (which is what needs to go into the manifest)
			if (Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				foreach (FileReference BinaryPath in Binary.Config.OutputFilePaths)
				{
					FileReference ApkFile = BinaryPath.ChangeExtension(".apk");
					BuildProducts.Add(ApkFile, BuildProductType.Executable);
				}
			}
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, FileReference ProjectFileName, FileReference DestinationFile)
		{
			throw new BuildException("Android cannot compile C# files");
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Log.TraceInformation(Line.Data);
			}
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			File.Copy(SourceFileName, TargetFileName, true);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			if (SourceFileName.Contains("-armv7"))
			{
				StartInfo.FileName = ArPathArm.Replace("-ar.exe", "-strip.exe");
			}
			else
			if (SourceFileName.Contains("-arm64"))
            {
				StartInfo.FileName = ArPathArm64.Replace("-ar.exe", "-strip.exe");
			}
			else
			if (SourceFileName.Contains("-x86"))
            {
				StartInfo.FileName = ArPathx86.Replace("-ar.exe", "-strip.exe");
			}
			else
			if (SourceFileName.Contains("-x64"))
            {
				StartInfo.FileName = ArPathx64.Replace("-ar.exe", "-strip.exe");
			}
			else
			{
				throw new BuildException("Couldn't determine Android architecture to strip symbols from {0}", SourceFileName);
			}
			StartInfo.Arguments = "--strip-debug " + TargetFileName;
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	};
}
