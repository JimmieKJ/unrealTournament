// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

using Microsoft.Win32;

namespace UnrealBuildTool
{
	public class UWPToolChain : UEToolChain
	{
		public UWPToolChain() 
			: base(CPPTargetPlatform.UWP)
		{
		}

		static void AppendCLArguments_Global(CPPEnvironment CompileEnvironment, VCEnvironment EnvVars, StringBuilder Arguments)
		{
			//Arguments.Append(" /showIncludes");

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

			// Prevents the compiler from displaying its logo for each invocation.
			Arguments.Append(" /nologo");

			// Enable intrinsic functions.
			Arguments.Append(" /Oi");

			// Pack struct members on 8-byte boundaries.
			Arguments.Append(" /Zp8");

			// Separate functions for linker.
			Arguments.Append(" /Gy");

			// Relaxes floating point precision semantics to allow more optimization.
			Arguments.Append(" /fp:fast");

			// Compile into an .obj file, and skip linking.
			Arguments.Append(" /c");

			// Allow 800% of the default memory allocation limit.
			Arguments.Append(" /Zm800");

			// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
			Arguments.Append(" /bigobj");

			// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
			Arguments.Append(" /wd4819");

			// @todo UWP: Disable "unreachable code" warning since auto-included vccorlib.h triggers it
			Arguments.Append(" /wd4702");

			// @todo UWP: Silence the hash_map deprecation errors for now. This should be replaced with unordered_map for the real fix.
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				Arguments.Append(" /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS");
			}

			if (BuildConfiguration.bUseSharedPCHs)
			{
				// @todo SharedPCH: Disable warning about PCH defines not matching .cpp defines.  We "cheat" these defines a little
				// bit to make shared PCHs work.  But it's totally safe.  Trust us.
				Arguments.Append(" /wd4651");

				// @todo SharedPCH: Disable warning about redefining *API macros.  The PCH header is compiled with various DLLIMPORTs, but
				// when a module that uses that PCH header *IS* one of those imports, that module is compiled with EXPORTS, so the macro
				// is redefined on the command-line.  We need to clobber those defines to make shared PCHs work properly!
				Arguments.Append(" /wd4005");
			}

			// If compiling as a DLL, set the relevant defines
			if (CompileEnvironment.Config.bIsBuildingDLL)
			{
				Arguments.Append(" /D_WINDLL");
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
				// Disable compiler optimization.
				Arguments.Append(" /Od");

				// Favor code size (especially useful for embedded platforms).
				Arguments.Append(" /Os");

				// Allow inline method expansion unless E&C support is requested
				if (!BuildConfiguration.bSupportEditAndContinue)
				{
					// @todo UWP: No inlining in Debug builds except in the editor where DLL exports/imports aren't handled properly in module _API macros.
					if (UEBuildConfiguration.bBuildEditor)
					{
						Arguments.Append(" /Ob2");
					}
				}

				// Runtime stack checks are not allowed when compiling for CLR
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
				{
					Arguments.Append(" /RTCs");
				}
			}
			//
			//	Development and LTCG
			//
			else
			{
				// Maximum optimizations if desired.
				if (CompileEnvironment.Config.OptimizeCode >= ModuleRules.CodeOptimization.InNonDebugBuilds)
				{
					Arguments.Append(" /Ox");

					// Allow optimized code to be debugged more easily.  This makes PDBs a bit larger, but doesn't noticeably affect
					// compile times.  The executable code is not affected at all by this switch, only the debugging information.
					Arguments.Append(" /Zo");
				}

				// Favor code speed.
				Arguments.Append(" /Ot");

				// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
				if (BuildConfiguration.bOmitFramePointers == false
				&& (CompileEnvironment.Config.Target.Platform == CPPTargetPlatform.UWP))
				{
					Arguments.Append(" /Oy-");
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

			//
			//	PC
			//

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Append(" /errorReport:prompt");

			// Enable C++ exception handling, but not C exceptions.
			Arguments.Append(" /EHsc");

			// If enabled, create debug information.
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				// Store debug info in .pdb files.
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
					if (BuildConfiguration.bUseIncrementalLinking && WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2013)
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

			// Specify the appropriate runtime library based on the platform and config.
			if (CompileEnvironment.Config.bUseStaticCRT)
			{
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					Arguments.Append(" /MTd");
				}
				else
				{
					Arguments.Append(" /MT");
				}
			}
			else
			{
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					Arguments.Append(" /MDd");
				}
				else
				{
					Arguments.Append(" /MD");
				}
			}

