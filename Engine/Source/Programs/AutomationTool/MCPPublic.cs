// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using System.Runtime.Serialization;
using System.Net;
using System.Reflection;
using System.Text.RegularExpressions;
using UnrealBuildTool;

namespace EpicGames.MCP.Automation
{
	using EpicGames.MCP.Config;

    /// <summary>
    /// Utility class to provide commit/rollback functionality via an RAII-like functionality.
    /// Usage is to provide a rollback action that will be called on Dispose if the Commit() method is not called.
    /// This is expected to be used from within a using() ... clause.
    /// </summary>
    public class CommitRollbackTransaction : IDisposable
    {
        /// <summary>
        /// Track whether the transaction will be committed.
        /// </summary>
        private bool IsCommitted = false;

        /// <summary>
        /// 
        /// </summary>
        private System.Action RollbackAction;

        /// <summary>
        /// Call when you want to commit your transaction. Ensures the Rollback action is not called on Dispose().
        /// </summary>
        public void Commit()
        {
            IsCommitted = true;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="RollbackAction">Action to be executed to rollback the transaction.</param>
        public CommitRollbackTransaction(System.Action InRollbackAction)
        {
            RollbackAction = InRollbackAction;
        }

        /// <summary>
        /// Rollback the transaction if its not committed on Dispose.
        /// </summary>
        public void Dispose()
        {
            if (!IsCommitted)
            {
                RollbackAction();
            }
        }
    }

    /// <summary>
    /// Enum that defines the MCP backend-compatible platform
    /// </summary>
    public enum MCPPlatform
    {
        /// <summary>
        /// MCP doesn't care about Win32 vs. Win64
        /// </summary>
        Windows,

        /// <summary>
        /// Only other platform MCP understands is Mac.
        /// </summary>
        Mac,
    }

    /// <summary>
    /// Enum that defines CDN types
    /// </summary>
    public enum CDNType
    {
        /// <summary>
        /// Internal HTTP CDN server
        /// </summary>
        Internal,

        /// <summary>
        /// Production HTTP CDN server
        /// </summary>
        Production,
    }

    /// <summary>
    /// Class that holds common state used to control the BuildPatchTool build commands that chunk and create patching manifests and publish build info to the BuildInfoService.
    /// </summary>
    public class BuildPatchToolStagingInfo
    {
        /// <summary>
        /// The currently running command, used to get command line overrides
        /// </summary>
        public BuildCommand OwnerCommand;
        /// <summary>
        /// name of the app. Can't always use this to define the staging dir because some apps are not staged to a place that matches their AppName.
        /// </summary>
        public readonly string AppName;
        /// <summary>
        /// Usually the base name of the app. Used to get the MCP key from a branch dictionary. 
        /// </summary>
        public readonly string McpConfigKey;
        /// <summary>
        /// ID of the app (needed for the BuildPatchTool)
        /// </summary>
        public readonly int AppID;
        /// <summary>
        /// BuildVersion of the App we are staging.
        /// </summary>
        public readonly string BuildVersion;
        /// <summary>
        /// Directory where builds will be staged. Rooted at the BuildRootPath, using a subfolder passed in the ctor, 
        /// and using BuildVersion/PlatformName to give each builds their own home.
        /// </summary>
        public readonly string StagingDir;
        /// <summary>
        /// Path to the CloudDir where chunks will be written (relative to the BuildRootPath)
        /// This is used to copy to the web server, so it can use the same relative path to the root build directory.
        /// This allows file to be either copied from the local file system or the webserver using the same relative paths.
        /// </summary>
        public readonly string CloudDirRelativePath;
        /// <summary>
        /// full path to the CloudDir where chunks and manifests should be staged. Rooted at the BuildRootPath, using a subfolder pass in the ctor.
        /// </summary>
        public readonly string CloudDir;
        /// <summary>
        /// Platform we are staging for.
        /// </summary>
        public readonly MCPPlatform Platform;

        /// <summary>
        /// Gets the base filename of the manifest that would be created by invoking the BuildPatchTool with the given parameters.
        /// </summary>
        public virtual string ManifestFilename
        {
            get
            {
                var BaseFilename = AppName + BuildVersion + "-" + Platform.ToString() + ".manifest";
                return Regex.Replace(BaseFilename, @"\s+", ""); // Strip out whitespace in order to be compatible with BuildPatchTool
            }
        }

