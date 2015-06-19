// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Xml;
using System.Diagnostics;

namespace AutomationTool
{
	[Help("ForceMonolithic", "Toggle to combined the result into one executable")]
	[Help("ForceDebugInfo", "Forces debug info even in development builds")]
	[Help("NoXGE", "Toggle to disable the distributed build process")]
	[Help("ForceNonUnity", "Toggle to disable the unity build system")]
	[Help("ForceUnity", "Toggle to force enable the unity build system")]
	[Help("Licensee", "If set, this build is being compiled by a licensee")]
	public class UE4Build : CommandUtils
	{
		private BuildCommand OwnerCommand;

		public void PrepareBuildProduct(string File, bool DeleteProduct = true)
		{
			if (!BuildProductFiles.Contains(File))
			{
				if (DeleteProduct && DeleteBuildProducts)
				{
					DeleteFile(File);
				}
			}
		}

		public bool HasBuildProduct(string InFile)
		{
			string File = CombinePaths(InFile);
			foreach (var ExistingFile in BuildProductFiles)
			{
				if (ExistingFile.Equals(File, StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		public void AddBuildProduct(string InFile)
		{
			string File = CombinePaths(InFile);
			if (!FileExists(File))
			{
				throw new AutomationException("BUILD FAILED specified file to AddBuildProduct {0} does not exist.", File);
			}
			if (!HasBuildProduct(InFile))
			{
				BuildProductFiles.Add(File);
			}
		}

		void PrepareManifest(string ManifestName, bool bAddReceipt)
		{
			if (FileExists(ManifestName) == false)
			{
				throw new AutomationException("BUILD FAILED UBT Manifest {0} does not exist.", ManifestName);
			}
			int FileNum = 0;
			string OutFile;
			while (true)
			{
				OutFile = CombinePaths(CmdEnv.LogFolder, String.Format("UBTManifest.{0}", FileNum) + Path.GetExtension(ManifestName));
				FileInfo ItemInfo = new FileInfo(OutFile);
				if (!ItemInfo.Exists)
				{
					break;
				}
				FileNum++;
			}
			CopyFile(ManifestName, OutFile);
			Log("Copied UBT manifest to {0}", OutFile);


			UnrealBuildTool.BuildManifest Manifest = ReadManifest(ManifestName);
			foreach (string Item in Manifest.BuildProducts)
			{
				if(bAddReceipt && IsBuildReceipt(Item))
				{
					AddBuildProduct(Item);
				}
				else
				{
					PrepareBuildProduct(Item);
				}
			}
		}

		static bool IsBuildReceipt(string FileName)
		{
			return Path.GetDirectoryName(FileName).Replace('\\', '/').EndsWith("/Receipts", StringComparison.InvariantCultureIgnoreCase);
		}

		void AddBuildProductsFromManifest(string ManifestName)
		{
			if (FileExists(ManifestName) == false)
			{
				throw new AutomationException("BUILD FAILED UBT Manifest {0} does not exist.", ManifestName);
			}
			UnrealBuildTool.BuildManifest Manifest = ReadManifest(ManifestName);
			foreach (string Item in Manifest.BuildProducts)
			{
				if (!FileExists_NoExceptions(Item))
				{
					throw new AutomationException("BUILD FAILED {0} was in manifest but was not produced.", Item);
				}
				AddBuildProduct(Item);
			}
		}

	
		/// True if UBT is compiled and ready to build!
		private bool bIsUBTReady = false;
		
		private void PrepareUBT()
		{			
			// Don't build UBT if we're running with pre-compiled binaries and if there's a debugger attached to this process.
			// With the debugger attached, even though deleting the exe will work, the pdb files are still locked and the build will fail.
			// Also, if we're running from VS then since UAT references UBT, we already have the most up-to-date version of UBT.exe
			if (!bIsUBTReady && !GlobalCommandLine.NoCompile && !System.Diagnostics.Debugger.IsAttached)
			{
				DeleteFile(UBTExecutable);

				MsBuild(CmdEnv,
						CmdEnv.LocalRoot + @"/Engine/Source/Programs/UnrealBuildTool/" + HostPlatform.Current.UBTProjectName + @".csproj",
						"/verbosity:normal /target:Rebuild /property:Configuration=Development /property:Platform=AnyCPU",
						"BuildUBT");

				bIsUBTReady = true;
			}

			if (FileExists(UBTExecutable) == false)
			{
				throw new AutomationException("UBT does not exist in {0}.", UBTExecutable);
			}
		}

		public class XGEItem
		{
			public UnrealBuildTool.BuildManifest Manifest;
			public string CommandLine;
			public UnrealBuildTool.UnrealTargetPlatform Platform;
			public string Config;
			public string ProjectName;
			public string UProjectPath;
			public List<string> XgeXmlFiles;
			public string OutputCaption;
		}

		XGEItem XGEPrepareBuildWithUBT(string ProjectName, string TargetName, UnrealBuildTool.UnrealTargetPlatform Platform, string Config, string UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (string.IsNullOrEmpty(UprojectPath) == false)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath);
			}
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}

			PrepareUBT();

            string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

			DeleteFile(UBTManifest);
			XGEItem Result = new XGEItem();

			ClearExportedXGEXML();

			RunUBT(CmdEnv, UBTExecutable: UBTExecutable, Project: ProjectName, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-generatemanifest -nobuilduht -xgeexport" + AddArgs, EnvVars: EnvVars);

			PrepareManifest(UBTManifest, true);

			Result.Platform = Platform;
			Result.Config = Config;
			Result.ProjectName = string.IsNullOrEmpty(ProjectName) ? TargetName : ProjectName; // use the target as the project if no project is set
			Result.UProjectPath = UprojectPath;
			Result.Manifest = ReadManifest(UBTManifest);
			Result.OutputCaption = String.Format("{0}-{1}-{2}", Path.GetFileNameWithoutExtension(Result.ProjectName), Platform.ToString(), Config.ToString());
			DeleteFile(UBTManifest);

			Result.CommandLine = UBTExecutable + " " + UBTCommandline(Project: ProjectName, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-noxge -nobuilduht" + AddArgs);

			Result.XgeXmlFiles = new List<string>();
			foreach (var XGEFile in FindXGEFiles())
			{
				if (!FileExists_NoExceptions(XGEFile))
				{
					throw new AutomationException("BUILD FAILED: Couldn't find file: {0}", XGEFile);
				}
				int FileNum = 0;
				string OutFile;
				while (true)
				{
					OutFile = CombinePaths(CmdEnv.LogFolder, String.Format("UBTExport.{0}.xge.xml", FileNum));
					FileInfo ItemInfo = new FileInfo(OutFile);
					if (!ItemInfo.Exists)
					{
						break;
					}
					FileNum++;
				}
				CopyFile(XGEFile, OutFile);
				Result.XgeXmlFiles.Add(OutFile);
			}
			ClearExportedXGEXML();
			return Result;
		}

		void XGEFinishBuildWithUBT(XGEItem Item)
		{
			// allow the platform to perform any actions after building a target (seems almost like this should be done in UBT)
			Platform.Platforms[Item.Platform].PostBuildTarget(this, Item.ProjectName, Item.UProjectPath, Item.Config);

			foreach (string ManifestItem in Item.Manifest.BuildProducts)
			{
				if (!FileExists_NoExceptions(ManifestItem))
				{
					throw new AutomationException("BUILD FAILED {0} was in manifest but was not produced.", ManifestItem);
				}
				AddBuildProduct(ManifestItem);
			}
		}

		void XGEDeleteBuildProducts(UnrealBuildTool.BuildManifest Manifest)
		{
			foreach (string Item in Manifest.BuildProducts)
			{
				DeleteFile(Item);
			}
		}

		void CleanWithUBT(string ProjectName, string TargetName, UnrealBuildTool.UnrealTargetPlatform Platform, string Config, string UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (string.IsNullOrEmpty(UprojectPath) == false)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath);
			}
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}
			if (!TargetName.Equals("UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase))
			{
				AddArgs += " -nobuilduht";
			}

			PrepareUBT();

			RunUBT(CmdEnv, UBTExecutable: UBTExecutable, Project: ProjectName, Target: TargetName, Platform: Platform.ToString(), Config: Config, AdditionalArgs: "-clean" + AddArgs, EnvVars: EnvVars);
		}

		void BuildWithUBT(string ProjectName, string TargetName, UnrealBuildTool.UnrealTargetPlatform TargetPlatform, string Config, string UprojectPath, bool ForceMonolithic = false, bool ForceNonUnity = false, bool ForceDebugInfo = false, bool ForceFlushMac = false, bool DisableXGE = false, string InAddArgs = "", bool ForceUnity = false, Dictionary<string, string> EnvVars = null)
		{
			string AddArgs = "";
			if (string.IsNullOrEmpty(UprojectPath) == false)
			{
				AddArgs += " " + CommandUtils.MakePathSafeToUseWithCommandLine(UprojectPath);
			}
			AddArgs += " " + InAddArgs;
			if (ForceMonolithic)
			{
				AddArgs += " -monolithic";
			}
			if (ForceNonUnity)
			{
				AddArgs += " -disableunity";
			}
			if (ForceUnity)
			{
				AddArgs += " -forceunity";
			}
			if (ForceDebugInfo)
			{
				AddArgs += " -forcedebuginfo";
			}
			if (ForceFlushMac)
			{
				AddArgs += " -flushmac";
			}
			if (DisableXGE)
			{
				AddArgs += " -noxge";
			}

			PrepareUBT();

			// let the platform determine when to use the manifest
            bool UseManifest = Platform.Platforms[TargetPlatform].ShouldUseManifestForUBTBuilds(AddArgs);

			if (UseManifest)
			{
                string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

				DeleteFile(UBTManifest);

				RunUBT(CmdEnv, UBTExecutable: UBTExecutable, Project: ProjectName, Target: TargetName, Platform: TargetPlatform.ToString(), Config: Config, AdditionalArgs: AddArgs +  " -generatemanifest" , EnvVars: EnvVars);

				PrepareManifest(UBTManifest, false);
			}

			RunUBT(CmdEnv, UBTExecutable: UBTExecutable, Project: ProjectName, Target: TargetName, Platform: TargetPlatform.ToString(), Config: Config, AdditionalArgs: AddArgs, EnvVars: EnvVars);

			// allow the platform to perform any actions after building a target (seems almost like this should be done in UBT)
			Platform.Platforms[TargetPlatform].PostBuildTarget(this, string.IsNullOrEmpty(ProjectName) ? TargetName : ProjectName, UprojectPath, Config);

			if (UseManifest)
			{
                string UBTManifest = GetUBTManifest(UprojectPath, AddArgs);

				AddBuildProductsFromManifest(UBTManifest);

				DeleteFile(UBTManifest);
			}
		}

		string[] DotNetProductExtenstions()
		{
			return new string[] 
			{
				".dll",
				".pdb",
				".exe.config",
				".exe",
				"exe.mdb"
			};
		}

		string[] SwarmBuildProducts()
		{
			return new string[]
            {
                "AgentInterface",
                "SwarmAgent",
                "SwarmCoordinator",
                "SwarmCoordinatorInterface",
                "SwarmInterface",
				"SwarmCommonUtils"
            };
		}

		void PrepareBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				PrepareBuildProduct(BaseOutput + Ext, false); // we don't delete these because we are not sure they are actually a build product
			}
		}

