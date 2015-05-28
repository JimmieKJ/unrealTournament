// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	public class AndroidToolChain : UEToolChain
	{
		// the number of the clang version being used to compile 
		static private float ClangVersionFloat = 0;

		// the list of architectures we will compile for
		static private string[] Arches = null;
		// the list of GPU architectures we will compile for
		static private string[] GPUArchitectures = null;
		// a list of all architecture+GPUArchitecture names (-armv7-es2, etc)
		static private string[] AllComboNames = null;

		static private Dictionary<string, string[]> AllArchNames = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { "armv7", "armeabi-v7a", } }, 
			{ "-arm64", new string[] { "arm64", } }, 
			{ "-x86",   new string[] { "x86", } }, 
			{ "-x64",   new string[] { "x64", "x86_64", } }, 
		};

		static private Dictionary<string, string[]> LibrariesToSkip = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { } }, 
			{ "-arm64", new string[] { } }, 
			{ "-x86",   new string[] { "nvToolsExt", } }, 
			{ "-x64",   new string[] { } }, 
		};

		public static void ParseArchitectures()
		{
			// look in ini settings for what platforms to compile for
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
			List<string> ProjectArches = new List<string>();
			bool bBuild = true;
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForArmV7", out bBuild) && bBuild)
			{
				ProjectArches.Add("-armv7");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForArm64", out bBuild) && bBuild)
			{
				ProjectArches.Add("-arm64");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForx86", out bBuild) && bBuild)
			{
				ProjectArches.Add("-x86");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForx8664", out bBuild) && bBuild)
			{
				ProjectArches.Add("-x64");
			}

			// force armv7 if something went wrong
			if (ProjectArches.Count == 0)
			{
				ProjectArches.Add("-armv7");
			}

			Arches = ProjectArches.ToArray();

			// Parse selected GPU architectures
			List<string> ProjectGPUArches = new List<string>();
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForES2", out bBuild) && bBuild)
			{
				ProjectGPUArches.Add("-es2");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForES31", out bBuild) && bBuild)
			{
				ProjectGPUArches.Add("-es31");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForGL4", out bBuild) && bBuild)
			{
				ProjectGPUArches.Add("-gl4");
			}
			if (ProjectGPUArches.Count == 0)
			{
				ProjectGPUArches.Add("-es2");
			}
			GPUArchitectures = ProjectGPUArches.ToArray();

			List<string> FullArchCombinations = new List<string>();
			foreach (string Arch in Arches)
			{
				foreach (string GPUArch in GPUArchitectures)
				{
					FullArchCombinations.Add(Arch + GPUArch);
				}
			}
			AllComboNames = FullArchCombinations.ToArray();
		}

		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			ParseArchitectures();
		}

		static public string[] GetAllArchitectures()
		{
			if (Arches == null)
			{
				ParseArchitectures();
			}

			return Arches;
		}

		static public string[] GetAllGPUArchitectures()
		{
			if (GPUArchitectures == null)
			{
				ParseArchitectures();
			}

			return GPUArchitectures;
		}

		static public string GetNdkApiLevel()
		{
			// ask the .ini system for what version to use
			ConfigCacheIni Ini = new ConfigCacheIni(UnrealTargetPlatform.Android, "Engine", UnrealBuildTool.GetUProjectPath());
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

		static public string GetLargestApiLevel(string[] ApiLevels)
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

		public override void RegisterToolChain()
		{
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");

			// don't register if we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return;
			}

			NDKPath = NDKPath.Replace("\"", "");

			string ClangVersion = "";
			string GccVersion = "";

			// prefer clang 3.5, but fall back if needed for now
			if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.5")))
			{
				ClangVersion = "3.5";
				GccVersion = "4.9";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.3")))
			{
				ClangVersion = "3.3";
				GccVersion = "4.8";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm-3.1")))
			{
				ClangVersion = "3.1";
				GccVersion = "4.6";
			}
			else
			{
				return;
			}

			ClangVersionFloat = float.Parse(ClangVersion, System.Globalization.CultureInfo.InvariantCulture);
			// Console.WriteLine("Compiling with clang {0}", ClangVersionFloat);

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
                Log.TraceVerbose("        Did not find 32 bit or 64 bit versions of toolchain");
                return;
            }

			// set up the path to our toolchains
			ClangPath = Path.Combine(NDKPath, @"toolchains/llvm-" + ClangVersion, ArchitecturePath, @"bin/clang++" + ExeExtension);
			ArPathArm = Path.Combine(NDKPath, @"toolchains/arm-linux-androideabi-" + GccVersion, ArchitecturePath, @"bin/arm-linux-androideabi-ar" + ExeExtension);		//@todo android: use llvm-ar.exe instead?
			ArPathx86 = Path.Combine(NDKPath, @"toolchains/x86-" + GccVersion, ArchitecturePath, @"bin/i686-linux-android-ar" + ExeExtension);							//@todo android: verify x86 toolchain


			// toolchain params
			ToolchainParamsArm = " -target armv7-none-linux-androideabi" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", GetNdkApiLevel(), "arch-arm") + "\"" +
                                   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains/arm-linux-androideabi-" + GccVersion, ArchitecturePath) + "\"";
			ToolchainParamsx86 = " -target i686-none-linux-android" +
								   " --sysroot=\"" + Path.Combine(NDKPath, "platforms", GetNdkApiLevel(), "arch-x86") + "\"" +
                                   " -gcc-toolchain \"" + Path.Combine(NDKPath, @"toolchains/x86-" + GccVersion, ArchitecturePath) + "\"";

			// Register this tool chain
			Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.Android.ToString());
			UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.Android, this);
		}

		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment, string Architecture)
		{
			string Result = "";
			
			Result += (Architecture == "-armv7") ? ToolchainParamsArm : ToolchainParamsx86;

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

			// new for clang4.5 warnings:
			if (ClangVersionFloat >= 3.5)
			{
				Result += " -Wno-undefined-bool-conversion"; // 'this' pointer cannot be null in well-defined C++ code; pointer may be assumed to always convert to true (if (this))

			}

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
			}

			// debug info
            if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				Result += " -g2 -gdwarf-2";
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

		static string GetLinkArguments(LinkEnvironment LinkEnvironment, string Architecture)
		{
			string Result = "";

			Result += " -nostdlib";
			Result += " -Wl,-shared,-Bsymbolic";
			Result += " -Wl,--no-undefined";

            if (Architecture == "-armv7")
			{
				Result += ToolchainParamsArm;
				Result += " -march=armv7-a";
				Result += " -Wl,--fix-cortex-a8";		// required to route around a CPU bug in some Cortex-A8 implementations
			}
			else if (Architecture == "-x86")
			{
				Result += ToolchainParamsx86;
				Result += " -march=atom";
			}

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

		public static void CompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{

				bool bWasHandled = false;
				// does it look like an error? something like this:
				//     2>Core/Inc/UnStats.h:478:3: error : no matching constructor for initialization of 'FStatCommonData'

				try
				{
					if (!Line.Data.StartsWith(" ") && !Line.Data.StartsWith(","))
					{
						// if we split on colon, an error will have at least 4 tokens
						string[] Tokens = Line.Data.Split("(".ToCharArray());
						if (Tokens.Length > 1)
						{
							// make sure what follows the parens is what we expect
							string Filename = Path.GetFullPath(Tokens[0]);
							// build up the final string
							string Output = string.Format("{0}({1}", Filename, Tokens[1], Line.Data[0]);
							for (int T = 3; T < Tokens.Length; T++)
							{
								Output += Tokens[T];
								if (T < Tokens.Length - 1)
								{
									Output += "(";
								}
							}

							// output the result
							Log.TraceInformation(Output);
							bWasHandled = true;
						}
					}
				}
				catch (Exception)
				{
					bWasHandled = false;
				}

				// write if not properly handled
				if (!bWasHandled)
				{
					Log.TraceWarning("{0}", Line.Data);
				}
			}
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
						if (Dir.Contains("/" + ArchName))
						{
							return false;
						}
					}
				}
			}

			// if nothing was found, we are okay
			return true;
		}

		static bool ShouldSkipLib(string Lib, string Arch, string GPUArchitecture)
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
				SourceFiles.Add(FileItem.GetItemByPath(Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/cpufeatures/cpu-features.c"));
			}
		}

		// cache the location of NDK tools
		static string ClangPath;
		static string ToolchainParamsArm;
		static string ToolchainParamsx86;
		static string ArPathArm;
		static string ArPathx86;

		static private bool bHasPrintedApiLevel = false;
		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			if (Arches.Length == 0)
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

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				BaseArguments += string.Format(" -D \"{0}\"", Definition);
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);



			string BasePCHName = "";
			var PCHExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Android].GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				BasePCHName = RemoveArchName(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath).Replace(PCHExtension, "");
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (string Arch in Arches)
			{
				foreach (string GPUArchitecture in GPUArchitectures)
				{
					// which toolchain to use
					string Arguments = GetCLArguments_Global(CompileEnvironment, Arch) + BaseArguments;

					if (GPUArchitecture == "-gl4")
					{
						Arguments += " -DPLATFORM_ANDROIDGL4=1";
					}
					else if (GPUArchitecture == "-es31")
					{
						Arguments += " -DPLATFORM_ANDROIDES31=1";
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
							FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
								Path.Combine(
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

							var ObjectFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Android].GetBinaryExtension(UEBuildBinaryType.Object);

							// Add the object file to the produced item list.
							FileItem ObjectFile = FileItem.GetItemByPath(
								InlineArchName( Path.Combine(
									CompileEnvironment.Config.OutputDirectory,
									Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension), Arch, GPUArchitecture
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

						// Create the response file
						string ResponseFileName = CompileAction.ProducedItems[0].AbsolutePath + ".response";
						string ResponseArgument = string.Format("@\"{0}\"", ResponseFile.Create(ResponseFileName, new List<string> { AllArguments }));

						CompileAction.WorkingDirectory = Path.GetFullPath(".");
						CompileAction.CommandPath = ClangPath;
						CompileAction.CommandArguments = ResponseArgument;
						CompileAction.StatusDescription = string.Format("{0} [{1}-{2}]", Path.GetFileName(SourceFile.AbsolutePath), Arch.Replace("-", ""), GPUArchitecture.Replace("-", ""));

						CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);

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

		static public string RemoveArchName(string Pathname)
		{
			// remove all architecture names
			foreach (string Arch in Arches)
			{
				foreach (string GPUArchitecture in GPUArchitectures)
				{
					Pathname = Path.Combine(Path.GetDirectoryName(Pathname), Path.GetFileName(Pathname).Replace(Arch+GPUArchitecture, ""));
				}
			}
			return Pathname;
		}

		public override FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			List<FileItem> Outputs = new List<FileItem>();

			for (int ArchIndex = 0; ArchIndex < Arches.Length; ArchIndex++)
			{
				string Arch = Arches[ArchIndex];
				for (int GPUArchIndex = 0; GPUArchIndex < GPUArchitectures.Length; GPUArchIndex++)
				{
					string GPUArchitecture = GPUArchitectures[GPUArchIndex];
					int OutputPathIndex = ArchIndex * GPUArchitectures.Length + GPUArchIndex;

					// Android will have an array of outputs
					if (LinkEnvironment.Config.OutputFilePaths.Length < OutputPathIndex ||
						!Path.GetFileNameWithoutExtension(LinkEnvironment.Config.OutputFilePaths[OutputPathIndex]).EndsWith(Arch + GPUArchitecture))
					{
						throw new BuildException("The OutputFilePaths array didn't match the Arches array in AndroidToolChain.LinkAllFiles");
					}

					// Create an action that invokes the linker.
					Action LinkAction = new Action(ActionType.Link);
					LinkAction.WorkingDirectory = Path.GetFullPath(".");

					if (LinkEnvironment.Config.bIsBuildingLibrary)
					{
						LinkAction.CommandPath = Arch == "-armv7" ? ArPathArm : ArPathx86;
					}
					else
					{
						LinkAction.CommandPath = ClangPath;
					}

					string LinkerPath = LinkAction.WorkingDirectory;

					LinkAction.WorkingDirectory = LinkEnvironment.Config.IntermediateDirectory;

					// Get link arguments.
					LinkAction.CommandArguments = LinkEnvironment.Config.bIsBuildingLibrary ? GetArArguments(LinkEnvironment) : GetLinkArguments(LinkEnvironment, Arch);

					// Add the output file as a production of the link action.
					FileItem OutputFile;
					OutputFile = FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePaths[OutputPathIndex]);
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

							AbsolutePath = AbsolutePath.Replace(LinkEnvironment.Config.IntermediateDirectory.Replace("\\", "/"), "");
							AbsolutePath = AbsolutePath.TrimStart(new char[] { '/' });

							InputFileNames.Add(string.Format("\"{0}\"", AbsolutePath));
							LinkAction.PrerequisiteItems.Add(InputFile);
						}
					}

					string ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
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

		public override void AddFilesToReceipt(BuildReceipt Receipt, UEBuildBinary Binary)
		{
			// the binary will have all of the .so's in the output files, we need to trim down to the shared apk (which is what needs to go into the manifest)
			if (Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				foreach (string BinaryPath in Binary.Config.OutputFilePaths)
				{
					string ApkFile = Path.ChangeExtension(BinaryPath, ".apk");
					Receipt.AddBuildProduct(ApkFile, BuildProductType.Executable);
				}
			}
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile)
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

		public override UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.Android;
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			File.Copy(SourceFileName, TargetFileName, true);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			if(SourceFileName.Contains("-armv7"))
			{
				StartInfo.FileName = ArPathArm.Replace("-ar.exe", "-strip.exe");
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
