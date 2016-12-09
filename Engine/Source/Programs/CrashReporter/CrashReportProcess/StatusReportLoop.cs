using System;
using System.Collections.Generic;
using System.Data.Linq.Mapping;
using System.Linq;
using System.Security.Policy;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	abstract class StatusReportLoop : IDisposable
	{
		public TimeSpan InitialPause { get; set; }

		public TimeSpan LoopPeriod { get; set; }

		public void Dispose()
		{
			CancelSource.Cancel();
			lock (TaskMonitor)
			{
				Monitor.PulseAll(TaskMonitor);
			}
			LoopTask.Wait();
			LoopTask.Dispose();
			CancelSource.Dispose();
		}

		public StatusReportLoop(TimeSpan InLoopPeriod, Func<StatusReportLoop, TimeSpan, string> LoopFunction)
		{
			InitialPause = TimeSpan.FromSeconds(30);
			LoopPeriod = InLoopPeriod;
			LoopTask = new Task(() =>
			{
				var Cancel = CancelSource.Token;

				// Small pause to allow the app to get up and running before we run the first report
				Thread.Sleep(InitialPause);
				DateTime PeriodStartTime = CreationTimeUtc;

				lock (TaskMonitor)
				{
					while (!Cancel.IsCancellationRequested)
					{
						DateTime PeriodEndTime = DateTime.UtcNow;
						string ReportMessage = LoopFunction.Invoke(this, PeriodEndTime - PeriodStartTime);
						PeriodStartTime = PeriodEndTime;

						if (!string.IsNullOrWhiteSpace(ReportMessage))
						{
							CrashReporterProcessServicer.WriteSlack(ReportMessage);
						}

						Monitor.Wait(TaskMonitor, LoopPeriod);
					}
				}
			});
		}

		protected void Start() { LoopTask.Start(); }

		private Task LoopTask;
		private readonly CancellationTokenSource CancelSource = new CancellationTokenSource();
		private readonly DateTime CreationTimeUtc = DateTime.UtcNow;
		private readonly object TaskMonitor = new object();
	}

	class RegularStatusReport : StatusReportLoop
	{
		public RegularStatusReport(TimeSpan InLoopPeriod, Func<StatusReportLoop, TimeSpan, string> LoopFunction)
			: base(InLoopPeriod, LoopFunction)
		{
			Start();
		}

		public Dictionary<string, int> GetCountsInPeriod(IDictionary<string, int> InCounters)
		{
			Dictionary<string, int> CountsInPeriod = new Dictionary<string, int>();

			foreach (var Counter in InCounters)
			{
				int LastCount = 0;
				if (!CountersAtLastReport.TryGetValue(Counter.Key, out LastCount))
				{
					CountersAtLastReport.Add(Counter.Key, 0);
				}
				CountsInPeriod.Add(Counter.Key, Counter.Value - LastCount);
				CountersAtLastReport[Counter.Key] = Counter.Value;
			}

			return CountsInPeriod;
		}

		public int QueueSizeSumAtLastReport { get; set; }

		private readonly Dictionary<string, int> CountersAtLastReport = new Dictionary<string, int>();
	}

	class DiskStatusReport : StatusReportLoop
	{
		public DiskStatusReport(TimeSpan InLoopPeriod, Func<StatusReportLoop, TimeSpan, string> LoopFunction)
			: base(InLoopPeriod, LoopFunction)
		{
			Start();
		}
	}

	class PerfStatusReport : StatusReportLoop
	{
		public PerfStatusReport(TimeSpan InLoopPeriod, Func<StatusReportLoop, TimeSpan, string> LoopFunction)
			: base(InLoopPeriod, LoopFunction)
		{
			Start();
		}

		public Dictionary<string, StatusReportingMeanCounter> GetMeanCountsInPeriod(IDictionary<string, StatusReportingMeanCounter> InCounters)
		{
			Dictionary<string, StatusReportingMeanCounter> CountsInPeriod = new Dictionary<string, StatusReportingMeanCounter>();

			foreach (var Counter in InCounters)
			{
				StatusReportingMeanCounter LastCount;
				if (!MeanCountersAtLastReport.TryGetValue(Counter.Key, out LastCount))
				{
					LastCount = new StatusReportingMeanCounter();
					MeanCountersAtLastReport.Add(Counter.Key, LastCount);
				}
				CountsInPeriod.Add(Counter.Key, new StatusReportingMeanCounter(LastCount, Counter.Value));

				LastCount.SampleCount = Counter.Value.SampleCount;
				LastCount.TotalMillisec = Counter.Value.TotalMillisec;
			}

			return CountsInPeriod;
		}

		public static string GetPerfTimeString(float Millisec)
		{
			if (Millisec >= 600000.0f) // <== over ten minutes
			{
				// Express as simple number of integer minutes
				int Minutes = (int)Millisec/60000;
				return Minutes + " min";
			}

			if (Millisec >= 20000.0f) // <== over twenty seconds
			{
				// Express as a number of integer seconds
				int Seconds = (int)Millisec / 1000;
				return Seconds + " s";
			}

			if (Millisec >= 1000.0f) // <== over one second
			{
				// Express as a number of seconds with one decimal place
				float Seconds = Millisec / 1000.0f;
				return string.Format("{0:F1} s", Seconds);
			}

			if (Millisec >= 20.0f) // <== over twenty milliseconds
			{
				// Express as a number of integer milliseconds
				return string.Format("{0:F0} ms", Millisec);
			}

			// Express as a number of milliseconds with one decimal place
			return string.Format("{0:F1} ms", Millisec);
		}

		private readonly Dictionary<string, StatusReportingMeanCounter> MeanCountersAtLastReport = new Dictionary<string, StatusReportingMeanCounter>();
	}
}
