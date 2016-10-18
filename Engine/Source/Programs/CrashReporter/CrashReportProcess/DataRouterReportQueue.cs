using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Text;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.S3.Model;
using Amazon.SQS;
using Amazon.SQS.Model;
using Tools.CrashReporter.CrashReportCommon;

namespace Tools.CrashReporter.CrashReportProcess
{
	class DataRouterReportQueue : ReportQueueBase
	{
		protected override string QueueProcessingStartedEventName
		{
			get
			{
				return StatusReportingEventNames.ProcessingStartedDataRouterEvent;
			}
		}

		/// <summary>
		/// Constructor taking the landing zone
		/// </summary>
		public DataRouterReportQueue(string InQueueName, string InLandingZoneTempPath)
			: base(InQueueName, InLandingZoneTempPath)
		{
			AWSCredentials Credentials = new StoredProfileAWSCredentials(Config.Default.AWSProfileName, Config.Default.AWSCredentialsFilepath);

			AmazonSQSConfig SqsConfig = new AmazonSQSConfig
			{
				ServiceURL = Config.Default.AWSSQSServiceURL
			};

			SqsClient = new AmazonSQSClient(Credentials, SqsConfig);

			AmazonS3Config S3Config = new AmazonS3Config
			{
				ServiceURL = Config.Default.AWSS3ServiceURL
			};

			S3Client = new AmazonS3Client(Credentials, S3Config);
		}

		public override void Dispose()
		{
			SqsClient.Dispose();
			S3Client.Dispose();

			base.Dispose();
		}

		protected override int GetTotalWaitingCount()
		{
			return LastQueueSizeOnDisk + GetSQSCount();
		}

		protected override bool GetCrashesFromLandingZone(out DirectoryInfo[] OutDirectories)
		{
			if (!base.GetCrashesFromLandingZone(out OutDirectories))
			{
				return false;
			}

			if (OutDirectories.Length < Config.Default.MaxMemoryQueueSize)
			{
				// Try to get new crashes from S3
				DirectoryInfo DirInfo = new DirectoryInfo(LandingZone);
				TryGetNewS3Crashes(Config.Default.MaxMemoryQueueSize - OutDirectories.Length);
				OutDirectories = DirInfo.GetDirectories().OrderBy(dirinfo => dirinfo.CreationTimeUtc).ToArray();
			}

			return true;
		}