        /// <summary>
        /// Determine the platform name (Win32/64 becomes Windows, Mac is Mac, the rest we don't currently understand)
        /// </summary>
        static public MCPPlatform ToMCPPlatform(UnrealTargetPlatform TargetPlatform)
        {
            if (TargetPlatform != UnrealTargetPlatform.Win64 && TargetPlatform != UnrealTargetPlatform.Win32 && TargetPlatform != UnrealTargetPlatform.Mac)
            {
                throw new AutomationException("Platform {0} is not properly supported by the MCP backend yet", TargetPlatform);
            }
            return (TargetPlatform == UnrealTargetPlatform.Win64 || TargetPlatform == UnrealTargetPlatform.Win32) ? MCPPlatform.Windows : MCPPlatform.Mac;
        }
        /// <summary>
        /// Determine the platform name (Win32/64 becomes Windows, Mac is Mac, the rest we don't currently understand)
        /// </summary>
        static public UnrealTargetPlatform FromMCPPlatform(MCPPlatform TargetPlatform)
        {
            if (TargetPlatform != MCPPlatform.Windows && TargetPlatform != MCPPlatform.Mac)
            {
                throw new AutomationException("Platform {0} is not properly supported by the MCP backend yet", TargetPlatform);
            }
            return (TargetPlatform == MCPPlatform.Windows) ? UnrealTargetPlatform.Win64 : UnrealTargetPlatform.Mac;
        }
        /// <summary>
        /// Returns the build root path (P:\Builds on build machines usually)
        /// </summary>
        /// <returns></returns>
        static public string GetBuildRootPath()
        {
            return CommandUtils.P4Enabled && CommandUtils.AllowSubmit
                ? CommandUtils.RootSharedTempStorageDirectory()
                : CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "LocalBuilds");
        }

        /// <summary>
        /// Basic constructor. 
        /// </summary>
        /// <param name="InAppName"></param>
        /// <param name="InAppID"></param>
        /// <param name="InBuildVersion"></param>
        /// <param name="platform"></param>
        /// <param name="stagingDirRelativePath">Relative path from the BuildRootPath where files will be staged. Commonly matches the AppName.</param>
        public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, UnrealTargetPlatform platform, string stagingDirRelativePath)
            : this(InOwnerCommand, InAppName, InMcpConfigKey, InAppID, InBuildVersion, ToMCPPlatform(platform), stagingDirRelativePath)
        {
        }

