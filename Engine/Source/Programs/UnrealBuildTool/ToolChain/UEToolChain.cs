// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	public interface IUEToolChain
	{
		void RegisterToolChain();
		
		CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName);
		
		CPPOutput CompileRCFiles(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles);
		
		FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly);
		
		void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile);
		
		/** Converts the passed in path from UBT host to compiler native format. */
		String ConvertPath(String OriginalPath);
		
		/// <summary>
		/// Called immediately after UnrealHeaderTool is executed to generated code for all UObjects modules.  Only is called if UnrealHeaderTool was actually run in this session.
		/// </summary>
		/// <param name="Manifest">List of UObject modules we generated code for.</param>
		void PostCodeGeneration(UHTManifest Manifest);
		
		void PreBuildSync();
		
		void PostBuildSync(UEBuildTarget Target);

		ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment ExecutableLinkEnvironment);

		void SetUpGlobalEnvironment();

        void AddFilesToManifest(ref FileManifest manifest, UEBuildBinary Binary );

		void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName);

		void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries);

		string GetPlatformVersion();

		string GetPlatformDevices();

		UnrealTargetPlatform GetPlatform();
	}

	public abstract class UEToolChain : IUEToolChain
	{
		static Dictionary<CPPTargetPlatform, IUEToolChain> CPPToolChainDictionary = new Dictionary<CPPTargetPlatform, IUEToolChain>();

		public static void RegisterPlatformToolChain(CPPTargetPlatform InPlatform, IUEToolChain InToolChain)
		{
			if (CPPToolChainDictionary.ContainsKey(InPlatform) == true)
			{
				Log.TraceInformation("RegisterPlatformToolChain Warning: Registering tool chain {0} for {1} when it is already set to {2}",
					InToolChain.ToString(), InPlatform.ToString(), CPPToolChainDictionary[InPlatform].ToString());
				CPPToolChainDictionary[InPlatform] = InToolChain;
			}
			else
			{
				CPPToolChainDictionary.Add(InPlatform, InToolChain);
			}
		}

		public static void UnregisterPlatformToolChain(CPPTargetPlatform InPlatform)
		{
			CPPToolChainDictionary.Remove(InPlatform);
		}

		public static IUEToolChain GetPlatformToolChain(CPPTargetPlatform InPlatform)
		{
			if (CPPToolChainDictionary.ContainsKey(InPlatform) == true)
			{
				return CPPToolChainDictionary[InPlatform];
			}
			throw new BuildException("GetPlatformToolChain: No tool chain found for {0}", InPlatform.ToString());
		}

		public abstract void RegisterToolChain();

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


		public virtual void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile)
		{
		}

		/// <summary>
		/// Get the name of the response file for the current linker environment and output file
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="OutputFile"></param>
		/// <returns></returns>
		public static string GetResponseFileName( LinkEnvironment LinkEnvironment, FileItem OutputFile )
		{
			// Construct a relative path for the intermediate response file
			string ResponseFileName = Path.Combine( LinkEnvironment.Config.IntermediateDirectory, Path.GetFileName( OutputFile.AbsolutePath ) + ".response" );
			if (UnrealBuildTool.HasUProjectFile())
			{
				// If this is the uproject being built, redirect the intermediate
				if (Utils.IsFileUnderDirectory( OutputFile.AbsolutePath, UnrealBuildTool.GetUProjectPath() ))
				{
					ResponseFileName = Path.Combine(
						UnrealBuildTool.GetUProjectPath(), 
						BuildConfiguration.PlatformIntermediateFolder,
						Path.GetFileNameWithoutExtension(UnrealBuildTool.GetUProjectFile()),
						LinkEnvironment.Config.Target.Configuration.ToString(),
						Path.GetFileName(OutputFile.AbsolutePath) + ".response");
				}
			}
			// Convert the relative path to an absolute path
			ResponseFileName = Path.GetFullPath( ResponseFileName );

			return ResponseFileName;
		}

		/** Converts the passed in path from UBT host to compiler native format. */
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
			return new List<FileItem> ();
		}

		public virtual void SetUpGlobalEnvironment()
		{
			ParseProjectSettings();
		}

		public virtual void ParseProjectSettings()
		{
			ConfigCacheIni Ini = new ConfigCacheIni(GetPlatform(), "Engine", UnrealBuildTool.GetUProjectPath());
			bool bValue = UEBuildConfiguration.bCompileAPEX;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileApex", out bValue))
			{
				UEBuildConfiguration.bCompileAPEX = bValue;
			}

			bValue = UEBuildConfiguration.bCompileBox2D;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileBox2D", out bValue))
			{
				UEBuildConfiguration.bCompileBox2D = bValue;
			}

			bValue = UEBuildConfiguration.bCompileICU;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileICU", out bValue))
			{
				UEBuildConfiguration.bCompileICU = bValue;
			}

			bValue = UEBuildConfiguration.bCompileSimplygon;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileSimplygon", out bValue))
			{
				UEBuildConfiguration.bCompileSimplygon = bValue;
			}

			bValue = UEBuildConfiguration.bCompileLeanAndMeanUE;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileLeanAndMeanUE", out bValue))
			{
				UEBuildConfiguration.bCompileLeanAndMeanUE = bValue;
			}

			bValue = UEBuildConfiguration.bIncludeADO;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bIncludeADO", out bValue))
			{
				UEBuildConfiguration.bIncludeADO = bValue;
			}

			bValue = UEBuildConfiguration.bCompileRecast;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileRecast", out bValue))
			{
				UEBuildConfiguration.bCompileRecast = bValue;
			}

			bValue = UEBuildConfiguration.bCompileSpeedTree;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileSpeedTree", out bValue))
			{
				UEBuildConfiguration.bCompileSpeedTree = bValue;
			}

			bValue = UEBuildConfiguration.bCompileWithPluginSupport;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileWithPluginSupport", out bValue))
			{
				UEBuildConfiguration.bCompileWithPluginSupport = bValue;
			}

			bValue = UEBuildConfiguration.bCompilePhysXVehicle;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompilePhysXVehicle", out bValue))
			{
				UEBuildConfiguration.bCompilePhysXVehicle = bValue;
			}

			bValue = UEBuildConfiguration.bCompileFreeType;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileFreeType", out bValue))
			{
				UEBuildConfiguration.bCompileFreeType = bValue;
			}

			bValue = UEBuildConfiguration.bCompileForSize;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileForSize", out bValue))
			{
				UEBuildConfiguration.bCompileForSize = bValue;
			}

			bValue = UEBuildConfiguration.bCompileCEF3;
			if (Ini.GetBool("/Script/BuildSettings.BuildSettings", "bCompileCEF3", out bValue))
			{
				UEBuildConfiguration.bCompileCEF3 = bValue;
			}
		}

		protected void RunUnrealHeaderToolIfNeeded()
		{

		}

        public virtual void AddFilesToManifest(ref FileManifest manifest, UEBuildBinary Binary )
        {

        }


		protected void AddPrerequisiteSourceFile( UEBuildTarget Target, IUEBuildPlatform BuildPlatform, CPPEnvironment CompileEnvironment, FileItem SourceFile, List<FileItem> PrerequisiteItems )
		{
			PrerequisiteItems.Add( SourceFile );

			var RemoteThis = this as RemoteToolChain;
			bool bAllowUploading = RemoteThis != null && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac;	// Don't use remote features when compiling from a Mac
			if( bAllowUploading )
			{
				RemoteThis.QueueFileForBatchUpload(SourceFile);
			}

			if( !BuildConfiguration.bUseExperimentalFastBuildIteration )	// In fast build iteration mode, we'll gather includes later on
			{
				// @todo fastubt: What if one of the prerequisite files has become missing since it was updated in our cache? (usually, because a coder eliminated the source file)
				//		-> Two CASES:
				//				1) NOT WORKING: Non-unity file went away (SourceFile in this context).  That seems like an existing old use case.  Compile params or Response file should have changed?
				//				2) WORKING: Indirect file went away (unity'd original source file or include).  This would return a file that no longer exists and adds to the prerequiteitems list
				var IncludedFileList = CPPEnvironment.FindAndCacheAllIncludedFiles( Target, SourceFile, BuildPlatform, CompileEnvironment.Config.CPPIncludeInfo, bOnlyCachedDependencies:BuildConfiguration.bUseExperimentalFastDependencyScan );
				foreach (FileItem IncludedFile in IncludedFileList)
				{
					PrerequisiteItems.Add( IncludedFile );

					if( bAllowUploading &&
						!BuildConfiguration.bUseExperimentalFastDependencyScan )	// With fast dependency scanning, we will not have an exhaustive list of dependencies here.  We rely on PostCodeGeneration() to upload these files.
					{
						RemoteThis.QueueFileForBatchUpload(IncludedFile);
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

		public virtual string GetPlatformVersion()
		{
			return "";
		}

		public virtual string GetPlatformDevices()
		{
			return "";
		}

		public virtual UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.Unknown;
		}
	};
}