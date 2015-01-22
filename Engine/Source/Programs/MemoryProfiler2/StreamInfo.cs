/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> Helper class containing information shared for all snapshots of a given stream. </summary>
	public class FStreamInfo
	{
		public const ulong INVALID_STREAM_INDEX = ulong.MaxValue;

		/// <summary> Global public stream info so we don't need to pass it around. </summary>
		public static FStreamInfo GlobalInstance = null;

		/// <summary> If True console symbol parser will ask user to locate the file containing symbols. </summary>
		public static bool bLoadDefaultSymbols = false;

		/// <summary> File name associated with this stream. </summary>
		public string FileName;

		/// <summary> Array of unique names. Code has fixed indexes into it. </summary>
		public List<string> NameArray;

		/// <summary> Array of unique names used for script, object types, etc... (FName table from runtime). </summary>
		public List<string> ScriptNameArray;

		/// <summary> Array of unique call stacks. Code has fixed indexes into it. </summary>
		public List<FCallStack> CallStackArray;

		/// <summary> Array of unique call stack addresses. Code has fixed indexes into it. </summary>
		public List<FCallStackAddress> CallStackAddressArray;

		/// <summary> Proper symbol parser will be created based on platform. </summary>
		public ISymbolParser ConsoleSymbolParser = null;

		/// <summary> Memory pool ranges for all pools. </summary>
		public FMemoryPoolInfoCollection MemoryPoolInfo = new FMemoryPoolInfoCollection();

		/// <summary> Array of frame indices located in the stream. </summary>
		public List<ulong> FrameStreamIndices = new List<ulong>();

		// 2011-Sep-08, 16:20 JarekS@TODO This should be replaced with GCurrentTime for more accurate results??
		/// <summary> Array of delta time for all frames. </summary> 
		public List<float> DeltaTimeArray = new List<float>(); 

		/// <summary> List of snapshots created by parser and used by various visualizers and dumping tools. </summary>
		public List<FStreamSnapshot> SnapshotList = new List<FStreamSnapshot>();

		/// <summary> Platform on which these statistics were collected V3. </summary>
		public EPlatformType Platform;

		/// <summary> Platform that was captured V4. </summary>
		public string PlatformName;

		/// <summary> Profiling options, default all options are enabled. UI is not provided yet. </summary>
		public ProfilingOptions CreationOptions;

		/// <summary> Array of script callstacks. </summary>
		public List<FScriptCallStack> ScriptCallstackArray;

		/// <summary> Index of UObject::ProcessInternal function in the main names array. </summary>
		public int ProcessInternalNameIndex;

		/// <summary> Index of UObject::StaticAllocateObject function in the main names array. </summary>
		public int StaticAllocateObjectNameIndex;

		/// <summary>
		/// Array of indices to all functions related to UObject Virtual Machine in the main names array.
		/// This array includes following functions:
		/// UObject::exec*
		/// UObject::CallFunction
		/// UObject::ProcessEvent
		/// </summary>
		public List<int> ObjectVMFunctionIndexArray = new List<int>( 64 );

		/// <summary> Mapping from the script-function name ([Script] package.class:function) in the main names array to the index in the main callstacks array. </summary>
		public Dictionary<int, int> ScriptCallstackMapping = new Dictionary<int, int>();

		/// <summary> Mapping from the script-object name ([Type] UObject::StaticAllocateObject) in the main names array to the index in the main callstacks array. </summary>
		public Dictionary<int, FScriptObjectType> ScriptObjectTypeMapping = new Dictionary<int, FScriptObjectType>();

		/// <summary> True if some callstacks has been allocated to multiple pools. </summary>
		public bool bHasMultiPoolCallStacks;

		/// <summary> Constructor, associating this stream info with a filename. </summary>
		/// <param name="InFileName"> FileName to use for this stream </param>
		public FStreamInfo( string InFileName, ProfilingOptions InCreationOptions )
		{
			FileName = InFileName;
			CreationOptions = new ProfilingOptions( InCreationOptions, true );
		}

		/// <summary> Initializes and sizes the arrays. Size is known as soon as header is serialized. </summary>
		public void Initialize( FProfileDataHeader Header )
		{
			Platform = Header.Platform;
			PlatformName = Header.PlatformName;
			NameArray = new List<string>( (int)Header.NameTableEntries );
			CallStackArray = new List<FCallStack>( (int)Header.CallStackTableEntries );
			CallStackAddressArray = new List<FCallStackAddress>( (int)Header.CallStackAddressTableEntries );
		}

		/// <summary> 
		/// Returns index of the name, if the name doesn't exit creates a new one if bCreateIfNonExistent is true
		/// NOTE: this is slow because it has to do a reverse lookup into NameArray
		/// </summary>
		public int GetNameIndex( string Name, bool bCreateIfNonExistent )
		{
			// This is the only method where there should be concurrent access of NameArray, so
			// don't worry about locking it anywhere else.
			lock( NameArray )
			{
				int NameIndex = NameArray.IndexOf( Name );

				if( NameIndex == -1 && bCreateIfNonExistent )
				{
					NameArray.Add( Name );
					NameIndex = NameArray.Count - 1;
				}

				return NameIndex;
			}
		}

		/// <summary> 
		/// If found returns index of the name, returns –1 otherwise.
		/// This method uses method Contains during looking up for the name.
		/// NOTE: this is slow because it has to do a reverse lookup into NameArray
		/// </summary>
		/// <param name="PartialName"> Partial name that we want find the index. </param>
		public int GetNameIndex( string PartialName )
		{
			// This is the only method where there should be concurrent access of NameArray, so
			// don't worry about locking it anywhere else.
			lock( NameArray )
			{
				for( int NameIndex = 0; NameIndex < NameArray.Count; NameIndex ++ )
				{
					if( NameArray[NameIndex].Contains( PartialName ) )
					{
						return NameIndex;
					}
				}

				return -1;
			}
		}

		/// <summary> Returns bank size in megabytes. </summary>
		public static int GetMemoryBankSize( int BankIndex )
		{
			Debug.Assert( BankIndex >= 0 );

			if( FStreamToken.Version >= 4 )
			{
				return BankIndex == 0 ? 512 : 0;
			}
			else
			{
				//private static int[][] MEMORY_BANK_SIZE = new int[][] { new int[] { 512, 1 }, new int[] { 180, 256 } };
				//private static int[][] MEMORY_BANK_SIZE_FOR_RENDERED_GRAPHS = new int[][] { new int[] { 512 * 1024 * 1024, 1 }, new int[] { 256 * 1024 * 1024, 256 * 1024 * 1024 } };
				if( GlobalInstance.Platform == EPlatformType.Xbox360 || GlobalInstance.Platform == EPlatformType.IPhone )
				{
					return BankIndex == 0 ? 512 : 0;
				}
				else if( GlobalInstance.Platform == EPlatformType.PS3 )
				{
					return BankIndex < 2 ? 256 : 0;
				}
				else if( ( GlobalInstance.Platform & EPlatformType.AnyWindows ) != EPlatformType.Unknown )
				{
					// Technically could be much larger, but the graph becomes unusable
					// if the scale is too large.
					return BankIndex == 0 ? 512 : 0;
				}
				else
				{
					throw new Exception( "Unsupported platform" );
				}
			}
		}

		/// <summary> Returns a frame number based on the stream index. </summary>
		public int GetFrameNumberFromStreamIndex( int StartFrame, ulong StreamIndex )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( StartFrame >= 1 && StartFrame < FrameStreamIndices.Count );

			// Check StartFrame is valid.
			Debug.Assert( StreamIndex != INVALID_STREAM_INDEX );

			// The only case where these can be equal is if they are both 0.
			Debug.Assert( StreamIndex >= FrameStreamIndices[ StartFrame - 1 ] );

			while( StreamIndex > FrameStreamIndices[ StartFrame ] && StartFrame + 1 < FrameStreamIndices.Count )
			{
				StartFrame++;
			}

			return StartFrame;
		}

		/// <summary> Returns the time of the stream index. </summary>
		/// <param name="StartFrame"> Stream index that we want to know the time. </param>
		public float GetTimeFromStreamIndex( ulong StreamIndex )
		{
			// Check StartFrame is valid.
			Debug.Assert( StreamIndex != INVALID_STREAM_INDEX );

			// Check StreamIndex is in the correct range.
			Debug.Assert( StreamIndex >= 0 && StreamIndex < FrameStreamIndices[ FrameStreamIndices.Count - 1 ] );

			int FrameNumber = GetFrameNumberFromStreamIndex( 1, StreamIndex );
			float Time = GetTimeFromFrameNumber( FrameNumber );
			return Time;
		}

		/// <summary> Returns the time of the frame. </summary>
		/// <param name="FrameNumber"> Frame number that we want to know the time. </param>
		public float GetTimeFromFrameNumber( int FrameNumber )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( FrameNumber >= 0 && FrameNumber < FrameStreamIndices.Count );

			return DeltaTimeArray[FrameNumber];
		}

		/// <summary> Returns the total time of the frame. </summary>
		/// <param name="FrameNumber"> Frame number that we want to know the total time. </param>
		public float GetTotalTimeFromFrameNumber( int FrameNumber )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( FrameNumber >= 0 && FrameNumber < FrameStreamIndices.Count );

			float TotalTime = 0.0f;

			for( int FrameIndex = 0; FrameIndex < FrameNumber; FrameIndex ++ )
			{
				TotalTime += DeltaTimeArray[ FrameIndex ];
			}

			return TotalTime;
		}

		public void Shutdown()
		{
			if( ConsoleSymbolParser is IDisplayCallback )
			{
				( ( IDisplayCallback )ConsoleSymbolParser ).Shutdown();
			}
		}
	}

	/// <summary> Helper class for logging timing in cases scopes.
	///	
	///	Example of usage
	/// 
	/// using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "HistogramParser.ParseSnapshot" ) )
	/// {
	///		code...
	/// }
	///
	/// </summary>
	public class FScopedLogTimer : IDisposable
	{
		/// <summary> Constructor. </summary>
		/// <param name="InDescription"> Global tag to use for logging </param>
		public FScopedLogTimer( string InDescription )
		{
			Description = InDescription;
			Timer = new Stopwatch();
			Start();
		}

		public void Start()
		{
			Timer.Start();
		}

		public void Stop()
		{
			Timer.Stop();
		}

		/// <summary> Destructor, logging delta time from constructor. </summary>
		public void Dispose()
		{
			Debug.WriteLine( Description + " took " + ( float )Timer.ElapsedMilliseconds / 1000.0f + " seconds to finish" );
		}

		/// <summary> Global tag to use for logging. </summary>
		protected string Description;

		/// <summary> Timer used to measure the timing. </summary>
		protected Stopwatch Timer;
	}

	public class FGlobalTimer
	{
		public FGlobalTimer( string InName )
		{
			Name = InName;
		}

		public void Start()
		{
			CurrentTicks = DateTime.Now.Ticks;
		}

		public void Stop()
		{
			long ElapsedTicks = DateTime.Now.Ticks - CurrentTicks;
			TotalTicks += ElapsedTicks;

			CallsNum++;
		}

		public override string ToString()
		{
			return string.Format( "Global timer for {0} took {1} ms with {2} calls", Name, ( float )TotalTicks / 10000.0f, CallsNum );
		}

		long CallsNum = 0;
		string Name;
		Stopwatch Timer = new Stopwatch();

		long TotalTicks = 0;
		long CurrentTicks = 0;
	};
}
