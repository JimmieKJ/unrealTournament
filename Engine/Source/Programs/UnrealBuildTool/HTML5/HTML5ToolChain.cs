// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace UnrealBuildTool
{
	class HTML5ToolChain : VCToolChain
	{
        static string EMCCPath;
        static string PythonPath;
		static string EMSDKVersionString;
        // Debug options -- TODO: add these to SDK/Project setup?
        static bool   bEnableTracing = false;

		// cache the location of SDK tools
		public override void RegisterToolChain()
		{
            if (HTML5SDKInfo.IsSDKInstalled() && HTML5SDKInfo.IsPythonInstalled())
            {
                    EMCCPath = "\"" + HTML5SDKInfo.EmscriptenCompiler() + "\"";
					PythonPath = HTML5SDKInfo.PythonPath();
					EMSDKVersionString = HTML5SDKInfo.EmscriptenVersion().Replace(".", "");

					// set some environment variable we'll need
					//Environment.SetEnvironmentVariable("EMCC_DEBUG", "cache");
					Environment.SetEnvironmentVariable("EMCC_CORES", "8");
					Environment.SetEnvironmentVariable("EMCC_FORCE_STDLIBS", "1");
					Environment.SetEnvironmentVariable("EMCC_OPTIMIZE_NORMALLY", "1");
					// finally register the toolchain that is now ready to go
                    Log.TraceVerbose("        Registered for {0}", CPPTargetPlatform.HTML5.ToString());
					UEToolChain.RegisterPlatformToolChain(CPPTargetPlatform.HTML5, this);

   
			}
		}

		static string GetSharedArguments_Global(CPPTargetConfiguration TargetConfiguration, string Architecture)
		{
            string Result = " ";

            if (Architecture == "-win32")
            {
                return Result;
            }

            // 			Result += " -funsigned-char";
            // 			Result += " -fno-strict-aliasing";
            Result += " -fno-exceptions";
            // 			Result += " -fno-short-enums";

            Result += " -Wno-unused-value"; // appErrorf triggers this
            Result += " -Wno-switch"; // many unhandled cases
            Result += " -Wno-tautological-constant-out-of-range-compare"; // disables some warnings about comparisons from TCHAR being a char
            // this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
            Result += " -Wno-tautological-compare";

            // okay, in UE4, we'd fix the code for these, but in UE3, not worth it
            Result += " -Wno-logical-op-parentheses"; // appErrorf triggers this
            Result += " -Wno-array-bounds"; // some VectorLoads go past the end of the array, but it's okay in that case
            Result += " -Wno-invalid-offsetof"; // too many warnings kills windows clang. 

            // JavsScript option overrides (see src/settings.js)

            // we have to specify the full amount of memory with Asm.JS (1.5 G)
            // I wonder if there's a per game way to change this. 
			int TotalMemory = 256 * 1024 * 1024;
            Result += " -s TOTAL_MEMORY=" + TotalMemory.ToString();

            // no need for exceptions
            Result += " -s DISABLE_EXCEPTION_CATCHING=1";
            // enable checking for missing functions at link time as opposed to runtime
            Result += " -s WARN_ON_UNDEFINED_SYMBOLS=1";
            // we want full ES2
            Result += " -s FULL_ES2=1 ";
            // export console command handler. Export main func too because default exports ( e.g Main ) are overridden if we use custom exported functions. 
            Result += " -s EXPORTED_FUNCTIONS=\"['_main', '_resize_game', '_on_fatal']\" ";

            // NOTE: This may slow down the compiler's startup time!
            { 
                Result += " -s NO_EXIT_RUNTIME=1 --memory-init-file 1";
            }

            if (bEnableTracing)
            {
            	Result += " --tracing";
            }

            return Result;
		}
		
		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = GetSharedArguments_Global(CompileEnvironment.Config.Target.Configuration, CompileEnvironment.Config.Target.Architecture);

			if (CompileEnvironment.Config.Target.Architecture != "-win32")
			{
				// do we want debug info?
				/*			if (CompileEnvironment.Config.bCreateDebugInfo)
							{
								 Result += " -g";
							}*/

				Result += " -Wno-warn-absolute-paths ";
				Result += " -Wno-reorder"; // we disable constructor order warnings.

				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
				{
					Result += " -O0";
				}
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug || CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Development)
				{
					Result += " -s GL_ASSERTIONS=1 ";
				}
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Development)
				{
					if (UEBuildConfiguration.bCompileForSize)
					{
						Result += " -Oz -s ASM_JS=1 -s OUTLINING_LIMIT=40000";
					}
					else
					{
						Result += " -O2 -s ASM_JS=1 -s OUTLINING_LIMIT=110000";
					}
				}
				if (CompileEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
				{
					if (UEBuildConfiguration.bCompileForSize)
					{
						Result += " -Oz -s ASM_JS=1 -s OUTLINING_LIMIT=40000";
					}
					else
					{
						Result += " -O3 -s ASM_JS=1 -s OUTLINING_LIMIT=110000";
					}
				}

			}

			return Result;
		}

		static string GetCLArguments_CPP(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			if (CompileEnvironment.Config.Target.Architecture != "-win32")
			{
				Result = " -std=c++11";
			}

			return Result;
		}

		static string GetCLArguments_C(string Architecture)
		{
			string Result = "";
			return Result;
		}

		static string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = GetSharedArguments_Global(LinkEnvironment.Config.Target.Configuration, LinkEnvironment.Config.Target.Architecture);

			if (LinkEnvironment.Config.Target.Architecture != "-win32")
			{

				// enable verbose mode
				Result += " -v";

				if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
				{
					// check for alignment/etc checking
					//Result += " -s SAFE_HEAP=1";
					//Result += " -s CHECK_HEAP_ALIGN=1";
					//Result += " -s SAFE_DYNCALLS=1";

					// enable assertions in non-Shipping/Release builds
					Result += " -s ASSERTIONS=1";
				}

				if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug)
				{
					Result += " -O0";
				}
				if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Debug || LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Development)
				{
					Result += " -s GL_ASSERTIONS=1 ";
				}
				if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Development)
				{
					Result += " -O2 -s ASM_JS=1 -s OUTLINING_LIMIT=110000 ";
				}
				if (LinkEnvironment.Config.Target.Configuration == CPPTargetConfiguration.Shipping)
				{
					Result += " -O3 -s ASM_JS=1 -s OUTLINING_LIMIT=40000";
				}

				Result += " -s CASE_INSENSITIVE_FS=1 ";


			}

			return Result;
		}

		static string GetLibArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			if (LinkEnvironment.Config.Target.Architecture == "-win32")
			{
				// Prevents the linker from displaying its logo for each invocation.
				Result += " /NOLOGO";

				// Prompt the user before reporting internal errors to Microsoft.
				Result += " /errorReport:prompt";

				// Win32 build
                Result += " /MACHINE:x86";

                // Always CONSOLE because of main()
                Result += " /SUBSYSTEM:CONSOLE";

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

			return Result;
		}

		public static void CompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			var Output = Line.Data;
			if (Output == null)
			{
				return;
			}

			Output = Output.Replace("\\", "/");
			// Need to match following for clickable links
			string RegexFilePath = @"^([\/A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
			string RegexFilePath2 = @"^([A-Z]:[\/A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
			string RegexLineNumber = @"\:\d+\:\d+\:";
			string RegexDescription = @"(\serror:\s|\swarning:\s).*";

			// Get Matches
			string MatchFilePath = Regex.Match(Output, RegexFilePath).Value;
			if (MatchFilePath.Length == 0)
			{
				MatchFilePath = Regex.Match(Output, RegexFilePath2).Value;
			}
			string MatchLineNumber = Regex.Match(Output, RegexLineNumber).Value;
			string MatchDescription = Regex.Match(Output, RegexDescription).Value;

			// If any of the above matches failed, do nothing
			if (MatchFilePath.Length == 0 ||
				MatchLineNumber.Length == 0 ||
				MatchDescription.Length == 0)
			{
				Log.TraceWarning(Output);
				return;
			}

			// Convert Path
			string RegexStrippedPath = @"(Engine\/|[A-Za-z0-9_\-\.]*\/).*";
			string ConvertedFilePath = Regex.Match(MatchFilePath, RegexStrippedPath).Value;
			ConvertedFilePath = Path.GetFullPath(/*"..\\..\\" +*/ ConvertedFilePath);

			// Extract Line + Column Number
			string ConvertedLineNumber = Regex.Match(MatchLineNumber, @"\d+").Value;
			string ConvertedColumnNumber = Regex.Match(MatchLineNumber, @"(?<=:\d+:)\d+").Value;

			// Write output
			string ConvertedExpression = "  " + ConvertedFilePath + "(" + ConvertedLineNumber + "," + ConvertedColumnNumber + "):" + MatchDescription;
			Log.TraceInformation(ConvertedExpression); // To create clickable vs link
			Log.TraceInformation(Output);				// To preserve readable output log
		}

		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			if (CompileEnvironment.Config.Target.Architecture == "-win32")
			{
				return base.CompileCPPFiles(Target, CompileEnvironment, SourceFiles, ModuleName);
			}

			string Arguments = GetCLArguments_Global(CompileEnvironment);

			CPPOutput Result = new CPPOutput();

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
				Arguments += string.Format(" -D{0}", Definition);
			}

			if (bEnableTracing)
			{
				Arguments += string.Format(" -D__EMSCRIPTEN_TRACING__");
			}

			Arguments += string.Format(" -D__EMCC_VER__={0}", EMSDKVersionString);

        
			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";
                
				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile( Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems );

				// Add the source file path to the command-line.
                string FileArguments = string.Format(" \"{0}\"", SourceFile.AbsolutePath);
				var ObjectFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.HTML5].GetBinaryExtension(UEBuildBinaryType.Object);
            	// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByPath(
					Path.Combine(
						CompileEnvironment.Config.OutputDirectory,
					Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					FileArguments += GetCLArguments_C(CompileEnvironment.Config.Target.Architecture);
				}
				else
				{
					FileArguments += GetCLArguments_CPP(CompileEnvironment);
				}

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = PythonPath;

				CompileAction.CommandArguments = EMCCPath + " " + Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;

                //System.Console.WriteLine(CompileAction.CommandArguments); 
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);

				// Don't farm out creation of precomputed headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;

				// this is the final output of the compile step (a .abc file)
				Result.ObjectFiles.Add(ObjectFile);

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = true;

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.Config.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					BuildConfiguration.bAllowRemotelyCompiledPCHs;
			}
        
			return Result;
		}

		public override CPPOutput CompileRCFiles(UEBuildTarget Target, CPPEnvironment Environment, List<FileItem> RCFiles)
		{
			CPPOutput Result = new CPPOutput();

			if (Environment.Config.Target.Architecture == "-win32")
			{
				return base.CompileRCFiles(Target, Environment, RCFiles);
			}

			return Result;
		}

		/**
		 * Translates clang output warning/error messages into vs-clickable messages
		 * 
		 * @param	sender		Sending object
		 * @param	e			Event arguments (In this case, the line of string output)
		 */
		protected void RemoteOutputReceivedEventHandler(object sender, DataReceivedEventArgs e)
		{
			var Output = e.Data;
			if (Output == null)
			{
				return;
			}

			if (Utils.IsRunningOnMono)
			{
				Log.TraceInformation(Output);
			}
			else
			{
				// Need to match following for clickable links
				string RegexFilePath = @"^(\/[A-Za-z0-9_\-\.]*)+\.(cpp|c|mm|m|hpp|h)";
				string RegexLineNumber = @"\:\d+\:\d+\:";
				string RegexDescription = @"(\serror:\s|\swarning:\s).*";

				// Get Matches
				string MatchFilePath = Regex.Match(Output, RegexFilePath).Value.Replace("Engine/Source/../../", "");
				string MatchLineNumber = Regex.Match(Output, RegexLineNumber).Value;
				string MatchDescription = Regex.Match(Output, RegexDescription).Value;

				// If any of the above matches failed, do nothing
				if (MatchFilePath.Length == 0 ||
					MatchLineNumber.Length == 0 ||
					MatchDescription.Length == 0)
				{
					Log.TraceInformation(Output);
					return;
				}

				// Convert Path
				string RegexStrippedPath = @"\/Engine\/.*"; //@"(Engine\/|[A-Za-z0-9_\-\.]*\/).*";
				string ConvertedFilePath = Regex.Match(MatchFilePath, RegexStrippedPath).Value;
				ConvertedFilePath = Path.GetFullPath("..\\.." + ConvertedFilePath);

				// Extract Line + Column Number
				string ConvertedLineNumber = Regex.Match(MatchLineNumber, @"\d+").Value;
				string ConvertedColumnNumber = Regex.Match(MatchLineNumber, @"(?<=:\d+:)\d+").Value;

				// Write output
				string ConvertedExpression = "  " + ConvertedFilePath + "(" + ConvertedLineNumber + "," + ConvertedColumnNumber + "):" + MatchDescription;
				Log.TraceInformation(ConvertedExpression); // To create clickable vs link
				//			Log.TraceInformation(Output);				// To preserve readable output log
			}
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			if (LinkEnvironment.Config.Target.Architecture == "-win32")
            {
                return base.LinkFiles(LinkEnvironment, bBuildImportLibraryOnly);
            }

			FileItem OutputFile;

			// Make the final javascript file
			Action LinkAction = new Action(ActionType.Link);

			LinkAction.bCanExecuteRemotely = false;
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = PythonPath;
			LinkAction.CommandArguments = EMCCPath;
		    LinkAction.CommandArguments += GetLinkArguments(LinkEnvironment);

			// Add the input files to a response file, and pass the response file on the command-line.
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
                //System.Console.WriteLine("File  {0} ", InputFile.AbsolutePath);
                LinkAction.CommandArguments += string.Format(" \"{0}\"", InputFile.AbsolutePath);
				LinkAction.PrerequisiteItems.Add(InputFile);
			}
			if (!LinkEnvironment.Config.bIsBuildingLibrary)
			{
                    // Make sure ThirdParty libs are at the end. 
                    List<string> ThirdParty = (from Lib in LinkEnvironment.Config.AdditionalLibraries
                                     where Lib.Contains("ThirdParty")
                                     select Lib).ToList();

                    LinkEnvironment.Config.AdditionalLibraries.RemoveAll(Element => Element.Contains("ThirdParty"));
                    LinkEnvironment.Config.AdditionalLibraries.AddRange(ThirdParty);

                    foreach (string InputFile in LinkEnvironment.Config.AdditionalLibraries)
                    {
                        FileItem Item = FileItem.GetItemByPath(InputFile);

                        if (Item.AbsolutePath.Contains(".lib"))
                            continue;

                        if (Item != null)
                        {
                            if (Item.ToString().Contains(".js"))
                                LinkAction.CommandArguments += string.Format(" --js-library \"{0}\"", Item.AbsolutePath);
                            else
                                LinkAction.CommandArguments += string.Format(" \"{0}\"", Item.AbsolutePath);
                            LinkAction.PrerequisiteItems.Add(Item);
                        }
                    }
			}
			// make the file we will create
			OutputFile = FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

		    FileItem OutputBC = FileItem.GetItemByPath(LinkEnvironment.Config.OutputFilePath.Replace(".js", ".bc").Replace(".html", ".bc"));
		    LinkAction.ProducedItems.Add(OutputBC);
			LinkAction.CommandArguments += " --emit-symbol-map " + string.Format(" --save-bc \"{0}\"", OutputBC.AbsolutePath);

     		LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);

			return OutputFile;
		}

		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile)
		{
			throw new BuildException("HTML5 cannot compile C# files");
		}

        public override void AddFilesToReceipt(BuildReceipt Receipt, UEBuildBinary Binary)
        {
            // we need to include the generated .mem and .symbols file.  
			if(Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
	            Receipt.AddBuildProduct(Binary.Config.OutputFilePath + ".mem", BuildProductType.RequiredResource);
				Receipt.AddBuildProduct(Binary.Config.OutputFilePath + ".symbols", BuildProductType.RequiredResource);
			}
        }

		public override UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.HTML5;
		}
	};
}
