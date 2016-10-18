// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum PerforceChangeType
	{
		Code,
		Content,
	}

	class PerforceMonitor : IDisposable
	{
		class PerforceChangeSorter : IComparer<PerforceChangeSummary>
		{
			public int Compare(PerforceChangeSummary SummaryA, PerforceChangeSummary SummaryB)
			{
				return SummaryB.Number - SummaryA.Number;
			}
		}

		PerforceConnection Perforce;
		readonly string BranchClientPath;
		readonly string SelectedClientFileName;
		readonly string SelectedProjectIdentifier;
		Thread WorkerThread;
		int PendingMaxChangesValue;
		SortedSet<PerforceChangeSummary> Changes = new SortedSet<PerforceChangeSummary>(new PerforceChangeSorter());
		SortedDictionary<int, PerforceChangeType> ChangeTypes = new SortedDictionary<int,PerforceChangeType>();
		SortedSet<int> PromotedChangeNumbers = new SortedSet<int>();
		int ZippedBinariesConfigChangeNumber;
		string ZippedBinariesPath;
		SortedList<int, string> ChangeNumberToZippedBinaries = new SortedList<int,string>();
		AutoResetEvent RefreshEvent = new AutoResetEvent(false);
		BoundedLogWriter LogWriter;
		bool bDisposing;

		public event Action OnUpdate;
		public event Action OnUpdateMetadata;
		public event Action OnStreamChange;

		public PerforceMonitor(PerforceConnection InPerforce, string InBranchClientPath, string InSelectedClientFileName, string InSelectedProjectIdentifier, string InLogPath)
		{
			Perforce = InPerforce;
			BranchClientPath = InBranchClientPath;
			SelectedClientFileName = InSelectedClientFileName;
			SelectedProjectIdentifier = InSelectedProjectIdentifier;
			PendingMaxChangesValue = 100;
			LastCodeChangeByCurrentUser = -1;
			OtherStreamNames = new List<string>();

			LogWriter = new BoundedLogWriter(InLogPath);

			WorkerThread = new Thread(() => PollForUpdates());
			WorkerThread.Start();
		}

		public void Dispose()
		{
			bDisposing = true;
			if(WorkerThread != null)
			{
				RefreshEvent.Set();
				if(!WorkerThread.Join(100))
				{
					WorkerThread.Abort();
				}
				WorkerThread = null;
			}
			if(LogWriter != null)
			{
				LogWriter.Dispose();
				LogWriter = null;
			}
		}

		public string LastStatusMessage
		{
			get;
			private set;
		}

		public int CurrentMaxChanges
		{
			get;
			private set;
		}

		public int PendingMaxChanges
		{
			get { return PendingMaxChangesValue; }
			set { lock(this){ if(value != PendingMaxChangesValue){ PendingMaxChangesValue = value; RefreshEvent.Set(); } } }
		}

		public IReadOnlyList<string> OtherStreamNames
		{
			get;
			private set;
		}

		void PollForUpdates()
		{
			string StreamName;
			if(!Perforce.GetActiveStream(out StreamName, LogWriter))
			{
				StreamName = null;
			}

			// Try to update the zipped binaries list before anything else, because it causes a state change in the UI
			UpdateZippedBinaries();

			while(!bDisposing)
			{
				Stopwatch Timer = Stopwatch.StartNew();

				// Check we haven't switched streams
				string NewStreamName;
				if(Perforce.GetActiveStream(out NewStreamName, LogWriter) && NewStreamName != StreamName)
				{
					OnStreamChange();
				}

				// Update the stream list
				if(StreamName != null)
				{
					List<string> NewOtherStreamNames;
					if(!Perforce.FindStreams(PerforceUtils.GetClientOrDepotDirectoryName(StreamName) + "/*", out NewOtherStreamNames, LogWriter))
					{
						NewOtherStreamNames = new List<string>();
					}
					OtherStreamNames = NewOtherStreamNames;
				}

				// Check for any p4 changes
				if(!UpdateChanges())
				{
					LastStatusMessage = "Failed to update changes";
				}
				else if(!UpdateChangeTypes())
				{
					LastStatusMessage = "Failed to update change types";
				}
				else if(!UpdateZippedBinaries())
				{
					LastStatusMessage = "Failed to update zipped binaries list";
				}
				else
				{
					LastStatusMessage = String.Format("Last update took {0}ms", Timer.ElapsedMilliseconds);
				}

				// Wait for another request, or scan for new builds after a timeout
				RefreshEvent.WaitOne(30 * 1000);
			}
		}

		bool UpdateChanges()
		{
			// Get the current status of the build
			int MaxChanges;
			int OldestChangeNumber = -1;
			int NewestChangeNumber = -1;
			HashSet<int> CurrentChangelists;
			SortedSet<int> PrevPromotedChangelists;
			lock(this)
			{
				MaxChanges = PendingMaxChanges;
				if(Changes.Count > 0)
				{
					NewestChangeNumber = Changes.First().Number;
					OldestChangeNumber = Changes.Last().Number;
				}
				CurrentChangelists = new HashSet<int>(Changes.Select(x => x.Number));
				PrevPromotedChangelists = new SortedSet<int>(PromotedChangeNumbers);
			}

			// Build a full list of all the paths to sync
			List<string> DepotPaths = new List<string>();
			if(SelectedClientFileName.EndsWith(".uprojectdirs", StringComparison.InvariantCultureIgnoreCase))
			{
				DepotPaths.Add(String.Format("{0}/...", BranchClientPath));
			}
			else
			{
				DepotPaths.Add(String.Format("{0}/*", BranchClientPath));
				DepotPaths.Add(String.Format("{0}/Engine/...", BranchClientPath));
				DepotPaths.Add(String.Format("{0}/...", PerforceUtils.GetClientOrDepotDirectoryName(SelectedClientFileName)));
			}

			// Read any new changes
			List<PerforceChangeSummary> NewChanges;
			if(MaxChanges > CurrentMaxChanges)
			{
				if(!Perforce.FindChanges(DepotPaths, MaxChanges, out NewChanges, LogWriter))
				{
					return false;
				}
			}
			else
			{
				if(!Perforce.FindChanges(DepotPaths.Select(DepotPath => String.Format("{0}@>{1}", DepotPath, NewestChangeNumber)), -1, out NewChanges, LogWriter))
				{
					return false;
				}
			}

			// Remove anything we already have
			NewChanges.RemoveAll(x => CurrentChangelists.Contains(x.Number));

			// Update the change ranges
			if(NewChanges.Count > 0)
			{
				OldestChangeNumber = Math.Max(OldestChangeNumber, NewChanges.Last().Number);
				NewestChangeNumber = Math.Min(NewestChangeNumber, NewChanges.First().Number);
			}

			// Fixup any ROBOMERGE authors
			const string RoboMergePrefix = "#ROBOMERGE-AUTHOR:";
			foreach(PerforceChangeSummary Change in NewChanges)
			{
				if(Change.Description.StartsWith(RoboMergePrefix))
				{
					int StartIdx = RoboMergePrefix.Length;
					while(StartIdx < Change.Description.Length && Change.Description[StartIdx] == ' ')
					{
						StartIdx++;
					}

					int EndIdx = StartIdx;
					while(EndIdx < Change.Description.Length && !Char.IsWhiteSpace(Change.Description[EndIdx]))
					{
						EndIdx++;
					}

					if(EndIdx > StartIdx)
					{
						Change.User = Change.Description.Substring(StartIdx, EndIdx - StartIdx);
						Change.Description = "ROBOMERGE: " + Change.Description.Substring(EndIdx).TrimStart();
					}
				}
			}

			// Insert them into the builds list
			if(NewChanges.Count > 0 || MaxChanges < CurrentMaxChanges)
			{
				lock(this)
				{
					Changes.UnionWith(NewChanges);
					while(Changes.Count > MaxChanges)
					{
						Changes.Remove(Changes.Last());
					}
					CurrentMaxChanges = MaxChanges;
				}
				if(OnUpdate != null)
				{
					OnUpdate();
				}
			}
			return true;
		}

		public bool UpdateChangeTypes()
		{
			// Get the minimum change we need to query
			bool bRequiresUpdate;
			lock(this)
			{
				bRequiresUpdate = Changes.Any(x => !ChangeTypes.ContainsKey(x.Number));
			}

			// If there's something to check for, find all the content changes after this changelist
			if(bRequiresUpdate)
			{
				string[] CodeExtensions = { ".cs", ".h", ".cpp", ".usf" };

				// Find all the content changes in this range. Include a few extra changes in case there have been more changes submitted since the last query (it seems -m is much faster than specifying a changelist range)
				List<PerforceChangeSummary> CodeChanges;
				if(!Perforce.FindChanges(CodeExtensions.Select(Extension => String.Format("{0}/...{1}", BranchClientPath, Extension)), CurrentMaxChanges + 10, out CodeChanges, LogWriter))
				{
					return false;
				}

				// Update the change types
				HashSet<int> CodeChangeNumbers = new HashSet<int>(CodeChanges.Select(x => x.Number));
				lock(this)
				{
					foreach(PerforceChangeSummary Change in Changes)
					{
						if(!ChangeTypes.ContainsKey(Change.Number))
						{
							PerforceChangeType Type = CodeChangeNumbers.Contains(Change.Number)? PerforceChangeType.Code : PerforceChangeType.Content;
							ChangeTypes.Add(Change.Number, Type);
						}
					}
				}

				// Find the last submitted code change by the current user
				int NewLastCodeChangeByCurrentUser = -1;
				foreach(PerforceChangeSummary Change in Changes)
				{
					if(String.Compare(Change.User, Perforce.UserName, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						PerforceChangeType Type;
						if(ChangeTypes.TryGetValue(Change.Number, out Type) && Type == PerforceChangeType.Code)
						{
							NewLastCodeChangeByCurrentUser = Math.Max(NewLastCodeChangeByCurrentUser, Change.Number);
						}
					}
				}
				LastCodeChangeByCurrentUser = NewLastCodeChangeByCurrentUser;

				// Notify the main window that we've got an update
				if(OnUpdateMetadata != null)
				{
					OnUpdateMetadata();
				}
			}
			return true;
		}

		bool UpdateZippedBinaries()
		{
			// Get the path to the config file
			string ClientConfigFileName = PerforceUtils.GetClientOrDepotDirectoryName(SelectedClientFileName) + "/Build/UnrealGameSync.ini";

			// Find the most recent change to that file (if the file doesn't exist, we succeed and just get 0 changes).
			List<PerforceChangeSummary> ConfigFileChanges;
			if(!Perforce.FindChanges(ClientConfigFileName, 1, out ConfigFileChanges, LogWriter))
			{
				return false;
			}

			// Update the zipped binaries path if it's changed
			int NewZippedBinariesConfigChangeNumber = (ConfigFileChanges.Count > 0)? ConfigFileChanges[0].Number : 0;
			if(NewZippedBinariesConfigChangeNumber != ZippedBinariesConfigChangeNumber)
			{
				string NewZippedBinariesPath = null;
				if(NewZippedBinariesConfigChangeNumber != 0)
				{
					List<string> Lines;
					if(!Perforce.Print(ClientConfigFileName, out Lines, LogWriter))
					{
						return false;
					}

					ConfigFile NewConfigFile = new ConfigFile();
					NewConfigFile.Parse(Lines.ToArray());

					ConfigSection ProjectSection = NewConfigFile.FindSection(SelectedProjectIdentifier);
					if(ProjectSection != null)
					{
						NewZippedBinariesPath = ProjectSection.GetValue("ZippedBinariesPath", null);
					}
				}
				ZippedBinariesPath = NewZippedBinariesPath;
				ZippedBinariesConfigChangeNumber = NewZippedBinariesConfigChangeNumber;
			}

			SortedList<int, string> NewChangeNumberToZippedBinaries = new SortedList<int,string>();
			if(ZippedBinariesPath != null)
			{
				List<PerforceFileChangeSummary> Changes;
				if(!Perforce.FindFileChanges(ZippedBinariesPath, 100, out Changes, LogWriter))
				{
					return false;
				}

				foreach(PerforceFileChangeSummary Change in Changes)
				{
					if(Change.Action != "purge")
					{
						string[] Tokens = Change.Description.Split(' ');
						if(Tokens[0].StartsWith("[CL") && Tokens[1].EndsWith("]"))
						{
							int OriginalChangeNumber;
							if(int.TryParse(Tokens[1].Substring(0, Tokens[1].Length - 1), out OriginalChangeNumber) && !NewChangeNumberToZippedBinaries.ContainsKey(OriginalChangeNumber))
							{
								NewChangeNumberToZippedBinaries[OriginalChangeNumber] = String.Format("{0}#{1}", ZippedBinariesPath, Change.Revision);
							}
						}
					}
				}
			}

			if(!ChangeNumberToZippedBinaries.SequenceEqual(NewChangeNumberToZippedBinaries))
			{
				ChangeNumberToZippedBinaries = NewChangeNumberToZippedBinaries;
				if(OnUpdateMetadata != null && Changes.Count > 0)
				{
					OnUpdateMetadata();
				}
			}

			return true;
		}

		public List<PerforceChangeSummary> GetChanges()
		{
			lock(this)
			{
				return new List<PerforceChangeSummary>(Changes);
			}
		}

		public bool TryGetChangeType(int Number, out PerforceChangeType Type)
		{
			lock(this)
			{
				return ChangeTypes.TryGetValue(Number, out Type);
			}
		}

		public HashSet<int> GetPromotedChangeNumbers()
		{
			lock(this)
			{
				return new HashSet<int>(PromotedChangeNumbers);
			}
		}

		public string CurrentStream
		{
			get;
			private set;
		}

		public int LastCodeChangeByCurrentUser
		{
			get;
			private set;
		}

		public bool HasZippedBinaries
		{
			get { return ChangeNumberToZippedBinaries.Count > 0; }
		}

		public bool TryGetArchivePathForChangeNumber(int ChangeNumber, out string ArchivePath)
		{
			return ChangeNumberToZippedBinaries.TryGetValue(ChangeNumber, out ArchivePath);
		}

		public void Refresh()
		{
			RefreshEvent.Set();
		}
	}
}
