// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;

namespace UnrealBuildTool
{
	public class IOSPlatformContext : UEBuildPlatformContext
	{
		private bool bInitializedProject = false;

		// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
		public static string IOSArchitecture = "";

		/// <summary>
		/// Which version of the iOS to allow at run time
		/// </summary>
		private static string RunTimeIOSVersion = "8.0";

		/// <summary>
		/// which devices the game is allowed to run on
		/// </summary>
		private static  string RunTimeIOSDevices = "1,2";

		/// <summary>
		/// The architecture(s) to compile
		/// </summary>
		private static string NonShippingArchitectures = "armv7";
		private static string ShippingArchitectures = "armv7,arm64";

		/// <summary>
		/// additional linker flags for shipping
		/// </summary>
		public string AdditionalShippingLinkerFlags = "";

		/// <summary>
		/// additional linker flags for non-shipping
		/// </summary>
		public string AdditionalLinkerFlags = "";

		/// <summary>
		/// mobile provision to use for code signing
		/// </summary>
		public string MobileProvision = "";

        /// <summary>
        /// mobile provision UUID to use for code signing
        /// </summary>
        public string MobileProvisionUUID = "";

        /// <summary>
        /// mobile provision UUID to use for code signing
        /// </summary>
        public string TeamUUID = "";

        /// <summary>
        /// signing certificate to use for code signing
        /// </summary>
        public string SigningCertificate = "";

		/// <summary>
		/// The list of architectures
		/// </summary>
		public List<string> ProjectArches;

		/// <summary>
		/// true if bit code should be embedded
		/// </summary>
		private bool bShipForBitcode = false;

        /// <summary>
        /// true if notifications are enabled
        /// </summary>
        private bool bNotificationsEnabled = false;

        private bool bForDistribtion = false;

        private string BundleIdentifier = "";

        public IOSPlatformContext(FileReference InProjectFile, bool ForDistribution = false)
            : this(UnrealTargetPlatform.IOS, InProjectFile, ForDistribution)
        {
        }

        protected IOSPlatformContext(UnrealTargetPlatform TargetPlatform, FileReference InProjectFile, bool ForDistribution)
            : base(TargetPlatform, InProjectFile)
		{
            bForDistribtion = ForDistribution;
        }

        // The current architecture - affects everything about how UBT operates on IOS
        public override string GetActiveArchitecture()
		{
			return IOSArchitecture;
		}

		public virtual string GetRunTimeVersion()
		{
			return RunTimeIOSVersion;
		}

		public virtual string GetRunTimeDevices()
		{
			return RunTimeIOSDevices;
		}

		public bool IsBitcodeCompilingEnabled(CPPTargetConfiguration Configuration)
		{
			return Configuration == CPPTargetConfiguration.Shipping && bShipForBitcode;
		}

		// The name that Xcode uses for the platform
		private const string XcodeDevicePlatformName = "iPhoneOS";
		private const string XcodeSimulatorPlatformName = "iPhoneSimulator";

		public virtual string GetXcodePlatformName(bool bForDevice)
		{
			return bForDevice ? XcodeDevicePlatformName : XcodeSimulatorPlatformName;
		}

		public virtual string GetXcodeMinVersionParam()
		{
			return "iphoneos-version-min";
		}

		public virtual string GetCodesignPlatformName()
		{
			return "iphoneos";
		}

		public virtual string GetArchitectureArgument(CPPTargetConfiguration Configuration, string UBTArchitecture)
		{
			SetUpProjectEnvironment(Configuration);

            // get the list of architectures to compile
            string Archs =
				UBTArchitecture == "-simulator" ? "i386" :
				(Configuration == CPPTargetConfiguration.Shipping) ? ShippingArchitectures : NonShippingArchitectures;

			Log.TraceLogOnce("Compiling with these architectures: " + Archs);

			// parse the string
			string[] Tokens = Archs.Split(",".ToCharArray());

			string Result = "";
			foreach (string Token in Tokens)
			{
				Result += " -arch " + Token;
			}

			return Result;
		}
		
		public virtual string GetRequiredCapabilities()
		{
			string	result = "";

            // get the list of architectures compiled
            string Archs = (TargetConfiguration == UnrealTargetConfiguration.Shipping) ? ShippingArchitectures : NonShippingArchitectures;
            string[] ArchArray = Archs.Split(",".ToCharArray());
            if (ArchArray.Length > 1)
            {
                result += "\t\t<string>armv7</string>\n";
            }
            else
            {
                result += "\t\t<string>" + ArchArray[0] + "</string>\n";
            }

            return result;
		}

