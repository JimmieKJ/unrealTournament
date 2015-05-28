// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Text;

namespace UnrealBuildTool
{
	class MacToolChain : RemoteToolChain
	{
		public override void RegisterToolChain()
		{
			RegisterRemoteToolChain(UnrealTargetPlatform.Mac, CPPTargetPlatform.Mac);
		}

		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************

		/** Which version of the Mac OS SDK to target at build time */
		public static string MacOSSDKVersion = "latest";

		/** Which version of the Mac OS X to allow at run time */
		public static string MacOSVersion = "10.9";

		/** Minimum version of Mac OS X to actually run on, running on earlier versions will display the system minimum version error dialog & exit. */
		public static string MinMacOSVersion = "10.9.2";

		/** Which developer directory to root from */
		private static string XcodeDeveloperDir = "/Applications/Xcode.app/Contents/Developer/";

		/** Location of the SDKs */
		private static string BaseSDKDir;

		/** Which compiler frontend to use */
		private static string MacCompiler = "clang++";

		/** Which linker frontend to use */
		private static string MacLinker = "clang++";

		/** Which archiver to use */
		private static string MacArchiver = "libtool";


		/** Track which scripts need to be deleted before appending to */
		private bool bHasWipedCopyDylibScript = false;
		private bool bHasWipedFixDylibScript = false;

		private static List<FileItem> BundleDependencies = new List<FileItem>();

		public List<string> BuiltBinaries = new List<string>();

		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			BaseSDKDir = XcodeDeveloperDir + "Platforms/MacOSX.platform/Developer/SDKs";

