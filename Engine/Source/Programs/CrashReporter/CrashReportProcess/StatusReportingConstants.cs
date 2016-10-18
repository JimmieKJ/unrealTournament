using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	static class StatusReportingEventNames
	{
		public const string ReadS3FileFailedEvent = "Reading crash records file from S3 failed";
		public const string ReadS3RecordFailedEvent = "Reading crash record from S3 failed";
		public const string DuplicateRejected = "Duplicates ignored";
		public const string QueuedEvent = "Queued for processing";
		public const string ProcessingStartedReceiverEvent = "Started processing (from CRR)";
		public const string ProcessingStartedDataRouterEvent = "Started processing (from Data Router)";
		public const string ProcessingSucceededEvent = "Processing succeeded";
		public const string ProcessingFailedEvent = "Processing failed";
		public const string ExceptionEvent = "Handled exceptions";	
	}
}
