/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Windows.Forms;

namespace MemoryProfiler2
{	
	public static class FStreamParser
	{
        private static BinaryReader SwitchStreams(int PartIndex, string BaseFilename, bool bIsBigEndian, out FileStream ParserStream)
        {
            // Determine the effective filename for this part
            string CurrentFilename = BaseFilename;
            if (PartIndex > 0)
            {
                CurrentFilename = Path.ChangeExtension(BaseFilename, String.Format(".m{0}", PartIndex));
            }

            // Open the file
            ParserStream = File.OpenRead(CurrentFilename);

            // Create a reader of the correct endianness
			BinaryReader BinaryStream;            
            if (bIsBigEndian)
            {
				BinaryStream = new BinaryReaderBigEndian(ParserStream);
            }
            else
            {
                BinaryStream = new BinaryReader(ParserStream, System.Text.Encoding.ASCII);
            }
            return BinaryStream;
        }

		//-----------------------------------------------------------------------------
		//@DEBUG
		static FGlobalTimer MallocTimer = new FGlobalTimer( "Malloc" );
		static FGlobalTimer FreeTimer = new FGlobalTimer( "Free" );
		static FGlobalTimer ReallocTimer = new FGlobalTimer( "Realloc" );
		static FGlobalTimer OtherTimer = new FGlobalTimer( "Other" );
		static long IncompleteLifeCycles = 0;
		//-----------------------------------------------------------------------------