		void AddBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				if (FileExists(BaseOutput + Ext))
				{
					AddBuildProduct(BaseOutput + Ext);
				}
			}
		}

		void AddIOSBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput = CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/IOS/" + Path.GetFileNameWithoutExtension(CsProj);
			foreach (var Ext in DotNetProductExtenstions())
			{
				if (FileExists(BaseOutput + Ext))
				{
					AddBuildProduct(BaseOutput + Ext);
				}
			}
		}

		void AddSwarmBuildProducts()
		{
			foreach (var SwarmProduct in SwarmBuildProducts())
			{
				string DotNETOutput = CmdEnv.LocalRoot + @"/Engine/Binaries/DotNET/" + SwarmProduct;
				string Win64Output = CmdEnv.LocalRoot + @"/Engine/Binaries/Win64/" + SwarmProduct;
				foreach (var Ext in DotNetProductExtenstions())
				{
					if (FileExists(DotNETOutput + Ext))
					{
						AddBuildProduct(DotNETOutput + Ext);
					}
				}
				foreach (var Ext in DotNetProductExtenstions())
				{
					if (FileExists(Win64Output + Ext))
					{
						AddBuildProduct(Win64Output + Ext);
					}
				}
			}
		}

		/// <summary>
		/// Updates the engine version files
		/// </summary>
		public List<string> UpdateVersionFiles(bool ActuallyUpdateVersionFiles = true, int? ChangelistNumberOverride = null)
		{
			bool bIsLicenseeVersion = OwnerCommand.ParseParam("Licensee");
			bool bDoUpdateVersionFiles = CommandUtils.P4Enabled && ActuallyUpdateVersionFiles;		
			int ChangelistNumber = 0;
			string ChangelistString = String.Empty;
			if (bDoUpdateVersionFiles)
			{
				ChangelistNumber = ChangelistNumberOverride.GetValueOrDefault(P4Env.Changelist);
				ChangelistString = ChangelistNumber.ToString();
			}

			var Result = new List<String>();
			string Branch = P4Enabled ? P4Env.BuildRootEscaped : "";
			{
				string VerFile = CombinePaths(CmdEnv.LocalRoot, "Engine", "Build", "build.properties");
				if (bDoUpdateVersionFiles)
				{
					Log("Updating {0} with:", VerFile);
					Log("  TimestampForBVT={0}", CmdEnv.TimestampAsString);
					Log("  EngineVersion={0}", ChangelistNumber.ToString());
					Log("  BranchName={0}", Branch);					

					PrepareBuildProduct(VerFile, false);
					VersionFileUpdater BuildProperties = new VersionFileUpdater(VerFile);
					BuildProperties.ReplaceLine("TimestampForBVT=", CmdEnv.TimestampAsString);
					BuildProperties.ReplaceLine("EngineVersion=", ChangelistNumber.ToString());
					BuildProperties.ReplaceLine("BranchName=", Branch);
                    BuildProperties.Commit();
				}
				else
				{
					Log("{0} will not be updated because P4 is not enabled.", VerFile);
				}
				if (IsBuildMachine)
				{
					// Only BuildMachines can check this file in
					AddBuildProduct(VerFile);
				}
				Result.Add(VerFile);
			}

			{
				string VerFile = CombinePaths(CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Core", "Private", "UObject", "ObjectVersion.cpp");
				if (bDoUpdateVersionFiles)
				{
					Log("Updating {0} with:", VerFile);
					Log(" #define	ENGINE_VERSION  {0}", ChangelistNumber.ToString());

					VersionFileUpdater ObjectVersionCpp = new VersionFileUpdater(VerFile);
					ObjectVersionCpp.ReplaceLine("#define	ENGINE_VERSION	", ChangelistNumber.ToString());
                    ObjectVersionCpp.Commit();
                }
				else
				{
					Log("{0} will not be updated because P4 is not enabled.", VerFile);
				}
                Result.Add(VerFile);
            }

            string EngineVersionFile = CombinePaths(CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Launch", "Resources", "Version.h");
            {
                string VerFile = EngineVersionFile;
				if (bDoUpdateVersionFiles)
				{
					Log("Updating {0} with:", VerFile);
					Log(" #define	ENGINE_VERSION  {0}", ChangelistNumber.ToString());
					Log(" #define	BRANCH_NAME  {0}", Branch);
					Log(" #define	BUILT_FROM_CHANGELIST  {0}", ChangelistString);
					Log(" #define   ENGINE_IS_LICENSEE_VERSION  {0}", bIsLicenseeVersion ? "1" : "0");

					VersionFileUpdater VersionH = new VersionFileUpdater(VerFile);
					VersionH.ReplaceLine("#define ENGINE_VERSION ", ChangelistNumber.ToString());
					VersionH.ReplaceLine("#define BRANCH_NAME ", "\"" + Branch + "\"");
					VersionH.ReplaceLine("#define BUILT_FROM_CHANGELIST ", ChangelistString);
					VersionH.ReplaceOrAddLine("#define ENGINE_IS_LICENSEE_VERSION ", bIsLicenseeVersion ? "1" : "0");

                    VersionH.Commit();
                }
				else
				{
					Log("{0} will not be updated because P4 is not enabled.", VerFile);
				}
                Result.Add(VerFile);
            }

            {
                // Use Version.h data to update MetaData.cs so the assemblies match the engine version.
                string VerFile = CombinePaths(CmdEnv.LocalRoot, "Engine", "Source", "Programs", "DotNETCommon", "MetaData.cs");

				if (bDoUpdateVersionFiles)
                {
                    // Get the MAJOR/MINOR/PATCH from the Engine Version file, as it is authoritative. The rest we get from the P4Env.
                    string NewInformationalVersion = FEngineVersionSupport.FromVersionFile(EngineVersionFile, ChangelistNumber).ToString();

                    Log("Updating {0} with AssemblyInformationalVersion: {1}", VerFile, NewInformationalVersion);

                    VersionFileUpdater VersionH = new VersionFileUpdater(VerFile);
                    VersionH.SetAssemblyInformationalVersion(NewInformationalVersion);
                    VersionH.Commit();
                }
                else
                {
                    Log("{0} will not be updated because P4 is not enabled.", VerFile);
                }
                Result.Add(VerFile);
            }

			return Result;
		}

		/// <summary>
		/// Updates the public key file (decryption)
		/// </summary>
		public void UpdatePublicKey(string KeyFilename)
		{
			var Lines = ReadEncryptionKeys(KeyFilename);
			if (Lines != null && Lines.Length >= 3)
			{
				// Line0: Private key exponent, Line1: Modulus, Line2: Public key exponent.
				string Modulus = String.Format("\"{0}\"", Lines[1]);
				string PublicKeyExponent = String.Format("\"{0}\"", Lines[2]);
				string VerFile = CmdEnv.LocalRoot + @"/Engine/Source/Runtime/PakFile/Private/PublicKey.inl";

				Log("Updating {0} with:", VerFile);
				Log(" #define DECRYPTION_KEY_EXPONENT {0}", PublicKeyExponent);
				Log(" #define DECYRPTION_KEY_MODULUS {0}", Modulus);

				VersionFileUpdater PublicKeyInl = new VersionFileUpdater(VerFile);
				PublicKeyInl.ReplaceLine("#define DECRYPTION_KEY_EXPONENT ", PublicKeyExponent);
				PublicKeyInl.ReplaceLine("#define DECYRPTION_KEY_MODULUS ", Modulus);
                PublicKeyInl.Commit();
			}
			else
			{
				LogError("{0} doesn't look like a valid encryption key file or value.");
			}
		}

		/// <summary>
		/// Parses the encryption keys from the command line or loads them from a file.
		/// </summary>
		/// <param name="KeyFilenameOrValues">Key values or filename</param>
		/// <returns>List of three encruption key values: private exponent, modulus and public exponent.</returns>
		private string[] ReadEncryptionKeys(string KeyFilenameOrValues)
		{
			string[] Values;
			if (KeyFilenameOrValues.StartsWith("0x", StringComparison.InvariantCultureIgnoreCase))
			{
				Values = KeyFilenameOrValues.Split(new char[] { '+' }, StringSplitOptions.RemoveEmptyEntries);
			}
			else
			{
				Values = ReadAllLines(KeyFilenameOrValues);
			}
			return Values;
		}

		[DebuggerDisplay("{TargetName} {Platform} {Config}")]
		public class BuildTarget
		{
			/// Unreal project name, if needed
			public string ProjectName;

			/// Name of the target
			public string TargetName;

			/// For code-based projects with a .uproject, the TargetName isn't enough for UBT to find the target, this will point UBT to the target
			public string UprojectPath;

			/// Platform to build
			public UnrealBuildTool.UnrealTargetPlatform Platform;

			/// Configuration to build
			public UnrealBuildTool.UnrealTargetConfiguration Config;

			/// If true, force monolithic
			public bool ForceMonolithic;

			/// If true, force non-unity
			public bool ForceNonUnity;

			/// If true, force debug info, even in development builds (useful for distributed builds where we need symbols)
			public bool ForceDebugInfo;

			/// Extra UBT args
			public string UBTArgs;
		}


		public class BuildAgenda
		{
			/// Full .NET solution files that we will compile and include in the build.  Currently we assume the output
			/// binary file names match the solution file base name, but with various different binary file extensions.
			public List<string> DotNetSolutions = new List<string>();

			/// .NET .csproj files that will be compiled and included in the build.  Currently we assume the output
			/// binary file names match the solution file base name, but with various different binary file extensions.
			public List<string> DotNetProjects = new List<string>();

			/// These are .NET project binary base file names that we want to include and label with the build, but
			/// we won't be compiling these projects directly ourselves (however, they may be incidentally build or
			/// published by a different project that we are building.)  We'll look for various .NET binary files with
			/// this base file name but with different extensions.
			public List<string> ExtraDotNetFiles = new List<string>();

			/// These are .NET project binary files that are specific to IOS and found in the IOS subdirectory.  We define
			/// these buildproducts as existing in the DotNET\IOS directory and assume that the output binary file names match
			/// the solution file base name, but with various binary file extensions
			public List<string> IOSDotNetProjects = new List<string>();

			/// These are .NET project binary files that are specific to HTML5.  We define these build products as existing in the 
			/// DotNET directory and assume that the output binary file names match
			/// the solution file base name, but with various binary file extensions
			public List<string> HTML5DotNetProjects = new List<string>();

			public string SwarmProject = "";


			/// List of targets to build.  These can be various Unreal projects, programs or libraries in various configurations
			public List<BuildTarget> Targets = new List<BuildTarget>();

			/// Used for special jobs to test things. At the moment, debugging some megaXGE thing
			public bool SpecialTestFlag = false;

			public bool DoRetries = IsBuildMachine;

			/// <summary>
			/// Adds a target with the specified configuration.
			/// </summary>
			/// <param name="TargetName">Name of the target</param>
			/// <param name="InPlatform">Platform</param>
			/// <param name="InConfiguration">Configuration</param>
			/// <param name="InUprojectPath">Path to optional uproject file</param>
			/// <param name="bForceMonolithic">Force monolithic build.</param>
			/// <param name="bForceNonUnity">Force non-unity</param>
			/// <param name="bForceDebugInfo">Force debug info even in development builds</param>
			/// <param name="InAddArgs">Specifies additional arguments for UBT</param>
			public void AddTarget(string TargetName, UnrealBuildTool.UnrealTargetPlatform InPlatform, UnrealBuildTool.UnrealTargetConfiguration InConfiguration, string InUprojectPath = null, bool bForceMonolithic = false, bool bForceNonUnity = false, bool bForceDebugInfo = false, string InAddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.Platforms[InPlatform].CanBeCompiled())
				{
					return;
				}

				Targets.Add(new BuildTarget()
				{
					TargetName = TargetName,
					Platform = InPlatform,
					Config = InConfiguration,
					UprojectPath = InUprojectPath,
					ForceMonolithic = bForceMonolithic,
					ForceNonUnity = bForceNonUnity,
					ForceDebugInfo = bForceDebugInfo,
					UBTArgs = InAddArgs,
				});
			}

			/// <summary>
			/// Adds multiple targets with the specified configuration.
			/// </summary>
			/// <param name="TargetNames">List of targets.</param>
			/// <param name="InPlatform">Platform</param>
			/// <param name="InConfiguration">Configuration</param>
			/// <param name="InUprojectPath">Path to optional uproject file</param>
			/// <param name="bForceMonolithic">Force monolithic build.</param>
			/// <param name="bForceNonUnity">Force non-unity</param>
			/// <param name="bForceDebugInfo">Force debug info even in development builds</param>
			public void AddTargets(string[] TargetNames, UnrealBuildTool.UnrealTargetPlatform InPlatform, UnrealBuildTool.UnrealTargetConfiguration InConfiguration, string InUprojectPath = null, bool bForceMonolithic = false, bool bForceNonUnity = false, bool bForceDebugInfo = false, string InAddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.Platforms[InPlatform].CanBeCompiled())
				{
					return;
				}

				foreach (var Target in TargetNames)
				{
					Targets.Add(new BuildTarget()
					{
						TargetName = Target,
						Platform = InPlatform,
						Config = InConfiguration,
						UprojectPath = InUprojectPath,
						ForceMonolithic = bForceMonolithic,
						ForceNonUnity = bForceNonUnity,
						ForceDebugInfo = bForceDebugInfo,
						UBTArgs = InAddArgs,
					});
				}
			}
		}


		public UE4Build(BuildCommand Command)
		{
			OwnerCommand = Command;
			BuildProductFiles.Clear();
		}

		public List<string> FindXGEFiles()
		{
			var Result = new List<string>();
			var Root = CombinePaths(CmdEnv.LocalRoot, @"\Engine\Intermediate\Build");			
			Result.AddRange(FindFiles_NoExceptions("*.xge.xml", false, Root));
			Result.Sort();
			return Result;
		}
		public string XGEConsoleExe()
		{
			return @"C:\Program Files (x86)\Xoreax\IncrediBuild\xgConsole.exe";
		}

		public bool RunXGE(List<XGEItem> Actions, string TaskFilePath, bool DoRetries, bool SpecialTestFlag)
		{
			string XGEConsole = XGEConsoleExe();
			if (!FileExists(XGEConsole))
			{
				throw new AutomationException("Unable to find xge executable: " + XGEConsole);
			}

			XmlDocument XGETaskDocument = new XmlDocument();

			// <BuildSet FormatVersion="1">...</BuildSet>
			XmlElement BuildSetElement = XGETaskDocument.CreateElement("BuildSet");
			XGETaskDocument.AppendChild(BuildSetElement);
			BuildSetElement.SetAttribute("FormatVersion", "1");

			// <Environments>...</Environments>
			XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");
			BuildSetElement.AppendChild(EnvironmentsElement);

			int Job = 0;
			int Env = 0;
			Dictionary<string, XmlElement> EnvStringToEnv = new Dictionary<string, XmlElement>();
			Dictionary<string, XmlElement> EnvStringToProject = new Dictionary<string, XmlElement>();
			Dictionary<string, string> ParamsToTool = new Dictionary<string, string>();
			Dictionary<string, XmlElement> ParamsToToolElement = new Dictionary<string, XmlElement>();
			Dictionary<string, string> ToolToAction = new Dictionary<string, string>();
			foreach (var Item in Actions)
			{
				var CurrentDependencies = new List<string>();
				foreach (var XGEFile in Item.XgeXmlFiles)
				{
					if (!FileExists_NoExceptions(XGEFile))
					{
						throw new AutomationException("BUILD FAILED: Couldn't find file: {0}", XGEFile);
					}
					var TargetFile = TaskFilePath + "." + Path.GetFileName(XGEFile);
					CopyFile(XGEFile, TargetFile);
					CopyFile_NoExceptions(XGEFile, TaskFilePath);
					XmlDocument UBTTask = new XmlDocument();
					UBTTask.XmlResolver = null;
					UBTTask.Load(XGEFile);
					DeleteFile(XGEFile);

					var All = new List<string>();
					{
						var Elements = UBTTask.GetElementsByTagName("Variable");
						foreach (XmlElement Element in Elements)
						{
							string Pair = Element.Attributes["Name"].Value + "=" + Element.Attributes["Value"].Value;
							All.Add(Pair);
						}
					}
					All.Sort();
					string AllString = "";
					foreach (string Element in All)
					{
						AllString += Element + "\n";
					}
					XmlElement ToolsElement;
					XmlElement ProjectElement;

					if (EnvStringToEnv.ContainsKey(AllString))
					{
						ToolsElement = EnvStringToEnv[AllString];
						ProjectElement = EnvStringToProject[AllString];
					}
					else
					{
						string EnvName = string.Format("Env_{0}", Env);
						Env++;
						// <Environment Name="Win32">...</Environment>
						XmlElement EnvironmentElement = XGETaskDocument.CreateElement("Environment");
						EnvironmentsElement.AppendChild(EnvironmentElement);
						EnvironmentElement.SetAttribute("Name", EnvName);

						// <Tools>...</Tools>
						ToolsElement = XGETaskDocument.CreateElement("Tools");
						EnvironmentElement.AppendChild(ToolsElement);

						{
							// <Variables>...</Variables>
							XmlElement VariablesElement = XGETaskDocument.CreateElement("Variables");
							EnvironmentElement.AppendChild(VariablesElement);

							var Elements = UBTTask.GetElementsByTagName("Variable");
							foreach (XmlElement Element in Elements)
							{
								// <Variable>...</Variable>
								XmlElement VariableElement = XGETaskDocument.CreateElement("Variable");
								VariablesElement.AppendChild(VariableElement);
								VariableElement.SetAttribute("Name", Element.Attributes["Name"].Value);
								VariableElement.SetAttribute("Value", Element.Attributes["Value"].Value);
							}
						}

						// <Project Name="Default" Env="Default">...</Project>
						ProjectElement = XGETaskDocument.CreateElement("Project");
						BuildSetElement.AppendChild(ProjectElement);
						ProjectElement.SetAttribute("Name", EnvName);
						ProjectElement.SetAttribute("Env", EnvName);

						EnvStringToEnv.Add(AllString, ToolsElement);
						EnvStringToProject.Add(AllString, ProjectElement);

					}

					Dictionary<string, string> ToolToTool = new Dictionary<string, string>();
					Dictionary<string, string> ActionToAction = new Dictionary<string, string>();

					{
						var Elements = UBTTask.GetElementsByTagName("Tool");
						foreach (XmlElement Element in Elements)
						{
							string Key = Element.Attributes["Path"].Value;
							Key += " ";
							Key += Element.Attributes["Params"].Value;

							//hack hack hack
							string ElementParams = Element.Attributes["Params"].Value;
							if (!String.IsNullOrEmpty(ElementParams))
							{
								int YcIndex = ElementParams.IndexOf(" /Yc\"");
								if (YcIndex >= 0)
								{
									// /Fp&quot;D:\BuildFarm\buildmachine_++depot+UE4\Engine\Intermediate\BuildData\Win64\UE4Editor\Development\SharedPCHs\CoreUObject.h.pch&quot
									string Fp = " /Fp\"";
									int FpIndex = ElementParams.IndexOf(Fp, YcIndex);
									if (FpIndex >= 0)
									{
										int EndIndex = ElementParams.IndexOf("\"", FpIndex + Fp.Length);
										if (EndIndex >= 0)
										{
											string PCHFileName = ElementParams.Substring(FpIndex + Fp.Length, EndIndex - FpIndex - Fp.Length);
											if (PCHFileName.Contains(@"\SharedPCHs\") && PCHFileName.Contains(@"\UE4Editor\"))
											{
												Key = "SharedEditorPCH$ " + PCHFileName;
												Log("Hack: detected Shared PCH, which will use a different key {0}", Key);

											}
										}
									}
								}

							}

							string ToolName = string.Format("{0}_{1}", Element.Attributes["Name"].Value, Job);
							string OriginalToolName = ToolName;

							if (ParamsToTool.ContainsKey(Key))
							{
								ToolName = ParamsToTool[Key];
								ToolToTool.Add(OriginalToolName, ToolName);

								XmlElement ToolElement = ParamsToToolElement[Key];
								ToolElement.SetAttribute("GroupPrefix", ToolElement.Attributes["GroupPrefix"].Value + " + " + Item.OutputCaption);
							}
							else
							{
								// <Tool ... />
								XmlElement ToolElement = XGETaskDocument.CreateElement("Tool");
								ToolsElement.AppendChild(ToolElement);

								ParamsToTool.Add(Key, ToolName);
								ParamsToToolElement.Add(Key, ToolElement);

								ToolElement.SetAttribute("Name", ToolName);
								ToolElement.SetAttribute("AllowRemote", Element.Attributes["AllowRemote"].Value);
								if (Element.HasAttribute("OutputPrefix"))
								{
									ToolElement.SetAttribute("OutputPrefix", Element.Attributes["OutputPrefix"].Value);
								}
								ToolElement.SetAttribute("GroupPrefix", "** For " + Item.OutputCaption);

								ToolElement.SetAttribute("Params", Element.Attributes["Params"].Value);
								ToolElement.SetAttribute("Path", Element.Attributes["Path"].Value);
								if(Element.HasAttribute("VCCompiler"))
								{
									ToolElement.SetAttribute("VCCompiler", Element.Attributes["VCCompiler"].Value);
								}
								ToolElement.SetAttribute("SkipIfProjectFailed", Element.Attributes["SkipIfProjectFailed"].Value);
								if (Element.HasAttribute("AutoReserveMemory"))
								{
									ToolElement.SetAttribute("AutoReserveMemory", Element.Attributes["AutoReserveMemory"].Value);
								}
								ToolElement.SetAttribute("OutputFileMasks", Element.Attributes["OutputFileMasks"].Value);
								//ToolElement.SetAttribute("AllowRestartOnLocal", "false");  //vs2012 can't really restart, so we will go with this for now
								if (Element.Attributes["OutputFileMasks"].Value == "PCLaunch.rc.res")
								{
									// total hack, when doing clean compiles, this output directory does not exist, we need to create it now
									string Parms = Element.Attributes["Params"].Value;
									string Start = "/fo \"";
									int StartIndex = Parms.IndexOf(Start);
									if (StartIndex >= 0)
									{
										Parms = Parms.Substring(StartIndex + Start.Length);
										int EndIndex = Parms.IndexOf("\"");
										if (EndIndex > 0)
										{
											string ResLocation = CombinePaths(Parms.Substring(0, EndIndex));
											if (!DirectoryExists_NoExceptions(GetDirectoryName(ResLocation)))
											{
												CreateDirectory(GetDirectoryName(ResLocation));
											}
										}
									}
								}
							}
						}
					}
					{
						var NextDependencies = new List<string>();

						var Elements = UBTTask.GetElementsByTagName("Task");
						foreach (XmlElement Element in Elements)
						{
							string ToolName = string.Format("{0}_{1}", Element.Attributes["Tool"].Value, Job);
							string ActionName = string.Format("{0}_{1}", Element.Attributes["Name"].Value, Job);
							string OriginalActionName = ActionName;

							if (ToolToTool.ContainsKey(ToolName))
							{
								ToolName = ToolToTool[ToolName];
								ActionName = ToolToAction[ToolName];
								ActionToAction.Add(OriginalActionName, ActionName);
							}
							else
							{
								ActionToAction.Add(OriginalActionName, ActionName);
								ToolToAction.Add(ToolName, ActionName);
								// <Task ... />
								XmlElement TaskElement = XGETaskDocument.CreateElement("Task");
								ProjectElement.AppendChild(TaskElement);

								TaskElement.SetAttribute("SourceFile", Element.Attributes["SourceFile"].Value);
								if (Element.HasAttribute("Caption"))
								{
									TaskElement.SetAttribute("Caption", Element.Attributes["Caption"].Value);
								}
								TaskElement.SetAttribute("Name", ActionName);
								NextDependencies.Add(ActionName);
								TaskElement.SetAttribute("Tool", ToolName);
								TaskElement.SetAttribute("WorkingDir", Element.Attributes["WorkingDir"].Value);
								TaskElement.SetAttribute("SkipIfProjectFailed", Element.Attributes["SkipIfProjectFailed"].Value);

								string NewDepends = "";
								if (Element.HasAttribute("DependsOn"))
								{
									string Depends = Element.Attributes["DependsOn"].Value;
									while (Depends.Length > 0)
									{
										string ThisAction = Depends;
										int Semi = Depends.IndexOf(";");
										if (Semi >= 0)
										{
											ThisAction = Depends.Substring(0, Semi);
											Depends = Depends.Substring(Semi + 1);
										}
										else
										{
											Depends = "";
										}
										if (ThisAction.Length > 0)
										{
											if (NewDepends.Length > 0)
											{
												NewDepends += ";";
											}
											string ActionJob = ThisAction + string.Format("_{0}", Job);
											if (!ActionToAction.ContainsKey(ActionJob))
											{
												throw new AutomationException("Action not found '{0}' in {1}->{2}", ActionJob, XGEFile, TargetFile);
												// the XGE schedule is not topologically sorted. Hmmm. Well, we make a scary assumption here that this 
											}
											NewDepends += ActionToAction[ActionJob];
										}
									}
								}
								foreach (var Dep in CurrentDependencies)
								{
									if (NewDepends.Length > 0)
									{
										NewDepends += ";";
									}
									NewDepends += Dep;
								}
								if (NewDepends != "")
								{
									TaskElement.SetAttribute("DependsOn", NewDepends);
								}
							}

						}
						CurrentDependencies.AddRange(NextDependencies);
					}
					Job++;
				}
			}
			if (Job == 0)
			{
				if (DeleteBuildProducts)
				{
					throw new AutomationException("Unable to find xge xmls: " + CombinePaths(CmdEnv.LogFolder, "*.xge.xml"));
				}
				else
				{
					Log("Incremental build, apparently everything was up to date, no XGE jobs produced.");
				}
			}
			else
			{
				// Write the XGE task XML to a temporary file.
				using (FileStream OutputFileStream = new FileStream(TaskFilePath, FileMode.Create, FileAccess.Write))
				{
					XGETaskDocument.Save(OutputFileStream);
				}
				if (!FileExists(TaskFilePath))
				{
					throw new AutomationException("Unable to find xge xml: " + TaskFilePath);
				}

				string Args = "\"" + TaskFilePath + "\" /Rebuild /MaxCPUS=200";

				int Retries = DoRetries ? 2 : 1;
                int ConnectionRetries = 4;
                for (int i = 0; i < Retries; i++)
				{
					try
					{
                        while (true)
                        {
                            Log("Running XGE *******");
                            PushDir(CombinePaths(CmdEnv.LocalRoot, @"\Engine\Source"));
                            int SuccesCode;
                            string LogFile = GetRunAndLogLogName(CmdEnv, XGEConsole);
                            string Output = RunAndLog(XGEConsole, Args, out SuccesCode, LogFile);
                            PopDir();
                            if (ConnectionRetries > 0 && (SuccesCode == 4 || SuccesCode == 2) && !Output.Contains("------Project:"))
                            {
                                Log(System.Diagnostics.TraceEventType.Warning, "XGE failure on the local connection timeout");
                                if (ConnectionRetries < 2)
                                {
                                    System.Threading.Thread.Sleep(60000);
                                }
                                ConnectionRetries--;
                                continue;
                            }
                            else if (SuccesCode != 0)
                            {
                                Log("XGE did not succeed *******");
                                throw new AutomationException(String.Format("Command failed (Result:{3}): {0} {1}. See logfile for details: '{2}' ",
                                                                XGEConsole, Args, Path.GetFileName(LogFile), SuccesCode));
                            }
                            Log("XGE Done *******");
                            break;
                        }
                        break;
					}
					catch (Exception Ex)
					{
						Log("XGE failed on try {0}: {1}", i + 1, Ex.ToString());
						if (i + 1 >= Retries)
						{
							return false;
						}
						Log("Deleting build products to force a rebuild.");
						foreach (var Item in Actions)
						{
							XGEDeleteBuildProducts(Item.Manifest);
						}
					}
				}

			}
			foreach (var Item in Actions)
			{
				XGEFinishBuildWithUBT(Item);
			}
			return true;
		}

		public void ClearExportedXGEXML()
		{
			foreach (var XGEFile in FindXGEFiles())
			{
				DeleteFile(XGEFile);
			}
		}

		public bool CanUseXGE(UnrealBuildTool.UnrealTargetPlatform Platform)
		{
			if (!UnrealBuildTool.UEBuildPlatform.BuildPlatformDictionary.ContainsKey(Platform))
			{
				return false;
			}

			return UnrealBuildTool.UEBuildPlatform.GetBuildPlatform(Platform).CanUseXGE();
		}

		public string GenerateExternalFileList(BuildAgenda Agenda)
		{
			string AdditionalArguments = "";
			foreach (var Target in Agenda.Targets)
			{
				RunUBT(CmdEnv, UBTExecutable, Target.UprojectPath, Target.TargetName, Target.Platform.ToString(), Target.Config.ToString(), "-generateexternalfilelist" + AdditionalArguments + " " + Target.UBTArgs);
				AdditionalArguments = " -mergeexternalfilelist";
			}
			return CommandUtils.CombinePaths(CmdEnv.LocalRoot, @"/Engine/Intermediate/Build/ExternalFiles.xml");
		}

		public string[] GetExternalFileList(BuildAgenda Agenda)
		{
			string FileListPath = GenerateExternalFileList(Agenda);
			return UnrealBuildTool.Utils.ReadClass<UnrealBuildTool.ExternalFileList>(FileListPath).FileNames.ToArray();
		}

		/// <summary>
		/// Executes a build.
		/// </summary>
		/// <param name="Agenda">Build agenda.</param>
		/// <param name="InDeleteBuildProducts">if specified, determines if the build products will be deleted before building. If not specified -clean parameter will be used,</param>
		/// <param name="InUpdateVersionFiles">True if the version files are to be updated </param>
		/// <param name="InForceNoXGE">If true will force XGE off</param>
		public void Build(BuildAgenda Agenda, bool? InDeleteBuildProducts = null, bool InUpdateVersionFiles = true, bool InForceNoXGE = false, bool InForceNonUnity = false, bool InForceUnity = false, Dictionary<UnrealBuildTool.UnrealTargetPlatform, Dictionary<string, string>> PlatformEnvVars = null)
		{
			if (!CmdEnv.HasCapabilityToCompile)
			{
				throw new AutomationException("You are attempting to compile on a machine that does not have a supported compiler!");
			}
			DeleteBuildProducts = InDeleteBuildProducts.HasValue ? InDeleteBuildProducts.Value : OwnerCommand.ParseParam("Clean");
			if (InUpdateVersionFiles)
			{
				UpdateVersionFiles();
			}

			{
				var EncryptionKeyFilename = OwnerCommand.ParseParamValue("SignPak");
				if (String.IsNullOrEmpty(EncryptionKeyFilename) == false)
				{
					UpdatePublicKey(EncryptionKeyFilename);
				}
			}

			//////////////////////////////////////

			// make a set of unique platforms involved
			var UniquePlatforms = new List<UnrealBuildTool.UnrealTargetPlatform>();
			foreach (var Target in Agenda.Targets)
			{
				if (!UniquePlatforms.Contains(Target.Platform))
				{
					UniquePlatforms.Add(Target.Platform);
				}
			}

			// allow all involved platforms to hook into the agenda
			foreach (var TargetPlatform in UniquePlatforms)
			{
				Platform.Platforms[TargetPlatform].PreBuildAgenda(this, Agenda);
			}


			foreach (var File in Agenda.ExtraDotNetFiles)
			{
				PrepareBuildProductsForCSharpProj(Path.Combine(CmdEnv.LocalRoot, File));
			}

			if (Agenda.SwarmProject != "")
			{
				string SwarmSolution = Path.Combine(CmdEnv.LocalRoot, Agenda.SwarmProject);
				PrepareBuildProductsForCSharpProj(SwarmSolution);

				BuildSolution(CmdEnv, SwarmSolution);
				AddSwarmBuildProducts();
			}

			foreach (var DotNetSolution in Agenda.DotNetSolutions)
			{
				string Solution = Path.Combine(CmdEnv.LocalRoot, DotNetSolution);

				PrepareBuildProductsForCSharpProj(Solution);

				BuildSolution(CmdEnv, Solution);

				AddBuildProductsForCSharpProj(Solution);
			}

			foreach (var DotNetProject in Agenda.DotNetProjects)
			{
				string CsProj = Path.Combine(CmdEnv.LocalRoot, DotNetProject);

				PrepareBuildProductsForCSharpProj(CsProj);

				BuildCSharpProject(CmdEnv, CsProj);

				AddBuildProductsForCSharpProj(CsProj);
			}

			foreach (var IOSDotNetProject in Agenda.IOSDotNetProjects)
			{
				string IOSCsProj = Path.Combine(CmdEnv.LocalRoot, IOSDotNetProject);

				PrepareBuildProductsForCSharpProj(IOSCsProj);

				BuildCSharpProject(CmdEnv, IOSCsProj);

				AddIOSBuildProductsForCSharpProj(IOSCsProj);
			}

			foreach (var HTML5DotNetProject in Agenda.HTML5DotNetProjects)
			{
				string CsProj = Path.Combine(CmdEnv.LocalRoot, HTML5DotNetProject);

				PrepareBuildProductsForCSharpProj(CsProj);

				BuildCSharpProject(CmdEnv, CsProj);

				AddBuildProductsForCSharpProj(CsProj);
			}

			foreach (var File in Agenda.ExtraDotNetFiles)
			{
				AddBuildProductsForCSharpProj(Path.Combine(CmdEnv.LocalRoot, File));
			}

			bool bForceMonolithic = OwnerCommand.ParseParam("ForceMonolithic");
			bool bForceNonUnity = OwnerCommand.ParseParam("ForceNonUnity") || InForceNonUnity;
			bool bForceUnity = OwnerCommand.ParseParam("ForceUnity") || InForceUnity;
			bool bForceDebugInfo = OwnerCommand.ParseParam("ForceDebugInfo");
			bool bUsedXGE = false;
			bool bCanUseXGE = !OwnerCommand.ParseParam("NoXGE") && !InForceNoXGE;
			string XGEConsole = XGEConsoleExe();
			if (bCanUseXGE && !FileExists(XGEConsole))
			{
				Log("XGE was requested, but is unavailable, so we won't use it.");
				bCanUseXGE = false;
			}

			Log("************************* UE4Build:");
			Log("************************* ForceMonolithic: {0}", bForceMonolithic);
			Log("************************* ForceNonUnity:{0} ", bForceNonUnity);
			Log("************************* ForceDebugInfo: {0}", bForceDebugInfo);
			Log("************************* UseXGE: {0}", bCanUseXGE);

			if (bCanUseXGE)
			{
				string TaskFilePath = CombinePaths(CmdEnv.LogFolder, @"UAT_XGE.xml");

				if (DeleteBuildProducts)
				{
					foreach (var Target in Agenda.Targets)
					{
						CleanWithUBT(Target.ProjectName, Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
					}
				}
				foreach (var Target in Agenda.Targets)
				{
					if (Target.TargetName == "UnrealHeaderTool" || !CanUseXGE(Target.Platform))
					{
						// When building a target for Mac or iOS, use UBT's -flushmac option to clean up the remote builder
						bool bForceFlushMac = DeleteBuildProducts && (Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac || Target.Platform == UnrealBuildTool.UnrealTargetPlatform.IOS);
						BuildWithUBT(Target.ProjectName, Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, bForceFlushMac, true, Target.UBTArgs, bForceUnity);
					}
				}

				List<XGEItem> XGEItems = new List<XGEItem>();
				foreach (var Target in Agenda.Targets)
				{
					if (Target.TargetName != "UnrealHeaderTool" && CanUseXGE(Target.Platform))
					{
						XGEItem Item = XGEPrepareBuildWithUBT(Target.ProjectName, Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
						XGEItems.Add(Item);
					}
				}
				if (XGEItems.Count > 0)
				{
					if (!RunXGE(XGEItems, TaskFilePath, Agenda.DoRetries, Agenda.SpecialTestFlag))
					{
						throw new AutomationException("BUILD FAILED: XGE failed, retries not enabled:");
					}
					else
					{
						bUsedXGE = true;
					}
				}
				else
				{
					bUsedXGE = true; // if there was nothing to build, we still consider this a success
				}
			}
			if (!bUsedXGE)
			{
				if (DeleteBuildProducts)
				{
					foreach (var Target in Agenda.Targets)
					{
						CleanWithUBT(Target.ProjectName, Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, Target.UBTArgs, bForceUnity);
					}
				}
				foreach (var Target in Agenda.Targets)
				{
					// When building a target for Mac or iOS, use UBT's -flushmac option to clean up the remote builder
					bool bForceFlushMac = DeleteBuildProducts && (Target.Platform == UnrealBuildTool.UnrealTargetPlatform.Mac || Target.Platform == UnrealBuildTool.UnrealTargetPlatform.IOS);
					BuildWithUBT(Target.ProjectName, Target.TargetName, Target.Platform, Target.Config.ToString(), Target.UprojectPath, bForceMonolithic, bForceNonUnity, bForceDebugInfo, bForceFlushMac, true, Target.UBTArgs, bForceUnity);
				}
			}

			// NOTE: OK, we're done building projects.  You can build additional targets after this.  Call FinalizebuildProducts() when done.
		}


		/// <summary>
		/// Checks to make sure there was at least one build product, and that all files exist.  Also, logs them all out.
		/// </summary>
		/// <param name="BuildProductFiles">List of files</param>
		public static void CheckBuildProducts(List<string> BuildProductFiles)
		{
			// Check build products
			{
				Log("Build products *******");
				if (BuildProductFiles.Count < 1)
				{
					Log("No build products were made");
				}
				else
				{
					foreach (var Product in BuildProductFiles)
					{
						if (!FileExists(Product))
						{
							throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", Product);
						}
						Log(Product);
					}
				}
				Log("End Build products *******");
			}
		}


		/// <summary>
		/// Adds or edits existing files at head revision, expecting an exclusive lock, resolving by clobbering any existing version
		/// </summary>
		/// <param name="Files">List of files to check out</param>
		public static void AddBuildProductsToChangelist(int WorkingCL, List<string> Files)
		{
			Log("Adding {0} build products to changelist {1}...", Files.Count, WorkingCL);
			foreach (var File in Files)
			{
				P4.Sync("-f -k " + CommandUtils.MakePathSafeToUseWithCommandLine(File) + "#head"); // sync the file without overwriting local one
				if (!FileExists(File))
				{
					throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", File);
				}

				P4.ReconcileNoDeletes(WorkingCL, CommandUtils.MakePathSafeToUseWithCommandLine(File));

				// Change file type on binary files to be always writeable.
				var FileStats = P4.FStat(File);

                if (IsProbablyAMacOrIOSExe(File))
                {
                    if (FileStats.Type == P4FileType.Binary && (FileStats.Attributes & (P4FileAttributes.Executable | P4FileAttributes.Writeable)) != (P4FileAttributes.Executable | P4FileAttributes.Writeable))
                    {
                        P4.ChangeFileType(File, (P4FileAttributes.Executable | P4FileAttributes.Writeable));
                    }
                }
                else
                {
					if (IsBuildProduct(File, FileStats) && (FileStats.Attributes & P4FileAttributes.Writeable) != P4FileAttributes.Writeable)
                    {
                        P4.ChangeFileType(File, P4FileAttributes.Writeable);
                    }

                }                    
			}
		}

		/// <summary>
		/// Determines if this file is a build product.
		/// </summary>
		/// <param name="File">File path</param>
		/// <param name="FileStats">P4 file stats.</param>
		/// <returns>True if this is a Windows build product. False otherwise.</returns>
		private static bool IsBuildProduct(string File, P4FileStat FileStats)
		{
			if(FileStats.Type == P4FileType.Binary || IsBuildReceipt(File))
			{
				return true;
			}

			return FileStats.Type == P4FileType.Text && File.EndsWith(".exe.config", StringComparison.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Add UBT files to build products
		/// </summary>
		public void AddUBTFilesToBuildProducts()
		{
			if (GlobalCommandLine.NoCompile)
			{
				Log("We are being asked to copy the UBT build products, but we are running precompiled, so this does not make much sense.");
			}

			var UBTLocation = Path.GetDirectoryName(GetUBTExecutable());
			var UBTFiles = new List<string>(new string[] 
					{
						"UnrealBuildTool.exe",
						"UnrealBuildTool.exe.config",
						"EnvVarsToXML.exe",
						"EnvVarsToXML.exe.config"
					});

			foreach (var UBTFile in UBTFiles)
			{
				var UBTProduct = CombinePaths(UBTLocation, UBTFile);
				if (!FileExists_NoExceptions(UBTProduct))
				{
					throw new AutomationException("Cannot add UBT to the build products because {0} does not exist.", UBTProduct);
				}
				AddBuildProduct(UBTProduct);
			}
		}

		/// <summary>
		/// Copy the UAT files to their precompiled location, and add them as build products
		/// </summary>
		public void AddUATFilesToBuildProducts()
		{
			// Find all DLLs (scripts and their dependencies)
			const string ScriptsPostfix = ".dll";
            var DotNetOutputLocation = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET");
			var UATScriptsLocation = CombinePaths(DotNetOutputLocation, "AutomationScripts");
			var UATScripts = Directory.GetFiles(UATScriptsLocation, "*" + ScriptsPostfix, SearchOption.AllDirectories);
			if (IsNullOrEmpty(UATScripts))
			{
				throw new AutomationException("No automation scripts found in {0}. Cannot add UAT files to the build products.", UATScriptsLocation);
			}

			var UATFiles = new List<string>(new string[] 
					{
						"AutomationTool.exe",
						"AutomationTool.exe.config",
						"UnrealBuildTool.exe",
						"UnrealBuildTool.exe.config",
						"DotNETUtilities.dll",
					});

			foreach (var UATFile in UATFiles)
			{
				var OutputFile = CombinePaths(DotNetOutputLocation, UATFile);
				if (!FileExists_NoExceptions(OutputFile))
				{
					throw new AutomationException("Cannot add UAT to the build products because {0} does not exist.", OutputFile);
				}
				AddBuildProduct(OutputFile);
			}

			// All scripts are expected to exist in DotNET/AutomationScripts subfolder.
			foreach (var UATScriptFilePath in UATScripts)
			{
				if (!FileExists_NoExceptions(UATScriptFilePath))
				{
					throw new AutomationException("Cannot add UAT to the build products because {0} does not exist.", UATScriptFilePath);
				}

				AddBuildProduct(UATScriptFilePath);
			}
		}

		string GetUBTManifest(string UProjectPath, string InAddArgs)
		{
			// Can't write to Engine directory on 
            bool bForceRocket = InAddArgs.ToLowerInvariant().Contains(" -rocket"); //awful

            if ((GlobalCommandLine.Rocket || bForceRocket) && !String.IsNullOrEmpty(UProjectPath))
			{
				return Path.Combine(Path.GetDirectoryName(UProjectPath), "Intermediate/Build/Manifest.xml");				
			}
			else
			{
				return CommandUtils.CombinePaths(CmdEnv.LocalRoot, @"/Engine/Intermediate/Build/Manifest.xml");
			}
		}

		public static string GetUBTExecutable()
		{
			return CommandUtils.CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/UnrealBuildTool.exe");
		}

		public string UBTExecutable
		{
			get
			{
				return GetUBTExecutable();							
			}
		}

		// List of everything we built so far
		public readonly List<string> BuildProductFiles = new List<string>();

		private bool DeleteBuildProducts = true;
	}
}
