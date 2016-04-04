using AutomationTool;
using EpicGames.MCP.Automation;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace UnrealTournamentGame.Automation
{
	public class GameServerManager
	{
		private string D2BaseUri { get; set; }
		private string S3BucketName { get; set; }

		public bool Debug { get; private set; }
		public string GameName { get; private set; }
		public string GameNameShort { get; private set; }
		public string BuildString { get; private set; }
		public UnrealTournamentBuild.UnrealTournamentAppName AppName { get; set; }
		public int Changelist { get; set; }
		public int CpuBudget { get; set; }
		public int RamBudget { get; set; }
		public string HTTPAuthBase64 { get; set; }

		public GameServerManager(UnrealTournamentBuild.UnrealTournamentAppName InAppName, string InS3BucketName, string InBuildString, bool InDebug = false)
		{
			this.GameName = "unrealtournament";
			this.GameNameShort = "ut";
			this.AppName = InAppName;
			this.S3BucketName = InS3BucketName;
			this.BuildString = InBuildString;
			this.Debug = InDebug;
			this.CpuBudget = 200;
			this.RamBudget = 200;

			FEngineVersionSupport ParsedVersion = FEngineVersionSupport.FromString(InBuildString, bAllowNoVersion: true);
			this.Changelist = ParsedVersion.Changelist;

			// using stage only while testing this for the first time.
			this.D2BaseUri = "https://fleet-manager-ci.ol.epicgames.net:8080/v1/";

			//if (AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest || AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevStage)
			//{
			//	// test the stage endpoint for DevPlaytest and DevStage
			//	this.D2BaseUri = "https://fleet-manager-stage.ol.epicgames.net:8080/v1/";
			//}
			//else 
			//{
			//	// Use the Prod endpoint for everything else.
			//	this.D2BaseUri = "https://fleet-manager.ol.epicgames.net:8080/v1/";
			//}
		}

		public void DistributeImagesAsync()
		{

			string GceArgs = Deployment2GceArgs(Zone: "us-central1-c", Region: "NONE");

			CommandUtils.Log("Uploading build {0} to s3", BuildString);
			if (Debug == false)
			{
				DeployLinuxServerS3(BuildString);
			}

			// TODO once UnrealTournament Live lable exists, we'll need to
			// create GCE images in the Live and Dev projects
			CommandUtils.Log("Creating VM image in {0} for build {1}", AppName.ToString(), BuildString);
			Deployment2Command("image_create", GceArgs, "true", 1);
		}

		public void DistributeImagesWait(int MaxRetries)
		{
			string GceArgs = Deployment2GceArgs(Zone: "us-central-1c", Region: "NONE");
			CommandUtils.Log("Waiting for VM image creation in {0} to complete", AppName.ToString());

			// TODO once UnrealTournament Live lable exists, we'll need to
			// create GCE images in the Live and Dev projects
			Deployment2Command("image_create", GceArgs, "get_pending", MaxRetries);
		}

		public void DeployNewFleets(int MaxRetries)
		{
			string GceArgsNa1 = Deployment2GceArgs(tag: "Gce1", MachineType: "n1-standard-8", Zone: "us-central1-c", Region: "NA");
			CommandUtils.Log("Deploying new fleets for change list {0}", Changelist);

			Deployment2Command("deployment_create", GceArgsNa1, "true", 1);

			Deployment2Command("deployment_create", GceArgsNa1, "get_pending", MaxRetries);
		}

		public void DegradeOldFleets(int MaxRetries)
		{
			string GceArgsNa1 = Deployment2GceArgs(tag: "Gce1", MachineType: "n1-standard-8", Zone: "us-central1-c", Region: "NA");
			CommandUtils.Log("Degrading old fleets");

			Deployment2Command("deployment_degrade_old", GceArgsNa1, "true", 1);
			Deployment2Command("deployment_degrade_old", GceArgsNa1, "get_pending", MaxRetries);
		}

		private string GetUtilitiesFilePath(string Filename)
		{
			return CommandUtils.CombinePaths(BuildPatchToolStagingInfo.GetBuildRootPath(), "Utilities", Filename);
		}

		private string ReadFileContents(string FilePath)
		{
			try
			{
				string FileContents = System.IO.File.ReadAllText(FilePath);
				if (string.IsNullOrEmpty(FileContents))
				{
					throw new AutomationException("Unable to retrieve contents from file {0}, no content found", FilePath);
				}
				return FileContents;
			}
			catch (IOException ex)
			{
				throw new AutomationException("Unable to retrieve contents from file {0}. Exception {1}", FilePath, ex);
			}
		}

		// Eventually there will be functions to create args for other cloud providers... Like AWS and Tencent.
		private string Deployment2GceArgs(string tag = "",
										  string MachineType = "n1-standard-16",
										  string Zone = "us-central1-a",
										  string Region = "NA",
										  int InstanceSize = 1,
										  string ExtraCliArgs = "",
										  string AppNameStringOverride = "")
		{
			UnrealTournamentBuild.UnrealTournamentAppName LocalAppName = AppName;
			if( ! String.IsNullOrEmpty(AppNameStringOverride))
			{
				if ( ! Enum.IsDefined(typeof(UnrealTournamentBuild.UnrealTournamentAppName), AppNameStringOverride))
				{
					throw new AutomationException("Trying to override AppName to non-existant type: " + AppNameStringOverride);
				}
				LocalAppName = (UnrealTournamentBuild.UnrealTournamentAppName) Enum.Parse(typeof(UnrealTournamentBuild.UnrealTournamentAppName), AppNameStringOverride);
				CommandUtils.Log("Overriding AppName from {0} to {1}", AppName, LocalAppName);
			}

			string NetworkId = "default";
			string CredentialsFile = "GceUtDevCredentials.txt";

			// once live labels exist, enable deployment into live GCE project.
			//if (LocalAppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLive || LocalAppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentLive)
			//{
			//	NetworkId = "unreal-tournament";
			//	CredentialsFile = "GceUtLiveCredentials.txt";
			//}

			string CredentialsFilePath = GetUtilitiesFilePath(CredentialsFile);
			string CredentialsFileContentsBase64 = ReadFileContents(CredentialsFilePath);
			string CredentialsFileContents = ConvertFromBase64(CredentialsFileContentsBase64);
			GceCredentials gceCredentials = GceCredentialsFromJson(CredentialsFileContents);

			string HTTPCredentialsFilePath = GetUtilitiesFilePath("DeployerAuthBase64.txt");
			if (File.Exists(HTTPCredentialsFilePath))
			{
				string HTTPCredentialsFileContentsBase64 = ReadFileContents(HTTPCredentialsFilePath);
				HTTPAuthBase64 = HTTPCredentialsFileContentsBase64;
			}

			Byte[] CliArgBytes = System.Text.Encoding.UTF8.GetBytes("UnrealTournament ut-entry?game=empty?region="+Region+" -epicapp=" + LocalAppName.ToString() + " " + ExtraCliArgs);
			Byte[] BuildStringBytes = System.Text.Encoding.UTF8.GetBytes(BuildString);
			string CloudArgs = "epic_game_name=" + GameNameShort + "&" +
				"epic_game_tag=" + GetTag(LocalAppName, Region, tag) + "&" +
				"epic_game_version=" + Changelist.ToString() + "&" +
				"epic_game_region=" + Region + "&" +
				"epic_game_cpu_budget=" +RamBudget +"&" +
				"epic_game_ram_budget=" + CpuBudget + "&" +
				"epic_game_buildstr_base64=" + System.Convert.ToBase64String(BuildStringBytes) + "&" +
				"epic_instance_size=" + InstanceSize.ToString() + "&" +
				"epic_gce_project_id=" + gceCredentials.project_id + "&" +
				"epic_gce_base64_credentials=" + CredentialsFileContentsBase64 + "&" +
				"epic_gce_machine_type=" + MachineType + "&" +
				"epic_gce_network=" + NetworkId + "&" +
				"epic_gce_zone=" + Zone + "&" +
				"epic_game_base64_cli_args=" + System.Convert.ToBase64String(CliArgBytes);

			return CloudArgs;
		}

		private String GetTag(UnrealTournamentBuild.UnrealTournamentAppName InAppName, String InRegion, String InTag)
		{
			string AppEnvironment = AppEnvironment = InAppName.ToString().Substring(GameName.Length);
			
			string Tag = String.Format("{0}{1}{2}",
				AppEnvironment,
				InRegion,
				InTag
				);

			if (Debug == true)
			{
				Tag = Tag + "debug";
			}
			return Tag;
		}

		private HttpStatusCode Deployment2SendHelper(string D2Function, string CloudArgs, string D2AsyncType)
		{
			string DistributionUri = D2BaseUri + D2Function;
			HttpWebRequest req = (HttpWebRequest)WebRequest.Create(DistributionUri);
			if ( ! String.IsNullOrEmpty(HTTPAuthBase64))
			{
				req.Headers.Add("Authorization", "Basic " + HTTPAuthBase64);
			}

			// tack on the async type to the end of the cloud args. 
			CloudArgs = CloudArgs + "&epic_async=" + D2AsyncType;
			CommandUtils.Log("function {0} to {1} with rest api args: {2}", D2Function, D2BaseUri, CloudArgs);
			Byte[] bytes = Encoding.UTF8.GetBytes(CloudArgs);
			req.ContentLength = bytes.Length;
			req.Method = "POST";
			req.ContentType = "application/x-www-form-urlencoded";

			// [VOSSEL] We are using a self signed certificate. To do this we need a security exception for this cert
			// so we can use https. 
			ServicePointManager.ServerCertificateValidationCallback += (sender, certificate, chain, sslPolicyErrors) =>
			{
				return sslPolicyErrors == System.Net.Security.SslPolicyErrors.None ||
											certificate.GetCertHashString() == "39C55A5EC07FF9CD08D9A6DA26F790508E876D0E"; // Deployment 2.0 hash
			};

			using (Stream dataStream = req.GetRequestStream())
			{
				dataStream.Write(bytes, 0, bytes.Length);
				dataStream.Close();
			}

			try
			{
				HttpWebResponse response;
				using (response = (HttpWebResponse)req.GetResponse())
				{
					// we always expect one of these return codes regardless if this is async or not.
					if (response.StatusCode != HttpStatusCode.OK && response.StatusCode != HttpStatusCode.Accepted)
					{
						throw new AutomationException("Deployment 2 function {0} for {1} failed. ResponseDesc = {2}. Response = {3}. BaseUri = {4}.",
							D2Function,
							Changelist,
							response.StatusDescription,
							Encoding.UTF8.GetString(response.GetResponseStream().ReadIntoMemoryStream().ToArray()),
							D2BaseUri);
					}

					CommandUtils.Log("Response Code: {0} Response String: {1}", response.StatusCode, Encoding.UTF8.GetString(response.GetResponseStream().ReadIntoMemoryStream().ToArray()));
					return response.StatusCode;
				}
			}
			catch (WebException ex)
			{
				if (ex.Response == null)
				{
					throw new AutomationException("Deployment 2.0 function {0}. Unexpected error retrieving response. BaseUrl = {1}. Status = {2}. Exception = {3}.",
							D2Function,
							D2BaseUri,
							ex.Status,
							ex);
				}
				string WebErrorMessage = Encoding.UTF8.GetString(ex.Response.GetResponseStream().ReadIntoMemoryStream().ToArray());
				throw new AutomationException(ex, "Deployment 2.0 function {0} failed. BaseUrl = {1}. ResponseDesc = {2}. Response = {3}. ResponseStatus = {4}.",
						D2Function,
						D2BaseUri,
						((HttpWebResponse)ex.Response).StatusDescription,
						WebErrorMessage,
						ex.Status);
			}
		}

		private void Deployment2Command(string D2Function, string CloudArgs, string D2AsyncType, int MaxRetries)
		{
			if (string.IsNullOrEmpty(D2BaseUri))
			{
				throw new AutomationException("Could not determine Deployment 2.0 Base Uri to set build {0} live on.", Changelist);
			}
			if (string.IsNullOrEmpty(D2AsyncType))
			{
				throw new AutomationException("D2AsyncType must be set (true, false, get_pending, no_wait) in order to issue Deployment 2.0 request");
			}
			if (string.IsNullOrEmpty(CloudArgs))
			{
				throw new AutomationException("No Cloud arguments found for function {0}", D2Function);
			}

			bool bSuccess = false;
			int Retries = 0;
			do
			{
				HttpStatusCode StatusCode = Deployment2SendHelper(D2Function, CloudArgs, D2AsyncType);

				switch (StatusCode)
				{
					case HttpStatusCode.Accepted:
						if (D2AsyncType.Equals("get_pending"))
						{
							// fall through. This is the only condition we are allowing the retry
							// loop to occur for. When get_pending returns 200 OK we know the action
							// has completed. 202 Accepted means the server is still processing it.
						}
						else
						{
							bSuccess = true;
						}
						break;
					case HttpStatusCode.OK:
						bSuccess = true;
						break;
					default:
						// if unexpected return code, bail immediately
						throw new AutomationException("Unexpected status code {0}", StatusCode);
				}

				if (bSuccess)
				{
					break;
				}
				CommandUtils.Log("Retrying deployment command {0} for {1}. async is set to {2}", D2Function, Changelist, D2AsyncType);
				Thread.Sleep(1000);

			} while (Retries++ < MaxRetries);

			if (!bSuccess)
			{
				// If we hit this, get_pending hit max retries. That's the only possible outcome to make it this far.
				throw new AutomationException("Max retries hit while waiting for async result. Function: {0} MaxRetries: {1}",
					D2Function, MaxRetries);
			}
		}

		public void DeployLinuxServerS3(string BuildVersion)
		{
			string BuildBaseDir = CommandUtils.CombinePaths(
				BuildPatchToolStagingInfo.GetBuildRootPath(),
				GameName,
				BuildVersion
			);
			string LinuxServerBaseDir = CommandUtils.CombinePaths(BuildBaseDir, "LinuxServer");
			string ServerZipFilePath = CommandUtils.CombinePaths(BuildBaseDir, "LinuxServer.zip");

			if (CommandUtils.FileExists_NoExceptions(ServerZipFilePath))
			{
				CommandUtils.Log("Skipping creating server zip file {0}, as it already exists.", ServerZipFilePath);
			}
			else
			{
				CommandUtils.Log("Compressing Linux server binaries to {0}", ServerZipFilePath);
				CommandUtils.ZipFiles(ServerZipFilePath, LinuxServerBaseDir, new FileFilter(FileFilterType.Include));
				CommandUtils.Log("Completed compressing Linux server binaries.");
			}

			S3Configuration S3Config = new S3Configuration(this);
			CloudStorageBase S3 = S3Config.GetConnection();

			string S3Filename = string.Format("LinuxServer-{0}.zip", Changelist);
			bool bSuccess = false;
			int Retries = 0;
			int MaxRetries = 5;
			do
			{
				CommandUtils.Log("Uploading server binaries zip file to Amazon S3 bucket {0}.", S3BucketName);
				bSuccess = S3.PostFile(S3BucketName, S3Filename, ServerZipFilePath, "application/zip").bSuccess;
				if (!bSuccess)
				{
					bool bDoRetry = Retries + 1 < MaxRetries;
					CommandUtils.LogWarning("Failed to post server binaries to S3 (attempt {0} of {1}). {2}.",
											Retries + 1,
											MaxRetries,
											bDoRetry ? "Sleeping for ten seconds before retrying" : "Not retrying"
					);
					if (bDoRetry)
					{
						Thread.Sleep(10000);
					}
				}
			} while (!bSuccess && Retries++ < MaxRetries);

			if (!bSuccess)
			{
				throw new AutomationException("Could not upload server binaries to S3.");
			}

			CommandUtils.Log("Server binaries uploaded successfully to S3.");
		}

		public class S3Configuration
		{
			private CloudStorageBase Connection;
			private readonly object m_lock = new object();

			public S3Configuration(GameServerManager manager)
			{
				CredentialsFilePath = CommandUtils.CombinePaths(BuildPatchToolStagingInfo.GetBuildRootPath(), "Utilities", "S3Credentials.txt");
				CredentialsKey = string.Format("{0}_server", manager.GameName).ToLower();
				AWSRegion = "us-east-1";
				CommandUtils.Log("Using credentials key {0} for region {1} from {2}", CredentialsKey, AWSRegion, CredentialsFilePath);
			}

			public S3Configuration(string InCredentialsFilePath, string InAWSRegion, string InCredentialsKey)
			{
				CredentialsFilePath = InCredentialsFilePath;
				AWSRegion = InAWSRegion;
				CredentialsKey = InCredentialsKey;
				CommandUtils.Log("Using credentials key {0} for region {1} from {2} (passed in)", CredentialsKey, AWSRegion, CredentialsFilePath);
			}

			public string CredentialsFilePath { get; private set; }
			public string AWSRegion { get; private set; }
			public string CredentialsKey { get; private set; }

			public CloudStorageBase GetConnection()
			{
				if (Connection == null)
				{
					lock (m_lock)
					{
						if (Connection == null)
						{
							Connection = CloudStorageBase.GetByName("GameServerManager");
							Dictionary<string, object> CloudConfiguration = new Dictionary<string, object>
							{
								{ "CredentialsFilePath", CredentialsFilePath },
								{ "CredentialsKey",	  CredentialsKey },
								{ "AWSRegion",		   AWSRegion }
							};
							Connection.Init(CloudConfiguration);
						}
					}
				}
				return Connection;
			}
		}

		[DataContract]
		class GceCredentials
		{
			[DataMember]
			public String type = "";

			[DataMember(IsRequired=true)]
			public String project_id = "";

			[DataMember(IsRequired=true)]
			public String private_key_id = "";

			[DataMember(IsRequired=true)]
			public String private_key = "";

			[DataMember]
			public String client_email = "";

			[DataMember]
			public String client_id = "";

			[DataMember]
			public String auth_uri = "";

			[DataMember]
			public String token_uri = "";

			[DataMember]
			public String auth_provider_x509_cert_url = "";

			[DataMember]
			public String client_x509_cert_url = "";
		}

		GceCredentials GceCredentialsFromJson(String InString)
		{
			try
			{
				DataContractJsonSerializer Serializer = new DataContractJsonSerializer(typeof(GceCredentials));
				MemoryStream Stream = new MemoryStream(System.Text.UTF8Encoding.UTF8.GetBytes(InString));
				GceCredentials Credential = (GceCredentials)Serializer.ReadObject(Stream);

				return Credential;
			}
			catch (Exception ex)
			{
				throw new AutomationException(ex, "Failed to deserialize GceCredentials. Ex {0}");
			}
		}

		String ConvertFromBase64(String Base64Content)
		{
			try
			{
				byte[] Base64Data = Convert.FromBase64String(Base64Content);
				String Data = Encoding.UTF8.GetString(Base64Data);

				return Data;
			}
			catch (Exception ex)
			{
				throw new AutomationException(ex, "Failed to decode Base64 content. Ex {0}");
			}
		}
	}
}