		/// <summary> Parses the passed in token stream and returns list of snapshots. </summary>
		public static void Parse( MainWindow MainMProfWindow, BackgroundWorker BGWorker, StreamObserver Observer, List<int> CustomSnapshots, DoWorkEventArgs EventArgs )
		{
            string PrettyFilename = Path.GetFileNameWithoutExtension(FStreamInfo.GlobalInstance.FileName);
			BGWorker.ReportProgress( 0, "1/8 Loading header information for " + PrettyFilename );

			// Create binary reader and file info object from filename.
            bool bIsBigEndian = false;
			FileStream ParserFileStream = File.OpenRead(FStreamInfo.GlobalInstance.FileName);
			BinaryReader BinaryStream = new BinaryReader(ParserFileStream,System.Text.Encoding.ASCII);

			// Serialize header.
			FProfileDataHeader Header = new FProfileDataHeader(BinaryStream);

			// Determine whether read file has magic header. If no, try again byteswapped.
			if(Header.Magic != FProfileDataHeader.ExpectedMagic)
			{
				// Seek back to beginning of stream before we retry.
				ParserFileStream.Seek(0,SeekOrigin.Begin);

				// Use big endian reader. It transparently endian swaps data on read.
				BinaryStream = new BinaryReaderBigEndian(ParserFileStream);
                bIsBigEndian = true;
				
				// Serialize header a second time.
				Header = new FProfileDataHeader(BinaryStream);
			}

			// At this point we should have a valid header. If no, throw an exception.
			if( Header.Magic != FProfileDataHeader.ExpectedMagic )
			{
				throw new InvalidDataException();
			}

            // Keep track of the current data file for multi-part recordings
            UInt64 NextDataFile = 1;

			// Initialize shared information across snapshots, namely names, callstacks and addresses.
			FStreamInfo.GlobalInstance.Initialize( Header );

			// Keep track of current position as it's where the token stream starts.
			long TokenStreamOffset = ParserFileStream.Position;

			// Seek to name table and serialize it.
			ParserFileStream.Seek((Int64)Header.NameTableOffset,SeekOrigin.Begin);
			for(UInt64 NameIndex = 0;NameIndex < Header.NameTableEntries;NameIndex++)
			{
				FStreamInfo.GlobalInstance.NameArray.Add( ReadString( BinaryStream ) );
			}

			// Seek to callstack address array and serialize it.                
			ParserFileStream.Seek( (Int64)Header.CallStackAddressTableOffset, SeekOrigin.Begin );
			for(UInt64 AddressIndex = 0;AddressIndex < Header.CallStackAddressTableEntries;AddressIndex++)
			{
                FStreamInfo.GlobalInstance.CallStackAddressArray.Add(new FCallStackAddress(BinaryStream, Header.bShouldSerializeSymbolInfo));
			}

			// Seek to callstack array and serialize it.
			ParserFileStream.Seek( (Int64)Header.CallStackTableOffset, SeekOrigin.Begin );

			for(UInt64 CallStackIndex = 0;CallStackIndex < Header.CallStackTableEntries;CallStackIndex++)
			{
                FStreamInfo.GlobalInstance.CallStackArray.Add(new FCallStack(BinaryStream));
			}

			// Check for pending cancellation of a background operation.
			if( BGWorker.CancellationPending )
			{
				EventArgs.Cancel = true;
				return;
			}

            // We need to look up symbol information ourselves if it wasn't serialized.
            try
            {
                if (!Header.bShouldSerializeSymbolInfo)
                {
                    LookupSymbols(Header, MainMProfWindow, BinaryStream, BGWorker);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(String.Format("Failed to look up symbols ({0}). Attempting to continue parsing stream", ex.Message), "Memory Profiler 2", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            // Seek to script callstack data and serialize it.
			if( Header.bDecodeScriptCallstacks )
			{
				BGWorker.ReportProgress( 0, "4/8 Decoding script callstacks for " + PrettyFilename );

				// Read the script name table (it's the full dumped FName table).
				ParserFileStream.Seek( Header.ScriptNameTableOffset, SeekOrigin.Begin );
				int NumScriptNames = BinaryStream.ReadInt32();
				FStreamInfo.GlobalInstance.ScriptNameArray = new List<string>( NumScriptNames );
				for( int ScriptIndex = 0; ScriptIndex < NumScriptNames; ++ScriptIndex )
				{
					FStreamInfo.GlobalInstance.ScriptNameArray.Add( ReadString( BinaryStream ) );
				}

				// Read the script call stacks.
				ParserFileStream.Seek( Header.ScriptCallstackTableOffset, SeekOrigin.Begin );
				int NumScriptCallstacks = BinaryStream.ReadInt32();
				FStreamInfo.GlobalInstance.ScriptCallstackArray = new List<FScriptCallStack>( NumScriptCallstacks );
				for( int ScriptIndex = 0; ScriptIndex < NumScriptCallstacks; ScriptIndex++ )
				{
					FStreamInfo.GlobalInstance.ScriptCallstackArray.Add( new FScriptCallStack( BinaryStream ) );
				}

				// Find the ProcessInternal index for later replacement if script callstacks were captured.
				FStreamInfo.GlobalInstance.ProcessInternalNameIndex = FStreamInfo.GlobalInstance.GetNameIndex( "UObject::ProcessInternal(FFrame&, void*)", false );
				if( FStreamInfo.GlobalInstance.ProcessInternalNameIndex == -1 )
				{
					// Try alternative name.
					FStreamInfo.GlobalInstance.ProcessInternalNameIndex = FStreamInfo.GlobalInstance.GetNameIndex( "UObject::ProcessInternal", false );

					if( FStreamInfo.GlobalInstance.ProcessInternalNameIndex == -1 )
					{
						Debug.WriteLine( "WARNING: Couldn't find name index for ProcessInternal(). Script callstacks will not be decoded." );
					}
				}
			
				// Build the list of names
				// UObject::exec*
				// UObject::CallFunction
				// UObject::ProcessEvent

				List<string> ObjectVMFunctionNamesArray = new List<string>();
				ObjectVMFunctionNamesArray.Add( "UObject::CallFunction" );
				ObjectVMFunctionNamesArray.Add( "UObject::ProcessEvent" );

				for( int NameIndex = 0; NameIndex < FStreamInfo.GlobalInstance.NameArray.Count; NameIndex ++ )
				{
					string Name = FStreamInfo.GlobalInstance.NameArray[ NameIndex ];
					if( Name.Contains( "UObject::exec" ) )
					{
						ObjectVMFunctionNamesArray.Add( Name );
					}
				}

				// Build the indices for functions related to object vm for later removal if script callstacks were captured.
				for( int FunctionIndex = 0; FunctionIndex < ObjectVMFunctionNamesArray.Count; FunctionIndex ++ )
				{
					string FunctionName = ObjectVMFunctionNamesArray[FunctionIndex];
					int Function2NamesIndex = FStreamInfo.GlobalInstance.GetNameIndex( FunctionName );
					Debug.Assert( Function2NamesIndex != -1 );
					FStreamInfo.GlobalInstance.ObjectVMFunctionIndexArray.Add( Function2NamesIndex );
				}
			}

			// Check for pending cancellation of a background operation.
			if( BGWorker.CancellationPending )
			{
				EventArgs.Cancel = true;
				return;
			}

			// Find the StaticAllocateObject index for later replacement if script callstacks were captured.
			if( Header.bDecodeScriptCallstacks )
			{
				FStreamInfo.GlobalInstance.StaticAllocateObjectNameIndex = FStreamInfo.GlobalInstance.GetNameIndex( "UObject::StaticAllocateObject(UClass*, UObject*, FName, unsigned long long, UObject*, FOutputDevice*, UObject*, UObject*, FObjectInstancingGraph*)", false );
				if( FStreamInfo.GlobalInstance.StaticAllocateObjectNameIndex == -1 )
				{
					// Try alternative name.
					FStreamInfo.GlobalInstance.StaticAllocateObjectNameIndex = FStreamInfo.GlobalInstance.GetNameIndex( "UObject::StaticAllocateObject", false );

					if( FStreamInfo.GlobalInstance.StaticAllocateObjectNameIndex == -1 )
					{
						Debug.WriteLine( "WARNING: Couldn't find name index for StaticAllocateObject(). Script types will not be processed." );
					}
				}
			}

			BGWorker.ReportProgress( 0, "5/8 Trimming profiler entries for " + PrettyFilename );;

			// Trim profiler entries from callstacks.
			foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
			{
				CallStack.TrimProfilerEntries();
			}

			if( MainMProfWindow.Options.FilterOutObjectVMFunctions )
			{
				BGWorker.ReportProgress( 0, "6/8 Filtering out functions related to UObject Virtual Machine for " + PrettyFilename );

				// Filter out functions related to UObject Virtual Machine from callstacks.
				foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
				{
					CallStack.FilterOutObjectVMFunctions();
				}
			}

			// Snapshot used for parsing. A copy will be made if a special token is encountered. Otherwise it
			// will be returned as the only snaphot at the end.
			FStreamSnapshot Snapshot = new FStreamSnapshot("End");
			List<FStreamSnapshot> SnapshotList = new List<FStreamSnapshot>();
            Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap = new Dictionary<ulong, FCallStackAllocationInfo>();

			// Seek to beginning of token stream.
			ParserFileStream.Seek(TokenStreamOffset, SeekOrigin.Begin);

            FStreamToken Token;
            FStreamToken.bDecodeScriptCallstacks = Header.bDecodeScriptCallstacks;

            ResetSnapshotDescriptions();

            bool bFoundMultiPoolCallStacks = false;

            // Start frame 0.
            FStreamInfo.GlobalInstance.FrameStreamIndices.Add(0);
			FStreamInfo.GlobalInstance.DeltaTimeArray.Add( 0.0f );

            // The USE_GLOBAL_REALLOC_ZERO_PTR option is used by dlmalloc.
            // When it's enabled, calls to realloc(NULL, 0) will always return the same valid pointer,
            // which can cause unnecessary warnings (double frees, etc) when we parse the allocation data.
            // Whether the option is enabled or disabled will be automatically detected by the code below.
            bool bUseGlobalReallocZeroPtr = false;
            bool bDetectingUseGlobalReallocZeroPtr = true;
            int ReallocZeroCount = 0;
            ulong ReallocZeroPtr = 0;

            FAllocationLifecycle NewLifecycle = new FAllocationLifecycle();

            int SnapshotIndex = 0;

            // Figure out the progress scale
            UInt64 StartOfMetadata = Math.Min(Header.NameTableOffset, Header.CallStackAddressTableOffset);
            StartOfMetadata = Math.Min(StartOfMetadata, Header.CallStackTableOffset);
            StartOfMetadata = Math.Min(StartOfMetadata, Header.ModulesOffset);

            long ProgressInterval = ((Int64)StartOfMetadata - TokenStreamOffset) / 100;
			double ProgressScaleFactor = 100.0f / ( (Int64)StartOfMetadata - TokenStreamOffset );
            long NextProgressUpdate = TokenStreamOffset;

			// Parse tokens till we reach the end of the stream.
			Token = new FStreamToken();

			using( FScopedLogTimer LoadingTime = new FScopedLogTimer( "Reading and parsing tokens") )
			{
				EProfilingPayloadType LastTokenType = EProfilingPayloadType.TYPE_Other;

				while( Token.ReadNextToken( BinaryStream ) )
				{
					// Check for pending cancellation of a background operation.
					if( BGWorker.CancellationPending )
					{
						EventArgs.Cancel = true;
						return;
					}

					long CurrentStreamPos = ParserFileStream.Position;

					if( ParserFileStream.Position >= NextProgressUpdate )
					{
						BGWorker.ReportProgress(
							( int )( ( CurrentStreamPos - TokenStreamOffset ) * ProgressScaleFactor ),
							String.Format( "7/8 Parsing token stream for {0}, part {1} of {2}", PrettyFilename, NextDataFile, Header.NumDataFiles ) );
						NextProgressUpdate += ProgressInterval;
					}

					if( CustomSnapshots.Count > 0 && Snapshot.AllocationCount >= CustomSnapshots[ 0 ] * 5000 )
					{
						// Create an unnamed snapshot.
						FStreamSnapshot MarkerSnapshot = Snapshot.DeepCopy( PointerToPointerInfoMap );

						MarkerSnapshot.Description = "Unnamed snapshot allocations: " + Snapshot.AllocationCount;
						MarkerSnapshot.StreamIndex = Token.StreamIndex;
						MarkerSnapshot.FrameNumber = FStreamInfo.GlobalInstance.FrameStreamIndices.Count;
						MarkerSnapshot.CurrentTime = Token.TotalTime;
						MarkerSnapshot.ElapsedTime = Token.ElapsedTime;
						MarkerSnapshot.SubType = Token.SubType;
						MarkerSnapshot.SnapshotIndex = SnapshotIndex;
						MarkerSnapshot.MetricArray = new List<long>( Token.Metrics );
						MarkerSnapshot.LoadedLevels = new List<int>( Token.LoadedLevels );
						MarkerSnapshot.OverallMemorySlice = new List<FMemorySlice>( Snapshot.OverallMemorySlice );
						MarkerSnapshot.MemoryAllocationStats3 = Token.MemoryAllocationStats3.DeepCopy();
						MarkerSnapshot.MemoryAllocationStats4 = Token.MemoryAllocationStats4.DeepCopy();

						FStreamInfo.GlobalInstance.SnapshotList.Add( MarkerSnapshot );

						CustomSnapshots.RemoveAt( 0 );

						Token.ElapsedTime = 0.0f;
					}

					switch( Token.Type )
					{
						// Malloc
						case EProfilingPayloadType.TYPE_Malloc:
						{
							MallocTimer.Start();

							if( Token.Pointer != 0 )
							{
								Token.CallStackIndex = GetVirtualCallStackIndex( Token, Observer );

								HandleMalloc( Token, Snapshot, PointerToPointerInfoMap );

								FCallStack CurrentCallstack = FStreamInfo.GlobalInstance.CallStackArray[ Token.CallStackIndex ];
								if( CurrentCallstack.MemoryPool != EMemoryPool.MEMPOOL_None && CurrentCallstack.MemoryPool != Token.Pool )
								{
									bFoundMultiPoolCallStacks = true;
								}

								CurrentCallstack.MemoryPool |= Token.Pool;
								FStreamInfo.GlobalInstance.MemoryPoolInfo[ Token.Pool ].AddPointer( Token.Pointer, Token.Size );

								CurrentCallstack.ProcessMalloc( Token, ref NewLifecycle );
							}

							MallocTimer.Stop();
						}
						break;

						// Free
						case EProfilingPayloadType.TYPE_Free:
						{
							FreeTimer.Start();

							if( bDetectingUseGlobalReallocZeroPtr )
							{
								if( ReallocZeroCount > 0 && Token.Pointer == ReallocZeroPtr )
								{
									ReallocZeroCount--;
								}
							}

							if( bDetectingUseGlobalReallocZeroPtr || !bUseGlobalReallocZeroPtr || Token.Pointer != ReallocZeroPtr )
							{
								// Either USE_GLOBAL_REALLOC_ZERO_PTR is not being used, or we're not
								// trying to free the ReallocZeroPtr.

								FCallStackAllocationInfo FreedAllocInfo;
								if( HandleFree( Token, Snapshot, PointerToPointerInfoMap, out FreedAllocInfo ) )
								{
									FCallStack PreviousCallStack = FStreamInfo.GlobalInstance.CallStackArray[ FreedAllocInfo.CallStackIndex ];
									PreviousCallStack.ProcessFree( Token );
								}
							}

							FreeTimer.Stop();
						}
						break;

						// Realloc
						case EProfilingPayloadType.TYPE_Realloc:
						{
							ReallocTimer.Start();

							Token.CallStackIndex = GetVirtualCallStackIndex( Token, Observer );

							FCallStack PreviousCallstack = null;
							FAllocationLifecycle OldReallocLifecycle = null;

							if( Token.OldPointer != 0 )
							{
								FCallStackAllocationInfo FreedAllocInfo;
								if( HandleFree( Token, Snapshot, PointerToPointerInfoMap, out FreedAllocInfo ) )
								{
									PreviousCallstack = FStreamInfo.GlobalInstance.CallStackArray[ FreedAllocInfo.CallStackIndex ];
									if( Token.Size > 0 )
									{
										OldReallocLifecycle = PreviousCallstack.ProcessRealloc( Token, ref NewLifecycle, null, null );
									}
									else
									{
										PreviousCallstack.ProcessFree( Token );
									}
								}
							}
							else if( Token.Size == 0 )
							{
								if( bDetectingUseGlobalReallocZeroPtr )
								{
									if( ReallocZeroCount > 1 )
									{
										// This code checks to see if the return values of the second and third realloc(0, NULL) calls
										// match. The first one is always different for some reason.

										bUseGlobalReallocZeroPtr = Token.NewPointer == ReallocZeroPtr;
										bDetectingUseGlobalReallocZeroPtr = false;

										Debug.WriteLine( "USE_GLOBAL_REALLOC_ZERO_PTR is " + bUseGlobalReallocZeroPtr );
									}
									else
									{
										ReallocZeroPtr = Token.NewPointer;
										ReallocZeroCount++;
									}
								}


								if( bUseGlobalReallocZeroPtr )
								{
									// break out of case to avoid 'double malloc' warnings
									ReallocTimer.Stop();
									break;
								}
							}

							if( Token.NewPointer != 0 )
							{
								Token.Pointer = Token.NewPointer;

								FCallStack CurrentCallstack = FStreamInfo.GlobalInstance.CallStackArray[ Token.CallStackIndex ];

								if( CurrentCallstack.MemoryPool != EMemoryPool.MEMPOOL_None && CurrentCallstack.MemoryPool != Token.Pool )
								{
									bFoundMultiPoolCallStacks = true;
								}

								CurrentCallstack.MemoryPool |= Token.Pool;
								FStreamInfo.GlobalInstance.MemoryPoolInfo[ Token.Pool ].AddPointer( Token.Pointer, Token.Size );

								HandleMalloc( Token, Snapshot, PointerToPointerInfoMap );

								if( CurrentCallstack != PreviousCallstack )
								{
									CurrentCallstack.ProcessRealloc( Token, ref NewLifecycle, PreviousCallstack, OldReallocLifecycle );
									IncompleteLifeCycles += CurrentCallstack.IncompleteLifecycles.Count;

								}

								ReallocTimer.Stop();
							}
						}
						break;

						// Status/ payload.
						case EProfilingPayloadType.TYPE_Other:
						{
							OtherTimer.Start();

							switch( Token.SubType )
							{
								case EProfilingPayloadSubType.SUBTYPE_EndOfStreamMarker:
								{
									// Should never receive EOS marker as ReadNextToken should've returned false.
									throw new InvalidDataException();
								}

								case EProfilingPayloadSubType.SUBTYPE_EndOfFileMarker:
								{
									// Switch to the next file in the chain
									ParserFileStream.Close();
									BinaryStream = SwitchStreams( (int)NextDataFile, FStreamInfo.GlobalInstance.FileName, bIsBigEndian, out ParserFileStream );

									// Update variables used for reporting progress
									TokenStreamOffset = 0;
									ProgressInterval = ParserFileStream.Length / 100;
									ProgressScaleFactor = 100.0f / ParserFileStream.Length;
									NextProgressUpdate = 0;

									// Tick over to the next file, and make sure things are still ending as expected
									NextDataFile++;
									if( NextDataFile > Header.NumDataFiles )
									{
										throw new InvalidDataException( "Found an unexpected number of data files (more than indicated in the master file" );
									}
									break;
								}

								// Create snapshot.
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Mid:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_Start:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_End:
								case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker:
								{
									if( ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start
											&& FStreamInfo.GlobalInstance.CreationOptions.LoadMapStartSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Mid
											&& FStreamInfo.GlobalInstance.CreationOptions.LoadMapMidSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End
											&& FStreamInfo.GlobalInstance.CreationOptions.LoadMapEndSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start
											&& FStreamInfo.GlobalInstance.CreationOptions.GCStartSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End
											&& FStreamInfo.GlobalInstance.CreationOptions.GCEndSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_Start
											&& FStreamInfo.GlobalInstance.CreationOptions.LevelStreamStartSnapshotsCheckBox.Checked )
										|| ( Token.SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_End )
											&& FStreamInfo.GlobalInstance.CreationOptions.LevelStreamEndSnapshotsCheckBox.Checked )
									{
										FStreamSnapshot MarkerSnapshot = Snapshot.DeepCopy( PointerToPointerInfoMap );

										MarkerSnapshot.Description = GetNextSnapshotDescription( Token.SubType, FStreamInfo.GlobalInstance.NameArray[ Token.TextIndex ] );
										MarkerSnapshot.StreamIndex = Token.StreamIndex;
										MarkerSnapshot.FrameNumber = FStreamInfo.GlobalInstance.FrameStreamIndices.Count;
										MarkerSnapshot.CurrentTime = Token.TotalTime;
										MarkerSnapshot.ElapsedTime = Token.ElapsedTime;
										MarkerSnapshot.SubType = Token.SubType;
										MarkerSnapshot.SnapshotIndex = SnapshotIndex;
										MarkerSnapshot.MetricArray = new List<long>( Token.Metrics );
										MarkerSnapshot.LoadedLevels = new List<int>( Token.LoadedLevels );
										MarkerSnapshot.MemoryAllocationStats3 = Snapshot.MemoryAllocationStats3.DeepCopy();
										MarkerSnapshot.MemoryAllocationStats4 = Snapshot.MemoryAllocationStats4.DeepCopy();
										MarkerSnapshot.OverallMemorySlice = new List<FMemorySlice>( Snapshot.OverallMemorySlice );

										FStreamInfo.GlobalInstance.SnapshotList.Add( MarkerSnapshot );

										Token.ElapsedTime = 0.0f;
									}

									SnapshotIndex++;
									break;
								}


								case EProfilingPayloadSubType.SUBTYPE_TotalUsed:
								{
									Snapshot.MemoryAllocationStats3.TotalUsed = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_TotalAllocated:
								{
									Snapshot.MemoryAllocationStats3.TotalAllocated = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_CPUUsed:
								{
									Snapshot.MemoryAllocationStats3.CPUUsed = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_CPUSlack:
								{
									Snapshot.MemoryAllocationStats3.CPUSlack = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_CPUWaste:
								{
									Snapshot.MemoryAllocationStats3.CPUWaste = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_GPUUsed:
								{
									Snapshot.MemoryAllocationStats3.GPUUsed = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_GPUSlack:
								{
									Snapshot.MemoryAllocationStats3.GPUSlack = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_GPUWaste:
								{
									Snapshot.MemoryAllocationStats3.GPUWaste = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_ImageSize:
								{
									Snapshot.MemoryAllocationStats3.ImageSize = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_OSOverhead:
								{
									Snapshot.MemoryAllocationStats3.OSOverhead = Token.Payload;
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_FrameTimeMarker:
								{
									FStreamInfo.GlobalInstance.FrameStreamIndices.Add( Token.StreamIndex );
									FStreamInfo.GlobalInstance.DeltaTimeArray.Add( Token.DeltaTime );
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_TextMarker:
								{
									break;
								}

								case EProfilingPayloadSubType.SUBTYPE_MemoryAllocationStats:
								{
									Snapshot.MemoryAllocationStats3 = Token.MemoryAllocationStats3.DeepCopy();
									Snapshot.MemoryAllocationStats4 = Token.MemoryAllocationStats4.DeepCopy();
									break;
								}

								// Unhandled.
								default:
								{
									throw new InvalidDataException();
								}
							}
							OtherTimer.Stop();
							break;
						}

						// Unhandled.
						default:
						{
							throw new InvalidDataException();
						}
					}

					if( NewLifecycle == null )
					{
						NewLifecycle = new FAllocationLifecycle();
					}

					// Advance the stream index.
					Token.StreamIndex++;
					LastTokenType = Token.Type;
				}
			}
			//-----------------------------------------------------------------------------
			//@DEBUG
			string Info = "Timings\n" +
						MallocTimer.ToString() + "\n" +
						FreeTimer.ToString() + "\n" +
						ReallocTimer.ToString() + "\n" +
						OtherTimer.ToString() + "\n" +
						"IncompleteLifeCycles " + IncompleteLifeCycles;

			Debug.WriteLine( Info );
			//-----------------------------------------------------------------------------

            if (MainMProfWindow != null && bFoundMultiPoolCallStacks)
            {
                MessageBox.Show("Some callstacks appear to allocate to multiple pools. This will make profiling more difficult and is usually caused by function inlining. It can also be caused by using the wrong executable to decode the profile.");
            }

            FStreamInfo.GlobalInstance.bHasMultiPoolCallStacks = bFoundMultiPoolCallStacks;

			// Closes the file so it can potentially be opened for writing.
			ParserFileStream.Close();

            // Mark end of last frame.
			FStreamInfo.GlobalInstance.FrameStreamIndices.Add( Token.StreamIndex );
			FStreamInfo.GlobalInstance.DeltaTimeArray.Add( 0.0f );

            // make sure all lifetimecallstacklists are as big as the latest one
			foreach( FStreamSnapshot PreviousSnapshot in FStreamInfo.GlobalInstance.SnapshotList )
			{
				while( PreviousSnapshot.LifetimeCallStackList.Count < Snapshot.LifetimeCallStackList.Count )
				{
					PreviousSnapshot.LifetimeCallStackList.Add( new FCallStackAllocationInfo( 0, PreviousSnapshot.LifetimeCallStackList.Count, 0 ) );
				}
			}

            List<CallStackPattern> OrderedPatternList = MainMProfWindow.Options.GetOrderedPatternList();
            ClassGroup UngroupedGroup = MainMProfWindow.Options.UngroupedGroup;


			double CallStackScaleFactor = 100.0f / FStreamInfo.GlobalInstance.CallStackArray.Count;
			long CallStackInterval = FStreamInfo.GlobalInstance.CallStackArray.Count / 100;
			long CallStackNextProgressUpdate = CallStackInterval;

			int CallStackCurrent = 0;
			foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
			{
				if( CallStackCurrent >= CallStackNextProgressUpdate )
				{
					BGWorker.ReportProgress( ( int )( CallStackCurrent * CallStackScaleFactor ), "8/8 Matching callstacks to groups for " + PrettyFilename );
					CallStackNextProgressUpdate += CallStackInterval;
				}
				CallStackCurrent++;

				if( CallStack.AddressIndices.Count > 0 )
				{
					// Find the first non templated entry in each callstack
					CallStack.EvaluateFirstNonContainer();

					// Go through each pattern to find the first match.
					// It's important that the patterns are evaluated in the correct order.
					foreach( CallStackPattern Pattern in OrderedPatternList )
					{
						if( Pattern.Matches( CallStack ) )
						{
							CallStack.Group = Pattern.Group;
							Pattern.CallStacks.Add( CallStack );
							break;
						}
					}

					if( CallStack.Group == null )
					{
						CallStack.Group = UngroupedGroup;
						UngroupedGroup.CallStackPatterns[ 0 ].CallStacks.Add( CallStack );
					}
				}			
            }

			// Add snapshot in end state to the list and return it.
			Snapshot.StreamIndex = Token.StreamIndex;
            Snapshot.FrameNumber = FStreamInfo.GlobalInstance.FrameStreamIndices.Count;
			Snapshot.CurrentTime = Token.TotalTime;
			Snapshot.ElapsedTime = Token.ElapsedTime;
            Snapshot.MetricArray = new List<long>(Token.Metrics);
            Snapshot.LoadedLevels = new List<int>(Token.LoadedLevels);
			Snapshot.MemoryAllocationStats3 = Token.MemoryAllocationStats3.DeepCopy();
			Snapshot.MemoryAllocationStats4 = Token.MemoryAllocationStats4.DeepCopy();
            Snapshot.FinalizeSnapshot(PointerToPointerInfoMap);
            FStreamInfo.GlobalInstance.SnapshotList.Add(Snapshot);

            BGWorker.ReportProgress(100, "Finalizing snapshots for " + PrettyFilename);

			// Finalize snapshots. This entails creating the sorted snapshot list.
			foreach( FStreamSnapshot SnapshotToFinalize in SnapshotList )
			{
				SnapshotToFinalize.FinalizeSnapshot(null);
			}
		}

		private static int GetVirtualCallStackIndex( FStreamToken StreamToken, StreamObserver Observer )
		{
			if( StreamToken.ScriptCallstackIndex == -1 && StreamToken.ScriptObjectTypeIndex == -1 )
			{
				return StreamToken.CallStackIndex;
			}

			FCallStack OriginalCallStack = FStreamInfo.GlobalInstance.CallStackArray[ StreamToken.CallStackIndex ];
			FScriptCallStack ScriptCallStack = StreamToken.ScriptCallstackIndex == -1 ? null : FStreamInfo.GlobalInstance.ScriptCallstackArray[ StreamToken.ScriptCallstackIndex ];

			// Get the script-object type for this index (if it's not cached, create a ScriptObjectType instance and cache it).
			FScriptObjectType ScriptObjectType = null;
			if( StreamToken.ScriptObjectTypeIndex != -1 )
			{
				if( !FStreamInfo.GlobalInstance.ScriptObjectTypeMapping.TryGetValue( StreamToken.ScriptObjectTypeIndex, out ScriptObjectType ) )
				{
					ScriptObjectType = new FScriptObjectType( FStreamInfo.GlobalInstance.ScriptNameArray[ StreamToken.ScriptObjectTypeIndex ] );
					FStreamInfo.GlobalInstance.ScriptObjectTypeMapping.Add( StreamToken.ScriptObjectTypeIndex, ScriptObjectType );
				}
			}

			for( int ChildIndex = 0; ChildIndex < OriginalCallStack.Children.Count; ChildIndex++ )
			{
				if( ( OriginalCallStack.Children[ ChildIndex ].ScriptCallStack == ScriptCallStack )
					&& ( OriginalCallStack.Children[ ChildIndex ].ScriptObjectType == ScriptObjectType ) )
				{
					return OriginalCallStack.ChildIndices[ ChildIndex ];
				}
			}

			FCallStack NewCallStack = new FCallStack( OriginalCallStack, ScriptCallStack, ScriptObjectType, FStreamInfo.GlobalInstance.CallStackArray.Count );
			FStreamInfo.GlobalInstance.CallStackArray.Add( NewCallStack );

			return FStreamInfo.GlobalInstance.CallStackArray.Count - 1;
		}

		/// <summary> Updates internal state with allocation. </summary>
        private static void HandleMalloc(FStreamToken StreamToken, FStreamSnapshot Snapshot, Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap)
		{
			// Keep track of size associated with pointer and also current callstack.
            FCallStackAllocationInfo PointerInfo = new FCallStackAllocationInfo(StreamToken.Size, StreamToken.CallStackIndex, 1);

            if (PointerToPointerInfoMap.ContainsKey(StreamToken.Pointer))
            {
                Debug.WriteLine("Same pointer malloced twice without being freed: " + StreamToken.Pointer + " in pool " + StreamToken.Pool);
                PointerToPointerInfoMap.Remove(StreamToken.Pointer);
            }

            PointerToPointerInfoMap.Add(StreamToken.Pointer, PointerInfo);

            if (StreamToken.CallStackIndex >= FStreamInfo.GlobalInstance.CallStackArray.Count)
            {
                Debug.WriteLine("CallStackIndex out of range!");
                return;
            }
            
            // Add size to lifetime churn tracking.
            while (StreamToken.CallStackIndex >= Snapshot.LifetimeCallStackList.Count)
			{
                Snapshot.LifetimeCallStackList.Add(new FCallStackAllocationInfo(0, Snapshot.LifetimeCallStackList.Count, 0));
			}

            //@todo: Sadly, we have to do all this ugly shuffling because of the way lists of structs work in C#
			Snapshot.LifetimeCallStackList[ StreamToken.CallStackIndex ] = Snapshot.LifetimeCallStackList[ StreamToken.CallStackIndex ].Add( StreamToken.Size, 1 );

			if( Snapshot.AllocationSize > Snapshot.AllocationMaxSize )
			{
				Snapshot.AllocationMaxSize = Snapshot.AllocationSize;
			}

			// Maintain timeline view
			Snapshot.AllocationSize += PointerInfo.Size;
			Snapshot.AllocationCount++;
			if( Snapshot.AllocationCount % 5000 == 0 )
			{
				FMemorySlice Slice = new FMemorySlice( Snapshot );
				Snapshot.OverallMemorySlice.Add( Slice );
				Snapshot.AllocationMaxSize = 0;
			}
		}

		/// <summary> Updates internal state with free. </summary>
        private static bool HandleFree(FStreamToken StreamToken, FStreamSnapshot Snapshot, Dictionary<ulong, FCallStackAllocationInfo> PointerToPointerInfoMap, out FCallStackAllocationInfo FreedAllocInfo)
		{
            if (!PointerToPointerInfoMap.TryGetValue(StreamToken.Pointer, out FreedAllocInfo))
            {
                Debug.WriteLine("Free without malloc: " + StreamToken.Pointer + " in pool " + StreamToken.Pool);
                return false;
            }

			// Maintain timeline view
            Snapshot.AllocationSize -= FreedAllocInfo.Size;
			Snapshot.AllocationCount++;
			if( Snapshot.AllocationCount % 5000 == 0 )
			{
				FMemorySlice Slice = new FMemorySlice( Snapshot );
				Snapshot.OverallMemorySlice.Add( Slice );
			}

			// Remove freed pointer if it is in the array.
			PointerToPointerInfoMap.Remove(StreamToken.Pointer);

            return true;
		}

		/// <summary> Lookup symbols for callstack addresses in passed in FStreamInfo.GlobalInstance. </summary>
		private static void LookupSymbols( FProfileDataHeader Header, MainWindow MainMProfWindow, BinaryReader BinaryStream, BackgroundWorker BGWorker )
		{
            string PrettyFilename = Path.GetFileNameWithoutExtension(FStreamInfo.GlobalInstance.FileName);
			BGWorker.ReportProgress( 0, "2/8 Loading symbols for " + PrettyFilename );
            
            // Proper Symbol parser will be created based on platform.
			FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;

            // Search for symbols in the same directory as the mprof first to allow packaging of previous results
			FStreamInfo.bLoadDefaultSymbols = false;
            if (Header.Platform == EPlatformType.PS3)
            {
                FStreamInfo.GlobalInstance.ConsoleSymbolParser = new FPS3SymbolParser();
                int LastPathSeparator = MainMProfWindow.CurrentFilename.LastIndexOf('\\');
                if (LastPathSeparator != -1)
                {
                    string CurrentPath = MainMProfWindow.CurrentFilename.Substring(0, LastPathSeparator);
					if( !FStreamInfo.GlobalInstance.ConsoleSymbolParser.LoadSymbols( CurrentPath + "\\" + Header.ExecutableName + ".elf" ) )
                    {
						FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;
                    }
                }
            }
			else if( Header.Platform == EPlatformType.Xbox360 )
            {
                Directory.SetCurrentDirectory(Path.GetDirectoryName(MainMProfWindow.CurrentFilename));

                FStreamInfo.GlobalInstance.ConsoleSymbolParser = new FXbox360SymbolParser();
                int LastPathSeparator = MainMProfWindow.CurrentFilename.LastIndexOf('\\');
                if (LastPathSeparator != -1)
                {
					if( Header.ModuleEntries > 0 )
					{
						BinaryStream.BaseStream.Seek( (Int64)Header.ModulesOffset, SeekOrigin.Begin );
						FStreamInfo.GlobalInstance.ConsoleSymbolParser.ReadModuleInfo( BinaryStream, (uint)Header.ModuleEntries );
					}

                    string CurrentPath = MainMProfWindow.CurrentFilename.Substring(0, LastPathSeparator);
                    if (!FStreamInfo.GlobalInstance.ConsoleSymbolParser.LoadSymbols(CurrentPath + "\\" + Header.ExecutableName + ".xex" ) )
                    {
                        FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;
                    }
                }
            }
            else if ((Header.Platform & EPlatformType.AnyWindows) != EPlatformType.Unknown)
			{
				// unsupported - application evaluates symbols at runtime
			}

            // If symbols weren't found in the same directory as the mprof, search the intermediate pdb locations
			FStreamInfo.bLoadDefaultSymbols = true;
            if (FStreamInfo.GlobalInstance.ConsoleSymbolParser == null)
            {
                if (Header.Platform == EPlatformType.PS3)
                {
					FStreamInfo.GlobalInstance.ConsoleSymbolParser = new FPS3SymbolParser();
					if( !FStreamInfo.GlobalInstance.ConsoleSymbolParser.LoadSymbols( Header.ExecutableName + ".elf" ) )
					{
						FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;
					}
                }
				else if( Header.Platform == EPlatformType.Xbox360 )
                {
                    FStreamInfo.GlobalInstance.ConsoleSymbolParser = new FXbox360SymbolParser();
                    if (!FStreamInfo.GlobalInstance.ConsoleSymbolParser.LoadSymbols("..\\" + Header.ExecutableName + ".xex" ) )
                    {
                        FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;
                    }
                }
				else if (Header.Platform == EPlatformType.IPhone)
				{
					FStreamInfo.GlobalInstance.ConsoleSymbolParser = new FIPhoneSymbolParser();
					if (!FStreamInfo.GlobalInstance.ConsoleSymbolParser.LoadSymbols(Header.ExecutableName + ".ipa"))
					{
						FStreamInfo.GlobalInstance.ConsoleSymbolParser = null;
					}
				}
                else if ((Header.Platform & EPlatformType.AnyWindows) != EPlatformType.Unknown)
                {
					// unsupported - application evaluates symbols at runtime
				}
            }

			// If console symbol parser is null it means that either the platform is not supported or the symbols couldn't
			// be loaded.
			if( FStreamInfo.GlobalInstance.ConsoleSymbolParser == null )
			{
                if (MainMProfWindow != null)
                {
                    MessageBox.Show("Failed to load symbols for " + Header.ExecutableName);
                }
                else
                {
                    Console.WriteLine("Failed to load symbols for " + Header.ExecutableName);
                }

				throw new InvalidDataException();
			}

			// Create mapping from string to index in name array.
			Dictionary<string, int> NameToIndexMap = new Dictionary<string,int>();

			// Propagate existing name entries to map.
			for( int NameIndex=0; NameIndex<FStreamInfo.GlobalInstance.NameArray.Count; NameIndex++ )
			{
				NameToIndexMap.Add( FStreamInfo.GlobalInstance.NameArray[NameIndex], NameIndex );
			}

			// Current index is incremented whenever a new name is added.
			int CurrentNameIndex = FStreamInfo.GlobalInstance.NameArray.Count;

			// Iterate over all addresses and look up symbol information.
            int NumAddressesPerTick = FStreamInfo.GlobalInstance.CallStackAddressArray.Count / 100;
            int AddressIndex = 0;
			string ProgressString = "3/8 Resolving symbols for " + PrettyFilename;

			if (Header.Platform == EPlatformType.IPhone)
			{
				// When resolving the addresses to symbols we require the use of UnrealRemoteTool to call
				// the relevant command on the mac. This method allows us to minimize network traffic required
				const int MaxAddressesPerBatch = 5000;
				int NumCallStackAddresses = FStreamInfo.GlobalInstance.CallStackAddressArray.Count;

				for (int NextBatchIdx = 0; NextBatchIdx < NumCallStackAddresses; NextBatchIdx += MaxAddressesPerBatch)
				{
					int RemainingAddresses = FStreamInfo.GlobalInstance.CallStackAddressArray.Count - NextBatchIdx;
					int BatchCount = RemainingAddresses >= MaxAddressesPerBatch ? MaxAddressesPerBatch : RemainingAddresses;

					ulong[] Addresses = new ulong[BatchCount];

					String[] FileNames = new String[BatchCount];
					String[] FunctionNames = new String[BatchCount];
					int[] LineNumbers = new int[BatchCount];

					if (NumCallStackAddresses - NextBatchIdx >= MaxAddressesPerBatch)
					{
						int PercentageComplete = (int)(((float)NextBatchIdx / (float)NumCallStackAddresses) * (float)100);
						BGWorker.ReportProgress(PercentageComplete, ProgressString);
					}

					for (int CurrentAddressIdx = NextBatchIdx; CurrentAddressIdx < NextBatchIdx + MaxAddressesPerBatch; CurrentAddressIdx++)
					{
						// Jump out of the loops if we have iterated over all addresses
						if (CurrentAddressIdx >= FStreamInfo.GlobalInstance.CallStackAddressArray.Count)
						{
							break;
						}

						FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[CurrentAddressIdx];

						int BatchAddressIdx = CurrentAddressIdx % MaxAddressesPerBatch;
						Addresses[BatchAddressIdx] = (ulong)Address.ProgramCounter;
					}

					// Use Atos on the target Mac to resolve the symbols in batches.
					bool bSuccessfullyResolved = FStreamInfo.GlobalInstance.ConsoleSymbolParser.ResolveAddressBatchesToSymbolInfo(Addresses, ref FileNames, ref FunctionNames, ref LineNumbers);

					if (bSuccessfullyResolved)
					{
						for (int CurrentAddressIdx = NextBatchIdx; CurrentAddressIdx < NextBatchIdx + BatchCount; CurrentAddressIdx++)
						{
							FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[CurrentAddressIdx];

							int BatchAddressIdx = CurrentAddressIdx % MaxAddressesPerBatch;
							String Filename = FileNames[BatchAddressIdx];
							String Function = FunctionNames[BatchAddressIdx];
							Address.LineNumber = LineNumbers[BatchAddressIdx];

							if (Filename != null && Function != null)
							{
								// Look up filename index.
								if (NameToIndexMap.ContainsKey(Filename))
								{
									// Use existing entry.
									Address.FilenameIndex = NameToIndexMap[Filename];
								}
								// Not found, so we use global name index to set new one.
								else
								{
									// Set name in map associated with global ever increasing index.
									Address.FilenameIndex = CurrentNameIndex++;
									NameToIndexMap.Add(Filename, Address.FilenameIndex);
								}

								// Look up function index.
								if (NameToIndexMap.ContainsKey(Function))
								{
									// Use existing entry.
									Address.FunctionIndex = NameToIndexMap[Function];
								}
								// Not found, so we use global name index to set new one.
								else
								{
									// Set name in map associated with global ever increasing index.
									Address.FunctionIndex = CurrentNameIndex++;
									NameToIndexMap.Add(Function, Address.FunctionIndex);
								}
							}
						}
					}
				}
			}
			else
			{
				foreach( FCallStackAddress Address in FStreamInfo.GlobalInstance.CallStackAddressArray )
				{
					if( ( AddressIndex % NumAddressesPerTick ) == 0 )
					{
						BGWorker.ReportProgress( AddressIndex / NumAddressesPerTick, ProgressString );
					}
					++AddressIndex;

					// Look up symbol info via console support DLL.
					string Filename = "";
					string Function = "";
					FStreamInfo.GlobalInstance.ConsoleSymbolParser.ResolveAddressToSymboInfo( (uint)Address.ProgramCounter, ref Filename, ref Function, ref Address.LineNumber );

					// Look up filename index.
					if( NameToIndexMap.ContainsKey( Filename ) )
					{
						// Use existing entry.
						Address.FilenameIndex = NameToIndexMap[Filename];
					}
					// Not found, so we use global name index to set new one.
					else
					{
						// Set name in map associated with global ever increasing index.
						Address.FilenameIndex = CurrentNameIndex++;
						NameToIndexMap.Add( Filename, Address.FilenameIndex );
					}

					// Look up function index.
					if( NameToIndexMap.ContainsKey( Function ) )
					{
						// Use existing entry.
						Address.FunctionIndex = NameToIndexMap[Function];
					}
					// Not found, so we use global name index to set new one.
					else
					{
						// Set name in map associated with global ever increasing index.
						Address.FunctionIndex = CurrentNameIndex++;
						NameToIndexMap.Add( Function, Address.FunctionIndex );
					}
				}
			}

			// Create new name array based on dictionary.
			FStreamInfo.GlobalInstance.NameArray = new List<string>(CurrentNameIndex);
			foreach (KeyValuePair<string, int> NameMapping in NameToIndexMap)
			{
                while (FStreamInfo.GlobalInstance.NameArray.Count <= NameMapping.Value)
				{
                    FStreamInfo.GlobalInstance.NameArray.Add(null);
				}

				FStreamInfo.GlobalInstance.NameArray[NameMapping.Value] = NameMapping.Key;
			}

			// Unload symbols again.
			FStreamInfo.GlobalInstance.ConsoleSymbolParser.UnloadSymbols();
		}

        private static int[] SnapshotTypeCounts;

        private static void ResetSnapshotDescriptions()
        {
            SnapshotTypeCounts = new int[(int)EProfilingPayloadSubType.SUBTYPE_Unknown];
        }

        private static string GetNextSnapshotDescription(EProfilingPayloadSubType SubType, string Tag)
        {
            string Result;
            switch (SubType)
            {
                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start:
                    Result = "LoadMap Start";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Mid:
                    Result = "LoadMap Mid";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End:
                    Result = "LoadMap End";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start:
                    Result = "GC Start";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End:
                    Result = "GC End";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_Start:
                    Result = "LevelStream Start";
                    break;

                case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_End:
                    Result = "LevelStream End";
                    break;

                default:
                    Result = "Snapshot";
                    break;
            }

            Result += " " + SnapshotTypeCounts[(int)SubType];

            if (!String.IsNullOrEmpty(Tag))
            {
                Result += ": " + Tag;
            }

            SnapshotTypeCounts[(int)SubType]++;

            return Result;
        }

		/// <summary> Reads string from stream. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
        public static string ReadString(BinaryReader BinaryStream)
        {
			uint Length = BinaryStream.ReadUInt32();
			return new string( BinaryStream.ReadChars( ( int )Length ) );
        }
	};
}
