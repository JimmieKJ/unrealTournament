// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using Ionic.Zip;
using Ionic.Zlib;

namespace UnrealBuildTool
{
	class IOSToolChain : AppleToolChain
	{
		IOSPlatformContext PlatformContext;

		public IOSToolChain(FileReference InProjectFile, IOSPlatformContext InPlatformContext)
			: base(CPPTargetPlatform.IOS, UnrealTargetPlatform.Mac, InProjectFile)
		{
			PlatformContext = InPlatformContext;
		}

		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************/

		// If you are looking for where to change the remote compile server name, look in RemoteToolChain.cs

		/// <summary>
		/// If this is set, then we don't do any post-compile steps except moving the executable into the proper spot on the Mac
		/// </summary>
		[XmlConfig]
		public static bool bUseDangerouslyFastMode = false;

		/// <summary>
		/// Which version of the iOS SDK to target at build time
		/// </summary>
		[XmlConfig]
		public static string IOSSDKVersion = "latest";
		public static float IOSSDKVersionFloat = 0.0f;

		/// <summary>
		/// Which version of the iOS to allow at build time
		/// </summary>
		[XmlConfig]
		public static string BuildIOSVersion = "7.0";

		/// <summary>
		/// Which developer directory to root from
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
		private static string BaseSDKDirSim;

		/// <summary>
		/// Which compiler frontend to use
		/// </summary>
		private static string IOSCompiler = "clang++";

		/// <summary>
		/// Which linker frontend to use
		/// </summary>
		private static string IOSLinker = "clang++";

		/// <summary>
		/// Which library archiver to use
		/// </summary>
		private static string IOSArchiver = "libtool";

		public List<FileReference> BuiltBinaries = new List<FileReference>();

		/// <summary>
		/// Additional frameworks stored locally so we have access without LinkEnvironment
		/// </summary>
		public List<UEBuildFramework> RememberedAdditionalFrameworks = new List<UEBuildFramework>();

		private static void SetupXcodePaths(bool bVerbose)
		{
			// choose the XCode to use
			SelectXcode(ref XcodeDeveloperDir, bVerbose);

			// update cached paths
			BaseSDKDir = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs";
			BaseSDKDirSim = XcodeDeveloperDir + "Platforms/iPhoneSimulator.platform/Developer/SDKs";
			ToolchainDir = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/";

			// make sure SDK is selected
			SelectSDK(BaseSDKDir, "iPhoneOS", ref IOSSDKVersion, bVerbose);

			// convert to float for easy comparison
			IOSSDKVersionFloat = float.Parse(IOSSDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}

		/// <summary>
		/// Hunt down the latest IOS sdk if desired
		/// </summary>
		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			SetupXcodePaths(true);
		}

		public override void ModifyBuildProducts(UEBuildBinary Binary, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (BuildConfiguration.bCreateStubIPA && Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				FileReference StubFile = FileReference.Combine(Binary.Config.OutputFilePath.Directory, Binary.Config.OutputFilePath.GetFileNameWithoutExtension() + ".stub");
				BuildProducts.Add(StubFile, BuildProductType.Executable);
			}
		}

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="DebugExtension">Extension for the matching debug file (may be null).</param>
		public override bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return OutputType == BuildProductType.Executable;
		}

		static bool bHasPrinted = false;
		string GetArchitectureArgument(CPPTargetConfiguration Configuration, string UBTArchitecture)
		{
			PlatformContext.SetUpProjectEnvironment();

			// get the list of architectures to compile
			string Archs =
				UBTArchitecture == "-simulator" ? "i386" :
				(Configuration == CPPTargetConfiguration.Shipping) ? PlatformContext.ShippingArchitectures : PlatformContext.NonShippingArchitectures;

			if (!bHasPrinted)
			{
				bHasPrinted = true;
				Console.WriteLine("Compiling with these architectures: " + Archs);
			}

			// parse the string
			string[] Tokens = Archs.Split(",".ToCharArray());

			string Result = "";
			foreach (string Token in Tokens)
			{
				Result += " -arch " + Token;
			}

			return Result;
		}

		string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			PlatformContext.SetUpProjectEnvironment();

			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";

