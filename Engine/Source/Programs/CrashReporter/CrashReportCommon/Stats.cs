using System;
using System.Collections.Generic;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// Thread-safe event counter for measuring approximate rate over a fixed period between now and a fixed time ago
	/// </summary>
	public class EventCounter
	{
		private class Bin
		{
			public int Count { get; set; }
		}

		/// <summary>
		/// Create an event counter
		/// </summary>
		/// <param name="InAvePeriod">The period over which events are counter. The time we keep them and calculate the average rate.</param>
		/// <param name="InResolution">The number of bins in which events are kept in the given period.</param>
		public EventCounter(TimeSpan InAvePeriod, int InResolution)
		{
			Period = InAvePeriod;
			Resolution = InResolution;
			Bins = new Queue<Bin>(Resolution);
			BinPeriod = TimeSpan.FromTicks(Period.Ticks/Resolution);
			ActiveBin = new Bin();
			Bins.Enqueue(ActiveBin);
			Count = 0;
			Timestamp = DateTime.UtcNow;
		}

		/// <summary>
		/// Adds events to the counter. Keeps track of internal bins and updates them keeping only those in our Period.
		/// </summary>
		/// <param name="InCount">The number of events to add</param>
		public void AddEvent(int InCount = 1)
		{
			lock (ThisLock)
			{
				TotalCount += InCount;
				DateTime EventTime = DateTime.UtcNow;

				while (EventTime - Timestamp > BinPeriod)
				{
					ActiveBin = new Bin();
					Bins.Enqueue(ActiveBin);
					Timestamp += BinPeriod;
				}

				Count += InCount;
				ActiveBin.Count += InCount;

				while (Bins.Count > Resolution)
				{
					Count -= Bins.Dequeue().Count;
				}
			}
		}

		/// <summary>
		/// Count of all events since the counter was created.
		/// </summary>
		public int TotalEvents { get { return TotalCount; } }

		/// <summary>
		/// Count of all events currently in the time period we're interested in (Period) preceding now.
		/// </summary>
		public int EventsInPeriod { get { return Count; } }

		/// <summary>
		/// Current rate of events average over the period.
		/// </summary>
		public double EventsPerSecond
		{
			get
			{
				lock (ThisLock)
				{
					return Count/(BinPeriod.TotalSeconds*Bins.Count);
				}
			}
		}

		private DateTime Timestamp;
		private readonly TimeSpan Period;
		private readonly int Resolution;
		private readonly TimeSpan BinPeriod;
		private readonly Queue<Bin> Bins;
		private Bin ActiveBin;
		private int TotalCount;
		private int Count;
		private readonly Object ThisLock = new Object();
	}
}
