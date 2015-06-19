// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	class WinRTToolChain : UEToolChain
	{
		public override void RegisterToolChain()
		{
			// Register this tool chain for WinRT
			Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.WinRT.ToString());
			Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.WinRT_ARM.ToString());
			UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.WinRT, this);
			UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.WinRT_ARM, this);
		}

		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			//Result += " /showIncludes";

			// Prevents the compiler from displaying its logo for each invocation.
			Result += " /nologo";

			// Enable intrinsic functions.
			Result += " /Oi";

			// Enable for static code analysis (where supported). Not treating analysis warnings as errors.
			// Result += " /analyze:WX-";

			// Pack struct members on 8-byte boundaries.
			Result += " /Zp8";

			// Calling convention - _cdecl
			Result += " /Gd";
			// Disable minimal rebuild
			Result += " /Gm-";
			// Security checks enabled
			Result += " /GS";

			// Separate functions for linker.
			Result += " /Gy";

			// Relaxes floating point precision semantics to allow more optimization.
			Result += " /fp:fast";

			// Compile into an .obj file, and skip linking.
			Result += " /c";

			// Allow 400% of the default memory allocation limit.
			Result += " /Zm400";

			// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
			Result += " /bigobj";

			Result += " /Zc:wchar_t";
			Result += " /Zc:forScope";

			// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
			Result += " /wd4819";

			if( BuildConfiguration.bUseSharedPCHs )
			{
				// @todo SharedPCH: Disable warning about PCH defines not matching .cpp defines.  We "cheat" these defines a little
				// bit to make shared PCHs work.  But it's totally safe.  Trust us.
				Result += " /wd4651";

				// @todo SharedPCH: Disable warning about redefining *API macros.  The PCH header is compiled with various DLLIMPORTs, but
				// when a module that uses that PCH header *IS* one of those imports, that module is compiled with EXPORTS, so the macro
				// is redefined on the command-line.  We need to clobber those defines to make shared PCHs work properly!
				Result += " /wd4005";
			}

			// If compiling as a DLL, set the relevant defines
			if (CompileEnvironment.Config.bIsBuildingDLL)
			{
				Result += " /D _WINDLL";
				Result += " /D _USRDLLundefined_EXPORTS";
			}

			//
			//	Debug
			//
			if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
			{
				// Disable compiler optimization.
				Result += " /Od";

				// Favor code size (especially useful for embedded platforms).
				Result += " /Os";

				// Allow inline method expansion unless E&C support is requested
				if (!BuildConfiguration.bSupportEditAndContinue)
				{
					// Allow inline method expansion of any suitable.
					Result += " /Ob2";
				}

				// Perform runtime checks for (s) stack frames and (u) unintialized variables
				// RTC1 == RTCsu
				Result += " /RTC1";
			}
			//
			//	Release and LTCG
			//
			else
			{
				// Maximum optimizations.
				Result += " /Ox";
				// Enable intrinsics
				Result += " /Oi";
				// Allow inline method expansion
				Result += " /Ob2";

				//
				// LTCG
				//
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
				{
					if (BuildConfiguration.bAllowLTCG)
					{
						// Enable link-time code generation.
						Result += " /GL";
					}
				}
			}

			//
			//	WinRT
			//
			if (CompileEnvironment.Config.bIsBuildingLibrary == false)
			{
				Result += " /ZW";
			}

			if (WinRTPlatform.ShouldCompileWinRT() == true)
			{
				// Prompt the user before reporting internal errors to Microsoft.
				Result += " /errorReport:prompt";

				// Enable C++ exception handling, but not C exceptions.
				Result += " /EHsc";

				// Disable "unreferenced formal parameter" warning since auto-included vccorlib.h triggers it
				Result += " /wd4100";

				// Disable "unreachable code" warning since auto-included vccorlib.h triggers it
				Result += " /wd4702";
			}
			else
			{
				Result += " /AI\"C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\VC\\vcpackages\"";

				Result += " /D USE_WINRT_MAIN=1";

				// WinRT requires exceptions!
//				if (CompileEnvironment.Config.bEnableExceptions)
				{
					// Enable C++ exception handling, but not C exceptions.
					Result += " /EHsc";
				}

				// Disable "unreferenced formal parameter" warning since auto-included vccorlib.h triggers it
				Result += " /wd4100";

				// Disable "unreachable code" warning since auto-included vccorlib.h triggers it
				Result += " /wd4702";

				// Disable "alignment of a member was sensitive to packing" warning since auto-included vccorlib.h triggers it
				Result += " /wd4121";

				// reinterpret_cast used between related classes: 'Platform::Object' and ...
				Result += " /wd4946";

				// macro redefinition
				Result += " /wd4005";

				//Result += " /showIncludes";
			}

			// If enabled, create debug information.
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				// Store debug info in .pdb files.
				if (BuildConfiguration.bUsePDBFiles)
				{
					// Create debug info suitable for E&C if wanted.
					if (BuildConfiguration.bSupportEditAndContinue
						// We only need to do this in debug as that's the only configuration that supports E&C.
						&& CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
					{
						Result += " /ZI";
					}
					// Regular PDB debug information.
					else
					{
						Result += " /Zi";
					}
				}
				// Store C7-format debug info in the .obj files, which is faster.
				else
				{
					Result += " /Z7";
				}
			}

			// Specify the appropriate runtime library based on the platform and config.
			if( CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
			{
				Result += " /MDd";
			}
			else
			{
				Result += " /MD";
			}

			return Result;
		}

		static string GetCLArguments_CPP(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			// Explicitly compile the file as C++.
			Result += " /TP";

			// C++/CLI requires that RTTI is left enabled
			if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLRDisabled)
			{
				if (CompileEnvironment.Config.bUseRTTI)
				{
					// Enable C++ RTTI.
					Result += " /GR";
				}
				else
				{
					// Disable C++ RTTI.
					Result += " /GR-";
				}
			}

			if (WinRTPlatform.ShouldCompileWinRT() == true)
			{
				// WinRT headers generate too many warnings for /W4
				Result += " /W1";
			}
			else
			{
				// Level 3 warnings.
//				Result += " /W3";
				Result += " /W1";
			}

			return Result;
		}

		static string GetCLArguments_C()
		{
			string Result = "";

			// Explicitly compile the file as C.
			Result += " /TC";

			// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
			Result += " /W0";

			return Result;
		}

		static string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// Prevents the linker from displaying its logo for each invocation.
			Result += " /NOLOGO";
			Result += " /TLBID:1";

			// Don't create a side-by-side manifest file for the executable.
			Result += " /MANIFEST:NO";

			if (LinkEnvironment.Config.bCreateDebugInfo)
			{
				// Output debug info for the linked executable.
				Result += " /DEBUG";
			}

			// Prompt the user before reporting internal errors to Microsoft.
			Result += " /errorReport:prompt";

			// Set machine type/ architecture to be 64 bit.
			Result += " /MACHINE:x64";

			Result += " /DYNAMICBASE \"d2d1.lib\" \"d3d11.lib\" \"dxgi.lib\" \"ole32.lib\" \"windowscodecs.lib\" \"dwrite.lib\" \"kernel32.lib\"";

			// WinRT
