// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Text;
using System.Linq;
using Ionic.Zip;

namespace UnrealBuildTool
{
	public class MacToolChain : AppleToolChain
	{
		public MacToolChain(FileReference InProjectFile) 
			: base(CPPTargetPlatform.Mac, UnrealTargetPlatform.Mac, InProjectFile)
		{
		}

		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************/

		/// <summary>
		/// Which version of the Mac OS SDK to target at build time
		/// </summary>
		public static string MacOSSDKVersion = "latest";
		public static float MacOSSDKVersionFloat = 0.0f;

		/// <summary>
		/// Which version of the Mac OS X to allow at run time
		/// </summary>
		public static string MacOSVersion = "10.9";

		/// <summary>
		/// Minimum version of Mac OS X to actually run on, running on earlier versions will display the system minimum version error dialog & exit.
		/// </summary>
		public static string MinMacOSVersion = "10.10.5";

		/// <summary>
		/// Which developer directory to root from? If this is "xcode-select", UBT will query for the currently selected Xcode
		/// </summary>
		private static string XcodeDeveloperDir = "xcode-select";

		/// <summary>
		/// Directory for the developer binaries
		/// </summary>
		private static string ToolchainDir = "";

		/// <summary>
		/// Location of the SDKs
		/// </summary>
		private static string BaseSDKDir;

		/// <summary>
		/// Which compiler frontend to use
		/// </summary>
		private static string MacCompiler = "clang++";

		/// <summary>
		/// Which linker frontend to use
		/// </summary>
		private static string MacLinker = "clang++";

		/// <summary>
		/// Which archiver to use
		/// </summary>
		private static string MacArchiver = "libtool";

		/// <summary>
		/// Track which scripts need to be deleted before appending to
		/// </summary>
		private bool bHasWipedCopyDylibScript = false;
		private bool bHasWipedFixDylibScript = false;

		private static List<FileItem> BundleDependencies = new List<FileItem>();

