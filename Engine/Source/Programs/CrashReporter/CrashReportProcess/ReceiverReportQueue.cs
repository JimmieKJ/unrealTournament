
namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// A queue of pending reports in a specific folder
	/// </summary>
	class ReceiverReportQueue : ReportQueueBase
	{
		protected override string QueueProcessingStartedEventName
		{ 
			get
			{
				return ProcessingStartedEventName;
			}
		}

		/// <summary>
		/// Constructor taking the landing zone
		/// </summary>
		public ReceiverReportQueue(string InQueueName, string LandingZonePath, string InProcessingStartedEventName, int InDecimateWaitingCountStart,
		                           int InDecimateWaitingCountEnd)
			: base(InQueueName, LandingZonePath, InDecimateWaitingCountStart, InDecimateWaitingCountEnd)
		{
			ProcessingStartedEventName = InProcessingStartedEventName;
		}

		protected override int GetTotalWaitingCount()
		{
			return LastQueueSizeOnDisk;
		}

		private readonly string ProcessingStartedEventName;
	}
}