//			if (WinRTPlatform.ShouldCompileWinRT() == true)
			{
				// generate metadata
//				Result += " /WINMD:ONLY";

				Result += " /WINMD";
				Result += " /APPCONTAINER";

				// location of metadata
				Result += string.Format(" /WINMDFILE:\"{0}\"", Path.ChangeExtension(LinkEnvironment.Config.OutputFilePath, "winmd"));
			}

			if (LinkEnvironment.Config.Target.Platform == CPPTargetPlatform.WinRT_ARM)
			{
				// Link for console.
				Result += " /SUBSYSTEM:CONSOLE";
			}
			else
			{
				// Link for Windows.
				Result += " /SUBSYSTEM:WINDOWS";
			}

			// Allow the OS to load the EXE at different base addresses than its preferred base address.
			Result += " /FIXED:No";

			// Explicitly declare that the executable is compatible with Data Execution Prevention.
			Result += " /NXCOMPAT";

			// Set the default stack size.
			Result += " /STACK:5000000,5000000";

			// Allow delay-loaded DLLs to be explicitly unloaded.
			Result += " /DELAY:UNLOAD";

			if (LinkEnvironment.Config.bIsBuildingDLL == true)
			{
				Result += " /DLL";
			}

			//
			//	ReleaseLTCG
			//
			if (BuildConfiguration.bAllowLTCG &&
				LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Result += " /LTCG";

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
				Result += " /RELEASE";

				// Eliminate unreferenced symbols.
				Result += " /OPT:REF";

				// Remove redundant COMDATs.
				Result += " /OPT:ICF";
			}
			//
			//	Regular development binary. 
			//
			else
			{
				// Keep symbols that are unreferenced.
				Result += " /OPT:NOREF";

				// Disable identical COMDAT folding.
				Result += " /OPT:NOICF";
			}

			// Enable incremental linking if wanted.
			if (BuildConfiguration.bUseIncrementalLinking)
			{
				Result += " /INCREMENTAL";
			}
			// Disabled by default as it can cause issues and forces local execution.
			else
			{
				Result += " /INCREMENTAL:NO";
			}

			return Result;
		}

		static string GetLibArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// Prevents the linker from displaying its logo for each invocation.
			Result += " /NOLOGO";

			// Prompt the user before reporting internal errors to Microsoft.
			Result += " /errorReport:prompt";

			// Set machine type/ architecture to be 64 bit.
			Result += " /MACHINE:x64";
			Result += " /SUBSYSTEM:WINDOWS";

			//
			//	Shipping & LTCG
			//
			if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
			{
				// Use link-time code generation.
				Result += " /ltcg";
			}

			return Result;
		}

		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.IncludePaths)
			{
				Arguments += string.Format(" /I \"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths)
			{
				Arguments += string.Format(" /I \"{0}\"", IncludePath);
			}

			if ((CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled) ||
				(WinRTPlatform.ShouldCompileWinRT() == true))
			{
				// Add .NET framework assembly paths.  This is needed so that C++/CLI projects
				// can reference assemblies with #using, without having to hard code a path in the
				// .cpp file to the assembly's location.				
				foreach (string AssemblyPath in CompileEnvironment.Config.SystemDotNetAssemblyPaths)
				{
					Arguments += string.Format(" /AI \"{0}\"", AssemblyPath);
				}

				// Add explicit .NET framework assembly references				
				foreach (string AssemblyName in CompileEnvironment.Config.FrameworkAssemblyDependencies)
				{
					Arguments += string.Format(" /FU \"{0}\"", AssemblyName);
				}

				// Add private assembly references				
				foreach (PrivateAssemblyInfo CurAssemblyInfo in CompileEnvironment.PrivateAssemblyDependencies)
				{
					Arguments += string.Format(" /FU \"{0}\"", CurAssemblyInfo.FileItem.AbsolutePath);
				}
			}
			else
			{
				foreach (string AssemblyPath in CompileEnvironment.Config.SystemDotNetAssemblyPaths)
				{
					Arguments += string.Format(" /AI \"{0}\"", AssemblyPath);
				}

				// Add explicit .NET framework assembly references				
				foreach (string AssemblyName in CompileEnvironment.Config.FrameworkAssemblyDependencies)
				{
					Arguments += string.Format(" /FU \"{0}\"", AssemblyName);
				}
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				Arguments += string.Format(" /D \"{0}\"", Definition);
			}

// Log.TraceInformation("Compile Arguments for {0}:", ModuleName);
// Log.TraceInformation(Arguments);

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);
			
			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				string FileArguments = "";
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile( Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems );

				// If this is a CLR file then make sure our dependent assemblies are added as prerequisites
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					foreach (PrivateAssemblyInfo CurPrivateAssemblyDependency in CompileEnvironment.PrivateAssemblyDependencies)
					{
						CompileAction.PrerequisiteItems.Add(CurPrivateAssemblyDependency.FileItem);
					}
				}

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
					string OriginalPCHHeaderDirectory = Path.GetDirectoryName(SourceFile.AbsolutePath);
					FileArguments += string.Format(" /I \"{0}\"", OriginalPCHHeaderDirectory);

					var PCHExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.WinRT].GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);

					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PCHExtension
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" /Yc\"{0}\"", CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					FileArguments += string.Format(" /Fp\"{0}\"", PrecompiledHeaderFile.AbsolutePath);
					FileArguments += string.Format(" \"{0}\"", PCHCPPFile.AbsolutePath);

					// If we're creating a PCH that will be used to compile source files for a library, we need
					// the compiled modules to retain a reference to PCH's module, so that debugging information
					// will be included in the library.  This is also required to avoid linker warning "LNK4206"
					// when linking an application that uses this library.
					if (CompileEnvironment.Config.bIsBuildingLibrary)
					{
						// NOTE: The symbol name we use here is arbitrary, and all that matters is that it is
						// unique per PCH module used in our library
						string FakeUniquePCHSymbolName = Path.GetFileNameWithoutExtension(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
						FileArguments += string.Format(" /Yl{0}", FakeUniquePCHSymbolName);
					}
					CompileAction.StatusDescription = PCHCPPFilename;
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
						FileArguments += string.Format(" /Yu\"{0}\"", CompileEnvironment.Config.PCHHeaderNameInCode);
						FileArguments += string.Format(" /Fp\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath);

						// Is it unsafe to always force inclusion?  Clang is doing it, and .generated.cpp files
						// won't work otherwise, because they're not located in the context of the module,
						// so they can't access the module's PCH without an absolute path.
						//if (CompileEnvironment.Config.bForceIncludePrecompiledHeader)
						{
							// Force include the precompiled header file.  This is needed because we may have selected a
							// precompiled header that is different than the first direct include in the C++ source file, but
							// we still need to make sure that our precompiled header is the first thing included!
							FileArguments += string.Format(" /FI\"{0}\"", CompileEnvironment.Config.PCHHeaderNameInCode);
						}
					}

					// Add the source file path to the command-line.
					FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

					CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				}

				var ObjectFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.WinRT].GetBinaryExtension(UEBuildBinaryType.Object);

				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByPath(
					Path.Combine(
						CompileEnvironment.Config.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				Result.ObjectFiles.Add(ObjectFile);
				FileArguments += string.Format(" /Fo\"{0}\"", ObjectFile.AbsolutePath);

				// create PDBs per-file when not using debug info, otherwise it will try to share a PDB file, which causes
				// PCH creation to be serial rather than parallel (when debug info is disabled)
				// See https://udn.epicgames.com/lists/showpost.php?id=50619&list=unprog3
				if (!CompileEnvironment.Config.bCreateDebugInfo || BuildConfiguration.bUsePDBFiles)
				{
					string PDBFileName;
					bool bActionProducesPDB = false;

					// All files using the same PCH are required to share a PDB.
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						PDBFileName = Path.GetFileName(CompileEnvironment.Config.PrecompiledHeaderIncludeFilename);
					}
					// Files creating a PCH or ungrouped C++ files use a PDB per file.
					else if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create || !bIsPlainCFile)
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
					FileItem PDBFile = FileItem.GetItemByPath(
							Path.Combine(
								CompileEnvironment.Config.OutputDirectory,
								PDBFileName + ".pdb"
								)
							);
					FileArguments += string.Format(" /Fd\"{0}\"", PDBFile.AbsolutePath);

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
					FileArguments += GetCLArguments_C();
				}
				else
				{
					FileArguments += GetCLArguments_CPP(CompileEnvironment);
				}

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = GetVCToolPath(CompileEnvironment.Config.Target.Platform, CompileEnvironment.Config.Target.Configuration, "cl");
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));

				// Don't farm out creation of precomputed headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;

				// @todo: XGE has problems remote compiling C++/CLI files that use .NET Framework 4.0
				if (CompileEnvironment.Config.CLRMode == CPPCLRMode.CLREnabled)
				{
					CompileAction.bCanExecuteRemotely = false;
				}
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			if (LinkEnvironment.Config.bIsBuildingDotNetAssembly)
			{
				return FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePath);
			}

			bool bIsBuildingLibrary = LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly;
			bool bIncludeDependentLibrariesInLibrary = bIsBuildingLibrary && LinkEnvironment.Config.bIncludeDependentLibrariesInLibrary;

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = GetVCToolPath(
				LinkEnvironment.Config.Target.Platform,
				LinkEnvironment.Config.Target.Configuration,
				bIsBuildingLibrary ? "lib" : "link");

			// Get link arguments.
			LinkAction.CommandArguments = bIsBuildingLibrary ?
				GetLibArguments(LinkEnvironment) :
				GetLinkArguments(LinkEnvironment);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL;

			// If we're only building an import library, add the '/DEF' option that tells the LIB utility
			// to simply create a .LIB file and .EXP file, and don't bother validating imports
			if (bBuildImportLibraryOnly)
			{
				LinkAction.CommandArguments += " /DEF";

				// Ensure that the import library references the correct filename for the linked binary.
				LinkAction.CommandArguments += string.Format(" /NAME:\"{0}\"", Path.GetFileName(LinkEnvironment.Config.OutputFilePath));
			}

			if (!LinkEnvironment.Config.bIsBuildingLibrary || (LinkEnvironment.Config.bIsBuildingLibrary && bIncludeDependentLibrariesInLibrary))
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
				{
					LinkAction.CommandArguments += string.Format(" /LIBPATH:\"{0}\"", LibraryPath);
				}

				// Add the excluded default libraries to the argument list.
				foreach (string ExcludedLibrary in LinkEnvironment.Config.ExcludedLibraries)
				{
					LinkAction.CommandArguments += string.Format(" /NODEFAULTLIB:\"{0}\"", ExcludedLibrary);
				}
			}

			// For targets that are cross-referenced, we don't want to write a LIB file during the link step as that
			// file will clobber the import library we went out of our way to generate during an earlier step.  This
			// file is not needed for our builds, but there is no way to prevent MSVC from generating it when
			// linking targets that have exports.  We don't want this to clobber our LIB file and invalidate the
			// existing timstamp, so instead we simply emit it with a different name
			string ImportLibraryFilePath = Path.Combine(LinkEnvironment.Config.IntermediateDirectory,
														 Path.GetFileNameWithoutExtension(LinkEnvironment.Config.OutputFilePath) + ".lib");

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
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (!bBuildImportLibraryOnly)
			{
				// Add input libraries as prerequisites, too!
				foreach (FileItem InputLibrary in LinkEnvironment.InputLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputLibrary.AbsolutePath));
					LinkAction.PrerequisiteItems.Add(InputLibrary);
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
						LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
					}
				}
			}

			// Create a response file for the linker
			string ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);

			// Never create response files when we are only generating IntelliSense data
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				ResponseFile.Create(ResponseFileName, InputFileNames);
			}
			LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFileName);

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" /OUT:\"{0}\"", OutputFile.AbsolutePath);

			if (bBuildImportLibraryOnly || (LinkEnvironment.Config.bHasExports && !bIsBuildingLibrary))
			{
				// An export file is written to the output directory implicitly; add it to the produced items list.
				string ExportFilePath = Path.ChangeExtension(ImportLibraryFilePath, ".exp");
				FileItem ExportFile = FileItem.GetItemByPath(ExportFilePath);
				LinkAction.ProducedItems.Add(ExportFile);
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
					LinkAction.CommandArguments += string.Format(" /IMPLIB:\"{0}\"", ImportLibraryFilePath);
					LinkAction.ProducedItems.Add(ImportLibraryFile);
				}

				if (LinkEnvironment.Config.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					{
						string PDBFilePath = Path.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
						FileItem PDBFile = FileItem.GetItemByPath(PDBFilePath);
						LinkAction.CommandArguments += string.Format(" /PDB:\"{0}\"", PDBFilePath);
						LinkAction.ProducedItems.Add(PDBFile);
					}

					// Write the MAP file to the output directory.			
#if false					
					if (true)
					{
						string MAPFilePath = Path.Combine(LinkEnvironment.Config.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByPath(MAPFilePath);
						LinkAction.CommandArguments += string.Format(" /MAP:\"{0}\"", MAPFilePath);
						LinkAction.ProducedItems.Add(MAPFile);
					}
#endif
				}

				// Add the additional arguments specified by the environment.
				LinkAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;
			}

			Log.TraceVerbose("     Linking: " + LinkAction.StatusDescription);
			Log.TraceVerbose("     Command: " + LinkAction.CommandArguments);

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		/** Accesses the bin directory for the VC toolchain for the specified platform. */
		static string GetVCToolPath(CPPTargetPlatform Platform, CPPTargetConfiguration Configuration, string ToolName)
		{
			// Initialize environment variables required for spawned tools.
			InitializeEnvironmentVariables(Platform);

			// Out variable that is going to contain fully qualified path to executable upon return.
			string VCToolPath = "";

			// is target 64-bit?
			bool bIsTarget64Bit = true;

			// We need to differentiate between 32 and 64 bit toolchain on Windows.
			{
				// rc.exe resides in the Windows SDK folder.
				if (ToolName.ToUpperInvariant() == "RC")
				{
					// 64 bit -- we can use the 32 bit version to target 64 bit on 32 bit OS.
					if (bIsTarget64Bit)
					{
						VCToolPath = Path.Combine(WindowsSDKDir, "bin/x64/rc.exe");
					}
					// 32 bit
					else
					{
						VCToolPath = Path.Combine(WindowsSDKDir, "bin/x86/rc.exe");
					}
				}
				// cl.exe and link.exe are found in the toolchain specific folders (32 vs. 64 bit). We do however use the 64 bit linker if available
				// even when targeting 32 bit as it's noticeably faster.
				else
				{
					// Grab path to Visual Studio binaries from the system environment
					string BaseVSToolPath = WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2012);
					if (string.IsNullOrEmpty(BaseVSToolPath))
					{
						throw new BuildException("Visual Studio 2012 must be installed in order to build this target.");
					}

					if (Platform == CPPTargetPlatform.WinRT_ARM)
					{
						VCToolPath = Path.Combine(BaseVSToolPath, "../../VC/bin/x86_arm/" + ToolName + ".exe");
					}
					else
					{
						VCToolPath = Path.Combine(BaseVSToolPath, "../../VC/bin/amd64/" + ToolName + ".exe");
					}
				}
			}

			return VCToolPath;
		}

		/** Accesses the directory for .NET Framework binaries such as MSBuild */
		static string GetDotNetFrameworkToolPath(CPPTargetPlatform Platform, string ToolName)
		{
			// Initialize environment variables required for spawned tools.
			InitializeEnvironmentVariables(Platform);

			string FrameworkDirectory = Environment.GetEnvironmentVariable("FrameworkDir");
			string FrameworkVersion = Environment.GetEnvironmentVariable("FrameworkVersion");
			if (FrameworkDirectory == null || FrameworkVersion == null)
			{
				throw new BuildException("NOTE: Please ensure that 64bit Tools are installed with DevStudio - there is usually an option to install these during install");
			}
			string DotNetFrameworkBinDir = Path.Combine(FrameworkDirectory, FrameworkVersion);
			string ToolPath = Path.Combine(DotNetFrameworkBinDir, ToolName + ".exe");
			return ToolPath;
		}

		/** Helper to only initialize environment variables once. */
		static bool bAreEnvironmentVariablesAlreadyInitialized = false;

		/** Installation folder of the Windows SDK, e.g. C:\Program Files\Microsoft SDKs\Windows\v6.0A\ */
		static string WindowsSDKDir = "";

		/**
		 * Initializes environment variables required by toolchain. Different for 32 and 64 bit.
		 */
		static void InitializeEnvironmentVariables(CPPTargetPlatform Platform)
		{
			if (!bAreEnvironmentVariablesAlreadyInitialized)
			{
				string VCVarsBatchFile = "";

				// Grab path to Visual Studio binaries from the system environment
				string BaseVSToolPath = WindowsPlatform.GetVSComnToolsPath(WindowsCompiler.VisualStudio2012);
				if (string.IsNullOrEmpty(BaseVSToolPath))
				{
					BaseVSToolPath = "C:/Program Files (x86)/Microsoft Visual Studio 11.0/Common7/Tools/";
					if (Directory.Exists("C:/Program Files (x86)/Microsoft Visual Studio 11.0/Common7/Tools/") == false)
					{
						throw new BuildException("Visual Studio 2012 must be installed in order to build this target.");
					}
				}

				// 64 bit tool chain.
				if (Platform == CPPTargetPlatform.WinRT_ARM)
				{
					VCVarsBatchFile = Path.Combine(BaseVSToolPath, "../../VC/bin/x86_arm/vcvarsx86_arm.bat");
				}
				else
				{
					VCVarsBatchFile = Path.Combine(BaseVSToolPath, "../../VC/bin/x86_amd64/vcvarsx86_amd64.bat");
				}

				// @todo: This is failing when building the Win32 target through NMake!!!!
				Utils.SetEnvironmentVariablesFromBatchFile(VCVarsBatchFile);

				if (WinRTPlatform.ShouldCompileWinRT() == true)
				{
					string EnvVarName = Utils.ResolveEnvironmentVariable(WindowsSDKDir + "Windows Metadata" + ";%LIBPATH%");
					Environment.SetEnvironmentVariable("LIBPATH", EnvVarName);
				}

				bAreEnvironmentVariablesAlreadyInitialized = true;
			}
		}

		public override UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.WinRT;
		}
	};
}
