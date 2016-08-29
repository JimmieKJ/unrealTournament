using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Serialization;
using System.Reflection;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// Config values for the Crash Report Process. The binary expects to find an xml representation in the binary folder at runtime.
	/// </summary>
	[XmlRoot]
	public class Config
	{
		/// <summary>
		/// CRP version for displaying in status reports/logs etc
		/// </summary>
		[XmlElement]
		public string VersionString { get; set; }

		/// <summary>
		/// Folder where the files that are linked from the website are stored.
		/// </summary>
		[XmlElement]
		public string ProcessedReports { get; set; }

		/// <summary>
		/// Time after which old, unprocessed reports in landing zones are deleted.
		/// </summary>
		[XmlElement]
		public int DeleteWaitingReportsDays { get; set; }

		/// <summary>
		/// The website domain to which we output crashes.
		/// </summary>
		[XmlElement]
		public string CrashReportWebSite { get; set; }

		/// <summary>
		/// Folder where the video file that are linked from the website are stored.
		/// </summary>
		[XmlElement]
		public string ProcessedVideos { get; set; }

		/// <summary>
		/// Folder where the Perforce depot is located.
		/// </summary>
		[XmlElement]
		public string DepotRoot { get; set; }

        /// <summary>
        /// Perforce username used by the CRP and MDD.
        /// </summary>
        [XmlElement]
        public string P4User { get; set; }

        /// <summary>
        /// Perforce workspace used by the CRP and MDD.
        /// </summary>
        [XmlElement]
        public string P4Client { get; set; }

        /// <summary>
        /// Folder where new crash reports are queued from internal, company users.
        /// </summary>
        [XmlElement]
		public string InternalLandingZone { get; set; }

		/// <summary>
		/// Folder where new crash reports are queued from external users.
		/// </summary>
		[XmlElement]
		public string ExternalLandingZone { get; set; }

		/// <summary>
		/// Folder where new crash reports are queued from the data router S3 bucket.
		/// </summary>
		[XmlElement]
		public string DataRouterLandingZone { get; set; }

		/// <summary>
		/// Folder where failed reports are moved.
		/// </summary>
		[XmlElement]
		public string InvalidReportsDirectory { get; set; }

		/// <summary>
		/// String passed to Minidump Diagnostics to modify its Perforce depot root.
		/// </summary>
		[XmlElement]
		public string DepotIndex { get; set; }

		/// <summary>
		/// String specifying the binaries that will be synched from source control to get the latest MinidumpDiagnostics.
		/// </summary>
		[XmlElement]
		public string SyncBinariesFromDepot { get; set; }

		/// <summary>
		/// String specifying the config files that will be synched from source control for MinidumpDiagnostics.
		/// </summary>
		[XmlElement]
		public string SyncConfigFromDepot { get; set; }

		/// <summary>
		/// String specifying the third-party files that will be synched from source control for MinidumpDiagnostics. e.g. OpenSSL
		/// </summary>
		[XmlElement]
		public string SyncThirdPartyFromDepot { get; set; }

		/// <summary>
		/// String specifying the local path of the folder containing the MinidumpDiagnostics exe
		/// </summary>
		[XmlElement]
		public string MDDBinariesFolderInDepot { get; set; }

		/// <summary>
		/// The number of threads created by each main processor thread to upload crashes to the website. (relieves a bottleneck when using a single processor thread)
		/// </summary>
		[XmlElement]
		public int AddReportsPerProcessor { get; set; }

		/// <summary>
		/// The number of main processor threads
		/// </summary>
		[XmlElement]
		public int ProcessorThreadCount { get; set; }

		/// <summary>
		/// Incoming webhook URL for Slack integration.
		/// </summary>
		[XmlElement]
		public string SlackWebhookUrl { get; set; }

		/// <summary>
		/// Slack channel to which messages are posted. Can be null or empty to use the webhook default.
		/// </summary>
		[XmlElement]
		public string SlackChannel { get; set; }

		/// <summary>
		/// Slack username from which messages are posted. Can be null or empty to use the webhook default.
		/// </summary>
		[XmlElement]
		public string SlackUsername { get; set; }

		/// <summary>
		/// Slack user icon name (e.g. ":someemoji:") from which messages are posted. Can be null or empty to use the webhook default.
		/// </summary>
		[XmlElement]
		public string SlackEmoji { get; set; }

		/// <summary>
		/// Local folder used to setup testing folders in Debug builds. Overrides other folder params in config.
		/// </summary>
		[XmlElement]
		public string DebugTestingFolder { get; set; }

		/// <summary>
		/// Period between reports to Slack (if available).
		/// </summary>
		[XmlElement]
		public int MinutesBetweenQueueSizeReports { get; set; }

		/// <summary>
		/// The limit for AddCrash() calls to the website failing in a row. An alert will be created if this is exceeded.
		/// </summary>
		[XmlElement]
		public int ConsecutiveFailedWebAddLimit { get; set; }

		/// <summary>
		/// Size limit below which queues will attempt to enqueue more crashes into memory
		/// Above this size, a queue will skip enqueueing until the next update.
		/// </summary>
		[XmlElement]
		public int MinDesiredMemoryQueueSize { get; set; }

		/// <summary>
		/// Size limit beyond which crashes won't be enqueued into memory on each queue.
		/// </summary>
		[XmlElement]
		public int MaxMemoryQueueSize { get; set; }

		/// <summary>
		/// AWSSDK service Url for S3 client
		/// </summary>
		[XmlElement]
		public string AWSS3ServiceURL { get; set; }

		/// <summary>
		/// AWSSDK service Url for SQS client
		/// </summary>
		[XmlElement]
		public string AWSSQSServiceURL { get; set; }

		/// <summary>
		/// AWSSDK queue Url for SQS client
		/// </summary>
		[XmlElement]
		public string AWSSQSQueueUrl { get; set; }

		/// <summary>
		/// AWSSDK Profile name used in the AWS credentials file
		/// </summary>
		[XmlElement]
		public string AWSProfileName { get; set; }

		/// <summary>
		/// AWSSDK AWS credentials filepath containing the keys used to access SQS and S3
		/// </summary>
		[XmlElement]
		public string AWSCredentialsFilepath { get; set; }

		/// <summary>
		/// Buffer size used to decompress zlib archives taken from S3
		/// </summary>
		[XmlElement]
		public int MaxUncompressedS3RecordSize { get; set; }

		/// <summary>
		/// Index file used to store all processed crash names and times. Stops duplicates.
		/// </summary>
		[XmlElement]
		public string ProcessedReportsIndexPath { get; set; }

		/// <summary>
		/// Time that the ProcessedReportsIndex retains items for duplicate checking.
		/// </summary>
		[XmlElement]
		public int ReportsIndexRetentionDays { get; set; }

		/// <summary>
		/// Timeout when calling AddCrash to submit crashes to the website/database.
		/// </summary>
		[XmlElement]
		public int AddCrashRequestTimeoutMillisec { get; set; }

		/// <summary>
		/// Time that we wait between a failed AddCrash call and a retry.
		/// </summary>
		[XmlElement]
		public int AddCrashRetryDelayMillisec { get; set; }

		/// <summary>
		/// Get the default config object (lazy loads it on first access)
		/// </summary>
		public static Config Default
		{
			get
			{
				if (DefaultSingleton == null)
				{
					LoadConfig();
				}
				return DefaultSingleton;
			}
		}

		/// <summary>
		/// Force reload default config object from disk
		/// </summary>
		public static void LoadConfig()
		{
			DefaultSingleton = LoadConfigPrivate();
		}

		private static Config LoadConfigPrivate()
		{
			Config LoadedConfig = new Config();

			string ExePath = Assembly.GetEntryAssembly().Location;
			string ExeFolder = Path.GetDirectoryName(ExePath);
			string ConfigPath = Path.Combine(ExeFolder, "CrashReportProcess.config");

			if (File.Exists(ConfigPath))
			{
				if (!string.IsNullOrEmpty(ConfigPath))
				{
					using (XmlReader Reader = XmlReader.Create(ConfigPath))
					{
						CrashReporterProcessServicer.WriteEvent("Loading config from " + ConfigPath);
						XmlSerializer Xml = new XmlSerializer(typeof (Config));
						LoadedConfig = Xml.Deserialize(Reader) as Config;
					}
				}
			}

#if DEBUG
			// Debug mode redirects to local folder in DebugTestingFolder
			LoadedConfig.ProcessedReports = Path.Combine(LoadedConfig.DebugTestingFolder, "ProcessedReports");
			LoadedConfig.ProcessedVideos = Path.Combine(LoadedConfig.DebugTestingFolder, "ProcessedVideos");
			LoadedConfig.DepotRoot = Path.Combine(LoadedConfig.DebugTestingFolder, "DepotRoot");
			LoadedConfig.InternalLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "InternalLandingZone");
			LoadedConfig.ExternalLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "ExternalLandingZone");
			LoadedConfig.DataRouterLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "DataRouterLandingZone");
			LoadedConfig.InvalidReportsDirectory = Path.Combine(LoadedConfig.DebugTestingFolder, "InvalidReportsDirectory");
			LoadedConfig.VersionString += " debugbuild";
			LoadedConfig.AWSCredentialsFilepath = Path.Combine(LoadedConfig.DebugTestingFolder, "AWS", "credentials.ini");
			LoadedConfig.ProcessedReportsIndexPath = Path.Combine(LoadedConfig.DebugTestingFolder, "ProcessedReports.ini");

#if SLACKTESTING
			LoadedConfig.SlackUsername = "CrashReportProcess_TESTING_IgnoreMe";
#else
			LoadedConfig.SlackWebhookUrl = string.Empty;	// no Slack in dbeug
#endif
#endif
			return LoadedConfig;
		}

		private static Config DefaultSingleton;
	}
}