		private void TryGetNewS3Crashes(int CrashCount)
		{
			int NewCrashCount = 0;
			while (NewCrashCount < CrashCount)
			{
				string SQSRecord = "<unset>";

				try
				{
					if (!DequeueRecordSQS(out SQSRecord))
					{
						// Queue empty
						break;
					}

					var RecordPair = SQSRecord.Split(',');
					if (RecordPair.Length != 2)
					{
						CrashReporterProcessServicer.WriteFailure("TryGetNewS3Crashes: bad SQS message was " + SQSRecord);
						CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3FileFailedEvent);
						continue;
					}

					string S3BucketName = RecordPair[0];
					string S3Key = RecordPair[1];
					string ReadableRequestString = "Bucket=" + S3BucketName + " Key=" + S3Key;

					var ObjectRequest = new GetObjectRequest
					{
						BucketName = S3BucketName,
						Key = S3Key
					};

					using (Stream ProtocolBufferStream = new MemoryStream())
					{
						using (GetObjectResponse ObjectResponse = S3Client.GetObject(ObjectRequest))
						{
							using (Stream ResponseStream = ObjectResponse.ResponseStream)
							{
								if (!TryDecompResponseStream(ResponseStream, ProtocolBufferStream))
								{
									CrashReporterProcessServicer.WriteFailure("! GZip fail in DecompResponseStream(): " + ReadableRequestString);
									CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3FileFailedEvent);
									continue;
								}
							}
						}

						NewCrashCount += UnpackRecordsFromDelimitedProtocolBuffers(ProtocolBufferStream, LandingZone, ReadableRequestString);
					}
				}
				catch (Exception ex)
				{
					CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3FileFailedEvent);
					CrashReporterProcessServicer.WriteException("TryGetNewS3Crashes: failure during processing SQS record " + SQSRecord +
					                                            "\n" + ex);
				}
			}
		}

		private static bool TryDecompResponseStream(Stream ResponseStream, Stream OutputStream)
		{
			try
			{
				using (GZipStream DecompStream = new GZipStream(ResponseStream, CompressionMode.Decompress))
				{
					DecompStream.CopyTo(OutputStream);
				}
				OutputStream.Position = 0;
				return true;
			}
			catch (Exception)
			{
				return false;
			}
		}

		private static int UnpackRecordsFromDelimitedProtocolBuffers(Stream ProtocolBufferStream, string InLandingZone, string ReadableRequestString)
		{
			var UnpackedRecordCount = 0;
			DataRouterConsumer Consumer = new DataRouterConsumer();

			// Expect one or more pairs of varint size + protocol buffer in the stream
			while (ProtocolBufferStream.Position < ProtocolBufferStream.Length)
			{
				DataRouterConsumer.ProtocolBufferRecord Message;

				if (!Consumer.TryParse(ProtocolBufferStream, out Message))
				{
					string FailString = "! Protocol buffer parse fail in UnpackRecordsFromDelimitedProtocolBuffers(): " + ReadableRequestString;
					FailString += '\n' + Consumer.LastError;

					CrashReporterProcessServicer.WriteFailure(FailString);
					CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3FileFailedEvent);
					break;
				}

				if (DecompressDataRouterContent(Message.Payload, InLandingZone))
				{
					UnpackedRecordCount++;
				}
			}

			return UnpackedRecordCount;
		}

		private static unsafe bool DecompressDataRouterContent(byte[] CompressedBufferArray, string InLandingZone)
		{
			// Decompress to landing zone
			byte[] UncompressedBufferArray = new byte[Config.Default.MaxUncompressedS3RecordSize];
			int UncompressedSize = 0;
			fixed (byte* UncompressedBufferPtr = UncompressedBufferArray, CompressedBufferPtr = CompressedBufferArray)
			{
				Int32 UncompressResult = NativeMethods.UE4UncompressMemoryZLIB((IntPtr) UncompressedBufferPtr,
					UncompressedBufferArray.Length,
					(IntPtr) CompressedBufferPtr, CompressedBufferArray.Length);
				if (UncompressResult < 0)
				{
					string FailString = "! DecompressDataRouterContent() failed in UE4UncompressMemoryZLIB() with " +
					                    NativeMethods.GetUncompressError(UncompressResult);
					CrashReporterProcessServicer.WriteFailure(FailString);
					CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3RecordFailedEvent);
					return false;
				}
				UncompressedSize = UncompressResult;
			}

			using (BinaryReader BinaryData = new BinaryReader(new MemoryStream(UncompressedBufferArray, 0, UncompressedSize, false)))
			{
				char[] MarkerChars = BinaryData.ReadChars(3);
				if (MarkerChars[0] == 'C' && MarkerChars[1] == 'R' && MarkerChars[2] == '1')
				{
					CrashHeader CrashHeader = DataRouterReportQueue.CrashHeader.ParseCrashHeader(BinaryData);

					// Create safe directory name and then create the directory on disk
					string CrashFolderName = GetSafeFilename(CrashHeader.DirectoryName);
					string CrashFolderPath = Path.Combine(InLandingZone, CrashFolderName);

					// Early check for duplicate processed report
					lock (ReportIndexLock)
					{
						if (ReportIndex.ContainsReport(CrashFolderName))
						{
							// Crash report not accepted by index
							CrashReporterProcessServicer.WriteEvent(string.Format(
								"DataRouterReportQueue: Duplicate report skipped early {0} in a DataRouterReportQueue", CrashFolderPath));
							CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.DuplicateRejected);
							return false; // this isn't an error so don't set error message
						}
					}

					// Create target folder
					int TryIndex = 1;
					while (Directory.Exists(CrashFolderPath))
					{
						CrashFolderPath = Path.Combine(InLandingZone, string.Format("{0}_DUPE{1:D3}", CrashFolderName, TryIndex++));
					}
					Directory.CreateDirectory(CrashFolderPath);

					if (UncompressedSize != CrashHeader.UncompressedSize)
					{
						CrashReporterProcessServicer.WriteEvent(
							string.Format(
								"DecompressDataRouterContent() warning UncompressedSize mismatch (embedded={0}, actual={1}) Path={2}",
								CrashHeader.UncompressedSize, UncompressedSize, CrashFolderPath));
					}

					if (!ParseCrashFiles(BinaryData, CrashHeader.FileCount, CrashFolderPath))
					{
						string FailString = "! DecompressDataRouterContent() failed to write files Path=" + CrashFolderPath;
						CrashReporterProcessServicer.WriteFailure(FailString);
						CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3RecordFailedEvent);
						return false;
					}
				}
				else
				{
#if ALLOWOLDCLIENTDATA
					// Early Data Router upload format was broken.
					// Should be [CR1][CrashHeader][File][File][File][File]...
					// Actually [Undefined CrashHeader][File][File][File][File]...[CrashHeader]

					// Seek to end minus header size
					BinaryData.BaseStream.Position = UncompressedSize - DataRouterReportQueue.CrashHeader.FixedSize;
					var CrashHeader = DataRouterReportQueue.CrashHeader.ParseCrashHeader(BinaryData);

					// Create safe directory name and then create the directory on disk
					string CrashFolderName = GetSafeFilename(CrashHeader.DirectoryName);
					string CrashFolderPath = Path.Combine(InLandingZone, CrashFolderName);

					// Early check for duplicate processed report
					lock (ReportIndexLock)
					{
						if (ReportIndex.ContainsReport(CrashFolderName))
						{
							// Crash report not accepted by index
							CrashReporterProcessServicer.WriteEvent(string.Format(
								"DataRouterReportQueue: Duplicate report skipped early {0} in a DataRouterReportQueue", CrashFolderPath));
							CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.DuplicateRejected);
							return false; // this isn't an error so don't set error message
						}
					}

					// Create target folder
					int TryIndex = 1;
					while (Directory.Exists(CrashFolderPath))
					{
						CrashFolderPath = Path.Combine(InLandingZone, string.Format("{0}_DUPE{1:D3}", CrashFolderName, TryIndex++));
					}
					Directory.CreateDirectory(CrashFolderPath);

					if (UncompressedSize != CrashHeader.UncompressedSize + CrashHeader.FixedSize)
					{
						CrashReporterProcessServicer.WriteEvent(
							string.Format(
								"DecompressDataRouterContent() warning UncompressedSize mismatch (embedded={0}, actual={1}) Path={2}",
								CrashHeader.UncompressedSize, UncompressedSize, CrashFolderPath));
					}

					// Seek to start of files (header size in from start)
					BinaryData.BaseStream.Position = CrashHeader.FixedSize;
					if (!ParseCrashFiles(BinaryData, CrashHeader.FileCount, CrashFolderPath))
					{
						string FailString = "! DecompressDataRouterContent() failed to write files Path=" + CrashFolderPath;
						CrashReporterProcessServicer.WriteFailure(FailString);
						CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ReadS3RecordFailedEvent);
						return false;
					}