        /// <summary>
        /// Basic constructor. 
        /// </summary>
        /// <param name="InAppName"></param>
        /// <param name="InAppID"></param>
        /// <param name="InBuildVersion"></param>
        /// <param name="platform"></param>
        /// <param name="stagingDirRelativePath">Relative path from the BuildRootPath where files will be staged. Commonly matches the AppName.</param>
        public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, MCPPlatform platform, string stagingDirRelativePath)
        {
            OwnerCommand = InOwnerCommand;
            AppName = InAppName;
            McpConfigKey = InMcpConfigKey;
            AppID = InAppID;
            BuildVersion = InBuildVersion;
            Platform = platform;
            var BuildRootPath = GetBuildRootPath();
            StagingDir = CommandUtils.CombinePaths(BuildRootPath, stagingDirRelativePath, BuildVersion, Platform.ToString());
            CloudDirRelativePath = CommandUtils.CombinePaths(stagingDirRelativePath, "CloudDir");
            CloudDir = CommandUtils.CombinePaths(BuildRootPath, CloudDirRelativePath);
        }

		/// <summary>
		/// Basic constructor with staging dir suffix override, basically to avoid having platform concatenated
		/// </summary>
		public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, UnrealTargetPlatform platform, string stagingDirRelativePath, string stagingDirSuffix)
			: this(InOwnerCommand, InAppName, InMcpConfigKey, InAppID, InBuildVersion, ToMCPPlatform(platform), stagingDirRelativePath, stagingDirSuffix)
		{
		}

		/// <summary>
		/// Basic constructor with staging dir suffix override, basically to avoid having platform concatenated
		/// </summary>
		public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, MCPPlatform platform, string stagingDirRelativePath, string stagingDirSuffix)
		{
			OwnerCommand = InOwnerCommand;
			AppName = InAppName;
			McpConfigKey = InMcpConfigKey;
			AppID = InAppID;
			BuildVersion = InBuildVersion;
			Platform = platform;
			var BuildRootPath = GetBuildRootPath();
			StagingDir = CommandUtils.CombinePaths(BuildRootPath, stagingDirRelativePath, BuildVersion, stagingDirSuffix);
			CloudDirRelativePath = CommandUtils.CombinePaths(stagingDirRelativePath, "CloudDir");
			CloudDir = CommandUtils.CombinePaths(BuildRootPath, CloudDirRelativePath);
		}
    }

    /// <summary>
    /// Class that provides programmatic access to the BuildPatchTool
    /// </summary>
    public abstract class BuildPatchToolBase
    {
        /// <summary>
        /// Controls the chunking type used by the buildinfo server (nochunks parameter)
        /// </summary>
        public enum ChunkType
        {
            /// <summary>
            /// Chunk the files
            /// </summary>
            Chunk,
            /// <summary>
            /// Don't chunk the files, just build a file manifest.
            /// </summary>
            NoChunk,
        }


        public class PatchGenerationOptions
        {
            /// <summary>
            /// By default, we will only consider data modified within 12 days to be reusable
            /// </summary>
            public const int DEFAULT_DATA_AGE_THRESHOLD = 12;

            public PatchGenerationOptions()
            {
                DataAgeThreshold = DEFAULT_DATA_AGE_THRESHOLD;
            }

            /// <summary>
            /// Staging information
            /// </summary>
            public BuildPatchToolStagingInfo StagingInfo;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string BuildRoot;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string FileIgnoreList;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string FileAttributeList;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string AppLaunchCmd;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string AppLaunchCmdArgs;
            /// <summary>
            /// Corresponds to the -nochunks parameter
            /// </summary>
            public ChunkType AppChunkType;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public MCPPlatform Platform;
            /// <summary>
            /// When identifying existing patch data to reuse in this build, only
            /// files modified within this number of days will be considered for reuse.
            /// IMPORTANT: This should always be smaller than the data age threshold for any compactify process which will run on the directory, to ensure
            /// that we do not reuse any files which could be deleted by a concurrently running compactify. It is recommended that this number be at least
            /// two days less than the compactify data age threshold.
            /// </summary>
            public int DataAgeThreshold;
			/// <summary>
			/// Contains a list of custom string arguments to be embedded in the generated manifest file.
			/// </summary>
			public List<KeyValuePair<string, string>> CustomStringArgs;
			/// <summary>
			/// Contains a list of custom integer arguments to be embedded in the generated manifest file.
			/// </summary>
			public List<KeyValuePair<string, int>> CustomIntArgs;
			/// <summary>
			/// Contains a list of custom float arguments to be embedded in the generated manifest file.
			/// </summary>
			public List<KeyValuePair<string, float>> CustomFloatArgs;
		}

        static BuildPatchToolBase Handler = null;

        public static BuildPatchToolBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(BuildPatchToolBase) && typeof(BuildPatchToolBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as BuildPatchToolBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use BuildPatchToolBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }

        /// <summary>
        /// Runs the Build Patch Tool executable to generate patch data using the supplied parameters.
        /// </summary>
		/// <param name="Opts">Parameters which will be passed to the patch tool generation process</param>
		public abstract void Execute(PatchGenerationOptions Opts);

    }


    /// <summary>
    /// Helper class
    /// </summary>
    public abstract class BuildInfoPublisherBase
    {
        static BuildInfoPublisherBase Handler = null;
 
        public static BuildInfoPublisherBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(BuildInfoPublisherBase) && typeof(BuildInfoPublisherBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as BuildInfoPublisherBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use BuildInfoPublisherBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }
        /// <summary>
        /// Given a MCPStagingInfo defining our build info, posts the build to the MCP BuildInfo Service.
        /// </summary>
        /// <param name="stagingInfo">Staging Info describing the BuildInfo to post.</param>
        abstract public void PostBuildInfo(BuildPatchToolStagingInfo stagingInfo);

		/// <summary>
		/// Given a MCPStagingInfo defining our build info and a MCP config name, posts the build to the requested MCP BuildInfo Service.
		/// </summary>
		/// <param name="StagingInfo">Staging Info describing the BuildInfo to post.</param>
		/// <param name="McpConfigName">Name of which MCP config to post to.</param>
		abstract public void PostBuildInfo(BuildPatchToolStagingInfo StagingInfo, string McpConfigName);

		/// <summary>
		/// Given a BuildVersion defining our a build, return the labels applied to that build
		/// </summary>
		/// <param name="BuildVersion">Build version to return labels for.</param>
		/// <param name="McpConfigName">Which BuildInfo backend to get labels from for this promotion attempt.</param>
		abstract public List<string> GetBuildLabels(BuildPatchToolStagingInfo StagingInfo, string McpConfigName);

		/// <summary>
		/// Get a label string for the specific Platform requested.
		/// </summary>
		/// <param name="DestinationLabel">Base of label</param>
		/// <param name="Platform">Platform to add to base label.</param>
		abstract public string GetLabelWithPlatform(string DestinationLabel, MCPPlatform Platform);

		/// <summary>
		/// Get a BuildVersion string with the Platform concatenated on.
		/// </summary>
		/// <param name="DestinationLabel">Base of label</param>
		/// <param name="Platform">Platform to add to base label.</param>
		abstract public string GetBuildVersionWithPlatform(BuildPatchToolStagingInfo StagingInfo);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="AppName">Application name to check the label in</param>
		/// <param name="LabelName">Label name to get the build for</param>
		/// <param name="McpConfigName">Which BuildInfo backend to label the build in.</param>
		/// <returns></returns>
		abstract public string GetLabeledBuildVersion(string AppName, string LabelName, string McpConfigName);

		/// <summary>
		/// Apply the requested label to the requested build in the BuildInfo backend for the requested MCP environment
		/// </summary>
		/// <param name="StagingInfo">Staging info for the build to label.</param>
		/// <param name="DestinationLabelWithPlatform">Label, including platform, to apply</param>
		/// <param name="McpConfigName">Which BuildInfo backend to label the build in.</param>
		abstract public void LabelBuild(BuildPatchToolStagingInfo StagingInfo, string DestinationLabelWithPlatform, string McpConfigName);

        /// <summary>
        /// Informs Patcher Service of a new build availability after async labeling is complete
        /// (this usually means the build was copied to a public file server before the label could be applied).
        /// </summary>
        /// <param name="Command">Parent command</param>
        /// <param name="AppName">Application name that the patcher service will use.</param>
        /// <param name="BuildVersion">BuildVersion string that the patcher service will use.</param>
        /// <param name="ManifestRelativePath">Relative path to the Manifest file relative to the global build root (which is like P:\Builds) </param>
        /// <param name="LabelName">Name of the label that we will be setting.</param>
        abstract public void BuildPromotionCompleted(BuildPatchToolStagingInfo stagingInfo, string AppName, string BuildVersion, string ManifestRelativePath, string PlatformName, string LabelName);

        /// <summary>
        /// Mounts the production CDN share (allows overriding via -CDNDrive command line arg)
        /// </summary>
        /// <param name="command"></param>
        /// <returns>Path to the share (allows for override)</returns>
        abstract public string MountProductionCDNShare();

        /// <summary>
        /// Mounts the production CDN share (allows overriding via -CDNDrive command line arg)
        /// </summary>
        /// <param name="command"></param>
        /// <returns>Path to the share (allows for override)</returns>
        abstract public string MountInternalCDNShare();

		/// <summary>
		/// Checks for a stagingInfo's manifest on the production CDN.
		/// </summary>
		/// <param name="stagingInfo">Staging info used to determine where the chunks are to copy.</param>
		abstract public bool IsManifestOnProductionCDN(BuildPatchToolStagingInfo stagingInfo);

        /// <summary>
        /// Copies chunks from a staged location to the production CDN.
        /// </summary>
        /// <param name="command">Build command (used to allow the -CDNDrive cmdline override).</param>
        /// <param name="stagingInfo">Staging info used to determine where the chunks are to copy.</param>
        abstract public void CopyChunksToProductionCDN(BuildPatchToolStagingInfo stagingInfo);

        /// <summary>
        /// Copies chunks from a staged location to the production CDN.
        /// NOTE: This code assumes the location of the BuildRootPath at the time this build 
        /// by calling <see cref="BuildPatchToolStagingInfo.GetBuildRootPath"/> (usually P:\Builds).
        /// If this path changes then this code posting older builds will break because we won't know
        /// where the BuildRootPath for the older build was!
        /// </summary>
        /// <param name="command">Build command (used to allow the -CDNDrive cmdline override).</param>
        /// <param name="manifestUrlPath">relative path to the manifest file from the build info service</param>
        abstract public void CopyChunksToProductionCDN(string manifestUrlPath);
	}
    /// <summary>
    /// Helpers for using the MCP account service
    /// </summary>
    public abstract class McpAccountServiceBase
    {
        static McpAccountServiceBase Handler = null;

        public static McpAccountServiceBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(McpAccountServiceBase) && typeof(McpAccountServiceBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as McpAccountServiceBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use McpAccountServiceBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }
        public abstract string GetClientToken(BuildPatchToolStagingInfo StagingInfo);
		public abstract string GetClientToken(McpConfigData McpConfig);
        public abstract string SendWebRequest(WebRequest Upload, string Method, string ContentType, byte[] Data);
    }

}