		public string GetAdditionalLinkerFlags(CPPTargetConfiguration InConfiguration)
		{
			if (InConfiguration != CPPTargetConfiguration.Shipping)
			{
				return AdditionalLinkerFlags;
			}
			else
			{
				return AdditionalShippingLinkerFlags;
			}

		}

        public void SetUpProjectEnvironment(CPPTargetConfiguration Configuration)
        {
            UnrealTargetConfiguration unrealConfiguration;

            switch (Configuration)
            {
                case CPPTargetConfiguration.Shipping:
                    unrealConfiguration = UnrealTargetConfiguration.Shipping;
                    break;
                case CPPTargetConfiguration.Development:
                    unrealConfiguration = UnrealTargetConfiguration.Development;
                    break;
                default:
                    unrealConfiguration = UnrealTargetConfiguration.DebugGame;
                    break;
            }

            SetUpProjectEnvironment(unrealConfiguration);
        }

        static string Provision = "";
        static string Certificate = "";
        static bool bHaveCertificate = false;
        public static void IPPDataReceivedHandler(Object Sender, DataReceivedEventArgs Line)
        {
            if ((Line != null) && (Line.Data != null))
            {
                if (!string.IsNullOrEmpty(Certificate))
                {
                    if (Line.Data.Contains("CERTIFICATE-") && Line.Data.Contains(Certificate))
                    {
                        bHaveCertificate = true;
                    }
                }
                else
                {
                    int cindex = Line.Data.IndexOf("CERTIFICATE-");
                    int pindex = Line.Data.IndexOf("PROVISION-");
                    if (cindex > -1 && pindex > -1)
                    {
                        cindex += "CERTIFICATE-".Length;
                        Certificate = Line.Data.Substring(cindex, pindex - cindex - 1);
                        pindex += "PROVISION-".Length;
                        Provision = Line.Data.Substring(pindex);
                    }
                }
            }
        }

        struct ProvisionData
        {
            public string MobileProvision;
            public string Certificate;
            public string UUID;
            public string TeamUUID;
        }
        static Dictionary<string, ProvisionData> ProvisionCache = new Dictionary<string, ProvisionData>();

        public override void SetUpProjectEnvironment(UnrealTargetConfiguration Configuration, TargetInfo Target = null)
        {
            if (!bInitializedProject)
            {
                base.SetUpProjectEnvironment(Configuration, Target);

                // update the configuration based on the project file
                // look in ini settings for what platforms to compile for
                ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", DirectoryReference.FromFile(ProjectFile));
                string MinVersion = "IOS_8";
                if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumiOSVersion", out MinVersion))
                {
                    switch (MinVersion)
                    {
                        case "IOS_61":
                            Log.TraceWarningOnce("IOS 6 is no longer supported in UE4 as 4.11");
                            RunTimeIOSVersion = "8.0";
                            break;
                        case "IOS_7":
                            Log.TraceWarningOnce("IOS 7 is no longer supported in UE4 as 4.14");
                            RunTimeIOSVersion = "8.0";
                            break;
                        case "IOS_8":
                            RunTimeIOSVersion = "8.0";
                            break;
                        case "IOS_9":
                            RunTimeIOSVersion = "9.0";
                            break;
                        case "IOS_10":
                            RunTimeIOSVersion = "10.0";
                            break;
                    }
                }

                bool biPhoneAllowed = true;
                bool biPadAllowed = true;
                Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPhone", out biPhoneAllowed);
                Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPad", out biPadAllowed);
                if (biPhoneAllowed && biPadAllowed)
                {
                    RunTimeIOSDevices = "1,2";
                }
                else if (biPadAllowed)
                {
                    RunTimeIOSDevices = "2";
                }
                else if (biPhoneAllowed)
                {
                    RunTimeIOSDevices = "1";
                }

                ProjectArches = new List<string>();
                bool bBuild = true;
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7", out bBuild) && bBuild)
                {
                    ProjectArches.Add("armv7");
                }
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArm64", out bBuild) && bBuild)
                {
                    ProjectArches.Add("arm64");
                }
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7S", out bBuild) && bBuild)
                {
                    ProjectArches.Add("armv7s");
                }

                // force armv7 if something went wrong
                if (ProjectArches.Count == 0)
                {
                    ProjectArches.Add("armv7");
                }
                NonShippingArchitectures = ProjectArches[0];
                for (int Index = 1; Index < ProjectArches.Count; ++Index)
                {
                    NonShippingArchitectures += "," + ProjectArches[Index];
                }

                ProjectArches.Clear();
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7", out bBuild) && bBuild)
                {
                    ProjectArches.Add("armv7");
                }
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArm64", out bBuild) && bBuild)
                {
                    ProjectArches.Add("arm64");
                }
                if (Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7S", out bBuild) && bBuild)
                {
                    ProjectArches.Add("armv7s");
                }

                // force armv7 if something went wrong
                if (ProjectArches.Count == 0)
                {
                    ProjectArches.Add("armv7");
                    ProjectArches.Add("arm64");
                }
                ShippingArchitectures = ProjectArches[0];
                for (int Index = 1; Index < ProjectArches.Count; ++Index)
                {
                    ShippingArchitectures += "," + ProjectArches[Index];
                }

                // determine if we need to generate the dsym
                Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMFile", out BuildConfiguration.bGeneratedSYMFile);
                Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMBundle", out BuildConfiguration.bGeneratedSYMBundle);

                // determie if bitcode should be generated for the shipping code
                Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForBitcode", out bShipForBitcode);

                // @todo tvos: We probably want to handle TVOS versions here
                Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalLinkerFlags", out AdditionalLinkerFlags);
                Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalShippingLinkerFlags", out AdditionalShippingLinkerFlags);

                Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MobileProvision", out MobileProvision);
                Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "SigningCertificate", out SigningCertificate);

                // bundle identifier
                Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleIdentifier);

                bInitializedProject = true;
            }