		private static void SetupXcodePaths(bool bVerbose)
		{
			SelectXcode(ref XcodeDeveloperDir, bVerbose);

			BaseSDKDir = XcodeDeveloperDir + "Platforms/MacOSX.platform/Developer/SDKs";
			ToolchainDir = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/";

			SelectSDK(BaseSDKDir, "MacOSX", ref MacOSSDKVersion, bVerbose);

			// convert to float for easy comparison
			MacOSSDKVersionFloat = float.Parse(MacOSSDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}

		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			if (!UEBuildConfiguration.bListBuildFolders)
			{
				SetupXcodePaths(true);
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

            string SanitizerMode = Environment.GetEnvironmentVariable("CLANG_ADDRESS_SANITIZER");
            if(SanitizerMode != null && SanitizerMode == "YES")
            {
                Result += " -fsanitize=address";
            }

			Result += " -Wall -Werror";
			//Result += " -Wsign-compare"; // fed up of not seeing the signed/unsigned warnings we get on Windows - lets enable them here too.

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
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			if (MacOSSDKVersionFloat < 10.9f && MacOSSDKVersionFloat >= 10.11f)
			{
				Result += " -Wno-inconsistent-missing-override"; // too many missing overrides...
				Result += " -Wno-unused-local-typedef"; // PhysX has some, hard to remove
			}

			if (CompileEnvironment.Config.bEnableShadowVariableWarning)
			{
				Result += " -Wshadow" + (BuildConfiguration.bShadowVariableErrors ? "" : " -Wno-error=shadow");
			}

			// @todo: Remove these two when the code is fixed and they're no longer needed
			Result += " -Wno-logical-op-parentheses";
			Result += " -Wno-unknown-pragmas";

			Result += " -c";

			Result += " -arch x86_64";
			Result += " -isysroot " + BaseSDKDir + "/MacOSX" + MacOSSDKVersion + ".sdk";
			Result += " -mmacosx-version-min=" + (CompileEnvironment.Config.bEnableOSX109Support ? "10.9" : MacOSVersion);

			// Optimize non- debug builds.
            // Don't optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
			if (CompileEnvironment.Config.Target.Configuration != CPPTargetConfiguration.Debug && (SanitizerMode == null || SanitizerMode != "YES"))
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

			string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if(StaticAnalysisMode != null && StaticAnalysisMode != "")
			{
				Result += " --analyze";
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

            string SanitizerMode = Environment.GetEnvironmentVariable("CLANG_ADDRESS_SANITIZER");
            if(SanitizerMode != null && SanitizerMode == "YES")
            {
                Result += " -fsanitize=address";
            }

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
				var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
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
				AddPrerequisiteSourceFile(Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledHeaderExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
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
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.Mac).GetBinaryExtension(UEBuildBinaryType.Object);
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
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
				CompileAction.CommandPath = ToolchainDir + MacCompiler;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
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
			BuildVersion Version;
			if (BuildVersion.TryRead(out Version))
			{
				return Version.Changelist;
			}
			else
			{
				return 0;
			}
		}

		public static string LoadEngineDisplayVersion(bool bIgnorePatchVersion = false)
		{
			BuildVersion Version;
			if (BuildVersion.TryRead(out Version))
			{
				return String.Format("{0}.{1}.{2}", Version.MajorVersion, Version.MinorVersion, bIgnorePatchVersion? 0 : Version.PatchVersion);
			}
			else
			{
				return "4.0.0";
			}
		}

		private string LoadLauncherDisplayVersion()
		{
			string[] VersionHeader = Utils.ReadAllText("../../Portal/Source/Layers/DataAccess/Public/PortalVersion.h").Replace("\r\n", "\n").Replace("\t", " ").Split('\n');
			string LauncherVersionMajor = "1";
			string LauncherVersionMinor = "0";
			string LauncherVersionPatch = "0";
			foreach (string Line in VersionHeader)
			{
				if (Line.StartsWith("#define PORTAL_MAJOR_VERSION "))
				{
					LauncherVersionMajor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define PORTAL_MINOR_VERSION "))
				{
					LauncherVersionMinor = Line.Split(' ')[2];
				}
				else if (Line.StartsWith("#define PORTAL_PATCH_VERSION "))
				{
					LauncherVersionPatch = Line.Split(' ')[2];
				}
			}
			return LauncherVersionMajor + "." + LauncherVersionMinor + "." + LauncherVersionPatch;
		}

		private int LoadBuiltFromChangelistValue()
		{
			return LoadEngineCL();
		}

		private int LoadIsLicenseeVersionValue()
		{
			BuildVersion Version;
			if (BuildVersion.TryRead(out Version))
			{
				return Version.IsLicenseeVersion;
			}
			else
			{
				return 0;
			}
		}

		private string LoadEngineAPIVersion()
		{
			int CL = 0;

			BuildVersion Version;
			if (BuildVersion.TryRead(out Version))
			{
				CL = (Version.CompatibleChangelist != 0)? Version.CompatibleChangelist : Version.Changelist;
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
					// For CEF3 for the Shipping Launcher we only want the RPATH to the framework inside the app bundle, otherwise OS X gatekeeper erroneously complains about not seeing framework. 
					if (!ExeAbsolutePath.Contains("EpicGamesLauncher-Mac-Shipping") || !Library.Contains("CEF3"))
					{
					RPaths.Add(RelativePath);
					LinkCommand += " -rpath \"@loader_path/" + RelativePath + "\"";
					}

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
			string LinkCommand = ToolchainDir + Linker + VersionArg + " " + (bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment));

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = !Utils.IsRunningOnMono && (bBuildImportLibraryOnly || LinkEnvironment.Config.bIsBuildingDLL);

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
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
					if (FrameworkPath.EndsWith(".framework"))
					{
						foreach (string FrameworkFile in Directory.EnumerateFiles(FrameworkPath, "*", SearchOption.AllDirectories))
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

					if (Path.GetDirectoryName(AdditionalLibrary) != "" &&
							 (Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries/Mac") ||
							 Path.GetDirectoryName(AdditionalLibrary).Contains("Binaries\\Mac")))
					{
						// It's an engine or game dylib. Save it for later
						EngineAndGameLibraries.Add(ConvertPath(Path.GetFullPath(AdditionalLibrary)));

						if (!Utils.IsRunningOnMono)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(AdditionalLibrary);
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
							FileItem RemoteEngineLibDependency = FileItem.GetRemoteItemByPath(ConvertPath(Path.GetFullPath(AdditionalLibrary)), UnrealTargetPlatform.Mac);
							LinkAction.PrerequisiteItems.Add(RemoteEngineLibDependency);
							//Log.TraceInformation("Adding {0} / {1} as a prereq to {2}", EngineLibDependency.AbsolutePath, RemoteEngineLibDependency.AbsolutePath, RemoteOutputFile.AbsolutePath);
						}
						else if (LinkEnvironment.Config.bIsCrossReferenced == false)
						{
							FileItem EngineLibDependency = FileItem.GetItemByPath(AdditionalLibrary);
							LinkAction.PrerequisiteItems.Add(EngineLibDependency);
						}
					}
					else if (AdditionalLibrary.Contains(".framework/"))
					{
						LinkCommand += string.Format(" \'{0}\'", AdditionalLibrary);
					}
					else  if (string.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)) && string.IsNullOrEmpty(Path.GetExtension(AdditionalLibrary)))
					{
						LinkCommand += string.Format(" -l{0}", AdditionalLibrary);
					}
					else
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
				FileReference ResponsePath = FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, Path.GetFileName(OutputFile.AbsolutePath) + ".response");

				// Never create response files when we are only generating IntelliSense data
				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
					ResponseFile.Create(ResponsePath, InputFileNames);

					if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
					{
						RPCUtilHelper.CopyFile(ResponsePath.FullName, ConvertPath(ResponsePath.FullName), true);
					}
				}