namespace EpicGames.MCP.Config
{
    /// <summary>
    /// Class for retrieving MCP configuration data
    /// </summary>
    public class McpConfigHelper
    {
        // List of configs is cached off for fetching from multiple times
        private static Dictionary<string, McpConfigData> Configs;

        public static McpConfigData Find(string ConfigName)
        {
            if (Configs == null)
            {
                // Load all secret configs by trying to instantiate all classes derived from McpConfig from all loaded DLLs.
                // Note that we're using the default constructor on the secret config types.
                Configs = new Dictionary<string, McpConfigData>();
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(McpConfigData) && typeof(McpConfigData).IsAssignableFrom(PotentialConfigType))
                        {
                            try
                            {
                                McpConfigData Config = Activator.CreateInstance(PotentialConfigType) as McpConfigData;
                                if (Config != null)
                                {
                                    Configs.Add(Config.Name, Config);
                                }
                            }
                            catch
                            {
                                BuildCommand.LogWarning("Unable to create McpConfig: {0}", PotentialConfigType.Name);
                            }
                        }
                    }
                }
            }
            McpConfigData LoadedConfig;
            Configs.TryGetValue(ConfigName, out LoadedConfig);
            if (LoadedConfig == null)
            {
                throw new AutomationException("Unable to find requested McpConfig: {0}", ConfigName);
            }
            return LoadedConfig;
        }
    }

    // Class for storing mcp configuration data
    public class McpConfigData
    {
		public McpConfigData(string InName, string InAccountBaseUrl, string InFortniteBaseUrl, string InLauncherBaseUrl, string InBuildInfoV2BaseUrl, string InLauncherV2BaseUrl, string InClientId, string InClientSecret)
        {
            Name = InName;
            AccountBaseUrl = InAccountBaseUrl;
            FortniteBaseUrl = InFortniteBaseUrl;
            LauncherBaseUrl = InLauncherBaseUrl;
			BuildInfoV2BaseUrl = InBuildInfoV2BaseUrl;
			LauncherV2BaseUrl = InLauncherV2BaseUrl;
            ClientId = InClientId;
            ClientSecret = InClientSecret;
        }

        public string Name;
        public string AccountBaseUrl;
        public string FortniteBaseUrl;
        public string LauncherBaseUrl;
		public string BuildInfoV2BaseUrl;
		public string LauncherV2BaseUrl;
        public string ClientId;
        public string ClientSecret;

        public void SpewValues()
        {
            CommandUtils.Log("Name : {0}", Name);
            CommandUtils.Log("AccountBaseUrl : {0}", AccountBaseUrl);
            CommandUtils.Log("FortniteBaseUrl : {0}", FortniteBaseUrl);
            CommandUtils.Log("LauncherBaseUrl : {0}", LauncherBaseUrl);
			CommandUtils.Log("BuildInfoV2BaseUrl : {0}", BuildInfoV2BaseUrl);
			CommandUtils.Log("LauncherV2BaseUrl : {0}", LauncherV2BaseUrl);
            CommandUtils.Log("ClientId : {0}", ClientId);
            // we don't really want this in logs CommandUtils.Log("ClientSecret : {0}", ClientSecret);
        }
    }

    public class McpConfigMapper
    {
        static public McpConfigData FromMcpConfigKey(string McpConfigKey)
        {
            return McpConfigHelper.Find("MainGameDevNet");
        }

        static public McpConfigData FromStagingInfo(EpicGames.MCP.Automation.BuildPatchToolStagingInfo StagingInfo)
        {
            string McpConfigNameToLookup = null;
            if (StagingInfo.OwnerCommand != null)
            {
                McpConfigNameToLookup = StagingInfo.OwnerCommand.ParseParamValue("MCPConfig");
            }
            if (String.IsNullOrEmpty(McpConfigNameToLookup))
            {
                return FromMcpConfigKey(StagingInfo.McpConfigKey);
            }
            return McpConfigHelper.Find(McpConfigNameToLookup);
        }
    }

}
