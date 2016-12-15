// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using System.Net;
using System.Reflection;
using System.Web.Script.Serialization;
using EpicGames.MCP.Automation;
using EpicGames.MCP.Config;
using System.Net.Http;
using System.Security.Cryptography;

namespace UnrealTournamentGame.Automation
{
	public class UnrealTournamentBuild
	{
		// Use old-style UAT version for UnrealTournament
		// TODO this should probably use the new Engine Version stuff.
		public static string CreateBuildVersion()
		{
			string P4Change = "UnknownCL";
			string P4Branch = "UnknownBranch";
			if (CommandUtils.P4Enabled)
			{
				P4Change = CommandUtils.P4Env.ChangelistString;
				P4Branch = CommandUtils.P4Env.BuildRootEscaped;
			}
			return P4Branch + "-CL-" + P4Change;
		}


		public static UnrealTournamentAppName BranchNameToAppName(string BranchName)
		{
			return UnrealTournamentAppName.UnrealTournamentBuilds;
		}

		/// <summary>
		/// Enum that defines the MCP backend-compatible platform
		/// </summary>
		public enum UnrealTournamentAppName
		{
			// Dev and release branch game builds source app
			UnrealTournamentBuilds,
            UnrealTournamentReleaseBuilds,

            // Dev branch app in gamedev
            UnrealTournamentDevTesting,
			UnrealTournamentDevStage,
			UnrealTournamentDevPlaytest,

			// Release branch promotions
			UnrealTournamentReleaseTesting,
			UnrealTournamentReleaseStage,
			UnrealTournamentPublicTest,

			/// Live app, displays in Launcher as "UnrealTournament"
			UnrealTournamentDev,
		}


		public static UnrealTournamentEditorAppName EditorBranchNameToAppName(string BranchName)
		{
			return UnrealTournamentEditorAppName.UnrealTournamentEditorBuilds;
		}

		/// <summary>
		/// Enum that defines the MCP backend-compatible platform
		/// </summary>
		public enum UnrealTournamentEditorAppName
		{
			// Dev and release branch editor builds source app
			UnrealTournamentEditorBuilds,

            // Dev branch promotions
            UnrealTournamentEditorDevTesting,
			UnrealTournamentEditorDevStage,
			UnrealTournamentEditorDevPlaytest,

			// Release branch promotions
			UnrealTournamentEditorReleaseTesting,
			UnrealTournamentEditorReleaseStage,
			UnrealTournamentEditorPublicTest,

			/// Live branch promotions
			UnrealTournamentEditor
		}



