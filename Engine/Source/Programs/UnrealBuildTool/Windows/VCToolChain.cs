// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using System.Text;

namespace UnrealBuildTool
{
	public class VCToolChain : UEToolChain
	{
		bool bSupportWindowsXP;

		public VCToolChain(CPPTargetPlatform CppPlatform, bool bInSupportWindowsXP)
			: base(CppPlatform)
		{
			bSupportWindowsXP = bInSupportWindowsXP;
		}

		static void AddDefinition(StringBuilder String, string Definition)
		{
			// Split the definition into name and value
			int ValueIdx = Definition.IndexOf('=');
			if (ValueIdx == -1)
			{
				AddDefinition(String, Definition, null);
			}
			else
			{
				AddDefinition(String, Definition.Substring(0, ValueIdx), Definition.Substring(ValueIdx + 1));
			}
		}


		static void AddDefinition(StringBuilder String, string Variable, string Value)
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now
			if (Value != null && !Value.StartsWith("\"") && (Value.Contains(" ") || Value.Contains("$")))
			{
				Value = "\"" + Value + "\"";
			}

			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				if (Value != null)
				{
					String.Append(" /D" + Variable + "=" + Value);
				}
				else
				{
					String.Append(" /D" + Variable);
				}
			}
			else
			{
				if (Value != null)
				{
					String.Append(" -D " + Variable + "=" + Value);
				}
				else
				{
					String.Append(" -D " + Variable);
				}
			}
		}


		static void AddIncludePath(StringBuilder String, string IncludePath)
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now
			if (!IncludePath.StartsWith("\"") && (IncludePath.Contains(" ") || IncludePath.Contains("$")))
			{
				IncludePath = "\"" + IncludePath + "\"";
			}

			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				String.Append(" /I " + IncludePath);
			}
			else
			{
				String.Append(" -I" + IncludePath);
			}
		}


		void AppendCLArguments_Global(CPPEnvironment CompileEnvironment, VCEnvironment EnvVars, StringBuilder Arguments)
		{
			// @todo UWP: Why do we ever need WinRT headers when building regular Win32?  Is this just needed for the Windows 10 SDK?
			// @todo UWP: These include paths should be added in SetUpEnvironment(), not here.  Do they need to be the last includes or something?
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
			{
				if (Directory.Exists(EnvVars.WindowsSDKExtensionDir))
				{
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\um", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\shared", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\winrt", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
				}
				if (Directory.Exists(EnvVars.NetFxSDKExtensionDir))
				{
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\um", EnvVars.NetFxSDKExtensionDir));
				}
			}

			// NOTE: Uncommenting this line will print includes as they are encountered by the preprocessor.  This can help with diagnosing include order problems.
			if (WindowsPlatform.bCompileWithClang && !WindowsPlatform.bUseVCCompilerArgs)
			{
				// Arguments.Append( " -H" );
			}
			else
			{
				// Arguments.Append( " /showIncludes" );
			}

			if (WindowsPlatform.bCompileWithClang)
			{
				// Arguments.Append( " -###" );	// @todo clang: Print Clang command-lines (instead of outputting compile results!)

				if (!WindowsPlatform.bUseVCCompilerArgs)
				{
					Arguments.Append(" -std=c++11");
					Arguments.Append(" -fdiagnostics-format=msvc");
					Arguments.Append(" -Xclang -relaxed-aliasing -Xclang --dependent-lib=msvcrt -Xclang --dependent-lib=oldnames -gline-tables-only -ffunction-sections");
				}

				// @todo clang: We're impersonating the Visual C++ compiler by setting MSC_VER and _MSC_FULL_VER to values that MSVC would set
				string VersionString;
				string FullVersionString;
				switch (WindowsPlatform.Compiler)
				{
					case WindowsCompiler.VisualStudio2013:
						VersionString = "18.0";
						FullVersionString = "1800";
						break;

					case WindowsCompiler.VisualStudio2015:
						VersionString = "19.0";
						FullVersionString = "1900";
						break;

					default:
						throw new BuildException("Unexpected value for WindowsPlatform.Compiler: " + WindowsPlatform.Compiler.ToString());
				}

				Arguments.Append(" -fms-compatibility-version=" + VersionString);
				AddDefinition(Arguments, "_MSC_FULL_VER", FullVersionString + "00000");
			}

			// @todo clang: Clang on Windows doesn't respect "#pragma warning (error: ####)", and we're not passing "/WX", so warnings are not
			// treated as errors when compiling on Windows using Clang right now.

			// NOTE re: clang: the arguments for clang-cl can be found at http://llvm.org/viewvc/llvm-project/cfe/trunk/include/clang/Driver/CLCompatOptions.td?view=markup
			// This will show the cl.exe options that map to clang.exe ones, which ones are ignored and which ones are unsupported.

			if (BuildConfiguration.bEnableCodeAnalysis)
			{
				Arguments.Append(" /analyze");

				// Don't cause analyze warnings to be errors
				Arguments.Append(" /analyze:WX-");

				// Report functions that use a LOT of stack space.  You can lower this value if you
				// want more aggressive checking for functions that use a lot of stack memory.
				Arguments.Append(" /analyze:stacksize81940");

				// Don't bother generating code, only analyze code (may report fewer warnings though.)
				//Arguments.Append(" /analyze:only");
			}

			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				// Prevents the compiler from displaying its logo for each invocation.
				Arguments.Append(" /nologo");

				// Enable intrinsic functions.
				Arguments.Append(" /Oi");
			}
			else
			{
				// Enable intrinsic functions.
				Arguments.Append(" -fbuiltin");
			}


			if (WindowsPlatform.bCompileWithClang)
			{
				// Tell the Clang compiler whether we want to generate 32-bit code or 64-bit code
				if (CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					Arguments.Append(" --target=x86_64-pc-windows-msvc");
				}
				else
				{
					Arguments.Append(" --target=i686-pc-windows-msvc");
				}
			}

			// Compile into an .obj file, and skip linking.
			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				Arguments.Append(" /c");
			}
			else
			{
				Arguments.Append(" -c");
			}

			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				// Separate functions for linker.
				Arguments.Append(" /Gy");

				// Allow 800% of the default memory allocation limit.
				Arguments.Append(" /Zm800");

				// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
				Arguments.Append(" /wd4819");
			}

			// @todo UWP: UE4 is non-compliant when it comes to use of %s and %S
			// Previously %s meant "the current character set" and %S meant "the other one".
			// Now %s means multibyte and %S means wide. %Ts means "natural width".
			// Reverting this behaviour until the UE4 source catches up.
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				AddDefinition(Arguments, "_CRT_STDIO_LEGACY_WIDE_SPECIFIERS=1");
			}

			// @todo UWP: Silence the hash_map deprecation errors for now. This should be replaced with unordered_map for the real fix.
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				AddDefinition(Arguments, "_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS=1");
			}

			if (BuildConfiguration.bUseSharedPCHs)
			{
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// @todo SharedPCH: Disable warning about PCH defines not matching .cpp defines.  We "cheat" these defines a little
					// bit to make shared PCHs work.  But it's totally safe.  Trust us.
					Arguments.Append(" /wd4651");

					// @todo SharedPCH: Disable warning about redefining *API macros.  The PCH header is compiled with various DLLIMPORTs, but
					// when a module that uses that PCH header *IS* one of those imports, that module is compiled with EXPORTS, so the macro
					// is redefined on the command-line.  We need to clobber those defines to make shared PCHs work properly!
					Arguments.Append(" /wd4005");
				}
			}

			// If compiling as a DLL, set the relevant defines
			if (CompileEnvironment.Config.bIsBuildingDLL)
			{
				AddDefinition(Arguments, "_WINDLL");
			}

			// When targeting Windows XP with Visual Studio 2012+, we need to tell the compiler to use the older Windows SDK that works
			// with Windows XP (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
			if (bSupportWindowsXP)
			{
				AddDefinition(Arguments, "_USING_V110_SDK71_");
			}


			// Handle Common Language Runtime support (C++/CLI)
			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
			{
				Arguments.Append(" /clr");

				// Don't use default lib path, we override it explicitly to use the 4.0 reference assemblies.
				Arguments.Append(" /clr:nostdlib");
			}

			//
			//	Debug
			//
			if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
			{
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// Disable compiler optimization.
					Arguments.Append(" /Od");

					// Favor code size (especially useful for embedded platforms).
					Arguments.Append(" /Os");

					// Allow inline method expansion unless E&C support is requested
					if (!BuildConfiguration.bSupportEditAndContinue)
					{
						Arguments.Append(" /Ob2");
					}

					if ((CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32) ||
						(CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64))
					{
						// Runtime stack checks are not allowed when compiling for CLR
						if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
						{
							Arguments.Append(" /RTCs");
						}
					}
				}
			}
			//
			//	Development and LTCG
			//
			else
			{
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					if (CompileEnvironment.Config.OptimizeCode == ModuleRules.CodeOptimization.InShippingBuildsOnly && CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Shipping)
					{
						// Disable compiler optimization.
						Arguments.Append(" /Od");

						// Favor code size (especially useful for embedded platforms).
						Arguments.Append(" /Os");
					}
					else
					{
						// Maximum optimizations if desired.
						if (CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds)
						{
							Arguments.Append(" /Ox");
						}

						// Favor code speed.
						Arguments.Append(" /Ot");

						// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
						if (BuildConfiguration.bOmitFramePointers == false
						&& ((CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32) ||
							(CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)))
						{
							Arguments.Append(" /Oy-");
						}
					}

					// Allow inline method expansion
					Arguments.Append(" /Ob2");

					//
					// LTCG
					//
					if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
					{
						if (BuildConfiguration.bAllowLTCG)
						{
							// Enable link-time code generation.
							Arguments.Append(" /GL");
						}
					}
				}
				else
				{
					if (CompileEnvironment.Config.OptimizeCode != ModuleRules.CodeOptimization.InShippingBuildsOnly || CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
					{
						// Maximum optimizations if desired.
						if (CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds)
						{
							Arguments.Append(" -O3");
						}
					}
				}
			}

			//
			//	PC
			//
			if ((CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32) ||
				(CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64))
			{
				// SSE options are not allowed when using CLR compilation
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
				{
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						if (CompileEnvironment.Config.bUseAVX)
						{
							// Allow the compiler to generate AVX instructions.
							Arguments.Append(" /arch:AVX");
						}
						// SSE options are not allowed when using the 64 bit toolchain
						// (enables SSE2 automatically)
						else if (CompileEnvironment.Config.Target.Platform != CPPTargetPlatform.Win64)
						{
							// Allow the compiler to generate SSE2 instructions.
							Arguments.Append(" /arch:SSE2");
						}
					}
					else
					{
						if (CompileEnvironment.Config.bUseAVX)
						{
							// Allow the compiler to generate AVX instructions.
							Arguments.Append(" -mavx");
						}
						else if (CompileEnvironment.Config.Target.Platform != CPPTargetPlatform.Win64)
						{
							// Allow the compiler to generate SSE2 instructions.
							Arguments.Append(" -msse2");
						}
					}
				}

				if (WindowsPlatform.bUseVCCompilerArgs && !WindowsPlatform.bCompileWithICL)
				{
					// Prompt the user before reporting internal errors to Microsoft.
					Arguments.Append(" /errorReport:prompt");
				}

				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
				{
					// Enable C++ exceptions when building with the editor or when building UHT.
					if (!WindowsPlatform.bCompileWithClang &&	// @todo clang: C++ exceptions are not supported with Clang on Windows yet
						(CompileEnvironment.Config.bEnableExceptions || UEBuildConfiguration.bBuildEditor || UEBuildConfiguration.bForceEnableExceptions))
					{
						// Enable C++ exception handling, but not C exceptions.
						Arguments.Append(" /EHsc");
					}
					else
					{
						// This is required to disable exception handling in VC platform headers.
						CompileEnvironment.Config.Definitions.Add("_HAS_EXCEPTIONS=0");

						if (!WindowsPlatform.bUseVCCompilerArgs)
						{
							Arguments.Append(" -fno-exceptions");
						}
					}
				}
				else
				{
					// For C++/CLI all exceptions must be left enabled
					Arguments.Append(" /EHa");
				}
			}
			else if (CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.HTML5)
			{
				Arguments.Append(" /EHsc");
			}

			// If enabled, create debug information.
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// Store debug info in .pdb files.
					// @todo clang: PDB files are emited from Clang but do not fully work with Visual Studio yet (breakpoints won't hit due to "symbol read error")
					// @todo clang (update): as of clang 3.9 breakpoints work with PDBs, and the callstack is correct, so could be used for crash dumps. However debugging is still impossible due to the large amount of unreadable variables and unpredictable breakpoint stepping behaviour
					if (BuildConfiguration.bUsePDBFiles)
					{
						// Create debug info suitable for E&C if wanted.
						if (BuildConfiguration.bSupportEditAndContinue &&
							// We only need to do this in debug as that's the only configuration that supports E&C.
							CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
						{
							Arguments.Append(" /ZI");
						}
						// Regular PDB debug information.
						else
						{
							Arguments.Append(" /Zi");
						}
						// We need to add this so VS won't lock the PDB file and prevent synchronous updates. This forces serialization through MSPDBSRV.exe.
						// See http://msdn.microsoft.com/en-us/library/dn502518.aspx for deeper discussion of /FS switch.
						if (BuildConfiguration.bUseIncrementalLinking)
						{
							Arguments.Append(" /FS");
						}
					}
					// Store C7-format debug info in the .obj files, which is faster.
					else
					{
						Arguments.Append(" /Z7");
					}
				}
			}

			// Specify the appropriate runtime library based on the platform and config.
			if (CompileEnvironment.Config.bUseStaticCRT)
			{
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /MTd");
					}
					else
					{
						AddDefinition(Arguments, "_MT");
						AddDefinition(Arguments, "_DEBUG");
					}
				}
				else
				{
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /MT");
					}
					else
					{
						AddDefinition(Arguments, "_MT");
					}
				}
			}
			else
			{
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /MDd");
					}
					else
					{
						AddDefinition(Arguments, "_MT");
						AddDefinition(Arguments, "_DEBUG");
						AddDefinition(Arguments, "_DLL");
					}
				}
				else
				{
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /MD");
					}
					else
					{
						AddDefinition(Arguments, "_MT");
						AddDefinition(Arguments, "_DLL");
					}
				}
			}

			if (WindowsPlatform.bUseVCCompilerArgs && !BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				if (!WindowsPlatform.bCompileWithClang)
				{
					// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
					// Note: not needed for clang, it implicitly upgrades COFF files to bigobj format when necessary.
					Arguments.Append(" /bigobj");
				}

				if (!WindowsPlatform.bCompileWithICL)
				{
					// Relaxes floating point precision semantics to allow more optimization.
					Arguments.Append(" /fp:fast");
				}

				if (CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds)
				{
					// Allow optimized code to be debugged more easily.  This makes PDBs a bit larger, but doesn't noticeably affect
					// compile times.  The executable code is not affected at all by this switch, only the debugging information.
					if (EnvVars.CLExeVersion >= new Version("18.0.30723"))
					{
						// VC2013 Update 3 has a new flag for doing this
						Arguments.Append(" /Zo");
					}
					else
					{
						if (!WindowsPlatform.bCompileWithICL)
						{
							Arguments.Append(" /d2Zi+");
						}
					}
				}
			}
			else
			{
				// Relaxes floating point precision semantics to allow more optimization.
				Arguments.Append(" -ffast-math");
			}

			// Disabled when compiling UnrealCodeAnalyzer as it breaks compilation (some structs in clang/llvm headers require 8-byte alignment in 32-bit compilation)
			if (!UnrealBuildTool.CommandLineContains(@"UnrealCodeAnalyzer")
				// and when running UnrealCodeAnalyzer as it doesn't understand the /Zp syntax.
				&& !BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				if (CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					// Pack struct members on 8-byte boundaries.
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /Zp8");
					}
					else
					{
						Arguments.Append(" -fpack-struct=8");
					}
				}
				else
				{
					// Pack struct members on 4-byte boundaries.
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /Zp4");
					}
					else
					{
						Arguments.Append(" -fpack-struct=4");
					}
				}
			}

			//
			// Options required by UnrealCodeAnalyzer
			//
			if (BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				AddDefinition(Arguments, "UNREAL_CODE_ANALYZER");
			}

			//@todo: Disable warnings for VS2015. These should be reenabled as we clear the reasons for them out of the engine source and the VS2015 toolchain evolves.
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseVCCompilerArgs)
			{
				// Disable shadow variable warnings
				if (CompileEnvironment.Config.bEnableShadowVariableWarning == false)
				{
					Arguments.Append(" /wd4456"); // 4456 - declaration of 'LocalVariable' hides previous local declaration
					Arguments.Append(" /wd4458"); // 4458 - declaration of 'parameter' hides class member
					Arguments.Append(" /wd4459"); // 4459 - declaration of 'LocalVariable' hides global declaration
				}

				Arguments.Append(" /wd4463"); // 4463 - overflow; assigning 1 to bit-field that can only hold values from -1 to 0

				Arguments.Append(" /wd4838"); // 4838: conversion from 'type1' to 'type2' requires a narrowing conversion
			}

		}

		static void AppendCLArguments_CPP(CPPEnvironment CompileEnvironment, StringBuilder Arguments)
		{
			if (!WindowsPlatform.bCompileWithClang)
			{
				// Explicitly compile the file as C++.
				Arguments.Append(" /TP");
			}
			else
			{
				string FileSpecifier = "c++";
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Tell Clang to generate a PCH header
					FileSpecifier += "-header";
				}
				
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					Arguments.AppendFormat(" -Xclang -x -Xclang \"{0}\"", FileSpecifier);
				}
				else
				{
					Arguments.Append(" -x " + FileSpecifier);
				}
			}


			if (!CompileEnvironment.Config.bEnableBufferSecurityChecks)
			{
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// This will disable buffer security checks (which are enabled by default) that the MS compiler adds around arrays on the stack,
					// Which can add some performance overhead, especially in performance intensive code
					// Only disable this if you know what you are doing, because it will be disabled for the entire module!
					Arguments.Append(" /GS-");
				}
				else
				{
					Arguments.Append(" -fno-stack-protector");
				}
			}

			// C++/CLI requires that RTTI is left enabled
			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
			{
				if (CompileEnvironment.Config.bUseRTTI)
				{
					// Enable C++ RTTI.
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /GR");
					}
					else
					{
						Arguments.Append(" -Xclang -frtti-data");
					}
				}
				else
				{
					// Disable C++ RTTI.
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						Arguments.Append(" /GR-");
					}
					else
					{
						Arguments.Append(" -Xclang -fno-rtti-data");
					}
				}
			}

			// Set warning level.
			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				if (!BuildConfiguration.bRunUnrealCodeAnalyzer && !WindowsPlatform.bCompileWithICL)
				{
					// Restrictive during regular compilation.
					Arguments.Append(" /W4");
				}
				else
				{
					// If we had /W4 with clang or Intel on windows we would be flooded with warnings. This will be fixed incrementally.
					Arguments.Append(" /W0");
				}
			}
			else
			{
				Arguments.Append(" -Wall");
			}

			// Intel compiler options.
			if (WindowsPlatform.bCompileWithICL)
			{
				Arguments.Append(" /Qstd=c++11");
				Arguments.Append(" /fp:precise");
				Arguments.Append(" /nologo");
			}

			if (WindowsPlatform.bCompileWithClang)
			{
				// Disable specific warnings that cause problems with Clang
				// NOTE: These must appear after we set the MSVC warning level

				// @todo clang: Ideally we want as few warnings disabled as possible
				// 

				// Allow Microsoft-specific syntax to slide, even though it may be non-standard.  Needed for Windows headers.
				Arguments.Append(" -Wno-microsoft");

				// @todo clang: Hack due to how we have our 'DummyPCH' wrappers setup when using unity builds.  This warning should not be disabled!!
				Arguments.Append(" -Wno-msvc-include");

				if (CompileEnvironment.Config.bEnableShadowVariableWarning)
				{
					Arguments.Append(" -Wshadow" + (BuildConfiguration.bShadowVariableErrors ? "" : " -Wno-error=shadow"));
				}

				// @todo clang: Kind of a shame to turn these off.  We'd like to catch unused variables, but it is tricky with how our assertion macros work.
				Arguments.Append(" -Wno-inconsistent-missing-override");
				Arguments.Append(" -Wno-unused-variable");
				Arguments.Append(" -Wno-unused-local-typedefs");
				Arguments.Append(" -Wno-unused-function");
				Arguments.Append(" -Wno-unused-private-field");
				Arguments.Append(" -Wno-unused-value");

				Arguments.Append(" -Wno-inline-new-delete");	// @todo clang: We declare operator new as inline.  Clang doesn't seem to like that.
				Arguments.Append(" -Wno-implicit-exception-spec-mismatch");

				// Sometimes we compare 'this' pointers against nullptr, which Clang warns about by default
				Arguments.Append(" -Wno-undefined-bool-conversion");

				// @todo clang: Disabled warnings were copied from MacToolChain for the most part
				Arguments.Append(" -Wno-deprecated-declarations");
				Arguments.Append(" -Wno-deprecated-writable-strings");
				Arguments.Append(" -Wno-deprecated-register");
				Arguments.Append(" -Wno-switch-enum");
				Arguments.Append(" -Wno-logical-op-parentheses");	// needed for external headers we shan't change
				Arguments.Append(" -Wno-null-arithmetic");			// needed for external headers we shan't change
				Arguments.Append(" -Wno-deprecated-declarations");	// needed for wxWidgets
				Arguments.Append(" -Wno-return-type-c-linkage");	// needed for PhysX
				Arguments.Append(" -Wno-ignored-attributes");		// needed for nvtesslib
				Arguments.Append(" -Wno-uninitialized");
				Arguments.Append(" -Wno-tautological-compare");
				Arguments.Append(" -Wno-switch");
				Arguments.Append(" -Wno-invalid-offsetof"); // needed to suppress warnings about using offsetof on non-POD types.

				// @todo clang: Sorry for adding more of these, but I couldn't read my output log. Most should probably be looked at
				Arguments.Append(" -Wno-unused-parameter");			// Unused function parameter. A lot are named 'bUnused'...
				Arguments.Append(" -Wno-ignored-qualifiers");		// const ignored when returning by value e.g. 'const int foo() { return 4; }'
				Arguments.Append(" -Wno-expansion-to-defined");		// Usage of 'defined(X)' in a macro definition. Gives different results under MSVC
				Arguments.Append(" -Wno-gnu-string-literal-operator-template");	// String literal operator"" in template, used by delegates
				Arguments.Append(" -Wno-sign-compare");				// Signed/unsigned comparison - millions of these
				Arguments.Append(" -Wno-undefined-var-template");	// Variable template instantiation required but no definition available
				Arguments.Append(" -Wno-missing-field-initializers"); // Stupid warning, generated when you initialize with MyStruct A = {0};
			}
		}

		static void AppendCLArguments_C(StringBuilder Arguments)
		{
			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				Arguments.Append(" -x c");
			}
			else
			{
				// Explicitly compile the file as C.
				Arguments.Append(" /TC");
			}

			if (WindowsPlatform.bUseVCCompilerArgs)
			{
				// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
				Arguments.Append(" /W0");
			}
		}

		void AppendLinkArguments(LinkEnvironment LinkEnvironment, StringBuilder Arguments)
		{
			if (WindowsPlatform.bCompileWithClang && WindowsPlatform.bAllowClangLinker)
			{
				// This tells LLD to run in "Windows emulation" mode, meaning that it will accept MSVC Link arguments
				Arguments.Append(" -flavor link");

				// @todo clang: The following static libraries aren't linking correctly with Clang:
				//		tbbmalloc.lib, zlib_64.lib, libpng_64.lib, freetype2412MT.lib, IlmImf.lib
				//		LLD: Assertion failed: result.size() == 1, file ..\tools\lld\lib\ReaderWriter\FileArchive.cpp, line 71
				//		

				// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
				if (!BuildConfiguration.bOmitFramePointers)
				{
					Arguments.Append(" --disable-fp-elim");
				}
			}

			// Don't create a side-by-side manifest file for the executable.
			Arguments.Append(" /MANIFEST:NO");

			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Append(" /NOLOGO");

			if (LinkEnvironment.Config.bCreateDebugInfo)
			{
				// Output debug info for the linked executable.
				Arguments.Append(" /DEBUG");

				// Allow partial PDBs for faster linking
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && BuildConfiguration.bUseFastPDBLinking)
				{
					Arguments.Append(":FASTLINK");
				}
			}

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Append(" /errorReport:prompt");

			//
			//	PC
			//
			if ((LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32) ||
				(LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64))
			{
				// Set machine type/ architecture to be 64 bit.
				if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					Arguments.Append(" /MACHINE:x64");
				}
				// 32 bit executable/ target.
				else
				{
					Arguments.Append(" /MACHINE:x86");
				}

				{
					if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						Arguments.Append(" /SUBSYSTEM:CONSOLE");
					}
					else
					{
						Arguments.Append(" /SUBSYSTEM:WINDOWS");
					}

					// When targeting Windows XP in Visual Studio 2012+, we need to tell the linker we are going to support execution
					// on that older platform.  The compiler defaults to version 6.0+.  We'll modify the SUBSYSTEM parameter here.
					if (bSupportWindowsXP)
					{
						Arguments.Append(LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64 ? ",5.02" : ",5.01");
					}
				}

				if (LinkEnvironment.Config.bIsBuildingConsoleApplication && !LinkEnvironment.Config.bIsBuildingDLL && !String.IsNullOrEmpty(LinkEnvironment.Config.WindowsEntryPointOverride))
				{
					// Use overridden entry point
					Arguments.Append(" /ENTRY:" + LinkEnvironment.Config.WindowsEntryPointOverride);
				}

				// Allow the OS to load the EXE at different base addresses than its preferred base address.
				Arguments.Append(" /FIXED:No");

				// Option is only relevant with 32 bit toolchain.
				if ((LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32) && WindowsPlatform.bBuildLargeAddressAwareBinary)
				{
					// Disables the 2GB address space limit on 64-bit Windows and 32-bit Windows with /3GB specified in boot.ini
					Arguments.Append(" /LARGEADDRESSAWARE");
				}

				// Explicitly declare that the executable is compatible with Data Execution Prevention.
				Arguments.Append(" /NXCOMPAT");

				// Set the default stack size.
				Arguments.Append(" /STACK:5000000");

				// E&C can't use /SAFESEH.  Also, /SAFESEH isn't compatible with 64-bit linking
				if (!BuildConfiguration.bSupportEditAndContinue &&
					LinkEnvironment.Config.Target.Platform != CPPTargetPlatform.Win64)
				{
					// Generates a table of Safe Exception Handlers.  Documentation isn't clear whether they actually mean
					// Structured Exception Handlers.
					Arguments.Append(" /SAFESEH");
				}

				// Allow delay-loaded DLLs to be explicitly unloaded.
				Arguments.Append(" /DELAY:UNLOAD");

				if (LinkEnvironment.Config.bIsBuildingDLL)
				{
					Arguments.Append(" /DLL");
				}

				if (LinkEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					// DLLs built with managed code aren't allowed to have entry points as they will try to initialize
					// complex static variables.  Managed code isn't allowed to run during DLLMain, we can't allow
					// these variables to be initialized here.
					if (LinkEnvironment.Config.bIsBuildingDLL)
					{
						// NOTE: This appears to only be needed if we want to get call stacks for debugging exit crashes related to the above
						//		Result += " /NOENTRY /NODEFAULTLIB:nochkclr.obj";
					}
				}
			}

			// Don't embed the full PDB path; we want to be able to move binaries elsewhere. They will always be side by side.
			Arguments.Append(" /PDBALTPATH:%_PDB%");

			//
			//	Shipping & LTCG
			//
			if (BuildConfiguration.bAllowLTCG &&
				LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Arguments.Append(" /LTCG");

				// This is where we add in the PGO-Lite linkorder.txt if we are using PGO-Lite
				//Result += " /ORDER:@linkorder.txt";
				//Result += " /VERBOSE";
			}

			//
			//	Shipping binary
			//
			if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Generate an EXE checksum.
				Arguments.Append(" /RELEASE");

				// Eliminate unreferenced symbols.
				Arguments.Append(" /OPT:REF");

				// Remove redundant COMDATs.
				Arguments.Append(" /OPT:ICF");
			}
			//
			//	Regular development binary. 
			//
			else
			{
				// Keep symbols that are unreferenced.
				Arguments.Append(" /OPT:NOREF");

				// Disable identical COMDAT folding.
				Arguments.Append(" /OPT:NOICF");
			}

			// Enable incremental linking if wanted.
			// NOTE: Don't bother using incremental linking for C++/CLI projects, as that's not supported and the option
			//		 will silently be ignored anyway
			if (BuildConfiguration.bUseIncrementalLinking && LinkEnvironment.Config.CLRMode != CPPCLRMode.CLREnabled)
			{
				Arguments.Append(" /INCREMENTAL");
			}
			else
			{
				Arguments.Append(" /INCREMENTAL:NO");
			}

			// Disable 
			//LINK : warning LNK4199: /DELAYLOAD:nvtt_64.dll ignored; no imports found from nvtt_64.dll
			// type warning as we leverage the DelayLoad option to put third-party DLLs into a 
			// non-standard location. This requires the module(s) that use said DLL to ensure that it 
			// is loaded prior to using it.
			Arguments.Append(" /ignore:4199");

			// Suppress warnings about missing PDB files for statically linked libraries.  We often don't want to distribute
			// PDB files for these libraries.
			Arguments.Append(" /ignore:4099");		// warning LNK4099: PDB '<file>' was not found with '<file>'
		}

		void AppendLibArguments(LinkEnvironment LinkEnvironment, StringBuilder Arguments)
		{
			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Append(" /NOLOGO");

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Append(" /errorReport:prompt");

			//
			//	PC
			//
			if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32 || LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
			{
				// Set machine type/ architecture to be 64 bit.
				if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					Arguments.Append(" /MACHINE:x64");
				}
				// 32 bit executable/ target.
				else
				{
					Arguments.Append(" /MACHINE:x86");
				}

				{
					if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						Arguments.Append(" /SUBSYSTEM:CONSOLE");
					}
					else
					{
						Arguments.Append(" /SUBSYSTEM:WINDOWS");
					}

					// When targeting Windows XP in Visual Studio 2012+, we need to tell the linker we are going to support execution
					// on that older platform.  The compiler defaults to version 6.0+.  We'll modify the SUBSYSTEM parameter here.
					if (bSupportWindowsXP)
					{
						Arguments.Append(LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64 ? ",5.02" : ",5.01");
					}
				}
			}

			//
			//	Shipping & LTCG
			//
			if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Arguments.Append(" /LTCG");
			}
		}

		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			VCEnvironment EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.Config.Target.Platform, bSupportWindowsXP);

			StringBuilder SharedArguments = new StringBuilder();
			AppendCLArguments_Global(CompileEnvironment, EnvVars, SharedArguments);

			// Add include paths to the argument list.
			if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath(SharedArguments, IncludePath);
				}
			}
			else
			{
				foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath(SharedArguments, System.IO.Path.GetFullPath(IncludePath));
				}
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
				if (WindowsPlatform.bCompileWithClang)
				{
					// @todo Clang: Clang uses a special command-line syntax for system headers.  This is used for two reasons.  The first is that Clang will automatically 
					// suppress compiler warnings in headers found in these directories, such as the DirectX SDK headers.  The other reason this is important is in the case 
					// where there the same header include path is passed as both a regular include path and a system include path (extracted from INCLUDE environment).  In 
					// this case Clang will ignore any earlier occurrence of the include path, preventing a system header include path from overriding a different system 
					// include path set later on by a module.  NOTE: When passing "-Xclang", these options will always appear at the end of the command-line string, meaning
					// they will be forced to appear *after* all environment-variable-extracted includes.  This is technically okay though.
					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						SharedArguments.AppendFormat(" -Xclang -internal-isystem -Xclang \"{0}\"", IncludePath);
					}
					else
					{
						SharedArguments.AppendFormat(" -isystem \"{0}\"", IncludePath);
					}
				}
				else
				{
					AddIncludePath(SharedArguments, IncludePath);
				}
			}


			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
			{
				// Add .NET framework assembly paths.  This is needed so that C++/CLI projects
				// can reference assemblies with #using, without having to hard code a path in the
				// .cpp file to the assembly's location.				
				foreach (string AssemblyPath in CompileEnvironment.Config.SystemDotNetAssemblyPaths)
				{
					SharedArguments.AppendFormat(" /AI \"{0}\"", AssemblyPath);
				}

				// Add explicit .NET framework assembly references				
				foreach (string AssemblyName in CompileEnvironment.Config.FrameworkAssemblyDependencies)
				{
					SharedArguments.AppendFormat(" /FU \"{0}\"", AssemblyName);
				}

				// Add private assembly references				
				foreach (PrivateAssemblyInfo CurAssemblyInfo in CompileEnvironment.PrivateAssemblyDependencies)
				{
					SharedArguments.AppendFormat(" /FU \"{0}\"", CurAssemblyInfo.FileItem.AbsolutePath);
				}
			}

			if (!WindowsPlatform.bCompileWithClang && WindowsPlatform.bLogDetailedCompilerTimingInfo)
			{
				// Force MSVC
				SharedArguments.Append(" /Bt+");
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				string DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				AddDefinition(SharedArguments, DefinitionArgument);
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";
				// ensure compiler timings are captured when we execute the action.
				if (!WindowsPlatform.bCompileWithClang && WindowsPlatform.bLogDetailedCompilerTimingInfo)
				{
					CompileAction.bPrintDebugInfo = true;
				}

				StringBuilder FileArguments = new StringBuilder();
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				// If this is a CLR file then make sure our dependent assemblies are added as prerequisites
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					foreach (PrivateAssemblyInfo CurPrivateAssemblyDependency in CompileEnvironment.PrivateAssemblyDependencies)
					{
						CompileAction.PrerequisiteItems.Add(CurPrivateAssemblyDependency.FileItem);
					}
				}

				if (WindowsPlatform.bCompileWithClang || WindowsPlatform.bCompileWithICL)
				{
					CompileAction.OutputEventHandler = new DataReceivedEventHandler(ClangCompilerOutputFormatter);
				}


				bool bEmitsObjectFile = true;
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Generate a CPP File that just includes the precompiled header.
					string PCHCPPFilename = "PCH." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName() + ".cpp";
					FileReference PCHCPPPath = FileReference.Combine(CompileEnvironment.Config.OutputDirectory, PCHCPPFilename);
					FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
						PCHCPPPath,
						string.Format("#include \"{0}\"\r\n", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename)
						);

					// Make sure the original source directory the PCH header file existed in is added as an include
					// path -- it might be a private PCH header and we need to make sure that its found!
					string OriginalPCHHeaderDirectory = Path.GetDirectoryName(SourceFile.AbsolutePath);
					AddIncludePath(FileArguments, OriginalPCHHeaderDirectory);

					string PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Win64).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.PCHOutputDirectory ?? CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					if (WindowsPlatform.bUseVCCompilerArgs)
					{
						// Add the parameters needed to compile the precompiled header file to the command-line.
						FileArguments.AppendFormat(" /Yc\"{0}\"", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
						FileArguments.AppendFormat(" /Fp\"{0}\"", PrecompiledHeaderFile.AbsolutePath);

						// If we're creating a PCH that will be used to compile source files for a library, we need
						// the compiled modules to retain a reference to PCH's module, so that debugging information
						// will be included in the library.  This is also required to avoid linker warning "LNK4206"
						// when linking an application that uses this library.
						if (CompileEnvironment.Config.bIsBuildingLibrary)
						{
							// NOTE: The symbol name we use here is arbitrary, and all that matters is that it is
							// unique per PCH module used in our library
							string FakeUniquePCHSymbolName = CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileNameWithoutExtension();
							FileArguments.AppendFormat(" /Yl{0}", FakeUniquePCHSymbolName);
						}
					}
					else
					{
						FileArguments.Append(" -Xclang -emit-pch");

						FileArguments.AppendFormat(" -o\"{0}\"", PrecompiledHeaderFile.AbsolutePath);

						// Clang PCH generation doesn't create an .obj file to link in, unlike MSVC
						bEmitsObjectFile = false;
					}

					FileArguments.AppendFormat(" \"{0}\"", PCHCPPFile.AbsolutePath);

					CompileAction.StatusDescription = PCHCPPFilename;
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);

						if (WindowsPlatform.bCompileWithClang)
						{
							// NOTE: With Clang, PCH headers are ALWAYS forcibly included!
							// NOTE: This needs to be before the other include paths to ensure Clang uses it instead of the source header file.
							if (WindowsPlatform.bUseVCCompilerArgs)
							{
								FileArguments.AppendFormat(" /FI\"{0}\"", Path.ChangeExtension(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath, null));
							}
							else
							{
								// FileArguments.Append( " -Xclang -fno-validate-pch" );	// @todo clang: Interesting option for faster performance
								FileArguments.AppendFormat(" -include-pch \"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath);
							}
						}
						else
						{
							FileArguments.AppendFormat(" /Yu\"{0}\"", CompileEnvironment.Config.PCHHeaderNameInCode);
							FileArguments.AppendFormat(" /Fp\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath);

							// Is it unsafe to always force inclusion?  Clang is doing it, and .generated.cpp files
							// won't work otherwise, because they're not located in the context of the module,
							// so they can't access the module's PCH without an absolute path.
							//if( CompileEnvironment.Config.bForceIncludePrecompiledHeader )
							{
								// Force include the precompiled header file.  This is needed because we may have selected a
								// precompiled header that is different than the first direct include in the C++ source file, but
								// we still need to make sure that our precompiled header is the first thing included!
								FileArguments.AppendFormat(" /FI\"{0}\"", CompileEnvironment.Config.PCHHeaderNameInCode);
							}
						}
					}

					// UnrealCodeAnalyzer requires compiled file name to be first argument.
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						// Add the source file path to the command-line.
						FileArguments.AppendFormat(" \"{0}\"", SourceFile.AbsolutePath);
					}

					CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				}

				if (bEmitsObjectFile)
				{
					string ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Win64).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					// UnrealCodeAnalyzer requires specific position of output file.
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						if (WindowsPlatform.bUseVCCompilerArgs)
						{
							FileArguments.AppendFormat(" /Fo\"{0}\"", ObjectFile.AbsolutePath);
						}
						else
						{
							FileArguments.AppendFormat(" -o\"{0}\"", ObjectFile.AbsolutePath);
						}
					}
				}

				// Create PDB files if we were configured to do that.
				//
				// Also, when debug info is off and XGE is enabled, force PDBs, otherwise it will try to share
				// a PDB file, which causes PCH creation to be serial rather than parallel (when debug info is disabled)
				//		--> See https://udn.epicgames.com/lists/showpost.php?id=50619&list=unprog3
				if (BuildConfiguration.bUsePDBFiles ||
					(BuildConfiguration.bAllowXGE && !CompileEnvironment.Config.bCreateDebugInfo))
				{
					string PDBFileName;
					bool bActionProducesPDB = false;

					// All files using the same PCH are required to share the same PDB that was used when compiling the PCH
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						PDBFileName = "PCH." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
					}
					// Files creating a PCH use a PDB per file.
					else if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
						PDBFileName = "PCH." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
						bActionProducesPDB = true;
					}
					// Ungrouped C++ files use a PDB per file.
					else if (!bIsPlainCFile)
					{
						PDBFileName = Path.GetFileName(SourceFile.AbsolutePath);
						bActionProducesPDB = true;
					}
					// Group all plain C files that doesn't use PCH into the same PDB
					else
					{
						PDBFileName = "MiscPlainC";
					}

					{
						// Specify the PDB file that the compiler should write to.
						FileItem PDBFile = FileItem.GetItemByFileReference(
								FileReference.Combine(
									CompileEnvironment.Config.OutputDirectory,
									PDBFileName + ".pdb"
									)
								);
						FileArguments.AppendFormat(" /Fd\"{0}\"", PDBFile.AbsolutePath);

						// Only use the PDB as an output file if we want PDBs and this particular action is
						// the one that produces the PDB (as opposed to no debug info, where the above code
						// is needed, but not the output PDB, or when multiple files share a single PDB, so
						// only the action that generates it should count it as output directly)
						if (BuildConfiguration.bUsePDBFiles && bActionProducesPDB)
						{
							CompileAction.ProducedItems.Add(PDBFile);
							Result.DebugDataFiles.Add(PDBFile);
						}
					}
				}

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					AppendCLArguments_C(FileArguments);
				}
				else
				{
					AppendCLArguments_CPP(CompileEnvironment, FileArguments);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				if (BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					CompileAction.CommandPath = System.IO.Path.Combine(CompileAction.WorkingDirectory, @"..", @"Binaries", @"Win32", @"UnrealCodeAnalyzer.exe");
				}
				else
				{
					CompileAction.CommandPath = EnvVars.CompilerPath;
				}

				string UnrealCodeAnalyzerArguments = "";
				if (BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					string UCAMode = BuildConfiguration.bUCACheckUObjectThreadSafety ? @"-CheckThreadSafety " : @"-CreateIncludeFiles ";
					string ObjectFileExtension = BuildConfiguration.bUCACheckUObjectThreadSafety ? @".tsc" : @".includes";
					FileItem ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(CompileEnvironment.Config.OutputDirectory, Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension));
					string ClangPath = System.IO.Path.Combine(CompileAction.WorkingDirectory, @"ThirdParty", @"llvm", @"3.6.2", @"bin", @"vs2015", @"x86", @"release", @"clang++.exe");
					UnrealCodeAnalyzerArguments = UCAMode + SourceFile.AbsolutePath + @" -OutputFile=""" + ObjectFile.AbsolutePath + @""" -- " + ClangPath + @" --driver-mode=cl ";
				}

				if (!ProjectFileGenerator.bGenerateProjectFiles
					&& !WindowsPlatform.bCompileWithClang
					&& CompileAction.ProducedItems.Count > 0)
				{
					FileItem TargetFile = CompileAction.ProducedItems[0];
					string ResponseFileName = TargetFile.AbsolutePath + ".response";
					List<string> ResponseLines = new List<string>();
					string ResponseLine = UnrealCodeAnalyzerArguments + SharedArguments.ToString() + FileArguments.ToString() + CompileEnvironment.Config.AdditionalArguments;
					ResponseLines.Add(ActionThread.ExpandEnvironmentVariables(ResponseLine));
					ResponseFile.Create(new FileReference(ResponseFileName), ResponseLines);
					CompileAction.CommandArguments = " @\"" + ResponseFileName + "\"";
					CompileAction.PrerequisiteItems.Add(FileItem.GetExistingItemByPath(ResponseFileName));
				}
				else
				{
					CompileAction.CommandArguments = UnrealCodeAnalyzerArguments + SharedArguments.ToString() + FileArguments.ToString() + CompileEnvironment.Config.AdditionalArguments;
				}

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					Log.TraceVerbose("Creating PCH " + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename + ": \"" + CompileAction.CommandPath + "\"" + CompileAction.CommandArguments);
				}
				else
				{
					Log.TraceVerbose("   Compiling " + CompileAction.StatusDescription + ": \"" + CompileAction.CommandPath + "\"" + CompileAction.CommandArguments);
				}

				if (WindowsPlatform.bCompileWithClang || BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					// Clang doesn't print the file names by default, so we'll do it ourselves
					CompileAction.bShouldOutputStatusDescription = true;
				}
				else
				{
					// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
					CompileAction.bShouldOutputStatusDescription = false;
				}

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					BuildConfiguration.bAllowRemotelyCompiledPCHs
					;

				// @todo: XGE has problems remote compiling C++/CLI files that use .NET Framework 4.0
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					CompileAction.bCanExecuteRemotely = false;
				}

				// When compiling with SN-DBS, modules that contain a #import must be built locally
				if (CompileEnvironment.Config.bBuildLocallyWithSNDBS == true && BuildConfiguration.bAllowSNDBS)
				{
					CompileAction.bCanExecuteRemotely = false;
				}
			}
			return Result;
		}

		public override CPPOutput CompileRCFiles(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles)
		{
			VCEnvironment EnvVars = VCEnvironment.SetEnvironment(Environment.Config.Target.Platform, bSupportWindowsXP);

			CPPOutput Result = new CPPOutput();

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Environment.Config.Target.Platform);

			foreach (FileItem RCFile in RCFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Resource";
				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = EnvVars.ResourceCompilerPath;
				CompileAction.StatusDescription = Path.GetFileName(RCFile.AbsolutePath);

				// Resource tool can run remotely if possible
				CompileAction.bCanExecuteRemotely = true;

				StringBuilder Arguments = new StringBuilder();

				if (WindowsPlatform.bCompileWithClang)
				{
					CompileAction.OutputEventHandler = new DataReceivedEventHandler(ClangCompilerOutputFormatter);
				}

				// Suppress header spew
				Arguments.Append(" /nologo");

				// If we're compiling for 64-bit Windows, also add the _WIN64 definition to the resource
				// compiler so that we can switch on that in the .rc file using #ifdef.
				if (Environment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					AddDefinition(Arguments, "_WIN64");
				}

				// When targeting Windows XP with Visual Studio 2012+, we need to tell the compiler to use the older Windows SDK that works
				// with Windows XP (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
				if (bSupportWindowsXP)
				{
					AddDefinition(Arguments, "_USING_V110_SDK71_");
				}

				// Language
				Arguments.AppendFormat(" /l 0x409");

				// Include paths.
				foreach (string IncludePath in Environment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath(Arguments, IncludePath);
				}

				// System include paths.
				foreach (string SystemIncludePath in Environment.Config.CPPIncludeInfo.SystemIncludePaths)
				{
					AddIncludePath(Arguments, SystemIncludePath);
				}

				// Preprocessor definitions.
				foreach (string Definition in Environment.Config.Definitions)
				{
					AddDefinition(Arguments, Definition);
				}

				// Add the RES file to the produced item list.
				FileItem CompiledResourceFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						Environment.Config.OutputDirectory,
						Path.GetFileName(RCFile.AbsolutePath) + ".res"
						)
					);
				CompileAction.ProducedItems.Add(CompiledResourceFile);
				Arguments.AppendFormat(" /fo \"{0}\"", CompiledResourceFile.AbsolutePath);
				Result.ObjectFiles.Add(CompiledResourceFile);

				// Add the RC file as a prerequisite of the action.
				Arguments.AppendFormat(" \"{0}\"", RCFile.AbsolutePath);

				CompileAction.CommandArguments = Arguments.ToString();

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(Target, BuildPlatform, Environment, RCFile, CompileAction.PrerequisiteItems);
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			VCEnvironment EnvVars = VCEnvironment.SetEnvironment(LinkEnvironment.Config.Target.Platform, bSupportWindowsXP);

			// @todo UWP: These paths should be added in SetUpEnvironment(), not here.  Also is this actually needed for classic desktop targets or only UWP?
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
			{
				if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					LinkEnvironment.Config.LibraryPaths.Add(string.Format("{0}/Lib/um/x64", EnvVars.NetFxSDKExtensionDir));
				}
				else if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32)
				{
					LinkEnvironment.Config.LibraryPaths.Add(string.Format("{0}/Lib/um/x86", EnvVars.NetFxSDKExtensionDir));
				}
			}

			if (LinkEnvironment.Config.bIsBuildingDotNetAssembly)
			{
				return FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
			}

			bool bIsBuildingLibrary = LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly;
			bool bIncludeDependentLibrariesInLibrary = bIsBuildingLibrary && LinkEnvironment.Config.bIncludeDependentLibrariesInLibrary;

			// Get link arguments.
			StringBuilder Arguments = new StringBuilder();
			if (bIsBuildingLibrary)
			{
				AppendLibArguments(LinkEnvironment, Arguments);
			}
			else
			{
				AppendLinkArguments(LinkEnvironment, Arguments);
			}

			if (!WindowsPlatform.bCompileWithClang && WindowsPlatform.bLogDetailedCompilerTimingInfo)
			{
				Arguments.Append(" /time+");
			}

			// If we're only building an import library, add the '/DEF' option that tells the LIB utility
			// to simply create a .LIB file and .EXP file, and don't bother validating imports
			if (bBuildImportLibraryOnly)
			{
				Arguments.Append(" /DEF");

				// Ensure that the import library references the correct filename for the linked binary.
				Arguments.AppendFormat(" /NAME:\"{0}\"", LinkEnvironment.Config.OutputFilePath.GetFileName());
			}


			// Add delay loaded DLLs.
			if (!bIsBuildingLibrary)
			{
				// Delay-load these DLLs.
				foreach (string DelayLoadDLL in LinkEnvironment.Config.DelayLoadDLLs)
				{
					Arguments.AppendFormat(" /DELAYLOAD:\"{0}\"", DelayLoadDLL);
				}
			}

			// @todo UE4 DLL: Why do I need LIBPATHs to build only export libraries with /DEF? (tbbmalloc.lib)
			if (!LinkEnvironment.Config.bIsBuildingLibrary || (LinkEnvironment.Config.bIsBuildingLibrary && bIncludeDependentLibrariesInLibrary))
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
				{
					Arguments.AppendFormat(" /LIBPATH:\"{0}\"", LibraryPath);
				}

				// Add the excluded default libraries to the argument list.
				foreach (string ExcludedLibrary in LinkEnvironment.Config.ExcludedLibraries)
				{
					Arguments.AppendFormat(" /NODEFAULTLIB:\"{0}\"", ExcludedLibrary);
				}
			}

			// For targets that are cross-referenced, we don't want to write a LIB file during the link step as that
			// file will clobber the import library we went out of our way to generate during an earlier step.  This
			// file is not needed for our builds, but there is no way to prevent MSVC from generating it when
			// linking targets that have exports.  We don't want this to clobber our LIB file and invalidate the
			// existing timstamp, so instead we simply emit it with a different name
			FileReference ImportLibraryFilePath = FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory,
														 LinkEnvironment.Config.OutputFilePath.GetFileNameWithoutExtension() + ".lib");

			if (LinkEnvironment.Config.bIsCrossReferenced && !bBuildImportLibraryOnly)
			{
				ImportLibraryFilePath += ".suppressed";
			}

			FileItem OutputFile;
			if (bBuildImportLibraryOnly)
			{
				OutputFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
			}
			else
			{
				OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
				OutputFile.bNeedsHotReloadNumbersDLLCleanUp = LinkEnvironment.Config.bIsBuildingDLL;
			}

			List<FileItem> ProducedItems = new List<FileItem>();
			ProducedItems.Add(OutputFile);

			List<FileItem> PrerequisiteItems = new List<FileItem>();

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				PrerequisiteItems.Add(InputFile);
			}

			if (!bBuildImportLibraryOnly)
			{
				// Add input libraries as prerequisites, too!
				foreach (FileItem InputLibrary in LinkEnvironment.InputLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputLibrary.AbsolutePath));
					PrerequisiteItems.Add(InputLibrary);
				}
			}

			if (!bIsBuildingLibrary || (LinkEnvironment.Config.bIsBuildingLibrary && bIncludeDependentLibrariesInLibrary))
			{
				foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", AdditionalLibrary));

					// If the library file name has a relative path attached (rather than relying on additional
					// lib directories), then we'll add it to our prerequisites list.  This will allow UBT to detect
					// when the binary needs to be relinked because a dependent external library has changed.
					//if( !String.IsNullOrEmpty( Path.GetDirectoryName( AdditionalLibrary ) ) )
					{
						PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
					}
				}
			}

			// Create a response file for the linker
			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);

			// Never create response files when we are only generating IntelliSense data
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				ResponseFile.Create(ResponseFileName, InputFileNames);
			}
			Arguments.AppendFormat(" @\"{0}\"", ResponseFileName);

			// Add the output file to the command-line.
			Arguments.AppendFormat(" /OUT:\"{0}\"", OutputFile.AbsolutePath);

			if (bBuildImportLibraryOnly || (LinkEnvironment.Config.bHasExports && !bIsBuildingLibrary))
			{
				// An export file is written to the output directory implicitly; add it to the produced items list.
				FileReference ExportFilePath = ImportLibraryFilePath.ChangeExtension(".exp");
				FileItem ExportFile = FileItem.GetItemByFileReference(ExportFilePath);
				ProducedItems.Add(ExportFile);
			}

			if (!bIsBuildingLibrary)
			{
				// There is anything to export
				if (LinkEnvironment.Config.bHasExports
					// Shipping monolithic builds don't need exports
					&& (!((LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping) && (LinkEnvironment.bShouldCompileMonolithic != false))))
				{
					// Write the import library to the output directory for nFringe support.
					FileItem ImportLibraryFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
					Arguments.AppendFormat(" /IMPLIB:\"{0}\"", ImportLibraryFilePath);
					ProducedItems.Add(ImportLibraryFile);
				}

				if (LinkEnvironment.Config.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					{
						FileReference PDBFilePath = FileReference.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
						FileItem PDBFile = FileItem.GetItemByFileReference(PDBFilePath);
						Arguments.AppendFormat(" /PDB:\"{0}\"", PDBFilePath);
						ProducedItems.Add(PDBFile);
					}

					// Write the MAP file to the output directory.			
					if (BuildConfiguration.bCreateMapFile)
					{
						FileReference MAPFilePath = FileReference.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByFileReference(MAPFilePath);
						Arguments.AppendFormat(" /MAP:\"{0}\"", MAPFilePath);
						ProducedItems.Add(MAPFile);

						// Export a list of object file paths, so we can locate the object files referenced by the map file
						ExportObjectFilePaths(LinkEnvironment, Path.ChangeExtension(MAPFilePath.FullName, ".objpaths"));
					}
				}

				// Add the additional arguments specified by the environment.
				Arguments.Append(LinkEnvironment.Config.AdditionalArguments);
			}


			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.CommandDescription = "Link";
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = bIsBuildingLibrary ? EnvVars.LibraryLinkerPath : EnvVars.LinkerPath;
			LinkAction.CommandArguments = Arguments.ToString();
			LinkAction.ProducedItems.AddRange(ProducedItems);
			LinkAction.PrerequisiteItems.AddRange(PrerequisiteItems);
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			// ensure compiler timings are captured when we execute the action.
			if (!WindowsPlatform.bCompileWithClang && WindowsPlatform.bLogDetailedCompilerTimingInfo)
			{
				LinkAction.bPrintDebugInfo = true;
			}

			if (WindowsPlatform.bCompileWithClang || WindowsPlatform.bCompileWithICL)
			{
				LinkAction.OutputEventHandler = new DataReceivedEventHandler(ClangCompilerOutputFormatter);
			}

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL;

			// Allow remote linking.  Especially in modular builds with many small DLL files, this is almost always very efficient
			LinkAction.bCanExecuteRemotely = true;

			Log.TraceVerbose("     Linking: " + LinkAction.StatusDescription);
			Log.TraceVerbose("     Command: " + LinkAction.CommandArguments);

			return OutputFile;
		}

		private void ExportObjectFilePaths(LinkEnvironment LinkEnvironment, string FileName)
		{
			// Write the list of object file directories
			HashSet<DirectoryReference> ObjectFileDirectories = new HashSet<DirectoryReference>();
			foreach(FileItem InputFile in LinkEnvironment.InputFiles)
			{
				ObjectFileDirectories.Add(InputFile.Reference.Directory);
			}
			foreach(FileItem InputLibrary in LinkEnvironment.InputLibraries)
			{
				ObjectFileDirectories.Add(InputLibrary.Reference.Directory);
			}
			foreach(string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries.Where(x => Path.IsPathRooted(x)))
			{
				ObjectFileDirectories.Add(new FileReference(AdditionalLibrary).Directory);
			}
			foreach(string LibraryPath in LinkEnvironment.Config.LibraryPaths)
			{
				ObjectFileDirectories.Add(new DirectoryReference(LibraryPath));
			}
			foreach(string LibraryPath in (Environment.GetEnvironmentVariable("LIB") ?? "").Split(new char[]{ ';' }, StringSplitOptions.RemoveEmptyEntries))
			{
				ObjectFileDirectories.Add(new DirectoryReference(LibraryPath));
			}
			Directory.CreateDirectory(Path.GetDirectoryName(FileName));
			File.WriteAllLines(FileName, ObjectFileDirectories.Select(x => x.FullName).OrderBy(x => x).ToArray());
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, FileReference ProjectFileName, FileReference DestinationFile)
		{
			// Initialize environment variables required for spawned tools.
			VCEnvironment EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.EnvironmentTargetPlatform, bSupportWindowsXP);

			Action BuildProjectAction = new Action(ActionType.BuildProject);

			// Specify the source file (prerequisite) for the action
			FileItem ProjectFileItem = FileItem.GetExistingItemByFileReference(ProjectFileName);
			if (ProjectFileItem == null)
			{
				throw new BuildException("Expected C# project file {0} to exist.", ProjectFileName);
			}

			// Add the project and the files contained to the prerequisites.
			BuildProjectAction.PrerequisiteItems.Add(ProjectFileItem);
			VCSharpProjectFile ProjectFile = new VCSharpProjectFile(ProjectFileName);
			List<string> ProjectPreReqs = ProjectFile.GetCSharpDependencies();
			DirectoryReference ProjectFolder = ProjectFileName.Directory;
			foreach (string ProjectPreReqRelativePath in ProjectPreReqs)
			{
				FileReference ProjectPreReqAbsolutePath = FileReference.Combine(ProjectFolder, ProjectPreReqRelativePath);
				FileItem ProjectPreReqFileItem = FileItem.GetExistingItemByFileReference(ProjectPreReqAbsolutePath);
				if (ProjectPreReqFileItem == null)
				{
					throw new BuildException("Expected C# dependency {0} to exist.", ProjectPreReqAbsolutePath);
				}
				BuildProjectAction.PrerequisiteItems.Add(ProjectPreReqFileItem);
			}

			// We might be able to distribute this safely, but it doesn't take any time.
			BuildProjectAction.bCanExecuteRemotely = false;

			// Setup execution via MSBuild.
			BuildProjectAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			BuildProjectAction.StatusDescription = ProjectFileName.GetFileName();
			BuildProjectAction.CommandPath = EnvVars.MSBuildPath;
			if (CompileEnvironment.TargetConfiguration == CSharpTargetConfiguration.Debug)
			{
				BuildProjectAction.CommandArguments = " /target:rebuild /property:Configuration=Debug";
			}
			else
			{
				BuildProjectAction.CommandArguments = " /target:rebuild /property:Configuration=Development";
			}

			// Be less verbose
			BuildProjectAction.CommandArguments += " /nologo /verbosity:minimal";

			// Add project
			BuildProjectAction.CommandArguments += String.Format(" \"{0}\"", ProjectFileItem.AbsolutePath);

			// Specify the output files.
			FileReference PDBFilePath = FileReference.Combine(DestinationFile.Directory, DestinationFile.GetFileNameWithoutExtension() + ".pdb");
			FileItem PDBFile = FileItem.GetItemByFileReference(PDBFilePath);
			BuildProjectAction.ProducedItems.Add(FileItem.GetItemByFileReference(DestinationFile));
			BuildProjectAction.ProducedItems.Add(PDBFile);
		}

		/// <summary>
		/// Gets the default include paths for the given platform.
		/// </summary>
		public static string GetVCIncludePaths(CPPTargetPlatform Platform, bool bSupportWindowsXP)
		{
			Debug.Assert(Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64);

			// Make sure we've got the environment variables set up for this target
			VCEnvironment.SetEnvironment(Platform, bSupportWindowsXP);

			// Also add any include paths from the INCLUDE environment variable.  MSVC is not necessarily running with an environment that
			// matches what UBT extracted from the vcvars*.bat using SetEnvironmentVariablesFromBatchFile().  We'll use the variables we
			// extracted to populate the project file's list of include paths
			// @todo projectfiles: Should we only do this for VC++ platforms?
			string IncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
			if (!String.IsNullOrEmpty(IncludePaths) && !IncludePaths.EndsWith(";"))
			{
				IncludePaths += ";";
			}

			return IncludePaths;
		}

		public override void ModifyBuildProducts(UEBuildBinary Binary, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary)
			{
				BuildProducts.Add(FileReference.Combine(Binary.Config.IntermediateDirectory, Binary.Config.OutputFilePath.GetFileNameWithoutExtension() + ".lib"), BuildProductType.ImportLibrary);
			}
		}


		/// <summary>
		/// Formats compiler output from Clang so that it is clickable in Visual Studio
		/// </summary>
		protected static void ClangCompilerOutputFormatter(object sender, DataReceivedEventArgs e)
		{
			string Output = e.Data;
			if (string.IsNullOrEmpty(Output))
			{
				return;
			}

			// Simplify some assumptions
			Output = Output.Replace("\r\n", "\n");
			Output = Output.Replace("/", "\\");

			// Clang has two main types of line number formats for diagnostic messages:
			const string SourceExtensions = "cpp|c|cc|cxx|mm|m|hpp|h|inl";
			const string RegexLineAndColumnNumber = @"\.(?:" + SourceExtensions + @")\(\d+(,\d+)\)";	// 'foo.cpp(30,10): warning : [...]'
			const string RegexEndOfLineNumber = @"\.(?:" + SourceExtensions + @")\:(\d+)\:";			// 'In file included from foo.cpp:30:'

			// We need to replace both of them with MSVC's format ('foo.cpp(30): warning') to make them colourized and clickable.
			// Note: MSVC does support clickable column numbers, but adding them makes the colours disappear which is much worse
			foreach (var Group in Regex.Matches(Output, RegexLineAndColumnNumber)
				.Cast<Match>()
				.Where(m => m.Groups.Count == 2)
				.Select(m => m.Groups[1])
				.OrderByDescending(g => g.Index)) // Work backwards so the match indices remain valid
			{
				string Left = Output.Substring(0, Group.Index) + "):";
				string Remainder = (Group.Index + Group.Length < Output.Length)
					? Output.Substring(Group.Index + Group.Length + 2, Output.Length - (Group.Index + Group.Length + 2))
					: "";
				Output = Left + Remainder;
			}

			foreach (var Group in Regex.Matches(Output, RegexEndOfLineNumber)
				.Cast<Match>()
				.Where(m => m.Groups.Count == 2)
				.Select(m => m.Groups[1])
				.OrderByDescending(g => g.Index))
			{
				string Left = Output.Substring(0, Group.Index - 1);
				string LineNumber = "(" + Group.Value + "):";
				string Remainder = (Group.Index + Group.Length < Output.Length)
					? Output.Substring(Group.Index + Group.Length + 1, Output.Length - (Group.Index + Group.Length + 1))
					: "";
				Output = Left + LineNumber + Remainder;
			}

			// Move the weird misplaced space in clang's 'note' diagnostic to make it consistent with warnings/errors, it bugs my OCD
			Output = Output.Replace(":  note", " : note ");

			LogEventType Verbosity = LogEventType.Console;
			const string RegexCompilerDiagnostic = @"([\s\:]error[\s\:]|[\s\:]warning[\s\:]|[\s\:]note[\s\:]).*";
			const string RegexFilePath =  @"([A-Za-z]:?)*([\\\/][A-Za-z0-9_\-\.\ ]*)+\.(" + SourceExtensions + ")";
			const string RegexAbsoluteFilePath = @"^(?:[A-Za-z]:)?\\";

			// An output string usually consists of multiple lines, each of which may contain a path
			string[] Lines = Output.Split(new[] {'\n'});
			for (int LineNum = 0; LineNum < Lines.Length; ++LineNum)
			{
				string Line = Lines[LineNum];
				if (Line.Length > 2 && Line[0] == '\\' && Line[1] != '\\')
				{
					Line = Line.Substring(1);
				}

				// Only match notices, warnings and errors. It is sometimes convenient to have everything clickable, but it gets spammy
				// with long paths and there are also cases where paths get incorrectly converted (e.g. include paths in errors)
				string DiagnosticLevel = Regex.Match(Line, RegexCompilerDiagnostic).Value.TrimStart().ToLowerInvariant();
				if (DiagnosticLevel.Length == 0)
				{
					continue;
				}

				// Set the verbosity level so that UBT console output is colourized
				if (DiagnosticLevel.Contains("error"))
				{
					Verbosity = (LogEventType)Math.Min((int)Verbosity, (int)LogEventType.Error);
				}
				else if (DiagnosticLevel.Contains("warning"))
				{
					Verbosity = (LogEventType)Math.Min((int)Verbosity, (int)LogEventType.Warning);
				}

				string FilePath = Regex.Match(Line, RegexFilePath).Value;
				bool bAbsoluteFilePath = Regex.IsMatch(FilePath, RegexAbsoluteFilePath); // If match, no need to convert
				if (FilePath.Length > 0 && !bAbsoluteFilePath)
				{
					// This assumes all relative source file paths to come from Engine/Source, which is true as far as I know.
					// Errors in e.g. Engine/Plugins seem to always be reported with their absolute paths.
					string EngineSourcePath = Path.Combine(BuildConfiguration.RelativeEnginePath, "Source");
					string AbsoluteFilePath = Path.GetFullPath(Path.Combine(EngineSourcePath, FilePath));
					Lines[LineNum] = Line.Replace(FilePath, AbsoluteFilePath);
				}
			}

			Output = string.Join("\n", Lines);
			Log.WriteLine(Verbosity, Output);
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			string PDBCopyPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "Microsoft", "VisualStudio", "v14.0", "AppxPackage", "PDBCopy.exe");
			if (!File.Exists(PDBCopyPath))
			{
				// Fall back on VS2013 version
				PDBCopyPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "Microsoft", "VisualStudio", "v12.0", "AppxPackage", "PDBCopy.exe");
			}
			StartInfo.FileName = PDBCopyPath;
			StartInfo.Arguments = String.Format("\"{0}\" \"{1}\" -p", SourceFileName, TargetFileName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	};
}
