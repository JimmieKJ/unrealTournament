// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Diagnostics;

namespace UnrealBuildTool
{
	public class Unity
	{
		/// <summary>
		/// A class which represents a list of files and the sum of their lengths.
		/// </summary>
		public class FileCollection
		{
			public List<FileItem> Files       { get; private set; }
			public long           TotalLength { get; private set; }

			public FileCollection()
			{
				Files       = new List<FileItem>();
				TotalLength = 0;
			}

			public void AddFile(FileItem File)
			{
				Files.Add(File);
				TotalLength += File.Info.Length;
			}
		}

		/// <summary>
		/// A class for building up a set of unity files.  You add files one-by-one using AddFile then call EndCurrentUnityFile to finish that one and
		/// (perhaps) begin a new one.
		/// </summary>
		public class UnityFileBuilder
		{
			private List<FileCollection> UnityFiles;
			private FileCollection       CurrentCollection;
			private int                  SplitLength;

			/// <summary>
			/// Constructs a new UnityFileBuilder.
			/// </summary>
			/// <param name="InSplitLength">The accumulated length at which to automatically split a unity file, or -1 to disable automatic splitting.</param>
			public UnityFileBuilder(int InSplitLength)
			{
				UnityFiles        = new List<FileCollection>();
				CurrentCollection = new FileCollection();
				SplitLength       = InSplitLength;
			}

			/// <summary>
			/// Adds a file to the current unity file.  If splitting is required and the total size of the
			/// unity file exceeds the split limit, then a new file is automatically started.
			/// </summary>
			/// <param name="File">The file to add.</param>
			public void AddFile(FileItem File)
			{
				CurrentCollection.AddFile(File);
				if (SplitLength != -1 && CurrentCollection.TotalLength > SplitLength)
				{
					EndCurrentUnityFile();
				}
			}

			/// <summary>
			/// Starts a new unity file.  If the current unity file contains no files, this function has no effect, i.e. you will not get an empty unity file.
			/// </summary>
			public void EndCurrentUnityFile()
			{
				if (CurrentCollection.Files.Count == 0)
					return;

				UnityFiles.Add(CurrentCollection);
				CurrentCollection = new FileCollection();
			}

			/// <summary>
			/// Returns the list of built unity files.  The UnityFileBuilder is unusable after this.
			/// </summary>
			/// <returns></returns>
			public List<FileCollection> GetUnityFiles()
			{
				EndCurrentUnityFile();

				var Result = UnityFiles;

				// Null everything to ensure that failure will occur if you accidentally reuse this object.
				CurrentCollection = null;
				UnityFiles         = null;

				return Result;
			}
		}