		/// GAME VERSIONS OF BUILDPATCHTOOLSTAGINGINFOS ///

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentAppName AppName, string ManifestFilename = null)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
		}

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName, string ManifestFilename = null)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
		}

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, UnrealTargetPlatform InPlatform, string BranchName)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, BranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
		}

		// Get a basic StagingInfo based on buildversion of command currently running
		public static BuildPatchToolStagingInfo GetUTBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, UnrealTargetPlatform InPlatform, string BranchName)
		{
			return GetUTBuildPatchToolStagingInfo(InOwnerCommand, CreateBuildVersion(), InPlatform, BranchName);
		}



		/// EDITOR VERSIONS OF BUILDPATCHTOOLSTAGINGINFOS ///

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, UnrealTournamentEditorAppName AppName, string ManifestFilename = null)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, AppName.ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
		}

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, MCPPlatform InPlatform, string BranchName, string ManifestFilename = null)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, EditorBranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament", "", ManifestFilename);
		}

		// Construct a buildpatchtool for a given buildversion that may be unrelated to the executing UAT job
		public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string BuildVersion, UnrealTargetPlatform InPlatform, string BranchName)
		{
			// Pass in a blank staging dir suffix in place of platform, we want \WindowsClient not \Windows\WindowsClient
			return new BuildPatchToolStagingInfo(InOwnerCommand, EditorBranchNameToAppName(BranchName).ToString(), "McpConfigUnused", 1, BuildVersion, InPlatform, "UnrealTournament");
		}

		// Get a basic StagingInfo based on buildversion of command currently running
		public static BuildPatchToolStagingInfo GetUTEditorBuildPatchToolStagingInfo(BuildCommand InOwnerCommand, UnrealTargetPlatform InPlatform, string BranchName)
		{
			return GetUTEditorBuildPatchToolStagingInfo(InOwnerCommand, CreateBuildVersion(), InPlatform, BranchName);
		}

		public static string GetArchiveDir()
		{
			return CommandUtils.CombinePaths(BuildPatchToolStagingInfo.GetBuildRootPath(), "UnrealTournament", CreateBuildVersion());
		}

		public static void Tweet(string InTweet)
		{
			string CredentialsPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "UnrealTournament", "Build", "NotForLicensees", "twittercredentials.txt");
			if (!CommandUtils.FileExists(CredentialsPath))
			{
				CommandUtils.Log("No twitter credentials found!");
				return;
			}

			JsonObject TwitterCreds;
			if (!JsonObject.TryRead(CredentialsPath, out TwitterCreds))
			{
				CommandUtils.Log("No twitter credentials found!");
				return;
			}

			string TwitterConsumerKey;
			string TwitterConsumerSecret;
			string TwitterAccessTokenSecret;
			string TwitterAccessToken;

			if (!TwitterCreds.TryGetStringField("consumerkey", out TwitterConsumerKey) ||
				!TwitterCreds.TryGetStringField("consumersecret", out TwitterConsumerSecret) ||
				!TwitterCreds.TryGetStringField("accesstokensecret", out TwitterAccessTokenSecret) ||
				!TwitterCreds.TryGetStringField("accesstoken", out TwitterAccessToken))
			{
				CommandUtils.Log("Invalid twitter credentials found!");
				return;
			}

			string TwitterBaseUrl = "https://api.twitter.com/1.1/statuses/update.json";
			string TwitterData = "status=" + InTweet;
			string TwitterMethod = "POST";

			string oAuthNonce = Convert.ToBase64String(new ASCIIEncoding().GetBytes(DateTime.Now.Ticks.ToString()));
			string oAuthTimestamp = Convert.ToInt64((DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc)).TotalSeconds).ToString();
			string oAuthVersion = "1.0";
			string oAuthSignatureMethod = "HMAC-SHA1";

			string oAuthFormat = "oauth_consumer_key={0}&oauth_nonce={1}&oauth_signature_method={2}&oauth_timestamp={3}&oauth_token={4}&oauth_version={5}";
			string oAuthString = string.Format(oAuthFormat, TwitterConsumerKey, oAuthNonce, oAuthSignatureMethod, oAuthTimestamp, TwitterAccessToken, oAuthVersion);

			string BaseString = string.Concat(TwitterMethod, "&", Uri.EscapeDataString(TwitterBaseUrl), "&", Uri.EscapeDataString(oAuthString), Uri.EscapeDataString("&"), Uri.EscapeDataString(TwitterData));

			string CompositeKey = string.Concat(Uri.EscapeDataString(TwitterConsumerSecret), "&", Uri.EscapeDataString(TwitterAccessTokenSecret));

			string oAuthSignature;
			using (HMACSHA1 hasher = new HMACSHA1(ASCIIEncoding.ASCII.GetBytes(CompositeKey)))
			{
				oAuthSignature = Convert.ToBase64String(hasher.ComputeHash(ASCIIEncoding.ASCII.GetBytes(BaseString)));
			}

			string HeaderFormat = "OAuth oauth_consumer_key=\"{0}\", oauth_nonce=\"{1}\", oauth_signature=\"{2}\", oauth_signature_method=\"{3}\", " +
								  "oauth_timestamp=\"{4}\", oauth_token=\"{5}\", oauth_version=\"{6}\"";

			string oAuthHeader = string.Format(HeaderFormat, Uri.EscapeDataString(TwitterConsumerKey), Uri.EscapeDataString(oAuthNonce), Uri.EscapeDataString(oAuthSignature),
											   Uri.EscapeDataString(oAuthSignatureMethod), Uri.EscapeDataString(oAuthTimestamp), Uri.EscapeDataString(TwitterAccessToken),
											   Uri.EscapeDataString(oAuthVersion));


			HttpWebRequest webRequest = (HttpWebRequest)WebRequest.Create(TwitterBaseUrl);
			webRequest.Headers.Add("Authorization", oAuthHeader);
			webRequest.Method = TwitterMethod;
			webRequest.ContentType = "application/x-www-form-urlencoded;charset=UTF-8";
			webRequest.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;

			byte[] byteArray = Encoding.UTF8.GetBytes(TwitterData);
			webRequest.ContentLength = byteArray.Length;
			Stream dataStream = webRequest.GetRequestStream();
			dataStream.Write(byteArray, 0, byteArray.Length);
			dataStream.Close();

			try
			{
				WebResponse TwitterResponse = webRequest.GetResponse();
				using (TwitterResponse)
				{
					using (var reader = new StreamReader(TwitterResponse.GetResponseStream()))
					{
						reader.ReadToEnd();
					}
				}
				CommandUtils.Log("Tweeted: " + InTweet);
			}
			catch
			{
				CommandUtils.Log("Tweet failed");
			}
		}
	}

	[RequireP4]
	class UnrealTournamentProto_BasicBuild : BuildCommand
	{
		static List<UnrealTargetConfiguration> GetClientConfigsToBuild(BuildCommand Cmd, string TargetPlatformOverride)
		{
			List<UnrealTargetConfiguration> ClientConfigs = new List<UnrealTargetConfiguration>();

			string ClientConfigNames = Cmd.ParseParamValue("clientconfig");
			if (ClientConfigNames != null)
			{
				if (ClientConfigNames.Equals("none", StringComparison.InvariantCultureIgnoreCase))
				{
					return ClientConfigs;
				}
				string[] ClientStrings = ClientConfigNames.Split('+');
				foreach (string Client in ClientStrings)
				{
					UnrealTargetConfiguration Config = (UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), Client);
					ClientConfigs.Add(Config);
				}
			}
			else
			{
				string TargetPlatform = TargetPlatformOverride.Length > 0 ? TargetPlatformOverride : Cmd.ParseParamValue("TargetPlatform");
				if (TargetPlatform == "Mac" || TargetPlatform == "Win32" || TargetPlatform == "Win64" || TargetPlatform == "PS4" || TargetPlatform == "XboxOne")
				{
					// The first config in this list is the one used in the launcher manifest.
					// If building for shipping, use shipping first. Otherwise, use development.
					if (Cmd.ParseParam("shipping"))
					{
						ClientConfigs.Add(UnrealTargetConfiguration.Shipping);
						ClientConfigs.Add(UnrealTargetConfiguration.Development);
						ClientConfigs.Add(UnrealTargetConfiguration.Test);
					}
					else
					{
						// Only producing test and development builds to reduce build machine burden in non-release branches
						ClientConfigs.Add(UnrealTargetConfiguration.Development);
						ClientConfigs.Add(UnrealTargetConfiguration.Test);
					}
				}
			}

			return ClientConfigs;
		}
		static List<UnrealTargetConfiguration> GetServerConfigsToBuild(BuildCommand Cmd, string TargetPlatformOverride)
		{
			List<UnrealTargetConfiguration> ServerConfigs = new List<UnrealTargetConfiguration>();

			string ServerConfigNames = Cmd.ParseParamValue("serverconfig");
			if (ServerConfigNames != null)
			{
				if (ServerConfigNames.Equals("none", StringComparison.InvariantCultureIgnoreCase))
				{
					return ServerConfigs;
				}
				string[] ServerStrings = ServerConfigNames.Split('+');
				foreach (string Server in ServerStrings)
				{
					UnrealTargetConfiguration Config = (UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), Server);
					ServerConfigs.Add(Config);
				}
			}
			else
			{
				string TargetPlatform = TargetPlatformOverride.Length > 0 ? TargetPlatformOverride : Cmd.ParseParamValue("TargetPlatform");
				if (TargetPlatform == "Linux" || TargetPlatform == "Win32" || TargetPlatform == "Win64")
				{
					// The order for servers does not matter as servers are not distributed by the launcher.
					if (Cmd.ParseParam("shipping"))
					{
						ServerConfigs.Add(UnrealTargetConfiguration.Development);
						ServerConfigs.Add(UnrealTargetConfiguration.Shipping);
						ServerConfigs.Add(UnrealTargetConfiguration.Test);
					}
					else
					{
						// Only producing test and development builds to reduce build machine burden in non-release branches
						ServerConfigs.Add(UnrealTargetConfiguration.Development);
						ServerConfigs.Add(UnrealTargetConfiguration.Test);
					}
				}
			}

			return ServerConfigs;
		}

		static List<TargetPlatformDescriptor> GetClientTargetPlatforms(BuildCommand Cmd, string TargetPlatformOverride)
		{
			List<UnrealTargetPlatform> ClientPlatforms = new List<UnrealTargetPlatform>();
			string TargetPlatform = TargetPlatformOverride.Length > 0 ? TargetPlatformOverride : Cmd.ParseParamValue("TargetPlatform");

			string[] PlatformStrings = TargetPlatform.Split('+');
			foreach (string PlatformString in PlatformStrings)
			{
				UnrealTargetPlatform Platform = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), PlatformString);
				ClientPlatforms.Add(Platform);
			}

			return ClientPlatforms.ConvertAll(x => new TargetPlatformDescriptor(x));
		}

		static List<TargetPlatformDescriptor> GetServerTargetPlatforms(BuildCommand Cmd, string TargetPlatformOverride)
		{
			List<UnrealTargetPlatform> ClientPlatforms = new List<UnrealTargetPlatform>();
			string TargetPlatform = TargetPlatformOverride.Length > 0 ? TargetPlatformOverride : Cmd.ParseParamValue("TargetPlatform");

			string[] PlatformStrings = TargetPlatform.Split('+');
			foreach (string PlatformString in PlatformStrings)
			{
				UnrealTargetPlatform Platform = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), PlatformString);
				ClientPlatforms.Add(Platform);
			}

			return ClientPlatforms.ConvertAll(x => new TargetPlatformDescriptor(x));
		}

		static public ProjectParams GetParams(BuildCommand Cmd, string TargetPlatformOverride="")
		{
			string P4Change = "Unknown";
			string P4Branch = "Unknown";
			if (P4Enabled)
			{
				P4Change = P4Env.ChangelistString;
				P4Branch = P4Env.BuildRootEscaped;
			}

			string StageDirectory = Cmd.ParseParamValue("StagingDir", null);

			List<TargetPlatformDescriptor> ClientTargetPlatforms = GetClientTargetPlatforms(Cmd, TargetPlatformOverride);
			List<TargetPlatformDescriptor> ServerTargetPlatforms = GetServerTargetPlatforms(Cmd, TargetPlatformOverride);
			List<UnrealTargetConfiguration> ClientConfigs = GetClientConfigsToBuild(Cmd, TargetPlatformOverride);
			List<UnrealTargetConfiguration> ServerConfigs = GetServerConfigsToBuild(Cmd, TargetPlatformOverride);

			ProjectParams Params = new ProjectParams
			(
				// Shared
				RawProjectPath: new FileReference(CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject")),

				// Build
				EditorTargets: new ParamList<string>("BuildPatchTool"),
				ClientCookedTargets: new ParamList<string>("UnrealTournament"),
				ServerCookedTargets: new ParamList<string>("UnrealTournamentServer"),

				ClientConfigsToBuild: ClientConfigs,
				ServerConfigsToBuild: ServerConfigs,
                ClientTargetPlatforms: ClientTargetPlatforms,
				ServerTargetPlatforms: ServerTargetPlatforms,
				NoClient: ClientConfigs.Count == 0,
				Build: !Cmd.ParseParam("skipbuild"),
				Cook: true,
				CulturesToCook: new ParamList<string>("en"),
				InternationalizationPreset: "English",
				SkipCook: Cmd.ParseParam("skipcook"),
				Clean: !Cmd.ParseParam("NoClean") && !Cmd.ParseParam("skipcook") && !Cmd.ParseParam("skippak") && !Cmd.ParseParam("skipstage") && !Cmd.ParseParam("skipbuild"),
				DedicatedServer: ServerConfigs.Count > 0,
				Pak: true,
				SkipPak: Cmd.ParseParam("skippak"),
				NoXGE: Cmd.ParseParam("NoXGE"),
				Stage: true,
				SkipStage: Cmd.ParseParam("skipstage"),
				NoDebugInfo: Cmd.ParseParam("NoDebugInfo"),
				CrashReporter: !Cmd.ParseParam("mac"), // @todo Mac: change to true when Mac implementation is ready
				CreateReleaseVersion: "UTVersion0",
				UnversionedCookedContent: true,
                NoBootstrapExe: true,
				// if we are running, we assume this is a local test and don't chunk
				Run: Cmd.ParseParam("Run"),
                TreatNonShippingBinariesAsDebugFiles: true,
				StageDirectoryParam: (StageDirectory != null ? StageDirectory : UnrealTournamentBuild.GetArchiveDir()),
                AppLocalDirectory: Cmd.ParseParamValue("AppLocalDirectory")
            );
			Params.ValidateAndLog();
			return Params;
		}

		public void CopyAssetRegistryFilesFromSavedCookedToReleases(ProjectParams Params)
		{
			if (P4Enabled && !String.IsNullOrEmpty(Params.CreateReleaseVersion))
			{
				Log("************************* Copying AssetRegistry.bin files from Saved/Cooked to Releases");
				string ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);
				string SavedPath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Saved", "Cooked");

				// add in the platforms we just made assetregistry files for.
				List<string> Platforms = new List<string>();
                if (Params.ClientConfigsToBuild.Count > 0)
                {
                    if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win32)) || Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
                        Platforms.Add("WindowsNoEditor");
                    if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Mac)))
                        Platforms.Add("MacNoEditor");
                    if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Linux)))
                        Platforms.Add("LinuxNoEditor");
                }

                if (Params.ServerConfigsToBuild.Count > 0)
                {
                    if (Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win32)) || Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
                        Platforms.Add("WindowsServer");
                    if (Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Linux)))
                        Platforms.Add("LinuxServer");
                }

				foreach (string Platform in Platforms)
				{
					string FromPath = CombinePaths(SavedPath, Platform, "UnrealTournament", "AssetRegistry.bin");
                    string Filename = CombinePaths(ReleasePath, Platform, "AssetRegistry.bin");
					Log("Copying AssetRegistry.bin file from: " + FromPath + " to " + Filename);
					CommandUtils.CopyFile_NoExceptions(FromPath, Filename);
				}
			}
		}

		public void SubmitAssetRegistryFilesToPerforce(ProjectParams Params)
		{
			// Submit release version assetregistry.bin
			if (P4Enabled && !String.IsNullOrEmpty(Params.CreateReleaseVersion))
			{
				Log("************************* Submitting AssetRegistry.bin files");
				int AssetRegCL = P4.CreateChange(P4Env.Client, String.Format("UnrealTournamentBuild AssetRegistry build built from changelist {0}", P4Env.Changelist));
				if (AssetRegCL > 0)
				{
					string ReleasePath = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Releases", Params.CreateReleaseVersion);

                    // add in the platforms we just made assetregistry files for.
                    List<string> Platforms = new List<string>();
                    if (Params.ClientConfigsToBuild.Count > 0)
                    {
                        if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win32)) || Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
                            Platforms.Add("WindowsNoEditor");
                        if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Mac)))
                            Platforms.Add("MacNoEditor");
                        if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Linux)))
                            Platforms.Add("LinuxNoEditor");
                    }

                    if (Params.ServerConfigsToBuild.Count > 0)
                    {
                        if (Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win32)) || Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
                            Platforms.Add("WindowsServer");
                        if (Params.ServerTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Linux)))
                            Platforms.Add("LinuxServer");
                    }

                    foreach (string Platform in Platforms)
					{
						string Filename = CombinePaths(ReleasePath, Platform, "AssetRegistry.bin");

						P4.Sync("-f -k " + Filename + "#head"); // sync the file without overwriting local one

						if (!FileExists(Filename))
						{
							throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", Filename);
						}

						P4.ReconcileNoDeletes(AssetRegCL, Filename);
					}
				}
				bool Pending;
				if (P4.ChangeFiles(AssetRegCL, out Pending).Count == 0)
				{
					Log("************************* No files to submit.");
					P4.DeleteChange(AssetRegCL);
				}
				else
				{
					int SubmittedCL;
					P4.Submit(AssetRegCL, out SubmittedCL, true, true);
				}
			}
		}

		public override void ExecuteBuild()
		{
			Log("************************* UnrealTournamentProto_BasicBuild");

			ProjectParams Params = GetParams(this);

			int WorkingCL = -1;
			if (P4Enabled && AllowSubmit)
			{
				WorkingCL = P4.CreateChange(P4Env.Client, String.Format("UnrealTournamentBuild build built from changelist {0}", P4Env.Changelist));
				Log("Build from {0}    Working in {1}", P4Env.Changelist, WorkingCL);
			}

			if (P4Enabled)
			{
                if (Params.ClientConfigsToBuild.Count > 0)
                {
                    UnrealTournamentBuild.Tweet(String.Format("Starting {2} Client {0} build from changelist {1}", P4Env.BuildRootP4, P4Env.Changelist, Params.ClientTargetPlatforms[0].ToString()));
                }
                else if (Params.ServerConfigsToBuild.Count > 0)
                {
                    UnrealTournamentBuild.Tweet(String.Format("Starting {2} Server {0} build from changelist {1}", P4Env.BuildRootP4, P4Env.Changelist, Params.ServerTargetPlatforms[0].ToString()));
                }
			}

			Project.Build(this, Params, WorkingCL);
			Project.Cook(Params);
			Project.CopyBuildToStagingDirectory(Params);
			if (!this.ParseParam("SkipPerforce"))
			{
				CopyAssetRegistryFilesFromSavedCookedToReleases(Params);
				SubmitAssetRegistryFilesToPerforce(Params);
                
                // Only trust win64 on version files, mac has bad line endings
                if (Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
                {
                    if (CommandUtils.P4Enabled && CommandUtils.P4Env.BuildRootP4.Contains("Release"))
                    {
                        SubmitVersionFilesToPerforce();
                    }
                }
			}
			PrintRunTime();
			Project.Deploy(Params);
			Project.Run(Params);

			if (WorkingCL > 0)
			{
				bool Pending;
				if (P4.ChangeFiles(WorkingCL, out Pending).Count == 0)
				{
					Log("************************* No files to submit.");
					P4.DeleteChange(WorkingCL);
				}
				else
				{
					// Check everything in!
					int SubmittedCL;
					P4.Submit(WorkingCL, out SubmittedCL, true, true);
				}

				//P4.MakeDownstreamLabel(P4Env, "UnrealTournamentBuild");
			}

		}

		private void SubmitVersionFilesToPerforce()
		{
			if (CommandUtils.P4Enabled)
			{
				int VersionFilesCL = CommandUtils.P4.CreateChange(CommandUtils.P4Env.Client, String.Format("UnrealTournamentBuild Version Files from changelist {0}", CommandUtils.P4Env.Changelist));
				if (VersionFilesCL > 0)
				{
					string[] VersionFiles = new string[4];
					VersionFiles[0] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Build", "Build.version");
					VersionFiles[1] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Core", "Private", "UObject", "ObjectVersion.cpp");
					VersionFiles[2] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Runtime", "Launch", "Resources", "Version.h");
					VersionFiles[3] = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source", "Programs", "DotNETCommon", "MetaData.cs");

					foreach (string VersionFile in VersionFiles)
					{
						CommandUtils.P4.Sync("-f -k " + VersionFile + "#head"); // sync the file without overwriting local one

						if (!CommandUtils.FileExists(VersionFile))
						{
							throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", VersionFile);
						}

						CommandUtils.P4.ReconcileNoDeletes(VersionFilesCL, VersionFile);
					}
					bool Pending;
					if (CommandUtils.P4.ChangeFiles(VersionFilesCL, out Pending).Count == 0)
					{
						CommandUtils.Log("************************* No files to submit.");
						CommandUtils.P4.DeleteChange(VersionFilesCL);
					}
					else
					{
						int SubmittedCL;
						CommandUtils.P4.Submit(VersionFilesCL, out SubmittedCL, true, true);
					}
				}
			}
		}
	}


	// Performs the appropriate labeling and other actions for a given promotion
	// See the EC job for an up-to-date description of all parameters
	[Help("ToAppName", "The appname to promote the build to.")]
	[Help("BuildVersion", "Non-platform-specific BuildVersion to promote.")]
	[Help("Platforms", "Comma-separated list of platforms to promote.")]
	[Help("AllowLivePromotion", "Optional.  Toggle on to allow promotion to the Live instance/label.")]
	[Help("AWSCredentialsFile", @"Optional.  The full path to the Amazon credentials file used to access S3. Defaults to P:\Builds\Utilities\S3Credentials.txt.")]
	[Help("AWSCredentialsKey", "Optional. The name of the credentials profile to use when accessing AWS.  Defaults to \"s3_origin_prod\".")]
	[Help("AWSRegion", "Optional. The system name of the Amazon region which contains the S3 bucket.  Defaults to \"us-east-1\".")]
	[Help("AWSBucket", "Optional. The name of the Amazon S3 bucket to copy a build to. Defaults to \"patcher-origin\".")]
	class UnrealTournament_PromoteBuild : BuildCommand
	{
		public override void ExecuteBuild()
		{
			Log("************************* UnrealTournament_PromoteBuild");

			// New cross-promote apps only ever have a Live label.  Let them by without setting the label dropdown.
			const string LiveLabel = "Live";

			// Determine whether to promote game, editor, or both
			bool bShouldPromoteGameClient = false;
			bool bShouldPromoteEditor = false;
			{
				List<string> AllProducts = new List<string> { "GameClient", "Editor" };
				string ProductsString = ParseParamValue("Products");
				if (string.IsNullOrEmpty(ProductsString))
				{
					throw new AutomationException("Products is a required parameter");
				}
				var Products = ProductsString.Split(',').Select(x => x.Trim()).ToList();
				var InvalidProducts = Products.Except(AllProducts);
				if (InvalidProducts.Any())
				{
					throw new AutomationException(CreateDebugList(InvalidProducts, "The following product names are invalid:"));
				}
				bShouldPromoteGameClient = Products.Contains("GameClient");
				bShouldPromoteEditor = Products.Contains("Editor");
			}

			// Setup the list of dev apps vs. release apps for enforcing branch consistency for each backend.  Editor builds track with game builds, so just use the game builds' app-to-env mappings
			List<UnrealTournamentBuild.UnrealTournamentAppName> DevBranchApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
			DevBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting);
			DevBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevStage);
			DevBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest);

			List<UnrealTournamentBuild.UnrealTournamentAppName> ReleaseBranchApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
			ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting);
			ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseStage);
			ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest);
			ReleaseBranchApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev); // live public app, stuck on confusing legacy name

            string BuildVersion = ParseParamValue("BuildVersion");
            if (String.IsNullOrEmpty(BuildVersion))
            {
                throw new AutomationException("BuildVersion is a required parameter");
            }

            // Strip platform if it's on there, common mistake/confusion to include it in EC because it's included in the admin portal UI
            foreach (String platform in Enum.GetNames(typeof(MCPPlatform)))
            {
                if (BuildVersion.EndsWith("-" + platform))
                {
                    BuildVersion = BuildVersion.Substring(0, BuildVersion.Length - ("-" + platform).Length);
                    Log("Stripped platform off BuildVersion, resulting in {0}", BuildVersion);
                }
            }

            // FromApps
            List<UnrealTournamentBuild.UnrealTournamentAppName> FromApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
            if (BuildVersion.Contains("Release"))
            {
                FromApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseBuilds);
            }
            else
            {
                FromApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentBuilds);
            }

			// All other apps are only for promoting "to"
			// Map which apps are GameDev only
			List<UnrealTournamentBuild.UnrealTournamentAppName> GameDevApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
			GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting);
			GameDevApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting);
			// Map which apps are Stage only
			List<UnrealTournamentBuild.UnrealTournamentAppName> StageApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
			StageApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevStage);
			StageApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseStage);
			// Map which apps are Production only
			List<UnrealTournamentBuild.UnrealTournamentAppName> ProdApps = new List<UnrealTournamentBuild.UnrealTournamentAppName>();
			ProdApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest);
			ProdApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest);
			ProdApps.Add(UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev);

			// Each environment's mcpconfig string
			string GameDevMcpConfigString = "MainGameDevNet";
			string StagingMcpConfigString = "StageNet";
			string ProdMcpConfigString = "ProdNet";

			// Enforce some restrictions on destination apps
			UnrealTournamentBuild.UnrealTournamentAppName ToGameApp;
			{
				string ToGameAppName = ParseParamValue("ToAppName");
				if (ToGameAppName == "Select Target App")
				{
					// Scrub the default option out of the param, treat it the same as not passing the param
					ToGameAppName = "";
				}
				if (String.IsNullOrEmpty(ToGameAppName))
				{
					throw new AutomationException("ToAppName is a required parameter");
				}
				if (!Enum.TryParse(ToGameAppName, out ToGameApp))
				{
					throw new AutomationException("Unrecognized ToAppName: {0}", ToGameAppName);
				}
				if (FromApps.Contains(ToGameApp))
				{
					throw new AutomationException("App passed in ToAppName is not valid as a destination app: {0}", ToGameAppName);
				}
			}

			// Setup the editor ToApp based on the game app
			UnrealTournamentBuild.UnrealTournamentEditorAppName ToEditorApp;
			{
				if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevTesting)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorDevTesting;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevStage)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorDevStage;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorDevPlaytest;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseTesting)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorReleaseTesting;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseStage)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorReleaseStage;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorPublicTest;
				}
				else if (ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
				{
					ToEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditor;
				}
				else
				{
					throw new AutomationException("Unable to find an editor app matching game app {1}", ToGameApp);
				}
			}

			// Setup source game app
			UnrealTournamentBuild.UnrealTournamentAppName FromGameApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentBuilds;
            if (BuildVersion.Contains("Release"))
            {
                FromGameApp = UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentReleaseBuilds;
            }            

            // Setup source editor app
            UnrealTournamentBuild.UnrealTournamentEditorAppName FromEditorApp = UnrealTournamentBuild.UnrealTournamentEditorAppName.UnrealTournamentEditorBuilds;

			// Set some simple flags for identifying the type of promotion
			bool bIsGameDevPromotion = GameDevApps.Contains(ToGameApp);
			bool bIsStagePromotion = StageApps.Contains(ToGameApp);
			bool bIsProdPromotion = ProdApps.Contains(ToGameApp);

			// Set this to false to disable dedicated server fleet deployment
			bool bEnableServerDeploy = true;

			// Booleans related to dedicated server deployments
			bool bDistributeServerBuild = false;
			bool bSetLiveServerBuild = false;
			if (bEnableServerDeploy)
			{
				bDistributeServerBuild = bIsStagePromotion;
				bSetLiveServerBuild = bIsProdPromotion;
			}

			// Determine the environment for the target app
			string TargetAppMcpConfig;
			if (bIsProdPromotion)
			{
				TargetAppMcpConfig = ProdMcpConfigString;
			}
			else if (bIsStagePromotion)
			{
				TargetAppMcpConfig = StagingMcpConfigString;
			}
			else if (bIsGameDevPromotion)
			{
				TargetAppMcpConfig = GameDevMcpConfigString;
			}
			else
			{
				// How the heck did we get here?
				throw new AutomationException("Couldn't decide which environments to apply label {0} of ToApp {1}", LiveLabel, ToGameApp.ToString());
			}

			// Make sure the switch was flipped if promoting to the Live app used by the public
			bool AllowLivePromotion = ParseParam("AllowLivePromotion");
			if (AllowLivePromotion == false && ToGameApp == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
			{
				throw new AutomationException("You attempted to promote to Live without toggling on AllowLivePromotions.  Did you mean to promote to Live?");
			}

			// Work out the manifest URL for each platform we're promoting, and check the source builds are available
			Log("-- Verifying source build exists for all platforms");
			var GameStagingInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
			var EditorStagingInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
			{
				// Pull the list of platforms requested for promotion
				string PlatformParam = ParseParamValue("Platforms");
				if (String.IsNullOrEmpty(PlatformParam))
				{
					throw new AutomationException("Platforms list is a required parameter - defaults are set in EC");
				}
				List<MCPPlatform> RequestedPlatforms = PlatformParam.Split(',').Select(PlatformStr => (MCPPlatform)Enum.Parse(typeof(MCPPlatform), PlatformStr)).ToList<MCPPlatform>();
				if (RequestedPlatforms.Count == 0)
				{
					throw new AutomationException("Platforms is a required parameter, unable to parse platforms from {0}", PlatformParam);
				}

				var InvalidPlatforms = new List<string>();
				if (bShouldPromoteGameClient)
				{
					// Setup game staging infos to be posted
					foreach (var Platform in RequestedPlatforms)
					{
						var GameSourceStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromGameApp);
						if (!BuildInfoPublisherBase.Get().BuildExists(GameSourceStagingInfo, GameDevMcpConfigString))
						{
							InvalidPlatforms.Add(Platform.ToString());
							Log("Unable to find platform {0} build with version {3} in app {1} on {2} mcp", GameSourceStagingInfo.Platform, FromGameApp, GameDevMcpConfigString, GameSourceStagingInfo.BuildVersion);
						}
						else
						{
							var ManifestFilename = Path.GetFileName(BuildInfoPublisherBase.Get().GetBuildManifestUrl(GameSourceStagingInfo, GameDevMcpConfigString));
							var GameTargetStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, BuildVersion, Platform, ToGameApp, ManifestFilename);
							GameStagingInfos.Add(Platform, GameTargetStagingInfo);
						}
					}
					// Make sure we didn't hit any platforms for which the build didn't exist
					if (InvalidPlatforms.Any())
					{
						throw new AutomationException("Source game builds for {0} build {1} do not exist on all specified platforms.", FromGameApp, BuildVersion);
					}
				}
				if (bShouldPromoteEditor)
				{
					// Only do non-win32 editor builds
					List<MCPPlatform> EditorPlatforms = RequestedPlatforms.Where(x => x != MCPPlatform.Win32).ToList();
					// Setup game staging infos to be posted
					foreach (var Platform in EditorPlatforms)
					{
						var EditorSourceStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, FromEditorApp);
						if (!BuildInfoPublisherBase.Get().BuildExists(EditorSourceStagingInfo, GameDevMcpConfigString))
						{
							InvalidPlatforms.Add(Platform.ToString());
							Log("Unable to find platform {0} build with version {3} in app {1} on {2} mcp", EditorSourceStagingInfo.Platform, FromEditorApp, GameDevMcpConfigString, EditorSourceStagingInfo.BuildVersion);
						}
						else
						{
							var ManifestFilename = Path.GetFileName(BuildInfoPublisherBase.Get().GetBuildManifestUrl(EditorSourceStagingInfo, GameDevMcpConfigString));
							var EditorTargetStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, BuildVersion, Platform, ToEditorApp, ManifestFilename);
							EditorStagingInfos.Add(Platform, EditorTargetStagingInfo);
						}
					}
					// Make sure we didn't hit any platforms for which the build didn't exist
					if (InvalidPlatforms.Any())
					{
						throw new AutomationException("Source editor builds for {0} build {1} do not exist on all specified platforms.", FromEditorApp, BuildVersion);
					}
				}

			}

			// S3 PARAMETERS (used during staging and operating on ProdCom only)
			CloudStorageBase CloudStorage = null;
			string S3Bucket = null;
			if (bIsProdPromotion || bIsStagePromotion)
			{
				S3Bucket = ParseParamValue("AWSBucket", "patcher-origin");

				var CloudConfiguration = new Dictionary<string, object>
			{
				{ "CredentialsFilePath", ParseParamValue("AWSCredentialsFile", @"P:\Builds\Utilities\S3Credentials.txt") },
				{ "CredentialsKey",      ParseParamValue("AWSCredentialsKey", "s3_origin_prod") },
				{ "AWSRegion",           ParseParamValue("AWSRegion", "us-east-1") }
			};

				CloudStorage = CloudStorageBase.Get();
				CloudStorage.Init(CloudConfiguration);
			}

			// Ensure that the build exists in gamedev Build Info services, regardless of where we're promoting to
			if (GameDevApps.Contains(ToGameApp))
			{
				Log("-- Ensuring all builds are posted to {0} app {1}/{2} for gamedev promotion", GameDevMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
				EnsureBuildIsRegistered(GameDevMcpConfigString, GameStagingInfos);
				EnsureBuildIsRegistered(GameDevMcpConfigString, EditorStagingInfos);
			}

			// If this label will go to Prod, then make sure the build manifests are all staged to the Prod CDN already
			if (bIsProdPromotion)
			{
				// Look for game builds' manifests
				foreach (var Platform in GameStagingInfos.Keys)
				{
					var StagingInfo = GameStagingInfos[Platform]; // We can use the target here, as it's guaranteed to have the correct manifest filename embedded

					// Check for the manifest on the S3 bucket which seeds the production CDN
					Log("Verifying manifest for prod promotion of Ocean {0} {1} was already staged to the S3 origin", BuildVersion, Platform);
					bool bWasManifestFound = CloudStorage.IsManifestOnCloudStorage(S3Bucket, StagingInfo);
					if (!bWasManifestFound)
					{
						string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
						throw new AutomationException("Promotion to Prod requires the build first be staged to the S3 origin. Manifest {0} not found for promotion to label {1} of app {2}"
							, StagingInfo.ManifestFilename
							, DestinationLabelWithPlatform
							, ToGameApp.ToString());
					}
				}
				// Look for editor builds' manifests
				foreach (var Platform in EditorStagingInfos.Keys)
				{
					var StagingInfo = EditorStagingInfos[Platform]; // We can use the target here, as it's guaranteed to have the correct manifest filename embedded

					// Check for the manifest on the S3 bucket which seeds the production CDN
					Log("Verifying manifest for prod promotion of Ocean {0} {1} was already staged to the S3 origin", BuildVersion, Platform);
					bool bWasManifestFound = CloudStorage.IsManifestOnCloudStorage(S3Bucket, StagingInfo);
					if (!bWasManifestFound)
					{
						string DestinationLabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(LiveLabel, Platform);
						throw new AutomationException("Promotion to Prod requires the build first be staged to the S3 origin. Manifest {0} not found for promotion to label {1} of app {2}"
							, StagingInfo.ManifestFilename
							, DestinationLabelWithPlatform
							, ToEditorApp.ToString());
					}
				}


				// Ensure the build is registered on the prod and staging build info services.
				Log("-- Ensuring builds are posted to {0} app {1}/{2} for prod promotion", StagingMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
				EnsureBuildIsRegistered(StagingMcpConfigString, GameStagingInfos);
				EnsureBuildIsRegistered(StagingMcpConfigString, EditorStagingInfos);
				Log("-- Ensuring build are posted to {0} app {1}/{2} for prod promotion", ProdMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
				EnsureBuildIsRegistered(ProdMcpConfigString, GameStagingInfos);
				EnsureBuildIsRegistered(ProdMcpConfigString, EditorStagingInfos);
			}

			//
			// Execute additional logic required for each promotion
			//

			GameServerManager GameServerManager = new GameServerManager(ToGameApp, "ut-builds", BuildVersion);

			// Create dedicated server VM images
			if (bDistributeServerBuild)
			{
				GameServerManager.DistributeImagesAsync();
			}

			Log("Performing build promotion actions for promoting to label {0} of app {1}/{2}.", LiveLabel, ToGameApp.ToString(), ToEditorApp.ToString());
			if (bIsStagePromotion)
			{
				// Copy game chunks to CDN
				foreach (var Platform in GameStagingInfos.Keys)
				{
					var StagingInfo = GameStagingInfos[Platform];
					{
						Log("Promoting game chunks to S3 origin");
						CloudStorage.CopyChunksToCloudStorage(S3Bucket, StagingInfo);
						Log("DONE Promoting game chunks");
					}
				}
				// Copy editor chunks to CDN
				foreach (var Platform in EditorStagingInfos.Keys)
				{
					var StagingInfo = EditorStagingInfos[Platform];
					{
						Log("Promoting editor chunks to S3 origin");
						CloudStorage.CopyChunksToCloudStorage(S3Bucket, StagingInfo);
						Log("DONE Promoting editor chunks");
					}
				}

				// Ensure the build is registered on the staging build info services.
				Log("Ensuring build is posted to {0} app {1}/{2} for stage promotion", StagingMcpConfigString, ToGameApp.ToString(), ToEditorApp.ToString());
				EnsureBuildIsRegistered(StagingMcpConfigString, GameStagingInfos);
				EnsureBuildIsRegistered(StagingMcpConfigString, EditorStagingInfos);
			}

			// Wait for images to finish being created and distributed
			if (bDistributeServerBuild)
			{
				GameServerManager.DistributeImagesWait(3600);
			}

			// Set dedicated servers fleets live. 
			if (bSetLiveServerBuild)
			{
				GameServerManager.DeployNewFleets(900);
			}

			// Apply game rollback label to preserve info about last Live build
			{
				Log("-- Starting Rollback Labeling");
				Dictionary<MCPPlatform, BuildPatchToolStagingInfo> RollbackBuildInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
				// For each platform, get the Live labeled build and create a staging info for posting to the Rollback label
				foreach (var Platform in GameStagingInfos.Keys)
				{
					var TargetStagingInfo = GameStagingInfos[Platform];
					string LiveLabelWithPlatform = LiveLabel + "-" + Platform;
					string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(TargetStagingInfo.AppName, LiveLabelWithPlatform, TargetAppMcpConfig);
					if (String.IsNullOrEmpty(LiveBuildVersionString))
					{
						Log("platform {0} of app {1}: No Live game build found, skipping rollback labeling", Platform, ToGameApp.ToString());
					}
					else if (LiveBuildVersionString.EndsWith("-" + Platform))
					{
						// Take off platform so it can fit in the FEngineVersion struct
						string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
						if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
						{
							// Create a staging info with the rollback BuildVersion and add it to the list
							Log("platform {0} of app {1}: Identified Live game build {2}, queueing for rollback labeling", Platform, ToGameApp.ToString(), LiveBuildVersionString);
							BuildPatchToolStagingInfo RollbackStagingInfo = UnrealTournamentBuild.GetUTBuildPatchToolStagingInfo(this, LiveBuildVersionStringWithoutPlatform, Platform, ToGameApp, GameStagingInfos[Platform].ManifestFilename);
							RollbackBuildInfos.Add(Platform, RollbackStagingInfo);
						}
						else
						{
							Log("platform {0} of app {1}: Identified Live game build {2}, skipping rollback labeling as this build was already live, and would cause us to lose track of the previous live buildversion", Platform, ToGameApp.ToString(), LiveBuildVersionString);
						}
					}
					else
					{
						throw new AutomationException("Current Live game buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
					}
				}
				if (RollbackBuildInfos.Count > 0)
				{
					Log("Applying rollback labels for {0} previous Live game build platforms identified above", RollbackBuildInfos.Count);
					LabelBuilds(RollbackBuildInfos, "Rollback", TargetAppMcpConfig);
				}
			}

			// Apply editor rollback label to preserve info about last Live build
			{
				Log("-- Starting Rollback Labeling");
				Dictionary<MCPPlatform, BuildPatchToolStagingInfo> RollbackBuildInfos = new Dictionary<MCPPlatform, BuildPatchToolStagingInfo>();
				// For each platform, get the Live labeled build and create a staging info for posting to the Rollback label
				foreach (var Platform in EditorStagingInfos.Keys)
				{
					var TargetStagingInfo = EditorStagingInfos[Platform];
					string LiveLabelWithPlatform = LiveLabel + "-" + Platform;
					string LiveBuildVersionString = BuildInfoPublisherBase.Get().GetLabeledBuildVersion(TargetStagingInfo.AppName, LiveLabelWithPlatform, TargetAppMcpConfig);
					if (String.IsNullOrEmpty(LiveBuildVersionString))
					{
						Log("platform {0} of app {1}: No Live editor build found, skipping rollback labeling", Platform, ToEditorApp.ToString());
					}
					else if (LiveBuildVersionString.EndsWith("-" + Platform))
					{
						// Take off platform so it can fit in the FEngineVersion struct
						string LiveBuildVersionStringWithoutPlatform = LiveBuildVersionString.Remove(LiveBuildVersionString.IndexOf("-" + Platform));
						if (LiveBuildVersionStringWithoutPlatform != BuildVersion)
						{
							// Create a staging info with the rollback BuildVersion and add it to the list
							Log("platform {0} of app {1}: Identified Live editor build {2}, queueing for rollback labeling", Platform, ToEditorApp.ToString(), LiveBuildVersionString);
							BuildPatchToolStagingInfo RollbackStagingInfo = UnrealTournamentBuild.GetUTEditorBuildPatchToolStagingInfo(this, LiveBuildVersionStringWithoutPlatform, Platform, ToEditorApp, EditorStagingInfos[Platform].ManifestFilename);
							RollbackBuildInfos.Add(Platform, RollbackStagingInfo);
						}
						else
						{
							Log("platform {0} of app {1}: Identified Live editor build {2}, skipping rollback labeling as this build was already live, and would cause us to lose track of the previous live buildversion", Platform, ToEditorApp.ToString(), LiveBuildVersionString);
						}
					}
					else
					{
						throw new AutomationException("Current Live editor buildversion: {0} doesn't appear to end with platform: {1} as it should!", LiveBuildVersionString, Platform);
					}
				}
				if (RollbackBuildInfos.Count > 0)
				{
					Log("Applying rollback labels for {0} previous Live build platforms identified above", RollbackBuildInfos.Count);
					LabelBuilds(RollbackBuildInfos, "Rollback", TargetAppMcpConfig);
				}
			}

			// Apply the LIVE label in the target apps
			Log("-- Labeling build {0} with label {1} on app {2}/{3} across all platforms", BuildVersion, LiveLabel, ToGameApp.ToString(), ToEditorApp.ToString());
			LabelBuilds(GameStagingInfos, LiveLabel, TargetAppMcpConfig);
			LabelBuilds(EditorStagingInfos, LiveLabel, TargetAppMcpConfig);

			// Set dedicated servers fleets live. 
			if (bSetLiveServerBuild)
			{
				GameServerManager.DegradeOldFleets(900);
			}
			Log("************************* Ocean_PromoteBuild completed");
		}

		private void EnsureBuildIsRegistered(string McpConfigNames, Dictionary<MCPPlatform, BuildPatchToolStagingInfo> StagingInfos)
		{
			foreach (string McpConfigName in McpConfigNames.Split(','))
			{
				foreach (var Platform in StagingInfos.Keys)
				{
					BuildPatchToolStagingInfo StagingInfo = StagingInfos[Platform];
					if (!BuildInfoPublisherBase.Get().BuildExists(StagingInfo, McpConfigName))
					{
						Log("Posting build {0} to app {1} of {2} build info service as it does not already exist.", StagingInfo.BuildVersion, StagingInfo.AppName, McpConfigName);
						BuildInfoPublisherBase.Get().PostBuildInfo(StagingInfo, McpConfigName);
					}
				}
			}
		}

		/// <summary>
		/// Apply the requested label to the requested build in the BuildInfo backend for the requested MCP environment
		/// Repeat for each passed platform, adding the platform to the end of the label that is applied
		/// </summary>
		/// <param name="BuildInfos">The dictionary of metadata about all builds we're working with</param>
		/// <param name="DestinationLabel">Label, WITHOUT platform embedded, to apply</param>
		/// <param name="McpConfigNames">Which BuildInfo backends to label the build in.</param>
		private void LabelBuilds(Dictionary<MCPPlatform, BuildPatchToolStagingInfo> StagingInfos, string DestinationLabel, string McpConfigNames)
		{
			foreach (string McpConfigName in McpConfigNames.Split(','))
			{
				foreach (var Platform in StagingInfos.Keys)
				{
					string LabelWithPlatform = BuildInfoPublisherBase.Get().GetLabelWithPlatform(DestinationLabel, Platform);
					BuildInfoPublisherBase.Get().LabelBuild(StagingInfos[Platform], LabelWithPlatform, McpConfigName);
				}
			}
		}

		static public string CreateDebugList<T>(IEnumerable<T> objects, string prefix)
		{
			return objects.Aggregate(new StringBuilder(prefix), (sb, obj) => sb.AppendFormat("\n    {0}", obj.ToString())).ToString();
		}
	}

	public class MakeUTDLC : BuildCommand
	{
		public string DLCName;
		public string DLCMaps;
		public string AssetRegistry;
		public string VersionString;

		static public ProjectParams GetParams(BuildCommand Cmd, string DLCName, string AssetRegistry)
		{
			string P4Change = "Unknown";
			string P4Branch = "Unknown";
			if (P4Enabled)
			{
				P4Change = P4Env.ChangelistString;
				P4Branch = P4Env.BuildRootEscaped;
			}

			ProjectParams Params = new ProjectParams
			(
				Command: Cmd,
				// Shared
				Cook: true,
				Stage: true,
				Pak: true,
				Compressed: true,
				BasedOnReleaseVersion: AssetRegistry,
				RawProjectPath: new FileReference(CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "UnrealTournament.uproject")),
				StageDirectoryParam: CommandUtils.CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Saved", "StagedBuilds", DLCName)
			);

			Params.ValidateAndLog();
			return Params;
		}

		public void Cook(DeploymentContext SC, ProjectParams Params)
		{
			string Parameters = "-newcook -BasedOnReleaseVersion=" + AssetRegistry;

			if (DLCMaps.Length > 0)
			{
				Parameters += " -map=" + DLCMaps;
			}

			string CookDir = CombinePaths(CmdEnv.LocalRoot, "UnrealTournament", "Plugins", DLCName, "Content");
			RunCommandlet(Params.RawProjectPath, "UE4Editor-Cmd.exe", "Cook", String.Format("-CookDir={0} -TargetPlatform={1} {2} -DLCName={3} -SKIPEDITORCONTENT", CookDir, SC.CookPlatform, Parameters, DLCName));
		}

		public void Stage(DeploymentContext SC, ProjectParams Params)
		{
			// Create the version.txt
			string VersionPath = CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", DLCName + "-" + "version.txt");
			CommandUtils.WriteToFile(VersionPath, VersionString);

			// Rename the asset registry file to DLC name
			CommandUtils.RenameFile(CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", "AssetRegistry.bin"),
									CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform, "UnrealTournament", DLCName + "-" + "AssetRegistry.bin"));

			// Put all of the cooked dir into the staged dir
			SC.StageFiles(StagedFileType.UFS, CombinePaths(SC.ProjectRoot, "Saved", "Cooked", DLCName, SC.CookPlatform), "*", true, new[] { "CookedAssetRegistry.json", "CookedIniVersion.txt" }, "", true, !Params.UsePak(SC.StageTargetPlatform));


			// Stage and pak it all
			Project.ApplyStagingManifest(Params, SC);

			CommandUtils.DeleteFile_NoExceptions(CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", DLCName + "-" + SC.CookPlatform + ".pak"), true);

			// Rename the pak file to DLC name
			CommandUtils.RenameFile(CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", "UnrealTournament-" + SC.CookPlatform + ".pak"),
									CombinePaths(SC.StageDirectory, "UnrealTournament", "Content", "Paks", DLCName + "-" + SC.CookPlatform + ".pak"));
		}

		public static List<DeploymentContext> CreateDeploymentContext(ProjectParams Params, bool InDedicatedServer, bool DoCleanStage = false)
		{
			ParamList<string> ListToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerCookedTargets : Params.ClientCookedTargets;
			var ConfigsToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerConfigsToBuild : Params.ClientConfigsToBuild;

			List<TargetPlatformDescriptor> PlatformsToStage = Params.ClientTargetPlatforms;
			if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
			{
				PlatformsToStage = Params.ServerTargetPlatforms;
			}

			bool prefixArchiveDir = false;
			if (PlatformsToStage.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win32)) && 
                PlatformsToStage.Contains(new TargetPlatformDescriptor(UnrealTargetPlatform.Win64)))
			{
				prefixArchiveDir = true;
			}

			List<DeploymentContext> DeploymentContexts = new List<DeploymentContext>();
			foreach (var StagePlatform in PlatformsToStage)
			{
                // Get the platform to get cooked data from, may differ from the stage platform
                TargetPlatformDescriptor CookedDataPlatform = Params.GetCookedDataPlatformForClientTarget(StagePlatform);

				if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
				{
					CookedDataPlatform = Params.GetCookedDataPlatformForServerTarget(StagePlatform);
				}

				List<string> ExecutablesToStage = new List<string>();

				string PlatformName = StagePlatform.ToString();
				string StageArchitecture = !String.IsNullOrEmpty(Params.SpecifiedArchitecture) ? Params.SpecifiedArchitecture : "";
				foreach (var Target in ListToProcess)
				{
					foreach (var Config in ConfigsToProcess)
					{
						string Exe = Target;
						if (Config != UnrealTargetConfiguration.Development)
						{
							Exe = Target + "-" + PlatformName + "-" + Config.ToString() + StageArchitecture;
						}
						ExecutablesToStage.Add(Exe);
					}
				}

				string StageDirectory = (Params.Stage || !String.IsNullOrEmpty(Params.StageDirectoryParam)) ? Params.BaseStageDirectory : "";
				string ArchiveDirectory = (Params.Archive || !String.IsNullOrEmpty(Params.ArchiveDirectoryParam)) ? Params.BaseArchiveDirectory : "";
				if (prefixArchiveDir && (StagePlatform.Type == UnrealTargetPlatform.Win32 || StagePlatform.Type == UnrealTargetPlatform.Win64))
				{
					if (Params.Stage)
					{
						StageDirectory = CombinePaths(Params.BaseStageDirectory, StagePlatform.ToString());
					}
					if (Params.Archive)
					{
						ArchiveDirectory = CombinePaths(Params.BaseArchiveDirectory, StagePlatform.ToString());
					}
				}

				List<StageTarget> TargetsToStage = new List<StageTarget>();

                //@todo should pull StageExecutables from somewhere else if not cooked
                DeploymentContext SC = new DeploymentContext(Params.RawProjectPath, CmdEnv.LocalRoot,
                    StageDirectory,
                    ArchiveDirectory,
                    Platform.Platforms[CookedDataPlatform],
                    Platform.Platforms[StagePlatform],
					ConfigsToProcess,
					TargetsToStage,
					ExecutablesToStage,
					InDedicatedServer,
					Params.Cook || Params.CookOnTheFly,
					Params.CrashReporter && !(StagePlatform.Type == UnrealTargetPlatform.Linux), // can't include the crash reporter from binary Linux builds
					Params.Stage,
					Params.CookOnTheFly,
					Params.Archive,
					Params.IsProgramTarget,
					Params.HasDedicatedServerAndClient,
					Params.Manifests
					);
				Project.LogDeploymentContext(SC);

				// If we're a derived platform make sure we're at the end, otherwise make sure we're at the front

				if (!CookedDataPlatform.Equals(StagePlatform))
				{
					DeploymentContexts.Add(SC);
				}
				else
				{
					DeploymentContexts.Insert(0, SC);
				}
			}

			return DeploymentContexts;
		}

		public override void ExecuteBuild()
		{
			VersionString = ParseParamValue("Version", "NOVERSION");

			DLCName = ParseParamValue("DLCName", "PeteGameMode");

			// Maps should be in format -maps=DM-DLCMap1+DM-DLCMap2+DM-DLCMap3
			DLCMaps = ParseParamValue("Maps", "");

			// Right now all platform asset registries seem to be the exact same, this may change in the future
			AssetRegistry = ParseParamValue("ReleaseVersion", "UTVersion0");

			ProjectParams Params = GetParams(this, DLCName, AssetRegistry);

			if (ParseParam("build"))
			{
				Project.Build(this, Params);
			}

			// Cook dedicated server configs
			if (Params.DedicatedServer)
			{
				var DeployContextServerList = CreateDeploymentContext(Params, true, true);
				foreach (var SC in DeployContextServerList)
				{
					if (!ParseParam("skipcook"))
					{
						Cook(SC, Params);
					}
					Stage(SC, Params);
				}
			}

			// Cook client configs
			var DeployClientContextList = CreateDeploymentContext(Params, false, true);
			foreach (var SC in DeployClientContextList)
			{
				if (!ParseParam("skipcook"))
				{
					Cook(SC, Params);
				}
				Stage(SC, Params);
			}
		}
	}

	public class UTTweet : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string TweetText = ParseParamValue("Tweet", "Test tweet from RunUAT");

			UnrealTournamentBuild.Tweet(TweetText);
		}
	}
}