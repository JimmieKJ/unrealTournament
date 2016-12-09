using System.IO;
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
		/// Folder where new crash reports are queued from the PS4 crash service.
		/// </summary>
		[XmlElement]
		public string PS4LandingZone { get; set; }

		/// <summary>
		/// Number of reports in a queue at which reports will start to be discarded to stop a backlog from growing uncontrollably.
		/// </summary>
		[XmlElement]
		public int QueueLowerLimitForDiscard { get; set; }

		/// <summary>
		/// Number of reports in a queue at which reports will all be discarded so this is the upper limit for a backlog.
		/// </summary>
		[XmlElement]
		public int QueueUpperLimitForDiscard { get; set; }

		/// <summary>
		/// String passed to Minidump Diagnostics to modify its Perforce depot root.
		/// </summary>
		[XmlElement]
		public string DepotIndex { get; set; }

		/// <summary>
		/// String specifying the local path of the MinidumpDiagnostics exe
		/// </summary>
		[XmlElement]
		public string MDDExecutablePath { get; set; }

		/// <summary>
		/// String specifying the path to the folder used by MinidumpDiagnostics for the PDB cache
		/// </summary>
		[XmlElement]
		public string MDDPDBCachePath { get; set; }

		/// <summary>
		/// Number telling MinidumpDiagnostics how large it can make the PDB cache
		/// </summary>
		[XmlElement]
		public int MDDPDBCacheSizeGB { get; set; }

		/// <summary>
		/// Number telling MinidumpDiagnostics when it should start clearing cache entries to create more disk space
		/// </summary>
		[XmlElement]
		public int MDDPDBCacheMinFreeSpaceGB { get; set; }

		/// <summary>
		/// Number telling MinidumpDiagnostics the minimum age if a cache entry that should be considered for deletion/stale
		/// </summary>
		[XmlElement]
		public int MDDPDBCacheFileDeleteDays { get; set; }

		/// <summary>
		/// Timeout when waiting for MinidumpDiagnostics to complete
		/// </summary>
		[XmlElement]
		public int MDDTimeoutMinutes { get; set; }

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
		/// The number of parallel MDD instances allowed
		/// </summary>
		[XmlElement]
		public int MaxConcurrentMDDs { get; set; }

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
		/// The time period in which an alert with the same identifying key is not allowed to repeat.
		/// </summary>
		[XmlElement]
		public int SlackAlertRepeatMinimumMinutes { get; set; }

		/// <summary>
		/// The time period in which a crash decimation (large backlog) alert with the same identifying key is not allowed to repeat.
		/// </summary>
		[XmlElement]
		public int SlackDecimateAlertRepeatMinimumMinutes { get; set; }

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
		/// AWSSDK AWS credentials filepath containing the keys used to access SQS and S3
		/// </summary>
		[XmlElement]
		public string AWSCredentialsFilepath { get; set; }

		/// <summary>
		/// AWSSDK Profile name used in the AWS credentials file for reading crashes from DataRouter
		/// </summary>
		[XmlElement]
		public string AWSProfileInputName { get; set; }

		/// <summary>
		/// AWSSDK service Url for S3 client reading crashes from DataRouter
		/// </summary>
		[XmlElement]
		public string AWSS3ServiceInputURL { get; set; }

		/// <summary>
		/// AWSSDK service Url for SQS client reading crashes from DataRouter
		/// </summary>
		[XmlElement]
		public string AWSSQSServiceInputURL { get; set; }

		/// <summary>
		/// AWSSDK queue Url for SQS client reading crashes from DataRouter
		/// </summary>
		[XmlElement]
		public string AWSSQSQueueInputUrl { get; set; }

		/// <summary>
		/// AWSSDK Profile name used in the AWS credentials file for writing crashes to S3
		/// </summary>
		[XmlElement]
		public string AWSProfileOutputName { get; set; }

		/// <summary>
		/// AWSSDK service Url for S3 client for writing crashes to S3
		/// </summary>
		[XmlElement]
		public string AWSS3ServiceOutputURL { get; set; }

		/// <summary>
		/// Should we output a copy of the crash report files to disk? (ProcessedReports, ProcessedVideos)
		/// </summary>
		[XmlElement]
		public bool CrashFilesToDisk { get; set; }

		/// <summary>
		/// Should we output a copy of the crash report files to S3?
		/// </summary>
		[XmlElement]
		public bool CrashFilesToAWS { get; set; }

		/// <summary>
		/// Should we save invalid reports that fail to process to S3?
		/// </summary>
		[XmlElement]
		public bool InvalidReportsToAWS { get; set; }

		/// <summary>
		/// AWSSDK AWS S3 bucket used for output of crash reporter files (optional)
		/// </summary>
		[XmlElement]
		public string AWSS3OutputBucket { get; set; }

		/// <summary>
		/// AWSSDK AWS S3 path/key prefix used for output of crash reporter files (suffix will be crash id and file name) (optional)
		/// </summary>
		[XmlElement]
		public string AWSS3OutputKeyPrefix { get; set; }

		/// <summary>
		/// AWSSDK AWS S3 path/key prefix used for writing invalid reports (optional)
		/// </summary>
		[XmlElement]
		public string AWSS3InvalidKeyPrefix { get; set; }

		/// <summary>
		/// Buffer size used to decompress zlib archives taken from S3
		/// </summary>
		[XmlElement]
		public int MaxUncompressedS3RecordSize { get; set; }

		/// <summary>
		/// Index file used to store all processed crash names and times. Stops duplicates. Leave blank to disable.
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
		/// Disk space available threshold that generates alerts. If a disk has less space than this, it will generate alerts.
		/// </summary>
		[XmlElement]
		public float DiskSpaceAlertPercent { get; set; }

		/// <summary>
		/// Switch on perf monitoring via StatusReporting. Adds an extra report with perf info.
		/// </summary>
		[XmlElement]
		public bool MonitorPerformance { get; set; }

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
			if (!string.IsNullOrWhiteSpace(LoadedConfig.InternalLandingZone))
			{
				LoadedConfig.InternalLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "InternalLandingZone");
			}
			if (!string.IsNullOrWhiteSpace(LoadedConfig.ExternalLandingZone))
			{
				LoadedConfig.ExternalLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "ExternalLandingZone");
			}
			if (!string.IsNullOrWhiteSpace(LoadedConfig.DataRouterLandingZone))
			{
				LoadedConfig.DataRouterLandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "DataRouterLandingZone");
			}
			if (!string.IsNullOrWhiteSpace(LoadedConfig.PS4LandingZone))
			{
				LoadedConfig.PS4LandingZone = Path.Combine(LoadedConfig.DebugTestingFolder, "PS4LandingZone");
			}
			if (!string.IsNullOrWhiteSpace(LoadedConfig.MDDExecutablePath))
			{
				LoadedConfig.MDDExecutablePath = Path.Combine(LoadedConfig.DebugTestingFolder, "MinidumpDiagnostics", "Engine", "Binaries", "Win64", "MinidumpDiagnostics.exe");
			}
			LoadedConfig.VersionString += " debugbuild";
			LoadedConfig.AWSCredentialsFilepath = Path.Combine(LoadedConfig.DebugTestingFolder, "AWS", "credentials.ini");
			if (!string.IsNullOrWhiteSpace(LoadedConfig.ProcessedReportsIndexPath))
			{
				LoadedConfig.ProcessedReportsIndexPath = Path.Combine(LoadedConfig.DebugTestingFolder, "ProcessedReports.ini");
			}
			LoadedConfig.CrashReportWebSite = string.Empty;
			LoadedConfig.AWSS3OutputKeyPrefix = LoadedConfig.AWSS3OutputKeyPrefix.Replace("prod", "test");
			LoadedConfig.AWSS3InvalidKeyPrefix = LoadedConfig.AWSS3InvalidKeyPrefix.Replace("prod", "test");

			LoadedConfig.MinDesiredMemoryQueueSize = 5;
			LoadedConfig.MaxMemoryQueueSize = 15;

#if SLACKTESTING
			LoadedConfig.SlackUsername = "CrashReportProcess_TESTING_IgnoreMe";
			//LoadedConfig.SlackChannel = "OPTIONALTESTINGCHANNELHERE";
#else
			LoadedConfig.SlackWebhookUrl = string.Empty;	// no Slack in dbeug
#endif
#endif
			return LoadedConfig;
		}

		private static Config DefaultSingleton;
	}
}
