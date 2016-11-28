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
		private int AsyncId { get; set; }
		private string GameBinary { get; set; }
		private string GameBinaryPath { get; set; }
		private string GameLogPath { get; set; }
		private string GameSavedPath { get; set; }
		private int MaxMatchLength { get; set; }
		private int TtlInterval { get; set; }
		private string InstallSumo { get; set; }

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
			this.InstallSumo = "true";
			this.GameBinaryPath = "Engine/Binaries/Linux";
			this.GameLogPath = "/UnrealTournament/Saved/Logs";
			this.GameSavedPath = "/UnrealTournament/Saved";
			this.MaxMatchLength = 0; /* value is in seconds. 0 disables ttl */
			this.TtlInterval = 0; /* idle server refresh time in seconds.  0 disables ttl*/
			this.GameBinary = "UE4Server-Linux-Shipping";
			this.CpuBudget = 33;
			this.RamBudget = 55;

			FEngineVersionSupport ParsedVersion = FEngineVersionSupport.FromString(InBuildString, bAllowNoVersion: true);
			this.Changelist = ParsedVersion.Changelist;

            if (this.Debug)
            {
                this.D2BaseUri = "https://fleet-manager-ci.ol.epicgames.net:8080/v1/";
            }
            else if (AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevStage ||
				AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest ||
				AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest)
            {
                this.D2BaseUri = "https://fleet-manager-stage.ol.epicgames.net:8080/v1/";
            }
            else
            {
                this.D2BaseUri = "https://fleet-manager.ol.epicgames.net:8080/v1/";
            }

			switch (AppName)
			{
				case UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev:
				case UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest:
					this.GameBinary = "UE4Server-Linux-Shipping";
					break;
				case UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDevPlaytest:
					this.GameBinary = "UE4Server-Linux-Test";
					break;
				default:
					this.GameBinary = "UE4Server-Linux-Shipping";
					break;
			}

			Random rnd = new Random();
			this.AsyncId = rnd.Next();
		}

		public void DistributeImagesAsync()
		{
			string AwsArgs = Deployment2AwsArgs(Region: "NONE");
			string GceArgs = Deployment2GceArgs(Zone: "us-central1-c", Region: "NONE");
			string GceLiveArgs = Deployment2GceArgs(Zone: "us-central1-c", Region: "NONE", AppNameStringOverride: "UnrealTournamentDev");

			CommandUtils.Log("Uploading build {0} to s3", BuildString);
			if (Debug == false)
			{
				DeployLinuxServerS3(BuildString);
			}

			CommandUtils.Log("Creating VM image in {0} for build {1}", AppName.ToString(), BuildString);
			if (AppName != UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
			{
				Deployment2Command("image_create", GceLiveArgs, "true", 1);
			}
			Deployment2Command("image_create", GceArgs, "true", 1);
			Deployment2Command("image_create", AwsArgs, "true", 1);
			
		}

		public void DistributeImagesWait(int MaxRetries)
		{
			string AwsArgs = Deployment2AwsArgs(Region: "NONE");
			string GceArgs = Deployment2GceArgs(Zone: "us-central-1c", Region: "NONE");
			string GceLiveArgs = Deployment2GceArgs(Zone: "us-central1-c", Region: "NONE", AppNameStringOverride: "UnrealTournamentDev");

			CommandUtils.Log("Waiting for VM image creation in {0} to complete", AppName.ToString());
			if (AppName != UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
			{
				Deployment2Command("image_create", GceLiveArgs, "get_pending", MaxRetries);
			}
			Deployment2Command("image_create", GceArgs, "get_pending", MaxRetries);
			Deployment2Command("image_create", AwsArgs, "get_pending", MaxRetries);
		}

		public void DeployNewFleets(int MaxRetries)
		{
			HandleDeployment(MaxRetries, "deployment_create");
		}
		public void DegradeOldFleets(int MaxRetries)
		{
			HandleDeployment(MaxRetries, "deployment_degrade_old");
		}

		public void HandleDeployment(int MaxRetries, string cmd)
		{
			string GceMachineType = "n1-standard-4";
			string AwsMachineType = "c4.2xlarge";
			string AwsSmallerMachineType = "c4.2xlarge";
			string AwsBraMachineType = "c3.2xlarge";

			if (AppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
			{
				GceMachineType = "n1-standard-32";
				AwsMachineType = "c4.8xlarge";
				AwsSmallerMachineType = "c4.4xlarge";
				AwsBraMachineType = "c3.4xlarge";
			}

			/* Hubs */
			string GceNaHub1 = Deployment2GceArgs(tag: "hub1", MachineType: GceMachineType, Zone: "us-central1-c", Region: "NA", HubServerName: "USA (Central) Hub 1");
			string GceNaHub2 = Deployment2GceArgs(tag: "hub2", MachineType: GceMachineType, Zone: "us-central1-b", Region: "NA", HubServerName: "USA (Central) Hub 2");
			string GceNaHub3 = Deployment2GceArgs(tag: "hub3", MachineType: GceMachineType, Zone: "us-central1-c", Region: "NA", HubServerName: "USA (Central) Hub 3");
			string GceEuHub1 = Deployment2GceArgs(tag: "hub1", MachineType: GceMachineType, Zone: "europe-west1-d", Region: "EU", HubServerName: "BEL (St. Ghislain) Hub 1");
			string GceEuHub2 = Deployment2GceArgs(tag: "hub2", MachineType: GceMachineType, Zone: "europe-west1-c", Region: "EU", HubServerName: "BEL (St. Ghislain) Hub 2");
			string GceEuHub3 = Deployment2GceArgs(tag: "hub3", MachineType: GceMachineType, Zone: "europe-west1-d", Region: "EU", HubServerName: "BEL (St. Ghislain) Hub 3");
			string AwsNaHub1 = Deployment2AwsArgs(AwsRegion: "us-east-1", tag: "AwsHub1", Region: "NA", InstanceType: AwsMachineType, HubServerName: "USA (East) Hub 1");
			string AwsNaHub2 = Deployment2AwsArgs(AwsRegion: "us-west-1", tag: "AwsHub2", Region: "NA", InstanceType: AwsMachineType, HubServerName: "USA (West) Hub 1");
			string AwsEuHub1 = Deployment2AwsArgs(AwsRegion: "eu-central-1", tag: "AwsHub1", Region: "EU", InstanceType: AwsMachineType, HubServerName: "GER (Frankfurt) Hub 1");
			string AwsEuHub2 = Deployment2AwsArgs(AwsRegion: "eu-central-1", tag: "AwsHub2", Region: "EU", InstanceType: AwsMachineType, HubServerName: "GER (Frankfurt) Hub 2");
			string AwsAuHub1 = Deployment2AwsArgs(AwsRegion: "ap-southeast-2", tag: "AwsHub1", Region: "AU", InstanceType: AwsSmallerMachineType, HubServerName: "AUS (Sydney) Hub 1");
			string AwsSaHub1 = Deployment2AwsArgs(AwsRegion: "sa-east-1", tag: "AwsHub1", Region: "SA", InstanceType: AwsBraMachineType, HubServerName: "BRA (Sao Paulo) Hub 1");

			/* Match Making */
			string GceArgsNa1 = Deployment2GceArgs(tag: "GceMM1", MachineType: GceMachineType, Zone: "us-central1-c", Region: "NA");
			string GceArgsNa2 = Deployment2GceArgs(tag: "GceMM2", MachineType: GceMachineType, Zone: "us-central1-b", Region: "NA");
			string GceArgsEu1 = Deployment2GceArgs(tag: "GceMM1", MachineType: GceMachineType, Zone: "europe-west1-c", Region: "EU");
			string GceArgsEu2 = Deployment2GceArgs(tag: "GceMM2", MachineType: GceMachineType, Zone: "europe-west1-d", Region: "EU");
			string AwsArgsNa1 = Deployment2AwsArgs(tag: "AwsMM1", InstanceType: AwsMachineType, AwsRegion: "us-east-1", Region: "NA");
			string AwsArgsEu1 = Deployment2AwsArgs(tag: "AwsMM1", InstanceType: AwsMachineType, AwsRegion: "eu-central-1", Region: "EU");

			CommandUtils.Log("Deploying new fleets for change list {0}", Changelist);

			if (cmd == "deployment_create")
			{
				switch (AppName)
				{
					case UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev:
						// deploy mm servers
						Deployment2Command("deployment_create", AwsArgsNa1, "true", 1);
						Deployment2Command("deployment_create", AwsArgsEu1, "true", 1);
						// deploy hubs
						Deployment2Command("deployment_create", AwsNaHub1, "true", 1);
						Deployment2Command("deployment_create", AwsNaHub2, "true", 1);
						Deployment2Command("deployment_create", AwsEuHub1, "true", 1);
						Deployment2Command("deployment_create", AwsEuHub2, "true", 1);
						Deployment2Command("deployment_create", AwsAuHub1, "true", 1);
						Deployment2Command("deployment_create", AwsSaHub1, "true", 1);

						// verify MM servers deployed
						Deployment2Command("deployment_create", AwsArgsNa1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsArgsEu1, "get_pending", MaxRetries);
						// verify Hubs deployed
						Deployment2Command("deployment_create", AwsNaHub1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsNaHub2, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsEuHub1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsEuHub2, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsAuHub1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsSaHub1, "get_pending", MaxRetries);
						break;
					case UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentPublicTest:
						Deployment2Command("deployment_create", AwsArgsNa1, "true", 1);
						Deployment2Command("deployment_create", AwsArgsEu1, "true", 1);
						Deployment2Command("deployment_create", AwsNaHub1, "true", 1);
						Deployment2Command("deployment_create", AwsEuHub1, "true", 1);

						Deployment2Command("deployment_create", AwsArgsNa1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsArgsEu1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsNaHub1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsEuHub1, "get_pending", MaxRetries);
						break;
					default:
						Deployment2Command("deployment_create", AwsArgsNa1, "true", 1);
						Deployment2Command("deployment_create", AwsNaHub1, "true", 1);
						Deployment2Command("deployment_create", AwsEuHub1, "true", 1);

						Deployment2Command("deployment_create", AwsArgsNa1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsNaHub1, "get_pending", MaxRetries);
						Deployment2Command("deployment_create", AwsEuHub1, "get_pending", MaxRetries);
						break;
				}
			}
			else if (cmd == "deployment_degrade_old")
			{
				CommandUtils.Log("Degrading old fleets");
				Deployment2Command("deployment_degrade_old", GceArgsNa1, "true", 1);
				Deployment2Command("deployment_degrade_old", GceArgsNa2, "true", 1);
				Deployment2Command("deployment_degrade_old", GceArgsEu1, "true", 1);
				Deployment2Command("deployment_degrade_old", GceArgsEu2, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsArgsNa1, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsArgsEu1, "true", 1);
				Deployment2Command("deployment_degrade_old", GceNaHub1, "true", 1);
				Deployment2Command("deployment_degrade_old", GceNaHub2, "true", 1);
				Deployment2Command("deployment_degrade_old", GceNaHub3, "true", 1);
				Deployment2Command("deployment_degrade_old", GceEuHub1, "true", 1);
				Deployment2Command("deployment_degrade_old", GceEuHub2, "true", 1);
				Deployment2Command("deployment_degrade_old", GceEuHub3, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsNaHub1, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsNaHub2, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsEuHub1, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsEuHub2, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsAuHub1, "true", 1);
				Deployment2Command("deployment_degrade_old", AwsSaHub1, "true", 1);

				Deployment2Command("deployment_degrade_old", GceArgsNa1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceArgsNa2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceArgsEu1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceArgsEu2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsArgsNa1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsArgsEu1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceNaHub1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceNaHub2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceNaHub3, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceEuHub1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceEuHub2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", GceEuHub3, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsNaHub1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsNaHub2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsEuHub1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsEuHub2, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsAuHub1, "get_pending", MaxRetries);
				Deployment2Command("deployment_degrade_old", AwsSaHub1, "get_pending", MaxRetries);
			}
			else
			{
				CommandUtils.Log("Unknown fleet-manager command " + cmd);
			}
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

		private string Deployment2AwsArgs(string tag = "",
										  string InstanceType = "c4.4xlarge",
										  string AwsRegion = "us-east-1", /* AU ap-southeast-2. KR ap-northeast-2 */
										  string Region = "NA", /* KR South Korea. AU Australia */
										  int InstanceSize = 1,
										  int InstanceSizeMin = 1,
										  int InstanceSizeMax = 6,
										  string AutoscalerId = "default",
										  string ExtraCliArgs = "",
										  string HubServerName = "")
		{
			string AwsCredentialsFile = "AwsDedicatedServerCredentialsDecoded.txt";

			int LocalCpuBudget = CpuBudget;
			if (InstanceSize == 0)
			{
				InstanceSizeMin = 0;
				InstanceSizeMax = 0;
			}
			else if (InstanceSizeMin > InstanceSize)
			{
				InstanceSize = InstanceSizeMin;
			}

			string CredentialsFilePath = GetUtilitiesFilePath(AwsCredentialsFile);
			string AwsCredentials = ReadFileContents(CredentialsFilePath);
			string[] AwsCredentialsList = AwsCredentials.Split(':');

			if (AwsCredentialsList.Length != 2)
			{
				throw new AutomationException("Unable to parse credentials from file {0} {1}",
					AwsCredentialsFile, AwsCredentials);
			}

			string AwsAccessKey = AwsCredentialsList[0];
			Byte[] AwsAccessSecretKeyBytes = System.Text.Encoding.UTF8.GetBytes(AwsCredentialsList[1]);

			Byte[] CliArgBytes;
			if (HubServerName != "")
			{
				CliArgBytes = System.Text.Encoding.UTF8.GetBytes("UnrealTournament ut-entry?game=lobby?ServerName=\""+HubServerName+"\" -nocore -epicapp=" + AppName.ToString() + " " + ExtraCliArgs);
				LocalCpuBudget=32000;
			}
			else
			{
				CliArgBytes = System.Text.Encoding.UTF8.GetBytes("UnrealTournament ut-entry?game=empty?region="+Region+" -epicapp=" + AppName.ToString() + " " + ExtraCliArgs);
			}
			Byte[] BuildStringBytes = System.Text.Encoding.UTF8.GetBytes(BuildString);

			string CloudArgs = "epic_game_name=" + GameNameShort + "&" +
                "epic_game_binary=" + this.GameBinary + "&" +
				"epic_game_binary_path=" + this.GameBinaryPath + "&" +
				"epic_game_log_path=" + this.GameLogPath + "&" +
				"epic_game_saved_path=" + this.GameSavedPath + "&" +
				"epic_game_max_match_length=" + this.MaxMatchLength.ToString() + "&" +
				"epic_game_ttl_interval=" + this.TtlInterval.ToString() + "&" +
				"epic_install_sumo=" + this.InstallSumo + "&" +
				"epic_game_tag=" + GetTag(AppName, Region, tag) + "&" +
				"epic_game_version=" + Changelist.ToString() + "&" +
				"epic_game_buildstr_base64=" + System.Convert.ToBase64String(BuildStringBytes) + "&" +
				"epic_game_cpu_budget=" + LocalCpuBudget +"&" +
				"epic_game_ram_budget=" + RamBudget + "&" +
				"epic_game_environment=" + AppName.ToString() + "&" +
				"epic_instance_size=" + InstanceSize.ToString() + "&" +
				"epic_instance_min=" + InstanceSizeMin.ToString() + "&" +
				"epic_instance_max=" + InstanceSizeMax.ToString() + "&" +
				"epic_autoscaler_id=" + AutoscalerId + "&" +
				"epic_aws_access_key=" + AwsAccessKey + "&" +
				"epic_aws_base64_secret_key=" + System.Convert.ToBase64String(AwsAccessSecretKeyBytes) + "&" +
				"epic_aws_instance_type=" + InstanceType + "&" +
				"epic_aws_region=" + AwsRegion + "&" +
				"epic_game_base64_cli_args=" + System.Convert.ToBase64String(CliArgBytes);

			return CloudArgs;
		}

		private string Deployment2GceArgs(string tag = "",
										  string MachineType = "n1-standard-16",
										  string Zone = "us-central1-a",
										  string Region = "NA",
										  int InstanceSize = 1,
										  int InstanceSizeMin = 1,
										  int InstanceSizeMax = 6,
										  string AutoscalerId = "default",
										  string ExtraCliArgs = "",
										  string AppNameStringOverride = "",
										  string HubServerName = "")
		{
			int LocalCpuBudget = CpuBudget;
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

			/* This is very confusing. UnrealTournament's live lable is UnrealTournamentDev. */
			if (LocalAppName == UnrealTournamentBuild.UnrealTournamentAppName.UnrealTournamentDev)
			{
				NetworkId = "unreal-tournament";
				CredentialsFile = "GceUtLiveCredentials.txt";
			}

			if (InstanceSize == 0)
			{
				InstanceSizeMin = 0;
				InstanceSizeMax = 0;
			}
			else if (InstanceSizeMin > InstanceSize)
			{
				InstanceSize = InstanceSizeMin;
			}

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

			Byte[] CliArgBytes;
			if (HubServerName != "")
			{
				CliArgBytes = System.Text.Encoding.UTF8.GetBytes("UnrealTournament ut-entry?game=lobby?ServerName=\""+HubServerName+"\" -nocore -epicapp=" + LocalAppName.ToString() + " " + ExtraCliArgs);
				LocalCpuBudget=32000;
			}
			else
			{
				CliArgBytes = System.Text.Encoding.UTF8.GetBytes("UnrealTournament ut-entry?game=empty?region="+Region+" -epicapp=" + LocalAppName.ToString() + " " + ExtraCliArgs);
			}
			Byte[] BuildStringBytes = System.Text.Encoding.UTF8.GetBytes(BuildString);
			string CloudArgs = "epic_game_name=" + GameNameShort + "&" +
                "epic_game_binary=" + this.GameBinary + "&" +
				"epic_game_binary_path=" + this.GameBinaryPath + "&" +
				"epic_game_log_path=" + this.GameLogPath + "&" +
				"epic_game_saved_path=" + this.GameSavedPath + "&" +
				"epic_game_max_match_length=" + this.MaxMatchLength.ToString() + "&" +
				"epic_game_ttl_interval=" + this.TtlInterval.ToString() + "&" +
				"epic_install_sumo=" + this.InstallSumo + "&" +
				"epic_game_tag=" + GetTag(LocalAppName, Region, tag) + "&" +
				"epic_game_version=" + Changelist.ToString() + "&" +
				"epic_game_region=" + Region + "&" +
				"epic_game_cpu_budget=" + LocalCpuBudget +"&" +
				"epic_game_ram_budget=" + RamBudget + "&" +
				"epic_game_buildstr_base64=" + System.Convert.ToBase64String(BuildStringBytes) + "&" +
				"epic_game_environment=" + LocalAppName.ToString() + "&" +
				"epic_instance_size=" + InstanceSize.ToString() + "&" +
				"epic_instance_min=" + InstanceSizeMin.ToString() + "&" +
				"epic_instance_max=" + InstanceSizeMax.ToString() + "&" +
				"epic_autoscaler_id=" + AutoscalerId + "&" +
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
			string AppEnvironment = InAppName.ToString().Substring(GameName.Length);
			
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
			CloudArgs = CloudArgs + "&epic_async=" + D2AsyncType + "&epic_async_id=" + AsyncId.ToString();
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