				LinkCommand += string.Format(" @\"{0}\"", ConvertPath(ResponsePath.FullName));
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
				if (UnrealBuildTool.IsEngineInstalled() || (Utils.IsRunningOnMono && LinkEnvironment.Config.bIsCrossReferenced == false))
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
					LinkCommand += "; " + ToolchainDir + "install_name_tool -change " + LibraryFileName + " " + DylibsPath + "/" + LibraryFileName + " \"" + ConvertPath(OutputFile.AbsolutePath) + "\"";
				}
			}

			string ReadyFilePath = LinkEnvironment.Config.IntermediateDirectory + "/" + LinkEnvironment.Config.OutputFilePath.GetFileName() + ".ready";
			FileItem DylibReadyOutputFile = FileItem.GetItemByPath(ReadyFilePath);
			FileItem RemoteDylibReadyOutputFile = LocalToRemoteFileItem(DylibReadyOutputFile, false);

			LinkAction.CommandArguments = "-c '" + LinkCommand + "; echo \"-\" >> \"" + RemoteDylibReadyOutputFile.AbsolutePath + "\"'";

			// Only execute linking on the local Mac.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);

			LinkAction.ProducedItems.Add(RemoteOutputFile);
			LinkAction.ProducedItems.Add(RemoteDylibReadyOutputFile);

			if (!LinkEnvironment.Config.IntermediateDirectory.Exists())
			{
				return OutputFile;
			}

			if (!bIsBuildingLibrary)
			{
				// Prepare a script that will run later, once every dylibs and the executable are created. This script will be called by action created in FixDylibDependencies()
				FileReference FixDylibDepsScriptPath = FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, "FixDylibDependencies.sh");
				if (!bHasWipedFixDylibScript)
				{
					if (FixDylibDepsScriptPath.Exists())
					{
						FixDylibDepsScriptPath.Delete();
					}
					bHasWipedFixDylibScript = true;
				}

				if (!LinkEnvironment.Config.LocalShadowDirectory.Exists())
				{
					LinkEnvironment.Config.LocalShadowDirectory.CreateDirectory();
				}

				StreamWriter FixDylibDepsScript = File.AppendText(FixDylibDepsScriptPath.FullName);

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
				FileReference DylibCopyScriptPath = FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, "DylibCopy.sh");
				if (!bHasWipedCopyDylibScript)
				{
					if (DylibCopyScriptPath.Exists())
					{
						DylibCopyScriptPath.Delete();
					}
					bHasWipedCopyDylibScript = true;
				}
				string ExistingScript = DylibCopyScriptPath.Exists() ? File.ReadAllText(DylibCopyScriptPath.FullName) : "";
				StreamWriter DylibCopyScript = File.AppendText(DylibCopyScriptPath.FullName);
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
					FileReference FinalizeAppBundleScriptPath = FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, "FinalizeAppBundle.sh");
					StreamWriter FinalizeAppBundleScript = File.CreateText(FinalizeAppBundleScriptPath.FullName);
					AppendMacLine(FinalizeAppBundleScript, "#!/bin/sh");
					string BinariesPath = Path.GetDirectoryName(OutputFile.AbsolutePath);
					BinariesPath = Path.GetDirectoryName(BinariesPath.Substring(0, BinariesPath.IndexOf(".app")));
					AppendMacLine(FinalizeAppBundleScript, "cd \"{0}\"", ConvertPath(BinariesPath).Replace("$", "\\$"));

					string ExeName = Path.GetFileName(OutputFile.AbsolutePath);
					bool bIsLauncherProduct = ExeName.StartsWith("EpicGamesLauncher") || ExeName.StartsWith("EpicGamesBootstrapLauncher");
					string[] ExeNameParts = ExeName.Split('-');
					string GameName = ExeNameParts[0];
					FileReference UProjectFilePath = null;

					if (GameName == "EpicGamesBootstrapLauncher")
					{
						GameName = "EpicGamesLauncher";
					}
					else if (GameName == "UE4" && ProjectFile != null)
					{
						UProjectFilePath = ProjectFile;
						GameName = UProjectFilePath.GetFileNameWithoutAnyExtensions();
					}

					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/MacOS\"", ExeName);
					AppendMacLine(FinalizeAppBundleScript, "mkdir -p \"{0}.app/Contents/Resources\"", ExeName);

					// Copy third party dylibs by calling additional script prepared earlier
					AppendMacLine(FinalizeAppBundleScript, "sh \"{0}\" \"{1}\"", ConvertPath(DylibCopyScriptPath.FullName).Replace("$", "\\$"), ExeName);

					string IconName = "UE4";
					string BundleVersion = bIsLauncherProduct ? LoadLauncherDisplayVersion() : LoadEngineDisplayVersion();
					string EngineSourcePath = ConvertPath(Directory.GetCurrentDirectory()).Replace("$", "\\$");
					string CustomResourcesPath = "";
					string CustomBuildPath = "";
					if (UProjectFilePath == null && !UProjectInfo.TryGetProjectFileName(GameName, out UProjectFilePath))
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
						string ResourceParentFolderName = bIsLauncherProduct ? "Application" : GameName;
						CustomResourcesPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Source/" + ResourceParentFolderName + "/Resources/Mac";
						CustomBuildPath = Path.GetDirectoryName(UProjectFilePath.FullName) + "/Build/Mac";
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
							QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(CustomIcon)));
							CustomIcon = ConvertPath(CustomIcon);
						}
					}
					AppendMacLine(FinalizeAppBundleScript, "cp -f \"{0}\" \"{2}.app/Contents/Resources/{1}.icns\"", CustomIcon, GameName, ExeName);

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
						QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(InfoPlistFile)));
						InfoPlistFile = ConvertPath(InfoPlistFile);
					}
					AppendMacLine(FinalizeAppBundleScript, "cp -f \"{0}\" \"{1}.app/Contents/Info.plist\"", InfoPlistFile, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "chmod 644 \"{0}.app/Contents/Info.plist\"", ExeName);

					// Fix contents of Info.plist
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{1}.app/Contents/Info.plist\"", "{EXECUTABLE_NAME}", ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{APP_NAME}", GameName, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{MACOSX_DEPLOYMENT_TARGET}", MinMacOSVersion, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{ICON_NAME}", GameName, ExeName);
					AppendMacLine(FinalizeAppBundleScript, "sed -i \"\" \"s/\\${0}/{1}/g\" \"{2}.app/Contents/Info.plist\"", "{BUNDLE_VERSION}", BundleVersion, ExeName);

					// Generate PkgInfo file
					AppendMacLine(FinalizeAppBundleScript, "echo 'echo -n \"APPL????\"' | bash > \"{0}.app/Contents/PkgInfo\"", ExeName);

					// Make sure OS X knows the bundle was updated
					AppendMacLine(FinalizeAppBundleScript, "touch -c \"{0}.app\"", ExeName);

					FinalizeAppBundleScript.Close();

					// copy over some needed files
					// @todo mac: Make a QueueDirectoryForBatchUpload
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/" + GameName + ".icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/UProject.icns")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference("../../Engine/Source/Runtime/Launch/Resources/Mac/Info.plist")));
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, "DylibCopy.sh")));
				}
			}

			return RemoteOutputFile;
		}

		FileItem FixDylibDependencies(LinkEnvironment LinkEnvironment, FileItem Executable)
		{
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = "/bin/sh";
			LinkAction.CommandDescription = "";

			// Call the FixDylibDependencies.sh script which will link the dylibs and the main executable, this time proper ones, as it's called
			// once all are already created, so the cross dependency problem no longer prevents linking.
			// The script is deleted after it's executed so it's empty when we start appending link commands for the next executable.
			FileItem FixDylibDepsScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, "FixDylibDependencies.sh"));
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

			FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.LocalShadowDirectory, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);

			LinkAction.CommandArguments += "echo \"Dummy\" >> \"" + RemoteOutputFile.AbsolutePath + "\"";
			LinkAction.CommandArguments += "'";

			LinkAction.ProducedItems.Add(RemoteOutputFile);

			return RemoteOutputFile;
		}

		private static Dictionary<Action, string> DebugOutputMap = new Dictionary<Action, string>();
		static public void RPCDebugInfoActionHandler(Action Action, out int ExitCode, out string Output)
		{
			RPCUtilHelper.RPCActionHandler(Action, out ExitCode, out Output);
			if (DebugOutputMap.ContainsKey(Action))
			{
				if (ExitCode == 0)
				{
					RPCUtilHelper.CopyDirectory(Action.ProducedItems[0].AbsolutePath, DebugOutputMap[Action], RPCUtilHelper.ECopyOptions.None);
				}
				DebugOutputMap.Remove(Action);
			}
		}

		/// <summary>
		/// Generates debug info for a given executable
		/// </summary>
		/// <param name="MachOBinary">FileItem describing the executable or dylib to generate debug info for</param>
		public FileItem GenerateDebugInfo(FileItem MachOBinary, LinkEnvironment LinkEnvironment)
		{
			string BinaryPath = MachOBinary.AbsolutePath;
			if (BinaryPath.Contains(".app"))
			{
				while (BinaryPath.Contains(".app"))
				{
					BinaryPath = Path.GetDirectoryName(BinaryPath);
				}
				BinaryPath = Path.Combine(BinaryPath, Path.GetFileName(Path.ChangeExtension(MachOBinary.AbsolutePath, ".dSYM")));
			}
			else
			{
				BinaryPath = Path.ChangeExtension(BinaryPath, ".dSYM");
			}

			string ReadyFilePath = LinkEnvironment.Config.IntermediateDirectory + "/" + Path.GetFileName(MachOBinary.AbsolutePath) + ".ready";
			FileItem ReadyFile = FileItem.GetItemByPath(ReadyFilePath);

			FileItem OutputFile = FileItem.GetItemByPath(BinaryPath);
			FileItem DestFile = LocalToRemoteFileItem(OutputFile, false);
			FileItem InputFile = LocalToRemoteFileItem(MachOBinary, false);

			// Delete on the local machine
			if (Directory.Exists(OutputFile.AbsolutePath))
			{
				Directory.Delete(OutputFile.AbsolutePath, true);
			}

			// Make the compile action
			Action GenDebugAction = new Action(ActionType.GenerateDebugInfo);
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				DebugOutputMap.Add(GenDebugAction, OutputFile.AbsolutePath);
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(MacToolChain.RPCDebugInfoActionHandler);
			}
			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";

			// Deletes ay existing file on the building machine,
			// note that the source and dest are switched from a copy command
			GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{2}\"; \"{0}\"dsymutil -f \"{1}\" -o \"{2}\"'",
				ToolchainDir,
				InputFile.AbsolutePath,
				DestFile.AbsolutePath);
			GenDebugAction.PrerequisiteItems.Add(LocalToRemoteFileItem(ReadyFile, false));
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.CommandDescription = "";
			GenDebugAction.StatusDescription = "Generating " + Path.GetFileName(BinaryPath);
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/// <summary>
		/// Creates app bundle for a given executable
		/// </summary>
		/// <param name="Executable">FileItem describing the executable to generate app bundle for</param>
		FileItem FinalizeAppBundle(LinkEnvironment LinkEnvironment, FileItem Executable, FileItem FixDylibOutputFile)
		{
			// Make a file item for the source and destination files
			string FullDestPath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			FileItem DestFile = FileItem.GetItemByPath(FullDestPath);
			FileItem RemoteDestFile = LocalToRemoteFileItem(DestFile, false);

			// Make the compile action
			Action FinalizeAppBundleAction = new Action(ActionType.CreateAppBundle);
			FinalizeAppBundleAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			FinalizeAppBundleAction.CommandPath = "/bin/sh";
			FinalizeAppBundleAction.CommandDescription = "";

			// make path to the script
			FileItem BundleScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.Config.IntermediateDirectory, "FinalizeAppBundle.sh"));
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
			CopyAction.WorkingDirectory = GetMacDevSrcRoot(); // Path.GetFullPath(".");
			CopyAction.CommandPath = "/bin/sh";
			CopyAction.CommandDescription = "";

			string BundlePath = Executable.AbsolutePath.Substring(0, Executable.AbsolutePath.IndexOf(".app") + 4);
			string SourcePath = Path.Combine(Path.GetFullPath("."), Resource.ResourcePath);
			string TargetPath = Path.Combine(BundlePath, "Contents", Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath));

			FileItem TargetItem;
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
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
					QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(ResourceFile)));
				}
			}
			else
			{
				QueueFileForBatchUpload(FileItem.GetItemByFileReference(new FileReference(SourcePath)));
			}

			return TargetItem;
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			base.SetupBundleDependencies(Binaries, GameName);

			foreach (UEBuildBinary Binary in Binaries)
			{
				BundleDependencies.Add(FileItem.GetItemByFileReference(Binary.Config.OutputFilePath));
			}
		}

		public override void FixBundleBinariesPaths(UEBuildTarget Target, List<UEBuildBinary> Binaries)
		{
			base.FixBundleBinariesPaths(Target, Binaries);

			string BundleContentsPath = Target.OutputPath.FullName + ".app/Contents/";
			foreach (UEBuildBinary Binary in Binaries)
			{
				string BinaryFileName = Path.GetFileName(Binary.Config.OutputFilePath.FullName);
				if (BinaryFileName.EndsWith(".dylib"))
				{
					// Only dylibs from the same folder as the executable should be moved to the bundle. UE4Editor-*Game* dylibs and plugins will be loaded
					// from their Binaries/Mac folders.
					string DylibDir = Path.GetDirectoryName(Path.GetFullPath(Binary.Config.OutputFilePath.FullName));
					string ExeDir = Path.GetDirectoryName(Path.GetFullPath(Target.OutputPath.FullName));
					if (DylibDir.StartsWith(ExeDir))
					{
						// get the subdir, which is the DylibDir - ExeDir
						string SubDir = DylibDir.Replace(ExeDir, "");
						Binary.Config.OutputFilePaths[0] = new FileReference(BundleContentsPath + "MacOS" + SubDir + "/" + BinaryFileName);
					}
				}
				else if (!BinaryFileName.EndsWith(".a") && !Binary.Config.OutputFilePath.FullName.Contains(".app/Contents/MacOS/")) // Binaries can contain duplicates
				{
					Binary.Config.OutputFilePaths[0] += ".app/Contents/MacOS/" + BinaryFileName;
				}
			}
		}

		static private DirectoryReference BundleContentsDirectory;

		public override void ModifyBuildProducts(UEBuildBinary Binary, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (BuildConfiguration.bUsePDBFiles == true)
			{
				KeyValuePair<FileReference, BuildProductType>[] BuildProductsArray = BuildProducts.ToArray();

				foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProductsArray)
				{
					string DebugExtension = "";
					switch (BuildProductPair.Value)
					{
						case BuildProductType.Executable:
							DebugExtension = UEBuildPlatform.GetBuildPlatform(Binary.Target.Platform).GetDebugInfoExtension(UEBuildBinaryType.Executable);
							break;
						case BuildProductType.DynamicLibrary:
							DebugExtension = UEBuildPlatform.GetBuildPlatform(Binary.Target.Platform).GetDebugInfoExtension(UEBuildBinaryType.DynamicLinkLibrary);
							break;
					}
					if (DebugExtension == ".dSYM")
					{
						string BinaryPath = BuildProductPair.Key.FullName;
						if(BinaryPath.Contains(".app"))
						{
							while(BinaryPath.Contains(".app"))
							{
								BinaryPath = Path.GetDirectoryName(BinaryPath);
							}
							BinaryPath = Path.Combine(BinaryPath, BuildProductPair.Key.GetFileName());
							BinaryPath = Path.ChangeExtension(BinaryPath, DebugExtension);
							FileReference Ref = new FileReference(BinaryPath);
							BuildProducts.Add(Ref, BuildProductType.SymbolFile);
						}
					}
					else if(BuildProductPair.Value == BuildProductType.SymbolFile && BuildProductPair.Key.FullName.Contains(".app"))
					{
						BuildProducts.Remove(BuildProductPair.Key);
					}
				}
			}

			if (Binary.Target.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				return;
			}

			if (BundleContentsDirectory == null && Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				BundleContentsDirectory = Binary.Config.OutputFilePath.Directory.ParentDirectory;
			}

			// We need to know what third party dylibs would be copied to the bundle
			if (Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				var Modules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
				var BinaryLinkEnvironment = Binary.Target.GlobalLinkEnvironment.DeepCopy();
				var BinaryDependencies = new List<UEBuildBinary>();
				var LinkEnvironmentVisitedModules = new HashSet<UEBuildModule>();
				foreach (var Module in Modules)
				{
					Module.SetupPrivateLinkEnvironment(Binary, BinaryLinkEnvironment, BinaryDependencies, LinkEnvironmentVisitedModules);
				}

				foreach (string AdditionalLibrary in BinaryLinkEnvironment.Config.AdditionalLibraries)
				{
					string LibName = Path.GetFileName(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						if (Path.GetExtension(AdditionalLibrary) == ".dylib" && BundleContentsDirectory != null)
						{
							FileReference Entry = FileReference.Combine(BundleContentsDirectory, "MacOS", LibName);
							BuildProducts.Add(Entry, BuildProductType.DynamicLibrary);
						}
					}
				}

				foreach (UEBuildBundleResource Resource in BinaryLinkEnvironment.Config.AdditionalBundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						foreach (string ResourceFile in Directory.GetFiles(Resource.ResourcePath, "*", SearchOption.AllDirectories))
						{
							BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, ResourceFile.Substring(Path.GetDirectoryName(Resource.ResourcePath).Length + 1)), BuildProductType.RequiredResource);
						}
					}
					else
					{
						BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, Resource.BundleContentsSubdir, Path.GetFileName(Resource.ResourcePath)), BuildProductType.RequiredResource);
					}
				}
			}

			if (Binary.Config.Type == UEBuildBinaryType.Executable)
			{
				// And we also need all the resources
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Info.plist"), BuildProductType.RequiredResource);
				BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "PkgInfo"), BuildProductType.RequiredResource);

				if (Binary.Target.TargetType == TargetRules.TargetType.Editor)
				{
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/UE4Editor.icns"), BuildProductType.RequiredResource);
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/UProject.icns"), BuildProductType.RequiredResource);
				}
				else
				{
					string IconName = Binary.Target.TargetName;
					if (IconName == "EpicGamesBootstrapLauncher")
					{
						IconName = "EpicGamesLauncher";
					}
					BuildProducts.Add(FileReference.Combine(BundleContentsDirectory, "Resources/" + IconName + ".icns"), BuildProductType.RequiredResource);
				}
			}
		}

		// @todo Mac: Full implementation.
		public override void CompileCSharpProject(CSharpEnvironment CompileEnvironment, FileReference ProjectFileName, FileReference DestinationFile)
		{
			string ProjectDirectory = Path.GetDirectoryName(ProjectFileName.FullName);

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				RPCUtilHelper.CopyFile(ProjectFileName.FullName, ConvertPath(ProjectFileName.FullName), true);
				RPCUtilHelper.CopyFile("Engine/Source/Programs/DotNETCommon/MetaData.cs", ConvertPath("Engine/Source/Programs/DotNETCommon/MetaData.cs"), true);

				string[] FileList = Directory.GetFiles(ProjectDirectory, "*.cs", SearchOption.AllDirectories);
				foreach (string File in FileList)
				{
					RPCUtilHelper.CopyFile(File, ConvertPath(File), true);
				}
			}

			string XBuildArgs = "/verbosity:quiet /nologo /target:Rebuild /property:Configuration=Development /property:Platform=AnyCPU " + ProjectFileName.GetFileName();

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
			base.PreBuildSync();
		}

		public override void PostBuildSync(UEBuildTarget InTarget)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				List<string> BuiltBinaries = new List<string>();
				foreach (UEBuildBinary Binary in InTarget.AppBinaries)
				{
					BuiltBinaries.Add(Path.GetFullPath(Binary.ToString()));
					BuiltBinaries.Add(Path.GetFullPath(Binary.ToString() + ".ready"));

					string DebugExtension = UEBuildPlatform.GetBuildPlatform(Binary.Target.Platform).GetDebugInfoExtension(Binary.Config.Type);
					if (DebugExtension == ".dSYM")
					{
						Dictionary<FileReference, BuildProductType> BuildProducts = new Dictionary<FileReference, BuildProductType>();
						Binary.GetBuildProducts(this, BuildProducts);
						foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
						{
							if (BuildProductPair.Value == BuildProductType.SymbolFile)
							{
								BuiltBinaries.Add(BuildProductPair.Key.FullName);
							}
						}
					}
				}

				string IntermediateDirectory = InTarget.EngineIntermediateDirectory.FullName;
				if (!Directory.Exists(IntermediateDirectory))
				{
					IntermediateDirectory = Path.GetFullPath("../../" + InTarget.AppName + "/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration);
					if (!Directory.Exists(IntermediateDirectory))
					{
						IntermediateDirectory = Path.GetFullPath("../../Engine/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration);
					}
				}

				string FixDylibDepsScript = Path.Combine(IntermediateDirectory, "FixDylibDependencies.sh");
				string FinalizeAppBundleScript = Path.Combine(IntermediateDirectory, "FinalizeAppBundle.sh");

				string RemoteWorkingDir = "";

				bool bIsStaticLibrary = InTarget.OutputPath.HasExtension(".a");

				if (!bIsStaticLibrary)
				{
					// Copy the command scripts to the intermediate on the target Mac.
					string RemoteFixDylibDepsScript = ConvertPath(Path.GetFullPath(FixDylibDepsScript));
					RemoteFixDylibDepsScript = RemoteFixDylibDepsScript.Replace("../../../../", "../../");
					RPCUtilHelper.CopyFile(Path.GetFullPath(FixDylibDepsScript), RemoteFixDylibDepsScript, true);

					if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						string RemoteFinalizeAppBundleScript = ConvertPath(Path.GetFullPath(FinalizeAppBundleScript));
						RemoteFinalizeAppBundleScript = RemoteFinalizeAppBundleScript.Replace("../../../../", "../../");
						RPCUtilHelper.CopyFile(Path.GetFullPath(FinalizeAppBundleScript), RemoteFinalizeAppBundleScript, true);
					}


					// run it remotely
					RemoteWorkingDir = ConvertPath(Path.GetDirectoryName(Path.GetFullPath(FixDylibDepsScript)));

					Log.TraceInformation("Running FixDylibDependencies.sh...");
					Hashtable Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FixDylibDependencies.sh", null);
					if (Results != null)
					{
						string Result = (string)Results["CommandOutput"];
						if (Result != null)
						{
							Log.TraceInformation(Result);
						}
					}

					if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						Log.TraceInformation("Running FinalizeAppBundle.sh...");
						Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FinalizeAppBundle.sh", null);
						if (Results != null)
						{
							string Result = (string)Results["CommandOutput"];
							if (Result != null)
							{
								Log.TraceInformation(Result);
							}
						}
					}
				}


				// If it is requested, send the app bundle back to the platform executing these commands.
				if (BuildConfiguration.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					try
					{
						string BinaryDir = InTarget.OutputPath.Directory + "\\";
						if (BinaryDir.EndsWith(InTarget.AppName + "\\Binaries\\Mac\\") && InTarget.TargetType != TargetRules.TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(InTarget.TargetType.ToString(), "Game");
						}

						string RemoteBinariesDir = ConvertPath(BinaryDir);
						string LocalBinariesDir = BinaryDir;

						// Get the app bundle's name
						string AppFullName = InTarget.AppName;
						if (InTarget.Configuration != InTarget.Rules.UndecoratedConfiguration)
						{
							AppFullName += "-" + InTarget.Platform.ToString();
							AppFullName += "-" + InTarget.Configuration.ToString();
						}

						if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
						{
							AppFullName += ".app";
						}

						List<string> NotBundledBinaries = new List<string>();
						foreach (string BinaryPath in BuiltBinaries)
						{
							if (InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication || bIsStaticLibrary || !BinaryPath.StartsWith(LocalBinariesDir + AppFullName))
							{
								NotBundledBinaries.Add(BinaryPath);
							}
						}

						// Zip the app bundle for transferring.
						if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication && !bIsStaticLibrary)
						{
							string ZipCommand = "zip -0 -r -y -T \"" + AppFullName + ".zip\" \"" + AppFullName + "\"";
							RPCUtilHelper.Command(RemoteBinariesDir, ZipCommand, "", null);

							// Copy the AppBundle back to the source machine
							string LocalZipFileLocation = LocalBinariesDir + AppFullName + ".zip ";
							string RemoteZipFileLocation = RemoteBinariesDir + AppFullName + ".zip";

							RPCUtilHelper.CopyFile(RemoteZipFileLocation, LocalZipFileLocation, false);

							// Extract the copied app bundle (in zip format) to the local binaries directory
							using (ZipFile AppBundleZip = ZipFile.Read(LocalZipFileLocation))
							{
								foreach (ZipEntry Entry in AppBundleZip)
								{
									Entry.Extract(LocalBinariesDir, ExtractExistingFileAction.OverwriteSilently);
								}
							}

							// Delete the zip as we no longer need/want it.
							File.Delete(LocalZipFileLocation);
							RPCUtilHelper.Command(RemoteBinariesDir, "rm -f \"" + AppFullName + ".zip\"", "", null);
						}

						if (NotBundledBinaries.Count > 0)
						{

							foreach (string BinaryPath in NotBundledBinaries)
							{
								RPCUtilHelper.CopyFile(ConvertPath(BinaryPath), BinaryPath, false);
							}
						}

						Log.TraceInformation("Copied binaries successfully.");
					}
					catch (Exception)
					{
						Log.TraceInformation("Copying binaries back to this device failed.");
					}
				}
			}

			base.PostBuildSync(InTarget);
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

			// For Mac, generate the dSYM file if the config file is set to do so
			if (BuildConfiguration.bUsePDBFiles == true && (!BinaryLinkEnvironment.Config.bIsBuildingLibrary || BinaryLinkEnvironment.Config.bIsBuildingDLL))
			{
				// We want dsyms to be created after all dylib dependencies are fixed. If FixDylibDependencies action was not created yet, save the info for later.
				if (FixDylibOutputFile != null)
				{
					OutputFiles.Add(GenerateDebugInfo(Executable, BinaryLinkEnvironment));
				}
				else
				{
					ExecutablesThatNeedDsyms.Add(Executable);
				}
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

			FixDylibOutputFile = FixDylibDependencies(BinaryLinkEnvironment, Executable);
			OutputFiles.Add(FixDylibOutputFile);
			if (!BinaryLinkEnvironment.Config.bIsBuildingConsoleApplication)
			{
				OutputFiles.Add(FinalizeAppBundle(BinaryLinkEnvironment, Executable, FixDylibOutputFile));
			}

			// Add dsyms that we couldn't add before FixDylibDependencies action was created
			foreach (FileItem Exe in ExecutablesThatNeedDsyms)
			{
				OutputFiles.Add(GenerateDebugInfo(Exe, BinaryLinkEnvironment));
			}
			ExecutablesThatNeedDsyms.Clear();

			return OutputFiles;
		}

		private FileItem FixDylibOutputFile = null;
		private List<FileItem> ExecutablesThatNeedDsyms = new List<FileItem>();

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			SetupXcodePaths(false);

			StripSymbolsWithXcode(SourceFileName, TargetFileName, ToolchainDir);
		}
	};
}