			Result += " -fno-exceptions";
			Result += " -fno-rtti";
			Result += " -fvisibility=hidden"; // hides the linker warnings with PhysX

			// 			if (CompileEnvironment.Config.TargetConfiguration == CPPTargetConfiguration.Shipping)
			// 			{
			// 				Result += " -flto";
			// 			}

			Result += " -Wall -Werror";

			if (CompileEnvironment.Config.bEnableShadowVariableWarning)
			{
				Result += " -Wshadow" + (BuildConfiguration.bShadowVariableErrors ? "" : " -Wno-error=shadow");
			}

			Result += " -Wno-unused-variable";
			Result += " -Wno-unused-value";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			//This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			if (IOSSDKVersionFloat >= 9.0)
			{
				Result += " -Wno-inconsistent-missing-override"; // too many missing overrides...
				Result += " -Wno-unused-local-typedef"; // PhysX has some, hard to remove
			}

			Result += " -c";

			// What architecture(s) to build for
			Result += GetArchitectureArgument(CompileEnvironment.Config.Target.Configuration, CompileEnvironment.Config.Target.Architecture);

			if (CompileEnvironment.Config.Target.Architecture == "-simulator")
			{
				Result += " -isysroot " + BaseSDKDirSim + "/iPhoneSimulator" + IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += " -isysroot " + BaseSDKDir + "/iPhoneOS" + IOSSDKVersion + ".sdk";
			}

			Result += " -miphoneos-version-min=" + PlatformContext.GetRunTimeVersion();

			Result += " " + PlatformContext.GetAdditionalLinkerFlags(CompileEnvironment.Config.Target.Configuration);

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

