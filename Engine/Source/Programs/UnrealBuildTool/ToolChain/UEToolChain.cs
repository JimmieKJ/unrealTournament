// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	// Included for compatibility during //UE4/Main import
	public interface IUEToolChain
	{
		[Obsolete]
		void StripSymbols(string SourceFile, string TargetFile);
	}

	public abstract class UEToolChain : IUEToolChain
	{
		public readonly CPPTargetPlatform CppPlatform;

		public UEToolChain(CPPTargetPlatform InCppPlatform)
		{
			CppPlatform = InCppPlatform;
		}

		// Included for compatibility during //UE4/Main import
		[Obsolete]
		public static IUEToolChain GetPlatformToolChain(CPPTargetPlatform Platform)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(UEBuildTarget.CPPTargetPlatformToUnrealTargetPlatform(Platform));
			UEBuildPlatformContext Context = BuildPlatform.CreateContext(null);
			return Context.CreateToolChain(BuildPlatform.DefaultCppPlatform);
		}

		public abstract CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName);

		public virtual CPPOutput CompileRCFiles(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles)
		{
			CPPOutput Result = new CPPOutput();
			return Result;
		}

		public abstract FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly);
		public virtual FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			return new FileItem[] { LinkFiles(LinkEnvironment, bBuildImportLibraryOnly) };
		}


		public virtual void CompileCSharpProject(CSharpEnvironment CompileEnvironment, FileReference ProjectFileName, FileReference DestinationFile)
		{
		}

		/// <summary>
		/// Get the name of the response file for the current linker environment and output file
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="OutputFile"></param>
		/// <returns></returns>
		public static FileReference GetResponseFileName(LinkEnvironment LinkEnvironment, FileItem OutputFile)
		{
			// Construct a relative path for the intermediate response file
			return FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, OutputFile.Reference.GetFileName() + ".response");
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		public virtual String ConvertPath(String OriginalPath)
		{
			return OriginalPath;
		}


		/// <summary>
		/// Called immediately after UnrealHeaderTool is executed to generated code for all UObjects modules.  Only is called if UnrealHeaderTool was actually run in this session.
		/// </summary>
		/// <param name="Manifest">List of UObject modules we generated code for.</param>
		public virtual void PostCodeGeneration(UHTManifest Manifest)
		{
		}

		public virtual void PreBuildSync()
		{
		}

		public virtual void PostBuildSync(UEBuildTarget Target)
		{
		}

		public virtual ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment ExecutableLinkEnvironment)
		{
			return new List<FileItem>();
		}

		public virtual void SetUpGlobalEnvironment()
		{
			ParseProjectSettings();
		}

		public virtual void ParseProjectSettings()
		{
		}

		protected void RunUnrealHeaderToolIfNeeded()
		{
		}

		public virtual void ModifyBuildProducts(UEBuildBinary Binary, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
		}

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="DebugExtension">Extension for the matching debug file (may be null).</param>
		public virtual bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return true;
		}

		protected void AddPrerequisiteSourceFile(UEBuildTarget Target, UEBuildPlatform BuildPlatform, CPPEnvironment CompileEnvironment, FileItem SourceFile, List<FileItem> PrerequisiteItems)
		{
			PrerequisiteItems.Add(SourceFile);

			var RemoteThis = this as RemoteToolChain;
			bool bAllowUploading = RemoteThis != null && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;	// Don't use remote features when compiling from a Mac
			if (bAllowUploading)
			{
				RemoteThis.QueueFileForBatchUpload(SourceFile);
			}

			if (!BuildConfiguration.bUseUBTMakefiles)	// In fast build iteration mode, we'll gather includes later on
			{
				// @todo ubtmake: What if one of the prerequisite files has become missing since it was updated in our cache? (usually, because a coder eliminated the source file)
				//		-> Two CASES:
				//				1) NOT WORKING: Non-unity file went away (SourceFile in this context).  That seems like an existing old use case.  Compile params or Response file should have changed?
				//				2) WORKING: Indirect file went away (unity'd original source file or include).  This would return a file that no longer exists and adds to the prerequiteitems list
				var IncludedFileList = CPPEnvironment.FindAndCacheAllIncludedFiles(Target, SourceFile, BuildPlatform, CompileEnvironment.Config.CPPIncludeInfo, bOnlyCachedDependencies: BuildConfiguration.bUseUBTMakefiles);
				if (IncludedFileList != null)
				{
					foreach (FileItem IncludedFile in IncludedFileList)
					{
						PrerequisiteItems.Add(IncludedFile);

						if (bAllowUploading &&
							!BuildConfiguration.bUseUBTMakefiles)	// With fast dependency scanning, we will not have an exhaustive list of dependencies here.  We rely on PostCodeGeneration() to upload these files.
						{
							RemoteThis.QueueFileForBatchUpload(IncludedFile);
						}
					}
				}
			}
		}

		public virtual void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{

		}

		public virtual void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries)
		{

		}

		public virtual void StripSymbols(string SourceFileName, string TargetFileName)
		{
			Log.TraceWarning("StripSymbols() has not been implemented for {0}; copying files", CppPlatform.ToString());
			File.Copy(SourceFileName, TargetFileName, true);
		}
	};
}