using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	class StatusReporting : IDisposable
	{
		public void InitQueue(string QueueName, string QueueLocation)
		{
			QueueLocations.Add(QueueName, QueueLocation);
			QueueSizes.Add(QueueName, 0);
		}

		public void SetQueueSize(string QueueName, int Size)
		{
			lock (DataLock)
			{
				QueueSizes[QueueName] = Size;
                SetQueueSizesCallCount++;
			}
		}

		public void IncrementCount(string CountName)
		{
			lock (DataLock)
			{
				if (Counters.ContainsKey(CountName))
				{
					Counters[CountName]++;
				}
				else
				{
					Counters.Add(CountName, 1);
				}
			}
		}

		public void Alert(string AlertText)
		{
			// Report an alert condition to Slack immediately
			CrashReporterProcessServicer.WriteSlack("@channel ALERT: " + AlertText);
		}

		public void Start()
		{
			CrashReporterProcessServicer.WriteSlack(string.Format("CRP started (version {0})", Config.Default.VersionString));

			StringBuilder StartupMessage = new StringBuilder();

			StartupMessage.AppendLine("Queues...");
			foreach (var Queue in QueueLocations)
			{
				StartupMessage.AppendLine("> " + Queue.Key + " @ " + Queue.Value);
			}
			CrashReporterProcessServicer.WriteSlack(StartupMessage.ToString());

			// Init events by enumerating event names
			FieldInfo[] EventNameFields = typeof(StatusReportingEventNames).GetFields(BindingFlags.Static | BindingFlags.Public);
			foreach (FieldInfo EventNameField in EventNameFields)
			{
				string CountName = (string)EventNameField.GetValue(null);
				Counters.Add(CountName, 0);
			}

			StatusReportingTask = Task.Factory.StartNew(() =>
			{
				var Cancel = CancelSource.Token;

				// Small pause to allow the app to get up and running before we run the first report
				Thread.Sleep(TimeSpan.FromSeconds(30));
				DateTime PeriodStartTime = CreationTimeUtc;

				lock (TaskMonitor)
				{
					while (!Cancel.IsCancellationRequested)
					{
						StringBuilder StatusReportMessage = new StringBuilder();
						lock (DataLock)
						{
							Dictionary<string, int> CountsInPeriod = GetCountsInPeriod(Counters, CountersAtLastReport);
							DateTime PeriodEndTime = DateTime.UtcNow;
							TimeSpan Period = PeriodEndTime - PeriodStartTime;
							PeriodStartTime = PeriodEndTime;

							if (SetQueueSizesCallCount >= QueueSizes.Count)
							{
								int ProcessingStartedInPeriodReceiver = 0;
								CountsInPeriod.TryGetValue(StatusReportingEventNames.ProcessingStartedReceiverEvent, out ProcessingStartedInPeriodReceiver);
								int ProcessingStartedInPeriodDataRouter = 0;
								CountsInPeriod.TryGetValue(StatusReportingEventNames.ProcessingStartedDataRouterEvent, out ProcessingStartedInPeriodDataRouter);
								int ProcessingStartedInPeriod = ProcessingStartedInPeriodReceiver + ProcessingStartedInPeriodDataRouter;

								if (ProcessingStartedInPeriod > 0)
								{
									int QueueSizeSum = QueueSizes.Values.Sum();
									TimeSpan MeanWaitTime =
										TimeSpan.FromTicks((long)(0.5*Period.Ticks*(QueueSizeSum + QueueSizeSumAtLastReport)/ProcessingStartedInPeriod));
									QueueSizeSumAtLastReport = QueueSizeSum;

									int WaitMinutes = Convert.ToInt32(MeanWaitTime.TotalMinutes);
									string WaitTimeString;
									if (MeanWaitTime == TimeSpan.Zero)
									{
										WaitTimeString = "nil";
									}
									else if (MeanWaitTime < TimeSpan.FromMinutes(1))
									{
										WaitTimeString = "< 1 minute";
									}
									else if (WaitMinutes == 1)
									{
										WaitTimeString = "1 minute";
									}
									else
									{
										WaitTimeString = string.Format("{0} minutes", WaitMinutes);
									}
									StatusReportMessage.AppendLine("Queue waiting time " + WaitTimeString);
								}

								StatusReportMessage.AppendLine("Queue sizes...");
								StatusReportMessage.Append("> ");
								foreach (var Queue in QueueSizes)
								{
									StatusReportMessage.Append(Queue.Key + " " + Queue.Value + "                ");
								}
								StatusReportMessage.AppendLine();
							}
							if (CountsInPeriod.Count > 0)
							{
								string PeriodTimeString;
								if (Period.TotalMinutes < 1.0)
								{
									PeriodTimeString = string.Format("{0:N0} seconds", Period.TotalSeconds);
								}
								else
								{
									PeriodTimeString = string.Format("{0:N0} minutes", Period.TotalMinutes);
								}

								StatusReportMessage.AppendLine(string.Format("Events in the last {0}...", PeriodTimeString));
								foreach (var CountInPeriod in CountsInPeriod)
								{
									StatusReportMessage.AppendLine("> " + CountInPeriod.Key + " " + CountInPeriod.Value);
								}
							}
						}
						CrashReporterProcessServicer.WriteSlack(StatusReportMessage.ToString());

						Monitor.Wait(TaskMonitor, TimeSpan.FromMinutes(Config.Default.MinutesBetweenQueueSizeReports));
					}
				}
			});
		}

		public void OnPreStopping()
		{
			CrashReporterProcessServicer.WriteSlack("CRP stopping...");
		}

		public void Dispose()
		{
			if (StatusReportingTask != null)
			{
				CancelSource.Cancel();
				lock (TaskMonitor)
				{
					Monitor.PulseAll(TaskMonitor);
				}
				StatusReportingTask.Wait();
				CancelSource.Dispose();
			}

			CrashReporterProcessServicer.WriteSlack("CRP stopped");
		}

		private static Dictionary<string, int> GetCountsInPeriod(Dictionary<string, int> InCounters, Dictionary<string, int> InCountersAtLastReport)
		{
			Dictionary<string, int> CountsInPeriod = new Dictionary<string, int>();

			foreach (var Counter in InCounters)
			{
				int LastCount = 0;
				if (!InCountersAtLastReport.TryGetValue(Counter.Key, out LastCount))
				{
					InCountersAtLastReport.Add(Counter.Key, 0);
				}
				CountsInPeriod.Add(Counter.Key, Counter.Value - LastCount);
				InCountersAtLastReport[Counter.Key] = Counter.Value;
			}

			return CountsInPeriod;
		}

		private Task StatusReportingTask;
		private readonly CancellationTokenSource CancelSource = new CancellationTokenSource();
		private readonly Dictionary<string, string> QueueLocations = new Dictionary<string, string>();
		private readonly Dictionary<string, int> QueueSizes = new Dictionary<string, int>();
		private readonly Dictionary<string, int> Counters = new Dictionary<string, int>();
		private int QueueSizeSumAtLastReport = 0;
		private readonly Dictionary<string, int> CountersAtLastReport = new Dictionary<string, int>();
		private readonly Object TaskMonitor = new Object();
		private readonly Object DataLock = new Object();
		private int SetQueueSizesCallCount = 0;
		private readonly DateTime CreationTimeUtc = DateTime.UtcNow;
	}
}