			// Add additional frameworks so that their headers can be found
			foreach (UEBuildFramework Framework in CompileEnvironment.Config.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					Result += " -F\"" + GetRemoteIntermediateFrameworkZipPath(Framework) + "\"";
				}
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fno-rtti";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
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
			Result += " -fno-rtti";
			Result += " -std=c++11";
			Result += " -stdlib=libc++";
			return Result;
		}

		string GetLocalFrameworkZipPath(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetLocalFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			return Path.GetFullPath(Framework.OwningModule.ModuleDirectory + "/" + Framework.FrameworkZipPath);
		}

		string GetRemoteFrameworkZipPath(UEBuildFramework Framework)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return ConvertPath(GetLocalFrameworkZipPath(Framework));
			}

			return GetLocalFrameworkZipPath(Framework);
		}

		string GetRemoteIntermediateFrameworkZipPath(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetRemoteIntermediateFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			string IntermediatePath = Framework.OwningModule.Target.ProjectDirectory + "/Intermediate/UnzippedFrameworks/" + Framework.OwningModule.Name;
			IntermediatePath = Path.GetFullPath((IntermediatePath + Framework.FrameworkZipPath).Replace(".zip", ""));

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return ConvertPath(IntermediatePath);
			}

			return IntermediatePath;
		}

		void CleanIntermediateDirectory(string Path)
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				// Delete the intermediate directory on the mac
				RPCUtilHelper.Command("/", String.Format("rm -rf \"{0}\"", Path), "", null);

				// Create a fresh intermediate after we delete it
				RPCUtilHelper.Command("/", String.Format("mkdir -p \"{0}\"", Path), "", null);
			}
			else
			{
				// Delete the local dest directory if it exists
				if (Directory.Exists(Path))
				{
					Directory.Delete(Path, true);
				}

				// Create the intermediate local directory
				string ResultsText;
				RunExecutableAndWait("mkdir", String.Format("-p \"{0}\"", Path), out ResultsText);
			}
		}

		string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			PlatformContext.SetUpProjectEnvironment();

			string Result = "";
			if (LinkEnvironment.Config.Target.Architecture == "-simulator")
			{
				Result += " -arch i386";
				Result += " -isysroot " + XcodeDeveloperDir + "Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator" + IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += Result += GetArchitectureArgument(LinkEnvironment.Config.Target.Configuration, LinkEnvironment.Config.Target.Architecture);
				Result += " -isysroot " + XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS" + IOSSDKVersion + ".sdk";
			}
			Result += " -dead_strip";
			Result += " -miphoneos-version-min=" + PlatformContext.GetRunTimeVersion();
			Result += " -Wl,-no_pie";
			Result += " -stdlib=libc++";
			//			Result += " -v";

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Config.Frameworks)
			{
				Result += " -framework " + Framework;
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.Config.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					// If this framework has a zip specified, we'll need to setup the path as well
					Result += " -F\"" + GetRemoteIntermediateFrameworkZipPath(Framework) + "\"";
				}

				Result += " -framework " + Framework.FrameworkName;
			}
			foreach (string Framework in LinkEnvironment.Config.WeakFrameworks)
			{
				Result += " -weak_framework " + Framework;
			}

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
			string Arguments = GetCompileArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				PCHArguments += string.Format(" -include \"{0}\"", ConvertPath(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PrecompiledFileExtension, "")));
			}

			// Add include paths to the argument list.
			HashSet<string> AllIncludes = new HashSet<string>(CompileEnvironment.Config.CPPIncludeInfo.IncludePaths);
			AllIncludes.UnionWith(CompileEnvironment.Config.CPPIncludeInfo.SystemIncludePaths);
			foreach (string IncludePath in AllIncludes)
			{
				Arguments += string.Format(" -I\"{0}\"", ConvertPath(Path.GetFullPath(IncludePath)));

				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
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
			}

			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
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
					FileArguments += PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(Target, BuildPlatform, CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PrecompiledFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PrecompiledFileExtension
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
					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS).GetBinaryExtension(UEBuildBinaryType.Object);
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

				string CompilerPath = ToolchainDir + IOSCompiler;
				if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				}

				// RPC utility parameters are in terms of the Mac side
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.bShouldOutputStatusDescription = true;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			string LinkerPath = ToolchainDir +
				(LinkEnvironment.Config.bIsBuildingLibrary ? IOSArchiver : IOSLinker);

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);

			if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = LinkerPath;

			// build this up over the rest of the function
			LinkAction.CommandArguments = LinkEnvironment.Config.bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment);

			if (!LinkEnvironment.Config.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
				{
					LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
				}

				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
				{
					// for absolute library paths, convert to remote filename
					if (!String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
					{
						// add it to the prerequisites to make sure it's built first (this should be the case of non-system libraries)
						FileItem LibFile = FileItem.GetItemByPath(AdditionalLibrary);
						FileItem RemoteLibFile = LocalToRemoteFileItem(LibFile, true);
						LinkAction.PrerequisiteItems.Add(RemoteLibFile);

						// and add to the commandline
						LinkAction.CommandArguments += string.Format(" \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));
					}
					else
					{
						LinkAction.CommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
					}
				}
			}

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
			}

			// Handle additional framework assets that might need to be shadowed
			foreach (UEBuildFramework Framework in LinkEnvironment.Config.AdditionalFrameworks)
			{
				if (Framework.OwningModule == null || Framework.FrameworkZipPath == null || Framework.FrameworkZipPath == "")
				{
					continue;	// Only care about frameworks that have a zip specified
				}

				// If we've already remembered this framework, skip
				if (RememberedAdditionalFrameworks.Contains(Framework))
				{
					continue;
				}

				// Remember any files we need to unzip
				RememberedAdditionalFrameworks.Add(Framework);

				// Copy them to remote mac if needed
				if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath(GetLocalFrameworkZipPath(Framework));

					if (ShadowFile != null)
					{
						QueueFileForBatchUpload(ShadowFile);
						LinkAction.PrerequisiteItems.Add(ShadowFile);
					}
					else
					{
						throw new BuildException("Couldn't find required additional file to shadow: {0}", Framework.FrameworkZipPath);
					}
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.Config.OutputFilePath);
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);
			LinkAction.ProducedItems.Add(RemoteOutputFile);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			if (LinkEnvironment.Config.bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkAction.CommandArguments += " " + Filename;
				}
				// @todo rocket lib: the -filelist command should take a response file (see else condition), except that it just says it can't
				// find the file that's in there. Rocket.lib may overflow the commandline by putting all files on the commandline, so this 
				// may be needed:
				// LinkAction.CommandArguments += string.Format(" -filelist \"{0}\"", ConvertPath(ResponsePath));
			}
			else
			{
				bool bIsUE4Game = LinkEnvironment.Config.OutputFilePath.FullName.Contains("UE4Game");
				FileReference ResponsePath = FileReference.Combine((!bIsUE4Game && ProjectFile != null) ? ProjectFile.Directory : UnrealBuildTool.EngineDirectory, BuildConfiguration.PlatformIntermediateFolder, "LinkFileList_" + LinkEnvironment.Config.OutputFilePath.GetFileNameWithoutExtension() + ".tmp");
				if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
				{
					ResponseFile.Create(ResponsePath, InputFileNames);
					RPCUtilHelper.CopyFile(ResponsePath.FullName, ConvertPath(ResponsePath.FullName), true);
				}
				else
				{
					ResponseFile.Create(new FileReference(ConvertPath(ResponsePath.FullName)), InputFileNames);
				}
				LinkAction.CommandArguments += string.Format(" @\"{0}\"", ConvertPath(ResponsePath.FullName));
			}

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = string.Format("{0}", OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			// For iPhone, generate the dSYM file if the config file is set to do so
			if (BuildConfiguration.bGeneratedSYMFile == true && Path.GetExtension(OutputFile.AbsolutePath) != ".a")
			{
				Log.TraceInformation("Generating the dSYM file - this will add some time to your build...");
				RemoteOutputFile = GenerateDebugInfo(RemoteOutputFile);
			}

			return RemoteOutputFile;
		}

		/// <summary>
		/// Generates debug info for a given executable
		/// </summary>
		/// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		public FileItem GenerateDebugInfo(FileItem Executable)
		{
			// Make a file item for the source and destination files
			string FullDestPathRoot = Executable.AbsolutePath + ".dSYM";
			string FullDestPath = FullDestPathRoot;

			FileItem DestFile;
			if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				DestFile = FileItem.GetRemoteItemByPath(FullDestPath, UnrealTargetPlatform.IOS);
			}
			else
			{
				DestFile = FileItem.GetItemByPath(FullDestPath);
			}

			// Make the compile action
			Action GenDebugAction = new Action(ActionType.GenerateDebugInfo);
			if (!Utils.IsRunningOnMono)
			{
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";

			// note that the source and dest are switched from a copy command
			if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				GenDebugAction.CommandArguments = string.Format("-c '/usr/bin/dsymutil \"{0}\" -f -o \"{1}\"; cd \"{1}/..\"; zip -r -y -1 {2}.dSYM.zip {2}.dSYM'",
					Executable.AbsolutePath,
					FullDestPathRoot,
					Path.GetFileName(Executable.AbsolutePath));
			}
			else
			{
				GenDebugAction.CommandArguments = string.Format("-c '/usr/bin/dsymutil \"{0}\" -f -o \"{1}\"'",
					Executable.AbsolutePath,
					FullDestPathRoot);
			}

			GenDebugAction.PrerequisiteItems.Add(Executable);
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		private void PackageStub(string BinaryPath, string GameName, string ExeName)
		{
			// create the ipa
			string IPAName = BinaryPath + "/" + ExeName + ".stub";
			// delete the old one
			if (File.Exists(IPAName))
			{
				File.Delete(IPAName);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(IPAName);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// set up the directories
			string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
			string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BinaryPath, GameName);

			// create the file
			using (ZipFile Zip = new ZipFile())
			{
				// add the entire directory
				Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

				// Update permissions to be UNIX-style
				// Modify the file attributes of any added file to unix format
				foreach (ZipEntry E in Zip.Entries)
				{
					const byte FileAttributePlatform_NTFS = 0x0A;
					const byte FileAttributePlatform_UNIX = 0x03;
					const byte FileAttributePlatform_FAT = 0x00;

					const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
					//const int UNIX_FILETYPE_SOCKET = 0xC000;
					//const int UNIX_FILETYPE_SYMLINK = 0xA000;
					//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
					const int UNIX_FILETYPE_DIRECTORY = 0x4000;
					//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
					//const int UNIX_FILETYPE_FIFO = 0x1000;

					const int UNIX_EXEC = 1;
					const int UNIX_WRITE = 2;
					const int UNIX_READ = 4;


					int MyPermissions = UNIX_READ | UNIX_WRITE;
					int OtherPermissions = UNIX_READ;

					int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
					int LowerBits = 0;

					// Try to preserve read-only if it was set
					bool bIsDirectory = E.IsDirectory;

					// Check to see if this 
					bool bIsExecutable = false;
					if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
					{
						bIsExecutable = true;
					}

					if (bIsExecutable)
					{
						// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
						// uncompressed gives a better indicator of IPA size for our distro builds
						E.CompressionLevel = CompressionLevel.None;
					}

					if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
					{
						FileAttributes OldAttributes = E.Attributes;
						//LowerBits = ((int)E.Attributes) & 0xFFFF;

						if ((OldAttributes & FileAttributes.Directory) != 0)
						{
							bIsDirectory = true;
						}

						// Permissions
						if ((OldAttributes & FileAttributes.ReadOnly) != 0)
						{
							MyPermissions &= ~UNIX_WRITE;
							OtherPermissions &= ~UNIX_WRITE;
						}
					}

					if (bIsDirectory || bIsExecutable)
					{
						MyPermissions |= UNIX_EXEC;
						OtherPermissions |= UNIX_EXEC;
					}

					// Re-jigger the external file attributes to UNIX style if they're not already that way
					if (PlatformEncodedBy != FileAttributePlatform_UNIX)
					{
						int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

						NewAttributes |= (MyPermissions << 6);
						NewAttributes |= (OtherPermissions << 3);
						NewAttributes |= (OtherPermissions << 0);

						// Now modify the properties
						E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
					}
				}

				// Save it out
				Zip.Save(IPAName);
			}
		}

		public override void PreBuildSync()
		{
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				BuiltBinaries = new List<FileReference>();
			}

			base.PreBuildSync();

			// Unzip any third party frameworks that are stored as zips
			foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
			{
				string ZipSrcPath = GetRemoteFrameworkZipPath(Framework);
				string ZipDstPath = GetRemoteIntermediateFrameworkZipPath(Framework);

				Log.TraceInformation("Unzipping: {0} -> {1}", ZipSrcPath, ZipDstPath);

				CleanIntermediateDirectory(ZipDstPath);

				// Assume that there is another directory inside the zip with the same name as the zip
				ZipDstPath = ZipDstPath.Substring(0, ZipDstPath.LastIndexOf('/'));

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					// If we're on the mac, just unzip using the shell
					string ResultsText;
					RunExecutableAndWait("unzip", String.Format("-o \"{0}\" -d \"{1}\"", ZipSrcPath, ZipDstPath), out ResultsText);
					continue;
				}
				else
				{
					// Use RPC utility if the zip is on remote mac
					Hashtable Result = RPCUtilHelper.Command("/", String.Format("unzip -o \"{0}\" -d \"{1}\"", ZipSrcPath, ZipDstPath), "", null);

					foreach (DictionaryEntry Entry in Result)
					{
						Log.TraceInformation("{0}", Entry.Value);
					}
				}
			}
		}

		public override void PostBuildSync(UEBuildTarget Target)
		{
			base.PostBuildSync(Target);

			string AppName = Target.TargetType == TargetRules.TargetType.Game ? Target.TargetName : Target.AppName;

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				string RemoteShadowDirectoryMac = Path.GetDirectoryName(Target.OutputPath.FullName);
				string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, AppName);

				// strip the debug info from the executable if needed
				if (BuildConfiguration.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
				{
					Process StripProcess = new Process();
					StripProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
					StripProcess.StartInfo.FileName = ToolchainDir + "strip";
					StripProcess.StartInfo.Arguments = "\"" + Target.OutputPath + "\"";
					StripProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StripProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					Utils.RunLocalProcess(StripProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}
				}

				// copy the executable
				if (!File.Exists(FinalRemoteExecutablePath))
				{
					Directory.CreateDirectory(String.Format("{0}/Payload/{1}.app", RemoteShadowDirectoryMac, AppName));
				}
				File.Copy(Target.OutputPath.FullName, FinalRemoteExecutablePath, true);

				if (BuildConfiguration.bCreateStubIPA)
				{
					string Project = Target.ProjectDirectory + "/" + AppName + ".uproject";

					// generate the dummy project so signing works
					if (AppName == "UE4Game" || AppName == "UE4Client" || Utils.IsFileUnderDirectory(Target.ProjectDirectory + "/" + AppName + ".uproject", Path.GetFullPath("../..")))
					{
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-platforms=IOS", "-NoIntellIsense", "-iosdeployonly", "-ignorejunk" });
						Project = Path.GetFullPath("../..") + "/UE4_IOS.xcodeproj";
					}
					else
					{
						Project = Target.ProjectDirectory + "/" + AppName + ".xcodeproj";
					}

					if (Directory.Exists(Project))
					{
						// ensure the plist, entitlements, and provision files are properly copied
						var DeployHandler = new UEDeployIOS();
						DeployHandler.PrepTargetForDeployment(Target);

						// code sign the project
						string CmdLine = XcodeDeveloperDir + "usr/bin/xcodebuild" +
										" -project \"" + Project + "\"" +
										" -configuration " + Target.Configuration +
										" -scheme '" + AppName + " - iOS'" +
										" -sdk iphoneos" +
										" CODE_SIGN_IDENTITY=\"iPhone Developer\"";

						Console.WriteLine("Code signing with command line: " + CmdLine);

						Process SignProcess = new Process();
						SignProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
						SignProcess.StartInfo.FileName = "/usr/bin/xcrun";
						SignProcess.StartInfo.Arguments = CmdLine;
						SignProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
						SignProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

						OutputReceivedDataEventHandlerEncounteredError = false;
						OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
						Utils.RunLocalProcess(SignProcess);

						// delete the temp project
						if (Project.Contains("_IOS.xcodeproj"))
						{
							Directory.Delete(Project, true);
						}

						if (OutputReceivedDataEventHandlerEncounteredError)
						{
							throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
						}

						// Package the stub
						PackageStub(RemoteShadowDirectoryMac, AppName, Target.OutputPath.GetFileNameWithoutExtension());
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/IOS/FrameworkAssets");

					// Clean the local dest directory if it exists
					CleanIntermediateDirectory(LocalFrameworkAssets);

					foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
					{
						if (Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "")
						{
							continue;		// Only care if we need to copy bundle assets
						}

						if (Framework.OwningModule.Target != Target)
						{
							continue;		// This framework item doesn't belong to this target, skip it
						}

						string UnpackedZipPath = GetRemoteIntermediateFrameworkZipPath(Framework);

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string LocalSource = UnpackedZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);
						string LocalDest = LocalFrameworkAssets + "/" + BundleName;

						Log.TraceInformation("Copying bundled asset... LocalSource: {0}, LocalDest: {1}", LocalSource, LocalDest);

						string ResultsText;
						RunExecutableAndWait("cp", String.Format("-R -L {0} {1}", LocalSource.Replace(" ", "\\ "), LocalDest.Replace(" ", "\\ ")), out ResultsText);
					}
				}
			}
			else
			{
				// store off the binaries
				foreach (UEBuildBinary Binary in Target.AppBinaries)
				{
					BuiltBinaries.Add(Binary.Config.OutputFilePath);
				}

				// check to see if the DangerouslyFast mode is valid (in other words, a build has gone through since a Rebuild/Clean operation)
				FileReference DangerouslyFastValidFile = FileReference.Combine(Target.GlobalLinkEnvironment.Config.IntermediateDirectory, "DangerouslyFastIsNotDangerous");
				bool bUseDangerouslyFastModeWasRequested = bUseDangerouslyFastMode;
				if (bUseDangerouslyFastMode)
				{
					if (!DangerouslyFastValidFile.Exists())
					{
						Log.TraceInformation("Dangeroulsy Fast mode was requested, but a slow mode hasn't completed. Performing slow now...");
						bUseDangerouslyFastMode = false;
					}
				}

				foreach (FileReference FilePath in BuiltBinaries)
				{
					string RemoteExecutablePath = ConvertPath(FilePath.FullName);

					// when going super fast, just copy the executable to the final resting spot
					if (bUseDangerouslyFastMode)
					{
						Log.TraceInformation("==============================================================================");
						Log.TraceInformation("USING DANGEROUSLY FAST MODE! IF YOU HAVE ANY PROBLEMS, TRY A REBUILD/CLEAN!!!!");
						Log.TraceInformation("==============================================================================");

						// copy the executable
						string RemoteShadowDirectoryMac = ConvertPath(Path.GetDirectoryName(Target.OutputPath.FullName));
						string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, Target.TargetName);
						RPCUtilHelper.Command("/", String.Format("cp -f {0} {1}", RemoteExecutablePath, FinalRemoteExecutablePath), "", null);
					}
					else if (!Utils.IsRunningOnMono && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
					{
						RPCUtilHelper.CopyFile(RemoteExecutablePath, FilePath.FullName, false);

						if (BuildConfiguration.bGeneratedSYMFile == true)
						{
							string DSYMExt = ".app.dSYM.zip";
							RPCUtilHelper.CopyFile(RemoteExecutablePath + DSYMExt, FilePath.FullName + DSYMExt, false);
						}
					}
				}

				// Generate the stub
				if (BuildConfiguration.bCreateStubIPA || bUseDangerouslyFastMode)
				{
					// ensure the plist, entitlements, and provision files are properly copied
					var DeployHandler = new UEDeployIOS();
					DeployHandler.PrepTargetForDeployment(Target);

					if (!bUseDangerouslyFastMode)
					{
						// generate the dummy project so signing works
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-NoIntellisense", "-iosdeployonly", ((Target.ProjectFile != null) ? "-game" : "") });
					}

					// now that 
					Process StubGenerateProcess = new Process();
					StubGenerateProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\Binaries\\DotNET\\IOS");
					StubGenerateProcess.StartInfo.FileName = Path.Combine(StubGenerateProcess.StartInfo.WorkingDirectory, "iPhonePackager.exe");

					string Arguments = "";
					string PathToApp = Target.RulesAssembly.GetTargetFileName(AppName).FullName;

					// right now, no programs have a Source subdirectory, so assume the PathToApp is directly in the root
					if (Path.GetDirectoryName(PathToApp).Contains(@"\Engine\Source\Programs"))
					{
						PathToApp = Path.GetDirectoryName(PathToApp);
					}
					else
					{
						Int32 SourceIndex = PathToApp.LastIndexOf(@"\Intermediate\Source");
						if (SourceIndex != -1)
						{
							PathToApp = PathToApp.Substring(0, SourceIndex);
						}
						else
						{
							SourceIndex = PathToApp.LastIndexOf(@"\Source");
							if (SourceIndex != -1)
							{
								PathToApp = PathToApp.Substring(0, SourceIndex);
							}
							else
							{
								throw new BuildException("The target was not in a /Source subdirectory");
							}
						}
						if (AppName != "UE4Game")
						{
							PathToApp += "\\" + AppName + ".uproject";
						}
					}

					if (bUseDangerouslyFastMode)
					{
						// the quickness!
						Arguments += "DangerouslyFast " + PathToApp;
					}
					else
					{
						Arguments += "PackageIPA \"" + PathToApp + "\" -createstub";
						// if we are making the dsym, then we can strip the debug info from the executable
						if (BuildConfiguration.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
						{
							Arguments += " -strip";
						}
					}
					Arguments += " -config " + Target.Configuration + " -mac " + RemoteServerName;

					string Architecture = PlatformContext.GetActiveArchitecture();
					if (Architecture != "")
					{
						// pass along the architecture if we need, skipping the initial -, so we have "-architecture simulator"
						Arguments += " -architecture " + Architecture.Substring(1);
					}

					if (!bUseRPCUtil)
					{
						Arguments += " -usessh";
						Arguments += " -sshexe \"" + ResolvedSSHExe + "\"";
						Arguments += " -sshauth \"" + ResolvedSSHAuthentication + "\"";
						Arguments += " -sshuser \"" + ResolvedRSyncUsername + "\"";
						Arguments += " -rsyncexe \"" + ResolvedRSyncExe + "\"";
						Arguments += " -overridedevroot \"" + UserDevRootMac + "\"";
					}

					// programmers that use Xcode packaging mode should use the following commandline instead, as it will package for Xcode on each compile
					//				Arguments = "PackageApp " + GameName + " " + Configuration;
					StubGenerateProcess.StartInfo.Arguments = Arguments;

					StubGenerateProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StubGenerateProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					int ExitCode = Utils.RunLocalProcess(StubGenerateProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						UnrealBuildTool.ExtendedErrorCode = ExitCode;
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}

					// now that a slow mode sync has finished, we can now do DangerouslyFast mode again (if requested)
					if (bUseDangerouslyFastModeWasRequested)
					{
						File.Create(DangerouslyFastValidFile.FullName);
					}
					else
					{
						// if we didn't want dangerously fast, then delete the file so that setting/unsetting the flag will do the right thing without a Rebuild
						File.Delete(DangerouslyFastValidFile.FullName);
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/IOS/FrameworkAssets");
					String RemoteFrameworkAssets = ConvertPath(LocalFrameworkAssets);

					CleanIntermediateDirectory(RemoteFrameworkAssets);

					// Delete the local dest directory if it exists
					if (Directory.Exists(LocalFrameworkAssets))
					{
						Directory.Delete(LocalFrameworkAssets, true);
					}

					foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
					{
						if (Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "")
						{
							continue;		// Only care if we need to copy bundle assets
						}

						if (Framework.OwningModule.Target != Target)
						{
							continue;		// This framework item doesn't belong to this target, skip it
						}

						string RemoteZipPath = GetRemoteIntermediateFrameworkZipPath(Framework);

						RemoteZipPath = RemoteZipPath.Replace(".zip", "");

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string RemoteSource = RemoteZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);

						String RemoteDest = RemoteFrameworkAssets + "/" + BundleName;
						String LocalDest = LocalFrameworkAssets + "\\" + BundleName;

						Log.TraceInformation("Copying bundled asset... RemoteSource: {0}, RemoteDest: {1}, LocalDest: {2}", RemoteSource, RemoteDest, LocalDest);

						Hashtable Results = RPCUtilHelper.Command("/", String.Format("cp -R -L {0} {1}", RemoteSource.Replace(" ", "\\ "), RemoteDest.Replace(" ", "\\ ")), "", null);

						foreach (DictionaryEntry Entry in Results)
						{
							Log.TraceInformation("{0}", Entry.Value);
						}

						// Copy the bundled resource from the remote mac to the local dest
						RPCUtilHelper.CopyDirectory(RemoteDest, LocalDest, RPCUtilHelper.ECopyOptions.None);
					}
				}

				// If it is requested, send the app bundle back to the platform executing these commands.
				if (BuildConfiguration.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					try
					{
						string BinaryDir = Path.GetDirectoryName(Target.OutputPath.FullName) + "\\";
						if (BinaryDir.EndsWith(Target.AppName + "\\Binaries\\IOS\\") && Target.TargetType != TargetRules.TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(Target.TargetType.ToString(), "Game");
						}

						// Get the app bundle's name
						string AppFullName = Target.AppName;
						if (Target.Configuration != UnrealTargetConfiguration.Development)
						{
							AppFullName += "-" + Target.Platform.ToString();
							AppFullName += "-" + Target.Configuration.ToString();
						}

						foreach (string BinaryPath in BuiltBinaries.Select(x => x.FullName))
						{
							if (!BinaryPath.Contains("Dummy"))
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
		}

		public static int RunExecutableAndWait(string ExeName, string ArgumentList, out string StdOutResults)
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo(ExeName, ArgumentList);
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start(PSI);

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}

		public override void StripSymbols(string SourceFileName, string TargetFileName)
		{
			SetupXcodePaths(false);

			StripSymbolsWithXcode(SourceFileName, TargetFileName, ToolchainDir);
		}
	};
}
