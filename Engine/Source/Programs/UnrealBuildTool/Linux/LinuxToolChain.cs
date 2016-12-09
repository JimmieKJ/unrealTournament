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
	class LinuxToolChain : UEToolChain
	{
		LinuxPlatformContext PlatformContext;

		public LinuxToolChain(LinuxPlatformContext InPlatformContext) 
			: base(CPPTargetPlatform.Linux)
		{
			PlatformContext = InPlatformContext;
			if (!CrossCompiling())
			{
				// use native linux toolchain
				string[] ClangNames = { "clang++", "clang++-3.9", "clang++-3.8", "clang++-3.7", "clang++-3.6", "clang++-3.5" };
				foreach (var ClangName in ClangNames)
				{
					ClangPath = Which(ClangName);
					if (!String.IsNullOrEmpty(ClangPath))
					{
						break;
					}
				}
				GCCPath = Which("g++");
				ArPath = Which("ar");
				RanlibPath = Which("ranlib");
				StripPath = Which("strip");

				// if clang is available, zero out gcc (@todo: support runtime switching?)
				if (!String.IsNullOrEmpty(ClangPath))
				{
					GCCPath = null;
				}
			}
			else
			{
				// if new multi-arch toolchain is used, prefer it
				string MultiArchRoot = Environment.GetEnvironmentVariable("LINUX_MULTIARCH_ROOT");

				if (MultiArchRoot != null)
				{
					// FIXME: UBT should loop across all the architectures and compile for all the selected ones.
					BaseLinuxPath = Path.Combine(MultiArchRoot, PlatformContext.GetActiveArchitecture());

					Console.WriteLine("Using LINUX_MULTIARCH_ROOT, building with toolchain '{0}'", BaseLinuxPath);
				}
				else
				{
					// use cross linux toolchain if LINUX_ROOT is specified
					BaseLinuxPath = Environment.GetEnvironmentVariable("LINUX_ROOT");

					Console.WriteLine("Using LINUX_ROOT (deprecated, consider LINUX_MULTIARCH_ROOT), building with toolchain '{0}'", BaseLinuxPath);
				}

				// don't register if we don't have an LINUX_ROOT specified
				if (String.IsNullOrEmpty(BaseLinuxPath))
				{
					throw new BuildException("LINUX_ROOT environment variable is not set; cannot instantiate Linux toolchain");
				}

				BaseLinuxPath = BaseLinuxPath.Replace("\"", "");

				// set up the path to our toolchains
				GCCPath = "";
				ClangPath = Path.Combine(BaseLinuxPath, @"bin\clang++.exe");
				// ar and ranlib will be switched later to match the architecture
				ArPath = "ar.exe";
				RanlibPath = "ranlib.exe";
				StripPath = "strip.exe";
			}

			if (!DetermineCompilerVersion())
			{
				throw new BuildException("Could not determine version of the compiler, not registering Linux toolchain.");
			}

			// refuse to use compilers that we know won't work
			// disable that only if you are a dev and you know what you are doing
			if (!UsingClang())
			{
				throw new BuildException("This version of the engine can only be compiled by clang - refusing to register the Linux toolchain.");
			}
			else if (CompilerVersionMajor == 3 && CompilerVersionMinor == 4)
			{
				throw new BuildException("clang 3.4.x is known to miscompile the engine - refusing to register the Linux toolchain.");
			}
			// prevent unknown clangs since the build is likely to fail on too old or too new compilers
			else if ((CompilerVersionMajor * 10 + CompilerVersionMinor) > 39 || (CompilerVersionMajor * 10 + CompilerVersionMinor) < 35)
			{
				throw new BuildException(
					string.Format("This version of the Unreal Engine can only be compiled with clang 3.9, 3.8, 3.7, 3.6 and 3.5. clang {0} may not build it - please use a different version.",
						CompilerVersionString)
					);
			}
		}

		protected static bool CrossCompiling()
		{
			return BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux;
		}

		protected static bool UsingClang()
		{
			return !String.IsNullOrEmpty(ClangPath);
		}

		private string Which(string name)
		{
			Process proc = new Process();
			proc.StartInfo.FileName = "/bin/sh";
			proc.StartInfo.Arguments = String.Format("-c 'which {0}'", name);
			proc.StartInfo.UseShellExecute = false;
			proc.StartInfo.CreateNoWindow = true;
			proc.StartInfo.RedirectStandardOutput = true;
			proc.StartInfo.RedirectStandardError = true;

			proc.Start();
			proc.WaitForExit();

			string path = proc.StandardOutput.ReadLine();
			Log.TraceVerbose(String.Format("which {0} result: ({1}) {2}", name, proc.ExitCode, path));

			if (proc.ExitCode == 0 && String.IsNullOrEmpty(proc.StandardError.ReadToEnd()))
			{
				return path;
			}
			return null;
		}

		/// <summary>
		/// Splits compiler version string into numerical components, leaving unchanged if not known
		/// </summary>
		private void DetermineCompilerMajMinPatchFromVersionString()
		{
			string[] Parts = CompilerVersionString.Split('.');
			if (Parts.Length >= 1)
			{
				CompilerVersionMajor = Convert.ToInt32(Parts[0]);
			}
			if (Parts.Length >= 2)
			{
				CompilerVersionMinor = Convert.ToInt32(Parts[1]);
			}
			if (Parts.Length >= 3)
			{
				CompilerVersionPatch = Convert.ToInt32(Parts[2]);
			}
		}

		/// <summary>
		/// Queries compiler for the version
		/// </summary>
		private bool DetermineCompilerVersion()
		{
			CompilerVersionString = null;
			CompilerVersionMajor = -1;
			CompilerVersionMinor = -1;
			CompilerVersionPatch = -1;

			Process Proc = new Process();
			Proc.StartInfo.UseShellExecute = false;
			Proc.StartInfo.CreateNoWindow = true;
			Proc.StartInfo.RedirectStandardOutput = true;
			Proc.StartInfo.RedirectStandardError = true;

			if (!String.IsNullOrEmpty(GCCPath) && File.Exists(GCCPath))
			{
				Proc.StartInfo.FileName = GCCPath;
				Proc.StartInfo.Arguments = " -dumpversion";

				Proc.Start();
				Proc.WaitForExit();

				if (Proc.ExitCode == 0)
				{
					// read just the first string
					CompilerVersionString = Proc.StandardOutput.ReadLine();
					DetermineCompilerMajMinPatchFromVersionString();
				}
			}
			else if (!String.IsNullOrEmpty(ClangPath) && File.Exists(ClangPath))
			{
				Proc.StartInfo.FileName = ClangPath;
				Proc.StartInfo.Arguments = " --version";

				Proc.Start();
				Proc.WaitForExit();

				if (Proc.ExitCode == 0)
				{
					// read just the first string
					string VersionString = Proc.StandardOutput.ReadLine();

					Regex VersionPattern = new Regex("version \\d+(\\.\\d+)+");
					Match VersionMatch = VersionPattern.Match(VersionString);

					// version match will be like "version 3.3", so remove the "version"
					if (VersionMatch.Value.StartsWith("version "))
					{
						CompilerVersionString = VersionMatch.Value.Replace("version ", "");

						DetermineCompilerMajMinPatchFromVersionString();
					}
				}
			}
			else
			{
				// icl?
			}

			if (!CrossCompiling() && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				Console.WriteLine("Using {0} version '{1}' (string), {2} (major), {3} (minor), {4} (patch)",
					String.IsNullOrEmpty(ClangPath) ? "gcc" : "clang",
					CompilerVersionString, CompilerVersionMajor, CompilerVersionMinor, CompilerVersionPatch);
			}
			return !String.IsNullOrEmpty(CompilerVersionString);
		}

		/// <summary>
		/// Checks if compiler version matches the requirements
		/// </summary>
		private static bool CompilerVersionGreaterOrEqual(int Major, int Minor, int Patch)
		{
			return CompilerVersionMajor > Major ||
				(CompilerVersionMajor == Major && CompilerVersionMinor > Minor) ||
				(CompilerVersionMajor == Major && CompilerVersionMinor == Minor && CompilerVersionPatch >= Patch);
		}

		/// <summary>
		/// Architecture-specific compiler switches
		/// </summary>
		static string ArchitectureSpecificSwitches(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("arm") || Architecture.StartsWith("aarch64"))
			{
				Result += " -fsigned-char";
			}

			return Result;
		}

		static string ArchitectureSpecificDefines(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64"))
			{
				Result += " -D_LINUX64";
			}

			return Result;
		}

		/// <summary>
		/// Gets architecture-specific ar paths
		/// </summary>
		private static string GetArPath(string Architecture)
		{
			if (CrossCompiling())
			{
				return Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, ArPath)));
			}

			return ArPath;
		}

		/// <summary>
		/// Gets architecture-specific ranlib paths
		/// </summary>
		private static string GetRanlibPath(string Architecture)
		{
			if (CrossCompiling())
			{
				return Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, RanlibPath)));
			}

			return RanlibPath;
		}

		/// <summary>
		/// Gets architecture-specific strip path
		/// </summary>
		private static string GetStripPath(string Architecture)
		{
			if (CrossCompiling())
			{
				return Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, StripPath)));
			}

			return StripPath;
		}

		private static bool ShouldUseLibcxx(string Architecture)
		{
			// set UE4_LINUX_USE_LIBCXX to either 0 or 1. If unset, defaults to 1.
			string UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE4_LINUX_USE_LIBCXX");
			if (UseLibcxxEnvVarOverride != null && (UseLibcxxEnvVarOverride == "1"))
			{
				return true;
			}

			// at the moment only x86_64 is supported
			return Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64");
		}

		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			// build up the commandline common to C and C++
			Result += " -c";
			Result += " -pipe";

			if (ShouldUseLibcxx(CompileEnvironment.Config.Architecture))
			{
				Result += " -nostdinc++";
				Result += " -I" + UEBuildConfiguration.UEThirdPartySourceDirectory + "Linux/LibCxx/include/";
				Result += " -I" + UEBuildConfiguration.UEThirdPartySourceDirectory + "Linux/LibCxx/include/c++/v1";
			}

			Result += " -Wall -Werror";
			// test without this next line?
			Result += " -funwind-tables";               // generate unwind tables as they seem to be needed for stack tracing (why??)
			Result += " -Wsequence-point";              // additional warning not normally included in Wall: warns if order of operations is ambigious
			//Result += " -Wunreachable-code";            // additional warning not normally included in Wall: warns if there is code that will never be executed - not helpful due to bIsGCC and similar 
			//Result += " -Wshadow";                      // additional warning not normally included in Wall: warns if there variable/typedef shadows some other variable - not helpful because we have gobs of code that shadows variables

			Result += ArchitectureSpecificSwitches(CompileEnvironment.Config.Architecture);

			Result += " -fno-math-errno";               // do not assume that math ops have side effects

			Result += GetRTTIFlag(CompileEnvironment);	// flag for run-time type info

			if (String.IsNullOrEmpty(ClangPath))
			{
				// GCC only option
				Result += " -fno-strict-aliasing";
				Result += " -Wno-sign-compare"; // needed to suppress: comparison between signed and unsigned integer expressions
				Result += " -Wno-enum-compare"; // Stats2.h triggers this (ALIGNOF(int64) <= DATA_ALIGN)
				Result += " -Wno-return-type"; // Variant.h triggers this
				Result += " -Wno-unused-local-typedefs";
				Result += " -Wno-multichar";
				Result += " -Wno-unused-but-set-variable";
				Result += " -Wno-strict-overflow"; // Array.h:518
			}
			else
			{
				// Clang only options
				if (CrossCompiling())
				{
					Result += " -fdiagnostics-format=msvc";     // make diagnostics compatible with MSVC when cross-compiling
				}
				Result += " -Wno-unused-private-field";     // MultichannelTcpSocket.h triggers this, possibly more
				// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
				Result += " -Wno-tautological-compare";

				// this switch is understood by clang 3.5.0, but not clang-3.5 as packaged by Ubuntu 14.04 atm
				if (CompilerVersionGreaterOrEqual(3, 5, 0))
				{
					Result += " -Wno-undefined-bool-conversion";	// hides checking if 'this' pointer is null
				}

				if (CompilerVersionGreaterOrEqual(3, 6, 0))
				{
					Result += " -Wno-unused-local-typedef";	// clang is being overly strict here? PhysX headers trigger this.
					Result += " -Wno-inconsistent-missing-override";	// these have to be suppressed for UE 4.8, should be fixed later.
				}

				if (CompilerVersionGreaterOrEqual(3, 9, 0))
				{
					Result += " -Wno-undefined-var-template"; // not really a good warning to disable
				}
			}

			Result += " -Wno-unused-variable";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			Result += " -Wno-unknown-pragmas";			// Slate triggers this (with its optimize on/off pragmas)
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			if (CompileEnvironment.Config.bEnableShadowVariableWarning)
			{
				Result += " -Wshadow" + (BuildConfiguration.bShadowVariableErrors ? "" : " -Wno-error=shadow");
			}

			//Result += " -DOPERATOR_NEW_INLINE=FORCENOINLINE";

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
				Result += " -fomit-frame-pointer";
			}
			// switches to help debugging
			else if (CompileEnvironment.Config.Configuration == CPPTargetConfiguration.Debug)
			{
				Result += " -fno-inline";                   // disable inlining for better debuggability (e.g. callstacks, "skip file" in gdb)
				Result += " -fno-omit-frame-pointer";       // force not omitting fp
				Result += " -fstack-protector";             // detect stack smashing
				//Result += " -fsanitize=address";            // detect address based errors (support properly and link to libasan)
			}

			// debug info 
			// bCreateDebugInfo is normally set for all configurations, including Shipping - this is needed to enable callstack in Shipping builds (proper resolution: UEPLAT-205, separate files with debug info)
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				// libdwarf (from elftoolchain 0.6.1) doesn't support DWARF4
				Result += " -gdwarf-3";
			}

			// optimization level
			if (!CompileEnvironment.Config.bOptimizeCode)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O2";	// warning: as of now (2014-09-28), clang 3.5.0 miscompiles PlatformerGame with -O3 (bitfields?)
			}

			if (CompileEnvironment.Config.bIsBuildingDLL)
			{
				Result += " -fPIC";
				// Use local-dynamic TLS model. This generates less efficient runtime code for __thread variables, but avoids problems of running into
				// glibc/ld.so limit (DTV_SURPLUS) for number of dlopen()'ed DSOs with static TLS (see e.g. https://www.cygwin.com/ml/libc-help/2013-11/msg00033.html)
				Result += " -ftls-model=local-dynamic";
			}

			if (CompileEnvironment.Config.bEnableExceptions || UEBuildConfiguration.bForceEnableExceptions)
			{
				Result += " -fexceptions";
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=0";
			}
			else
			{
				Result += " -fno-exceptions";               // no exceptions
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=1";
			}

			//Result += " -v";                            // for better error diagnosis

			Result += ArchitectureSpecificDefines(CompileEnvironment.Config.Architecture);
			if (CrossCompiling())
			{
				if (UsingClang())
				{
					Result += String.Format(" -target {0}", CompileEnvironment.Config.Architecture);        // Set target triple
				}
				Result += String.Format(" --sysroot=\"{0}\"", BaseLinuxPath);
			}

			return Result;
		}

		/// <summary>
		/// Sanitizes a definition argument if needed.
		/// </summary>
		/// <param name="definition">A string in the format "foo=bar".</param>
		/// <returns></returns>
		internal static string EscapeArgument(string definition)
		{
			string[] splitData = definition.Split('=');
			string myKey = splitData.ElementAtOrDefault(0);
			string myValue = splitData.ElementAtOrDefault(1);

			if (string.IsNullOrEmpty(myKey)) { return ""; }
			if (!string.IsNullOrEmpty(myValue))
			{
				if (!myValue.StartsWith("\"") && (myValue.Contains(" ") || myValue.Contains("$")))
				{
					myValue = myValue.Trim('\"');		// trim any leading or trailing quotes
					myValue = "\"" + myValue + "\"";	// ensure wrap string with double quotes
				}

				// replace double quotes to escaped double quotes if exists
				myValue = myValue.Replace("\"", "\\\"");
			}

			return myValue == null
				? string.Format("{0}", myKey)
				: string.Format("{0}={1}", myKey, myValue);
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x c++";
			Result += " -std=c++11";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++11";
			return Result;
		}

		// Conditionally enable (default disabled) generation of information about every class with virtual functions for use by the C++ runtime type identification features 
		// (`dynamic_cast' and `typeid'). If you don't use those parts of the language, you can save some space by using -fno-rtti. 
		// Note that exception handling uses the same information, but it will generate it as needed. 
		static string GetRTTIFlag(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			if (CompileEnvironment.Config.bUseRTTI)
			{
				Result = " -frtti";
			}
			else
			{
				Result = " -fno-rtti";
			}

			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++11";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x c++-header";
			Result += " -std=c++11";
			return Result;
		}

		string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// debugging symbols
			// Applying to all configurations @FIXME: temporary hack for FN to enable callstack in Shipping builds (proper resolution: UEPLAT-205)
			Result += " -rdynamic";   // needed for backtrace_symbols()...

			if (LinkEnvironment.Config.bIsBuildingDLL)
			{
				Result += " -shared";
			}
			else
			{
				// ignore unresolved symbols in shared libs
				Result += string.Format(" -Wl,--unresolved-symbols=ignore-in-shared-libs");
			}

			// RPATH for third party libs
			Result += " -Wl,-rpath=${ORIGIN}";
			Result += " -Wl,-rpath-link=${ORIGIN}";
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/Linux";
			Result += " -Wl,-rpath=${ORIGIN}/..";	// for modules that are in sub-folders of the main Engine/Binary/Linux folder
			// FIXME: really ugly temp solution. Modules need to be able to specify this
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Steamworks/Steamv132/Linux";
			if (LinkEnvironment.Config.Architecture.StartsWith("x86_64"))
			{
				Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Qualcomm/Linux";
			}
			else
			{
				// x86_64 is now using updated ICU that doesn't need extra .so
				Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Linux/" + LinkEnvironment.Config.Architecture;
			}
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/OpenAL/Linux/" + LinkEnvironment.Config.Architecture;
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/CEF3/Linux";

			// Some OS ship ld with new ELF dynamic tags, which use DT_RUNPATH vs DT_RPATH. Since DT_RUNPATH do not propagate to dlopen()ed DSOs,
			// this breaks the editor on such systems. See https://kenai.com/projects/maxine/lists/users/archive/2011-01/message/12 for details
			Result += " -Wl,--disable-new-dtags";

			// This severely improves dynamic linker performance, but affects iteration times. Do not do it in Debug,
			// because taking a hit of several tens of seconds on startup is better than linking for ~4 more minutes when iterating
			if (LinkEnvironment.Config.Configuration != CPPTargetConfiguration.Debug)
			{
				Result += " -Wl,--as-needed";
			}
			// Additionally speeds up editor startup by 1-2s
			Result += " -Wl,--hash-style=gnu";

			if (CrossCompiling())
			{
				if (UsingClang())
				{
					Result += String.Format(" -target {0}", LinkEnvironment.Config.Architecture);        // Set target triple
				}
				string SysRootPath = BaseLinuxPath.TrimEnd(new char[] { '\\', '/' });
				Result += String.Format(" \"--sysroot={0}\"", SysRootPath);
			}

			return Result;
		}

		static string GetArchiveArguments(LinkEnvironment LinkEnvironment)
		{
			return " rc";
		}

		public static void CrossCompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs e)
		{
			Debug.Assert(CrossCompiling());

			string Output = e.Data;
			if (String.IsNullOrEmpty(Output))
			{
				return;
			}

			// format the string so the output errors are clickable in Visual Studio

			// Need to match following for clickable links
			string RegexFilePath = @"^[A-Z]\:([\\\/][A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
			string RegexLineNumber = @"\:\d+\:\d+\:";
			string RegexDescription = @"(\serror:\s|\swarning:\s|\snote:\s).*";

			// Get Matches
			string MatchFilePath = Regex.Match(Output, RegexFilePath).Value.Replace("Engine\\Source\\..\\..\\", "");
			string MatchLineNumber = Regex.Match(Output, RegexLineNumber).Value;
			string MatchDescription = Regex.Match(Output, RegexDescription).Value;

			// If any of the above matches failed, do nothing
			if (MatchFilePath.Length == 0 ||
				MatchLineNumber.Length == 0 ||
				MatchDescription.Length == 0)
			{
				Console.WriteLine(Output);
				return;
			}

			// Convert Path
			string RegexStrippedPath = @"\\Engine\\.*"; //@"(Engine\/|[A-Za-z0-9_\-\.]*\/).*";
			string ConvertedFilePath = Regex.Match(MatchFilePath, RegexStrippedPath).Value;
			ConvertedFilePath = Path.GetFullPath("..\\.." + ConvertedFilePath);

			// Extract Line + Column Number
			string ConvertedLineNumber = Regex.Match(MatchLineNumber, @"\d+").Value;
			string ConvertedColumnNumber = Regex.Match(MatchLineNumber, @"(?<=:\d+:)\d+").Value;

			// Write output
			string ConvertedExpression = "  " + ConvertedFilePath + "(" + ConvertedLineNumber + "," + ConvertedColumnNumber + "):" + MatchDescription;
			Console.WriteLine(ConvertedExpression); // To create clickable vs link
		}

		// cache the location of NDK tools
		static string BaseLinuxPath;
		static string ClangPath;
		static string GCCPath;
		static string ArPath;
		static string RanlibPath;
		static string StripPath;

		/// <summary>
		/// Version string of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static string CompilerVersionString;
		/// <summary>
		/// Major version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionMajor = -1;
		/// <summary>
		/// Minor version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionMinor = -1;
		/// <summary>
		/// Patch version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionPatch = -1;

		/// <summary>
		/// Track which scripts need to be deleted before appending to
		/// </summary>
		private bool bHasWipedFixDepsScript = false;

		/// <summary>
		/// Tracks that information about used C++ library is only printed once
		/// </summary>
		private bool bHasPrintedLibcxxInformation = false;

		private static List<FileItem> BundleDependencies = new List<FileItem>();

		public override CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (!bHasPrintedLibcxxInformation)
			{
				// inform the user which C++ library the engine is going to be compiled against - important for compatibility with third party code that uses STL
				bool bUseLibCxx = ShouldUseLibcxx(CompileEnvironment.Config.Architecture);
				Console.WriteLine("Using {0} standard C++ library.", bUseLibCxx ? "bundled libc++" : "compiler default (most likely libstdc++)");
				bHasPrintedLibcxxInformation = true;
			}

			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so Clang can find it.
				// This needs to be before the other include paths to ensure Clang uses it instead of the source header file.
				var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Linux).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				PCHArguments += string.Format(" -include \"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PrecompiledFileExtension, ""));
			}

			foreach(FileReference ForceIncludeFile in CompileEnvironment.Config.ForceIncludeFiles)
			{
				PCHArguments += String.Format(" -include \"{0}\"", ForceIncludeFile.FullName);
			}

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				Arguments += string.Format(" -D \"{0}\"", EscapeArgument(Definition));
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Platform);

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				// Add C or C++ specific compiler arguments.
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					FileArguments += GetCompileArguments_PCH();
				}
				else if (Extension == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else if (Extension == ".CC")
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
				}
				else if (Extension == ".MM")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_MM();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					FileArguments += GetCompileArguments_CPP();

					// only use PCH for .cpp files
					FileArguments += PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Linux).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
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
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}

					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Linux).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);

					FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath, false);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				if (!UsingClang())
				{
					CompileAction.CommandPath = GCCPath;
				}
				else
				{
					CompileAction.CommandPath = ClangPath;
				}
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.bIsGCCCompiler = true;

				// Don't farm out creation of pre-compiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					BuildConfiguration.bAllowRemotelyCompiledPCHs;

				// piping output through the handler during native builds is unnecessary and reportedly causes problems with tools like octobuild.
				if (CrossCompiling())
				{
					CompileAction.OutputEventHandler = new DataReceivedEventHandler(CrossCompileOutputReceivedDataEventHandler);
				}
			}

			return Result;
		}

		/// <summary>
		/// Creates an action to archive all the .o files into single .a file
		/// </summary>
		public FileItem CreateArchiveAndIndex(LinkEnvironment LinkEnvironment, ActionGraph ActionGraph)
		{
			// Create an archive action
			Action ArchiveAction = ActionGraph.Add(ActionType.Link);
			ArchiveAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			bool bUsingSh = BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win64 && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Win32;
			if (bUsingSh)
			{
				ArchiveAction.CommandPath = "/bin/sh";
				ArchiveAction.CommandArguments = "-c '";
			}
			else
			{
				ArchiveAction.CommandPath = "cmd.exe";
				ArchiveAction.CommandArguments = "/c \"";
			}

			// this will produce a final library
			ArchiveAction.bProducesImportLibrary = true;

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
			ArchiveAction.ProducedItems.Add(OutputFile);
			ArchiveAction.CommandDescription = "Archive";
			ArchiveAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			ArchiveAction.CommandArguments += string.Format("\"{0}\" {1} \"{2}\"", GetArPath(LinkEnvironment.Config.Architecture), GetArchiveArguments(LinkEnvironment), OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string InputAbsolutePath = InputFile.AbsolutePath.Replace("\\", "/");
				InputFileNames.Add(string.Format("\"{0}\"", InputAbsolutePath));
				ArchiveAction.PrerequisiteItems.Add(InputFile);
				ArchiveAction.CommandArguments += string.Format(" \"{0}\"", InputAbsolutePath);
			}

			// add ranlib
			ArchiveAction.CommandArguments += string.Format(" && \"{0}\" \"{1}\"", GetRanlibPath(LinkEnvironment.Config.Architecture), OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			ArchiveAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;
			ArchiveAction.CommandArguments.Replace("\\", "/");

			if (bUsingSh)
			{
				ArchiveAction.CommandArguments += "'";
			}
			else
			{
				ArchiveAction.CommandArguments += "\"";
			}

			// Only execute linking on the local PC.
			ArchiveAction.bCanExecuteRemotely = false;

			// piping output through the handler during native builds is unnecessary and reportedly causes problems with tools like octobuild.
			if (CrossCompiling())
			{
				ArchiveAction.OutputEventHandler = new DataReceivedEventHandler(CrossCompileOutputReceivedDataEventHandler);
			}

			return OutputFile;
		}

		public FileItem FixDependencies(LinkEnvironment LinkEnvironment, FileItem Executable, ActionGraph ActionGraph)
		{
			if (!LinkEnvironment.Config.bIsCrossReferenced)
			{
				return null;
			}

			Log.TraceVerbose("Adding postlink step");

			bool bUseCmdExe = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32;
			string ShellBinary = bUseCmdExe ? "cmd.exe" : "/bin/sh";
			string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x
			string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

			FileItem FixDepsScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, ScriptName));

			Action PostLinkAction = ActionGraph.Add(ActionType.Link);
			PostLinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			PostLinkAction.CommandPath = ShellBinary;
			PostLinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(Executable.AbsolutePath));
			PostLinkAction.CommandDescription = "FixDeps";
			PostLinkAction.bCanExecuteRemotely = false;
			PostLinkAction.CommandArguments = ExecuteSwitch;

			PostLinkAction.CommandArguments += bUseCmdExe ? " \"" : " -c '";

			FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));

			// Make sure we don't run this script until the all executables and shared libraries
			// have been built.
			PostLinkAction.PrerequisiteItems.Add(Executable);
			foreach (FileItem Dependency in BundleDependencies)
			{
				PostLinkAction.PrerequisiteItems.Add(Dependency);
			}

			PostLinkAction.CommandArguments += ShellBinary + ExecuteSwitch + " \"" + FixDepsScript.AbsolutePath + "\" && ";

			// output file should not be empty or it will be rebuilt next time
			string Touch = bUseCmdExe ? "echo \"Dummy\" >> \"{0}\" && copy /b \"{0}\" +,," : "echo \"Dummy\" >> \"{0}\"";

			PostLinkAction.CommandArguments += String.Format(Touch, OutputFile.AbsolutePath);
			PostLinkAction.CommandArguments += bUseCmdExe ? "\"" : "'";

			System.Console.WriteLine("{0} {1}", PostLinkAction.CommandPath, PostLinkAction.CommandArguments);

			// piping output through the handler during native builds is unnecessary and reportedly causes problems with tools like octobuild.
			if (CrossCompiling())
			{
				PostLinkAction.OutputEventHandler = new DataReceivedEventHandler(CrossCompileOutputReceivedDataEventHandler);
			}

			PostLinkAction.ProducedItems.Add(OutputFile);
			return OutputFile;
		}


		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			Debug.Assert(!bBuildImportLibraryOnly);

			List<string> RPaths = new List<string>();

			if (LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly)
			{
				return CreateArchiveAndIndex(LinkEnvironment, ActionGraph);
			}

			// Create an action that invokes the linker.
			Action LinkAction = ActionGraph.Add(ActionType.Link);
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			if (String.IsNullOrEmpty(ClangPath))
			{
				LinkAction.CommandPath = GCCPath;
			}
			else
			{
				LinkAction.CommandPath = ClangPath;
			}

			// Get link arguments.
			LinkAction.CommandArguments = GetLinkArguments(LinkEnvironment);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = LinkEnvironment.Config.bIsBuildingDLL;

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.CommandDescription = "Link";
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath.Replace("\\", "/")));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
			LinkAction.CommandArguments += string.Format(" -Wl,@\"{0}\"", ResponseFile.Create(ResponseFileName, InputFileNames));

			if (LinkEnvironment.Config.bIsBuildingDLL)
			{
				LinkAction.CommandArguments += string.Format(" -Wl,-soname={0}", OutputFile);
			}

			// Start with the configured LibraryPaths and also add paths to any libraries that
			// we depend on (libraries that we've build ourselves).
			List<string> AllLibraryPaths = LinkEnvironment.Config.LibraryPaths;
			foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
			{
				string PathToLib = Path.GetDirectoryName(AdditionalLibrary);
				if (!String.IsNullOrEmpty(PathToLib))
				{
					// make path absolute, because FixDependencies script may be executed in a different directory
					string AbsolutePathToLib = Path.GetFullPath(PathToLib);
					if (!AllLibraryPaths.Contains(AbsolutePathToLib))
					{
						AllLibraryPaths.Add(AbsolutePathToLib);
					}
				}

				if ((AdditionalLibrary.Contains("Plugins") || AdditionalLibrary.Contains("Binaries/ThirdParty") || AdditionalLibrary.Contains("Binaries\\ThirdParty")) && Path.GetDirectoryName(AdditionalLibrary) != Path.GetDirectoryName(OutputFile.AbsolutePath))
				{
					string RelativePath = Utils.MakePathRelativeTo(Path.GetDirectoryName(AdditionalLibrary), Path.GetDirectoryName(OutputFile.AbsolutePath));
					if (!RPaths.Contains(RelativePath))
					{
						RPaths.Add(RelativePath);
						LinkAction.CommandArguments += string.Format(" -Wl,-rpath=\"${{ORIGIN}}/{0}\"", RelativePath);
					}
				}
			}

			LinkAction.CommandArguments += string.Format(" -Wl,-rpath-link=\"{0}\"", Path.GetDirectoryName(OutputFile.AbsolutePath));

			// Add the library paths to the argument list.
			foreach (string LibraryPath in AllLibraryPaths)
			{
				// use absolute paths because of FixDependencies script again
				LinkAction.CommandArguments += string.Format(" -L\"{0}\"", Path.GetFullPath(LibraryPath));
			}

			List<string> EngineAndGameLibraries = new List<string>();

			// Pre-2.25 ld has symbol resolution problems when .so are mixed with .a in a single --start-group/--end-group
			// when linking with --as-needed.
			// Move external libraries to a separate --start-group/--end-group to fix it (and also make groups smaller and faster to link).
			// See https://github.com/EpicGames/UnrealEngine/pull/2778 and https://github.com/EpicGames/UnrealEngine/pull/2793 for discussion
			string ExternalLibraries = "";

			// add libraries in a library group
			LinkAction.CommandArguments += string.Format(" -Wl,--start-group");

			foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
			{
				if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
				{
					// library was passed just like "jemalloc", turn it into -ljemalloc
					ExternalLibraries += string.Format(" -l{0}", AdditionalLibrary);
				}
				else if (Path.GetExtension(AdditionalLibrary) == ".a")
				{
					// static library passed in, pass it along but make path absolute, because FixDependencies script may be executed in a different directory
					string AbsoluteAdditionalLibrary = Path.GetFullPath(AdditionalLibrary);
					if (AbsoluteAdditionalLibrary.Contains(" "))
					{
						AbsoluteAdditionalLibrary = string.Format("\"{0}\"", AbsoluteAdditionalLibrary);
					}

					// libcrypto/libssl contain number of functions that are being used in different DSOs. FIXME: generalize?
					if (LinkEnvironment.Config.bIsBuildingDLL && (AbsoluteAdditionalLibrary.Contains("libcrypto") || AbsoluteAdditionalLibrary.Contains("libssl")))
					{
						LinkAction.CommandArguments += (" -Wl,--whole-archive " + AbsoluteAdditionalLibrary + " -Wl,--no-whole-archive");
					}
					else
					{
						LinkAction.CommandArguments += (" " + AbsoluteAdditionalLibrary);
					}

					LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
				}
				else
				{
					// Skip over full-pathed library dependencies when building DLLs to avoid circular
					// dependencies.
					FileItem LibraryDependency = FileItem.GetItemByPath(AdditionalLibrary);

					var LibName = Path.GetFileNameWithoutExtension(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						// Remove lib prefix
						LibName = LibName.Remove(0, 3);
					}
					string LibLinkFlag = string.Format(" -l{0}", LibName);

					if (LinkEnvironment.Config.bIsBuildingDLL && LinkEnvironment.Config.bIsCrossReferenced)
					{
						// We are building a cross referenced DLL so we can't actually include
						// dependencies at this point. Instead we add it to the list of
						// libraries to be used in the FixDependencies step.
						EngineAndGameLibraries.Add(LibLinkFlag);
						if (!LinkAction.CommandArguments.Contains("--allow-shlib-undefined"))
						{
							LinkAction.CommandArguments += string.Format(" -Wl,--allow-shlib-undefined");
						}
					}
					else
					{
						LinkAction.PrerequisiteItems.Add(LibraryDependency);
						ExternalLibraries += LibLinkFlag;
					}
				}
			}
			LinkAction.CommandArguments += " -Wl,--end-group";

			LinkAction.CommandArguments += " -Wl,--start-group";
			LinkAction.CommandArguments += ExternalLibraries;
			LinkAction.CommandArguments += " -Wl,--end-group";

			LinkAction.CommandArguments += " -lrt"; // needed for clock_gettime()
			LinkAction.CommandArguments += " -lm"; // math

			if (ShouldUseLibcxx(LinkEnvironment.Config.Architecture))
			{
				// libc++ and its abi lib
				LinkAction.CommandArguments += " -nodefaultlibs";
				LinkAction.CommandArguments += " -L" + UEBuildConfiguration.UEThirdPartySourceDirectory + "Linux/LibCxx/lib/Linux/" + LinkEnvironment.Config.Architecture + "/";
				LinkAction.CommandArguments += " " + UEBuildConfiguration.UEThirdPartySourceDirectory + "Linux/LibCxx/lib/Linux/" + LinkEnvironment.Config.Architecture + "/libc++.a";
				LinkAction.CommandArguments += " " + UEBuildConfiguration.UEThirdPartySourceDirectory + "Linux/LibCxx/lib/Linux/" + LinkEnvironment.Config.Architecture + "/libc++abi.a";
				LinkAction.CommandArguments += " -lm";
				LinkAction.CommandArguments += " -lc";
				LinkAction.CommandArguments += " -lgcc_s";
				LinkAction.CommandArguments += " -lgcc";
			}

			// these can be helpful for understanding the order of libraries or library search directories
			//LinkAction.CommandArguments += " -Wl,--verbose";
			//LinkAction.CommandArguments += " -Wl,--trace";
			//LinkAction.CommandArguments += " -v";

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;
			LinkAction.CommandArguments = LinkAction.CommandArguments.Replace("\\\\", "/");
			LinkAction.CommandArguments = LinkAction.CommandArguments.Replace("\\", "/");

			// prepare a linker script
			FileReference LinkerScriptPath = FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, "remove-sym.ldscript");
			if (!LinkEnvironment.Config.LocalShadowDirectory.Exists())
			{
				LinkEnvironment.Config.LocalShadowDirectory.CreateDirectory();
			}
			if (LinkerScriptPath.Exists())
			{
				LinkerScriptPath.Delete();
			}

			using (StreamWriter Writer = File.CreateText(LinkerScriptPath.FullName))
			{
				Writer.WriteLine("UE4 {");
				Writer.WriteLine("  global: *;");
				Writer.WriteLine("  local: _Znwm;");
				Writer.WriteLine("         _ZnwmRKSt9nothrow_t;");
				Writer.WriteLine("         _Znam;");
				Writer.WriteLine("         _ZnamRKSt9nothrow_t;");
				Writer.WriteLine("         _ZdaPv;");
				Writer.WriteLine("         _ZdaPvRKSt9nothrow_t;");
				Writer.WriteLine("         _ZdlPv;");
				Writer.WriteLine("         _ZdlPvRKSt9nothrow_t;");
				// Hide ALL std:: symbols as they can collide
				// with any c++ library out there (like LLVM)
				// and this may lead to operator new/delete mismatches
				Writer.WriteLine("         _ZNSt8*;");
				Writer.WriteLine("};");
			};

			LinkAction.CommandArguments += string.Format(" -Wl,--version-script=\"{0}\"", LinkerScriptPath);

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			// Prepare a script that will run later, once all shared libraries and the executable
			// are created. This script will be called by action created in FixDependencies()
			if (LinkEnvironment.Config.bIsCrossReferenced && LinkEnvironment.Config.bIsBuildingDLL)
			{
				bool bUseCmdExe = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32;
				string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

				string FixDepsScriptPath = Path.Combine(LinkEnvironment.Config.LocalShadowDirectory.FullName, ScriptName);
				if (!bHasWipedFixDepsScript)
				{
					bHasWipedFixDepsScript = true;
					Log.TraceVerbose("Creating script: {0}", FixDepsScriptPath);
					StreamWriter Writer = File.CreateText(FixDepsScriptPath);

					if (bUseCmdExe)
					{
						Writer.Write("@echo off\n");
						Writer.Write("rem Automatically generated by UnrealBuildTool\n");
						Writer.Write("rem *DO NOT EDIT*\n\n");
					}
					else
					{
						Writer.Write("#!/bin/sh\n");
						Writer.Write("# Automatically generated by UnrealBuildTool\n");
						Writer.Write("# *DO NOT EDIT*\n\n");
						Writer.Write("set -o errexit\n");
					}
					Writer.Close();
				}

				StreamWriter FixDepsScript = File.AppendText(FixDepsScriptPath);

				string EngineAndGameLibrariesString = "";
				foreach (string Library in EngineAndGameLibraries)
				{
					EngineAndGameLibrariesString += Library;
				}

				FixDepsScript.Write(string.Format("echo Fixing {0}\n", Path.GetFileName(OutputFile.AbsolutePath)));
				if (!bUseCmdExe)
				{
					FixDepsScript.Write(string.Format("TIMESTAMP=`stat --format %y \"{0}\"`\n", OutputFile.AbsolutePath));
				}
				string FixDepsLine = LinkAction.CommandPath + " " + LinkAction.CommandArguments;
				string Replace = "-Wl,--allow-shlib-undefined";

				FixDepsLine = FixDepsLine.Replace(Replace, EngineAndGameLibrariesString);
				string OutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
				FixDepsLine = FixDepsLine.Replace(OutputFileForwardSlashes, OutputFileForwardSlashes + ".fixed");
				FixDepsLine = FixDepsLine.Replace("$", "\\$");
				FixDepsScript.Write(FixDepsLine + "\n");
				if (bUseCmdExe)
				{
					FixDepsScript.Write(string.Format("move /Y \"{0}.fixed\" \"{0}\"\n", OutputFile.AbsolutePath));
				}
				else
				{
					FixDepsScript.Write(string.Format("mv \"{0}.fixed\" \"{0}\"\n", OutputFile.AbsolutePath));
					FixDepsScript.Write(string.Format("touch -d \"$TIMESTAMP\" \"{0}\"\n\n", OutputFile.AbsolutePath));
				}
				FixDepsScript.Close();
			}

			if (CrossCompiling())
			{
				LinkAction.OutputEventHandler = new DataReceivedEventHandler(CrossCompileOutputReceivedDataEventHandler);
			}
			return OutputFile;
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, FileReference ProjectFileName, FileReference DestinationFile, ActionGraph ActionGraph)
		{
			throw new BuildException("Linux cannot compile C# files");
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			foreach (UEBuildBinary Binary in Binaries)
			{
				BundleDependencies.Add(FileItem.GetItemByFileReference(Binary.Config.OutputFilePath));
			}
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		public override string ConvertPath(string OriginalPath)
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				return OriginalPath.Replace("\\", "/");
			}
			else
			{
				return OriginalPath;
			}
		}

		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, ActionGraph ActionGraph)
		{
			var OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment, ActionGraph);

			if (BinaryLinkEnvironment.Config.bIsBuildingDLL || BinaryLinkEnvironment.Config.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			FileItem FixDepsOutputFile = FixDependencies(BinaryLinkEnvironment, Executable, ActionGraph);
			if (FixDepsOutputFile != null)
			{
				OutputFiles.Add(FixDepsOutputFile);
			}

			return OutputFiles;
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			File.Copy(SourceFileName, TargetFileName, true);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = GetStripPath(PlatformContext.GetActiveArchitecture());
			StartInfo.Arguments = "--strip-debug \"" + TargetFileName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	}
}
