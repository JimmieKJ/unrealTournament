// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration class for LinkEnvironment
	/// </summary>
	public class LinkEnvironmentConfiguration : NativeBuildEnvironmentConfiguration
	{
		/// <summary>
		/// The directory to put the non-executable files in (PDBs, import library, etc)
		/// </summary>
		public DirectoryReference OutputDirectory;

		/// <summary>
		/// Intermediate file directory
		/// </summary>
		public DirectoryReference IntermediateDirectory;

		/// <summary>
		/// The directory to shadow source files in for syncing to remote compile servers
		/// </summary>
		public DirectoryReference LocalShadowDirectory = null;

		/// <summary>
		/// The file path for the executable file that is output by the linker.
		/// </summary>
		public List<FileReference> OutputFilePaths = new List<FileReference>();

		/// <summary>
		/// Returns the OutputFilePath is there is only one entry in OutputFilePaths
		/// </summary>
		public FileReference OutputFilePath
		{
			get
			{
				if (OutputFilePaths.Count != 1)
				{
					throw new BuildException("Attempted to use LinkEnvironmentConfiguration.OutputFilePath property, but there are multiple (or no) OutputFilePaths. You need to handle multiple in the code that called this (size = {0})", OutputFilePaths.Count);
				}
				return OutputFilePaths[0];
			}
		}

		/// <summary>
		/// The project file for this target
		/// </summary>
		public FileReference ProjectFile = null;

		/// <summary>
		/// A list of the paths used to find libraries.
		/// </summary>
		public List<string> LibraryPaths = new List<string>();

		/// <summary>
		/// A list of libraries to exclude from linking.
		/// </summary>
		public List<string> ExcludedLibraries = new List<string>();

		/// <summary>
		/// A list of additional libraries to link in.
		/// </summary>
		public List<string> AdditionalLibraries = new List<string>();

		/// <summary>
		/// A list of additional frameworks to link in.
		/// </summary>
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// For builds that execute on a remote machine (e.g. iPhone), this list contains additional files that
		/// need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
		/// automatically copied.  Usually this is simply a list of precompiled third party library dependencies.
		/// </summary>
		public List<string> AdditionalShadowFiles = new List<string>();

		/// <summary>
		/// The iOS/Mac frameworks to link in
		/// </summary>
		public List<string> Frameworks = new List<string>();
		public List<string> WeakFrameworks = new List<string>();

		/// <summary>
		/// iOS/Mac resources that should be copied to the app bundle
		/// </summary>
		public List<UEBuildBundleResource> AdditionalBundleResources = new List<UEBuildBundleResource>();

		/// <summary>
		/// A list of the dynamically linked libraries that shouldn't be loaded until they are first called
		/// into.
		/// </summary>
		public List<string> DelayLoadDLLs = new List<string>();

		/// <summary>
		/// Additional arguments to pass to the linker.
		/// </summary>
		public string AdditionalArguments = "";

		/// <summary>
		/// True if debug info should be created.
		/// </summary>
		public bool bCreateDebugInfo = true;

		/// <summary>
		/// True if debug symbols that are cached for some platforms should not be created.
		/// </summary>
		public bool bDisableSymbolCache = true;

		/// <summary>
		/// Whether the CLR (Common Language Runtime) support should be enabled for C++ targets (C++/CLI).
		/// </summary>
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/// <summary>
		/// True if we're compiling .cpp files that will go into a library (.lib file)
		/// </summary>
		public bool bIsBuildingLibrary = false;

		/// <summary>
		/// True if we're compiling a DLL
		/// </summary>
		public bool bIsBuildingDLL = false;

		/// <summary>
		/// True if this is a console application that's being build
		/// </summary>
		public bool bIsBuildingConsoleApplication = false;

		/// <summary>
		/// This setting is replaced by UEBuildBinaryConfiguration.bBuildAdditionalConsoleApp.
		/// </summary>
		[Obsolete("This setting is replaced by UEBuildBinaryConfiguration.bBuildAdditionalConsoleApp. It is explicitly set to true for editor targets, and defaults to false otherwise.")]
		public bool bBuildAdditionalConsoleApplication { set { } }

		/// <summary>
		/// If set, overrides the program entry function on Windows platform.  This is used by the base UE4
		/// program so we can link in either command-line mode or windowed mode without having to recompile the Launch module.
		/// </summary>
		public string WindowsEntryPointOverride = String.Empty;

		/// <summary>
		/// True if we're building a EXE/DLL target with an import library, and that library is needed by a dependency that
		/// we're directly dependent on.
		/// </summary>
		public bool bIsCrossReferenced = false;

		/// <summary>
		/// True if we should include dependent libraries when building a static library
		/// </summary>
		public bool bIncludeDependentLibrariesInLibrary = false;

		/// <summary>
		/// True if the application we're linking has any exports, and we should be expecting the linker to
		/// generate a .lib and/or .exp file along with the target output file
		/// </summary>
		public bool bHasExports = true;

		/// <summary>
		/// True if we're building a .NET assembly (e.g. C# project)
		/// </summary>
		public bool bIsBuildingDotNetAssembly = false;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public LinkEnvironmentConfiguration()
		{
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		public LinkEnvironmentConfiguration(LinkEnvironmentConfiguration InCopyEnvironment) :
			base(InCopyEnvironment)
		{
			OutputDirectory = InCopyEnvironment.OutputDirectory;
			IntermediateDirectory = InCopyEnvironment.IntermediateDirectory;
			LocalShadowDirectory = InCopyEnvironment.LocalShadowDirectory;
			OutputFilePaths = InCopyEnvironment.OutputFilePaths.ToList();
			ProjectFile = InCopyEnvironment.ProjectFile;
			LibraryPaths.AddRange(InCopyEnvironment.LibraryPaths);
			ExcludedLibraries.AddRange(InCopyEnvironment.ExcludedLibraries);
			AdditionalLibraries.AddRange(InCopyEnvironment.AdditionalLibraries);
			Frameworks.AddRange(InCopyEnvironment.Frameworks);
			AdditionalShadowFiles.AddRange(InCopyEnvironment.AdditionalShadowFiles);
			AdditionalFrameworks.AddRange(InCopyEnvironment.AdditionalFrameworks);
			WeakFrameworks.AddRange(InCopyEnvironment.WeakFrameworks);
			AdditionalBundleResources.AddRange(InCopyEnvironment.AdditionalBundleResources);
			DelayLoadDLLs.AddRange(InCopyEnvironment.DelayLoadDLLs);
			AdditionalArguments = InCopyEnvironment.AdditionalArguments;
			bCreateDebugInfo = InCopyEnvironment.bCreateDebugInfo;
			CLRMode = InCopyEnvironment.CLRMode;
			bIsBuildingLibrary = InCopyEnvironment.bIsBuildingLibrary;
			bIsBuildingDLL = InCopyEnvironment.bIsBuildingDLL;
			bIsBuildingConsoleApplication = InCopyEnvironment.bIsBuildingConsoleApplication;
			WindowsEntryPointOverride = InCopyEnvironment.WindowsEntryPointOverride;
			bIsCrossReferenced = InCopyEnvironment.bIsCrossReferenced;
			bHasExports = InCopyEnvironment.bHasExports;
			bIsBuildingDotNetAssembly = InCopyEnvironment.bIsBuildingDotNetAssembly;
		}
	}

	/// <summary>
	/// Encapsulates the environment that is used to link object files.
	/// </summary>
	public class LinkEnvironment
	{
		/// <summary>
		/// Whether we're linking in monolithic mode. Determines if linking should produce import library file. Relevant only for VC++, clang stores imports in shared library.
		/// </summary>
		public bool bShouldCompileMonolithic = false;

		/// <summary>
		/// A list of the object files to be linked.
		/// </summary>
		public List<FileItem> InputFiles = new List<FileItem>();

		/// <summary>
		/// A list of dependent static or import libraries that need to be linked.
		/// </summary>
		public List<FileItem> InputLibraries = new List<FileItem>();

		/// <summary>
		/// The LinkEnvironmentConfiguration.
		/// </summary>
		public LinkEnvironmentConfiguration Config = new LinkEnvironmentConfiguration();

		/// <summary>
		/// Default constructor.
		/// </summary>
		public LinkEnvironment()
		{
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		protected LinkEnvironment(LinkEnvironment InCopyEnvironment)
		{
			InputFiles.AddRange(InCopyEnvironment.InputFiles);
			InputLibraries.AddRange(InCopyEnvironment.InputLibraries);

			Config = new LinkEnvironmentConfiguration(InCopyEnvironment.Config);
		}

		/// <summary>
		/// Performs a deep copy of this LinkEnvironment object.
		/// </summary>
		/// <returns>Copied new LinkEnvironment object.</returns>
		public virtual LinkEnvironment DeepCopy()
		{
			return new LinkEnvironment(this);
		}
	}
}
