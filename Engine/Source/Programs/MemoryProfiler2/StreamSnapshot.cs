/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Linq;

namespace MemoryProfiler2
{
	/// <summary> Helper struct encapsulating information for a callstack and associated allocation size. Shared with stream parser. </summary>
	public struct FCallStackAllocationInfo
	{
		/// <summary> Size of allocation. </summary>
		public long Size;

		/// <summary> Index of callstack that performed allocation. </summary>
		public int CallStackIndex;

		/// <summary> Number of allocations. </summary>
		public int Count;

		/// <summary> Constructor, initializing all member variables to passed in values. </summary>
		public FCallStackAllocationInfo( long InSize, int InCallStackIndex, int InCount )
		{
			Size = InSize;
			CallStackIndex = InCallStackIndex;
			Count = InCount;
		}

		/// <summary>
		/// Adds the passed in size and count to this callstack info.
		/// </summary>
		/// <param name="SizeToAdd"> Size to add </param>
		/// <param name="CountToAdd"> Count to add </param>
		public FCallStackAllocationInfo Add( long SizeToAdd, int CountToAdd )
        {
			return new FCallStackAllocationInfo( Size + SizeToAdd, CallStackIndex, Count + CountToAdd );
        }

		/// <summary>
		/// Diffs the two passed in callstack infos and returns the difference.
		/// </summary>
		/// <param name="New"> Newer callstack info to subtract older from </param>
		/// <param name="Old"> Older callstack info to subtract from older </param>
        public static FCallStackAllocationInfo Diff( FCallStackAllocationInfo New, FCallStackAllocationInfo Old )
        {
            if( New.CallStackIndex != Old.CallStackIndex )
            {
                throw new InvalidDataException();
            }
            return new FCallStackAllocationInfo( New.Size - Old.Size, New.CallStackIndex, New.Count - Old.Count );
        }
	}

	/// <summary> Additional memory statistics. </summary>
    public enum ESnapshotMetricV3
    {
		/// <summary> Memory profiling overhead. </summary>
		MemoryProfilingOverhead,

		/// <summary> Memory allocated from OS. PS3 only. </summary>
		AllocatedFromOS,

		/// <summary> Maximum amount of memory allocated from OS. PS3 only. </summary>
		MaxAllocatedFromOS, 

		/// <summary> Memory requested by game. PS3 only. </summary>
		AllocateFromGame,

		/// <summary> Total used chunks by the allocator. </summary>
		TotalUsedChunks,

		/// <summary> Lightmap memory. </summary>
		TextureLightmapMemory,

		/// <summary> Shadowmap memory. </summary>
		TextureShadowmapMemory,

		/// <summary> Last item used as a count. </summary>
		Count,
    }

	/// <summary> State of total memory at a specific moment in time. </summary>
	public enum ESliceTypesV3
	{
		TotalUsed,
		TotalAllocated,
		CPUUsed,
		CPUSlack,
		CPUWaste,
		GPUUsed,
		GPUSlack,
		GPUWaste,
		OSOverhead,
		ImageSize,
		HostUsed,
		HostSlack,
		HostWaste,
		OverallAllocatedMemory,
		Count
	};

	public enum ESliceTypesV4
	{
		PlatformUsedPhysical,
		BinnedWasteCurrent,
		BinnedUsedCurrent,
		BinnedSlackCurrent,
		MemoryProfilingOverhead,
		OverallAllocatedMemory,
		Count
	};

	public class FMemorySlice
	{
		public long[] MemoryInfo = null;

