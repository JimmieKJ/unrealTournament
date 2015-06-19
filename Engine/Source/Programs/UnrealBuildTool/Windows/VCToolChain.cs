// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Text;

namespace UnrealBuildTool
{
	public class VCToolChain : UEToolChain
	{
		public override void RegisterToolChain()
		{
			// Register this tool chain for both Win64 and Win32
			Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.Win64.ToString());
			UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.Win64, this);
			Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.Win32.ToString());
			UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.Win32, this);
		}


		static void AddDefinition( StringBuilder String, string Definition )
		{
			// Split the definition into name and value
			int ValueIdx = Definition.IndexOf('=');
			if(ValueIdx == -1)
			{
				AddDefinition(String, Definition, null);
			}
			else
			{
				AddDefinition(String, Definition.Substring(0, ValueIdx), Definition.Substring(ValueIdx + 1));
			}
		}


		static void AddDefinition( StringBuilder String, string Variable, string Value )
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now
			if( Value != null && !Value.StartsWith( "\"" ) && ( Value.Contains( " " ) || Value.Contains( "$" ) ) )
			{
				Value = "\"" + Value + "\"";
			}

			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				if( Value != null )
				{ 
					String.Append( " /D" + Variable + "=" + Value );
				}
				else
				{
					String.Append( " /D" + Variable );
				}
			}
			else
			{
				if( Value != null )
				{ 
					String.Append( " -D " + Variable + "=" + Value );
				}
				else
				{
					String.Append( " -D " + Variable );
				}
			}
		}


		static void AddIncludePath( StringBuilder String, string IncludePath )
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now
			if( !IncludePath.StartsWith( "\"" ) && ( IncludePath.Contains( " " ) || IncludePath.Contains( "$" ) ) )
			{
				IncludePath = "\"" + IncludePath + "\"";
			}

			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				String.Append( " /I " + IncludePath );
			}
			else
			{
				String.Append( " -I" + IncludePath );
			}
		}


		static void AppendCLArguments_Global(CPPEnvironment CompileEnvironment, VCEnvironment EnvVars, StringBuilder Arguments)
		{
			// NOTE: Uncommenting this line will print includes as they are encountered by the preprocessor.  This can help with diagnosing include order problems.
			if( WindowsPlatform.bCompileWithClang && !WindowsPlatform.bUseVCCompilerArgs )
			{ 
				// Arguments.Append( " -H" );
			}
			else
			{
				// Arguments.Append( " /showIncludes" );
			}

			if( WindowsPlatform.bCompileWithClang )
			{ 
				// Arguments.Append( " -###" );	// @todo clang: Print Clang command-lines (instead of outputting compile results!)

				if( !WindowsPlatform.bUseVCCompilerArgs )
				{
					Arguments.Append( " -std=c++11" );
					Arguments.Append( " -fdiagnostics-format=msvc" );
					Arguments.Append( " -Xclang -relaxed-aliasing -Xclang --dependent-lib=msvcrt -Xclang --dependent-lib=oldnames -gline-tables-only -ffunction-sections" );
				}

				// @todo clang: We're impersonating the Visual C++ compiler by setting MSC_VER and _MSC_FULL_VER to values that MSVC would set
				string VersionString;
				string FullVersionString;
				switch( WindowsPlatform.Compiler )
				{
					case WindowsCompiler.VisualStudio2012:
						VersionString = "17.0";
						FullVersionString = "1700";
						break;
		
					case WindowsCompiler.VisualStudio2013:
						VersionString = "18.0";
						FullVersionString = "1800";
						break;

					default:
						throw new BuildException( "Unexpected value for WindowsPlatform.Compiler: " + WindowsPlatform.Compiler.ToString() );
				}

				Arguments.Append(" -fms-compatibility-version=" + VersionString);
				AddDefinition( Arguments, "_MSC_FULL_VER", FullVersionString + "00000" );
			}

			// @todo clang: Clang on Windows doesn't respect "#pragma warning (error: ####)", and we're not passing "/WX", so warnings are not
			// treated as errors when compiling on Windows using Clang right now.


			if( BuildConfiguration.bEnableCodeAnalysis )
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

			if( WindowsPlatform.bUseVCCompilerArgs )
			{ 
				// Prevents the compiler from displaying its logo for each invocation.
				Arguments.Append(" /nologo");

				// Enable intrinsic functions.
				Arguments.Append(" /Oi");	// @todo clang: No Clang equivalent to this?
			}

			
			if( WindowsPlatform.bCompileWithClang )
			{
				// Tell the Clang compiler whether we want to generate 32-bit code or 64-bit code
				if( CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64 )
				{
					Arguments.Append( " --target=x86_64-pc-windows-msvc" );
				}
				else
				{
					Arguments.Append( " --target=x86-pc-windows-msvc" );
				}
			}

			// Compile into an .obj file, and skip linking.
			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				Arguments.Append(" /c");
			}
			else
			{
				Arguments.Append(" -c");
			}

			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				// Separate functions for linker.
				Arguments.Append(" /Gy");

				// Allow 800% of the default memory allocation limit.
				Arguments.Append(" /Zm800");

				// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
				Arguments.Append(" /wd4819");
			}

			if( BuildConfiguration.bUseSharedPCHs )
			{
				if( WindowsPlatform.bUseVCCompilerArgs )
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
				AddDefinition( Arguments, "_WINDLL" );
			}

			// When targeting Windows XP with Visual Studio 2012+, we need to tell the compiler to use the older Windows SDK that works
			// with Windows XP (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
			if (WindowsPlatform.IsWindowsXPSupported())
			{
				AddDefinition( Arguments, "_USING_V110_SDK71_");
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
				if( WindowsPlatform.bUseVCCompilerArgs )
				{ 
					// Disable compiler optimization.
					Arguments.Append(" /Od");

					// Favor code size (especially useful for embedded platforms).
					Arguments.Append(" /Os");

					// Allow inline method expansion unless E&C support is requested
					if( !BuildConfiguration.bSupportEditAndContinue )
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
				if( WindowsPlatform.bUseVCCompilerArgs )
				{ 
					if(CompileEnvironment.Config.OptimizeCode == ModuleRules.CodeOptimization.InShippingBuildsOnly && CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Shipping)
					{
						// Disable compiler optimization.
						Arguments.Append(" /Od");

						// Favor code size (especially useful for embedded platforms).
						Arguments.Append(" /Os");
					}
					else
					{
						// Maximum optimizations if desired.
						if( CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds )
						{
							Arguments.Append(" /Ox");
						}
				
						// Favor code speed.
						Arguments.Append(" /Ot");

						// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
						if ( BuildConfiguration.bOmitFramePointers == false
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
						if( BuildConfiguration.bAllowLTCG )
						{
							// Enable link-time code generation.
							Arguments.Append(" /GL");
						}
					}
				}
				else
				{
					if(CompileEnvironment.Config.OptimizeCode != ModuleRules.CodeOptimization.InShippingBuildsOnly || CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
					{
						// Maximum optimizations if desired.
						if( CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds )
						{
							Arguments.Append( " -O3");
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
				// SSE options are not allowed when using CLR compilation or the 64 bit toolchain
				// (both enable SSE2 automatically)
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled &&
					CompileEnvironment.Config.Target.Platform != CPPTargetPlatform.Win64)
				{
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						// Allow the compiler to generate SSE2 instructions.
						Arguments.Append(" /arch:SSE2");
					}
				}

				if( WindowsPlatform.bUseVCCompilerArgs )
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

						if( !WindowsPlatform.bUseVCCompilerArgs )
						{ 
							Arguments.Append( " -fno-exceptions" );
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
				if( WindowsPlatform.bUseVCCompilerArgs )
				{ 
					// Store debug info in .pdb files.
					// @todo clang: PDB files are emited from Clang but do not fully work with Visual Studio yet (breakpoints won't hit due to "symbol read error")
					if( BuildConfiguration.bUsePDBFiles )
					{
						// Create debug info suitable for E&C if wanted.
						if( BuildConfiguration.bSupportEditAndContinue &&
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
						if (BuildConfiguration.bUseIncrementalLinking && WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
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
			if( CompileEnvironment.Config.bUseStaticCRT )
			{
				if( CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
				{
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /MTd");
					}
					else
					{
						AddDefinition( Arguments, "_MT" );
						AddDefinition( Arguments, "_DEBUG" );
					}
				}
				else
				{
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /MT");
					}
					else
					{
						AddDefinition( Arguments, "_MT" );
					}
				}
			}
			else
			{
				if( CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
				{
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /MDd");
					}
					else
					{
						AddDefinition( Arguments, "_MT" );
						AddDefinition( Arguments, "_DEBUG" );
						AddDefinition( Arguments, "_DLL" );
					}
				}
				else
				{
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /MD");
					}
					else
					{
						AddDefinition( Arguments, "_MT" );
						AddDefinition( Arguments, "_DLL" );
					}
				}
			}

			if (WindowsPlatform.bUseVCCompilerArgs && !BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				// @todo clang: Not supported in clang-cl yet
				if( !WindowsPlatform.bCompileWithClang )
				{
					// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
					Arguments.Append(" /bigobj");

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
						Arguments.Append(" /d2Zi+");
					}
				}
			}

			//
			// Options skipped for UnrealCodeAnalyzer
			//
			if (!BuildConfiguration.bRunUnrealCodeAnalyzer)	// Disabled when compiling UnrealCodeAnalyzer as it breaks compilation (some structs in clang/llvm headers require 8-byte alignment in 32-bit compilation)
			{
				if (CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					// Pack struct members on 8-byte boundaries.
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /Zp8");
					}
					else
					{
						Arguments.Append(" -fpack-struct=8" );
					}
				}
				else
				{
					// Pack struct members on 4-byte boundaries.
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /Zp4");
					}
					else
					{
						Arguments.Append(" -fpack-struct=4" );
					}
				}
			}

			//
			// Options required by UnrealCodeAnalyzer
			//
			if (BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				AddDefinition( Arguments, "UNREAL_CODE_ANALYZER");
			}
		}

		static void AppendCLArguments_CPP( CPPEnvironment CompileEnvironment, StringBuilder Arguments )
		{
			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				// Explicitly compile the file as C++.
				Arguments.Append(" /TP");
			}
			else
			{
				if( CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create )
				{ 
					// Tell Clang to generate a PCH header
					Arguments.Append(" -x c++-header");
				}
				else
				{
					Arguments.Append(" -x c++" );
				}
			}


			if (!CompileEnvironment.Config.bEnableBufferSecurityChecks)
			{
				if( WindowsPlatform.bUseVCCompilerArgs )
				{ 
					// This will disable buffer security checks (which are enabled by default) that the MS compiler adds around arrays on the stack,
					// Which can add some performance overhead, especially in performance intensive code
					// Only disable this if you know what you are doing, because it will be disabled for the entire module!
					Arguments.Append(" /GS-");
				}
			}

			// C++/CLI requires that RTTI is left enabled
			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
			{
				if (CompileEnvironment.Config.bUseRTTI)
				{
					// Enable C++ RTTI.
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /GR");
					}
					else
					{
						Arguments.Append(" -Xclang -frtti-data" );
					}
				}
				else
				{
					// Disable C++ RTTI.
					if( WindowsPlatform.bUseVCCompilerArgs )
					{ 
						Arguments.Append(" /GR-");
					}
					else
					{
						Arguments.Append(" -Xclang -fno-rtti-data" );
					}
				}
			}

			// Set warning level.
			if( WindowsPlatform.bUseVCCompilerArgs )
			{ 
				if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					// Restrictive during regular compilation.
					Arguments.Append(" /W4");
				}
				else
				{
					// If we had /W4 with clang on windows we would be flooded with warnings. This will be fixed incrementally.
					Arguments.Append(" /W0");
				}
			}
			else
			{
				Arguments.Append(" -Wall");	
			}

			if( WindowsPlatform.bCompileWithClang )
			{
				// Disable specific warnings that cause problems with Clang
				// NOTE: These must appear after we set the MSVC warning level

				// @todo clang: Ideally we want as few warnings disabled as possible
				// 
	
				// Allow Microsoft-specific syntax to slide, even though it may be non-standard.  Needed for Windows headers.
				Arguments.Append(" -Wno-microsoft");								

				// @todo clang: Hack due to how we have our 'DummyPCH' wrappers setup when using unity builds.  This warning should not be disabled!!
				Arguments.Append(" -Wno-msvc-include");

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
				Arguments.Append(" -Wno-undefined-bool-conversion" );

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
			}
		}
		
		static void AppendCLArguments_C(StringBuilder Arguments)
		{
			if( WindowsPlatform.bUseVCCompilerArgs )
			{
				Arguments.Append( " -x c");
			}
			else
			{ 
				// Explicitly compile the file as C.
				Arguments.Append(" /TC");
			}
		
			if( WindowsPlatform.bUseVCCompilerArgs )
			{ 
				// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
				Arguments.Append(" /W0");
			}
		}

		static void AppendLinkArguments(LinkEnvironment LinkEnvironment, StringBuilder Arguments)
		{
			if( WindowsPlatform.bCompileWithClang && WindowsPlatform.bAllowClangLinker )
			{
				// This tells LLD to run in "Windows emulation" mode, meaning that it will accept MSVC Link arguments
				Arguments.Append( " -flavor link" );

				// @todo clang: The following static libraries aren't linking correctly with Clang:
				//		tbbmalloc.lib, zlib_64.lib, libpng_64.lib, freetype2412MT.lib, IlmImf.lib
				//		LLD: Assertion failed: result.size() == 1, file ..\tools\lld\lib\ReaderWriter\FileArchive.cpp, line 71
				//		
			
				// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
				if ( !BuildConfiguration.bOmitFramePointers )
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
					if (WindowsPlatform.IsWindowsXPSupported())
					{
						Arguments.Append(LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win64 ? ",5.02" : ",5.01");
					}
				}

				if (LinkEnvironment.Config.bIsBuildingConsoleApplication && !LinkEnvironment.Config.bIsBuildingDLL && !String.IsNullOrEmpty( LinkEnvironment.Config.WindowsEntryPointOverride ) )
				{
					// Use overridden entry point
					Arguments.Append(" /ENTRY:" + LinkEnvironment.Config.WindowsEntryPointOverride);
				}

				// Allow the OS to load the EXE at different base addresses than its preferred base address.
				Arguments.Append(" /FIXED:No");

				// Option is only relevant with 32 bit toolchain.
				if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.Win32)
				{
					// Disables the 2GB address space limit on 64-bit Windows and 32-bit Windows with /3GB specified in boot.ini
					Arguments.Append(" /LARGEADDRESSAWARE");
				}

				// Explicitly declare that the executable is compatible with Data Execution Prevention.
				Arguments.Append(" /NXCOMPAT");

				// Set the default stack size.
				Arguments.Append(" /STACK:5000000");

				// E&C can't use /SAFESEH.  Also, /SAFESEH isn't compatible with 64-bit linking
				if( !BuildConfiguration.bSupportEditAndContinue &&
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
			if( BuildConfiguration.bAllowLTCG &&
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

		static void AppendLibArguments(LinkEnvironment LinkEnvironment, StringBuilder Arguments)
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
					if (WindowsPlatform.IsWindowsXPSupported())
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
			var EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.Config.Target.Platform);

			StringBuilder SharedArguments = new StringBuilder();
			AppendCLArguments_Global(CompileEnvironment, EnvVars, SharedArguments);

			// Add include paths to the argument list.
			if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
			{
				foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath( SharedArguments, IncludePath );
				}
			}
			else
			{
				foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath( SharedArguments, System.IO.Path.GetFullPath(IncludePath) );
				}
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
				if( WindowsPlatform.bCompileWithClang )
				{
					// @todo Clang: Clang uses a special command-line syntax for system headers.  This is used for two reasons.  The first is that Clang will automatically 
					// suppress compiler warnings in headers found in these directories, such as the DirectX SDK headers.  The other reason this is important is in the case 
					// where there the same header include path is passed as both a regular include path and a system include path (extracted from INCLUDE environment).  In 
					// this case Clang will ignore any earlier occurrence of the include path, preventing a system header include path from overriding a different system 
					// include path set later on by a module.  NOTE: When passing "-Xclang", these options will always appear at the end of the command-line string, meaning
					// they will be forced to appear *after* all environment-variable-extracted includes.  This is technically okay though.
					if( WindowsPlatform.bUseVCCompilerArgs )
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
					AddIncludePath( SharedArguments, IncludePath );
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
					SharedArguments.AppendFormat( " /FU \"{0}\"", AssemblyName );
				}

				// Add private assembly references				
				foreach( PrivateAssemblyInfo CurAssemblyInfo in CompileEnvironment.PrivateAssemblyDependencies )
				{
					SharedArguments.AppendFormat( " /FU \"{0}\"", CurAssemblyInfo.FileItem.AbsolutePath );
				}
			}


			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				var DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				AddDefinition(SharedArguments, DefinitionArgument);
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";

				StringBuilder FileArguments = new StringBuilder();
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile( Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems );

				// If this is a CLR file then make sure our dependent assemblies are added as prerequisites
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					foreach( PrivateAssemblyInfo CurPrivateAssemblyDependency in CompileEnvironment.PrivateAssemblyDependencies )
					{
						CompileAction.PrerequisiteItems.Add( CurPrivateAssemblyDependency.FileItem );
					}
				}

				if( WindowsPlatform.bCompileWithClang )
				{ 
					CompileAction.OutputEventHandler = new DataReceivedEventHandler( ClangCompilerOutputFormatter );
				}


				bool bEmitsObjectFile = true;
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Generate a CPP File that just includes the precompiled header.
					string PCHCPPFilename = "PCH." + Path.GetFileName(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename) + ".cpp";
					string PCHCPPPath = Path.Combine(CompileEnvironment.Config.OutputDirectory, PCHCPPFilename);
					FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
						PCHCPPPath,
						string.Format("#include \"{0}\"\r\n", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename)
						);

					// Make sure the original source directory the PCH header file existed in is added as an include
					// path -- it might be a private PCH header and we need to make sure that its found!
					string OriginalPCHHeaderDirectory = Path.GetDirectoryName( SourceFile.AbsolutePath );
					AddIncludePath( FileArguments, OriginalPCHHeaderDirectory );

					var PrecompiledFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Win64].GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					if( WindowsPlatform.bUseVCCompilerArgs )
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
							string FakeUniquePCHSymbolName = Path.GetFileNameWithoutExtension(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
							FileArguments.AppendFormat(" /Yl{0}", FakeUniquePCHSymbolName);
						}
					}
					else
					{
						FileArguments.Append( " -Xclang -emit-pch" );

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

						if( WindowsPlatform.bCompileWithClang )
						{
							// NOTE: With Clang, PCH headers are ALWAYS forcibly included!
							// NOTE: This needs to be before the other include paths to ensure Clang uses it instead of the source header file.
							if( WindowsPlatform.bUseVCCompilerArgs )
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
								FileArguments.AppendFormat( " /FI\"{0}\"", CompileEnvironment.Config.PCHHeaderNameInCode);
							}
						}
					}

					// UnrealCodeAnalyzer requires compiled file name to be first argument.
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						// Add the source file path to the command-line.
						FileArguments.AppendFormat(" \"{0}\"", SourceFile.AbsolutePath);
					}

					CompileAction.StatusDescription = Path.GetFileName( SourceFile.AbsolutePath );
				}

				if( bEmitsObjectFile )
				{
					var ObjectFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Win64].GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					// UnrealCodeAnalyzer requires specific position of output file.
					if (!BuildConfiguration.bRunUnrealCodeAnalyzer)
					{
						if( WindowsPlatform.bUseVCCompilerArgs )
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
						PDBFileName = "PCH." + Path.GetFileName(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					}
					// Files creating a PCH use a PDB per file.
					else if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
						PDBFileName = "PCH." + Path.GetFileName(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
						bActionProducesPDB = true;
					}
					// Ungrouped C++ files use a PDB per file.
					else if( !bIsPlainCFile )
					{
						PDBFileName = Path.GetFileName( SourceFile.AbsolutePath );
						bActionProducesPDB = true;
					}
					// Group all plain C files that doesn't use PCH into the same PDB
					else
					{
						PDBFileName = "MiscPlainC";
					}

					{ 
						// Specify the PDB file that the compiler should write to.
						FileItem PDBFile = FileItem.GetItemByPath(
								Path.Combine(
									CompileEnvironment.Config.OutputDirectory,
									PDBFileName + ".pdb"
									)
								);
						FileArguments.AppendFormat(" /Fd\"{0}\"", PDBFile.AbsolutePath);

						// Only use the PDB as an output file if we want PDBs and this particular action is
						// the one that produces the PDB (as opposed to no debug info, where the above code
						// is needed, but not the output PDB, or when multiple files share a single PDB, so
						// only the action that generates it should count it as output directly)
						if( BuildConfiguration.bUsePDBFiles && bActionProducesPDB )
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
					AppendCLArguments_CPP( CompileEnvironment, FileArguments );
				}

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				if (BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					CompileAction.CommandPath = System.IO.Path.Combine(CompileAction.WorkingDirectory, @"..", @"Binaries", @"Win32", @"NotForLicensees", @"UnrealCodeAnalyzer.exe");
				}
				else
				{
					CompileAction.CommandPath = EnvVars.CompilerPath;
				}

				string UnrealCodeAnalyzerArguments = "";
				if (BuildConfiguration.bRunUnrealCodeAnalyzer)
				{
					var ObjectFileExtension = @".includes";
					FileItem ObjectFile = FileItem.GetItemByPath(Path.Combine(CompileEnvironment.Config.OutputDirectory, Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension));
					var ClangPath = System.IO.Path.Combine(CompileAction.WorkingDirectory, @"ThirdParty", @"llvm", @"3.5.0", @"bin", @"vs2013", @"x86", @"release", @"clang++.exe");
					UnrealCodeAnalyzerArguments = @"-CreateIncludeFiles " + SourceFile.AbsolutePath + @" -OutputFile=""" + ObjectFile.AbsolutePath + @""" -- " + ClangPath + @" --driver-mode=cl ";
				}

				CompileAction.CommandArguments = UnrealCodeAnalyzerArguments + SharedArguments.ToString() + FileArguments.ToString() + CompileEnvironment.Config.AdditionalArguments;

				if( CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create )
				{
					Log.TraceVerbose("Creating PCH: " + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}
				else
				{
					Log.TraceVerbose("   Compiling: " + CompileAction.StatusDescription);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}

				if( WindowsPlatform.bCompileWithClang || BuildConfiguration.bRunUnrealCodeAnalyzer)
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
			}
			return Result;
		}

		public override CPPOutput CompileRCFiles(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles)
		{
			var EnvVars = VCEnvironment.SetEnvironment(Environment.Config.Target.Platform);

			CPPOutput Result = new CPPOutput();

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Environment.Config.Target.Platform);

			foreach (FileItem RCFile in RCFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Resource";
				CompileAction.WorkingDirectory   = Path.GetFullPath(".");
				CompileAction.CommandPath        = EnvVars.ResourceCompilerPath;
				CompileAction.StatusDescription  = Path.GetFileName(RCFile.AbsolutePath);

				// Resource tool can run remotely if possible
				CompileAction.bCanExecuteRemotely = true;

				var Arguments = new StringBuilder();

				if( WindowsPlatform.bCompileWithClang )
				{ 
					CompileAction.OutputEventHandler = new DataReceivedEventHandler( ClangCompilerOutputFormatter );
				}

				// Suppress header spew
				Arguments.Append( " /nologo" );

				// If we're compiling for 64-bit Windows, also add the _WIN64 definition to the resource
				// compiler so that we can switch on that in the .rc file using #ifdef.
				if (Environment.Config.Target.Platform == CPPTargetPlatform.Win64)
				{
					AddDefinition( Arguments, "_WIN64" );
				}

				// When targeting Windows XP with Visual Studio 2012+, we need to tell the compiler to use the older Windows SDK that works
				// with Windows XP (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
				if (WindowsPlatform.IsWindowsXPSupported())
				{
					AddDefinition( Arguments, "_USING_V110_SDK71_" );
				}

				// Language
				Arguments.AppendFormat( " /l 0x409" );

				// Include paths.
				foreach (string IncludePath in Environment.Config.CPPIncludeInfo.IncludePaths)
				{
					AddIncludePath( Arguments, IncludePath );
				}

				// System include paths.
				foreach( var SystemIncludePath in Environment.Config.CPPIncludeInfo.SystemIncludePaths )
				{
					AddIncludePath( Arguments, SystemIncludePath );
				}

				// Preprocessor definitions.
				foreach (string Definition in Environment.Config.Definitions)
				{
					AddDefinition( Arguments, Definition);
				}

				// Add the RES file to the produced item list.
				FileItem CompiledResourceFile = FileItem.GetItemByPath(
					Path.Combine(
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
				AddPrerequisiteSourceFile( Target, BuildPlatform, Environment, RCFile, CompileAction.PrerequisiteItems );
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			var EnvVars = VCEnvironment.SetEnvironment(LinkEnvironment.Config.Target.Platform);

			if (LinkEnvironment.Config.bIsBuildingDotNetAssembly)
			{
				return FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePath);
			}

			bool bIsBuildingLibrary = LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly;
			bool bIncludeDependentLibrariesInLibrary = bIsBuildingLibrary && LinkEnvironment.Config.bIncludeDependentLibrariesInLibrary;

			// Get link arguments.
			StringBuilder Arguments = new StringBuilder();
			if(bIsBuildingLibrary)
			{
				AppendLibArguments(LinkEnvironment, Arguments);
			}
			else
			{
				AppendLinkArguments(LinkEnvironment, Arguments);
			}



			// If we're only building an import library, add the '/DEF' option that tells the LIB utility
			// to simply create a .LIB file and .EXP file, and don't bother validating imports
			if (bBuildImportLibraryOnly)
			{
				Arguments.Append(" /DEF");

				// Ensure that the import library references the correct filename for the linked binary.
				Arguments.AppendFormat(" /NAME:\"{0}\"", Path.GetFileName(LinkEnvironment.Config.OutputFilePath));
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
			string ImportLibraryFilePath = Path.Combine( LinkEnvironment.Config.IntermediateDirectory, 
														 Path.GetFileNameWithoutExtension(LinkEnvironment.Config.OutputFilePath) + ".lib" );

			if (LinkEnvironment.Config.bIsCrossReferenced && !bBuildImportLibraryOnly)
			{
				ImportLibraryFilePath += ".suppressed";
			}

			FileItem OutputFile;
			if (bBuildImportLibraryOnly)
			{
				OutputFile = FileItem.GetItemByPath(ImportLibraryFilePath);
			}
			else
			{
				OutputFile = FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePath);
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
			string ResponseFileName = GetResponseFileName( LinkEnvironment, OutputFile );

			// Never create response files when we are only generating IntelliSense data
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				ResponseFile.Create( ResponseFileName, InputFileNames );
			}
			Arguments.AppendFormat(" @\"{0}\"", ResponseFileName);

			// Add the output file to the command-line.
			Arguments.AppendFormat(" /OUT:\"{0}\"", OutputFile.AbsolutePath);

			if (bBuildImportLibraryOnly || (LinkEnvironment.Config.bHasExports && !bIsBuildingLibrary))
			{
				// An export file is written to the output directory implicitly; add it to the produced items list.
				string ExportFilePath = Path.ChangeExtension(ImportLibraryFilePath, ".exp");
				FileItem ExportFile = FileItem.GetItemByPath(ExportFilePath);
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
					FileItem ImportLibraryFile = FileItem.GetItemByPath(ImportLibraryFilePath);
					Arguments.AppendFormat(" /IMPLIB:\"{0}\"", ImportLibraryFilePath);
					ProducedItems.Add(ImportLibraryFile);
				}

				if (LinkEnvironment.Config.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					{
						string PDBFilePath = Path.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
						FileItem PDBFile = FileItem.GetItemByPath(PDBFilePath);
						Arguments.AppendFormat(" /PDB:\"{0}\"", PDBFilePath);
						ProducedItems.Add(PDBFile);
					}

					// Write the MAP file to the output directory.			
#if false					
					if (true)
					{
						string MAPFilePath = Path.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByPath(MAPFilePath);
						Arguments.AppendFormat(" /MAP:\"{0}\"", MAPFilePath);
						LinkAction.ProducedItems.Add(MAPFile);
					}
#endif
				}

				// Add the additional arguments specified by the environment.
				Arguments.Append(LinkEnvironment.Config.AdditionalArguments);
			}


			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.CommandDescription = "Link";
			LinkAction.WorkingDirectory   = Path.GetFullPath(".");
			LinkAction.CommandPath        = bIsBuildingLibrary ? EnvVars.LibraryLinkerPath : EnvVars.LinkerPath;
			LinkAction.CommandArguments   = Arguments.ToString();
			LinkAction.ProducedItems    .AddRange(ProducedItems);
			LinkAction.PrerequisiteItems.AddRange(PrerequisiteItems);
			LinkAction.StatusDescription  = Path.GetFileName(OutputFile.AbsolutePath);

			if( WindowsPlatform.bCompileWithClang )
			{ 
				LinkAction.OutputEventHandler = new DataReceivedEventHandler( ClangCompilerOutputFormatter );
			}

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL;

			// Allow remote linking.  Especially in modular builds with many small DLL files, this is almost always very efficient
			LinkAction.bCanExecuteRemotely = true;

			Log.TraceVerbose( "     Linking: " + LinkAction.StatusDescription );
			Log.TraceVerbose( "     Command: " + LinkAction.CommandArguments );

			return OutputFile;
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile)
		{
			// Initialize environment variables required for spawned tools.
			var EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.EnvironmentTargetPlatform);

			var BuildProjectAction = new Action(ActionType.BuildProject);

			// Specify the source file (prerequisite) for the action
			var ProjectFileItem = FileItem.GetExistingItemByPath(ProjectFileName);
			if (ProjectFileItem == null)
			{
				throw new BuildException("Expected C# project file {0} to exist.", ProjectFileName);
			}
			
			// Add the project and the files contained to the prerequisites.
			BuildProjectAction.PrerequisiteItems.Add(ProjectFileItem);
			var ProjectFile = new VCSharpProjectFile( Utils.MakePathRelativeTo( ProjectFileName, ProjectFileGenerator.MasterProjectRelativePath ) );
			var ProjectPreReqs = ProjectFile.GetCSharpDependencies();
			var ProjectFolder = Path.GetDirectoryName(ProjectFileName);
			foreach( string ProjectPreReqRelativePath in ProjectPreReqs )
			{
				string ProjectPreReqAbsolutePath = Path.Combine( ProjectFolder, ProjectPreReqRelativePath );
				var ProjectPreReqFileItem = FileItem.GetExistingItemByPath(ProjectPreReqAbsolutePath);
				if( ProjectPreReqFileItem == null )
				{
					throw new BuildException("Expected C# dependency {0} to exist.", ProjectPreReqAbsolutePath);
				}
				BuildProjectAction.PrerequisiteItems.Add(ProjectPreReqFileItem);
			}

			// We might be able to distribute this safely, but it doesn't take any time.
			BuildProjectAction.bCanExecuteRemotely = false;

			// Setup execution via MSBuild.
			BuildProjectAction.WorkingDirectory  = Path.GetFullPath(".");
			BuildProjectAction.StatusDescription = Path.GetFileName(ProjectFileName);
			BuildProjectAction.CommandPath       = EnvVars.MSBuildPath;
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
			string PDBFilePath = Path.Combine( Path.GetDirectoryName(DestinationFile), Path.GetFileNameWithoutExtension(DestinationFile) + ".pdb" );
			FileItem PDBFile = FileItem.GetItemByPath( PDBFilePath );
			BuildProjectAction.ProducedItems.Add( FileItem.GetItemByPath(DestinationFile) );		
			BuildProjectAction.ProducedItems.Add( PDBFile );
		}

		/** Gets the default include paths for the given platform. */
		public static string GetVCIncludePaths(CPPTargetPlatform Platform)
		{
			Debug.Assert(Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64);

			// Make sure we've got the environment variables set up for this target
			VCEnvironment.SetEnvironment(Platform);

			// Also add any include paths from the INCLUDE environment variable.  MSVC is not necessarily running with an environment that
			// matches what UBT extracted from the vcvars*.bat using SetEnvironmentVariablesFromBatchFile().  We'll use the variables we
			// extracted to populate the project file's list of include paths
			// @todo projectfiles: Should we only do this for VC++ platforms?
			var IncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
			if (!String.IsNullOrEmpty(IncludePaths) && !IncludePaths.EndsWith(";"))
			{
				IncludePaths += ";";
			}

			return IncludePaths;
		}

        public override void AddFilesToReceipt(BuildReceipt Receipt, UEBuildBinary Binary)
        {
            if (Binary.Config.Type == UEBuildBinaryType.DynamicLinkLibrary)
            {
				Receipt.AddBuildProduct(Path.Combine(Binary.Config.IntermediateDirectory, Path.GetFileNameWithoutExtension(Binary.Config.OutputFilePath) + ".lib"), BuildProductType.ImportLibrary);
            }
        }


		/** Formats compiler output from Clang so that it is clickable in Visual Studio */
		protected static void ClangCompilerOutputFormatter(object sender, DataReceivedEventArgs e)
		{
			var Output = e.Data;
			if (Output == null)
			{
				return;
			}

			// @todo clang: Convert relative includes to absolute files so they'll be clickable
			Log.TraceInformation(Output);
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "Microsoft", "VisualStudio", "v12.0", "AppxPackage", "PDBCopy.exe");
			StartInfo.Arguments = String.Format("\"{0}\" \"{1}\" -p", SourceFileName, TargetFileName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	};
}