            ProvisionData Data = new ProvisionData();
            string BundleId = BundleIdentifier.Replace("[PROJECT_NAME]", ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game")).Replace("_", "");
            bool bIsTVOS = GetCodesignPlatformName() == "appletvos";
            if (!ProvisionCache.ContainsKey(BundleId + " " + bIsTVOS.ToString() + " " + bForDistribtion.ToString()))
            {
                Certificate = SigningCertificate;
                Provision = MobileProvision;
                if (!string.IsNullOrEmpty(SigningCertificate))
                {
                    // verify the certificate
                    Process IPPProcess = new Process();
                    if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
                    {
                        string IPPCmd = "\"" + UnrealBuildTool.EngineDirectory + "/Binaries/DotNET/IOS/IPhonePackager.exe\" certificates " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + BundleId + (bForDistribtion ? " -distribution" : "");
                        IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                        IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "/Build/BatchFiles/Mac/RunMono.sh";
                        IPPProcess.StartInfo.Arguments = IPPCmd;
                        IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                        IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    }
                    else
                    {
                        string IPPCmd = "certificates " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + BundleId + (bForDistribtion ? " -distribution" : "");
                        IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                        IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "\\Binaries\\DotNET\\IOS\\IPhonePackager.exe";
                        IPPProcess.StartInfo.Arguments = IPPCmd;
                        IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                        IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    }
                    Utils.RunLocalProcess(IPPProcess);
                }
                else
                {
                    Certificate = bForDistribtion ? "iPhone Distribution" : "iPhone Developer";
                    bHaveCertificate = true;
                }

                if (string.IsNullOrEmpty(MobileProvision) // no provision specified
                    || !File.Exists((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac ? (Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/") : (Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "/Apple Computer/MobileDevice/Provisioning Profiles/")) + MobileProvision) // file doesn't exist
                    || !bHaveCertificate) // certificate doesn't exist
                {

                    Certificate = "";
                    Provision = "";
                    Log.TraceLog("Provision not specified or not found for " + ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game") + ", searching for compatible match...");
                    Process IPPProcess = new Process();
                    if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
                    {
                        string IPPCmd = "\"" + UnrealBuildTool.EngineDirectory + "/Binaries/DotNET/IOS/IPhonePackager.exe\" signing_match " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + BundleId + (bIsTVOS ? " -tvos" : "") + (bForDistribtion ? " -distribution" : "");
                        IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                        IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "/Build/BatchFiles/Mac/RunMono.sh";
                        IPPProcess.StartInfo.Arguments = IPPCmd;
                        IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                        IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    }
                    else
                    {
                        string IPPCmd = "signing_match " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + BundleId + (bIsTVOS ? " -tvos" : "") + (bForDistribtion ? " -distribution" : "");
                        IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                        IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "\\Binaries\\DotNET\\IOS\\IPhonePackager.exe";
                        IPPProcess.StartInfo.Arguments = IPPCmd;
                        IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                        IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    }
                    Utils.RunLocalProcess(IPPProcess);
                    Log.TraceLog("Provision found for " + ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game") + ", Provision: " + Provision + " Certificate: " + Certificate);
                }
                // add to the dictionary
                Data.MobileProvision = Provision;
                Data.Certificate = Certificate.Replace("\"", "");

                // read the provision to get the UUID
                string filename = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac ? (Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/") : (Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "/Apple Computer/MobileDevice/Provisioning Profiles/")) + Data.MobileProvision;
                if (File.Exists(filename))
                {
                    string AllText = File.ReadAllText(filename);
                    int idx = AllText.IndexOf("<key>UUID</key>");
                    if (idx > 0)
                    {
                        idx = AllText.IndexOf("<string>", idx);
                        if (idx > 0)
                        {
                            idx += "<string>".Length;
                            Data.UUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
                        }
                    }
                    idx = AllText.IndexOf("<key>com.apple.developer.team-identifier</key>");
                    if (idx > 0)
                    {
                        idx = AllText.IndexOf("<string>", idx);
                        if (idx > 0)
                        {
                            idx += "<string>".Length;
                            Data.TeamUUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
                        }
                    }
                }
                else
                {
                    Log.TraceLog("No matching provision file was discovered. Please ensure you have a compatible provision installed.");
                }
                ProvisionCache.Add(BundleId + " " + bIsTVOS.ToString() + " " + bForDistribtion.ToString(), Data);
            }
            else
            {
                Data = ProvisionCache[BundleId + " " + bIsTVOS.ToString() + " " + bForDistribtion.ToString()];
            }
            MobileProvision = Data.MobileProvision;
            SigningCertificate = Data.Certificate;
            MobileProvisionUUID = Data.UUID;
            TeamUUID = Data.TeamUUID;
        }

        /// <summary>
        /// Whether this platform should create debug information or not
        /// </summary>
        /// <param name="Configuration"> The UnrealTargetConfiguration being built</param>
        /// <returns>bool    true if debug info should be generated, false if not</returns>
        public override bool ShouldCreateDebugInfo(UnrealTargetConfiguration Configuration)
		{
			return true;
		}

		public override void ResetBuildConfiguration(UnrealTargetConfiguration Configuration)
		{
			UEBuildConfiguration.bBuildEditor = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;
			UEBuildConfiguration.bCompileAPEX = false;
			UEBuildConfiguration.bRuntimePhysicsCooking = false;
			UEBuildConfiguration.bCompileSimplygon = false;
            UEBuildConfiguration.bCompileSimplygonSSF = false;
			UEBuildConfiguration.bBuildDeveloperTools = false;

			// we currently don't have any simulator libs for PhysX
			if (GetActiveArchitecture() == "-simulator")
			{
				UEBuildConfiguration.bCompilePhysX = false;
			}
		}

		public override void ValidateBuildConfiguration(CPPTargetConfiguration Configuration, CPPTargetPlatform Platform, bool bCreateDebugInfo)
		{
			// check the base first
			base.ValidateBuildConfiguration(Configuration, Platform, bCreateDebugInfo);

			BuildConfiguration.bUsePCHFiles = false;
			BuildConfiguration.bCheckExternalHeadersForModification = false;
			BuildConfiguration.bCheckSystemHeadersForModification = false;
			BuildConfiguration.ProcessorCountMultiplier = IOSToolChain.GetAdjustedProcessorCountMultiplier();
			BuildConfiguration.bDeployAfterCompile = true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="InBuildTarget"> The target being built</param>
		public override void SetUpEnvironment(UEBuildTarget InBuildTarget)
		{
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_IOS=1");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("PLATFORM_APPLE=1");

			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_TTS=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_EDITOR=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("USE_NULL_RHI=0");
			InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("REQUIRES_ALIGNED_INT_ACCESS");

            ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.IOS, "Engine", DirectoryReference.FromFile(ProjectFile));
            Ini.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableRemoteNotificationsSupport", out bNotificationsEnabled);
            if (bNotificationsEnabled)
            {
                InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("NOTIFICATIONS_ENABLED");
            }

			if (GetActiveArchitecture() == "-simulator")
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("WITH_SIMULATOR=1");
			}

			// we assume now we are building with IOS8 or later
			if (UEBuildConfiguration.bCompileAgainstEngine)
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=1");
				InBuildTarget.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				InBuildTarget.GlobalCompileEnvironment.Config.Definitions.Add("HAS_METAL=0");
			}

			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add(new UEBuildFramework("GameKit"));
			InBuildTarget.GlobalLinkEnvironment.Config.AdditionalFrameworks.Add(new UEBuildFramework("StoreKit"));
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Platform">The platform to create a toolchain for</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CPPTargetPlatform Platform)
		{
			return new IOSToolChain(ProjectFile, this);
		}

		/// <summary>
		/// Create a build deployment handler
		/// </summary>
		/// <param name="ProjectFile">The project file of the target being deployed. Used to find any deployment specific settings.</param>
		/// <param name="DeploymentHandler">The output deployment handler</param>
		/// <returns>True if the platform requires a deployment handler, false otherwise</returns>
		public override UEBuildDeploy CreateDeploymentHandler()
		{
			return new UEDeployIOS(ProjectFile, this);
		}
	}

	public class IOSPlatform : UEBuildPlatform
	{
		IOSPlatformSDK SDK;

		public IOSPlatform(IOSPlatformSDK InSDK)
			: this(InSDK, UnrealTargetPlatform.IOS, CPPTargetPlatform.IOS)
		{
		}

		protected IOSPlatform(IOSPlatformSDK InSDK, UnrealTargetPlatform TargetPlatform, CPPTargetPlatform CPPPlatform)
			: base(TargetPlatform, CPPPlatform)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public override string GetDebugInfoExtension(UEBuildBinaryType InBinaryType)
		{
			if(BuildConfiguration.bGeneratedSYMFile)
			{
				return ".dSYM";
			}

			if(BuildConfiguration.bGeneratedSYMBundle)
			{
				return ".dSYM.zip";
			}

			return "";
		}

		public override bool CanUseXGE()
		{
			return false;
		}

		public override bool CanUseDistcc()
		{
			return true;
		}


		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bDevForArmV7", "bDevForArm64", "bDevForArmV7S", "bShipForArmV7", 
				"bShipForArm64", "bShipForArmV7S", "bShipForBitcode", "bGeneratedSYMFile",
				"bGeneratedSYMBundle", "bEnableRemoteNotificationsSupport"
			};
			string[] StringKeys = new string[] {
				"MinimumiOSVersion", 
				"AdditionalLinkerFlags",
				"AdditionalShippingLinkerFlags"
			};

			// look up iOS specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/IOSRuntimeSettings.IOSRuntimeSettings",
					BoolKeys, null, StringKeys))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectDirectoryName);
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true; // for iOS can only run cooked. this is mostly for testing console code paths.
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		/// <summary>
		/// Whether the platform requires the extra UnityCPPWriter
		/// This is used to add an extra file for UBT to get the #include dependencies from
		/// </summary>
		/// <returns>bool true if it is required, false if not</returns>
		public override bool RequiresExtraUnityCPPWriter()
		{
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, TargetInfo Target)
		{
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				bool bBuildShaderFormats = UEBuildConfiguration.bForceBuildShaderFormats;
				if (!UEBuildConfiguration.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (UEBuildConfiguration.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						if (UEBuildConfiguration.bBuildDeveloperTools && UEBuildConfiguration.bCompileAgainstEngine)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (UEBuildConfiguration.bForceBuildTargetPlatforms)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("MetalShaderFormat");
					}
				}
			}
		}

		/// <summary>
		/// Creates a context for the given project on the current platform.
		/// </summary>
		/// <param name="ProjectFile">The project file for the current target</param>
		/// <returns>New platform context object</returns>
		public override UEBuildPlatformContext CreateContext(FileReference ProjectFile)
		{
			return new IOSPlatformContext(ProjectFile);
		}
	}

	public class IOSPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			if (!Utils.IsRunningOnMono)
			{
				// check to see if iTunes is installed
				string dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "iTunesMobileDeviceDLL", null) as string;
				if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
				{
					dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "MobileDeviceDLL", null) as string;
					if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
					{
						return SDKStatus.Invalid;
					}
				}
			}
			return SDKStatus.Valid;
		}
	}

	public class IOSPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.IOS; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms()
		{
			IOSPlatformSDK SDK = new IOSPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for IOS
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.IOS.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new IOSPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Apple);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.IOS);

			if (IOSPlatformContext.IOSArchitecture == "-simulator")
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Simulator);
			}
			else
			{
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Device);
			}
		}
	}
}