		public FMemorySlice( FStreamSnapshot Snapshot )
		{
			if( FStreamToken.Version >= 4 )
			{
				MemoryInfo = new long[]
				{
					Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.PlatformUsedPhysical],
					Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedWasteCurrent],
					Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedUsedCurrent],
					Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedSlackCurrent],
					Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.MemoryProfilingOverhead],
					Snapshot.AllocationSize,
				};
			}
			else
			{
				MemoryInfo = new long[]
				{
					Snapshot.MemoryAllocationStats3.TotalUsed,
					Snapshot.MemoryAllocationStats3.TotalAllocated,
					Snapshot.MemoryAllocationStats3.CPUUsed,
					Snapshot.MemoryAllocationStats3.CPUSlack,
					Snapshot.MemoryAllocationStats3.CPUWaste,
					Snapshot.MemoryAllocationStats3.GPUUsed,
					Snapshot.MemoryAllocationStats3.GPUSlack,
					Snapshot.MemoryAllocationStats3.GPUWaste,
					Snapshot.MemoryAllocationStats3.OSOverhead,
					Snapshot.MemoryAllocationStats3.ImageSize,
					Snapshot.MemoryAllocationStats3.HostUsed,
					Snapshot.MemoryAllocationStats3.HostSlack,
					Snapshot.MemoryAllocationStats3.HostWaste,
					Snapshot.AllocationSize,
				};
			}
		}

		public long GetSliceInfoV3( ESliceTypesV3 SliceType )
		{
			return ( MemoryInfo[(int)SliceType] );
		}

		public long GetSliceInfoV4( ESliceTypesV4 SliceType )
		{
			return ( MemoryInfo[(int)SliceType] );
		}
	}

	/// <summary> Snapshot of allocation state at a specific time in token stream. </summary>
	public class FStreamSnapshot
	{
		/// <summary> User defined description of time of snapshot. </summary>
		public string Description;

		/// <summary> List of callstack allocations. </summary>
		public List<FCallStackAllocationInfo> ActiveCallStackList;

		/// <summary> List of lifetime callstack allocations for memory churn. </summary>
        public List<FCallStackAllocationInfo> LifetimeCallStackList;

		/// <summary> Position in the stream. </summary>
        public ulong StreamIndex;

		/// <summary> Frame number. </summary>
        public int FrameNumber;

		/// <summary> Current time. </summary>
		public float CurrentTime;

		/// <summary> Current time starting from the previous snapshot marker. </summary>
		public float ElapsedTime;
        
		/// <summary> Snapshot type. </summary>
		public EProfilingPayloadSubType SubType = EProfilingPayloadSubType.SUBTYPE_SnapshotMarker;

		/// <summary> Index of snapshot. </summary>
        public int SnapshotIndex;

		/// <summary> Platform dependent array of memory metrics. </summary>
        public List<long> MetricArray;

		/// <summary> A list of indices into the name table, one for each loaded level including persistent level. </summary>
		public List<int> LoadedLevels = new List<int>();

		/// <summary> Array of names of all currently loaded levels formated for usage in details view tab. </summary>
		public List<string> LoadedLevelNames = new List<string>();

		/// <summary> Generic memory allocation stats. </summary>
		public FMemoryAllocationStatsV3 MemoryAllocationStats3 = new FMemoryAllocationStatsV3();

		/// <summary> Generic memory allocation stats. </summary>
		public FMemoryAllocationStatsV4 MemoryAllocationStats4 = new FMemoryAllocationStatsV4();

		/// <summary> Running count of number of allocations. </summary>
		public long AllocationCount = 0;

		/// <summary> Running total of size of allocations. </summary>
		public long AllocationSize = 0;

		/// <summary> Running total of size of allocations. </summary>
		public long AllocationMaxSize = 0;

		/// <summary> True if this snapshot was created as a diff of two snapshots. </summary>
        public bool bIsDiffResult;

		/// <summary> Running total of allocated memory. </summary>
		public List<FMemorySlice> OverallMemorySlice;

		/// <summary> Constructor, naming the snapshot and initializing map. </summary>
		public FStreamSnapshot( string InDescription )
		{
			Description = InDescription;
			ActiveCallStackList = new List<FCallStackAllocationInfo>();
            // Presize lifetime callstack array and populate.
            LifetimeCallStackList = new List<FCallStackAllocationInfo>( FStreamInfo.GlobalInstance.CallStackArray.Count );
            for( int CallStackIndex=0; CallStackIndex<FStreamInfo.GlobalInstance.CallStackArray.Count; CallStackIndex++ )
            {
                LifetimeCallStackList.Add( new FCallStackAllocationInfo( 0, CallStackIndex, 0 ) );
            }
			OverallMemorySlice = new List<FMemorySlice>();
		}

		/// <summary> Performs a deep copy of the relevant data structures. </summary>
		public FStreamSnapshot DeepCopy( Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap )
		{
			// Create new snapshot object.
			FStreamSnapshot Snapshot = new FStreamSnapshot( "Copy" );

			// Manually perform a deep copy of LifetimeCallstackList
			foreach( FCallStackAllocationInfo AllocationInfo in LifetimeCallStackList )
			{
				Snapshot.LifetimeCallStackList[ AllocationInfo.CallStackIndex ]
					= new FCallStackAllocationInfo( AllocationInfo.Size, AllocationInfo.CallStackIndex, AllocationInfo.Count );
			}

			Snapshot.AllocationCount = AllocationCount;
			Snapshot.AllocationSize = AllocationSize;
			Snapshot.AllocationMaxSize = AllocationMaxSize;

			Snapshot.FinalizeSnapshot( PointerToPointerInfoMap );

			// Return deep copy of this snapshot.
			return Snapshot;
		}

		public void FillActiveCallStackList( Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap )
		{
			ActiveCallStackList.Clear();
			ActiveCallStackList.Capacity = LifetimeCallStackList.Count;

			foreach( KeyValuePair<ulong, FCallStackAllocationInfo> PointerData in PointerToPointerInfoMap )
			{
				int CallStackIndex = PointerData.Value.CallStackIndex;
				long Size = PointerData.Value.Size;

				// make sure allocationInfoList is big enough
				while( CallStackIndex >= ActiveCallStackList.Count )
				{
					ActiveCallStackList.Add( new FCallStackAllocationInfo( 0, ActiveCallStackList.Count, 0 ) );
				}

				FCallStackAllocationInfo NewAllocInfo = ActiveCallStackList[ CallStackIndex ];
				NewAllocInfo.Size += Size;
				NewAllocInfo.Count++;
				ActiveCallStackList[ CallStackIndex ] = NewAllocInfo;
			}

			// strip out any callstacks with no allocations
			for( int i = ActiveCallStackList.Count - 1; i >= 0; i-- )
			{
				if( ActiveCallStackList[ i ].Count == 0 )
				{
					ActiveCallStackList.RemoveAt( i );
				}
			}

			ActiveCallStackList.TrimExcess();
		}

		/// <summary> Convert "callstack to allocation" mapping (either passed in or generated from pointer map) to callstack info list. </summary>
		public void FinalizeSnapshot( Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap )
		{
			FillActiveCallStackList( PointerToPointerInfoMap );
		}

		/// <summary> Diffs two snapshots and creates a result one. </summary>
		public static FStreamSnapshot DiffSnapshots( FStreamSnapshot Old, FStreamSnapshot New )
		{
			// Create result snapshot object.
			FStreamSnapshot ResultSnapshot = new FStreamSnapshot( "Diff " + Old.Description + " <-> " + New.Description );

			using( FScopedLogTimer LoadingTime = new FScopedLogTimer( "FStreamSnapshot.DiffSnapshots" ) )
			{
				// Copy over allocation count so we can track where the graph starts
				ResultSnapshot.AllocationCount = Old.AllocationCount;

				Debug.Assert( Old.MetricArray.Count == New.MetricArray.Count );
				ResultSnapshot.MetricArray = new List<long>( Old.MetricArray.Count );
				for( int CallstackIndex = 0; CallstackIndex < Old.MetricArray.Count; CallstackIndex++ )
				{
					ResultSnapshot.MetricArray.Add( New.MetricArray[ CallstackIndex ] - Old.MetricArray[ CallstackIndex ] );
				}

				
				ResultSnapshot.MemoryAllocationStats3 = FMemoryAllocationStatsV3.Diff( Old.MemoryAllocationStats3, New.MemoryAllocationStats3 );
				ResultSnapshot.MemoryAllocationStats4 = FMemoryAllocationStatsV4.Diff( Old.MemoryAllocationStats4, New.MemoryAllocationStats4 );

				ResultSnapshot.StreamIndex = New.StreamIndex;
				ResultSnapshot.bIsDiffResult = true;

				ResultSnapshot.AllocationMaxSize = New.AllocationMaxSize - Old.AllocationMaxSize;
				ResultSnapshot.AllocationSize = New.AllocationSize - Old.AllocationSize;
				ResultSnapshot.CurrentTime = 0;
				ResultSnapshot.ElapsedTime = New.CurrentTime - Old.CurrentTime;
				ResultSnapshot.FrameNumber = New.FrameNumber - Old.FrameNumber;
				ResultSnapshot.LoadedLevels = New.LoadedLevels;

				// These lists are guaranteed to be sorted by callstack index.
				List<FCallStackAllocationInfo> OldActiveCallStackList = Old.ActiveCallStackList;
				List<FCallStackAllocationInfo> NewActiveCallStackList = New.ActiveCallStackList;
				List<FCallStackAllocationInfo> ResultActiveCallStackList = new List<FCallStackAllocationInfo>( FStreamInfo.GlobalInstance.CallStackArray.Count );

				int OldIndex = 0;
				int NewIndex = 0;
				while( true )
				{
					FCallStackAllocationInfo OldAllocInfo = OldActiveCallStackList[ OldIndex ];
					FCallStackAllocationInfo NewAllocInfo = NewActiveCallStackList[ NewIndex ];

					if( OldAllocInfo.CallStackIndex == NewAllocInfo.CallStackIndex )
					{
						long ResultSize = NewAllocInfo.Size - OldAllocInfo.Size;
						int ResultCount = NewAllocInfo.Count - OldAllocInfo.Count;

						if( ResultSize != 0 || ResultCount != 0 )
						{
							ResultActiveCallStackList.Add( new FCallStackAllocationInfo( ResultSize, NewAllocInfo.CallStackIndex, ResultCount ) );
						}

						OldIndex++;
						NewIndex++;
					}
					else if( OldAllocInfo.CallStackIndex > NewAllocInfo.CallStackIndex )
					{
						ResultActiveCallStackList.Add( NewAllocInfo );
						NewIndex++;
					}
					else // OldAllocInfo.CallStackIndex < NewAllocInfo.CallStackIndex
					{
						ResultActiveCallStackList.Add( new FCallStackAllocationInfo( -OldAllocInfo.Size, OldAllocInfo.CallStackIndex, -OldAllocInfo.Count ) );
						OldIndex++;
					}

					if( OldIndex >= OldActiveCallStackList.Count )
					{
						for( ; NewIndex < NewActiveCallStackList.Count; NewIndex++ )
						{
							ResultActiveCallStackList.Add( NewActiveCallStackList[ NewIndex ] );
						}
						break;
					}

					if( NewIndex >= NewActiveCallStackList.Count )
					{
						for( ; OldIndex < OldActiveCallStackList.Count; OldIndex++ )
						{
							ResultActiveCallStackList.Add( OldActiveCallStackList[ OldIndex ] );
						}
						break;
					}
				}

				// Check that list was correctly constructed.
				for( int CallstackIndex = 0; CallstackIndex < ResultActiveCallStackList.Count - 1; CallstackIndex++ )
				{
					Debug.Assert( ResultActiveCallStackList[ CallstackIndex ].CallStackIndex < ResultActiveCallStackList[ CallstackIndex + 1 ].CallStackIndex );
				}

				ResultActiveCallStackList.TrimExcess();
				ResultSnapshot.ActiveCallStackList = ResultActiveCallStackList;

				// Iterate over new lifetime callstack info and subtract previous one.
				for( int CallStackIndex = 0; CallStackIndex < New.LifetimeCallStackList.Count; CallStackIndex++ )
				{
					ResultSnapshot.LifetimeCallStackList[ CallStackIndex ] = FCallStackAllocationInfo.Diff(
																					New.LifetimeCallStackList[ CallStackIndex ],
																					Old.LifetimeCallStackList[ CallStackIndex ] );
				}

				// Handle overall memory timeline
				if( New.OverallMemorySlice.Count > Old.OverallMemorySlice.Count )
				{
					ResultSnapshot.OverallMemorySlice = new List<FMemorySlice>( New.OverallMemorySlice );
					ResultSnapshot.OverallMemorySlice.RemoveRange( 0, Old.OverallMemorySlice.Count );
				}
				else
				{
					ResultSnapshot.OverallMemorySlice = new List<FMemorySlice>( Old.OverallMemorySlice );
					ResultSnapshot.OverallMemorySlice.RemoveRange( 0, New.OverallMemorySlice.Count );
					ResultSnapshot.OverallMemorySlice.Reverse();
				}

			}

			return ResultSnapshot;
		}

		/// <summary> Exports this snapshot to a CSV file of the passed in name. </summary>
		/// <param name="FileName"> File name to export to </param>
		/// <param name="bShouldExportActiveAllocations"> Whether to export active allocations or lifetime allocations </param>
		public void ExportToCSV( string FileName, bool bShouldExportActiveAllocations )
		{
			// Create stream writer used to output call graphs to CSV.
			StreamWriter CSVWriter = new StreamWriter(FileName);

            // Figure out which list to export.
            List<FCallStackAllocationInfo> CallStackList = null;
            if( bShouldExportActiveAllocations )
            {
                CallStackList = ActiveCallStackList;
            }
            else
            {
                CallStackList = LifetimeCallStackList;
            }

			// Iterate over each unique call graph and spit it out. The sorting is per call stack and not
			// allocation size. Excel can be used to sort by allocation if needed. This sorting is more robust
			// and also what the call graph parsing code needs.
			foreach( FCallStackAllocationInfo AllocationInfo in CallStackList )
			{
                // Skip callstacks with no contribution in this snapshot.
                if( AllocationInfo.Count > 0 )
                {
				    // Dump size first, followed by count.
				    CSVWriter.Write(AllocationInfo.Size + "," + AllocationInfo.Count + ",");

				    // Iterate over ach address in callstack and dump to CSV.
				    FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex];
				    foreach( int AddressIndex in CallStack.AddressIndices )
				    {
					    FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex];
					    string SymbolFunctionName = FStreamInfo.GlobalInstance.NameArray[Address.FunctionIndex];
					    string SymbolFileName = FStreamInfo.GlobalInstance.NameArray[Address.FilenameIndex];
					    // Write out function followed by filename and then line number if valid
					    if( SymbolFunctionName != "" || SymbolFileName != "" )
					    {
							if( FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3 )
							{
								CSVWriter.Write( SymbolFunctionName + "," );
							}
							else
							{
								CSVWriter.Write( SymbolFunctionName + " @ " + SymbolFileName + ":" + Address.LineNumber + "," );
							}
					    }
					    else
					    {
						    CSVWriter.Write("Unknown,");
					    }
				    }
				    CSVWriter.WriteLine("");
                }
			}

			// Close the file handle now that we're done writing.
			CSVWriter.Close();
		}

		/// <summary> Exports this snapshots internal properties into a human readable format. </summary>
		public override string ToString()
		{
			StringBuilder StrBuilder = new StringBuilder( 1024 );

			///// <summary> User defined description of time of snapshot. </summary>
			//public string Description;
			StrBuilder.AppendLine( "Description: " + Description );

			///// <summary> List of callstack allocations. </summary>
			//public List<FCallStackAllocationInfo> ActiveCallStackList;

			///// <summary> List of lifetime callstack allocations for memory churn. </summary>
			//public List<FCallStackAllocationInfo> LifetimeCallStackList;

			///// <summary> Position in the stream. </summary>
			//public ulong StreamIndex;

			StrBuilder.AppendLine( "Stream Index: " + StreamIndex );

			///// <summary> Frame number. </summary>
			//public int FrameNumber;
			StrBuilder.AppendLine( "Frame Number: " + FrameNumber );

			///// <summary> Current time. </summary>
			//public float CurrentTime;
			StrBuilder.AppendLine( "Current Time: " + CurrentTime + " seconds" );

			///// <summary> Current time starting from the previous snapshot marker. </summary>
			//public float ElapsedTime;
			StrBuilder.AppendLine( "Elapsed Time: " + ElapsedTime + " seconds" );

			///// <summary> Snapshot type. </summary>
			//public EProfilingPayloadSubType SubType = EProfilingPayloadSubType.SUBTYPE_SnapshotMarker;
			StrBuilder.AppendLine( "Snapshot Type: " + SubType.ToString() );

			///// <summary> Index of snapshot. </summary>
			//public int SnapshotIndex;
			StrBuilder.AppendLine( "Snapshot Index: " + SnapshotIndex );

			///// <summary> Platform dependent memory metrics. </summary>
			//public List<long> Metrics;
			StrBuilder.AppendLine( "Metrics: " );
			if( MetricArray.Count > 0 )
			{
				if( FStreamToken.Version >= 4 )
				{
					// Just ignore
				}
				else if( FStreamToken.Version > 2 && MetricArray.Count == (int)ESnapshotMetricV3.Count )
				{
					string[] MetricNames = Enum.GetNames( typeof( ESnapshotMetricV3 ) );
					for( int MetricIndex = 0; MetricIndex < MetricNames.Length - 1; MetricIndex++ )
					{
						string MetricName = MetricNames[ MetricIndex ];
						long MetricData = MetricArray[ MetricIndex ];
						if( MetricData != 0 )
						{
							string MetricValue = ( MetricName == "TotalUsedChunks" ) ? MetricData.ToString() : MainWindow.FormatSizeString2( MetricData );
							string Metric = "	" + MetricName + ": " + MetricValue;
							StrBuilder.AppendLine( Metric );
						}
					}	
				}
			}

			///// <summary> Array of names of all currently loaded levels formated for usage in details view tab. </summary>
			//public List<string> LoadedLevelNames = new List<string>();
			StrBuilder.AppendLine( "Loaded Levels: " + LoadedLevels.Count );
			foreach( string LevelName  in LoadedLevelNames )
			{
				StrBuilder.AppendLine( "	" + LevelName );
			}

			///// <summary> Generic memory allocation stats. </summary>
			//public FMemoryAllocationStats MemoryAllocationStats = new FMemoryAllocationStats();

			if( FStreamToken.Version >= 4 )
			{
				StrBuilder.AppendLine( "Memory Allocation Stats: " );
				StrBuilder.Append( MemoryAllocationStats4.ToString() );
			}
			else
			{
				StrBuilder.AppendLine( "Memory Allocation Stats: " );
				StrBuilder.Append( MemoryAllocationStats3.ToString() );
			}
			
			///// <summary> Running count of number of allocations. </summary>
			//public long AllocationCount = 0;
			StrBuilder.AppendLine( "Allocation Count: " + AllocationCount );

			///// <summary> Running total of size of allocations. </summary>
			//public long AllocationSize = 0;
			StrBuilder.AppendLine( "Allocation Size: " + MainWindow.FormatSizeString2( AllocationSize ) );

			///// <summary> Running total of size of allocations. </summary>
			//public long AllocationMaxSize = 0;
			StrBuilder.AppendLine( "Allocation Max Size: " + MainWindow.FormatSizeString2( AllocationMaxSize ) );

			///// <summary> True if this snapshot was created as a diff of two snapshots. </summary>
			//public bool bIsDiffResult;
			StrBuilder.AppendLine( "Is Diff Result: " + bIsDiffResult );

			///// <summary> Running total of allocated memory. </summary>
			//public List<FMemorySlice> OverallMemorySlice;
			//StrBuilder.AppendLine( "Overall Memory Slice: @TODO"  );

			return StrBuilder.ToString();
		}

		/// <summary> Encapsulates indices to levels in the diff snapshots in relation to start and end snapshot. </summary>
		struct FLevelIndex
		{
			public FLevelIndex( int InLeftIndex, int InRightIndex )
			{
				LeftIndex = InLeftIndex;
				RightIndex = InRightIndex;
			}

			public override string ToString()
			{
				return LeftIndex + " <-> " + RightIndex;
			}

			public int LeftIndex;
			public int RightIndex;
		}

		/// <summary> 
		/// Prepares three array with level names. Arrays will be placed into LoadedLevelNames properties of each of snapshots.
		/// "-" in level name means that level was unloaded.
		/// "+" in level name means that level was loaded.
		/// </summary>
		public static void PrepareLevelNamesForDetailsTab( FStreamSnapshot LeftSnapshot, FStreamSnapshot DiffSnapshot, FStreamSnapshot RightSnapshot )
		{
			if( DiffSnapshot != null && LeftSnapshot != null && RightSnapshot != null )
			{
				LeftSnapshot.LoadedLevelNames.Clear();
				DiffSnapshot.LoadedLevelNames.Clear();
				RightSnapshot.LoadedLevelNames.Clear();

				List<FLevelIndex> LevelIndexArray = new List<FLevelIndex>( LeftSnapshot.LoadedLevels.Count + RightSnapshot.LoadedLevels.Count );

				List<int> AllLevelIndexArray = new List<int>( LeftSnapshot.LoadedLevels );
				AllLevelIndexArray.AddRange( RightSnapshot.LoadedLevels );

				IEnumerable<int> AllLevelIndicesDistinct = AllLevelIndexArray.Distinct();

				foreach( int LevelIndex in AllLevelIndicesDistinct )
				{
					int StartLevelIndex = LeftSnapshot.LoadedLevels.IndexOf( LevelIndex );
					int EndLevelIndex = RightSnapshot.LoadedLevels.IndexOf( LevelIndex );

					LevelIndexArray.Add( new FLevelIndex( StartLevelIndex, EndLevelIndex ) );
				}

				foreach( FLevelIndex LevelIndex in LevelIndexArray )
				{
					string LeftLevelName = "";
					string DiffLevelName = "";
					string RightLevelName = "";

					if( LevelIndex.LeftIndex != -1 )
					{
						LeftLevelName = FStreamInfo.GlobalInstance.NameArray[ LeftSnapshot.LoadedLevels[ LevelIndex.LeftIndex ] ];
					}

					if( LevelIndex.RightIndex != -1 )
					{
						RightLevelName = FStreamInfo.GlobalInstance.NameArray[ RightSnapshot.LoadedLevels[ LevelIndex.RightIndex ] ];
					}

					if( LevelIndex.LeftIndex != -1 && LevelIndex.RightIndex == -1 )
					{
						DiffLevelName = "-" + LeftLevelName;
					}
					else if( LevelIndex.LeftIndex == -1 && LevelIndex.RightIndex != -1 )
					{
						DiffLevelName = "+" + RightLevelName;
					}
					else if( LevelIndex.LeftIndex != -1 && LevelIndex.RightIndex != -1 )
					{
						DiffLevelName = " " + RightLevelName;
					}

					LeftSnapshot.LoadedLevelNames.Add( LeftLevelName );
					DiffSnapshot.LoadedLevelNames.Add( DiffLevelName );
					RightSnapshot.LoadedLevelNames.Add( RightLevelName );
				}
			}
			else if( LeftSnapshot != null )
			{
				LeftSnapshot.LoadedLevelNames.Clear();
				foreach( int LevelIndex in LeftSnapshot.LoadedLevels )
				{
					LeftSnapshot.LoadedLevelNames.Add( FStreamInfo.GlobalInstance.NameArray[ LevelIndex ] );
				}

			}
			else if( RightSnapshot != null )
			{
				RightSnapshot.LoadedLevelNames.Clear();
				foreach( int LevelIndex in RightSnapshot.LoadedLevels )
				{
					RightSnapshot.LoadedLevelNames.Add( FStreamInfo.GlobalInstance.NameArray[ LevelIndex ] );
				}
			}
		}
	};
}