			if (UWPPlatform.bBuildForStore)
			{
				Arguments.Append(" /D_BUILD_FOR_STORE=1");
			}

			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				// @todo UWP: These must be appended to the end of the system include list, lest they override some of the third party sources includes
				if (Directory.Exists(EnvVars.WindowsSDKExtensionDir))
				{
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\ucrt", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\um", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\shared", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
					CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths.Add(string.Format(@"{0}\Include\{1}\winrt", EnvVars.WindowsSDKExtensionDir, EnvVars.WindowsSDKExtensionHeaderLibVersion));
				}
			}

			if (CompileEnvironment.Config.bIsBuildingLibrary == false) // will put in a config option, but for now...
			{
				// Enable Windows Runtime extensions
				Arguments.Append(" /ZW");
				Arguments.Append(" /DUSE_WINRT_MAIN=1");
				// TODO - richiem - this will have to be updated when final layout SDKs are available
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 &&
					Directory.Exists(Path.Combine(EnvVars.WindowsSDKExtensionDir, "UnionMetadata")))
				{
					Arguments.AppendFormat(@" /AI""{0}\UnionMetadata""", EnvVars.WindowsSDKExtensionDir);
					Arguments.AppendFormat(@" /FU""{0}\UnionMetadata\windows.winmd""", EnvVars.WindowsSDKExtensionDir);
				}
				Arguments.AppendFormat(@" /AI""{0}\..\..\VC\vcpackages""", EnvVars.BaseVSToolPath);
				Arguments.AppendFormat(@" /FU""{0}\..\..\VC\vcpackages\platform.winmd""", EnvVars.BaseVSToolPath);
			}

		}

		static void AppendCLArguments_CPP(CPPEnvironment CompileEnvironment, StringBuilder Arguments)
		{
			// Explicitly compile the file as C++.
			Arguments.Append(" /TP");

			if (!CompileEnvironment.Config.bEnableBufferSecurityChecks)
			{
				// This will disable buffer security checks (which are enabled by default) that the MS compiler adds around arrays on the stack,
				// Which can add some performance overhead, especially in performance intensive code
				// Only disable this if you know what you are doing, because it will be disabled for the entire module!
				Arguments.Append(" /GS-");
			}

			// C++/CLI requires that RTTI is left enabled
			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
			{
				if (CompileEnvironment.Config.bUseRTTI)
				{
					// Enable C++ RTTI.
					Arguments.Append(" /GR");
				}
				else
				{
					// Disable C++ RTTI.
					Arguments.Append(" /GR-");
				}
			}

			// Level 4 warnings.
			Arguments.Append(" /W3");
		}

		static void AppendCLArguments_C(StringBuilder Arguments)
		{
			// Explicitly compile the file as C.
			Arguments.Append(" /TC");

			// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
			Arguments.Append(" /W0");
		}

		static void AppendLinkArguments(LinkEnvironment LinkEnvironment, StringBuilder Arguments)
		{
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
			// Set machine type/ architecture to be 64 bit, and set as a store app
			if (UWPPlatform.bBuildForStore)
			{
				Arguments.Append(" /MACHINE:x64");
				Arguments.Append(" /APPCONTAINER");
				// this helps with store API compliance validation tools, adding additional pdb info
				Arguments.Append(" /PROFILE");
			}

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				Arguments.Append(" /SUBSYSTEM:CONSOLE");
			}
			else
			{
				Arguments.Append(" /SUBSYSTEM:WINDOWS");
			}

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication && !LinkEnvironment.Config.bIsBuildingDLL && !String.IsNullOrEmpty(LinkEnvironment.Config.WindowsEntryPointOverride))
			{
				// Use overridden entry point
				Arguments.Append(" /ENTRY:" + LinkEnvironment.Config.WindowsEntryPointOverride);
			}

			// Allow the OS to load the EXE at different base addresses than its preferred base address.
			Arguments.Append(" /FIXED:No");

			// Explicitly declare that the executable is compatible with Data Execution Prevention.
			Arguments.Append(" /NXCOMPAT");

			// Set the default stack size.
			Arguments.Append(" /STACK:5000000");

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
					//		Arguments.Append(" /NOENTRY /NODEFAULTLIB:nochkclr.obj");
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
				//Arguments.Append(" /ORDER:@linkorder.txt");
				//Arguments.Append(" /VERBOSE");
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
			// Set machine type/ architecture to be 64 bit.
			Arguments.Append(" /MACHINE:x64");

			if (LinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				Arguments.Append(" /SUBSYSTEM:CONSOLE");
			}
			else
			{
				Arguments.Append(" /SUBSYSTEM:WINDOWS");
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
			var EnvVars = VCEnvironment.SetEnvironment(CompileEnvironment.Config.Target.Platform, false);

			StringBuilder Arguments = new StringBuilder();
			AppendCLArguments_Global(CompileEnvironment, EnvVars, Arguments);

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
			{
				string IncludePathRelative = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(IncludePath, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source")), '/');
				Arguments.AppendFormat(" /I \"{0}\"", IncludePathRelative);
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
				string IncludePathRelative = Utils.CleanDirectorySeparators(Utils.MakePathRelativeTo(IncludePath, Path.Combine(ProjectFileGenerator.RootRelativePath, "Engine/Source")), '/');
				Arguments.AppendFormat(" /I \"{0}\"", IncludePathRelative);
			}


			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
			{
				// Add .NET framework assembly paths.  This is needed so that C++/CLI projects
				// can reference assemblies with #using, without having to hard code a path in the
				// .cpp file to the assembly's location.				
				foreach (string AssemblyPath in CompileEnvironment.Config.SystemDotNetAssemblyPaths)
				{
					Arguments.AppendFormat(" /AI \"{0}\"", AssemblyPath);
				}

				// Add explicit .NET framework assembly references				
				foreach (string AssemblyName in CompileEnvironment.Config.FrameworkAssemblyDependencies)
				{
					Arguments.AppendFormat(" /FU \"{0}\"", AssemblyName);
				}

				// Add private assembly references				
				foreach (PrivateAssemblyInfo CurAssemblyInfo in CompileEnvironment.PrivateAssemblyDependencies)
				{
					Arguments.AppendFormat(" /FU \"{0}\"", CurAssemblyInfo.FileItem.AbsolutePath);
				}
			}


			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				var DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				Arguments.AppendFormat(" /D\"{0}\"", DefinitionArgument);
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
				AddPrerequisiteSourceFile(Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				// If this is a CLR file then make sure our dependent assemblies are added as prerequisites
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					foreach (PrivateAssemblyInfo CurPrivateAssemblyDependency in CompileEnvironment.PrivateAssemblyDependencies)
					{
						CompileAction.PrerequisiteItems.Add(CurPrivateAssemblyDependency.FileItem);
					}
				}

				bool bEmitsObjectFile = true;
				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Generate a CPP File that just includes the precompiled header.
					string PCHCPPFilename = "PCH." + ModuleName + "." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName() + ".cpp";
					FileReference PCHCPPPath = FileReference.Combine(CompileEnvironment.Config.OutputDirectory, PCHCPPFilename);
					FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
						PCHCPPPath,
						string.Format("#include \"{0}\"\r\n", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename)
						);

					// Make sure the original source directory the PCH header file existed in is added as an include
					// path -- it might be a private PCH header and we need to make sure that its found!
					string OriginalPCHHeaderDirectory = Path.GetDirectoryName(SourceFile.AbsolutePath);
					FileArguments.AppendFormat(" /I \"{0}\"", OriginalPCHHeaderDirectory);

					var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.UWP).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

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

					FileArguments.AppendFormat(" \"{0}\"", PCHCPPFile.AbsolutePath);

					CompileAction.StatusDescription = PCHCPPFilename;
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);

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

					// Add the source file path to the command-line.
					FileArguments.AppendFormat(" \"{0}\"", SourceFile.AbsolutePath);

					CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				}

				if (bEmitsObjectFile)
				{
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.UWP).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					FileArguments.AppendFormat(" /Fo\"{0}\"", ObjectFile.AbsolutePath);
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
						PDBFileName = "PCH." + ModuleName + "." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
					}
					// Files creating a PCH use a PDB per file.
					else if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
						PDBFileName = "PCH." + ModuleName + "." + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename.GetFileName();
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
				CompileAction.CommandPath = EnvVars.CompilerPath;
				CompileAction.CommandArguments = Arguments.ToString() + FileArguments.ToString() + CompileEnvironment.Config.AdditionalArguments;

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					Log.TraceVerbose("Creating PCH: " + CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}
				else
				{
					Log.TraceVerbose("   Compiling: " + CompileAction.StatusDescription);
					Log.TraceVerbose("     Command: " + CompileAction.CommandArguments);
				}

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = false;

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
			var EnvVars = VCEnvironment.SetEnvironment(Environment.Config.Target.Platform, false);

			CPPOutput Result = new CPPOutput();

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(Environment.Config.Target.Platform);

			foreach (FileItem RCFile in RCFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Resource";
				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = EnvVars.ResourceCompilerPath;
				CompileAction.StatusDescription = Path.GetFileName(RCFile.AbsolutePath);

				// Suppress header spew
				CompileAction.CommandArguments += " /nologo";

				// If we're compiling for 64-bit Windows, also add the _WIN64 definition to the resource
				// compiler so that we can switch on that in the .rc file using #ifdef.
				CompileAction.CommandArguments += " /D_WIN64";

				// Language
				CompileAction.CommandArguments += " /l 0x409";

				// Include paths.
				foreach (string IncludePath in Environment.Config.CPPIncludeInfo.IncludePaths)
				{
					CompileAction.CommandArguments += string.Format(" /i \"{0}\"", IncludePath);
				}

				// System include paths.
				foreach (var SystemIncludePath in Environment.Config.CPPIncludeInfo.SystemIncludePaths)
				{
					CompileAction.CommandArguments += string.Format(" /i \"{0}\"", SystemIncludePath);
				}

				// Preprocessor definitions.
				foreach (string Definition in Environment.Config.Definitions)
				{
					CompileAction.CommandArguments += string.Format(" /d \"{0}\"", Definition);
				}

				// Add the RES file to the produced item list.
				FileItem CompiledResourceFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						Environment.Config.OutputDirectory,
						Path.GetFileName(RCFile.AbsolutePath) + ".res"
						)
					);
				CompileAction.ProducedItems.Add(CompiledResourceFile);
				CompileAction.CommandArguments += string.Format(" /fo \"{0}\"", CompiledResourceFile.AbsolutePath);
				Result.ObjectFiles.Add(CompiledResourceFile);

				// Add the RC file as a prerequisite of the action.
				CompileAction.CommandArguments += string.Format(" \"{0}\"", RCFile.AbsolutePath);

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(Target, BuildPlatform, Environment, RCFile, CompileAction.PrerequisiteItems);
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			var EnvVars = VCEnvironment.SetEnvironment(LinkEnvironment.Config.Target.Platform, false);

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
				// Xbox 360 LTCG does not seem to produce those.
				if (LinkEnvironment.Config.bHasExports &&
					(LinkEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Shipping))
				{
					// Write the import library to the output directory for nFringe support.
					FileItem ImportLibraryFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
					Arguments.AppendFormat(" /IMPLIB:\"{0}\"", ImportLibraryFilePath);
					ProducedItems.Add(ImportLibraryFile);
				}

				if (LinkEnvironment.Config.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					FileReference PDBFilePath = FileReference.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
					FileItem PDBFile = FileItem.GetItemByFileReference(PDBFilePath);
					Arguments.AppendFormat(" /PDB:\"{0}\"", PDBFilePath);
					ProducedItems.Add(PDBFile);

					// Write the MAP file to the output directory.			
#if false					
					if (true)
					{
						string MAPFilePath = Path.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByFileReference(MAPFilePath);
						LinkAction.CommandArguments += string.Format(" /MAP:\"{0}\"", MAPFilePath);
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
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = bIsBuildingLibrary ? EnvVars.LibraryLinkerPath : EnvVars.LinkerPath;
			LinkAction.CommandArguments = Arguments.ToString();
			LinkAction.ProducedItems.AddRange(ProducedItems);
			LinkAction.PrerequisiteItems.AddRange(PrerequisiteItems);
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			Log.TraceVerbose("     Linking: " + LinkAction.StatusDescription);
			Log.TraceVerbose("     Command: " + LinkAction.CommandArguments);

			return OutputFile;
		}


		/// <summary>
		/// Gets the default include paths for the given platform.
		/// </summary>
		public static string GetVCIncludePaths(CPPTargetPlatform Platform)
		{
			Debug.Assert(Platform == CPPTargetPlatform.UWP);

			// Make sure we've got the environment variables set up for this target
			VCEnvironment.SetEnvironment(Platform, false);

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
	};
}