#else
					ErrorMessage = "! DecompressDataRouterContent() failed to read invalid data format. Corrupt or old format data received Path=" + CrashFolderPath;
					return false;
#endif
				}
			}

			return true;
		}

		private static bool ParseCrashFiles(BinaryReader Reader, int FileCount, string CrashFolderPath)
		{
			for (int FileIndex = 0; FileIndex < FileCount; FileIndex++)
			{
				if (!WriteIncomingFile(Reader, FileIndex, CrashFolderPath))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Returns the count of items in the SQS
		/// </summary>
		private int GetSQSCount()
		{
			try
			{
				var AttribRequest = new GetQueueAttributesRequest
				{
					QueueUrl = Config.Default.AWSSQSQueueUrl,
					AttributeNames = new List<string>
					{
						"ApproximateNumberOfMessages"
					}
				};

				var AttribResponse = SqsClient.GetQueueAttributes(AttribRequest);
				return AttribResponse.ApproximateNumberOfMessages;
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("GetSQSCount: " + Ex.ToString());
			}
			return 0;
		}

		private bool DequeueRecordSQS(out string OutRecordString)
		{
			OutRecordString = string.Empty;

			try
			{
				var ReceiveRequest = new ReceiveMessageRequest
				{
					QueueUrl = Config.Default.AWSSQSQueueUrl,
					MaxNumberOfMessages = 1
				};

				var ReceiveResponse = SqsClient.ReceiveMessage(ReceiveRequest);

				if (ReceiveResponse.Messages.Count == 1)
				{
					var Message = ReceiveResponse.Messages[0];

					if (Message != null && TryDeleteRecordSQS(Message))
					{
						OutRecordString = Message.Body;
						return true;
					}
				}
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("DequeueRecordSQS: " + Ex.ToString());
			}
			return false;
		}

		private bool TryDeleteRecordSQS(Message InMessage)
		{
#if DEBUG
			// Debug doesn't empty the queue - it's just reads records
			return true;
#else
			try
			{
				var DeleteRequest = new DeleteMessageRequest
				{
					QueueUrl = Config.Default.AWSSQSQueueUrl,
					ReceiptHandle = InMessage.ReceiptHandle
				};

				var DeleteResponse = SqsClient.DeleteMessage(DeleteRequest);
				return DeleteResponse.HttpStatusCode == HttpStatusCode.OK;
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("TryDeleteRecordSQS: " + Ex.ToString());
			}
			return false;
#endif
        }

        private class CrashHeader
		{
			public const int FixedSize = 4 + 260 + 4 + 260 + 4 + 4;		// 2 x 260 ansi char strings with length + 2 x int32

			public string DirectoryName { get; set; }
			public string FileName { get; set; }
			public int UncompressedSize { get; set; }
			public int FileCount { get; set; }

			public static CrashHeader ParseCrashHeader(BinaryReader Reader)
			{
				var OutCrashHeader = new CrashHeader();
				OutCrashHeader.DirectoryName = FBinaryReaderHelper.ReadFixedSizeString(Reader);
				OutCrashHeader.FileName = FBinaryReaderHelper.ReadFixedSizeString(Reader);
				OutCrashHeader.UncompressedSize = Reader.ReadInt32();
				OutCrashHeader.FileCount = Reader.ReadInt32();
				return OutCrashHeader;
			}
		}

		private readonly AmazonSQSClient SqsClient;
		private readonly AmazonS3Client S3Client;
	}
}