			if (MacOSSDKVersion == "latest")
			{
				try
				{
					string[] SubDirs = null;
					if (Utils.IsRunningOnMono)
					{
						// on the Mac, we can just get the directory name
						SubDirs = System.IO.Directory.GetDirectories(BaseSDKDir);
					}
					else
					{
						Hashtable Results = RPCUtilHelper.Command("/", "ls", BaseSDKDir, null);
						if (Results != null)
						{
							string Result = (string)Results["CommandOutput"];
							SubDirs = Result.Split("\r\n".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
						}
					}

					// loop over the subdirs and parse out the version
					float MaxSDKVersion = 0.0f;
					string MaxSDKVersionString = null;
					foreach (string SubDir in SubDirs)
					{
						string SubDirName = Path.GetFileNameWithoutExtension(SubDir);
						if (SubDirName.StartsWith("MacOSX10."))
						{
							// get the SDK version from the directory name
							string SDKString = SubDirName.Replace("MacOSX10.", "");
							float SDKVersion = 0.0f;
							try
							{
								SDKVersion = float.Parse(SDKString, System.Globalization.CultureInfo.InvariantCulture);
							}
							catch (Exception)
							{
								// weirdly formatted SDKs
								continue;
							}

							// update largest SDK version number
							if (SDKVersion > MaxSDKVersion)
							{
								MaxSDKVersion = SDKVersion;
								MaxSDKVersionString = SDKString;
							}
						}
					}

					// convert back to a string with the exact format
					if (MaxSDKVersionString != null)
					{
						MacOSSDKVersion = "10." + MaxSDKVersionString;
					}
				}
				catch (Exception Ex)
				{
					// on any exception, just use the backup version
					Log.TraceInformation("Triggered an exception while looking for SDK directory in Xcode.app");
					Log.TraceInformation("{0}", Ex.ToString());
				}

				if (MacOSSDKVersion == "latest")
				{
					throw new BuildException("Unable to determine SDK version from Xcode, we cannot continue");
				}
			}

			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					Log.TraceInformation("Compiling with Mac SDK {0} on Mac {1}", MacOSSDKVersion, RemoteServerName);
				}
				else
				{
					Log.TraceInformation("Compiling with Mac SDK {0}", MacOSSDKVersion);
				}
			}
		}

		static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";

			Result += " -fexceptions";
			Result += " -fasm-blocks";

			Result += " -Wall -Werror";

			Result += " -Wno-unused-variable";
			Result += " -Wno-unused-value";
			// This will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// This hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// This hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			// This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			Result += " -Wno-invalid-offsetof"; // needed to suppress warnings about using offsetof on non-POD types.

			if (CompileEnvironment.Config.bEnableShadowVariableWarning)
			{
				Result += " -Wshadow";
			}

			// @todo: Remove these two when the code is fixed and they're no longer needed
			Result += " -Wno-logical-op-parentheses";
			Result += " -Wno-unknown-pragmas";

			Result += " -c";

			Result += " -arch x86_64";
			Result += " -isysroot " + BaseSDKDir + "/MacOSX" + MacOSSDKVersion + ".sdk";
			Result += " -mmacosx-version-min=" + MacOSVersion;

			// Optimize non- debug builds.
			if (CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug)
			{
				if (UEBuildConfiguration.bCompileForSize)
				{
					Result += " -Oz";
				}
				else
				{
					Result += " -O3";
				}
			}
			else
			{
				Result += " -O0";
			}

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				Result += " -gdwarf-2";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -fno-rtti";
			Result += " -std=c++11";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -fno-rtti";
			Result += " -std=c++11";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++11";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x objective-c++-header";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -fno-rtti";
			Result += " -std=c++11";
			Result += " -stdlib=libc++";
			return Result;
		}

		string AddFrameworkToLinkCommand(string FrameworkName, string Arg = "-framework")
		{
			string Result = "";
			if (FrameworkName.EndsWith(".framework"))
			{
				Result += " -F \"" + ConvertPath(Path.GetDirectoryName(Path.GetFullPath(FrameworkName))) + "\"";
				FrameworkName = Path.GetFileNameWithoutExtension(FrameworkName);
			}
			Result += " " + Arg + " \"" + FrameworkName + "\"";
			return Result;
		}

		string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -arch x86_64";
			Result += " -isysroot " + BaseSDKDir + "/MacOSX" + MacOSSDKVersion + ".sdk";
			Result += " -mmacosx-version-min=" + MacOSVersion;
			Result += " -dead_strip";

			if (LinkEnvironment.Config.bIsBuildingDLL)
			{
				Result += " -dynamiclib";
			}

			// Needed to make sure install_name_tool will be able to update paths in Mach-O headers
			Result += " -headerpad_max_install_names";

			Result += " -lc++";

			return Result;
		}

		static string GetArchiveArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";
			Result += " -static";
			return Result;
		}

		public override CPPOutput CompileCPPFiles(UEBuildTarget Target, CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			var Arguments = new StringBuilder();
			var PCHArguments = new StringBuilder();

			Arguments.Append(GetCompileArguments_Global(CompileEnvironment));

			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				var PrecompiledFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Mac].GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				PCHArguments.Append(" -include \"");
				PCHArguments.Append(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PrecompiledFileExtension, ""));
				PCHArguments.Append("\"");
			}

			// Add include paths to the argument list.
			HashSet<string> AllIncludes = new HashSet<string>(CompileEnvironment.Config.CPPIncludeInfo.IncludePaths);
			AllIncludes.UnionWith(CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths);
			foreach (string IncludePath in AllIncludes)
			{
				Arguments.Append(" -I\"");

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					Arguments.Append(ConvertPath(Path.GetFullPath(IncludePath)));

					// sync any third party headers we may need
					if (IncludePath.Contains("ThirdParty"))
					{
						string[] FileList = Directory.GetFiles(IncludePath, "*.h", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}

						FileList = Directory.GetFiles(IncludePath, "*.cpp", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}
					}
				}
				else
				{
					Arguments.Append(IncludePath);
				}

				Arguments.Append("\"");
			}

			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				Arguments.Append(" -D\"");
				Arguments.Append(Definition);
				Arguments.Append("\"");
			}

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Config.Target.Platform);

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Compile the file as a C++ PCH.
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
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();

					// only use PCH for .cpp files
					FileArguments += PCHArguments.ToString();
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile( Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems );

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledHeaderExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Mac].GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledHeaderExtension
							)
						);

					FileItem RemotePrecompiledHeaderFile = LocalToRemoteFileItem(PrecompiledHeaderFile, false);
					CompileAction.ProducedItems.Add(RemotePrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = RemotePrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", RemotePrecompiledHeaderFile.AbsolutePath, false);
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}
					var ObjectFileExtension = UEBuildPlatform.BuildPlatformDictionary[UnrealTargetPlatform.Mac].GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);

					FileItem RemoteObjectFile = LocalToRemoteFileItem(ObjectFile, false);
					CompileAction.ProducedItems.Add(RemoteObjectFile);
					Result.ObjectFiles.Add(RemoteObjectFile);
					FileArguments += string.Format(" -o \"{0}\"", RemoteObjectFile.AbsolutePath, false);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", ConvertPath(SourceFile.AbsolutePath), false);

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				}

				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = "xcrun";
				CompileAction.CommandArguments = MacCompiler + Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			}
			return Result;
		}

		private void AppendMacLine(StreamWriter Writer, string Format, params object[] Arg)
		{
			string PreLine = String.Format(Format, Arg);
			Writer.Write(PreLine + "\n");
		}

		private int LoadEngineCL()
		{
			string[] VersionHeader = Utils.ReadAllText("../Source/Runtime/Launch/Resources/Version.h").Replace("\r\n", "\n").Replace("\t", " ").Split('\n');
			foreach (string Line in VersionHeader)
			{
				if (Line.StartsWith("#define ENGINE_VERSION "))
				{
					return int.Parse(Line.Split(' ')[2]);
				}
			}
			return 0;
		}

		private string LoadEngineDisplayVersion(bool bIgnorePatchVersion = false)
		{
			string[] VersionHeader = Utils.ReadAllText("../Source/Runtime/Launch/Resources/Version.h").Replace("\r\n", "\n").Replace("\t", " ").Split('\n');
			string EngineVersionMajor = "4";
			string EngineVersionMinor = "0";
			string EngineVersionPatch = "0";
			foreach (string Line in VersionHeader)
			{
				if (Line.StartsWith("#define ENGINE_MAJOR_VERSION "))
				{
					EngineVersionMajor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define ENGINE_MINOR_VERSION "))
				{
					EngineVersionMinor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define ENGINE_PATCH_VERSION ") && !bIgnorePatchVersion)
				{
					EngineVersionPatch = Line.Split(' ')[2];
				}
			}
			return EngineVersionMajor + "." + EngineVersionMinor + "." + EngineVersionPatch;
		}

		private string LoadLauncherDisplayVersion()
		{
			string[] VersionHeader = Utils.ReadAllText("../Source/Programs/NoRedist/UnrealEngineLauncher/Private/Version.h").Replace("\r\n", "\n").Replace("\t", " ").Split('\n');
			string LauncherVersionMajor = "1";
			string LauncherVersionMinor = "0";
			string LauncherVersionPatch = "0";
			foreach (string Line in VersionHeader)
			{
				if (Line.StartsWith("#define LAUNCHER_MAJOR_VERSION "))
				{
					LauncherVersionMajor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define LAUNCHER_MINOR_VERSION "))
				{
					LauncherVersionMinor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define LAUNCHER_PATCH_VERSION "))
				{
					LauncherVersionPatch = Line.Split(' ')[2];
				}
			}
			return LauncherVersionMajor + "." + LauncherVersionMinor + "." + LauncherVersionPatch;
		}

		private int LoadBuiltFromChangelistValue()
		{
			string[] VersionHeader = Utils.ReadAllText("../Source/Runtime/Launch/Resources/Version.h").Replace("\r\n", "\n").Replace("\t", " ").Split('\n');
			foreach (string Line in VersionHeader)
			{
				if (Line.StartsWith("#define BUILT_FROM_CHANGELIST "))
				{
					return int.Parse(Line.Split(' ')[2]);
				}
			}
			return 0;
		}

		private string LoadEngineAPIVersion()
		{
			int CL = 0;
			// @todo: Temp solution to work around a problem with parsing ModuleVersion.h updated for 4.4.1 hotfix
			int BuiltFromChangelist = LoadBuiltFromChangelistValue();
			if (BuiltFromChangelist > 0)
			{
				foreach (string Line in File.ReadAllLines("../Source/Runtime/Core/Public/Modules/ModuleVersion.h"))
				{
					string[] Tokens = Line.Split(' ', '\t');
					if (Tokens[0] == "#define" && Tokens[1] == "MODULE_API_VERSION")
					{
						if(Tokens[2] == "BUILT_FROM_CHANGELIST")
						{
							CL = LoadEngineCL();
						}
						else
						{
							CL = int.Parse(Tokens[2]);
						}
						break;
					}
				}
			}
			return String.Format("{0}.{1}.{2}", CL / (100 * 100), (CL / 100) % 100, CL % 100);
		}

		private void AddLibraryPathToRPaths(string Library, string ExeAbsolutePath, ref List<string> RPaths, ref string LinkCommand, bool bIsBuildingAppBundle)
		{
			string LibraryDir = Path.GetDirectoryName(Library);
			string ExeDir = Path.GetDirectoryName(ExeAbsolutePath);
			if (!Library.Contains("/Engine/Binaries/Mac/") && (Library.EndsWith("dylib") || Library.EndsWith(".framework")) && LibraryDir != ExeDir)
			{
				string RelativePath = Utils.MakePathRelativeTo(LibraryDir, ExeDir).Replace("\\", "/");
				if (!RelativePath.Contains(LibraryDir) && !RPaths.Contains(RelativePath))
				{
					RPaths.Add(RelativePath);
					LinkCommand += " -rpath \"@loader_path/" + RelativePath + "\"";

					if (bIsBuildingAppBundle)
					{
						string PathInBundle = Path.Combine(Path.GetDirectoryName(ExeDir), "UE4/Engine/Binaries/Mac", RelativePath.Substring(9));
						Utils.CollapseRelativeDirectories(ref PathInBundle);
						string RelativePathInBundle = Utils.MakePathRelativeTo(PathInBundle, ExeDir).Replace("\\", "/");
						LinkCommand += " -rpath \"@loader_path/" + RelativePathInBundle + "\"";
					}
				}
			}
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			bool bIsBuildingLibrary = LinkEnvironment.Config.bIsBuildingLibrary || bBuildImportLibraryOnly;

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = "/bin/sh";
			LinkAction.CommandDescription = "Link";

			string EngineAPIVersion = LoadEngineAPIVersion();
			string EngineDisplayVersion = LoadEngineDisplayVersion(true);
			string VersionArg = LinkEnvironment.Config.bIsBuildingDLL ? " -current_version " + EngineAPIVersion + " -compatibility_version " + EngineDisplayVersion : "";

			string Linker = bIsBuildingLibrary ? MacArchiver : MacLinker;
			string LinkCommand = "xcrun " + Linker + VersionArg + " " + (bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment));

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = !Utils.IsRunningOnMono && (bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL);

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(Path.GetFullPath(LinkEnvironment.Config.OutputFilePath));
			OutputFile.bNeedsHotReloadNumbersDLLCleanUp = LinkEnvironment.Config.bIsBuildingDLL;

			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

			// To solve the problem with cross dependencies, for now we create a broken dylib that does not link with other engine dylibs.
			// This is fixed in later step, FixDylibDependencies. For this and to know what libraries to copy whilst creating an app bundle,
			// we gather the list of engine dylibs.
			List<string> EngineAndGameLibraries = new List<string>();

			string DylibsPath = "@rpath";

			string AbsolutePath = OutputFile.AbsolutePath.Replace("\\", "/");
			if (!bIsBuildingLibrary)
			{
				LinkCommand += " -rpath @loader_path/ -rpath @executable_path/";
			}

			List<string> ThirdPartyLibraries = new List<string>();

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// Add any additional files that we'll need in order to link the app
				foreach (string AdditionalShadowFile in LinkEnvironment.Config.AdditionalShadowFiles)
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalShadowFile);
					if (ShadowFile != null)
					{
						QueueFileForBatchUpload(ShadowFile);
						LinkAction.PrerequisiteItems.Add(ShadowFile);
					}
					else
					{
						throw new BuildException("Couldn't find required additional file to shadow: {0}", AdditionalShadowFile);
					}
				}

				// Add any frameworks to be shadowed to the remote
				foreach (string FrameworkPath in LinkEnvironment.Config.Frameworks)
				{
					if(FrameworkPath.EndsWith(".framework"))
					{
						foreach(string FrameworkFile in Directory.EnumerateFiles(FrameworkPath, "*", SearchOption.AllDirectories))
						{
							FileItem FrameworkFileItem = FileItem.GetExistingItemByPath(FrameworkFile);
							QueueFileForBatchUpload(FrameworkFileItem);
							LinkAction.PrerequisiteItems.Add(FrameworkFileItem);
						}
					}
				}
			}

			bool bIsBuildingAppBundle = !LinkEnvironment.Config.bIsBuildingDLL && !LinkEnvironment.Config.bIsBuildingLibrary && !LinkEnvironment.Config.bIsBuildingConsoleApplication;

			List<string> RPaths = new List<string>();

			if (!bIsBuildingLibrary || LinkEnvironment.Config.bIncludeDependentLibrariesInLibrary)
			{
				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
				{
					// Can't link dynamic libraries when creating a static one
					if (bIsBuildingLibrary && (Path.GetExtension(AdditionalLibrary) == ".dylib" || AdditionalLibrary == "z"))
					{
						continue;
					}

					if (Path.GetFileName(AdditionalLibrary).StartsWith("lib"))
					{
						LinkCommand += string.Format(" \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));
						if (Path.GetExtension(AdditionalLibrary) == ".dylib")
						{
							ThirdPartyLibraries.Add(AdditionalLibrary);
						}

						if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
						{
							// copy over libs we may need
							FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalLibrary);
							if (ShadowFile != null)
							{
								QueueFileForBatchUpload(ShadowFile);
							}
						}
					}
					else if (Path.GetDirectoryName(AdditionalLibrary) != "" &&
					         (Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries/Mac") ||
					         Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries\\Mac")))
					{
						// It's an engine or game dylib. Save it for later
						EngineAndGameLibraries.Add(ConvertPath(Path.GetFullPath(AdditionalLibrary)));

						if (!Utils.IsRunningOnMono)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(Path.GetFullPath(AdditionalLibrary));
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
							FileItem RemoteEngineLibDependency = FileItem.GetRemoteItemByPath(ConvertPath(Path.GetFullPath(AdditionalLibrary)), UnrealTargetPlatform.Mac);
							LinkAction.PrerequisiteItems.Add(RemoteEngineLibDependency);
							//Log.TraceInformation("Adding {0} / {1} as a prereq to {2}", EngineLibDependency.AbsolutePath, RemoteEngineLibDependency.AbsolutePath, RemoteOutputFile.AbsolutePath);
						}
						else if (LinkEnvironment.Config.bIsCrossReferenced == false)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(Path.GetFullPath(AdditionalLibrary));
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
						}
					}
					else if (AdditionalLibrary.Contains(".framework/"))
					{
						LinkCommand += string.Format(" \'{0}\'", AdditionalLibrary);
					}
					else
					{
						LinkCommand += string.Format(" -l{0}", AdditionalLibrary);
					}

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);
				}

				foreach (string AdditionalLibrary in LinkEnvironment.Config.DelayLoadDLLs)
				{
					// Can't link dynamic libraries when creating a static one
					if (bIsBuildingLibrary && (Path.GetExtension(AdditionalLibrary) == ".dylib" || AdditionalLibrary == "z"))
					{
						continue;
					}

					LinkCommand += string.Format(" -weak_library \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));

					AddLibraryPathToRPaths(AdditionalLibrary, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);
				}
			}

			// Add frameworks
			Dictionary<string, bool> AllFrameworks = new Dictionary<string, bool>();
			foreach (string Framework in LinkEnvironment.Config.Frameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework))
				{
					AllFrameworks.Add(Framework, false);
				}
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.Config.AdditionalFrameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework.FrameworkName))
				{
					AllFrameworks.Add(Framework.FrameworkName, false);
				}
			}
			foreach (string Framework in LinkEnvironment.Config.WeakFrameworks)
			{
				if (!AllFrameworks.ContainsKey(Framework))
				{
					AllFrameworks.Add(Framework, true);
				}
			}

			if (!bIsBuildingLibrary)
			{
				foreach (var Framework in AllFrameworks)
				{
					LinkCommand += AddFrameworkToLinkCommand(Framework.Key, Framework.Value ? "-weak_framework" : "-framework");
					AddLibraryPathToRPaths(Framework.Key, AbsolutePath, ref RPaths, ref LinkCommand, bIsBuildingAppBundle);
				}
			}

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				if (bIsBuildingLibrary)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				}
				else
				{
					string EnginePath = ConvertPath(Path.GetDirectoryName(Directory.GetCurrentDirectory()));
					string InputFileRelativePath = InputFile.AbsolutePath.Replace(EnginePath, "..");
					InputFileNames.Add(string.Format("\"{0}\"", InputFileRelativePath));
				}
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkCommand += " " + Filename;
				}
			}
			else
			{
				// Write the list of input files to a response file, with a tempfilename, on remote machine
				string ResponsePath = Path.Combine(LinkEnvironment.Config.IntermediateDirectory, Path.GetFileName(OutputFile.AbsolutePath) + ".response");
				
				// Never create response files when we are only generating IntelliSense data
				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
					ResponseFile.Create(ResponsePath, InputFileNames);

					if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
					{
						RPCUtilHelper.CopyFile(ResponsePath, ConvertPath(ResponsePath), true);
					}
				}

				LinkCommand += string.Format(" @\"{0}\"", ConvertPath(ResponsePath));
			}

			if (LinkEnvironment.Config.bIsBuildingDLL)
			{
				// Add the output file to the command-line.
				string Filename = "";
				int Index = OutputFile.AbsolutePath.LastIndexOf(".app/Contents/MacOS/");
				if (Index > -1)
				{
					Index += ".app/Contents/MacOS/".Length;
					Filename = OutputFile.AbsolutePath.Substring(Index);
				}
				else
				{
					Filename = Path.GetFileName(OutputFile.AbsolutePath);
				}
				LinkCommand += string.Format(" -install_name {0}/{1}", DylibsPath, Filename);
			}

			if (!bIsBuildingLibrary)
			{
				if (UnrealBuildTool.RunningRocket() || (Utils.IsRunningOnMono && LinkEnvironment.Config.bIsCrossReferenced == false))
				{
					foreach (string Library in EngineAndGameLibraries)
					{
						string LibraryPath = Library;
						if (!File.Exists(Library))
						{
							string LibraryDir = Path.GetDirectoryName(Library);
							string LibraryName = Path.GetFileName(Library);
							string AppBundleName = "UE4Editor";
							if (LibraryName.Contains("UE4Editor-Mac-"))
							{
								string[] Parts = LibraryName.Split('-');
								AppBundleName += "-" + Parts[1] + "-" + Parts[2];
							}
							AppBundleName += ".app";
							LibraryPath = LibraryDir + "/" + AppBundleName + "/Contents/MacOS/" + LibraryName;
							if (!File.Exists(LibraryPath))
							{
								LibraryPath = Library;
							}
						}
						LinkCommand += " \"" + LibraryPath + "\"";
					}
				}
				else
				{
					// Tell linker to ignore unresolved symbols, so we don't have a problem with cross dependent dylibs that do not exist yet.
					// This is fixed in later step, FixDylibDependencies.
					LinkCommand += string.Format(" -undefined dynamic_lookup");
				}
			}

			// Add the output file to the command-line.
			LinkCommand += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommand += LinkEnvironment.Config.AdditionalArguments;

			if (!bIsBuildingLibrary)
			{
				// Fix the paths for third party libs
				foreach (string Library in ThirdPartyLibraries)
				{
					string LibraryFileName = Path.GetFileName(Library);
					LinkCommand += "; xcrun install_name_tool -change " + LibraryFileName + " " + DylibsPath + "/" + LibraryFileName + " \"" + ConvertPath(OutputFile.AbsolutePath) + "\"";
				}
			}

			LinkAction.CommandArguments = "-c '" + LinkCommand + "'";

			// Only execute linking on the local Mac.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			
			LinkAction.ProducedItems.Add(RemoteOutputFile);

			if (!Directory.Exists(LinkEnvironment.Config.IntermediateDirectory))
			{
				return OutputFile;
			}

			if (!bIsBuildingLibrary)
			{
				// Prepare a script that will run later, once every dylibs and the executable are created. This script will be called by action created in FixDylibDependencies()
				string FixDylibDepsScriptPath = Path.Combine(LinkEnvironment.Config.LocalShadowDirectory, "FixDylibDependencies.sh");
				if (!bHasWipedFixDylibScript)
				{
					if (File.Exists(FixDylibDepsScriptPath))
					{
						File.Delete(FixDylibDepsScriptPath);
					}
					bHasWipedFixDylibScript = true;
				}

				if (!Directory.Exists(LinkEnvironment.Config.LocalShadowDirectory))
				{
					Directory.CreateDirectory(LinkEnvironment.Config.LocalShadowDirectory);
				}

				StreamWriter FixDylibDepsScript = File.AppendText(FixDylibDepsScriptPath);

				if (LinkEnvironment.Config.bIsCrossReferenced || !Utils.IsRunningOnMono)
				{
					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibraries)
					{
						EngineAndGameLibrariesString += " \"" + Library + "\"";
					}
					string FixDylibLine = "pushd \"" + ConvertPath(Directory.GetCurrentDirectory()) + "\"  > /dev/null; ";
					FixDylibLine += string.Format("TIMESTAMP=`stat -n -f \"%Sm\" -t \"%Y%m%d%H%M.%S\" \"{0}\"`; ", RemoteOutputFile.AbsolutePath);
					FixDylibLine += LinkCommand.Replace("-undefined dynamic_lookup", EngineAndGameLibrariesString).Replace("$", "\\$");
					FixDylibLine += string.Format("; touch -t $TIMESTAMP \"{0}\"; if [[ $? -ne 0 ]]; then exit 1; fi; ", RemoteOutputFile.AbsolutePath);
					FixDylibLine += "popd > /dev/null";
					AppendMacLine(FixDylibDepsScript, FixDylibLine);
				}

				FixDylibDepsScript.Close();

				// Prepare a script that will be called by FinalizeAppBundle.sh to copy all necessary third party dylibs to the app bundle
				// This is done this way as FinalizeAppBundle.sh script can be created before all the libraries are processed, so
				// at the time of it's creation we don't have the full list of third party dylibs all modules need.
				string DylibCopyScriptPath = Path.Combine(LinkEnvironment.Config.IntermediateDirectory, "DylibCopy.sh");
				if (!bHasWipedCopyDylibScript)
				{
					if (File.Exists(DylibCopyScriptPath))
					{
						File.Delete(DylibCopyScriptPath);
					}
					bHasWipedCopyDylibScript = true;
				}
				string ExistingScript = File.Exists(DylibCopyScriptPath) ? File.ReadAllText(DylibCopyScriptPath) : "";
				StreamWriter DylibCopyScript = File.AppendText(DylibCopyScriptPath);
				foreach (string Library in ThirdPartyLibraries)
				{
					string CopyCommandLineEntry = string.Format("cp -f \"{0}\" \"$1.app/Contents/MacOS\"", ConvertPath(Path.GetFullPath(Library)).Replace("$", "\\$"));
					if (!ExistingScript.Contains(CopyCommandLineEntry))
					{
						AppendMacLine(DylibCopyScript, CopyCommandLineEntry);
					}
				}
				DylibCopyScript.Close();

				// For non-console application, prepare a script that will create the app bundle. It'll be run by FinalizeAppBundle action
				if (bIsBuildingAppBundle)
				{
					string FinalizeAppBundleScriptPath = Path.Combine(LinkEnvironment.Config.IntermediateDirectory, "FinalizeAppBundle.sh");
					StreamWriter FinalizeAppBundleScript = File.CreateText(FinalizeAppBundleScriptPath);
					AppendMacLine(FinalizeAppBundleScript, "#!/bin/sh");
					string BinariesPath = Path.GetDirectoryName(OutputFile.AbsolutePath);
					BinariesPath = Path.GetDirectoryName(BinariesPath.Substring(0, BinariesPath.IndexOf(".app")));
					AppendMacLine(FinalizeAppBundleScript, "cd \"{0}\"", ConvertPath(BinariesPath).Replace("$", "\\$"));

					string ExeName = Path.GetFileName(OutputFile.AbsolutePath);
					string[] ExeNameParts = ExeName.Split('-');
					string GameName = ExeNameParts[0];

					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/MacOS\"", ExeName);
					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/Resources\"", ExeName);

					// Copy third party dylibs by calling additional script prepared earlier
					AppendMacLine(FinalizeAppBundleScript, "sh \"{0}\" \"{1}\"", ConvertPath(DylibCopyScriptPath).Replace("$", "\\$"), ExeName);

					string IconName = "UE4";
					string BundleVersion = ExeName.StartsWith("UnrealEngineLauncher") ? LoadLauncherDisplayVersion() : LoadEngineDisplayVersion();
					string EngineSourcePath = ConvertPath(Directory.GetCurrentDirectory()).Replace("$", "\\$");

					string UProjectFilePath = UProjectInfo.GetProjectFilePath(GameName);
					string CustomResourcesPath = "";
					string CustomBuildPath = "";
					if (string.IsNullOrEmpty(UProjectFilePath))
					{
						string[] TargetFiles = Directory.GetFiles(Directory.GetCurrentDirectory(), GameName + ".Target.cs", SearchOption.AllDirectories);
						if (TargetFiles.Length == 1)
						{
							CustomResourcesPath = Path.GetDirectoryName(TargetFiles[0]) + "/Resources/Mac";
							CustomBuildPath = Path.GetDirectoryName(TargetFiles[0]) + "../Build/Mac";
						}
						else
						{
							Log.TraceWarning("Found {0} Target.cs files for {1} in alldir search of directory {2}", TargetFiles.Length, GameName, Directory.GetCurrentDirectory());
						}
					}
					else
					{
						CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath) + "/Source/" + GameName + "/Resources/Mac";
						CustomBuildPath = Path.GetDirectoryName(UProjectFilePath) + "/Build/Mac";
					}

					bool bBuildingEditor = GameName.EndsWith("Editor");

					// Copy resources
					string DefaultIcon = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + IconName + ".icns";
					string CustomIcon = "";
					if (bBuildingEditor)
					{
						CustomIcon = DefaultIcon;
					}
					else
					{
						CustomIcon = CustomBuildPath + "/Application.icns";
						if (!File.Exists(CustomIcon))
						{
							CustomIcon = CustomResourcesPath + "/" + GameName + ".icns";
							if (!File.Exists(CustomIcon))
							{
								CustomIcon = DefaultIcon;
							}
						}

						if (CustomIcon != DefaultIcon)
						{
							QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath(CustomIcon)));
							CustomIcon = ConvertPath(CustomIcon);
						}
					}
					AppendMacLine(FinalizeAppBundleScript, "cp -f \"{0}\" \"{2}.app/Contents/Resources/{1}.icns\"", CustomIcon, IconName, ExeName);

					if (ExeName.StartsWith("UE4Editor"))
					{
						AppendMacLine(FinalizeAppBundleScript, "cp -f \"{0}/Runtime/Launch/Resources/Mac/UProject.icns\" \"{1}.app/Contents/Resources/UProject.icns\"", EngineSourcePath, ExeName);
					}

					string InfoPlistFile = CustomResourcesPath + "/Info.plist";
					if (!File.Exists(InfoPlistFile))
					{
						InfoPlistFile = EngineSourcePath + "/Runtime/Launch/Resources/Mac/" + (bBuildingEditor ? "Info-Editor.plist" : "Info.plist");
					}
					else
					{
						QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath(InfoPlistFile)));
						InfoPlistFile = ConvertPath(InfoPlistFile);
					}
					AppendMacLine(FinalizeAppBundleScript, "cp -f \"{0}\" \"{1}.app/Contents/Info.plist\"", InfoPlistFile, ExeName);

					// Fix contents of Info.plist
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{1}.app/Contents/Info.plist\"", "{EXECUTABLE_NAME}", ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{APP_NAME}", GameName, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{MACOSX_DEPLOYMENT_TARGET}", MinMacOSVersion, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{ICON_NAME}", IconName, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{BUNDLE_VERSION}", BundleVersion, ExeName);

					// Generate PkgInfo file
					AppendMacLine(FinalizeAppBundleScript, "echo 'echo -n \"APPL????\"' | bash > \"{0}.app/Contents/PkgInfo\"", ExeName);

					// Make sure OS X knows the bundle was updated
					AppendMacLine(FinalizeAppBundleScript, "touch -c \"{0}.app\"", ExeName);

					FinalizeAppBundleScript.Close();

					// copy over some needed files
					// @todo mac: Make a QueueDirectoryForBatchUpload
					QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath("../../Engine/Source/Runtime/Launch/Resources/Mac/UE4.icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath("../../Engine/Source/Runtime/Launch/Resources/Mac/UProject.icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath("../../Engine/Source/Runtime/Launch/Resources/Mac/Info.plist")));
					QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.Combine(LinkEnvironment.Config.IntermediateDirectory, "DylibCopy.sh")));
				}
			}

			// For Mac, generate the dSYM file if the config file is set to do so
			if ((BuildConfiguration.bGeneratedSYMFile == true || BuildConfiguration.bUsePDBFiles == true) && (!bIsBuildingLibrary || LinkEnvironment.Config.bIsBuildingDLL))
			{
				Log.TraceInformation("Generating dSYM file for {0} - this will add some time to your build...", Path.GetFileName(OutputFile.AbsolutePath));
				RemoteOutputFile = GenerateDebugInfo(OutputFile);
			}

			return RemoteOutputFile;
		}

		FileItem FixDylibDependencies(LinkEnvironment LinkEnvironment, FileItem Executable)
		{
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = "/bin/sh";
			LinkAction.CommandDescription = "";

			// Call the FixDylibDependencies.sh script which will link the dylibs and the main executable, this time proper ones, as it's called
			// once all are already created, so the cross dependency problem no longer prevents linking.
			// The script is deleted after it's executed so it's empty when we start appending link commands for the next executable.
			FileItem FixDylibDepsScript = FileItem.GetItemByFullPath(Path.Combine(LinkEnvironment.Config.LocalShadowDirectory, "FixDylibDependencies.sh"));
			FileItem RemoteFixDylibDepsScript = LocalToRemoteFileItem(FixDylibDepsScript, true);

			LinkAction.CommandArguments = "-c 'chmod +x \"" + RemoteFixDylibDepsScript.AbsolutePath + "\"; \"" + RemoteFixDylibDepsScript.AbsolutePath + "\"; if [[ $? -ne 0 ]]; then exit 1; fi; ";

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			// Make sure this action is executed after all the dylibs and the main executable are created

			foreach (FileItem Dependency in BundleDependencies)
			{
				LinkAction.PrerequisiteItems.Add(Dependency);
			}

			BundleDependencies.Clear();

			LinkAction.StatusDescription = string.Format("Fixing dylib dependencies for {0}", Path.GetFileName(Executable.AbsolutePath));
			LinkAction.bCanExecuteRemotely = false;

			FileItem OutputFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.Config.LocalShadowDirectory, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

			LinkAction.CommandArguments += "echo \"Dummy\" >> \"" + RemoteOutputFile.AbsolutePath + "\"";
			LinkAction.CommandArguments += "'";

			LinkAction.ProducedItems.Add(RemoteOutputFile);

			return RemoteOutputFile;
		}

		private static Dictionary<Action, string> DebugOutputMap = new Dictionary<Action, string>();
		static public void RPCDebugInfoActionHandler(Action Action, out int ExitCode, out string Output)
		{
			RPCUtilHelper.RPCActionHandler (Action, out ExitCode, out Output);
			if (DebugOutputMap.ContainsKey (Action)) 
			{
				if (ExitCode == 0) 
				{
					RPCUtilHelper.CopyDirectory (Action.ProducedItems[0].AbsolutePath, DebugOutputMap[Action], RPCUtilHelper.ECopyOptions.None);
				}
				DebugOutputMap.Remove (Action);
			}
		}

		/**
		 * Generates debug info for a given executable
		 * 
		 * @param MachOBinary FileItem describing the executable or dylib to generate debug info for
		 */
		public FileItem GenerateDebugInfo(FileItem MachOBinary)
		{
			// Make a file item for the source and destination files
			string FullDestPath = MachOBinary.AbsolutePath + ".dSYM";

			FileItem OutputFile = FileItem.GetItemByPath(FullDestPath);
			FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);
			FileItem InputFile = LocalToRemoteFileItem(MachOBinary, false);

			// Make the compile action
			Action GenDebugAction = new Action(ActionType.GenerateDebugInfo);
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				DebugOutputMap.Add (GenDebugAction, OutputFile.AbsolutePath);
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(MacToolChain.RPCDebugInfoActionHandler);
			}
			GenDebugAction.WorkingDirectory = Path.GetFullPath(".");
			GenDebugAction.CommandPath = "sh";

			// note that the source and dest are switched from a copy command
			GenDebugAction.CommandArguments = string.Format("-c '\"{0}\"usr/bin/xcrun dsymutil \"{1}\" -o \"{2}\"; \"{0}\"usr/bin/xcrun strip -S -X -x \"{1}\"'",
				XcodeDeveloperDir,
				InputFile.AbsolutePath,
				DestFile.AbsolutePath);
			GenDebugAction.PrerequisiteItems.Add(InputFile);
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/**
		 * Creates app bundle for a given executable
		 *
		 * @param Executable FileItem describing the executable to generate app bundle for
		 */
		FileItem FinalizeAppBundle(LinkEnvironment LinkEnvironment, FileItem Executable, FileItem FixDylibOutputFile)
		{
			// Make a file item for the source and destination files
			string FullDestPath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			FileItem DestFile = FileItem.GetItemByPath(FullDestPath);
			FileItem RemoteDestFile = LocalToRemoteFileItem(DestFile, false);

			// Make the compile action
			Action FinalizeAppBundleAction = new Action(ActionType.CreateAppBundle);
			FinalizeAppBundleAction.WorkingDirectory = Path.GetFullPath(".");
			FinalizeAppBundleAction.CommandPath = "/bin/sh";
			FinalizeAppBundleAction.CommandDescription = "";

			// make path to the script
			FileItem BundleScript = FileItem.GetItemByFullPath(Path.Combine(LinkEnvironment.Config.IntermediateDirectory, "FinalizeAppBundle.sh"));
			FileItem RemoteBundleScript = LocalToRemoteFileItem(BundleScript, true);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				FinalizeAppBundleAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			FinalizeAppBundleAction.CommandArguments = "\"" + RemoteBundleScript.AbsolutePath + "\"";
			FinalizeAppBundleAction.PrerequisiteItems.Add(FixDylibOutputFile);
			FinalizeAppBundleAction.ProducedItems.Add(RemoteDestFile);
			FinalizeAppBundleAction.StatusDescription = string.Format("Finalizing app bundle: {0}.app", Path.GetFileName(Executable.AbsolutePath));
			FinalizeAppBundleAction.bCanExecuteRemotely = false;

			return RemoteDestFile;
		}

		FileItem CopyBundleResource(UEBuildBundleResource Resource, FileItem Executable)
		{
			Action CopyAction = new Action(ActionType.CreateAppBundle);
			CopyAction.WorkingDirectory = Path.GetFullPath(".");
			CopyAction.CommandPath = "/bin/sh";
			CopyAction.CommandDescription = "";

			string BundlePath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			string SourcePath = Path.Combine(CopyAction.WorkingDirectory, Resource.ResourcePath);
			string TargetPath = Path.Combine(BundlePath, "Contents", Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath));

			FileItem TargetItem;
			if(BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				TargetItem = FileItem.GetItemByPath(TargetPath);
			}
			else
			{
				TargetItem = FileItem.GetRemoteItemByPath(TargetPath, RemoteToolChainPlatform);
			}

			CopyAction.CommandArguments = string.Format("-c 'cp -f -R \"{0}\" \"{1}\"; touch -c \"{2}\"'", ConvertPath(SourcePath), Path.GetDirectoryName(TargetPath).Replace('\\', '/') + "/", TargetPath.Replace('\\', '/'));
			CopyAction.PrerequisiteItems.Add(Executable);
			CopyAction.ProducedItems.Add(TargetItem);
			CopyAction.bShouldOutputStatusDescription = Resource.bShouldLog;
			CopyAction.StatusDescription = string.Format("Copying {0} to app bundle", Path.GetFileName(Resource.ResourcePath));
			CopyAction.bCanExecuteRemotely = false;

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				CopyAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			if (Directory.Exists(Resource.ResourcePath))
			{
				foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
				{
					QueueFileForBatchUpload(FileItem.GetItemByFullPath(Path.GetFullPath(ResourceFile)));
				}
			}
			else
			{
				QueueFileForBatchUpload(FileItem.GetItemByFullPath(SourcePath));
			}

			return TargetItem;
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			base.SetupBundleDependencies(Binaries, GameName);

			foreach (UEBuildBinary Binary in Binaries)
			{
				BundleDependencies.Add(FileItem.GetItemByPath(Binary.ToString()));
			}
		}
			
		public override void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries)
		{
			base.FixBundleBinariesPaths(Target, Binaries);

			string BundleContentsPath = Target.OutputPath + ".app/Contents/";
			foreach (UEBuildBinary Binary in Binaries)
			{
				string BinaryFileName = Path.GetFileName(Binary.Config.OutputFilePath);
				if (BinaryFileName.EndsWith(".dylib"))
				{
					// Only dylibs from the same folder as the executable should be moved to the bundle. UE4Editor-*Game* dylibs and plugins will be loaded
					// from their Binaries/Mac folders.
					string DylibDir = Path.GetDirectoryName(Path.GetFullPath(Binary.Config.OutputFilePath));
					string ExeDir = Path.GetDirectoryName(Path.GetFullPath(Target.OutputPath));
					if (DylibDir.StartsWith(ExeDir))
					{
						// get the subdir, which is the DylibDir - ExeDir
						string SubDir = DylibDir.Replace(ExeDir, "");
						Binary.Config.OutputFilePaths[0] = BundleContentsPath + "MacOS" + SubDir + "/" + BinaryFileName;
					}
				}
				else if (!BinaryFileName.EndsWith(".a") && !Binary.Config.OutputFilePath.Contains(".app/Contents/MacOS/")) // Binaries can contain duplicates
				{
					Binary.Config.OutputFilePaths[0] += ".app/Contents/MacOS/" + BinaryFileName;
				}
			}
		}

		static private string BundleContentsDirectory = "";

        public override void AddFilesToReceipt(BuildReceipt Receipt, UEBuildBinary Binary)
		{
			string DebugExtension = UEBuildPlatform.GetBuildPlatform(Binary.Target.Platform).GetDebugInfoExtension(Binary.Config.Type);
			if(DebugExtension == ".dsym")
			{
				for (int i = 0; i < Receipt.BuildProducts.Count; i++)
				{
					if(Receipt.BuildProducts[i].Type == BuildProductType.Executable || Receipt.BuildProducts[i].Type == BuildProductType.DynamicLibrary)
					{
						string OutputFilePath = Receipt.BuildProducts[i].Path;
						string DsymInfo = OutputFilePath + ".dSYM/Contents/Info.plist";
						Receipt.AddBuildProduct(DsymInfo, BuildProductType.SymbolFile);

						string DsymDylib = OutputFilePath + ".dSYM/Contents/Resources/DWARF/" + Path.GetFileName(OutputFilePath);
						Receipt.AddBuildProduct(DsymDylib, BuildProductType.SymbolFile);
					}
				}

				for (int i = 0; i < Receipt.BuildProducts.Count; i++)
				{
					if(Path.GetExtension(Receipt.BuildProducts[i].Path) == DebugExtension)
					{
						Receipt.BuildProducts.RemoveAt(i--);
					}
				}
			}

			if (Binary.Target.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				return;
			}

			if (BundleContentsDirectory == "" && Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				BundleContentsDirectory = Path.GetDirectoryName(Path.GetDirectoryName(Binary.Config.OutputFilePath)) + "/";
			}

			// We need to know what third party dylibs would be copied to the bundle
			if(Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				var Modules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
				var BinaryLinkEnvironment = Binary.Target.GlobalLinkEnvironment.DeepCopy();
				var BinaryDependencies = new List<UEBuildBinary>();
				var LinkEnvironmentVisitedModules = new Dictionary<UEBuildModule, bool>();
				foreach (var Module in Modules)
				{
					Module.SetupPrivateLinkEnvironment(Binary, BinaryLinkEnvironment, BinaryDependencies, LinkEnvironmentVisitedModules);
				}

				foreach (string AdditionalLibrary in BinaryLinkEnvironment.Config.AdditionalLibraries)
				{
					string LibName = Path.GetFileName(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						if (Path.GetExtension(AdditionalLibrary) == ".dylib" && !String.IsNullOrEmpty(BundleContentsDirectory))
						{
							string Entry = BundleContentsDirectory + "MacOS/" + LibName;
							Receipt.AddBuildProduct(Entry, BuildProductType.DynamicLibrary);
						}
					}
				}

				foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.Config.AdditionalBundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
						{
							Receipt.AddBuildProduct(Path.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, ResourceFile.Substring(Path.GetDirectoryName(Resource.ResourcePath).Length + 1)), BuildProductType.RequiredResource);
						}
					}
					else
					{
						Receipt.AddBuildProduct(Path.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath)), BuildProductType.RequiredResource);
					}
				}
			}

			if (Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				// And we also need all the resources
				Receipt.AddBuildProduct(BundleContentsDirectory + "Info.plist", BuildProductType.RequiredResource);
				Receipt.AddBuildProduct(BundleContentsDirectory + "PkgInfo", BuildProductType.RequiredResource);
				Receipt.AddBuildProduct(BundleContentsDirectory + "Resources/UE4.icns", BuildProductType.RequiredResource);

				if (Binary.Target.AppName.StartsWith("UE4Editor"))
				{
					Receipt.AddBuildProduct(BundleContentsDirectory + "Resources/UProject.icns", BuildProductType.RequiredResource);
				}
			}
		}

		// @todo Mac: Full implementation.
		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, string ProjectFileName, string DestinationFile)
		{
			string ProjectDirectory = Path.GetDirectoryName(Path.GetFullPath(ProjectFileName));

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				RPCUtilHelper.CopyFile(ProjectFileName, ConvertPath(ProjectFileName), true);
				RPCUtilHelper.CopyFile("Engine/Source/Programs/DotNETCommon/MetaData.cs", ConvertPath("Engine/Source/Programs/DotNETCommon/MetaData.cs"), true);

				string[] FileList = Directory.GetFiles(ProjectDirectory, "*.cs", SearchOption.AllDirectories);
				foreach (string File in FileList)
				{
					RPCUtilHelper.CopyFile(File, ConvertPath(File), true);
				}
			}

			string XBuildArgs = "/verbosity:quiet /nologo /target:Rebuild /property:Configuration=Development /property:Platform=AnyCPU " + Path.GetFileName(ProjectFileName);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				RPCUtilHelper.Command(ConvertPath(ProjectDirectory), "xbuild", XBuildArgs, null);
			}
			else
			{
				Process XBuildProcess = new Process();
				XBuildProcess.StartInfo.WorkingDirectory = ProjectDirectory;
				XBuildProcess.StartInfo.FileName = "sh";
				XBuildProcess.StartInfo.Arguments = "-c 'xbuild " + XBuildArgs + " |grep -i error; if [ $? -ne 1 ]; then exit 1; else exit 0; fi'";
				XBuildProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
				XBuildProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
				Utils.RunLocalProcess(XBuildProcess);
			}
		}

		public override void PreBuildSync()
		{
			BuiltBinaries = new List<string>();

			base.PreBuildSync();
		}

		public override void PostBuildSync(UEBuildTarget Target)
		{
			foreach (UEBuildBinary Binary in Target.AppBinaries)
			{
				BuiltBinaries.Add(Path.GetFullPath(Binary.ToString()));
			}

			base.PostBuildSync(Target);
		}

		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment)
		{
			var OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment);

			if (BinaryLinkEnvironment.Config.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.Config.AdditionalBundleResources)
			{
				OutputFiles.Add(CopyBundleResource(Resource, Executable));
			}

			// If building for Mac on a Mac, use actions to finalize the builds (otherwise, we use Deploy)
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return OutputFiles;
			}

			if (BinaryLinkEnvironment.Config.bIsBuildingDLL || BinaryLinkEnvironment.Config.bIsBuildingLibrary)
			{
				return OutputFiles;
			}

			FileItem FixDylibOutputFile = FixDylibDependencies(BinaryLinkEnvironment, Executable);
			OutputFiles.Add(FixDylibOutputFile);
			if (!BinaryLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				OutputFiles.Add(FinalizeAppBundle(BinaryLinkEnvironment, Executable, FixDylibOutputFile));
			}

			return OutputFiles;
		}

		public override UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.Mac;
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			File.Copy(SourceFileName, TargetFileName, true);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(XcodeDeveloperDir, "usr/bin/xcrun");
			StartInfo.Arguments = String.Format("strip \"{0}\" -S", TargetFileName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	};
}