		/// <summary>
		/// Given a set of C++ files, generates another set of C++ files that #include all the original
		/// files, the goal being to compile the same code in fewer translation units.
		/// The "unity" files are written to the CompileEnvironment's OutputDirectory.
		/// </summary>
		/// <param name="Target">The target we're building</param>
		/// <param name="CPPFiles">The C++ files to #include.</param>
		/// <param name="CompileEnvironment">The environment that is used to compile the C++ files.</param>
		/// <param name="BaseName">Base name to use for the Unity files</param>
		/// <returns>The "unity" C++ files.</returns>
		public static List<FileItem> GenerateUnityCPPs(
			UEBuildTarget Target,
			List<FileItem> CPPFiles, 
			CPPEnvironment CompileEnvironment,
			string BaseName
			)
		{
			var ToolChain = UEToolChain.GetPlatformToolChain(CompileEnvironment.Config.Target.Platform);

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			// Figure out size of all input files combined. We use this to determine whether to use larger unity threshold or not.
			long TotalBytesInCPPFiles = CPPFiles.Sum(F => F.Info.Length);

			// We have an increased threshold for unity file size if, and only if, all files fit into the same unity file. This
			// is beneficial when dealing with PCH files. The default PCH creation limit is X unity files so if we generate < X 
			// this could be fairly slow and we'd rather bump the limit a bit to group them all into the same unity file.
			//
			// Optimization only makes sense if PCH files are enabled.
			bool bForceIntoSingleUnityFile = BuildConfiguration.bStressTestUnity || (TotalBytesInCPPFiles < BuildConfiguration.NumIncludedBytesPerUnityCPP * 2 && CompileEnvironment.ShouldUsePCHs());

			// Build the list of unity files.
			List<FileCollection> AllUnityFiles;
			{
				var CPPUnityFileBuilder = new UnityFileBuilder(bForceIntoSingleUnityFile ? -1 : BuildConfiguration.NumIncludedBytesPerUnityCPP);
				foreach( var CPPFile in CPPFiles )
				{
					if (!bForceIntoSingleUnityFile && CPPFile.AbsolutePath.Contains(".GeneratedWrapper."))
					{
						CPPUnityFileBuilder.EndCurrentUnityFile();
						CPPUnityFileBuilder.AddFile(CPPFile);
						CPPUnityFileBuilder.EndCurrentUnityFile();
					}
					else
					{
						CPPUnityFileBuilder.AddFile(CPPFile);
					}

					// Now that the CPPFile is part of this unity file, we will no longer need to treat it like a root level prerequisite for our
					// dependency cache, as it is now an "indirect include" from the unity file.  We'll clear out the compile environment
					// attached to this file.  This prevents us from having to cache all of the indirect includes from these files inside our
					// dependency cache, which speeds up iterative builds a lot!
					CPPFile.CachedCPPIncludeInfo = null;
				}
				AllUnityFiles = CPPUnityFileBuilder.GetUnityFiles();
			}

			//THIS CHANGE WAS MADE TO FIX THE BUILD IN OUR BRANCH
			//DO NOT MERGE THIS BACK TO MAIN
			string PCHHeaderNameInCode = CPPFiles.Count > 0 ? CPPFiles[0].PCHHeaderNameInCode : "";
			if( CompileEnvironment.Config.PrecompiledHeaderIncludeFilename != null )
			{
				PCHHeaderNameInCode = ToolChain.ConvertPath( CompileEnvironment.Config.PrecompiledHeaderIncludeFilename );

				// Generated unity .cpp files always include the PCH using an absolute path, so we need to update
				// our compile environment's PCH header name to use this instead of the text it pulled from the original
				// C++ source files
				CompileEnvironment.Config.PCHHeaderNameInCode = PCHHeaderNameInCode;
			}

			// Create a set of CPP files that combine smaller CPP files into larger compilation units, along with the corresponding 
			// actions to compile them.
			int CurrentUnityFileCount = 0;
			var UnityCPPFiles         = new List<FileItem>();
			foreach( var UnityFile in AllUnityFiles )
			{
				++CurrentUnityFileCount;

				StringWriter OutputUnityCPPWriter      = new StringWriter();
				StringWriter OutputUnityCPPWriterExtra = null;

				// add an extra file for UBT to get the #include dependencies from
				if (BuildPlatform.RequiresExtraUnityCPPWriter() == true)
				{
					OutputUnityCPPWriterExtra = new StringWriter();
				}
				
				OutputUnityCPPWriter.WriteLine("// This file is automatically generated at compile-time to include some subset of the user-created cpp files.");

				// Explicitly include the precompiled header first, since Visual C++ expects the first top-level #include to be the header file
				// that was used to create the PCH.
				if (CompileEnvironment.Config.PrecompiledHeaderIncludeFilename != null)
				{
					OutputUnityCPPWriter.WriteLine("#include \"{0}\"", PCHHeaderNameInCode);
					if (OutputUnityCPPWriterExtra != null)
					{
						OutputUnityCPPWriterExtra.WriteLine("#include \"{0}\"", PCHHeaderNameInCode);
					}
				}

				// Add source files to the unity file
				foreach( var CPPFile in UnityFile.Files )
				{
					OutputUnityCPPWriter.WriteLine("#include \"{0}\"", ToolChain.ConvertPath(CPPFile.AbsolutePath));
                    if (OutputUnityCPPWriterExtra != null)
                    {
                        OutputUnityCPPWriterExtra.WriteLine("#include \"{0}\"", CPPFile.AbsolutePath);
                    }
                }

				// Determine unity file path name
				string UnityCPPFilePath;
				if (AllUnityFiles.Count > 1)
				{
					UnityCPPFilePath = string.Format("Module.{0}.{1}_of_{2}.cpp", BaseName, CurrentUnityFileCount, AllUnityFiles.Count);
				}
				else
				{
					UnityCPPFilePath = string.Format("Module.{0}.cpp", BaseName);
				}
				UnityCPPFilePath = Path.Combine(CompileEnvironment.Config.OutputDirectory, UnityCPPFilePath);

				// Write the unity file to the intermediate folder.
				FileItem UnityCPPFile = FileItem.CreateIntermediateTextFile(UnityCPPFilePath, OutputUnityCPPWriter.ToString());
				if (OutputUnityCPPWriterExtra != null)
				{
					FileItem.CreateIntermediateTextFile(UnityCPPFilePath + ".ex", OutputUnityCPPWriterExtra.ToString());
				}

				UnityCPPFile.RelativeCost        = UnityFile.TotalLength;
				UnityCPPFile.PCHHeaderNameInCode = PCHHeaderNameInCode;
                UnityCPPFiles.Add(UnityCPPFile);

				// Cache information about the unity .cpp dependencies
				// @todo ubtmake urgent: Fails when building remotely for Mac because unity .cpp has an include for a PCH on the REMOTE machine
				UEBuildModuleCPP.CachePCHUsageForModuleSourceFile( Target, CompileEnvironment, UnityCPPFile );
			}

			return UnityCPPFiles;
		}
	}
}
