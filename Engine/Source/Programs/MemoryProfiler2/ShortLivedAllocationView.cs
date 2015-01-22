/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;

namespace MemoryProfiler2
{
	public static class FShortLivedAllocationView
	{
		private static int[] ShortLivedColumnMapping = new int[] { 3, 4, 2, 0, 1 };

		private static MainWindow OwnerWindow;

		private static int ColumnToSortBy = 3;

		private static bool ColumnSortModeAscending = false;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ClearView()
		{
			OwnerWindow.ShortLivedListView.BeginUpdate();
			OwnerWindow.ShortLivedListView.Items.Clear();
			OwnerWindow.ShortLivedListView.SelectedItems.Clear();
			OwnerWindow.ShortLivedListView.EndUpdate();

			OwnerWindow.ShortLivedCallStackListView.BeginUpdate();
			OwnerWindow.ShortLivedCallStackListView.Items.Clear();
			OwnerWindow.ShortLivedCallStackListView.SelectedItems.Clear();
			OwnerWindow.ShortLivedCallStackListView.EndUpdate();

			ColumnToSortBy = 3;
			ColumnSortModeAscending = false;
			ShortLivedColumnMapping = new int[] { 3, 4, 2, 0, 1 };
		}

		public static void ParseSnapshot( string FilterText )
		{
			if( !FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked )
			{
				return;
			}

			// Progress bar
			long ProgressInterval = FStreamInfo.GlobalInstance.CallStackArray.Count / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;
			OwnerWindow.UpdateStatus( "Updating short lived allocation view for " + OwnerWindow.CurrentFilename );

			OwnerWindow.ShortLivedListView.BeginUpdate();
			OwnerWindow.ShortLivedListView.Items.Clear();

			const int MaxLifetime = 1;
			const int MinAllocations = 100;

			ulong StartStreamIndex = OwnerWindow.GetStartSnapshotStreamIndex();
			ulong EndStreamIndex = OwnerWindow.GetEndSnapshotStreamIndex();

			uint[] AllocationLifetimes = new uint[ MaxLifetime + 1 ];

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FShortLivedAllocationView.ParseSnapshot" ) )
			{
				foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
				{
					// Update progress bar.
					if( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FShortLivedAllocationView.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
					{
						Array.Clear( AllocationLifetimes, 0, AllocationLifetimes.Length );

						int NumAllocations = 0;
						uint UniqueFramesWithAge0Allocs = 0;
						int LastFrameWithAge0Allocs = 0;

						int CurrentRun = 0;
						float CurrentTotalAllocSize = 0;
						int CurrentRunFrame = -1;
						int LongestRun = 0;
						float LongestRunTotalAllocSize = 0;

						int LastEndFrame = 1;
						int LastSnapshot = 0;
						foreach( FAllocationLifecycle Lifecycle in CallStack.CompleteLifecycles )
						{
							// only process allocations that were really freed, not just realloced
							if( Lifecycle.FreeStreamIndex != FStreamInfo.INVALID_STREAM_INDEX
								&& Lifecycle.AllocEvent.StreamIndex > StartStreamIndex && Lifecycle.FreeStreamIndex < EndStreamIndex )
							{
								// CompleteLifecycles are sorted by FreeStreamIndex, so this search pattern ensures
								// that the frames will be found as quickly as possible.
								int EndFrame;
								if( Lifecycle.FreeStreamIndex < FStreamInfo.GlobalInstance.FrameStreamIndices[ LastEndFrame ] )
								{
									EndFrame = LastEndFrame;
								}
								else
								{
									if( Lifecycle.FreeStreamIndex > FStreamInfo.GlobalInstance.SnapshotList[ LastSnapshot ].StreamIndex )
									{
										// lifecycle isn't even in same snapshot, so search by snapshot first (much faster than
										// searching through frames one by one)
										LastSnapshot = OwnerWindow.GetSnapshotIndexFromStreamIndex( LastSnapshot, Lifecycle.FreeStreamIndex );

										if( LastSnapshot == 0 )
										{
											LastEndFrame = 1;
										}
										else
										{
											LastEndFrame = FStreamInfo.GlobalInstance.SnapshotList[ LastSnapshot - 1 ].FrameNumber;
										}
									}

									EndFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( LastEndFrame, Lifecycle.FreeStreamIndex );
								}

								int StartFrame = EndFrame;
								while( FStreamInfo.GlobalInstance.FrameStreamIndices[ StartFrame ] > Lifecycle.AllocEvent.StreamIndex
										   && StartFrame > 0 && EndFrame - StartFrame <= MaxLifetime + 1 )
								{
									StartFrame--;
								}
								StartFrame++;

								int Age = EndFrame - StartFrame;

								if( Age <= MaxLifetime )
								{
									AllocationLifetimes[ Age ]++;
									NumAllocations++;

									if( Age == 0 )
									{
										if( StartFrame != LastFrameWithAge0Allocs )
										{
											UniqueFramesWithAge0Allocs++;
											LastFrameWithAge0Allocs = StartFrame;
										}
									}
									else if( Age == 1 )
									{
										if( StartFrame == CurrentRunFrame )
										{
											CurrentRun++;

											CurrentTotalAllocSize += Lifecycle.PeakSize;

											if( CurrentRun > LongestRun )
											{
												LongestRun = CurrentRun;
												LongestRunTotalAllocSize = CurrentTotalAllocSize;
											}
										}
										else if( StartFrame > CurrentRunFrame )
										{
											CurrentRun = 1;
											CurrentTotalAllocSize = Lifecycle.PeakSize;
										}
										else if( EndFrame == CurrentRunFrame )
										{
											CurrentTotalAllocSize += Lifecycle.PeakSize;

											if( CurrentRun == LongestRun && LongestRunTotalAllocSize < CurrentTotalAllocSize )
											{
												LongestRunTotalAllocSize = CurrentTotalAllocSize;
											}
										}

										CurrentRunFrame = EndFrame;
									}
								}
							}
						}

						if( NumAllocations > MinAllocations )
						{
							float LongestRunAvgAllocSize = LongestRun == 0 ? 0 : LongestRunTotalAllocSize / LongestRun;

							ListViewItem LVItem = new ListViewItem();
							uint[] ColumnValues = new uint[ 5 ];
							ColumnValues[ ShortLivedColumnMapping[ 0 ] ] = AllocationLifetimes[ 0 ];
							ColumnValues[ ShortLivedColumnMapping[ 1 ] ] = UniqueFramesWithAge0Allocs;
							ColumnValues[ ShortLivedColumnMapping[ 2 ] ] = AllocationLifetimes[ 1 ];
							ColumnValues[ ShortLivedColumnMapping[ 3 ] ] = ( uint )LongestRun;
							ColumnValues[ ShortLivedColumnMapping[ 4 ] ] = ( uint )LongestRunAvgAllocSize;

							LVItem.Tag = new FShortLivedCallStackTag( CallStack, ColumnValues );
							LVItem.Text = ColumnValues[ ShortLivedColumnMapping[ 0 ] ].ToString();
							for( int ValueIndex = 1; ValueIndex < ColumnValues.Length; ValueIndex++ )
							{
								LVItem.SubItems.Add( ColumnValues[ ShortLivedColumnMapping[ ValueIndex ] ].ToString() );
							}

							bool bInsertedItem = false;
							for( int ItemIndex = OwnerWindow.ShortLivedListView.Items.Count - 1; ItemIndex >= 0; ItemIndex-- )
							{
								uint[] ItemValues = ( ( FShortLivedCallStackTag )OwnerWindow.ShortLivedListView.Items[ ItemIndex ].Tag ).ColumnValues;
								for( int ValueIndex = 0; ValueIndex < ColumnValues.Length; ValueIndex++ )
								{
									if( ItemValues[ ValueIndex ] > ColumnValues[ ValueIndex ] )
									{
										// found correct index to insert item
										OwnerWindow.ShortLivedListView.Items.Insert( ItemIndex + 1, LVItem );
										bInsertedItem = true;
										break;
									}
									else if( ItemValues[ ValueIndex ] < ColumnValues[ ValueIndex ] )
									{
										break;
									}
									else if( ValueIndex == ColumnValues.Length - 1 )
									{
										// the items being compared are identical, so just insert here
										OwnerWindow.ShortLivedListView.Items.Insert( ItemIndex + 1, LVItem );
										bInsertedItem = true;
									}
								}

								if( bInsertedItem )
								{
									break;
								}
							}

							if( !bInsertedItem )
							{
								// item must be at top of list
								OwnerWindow.ShortLivedListView.Items.Insert( 0, LVItem );
							}
						}
					}
				}
			}

			OwnerWindow.ShortLivedListView.SetSortArrow( 3, false );
			OwnerWindow.ShortLivedListView.EndUpdate();

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		public static void ListColumnClick( ColumnClickEventArgs e )
		{
			ListViewEx ShortLivedListView = OwnerWindow.ShortLivedListView;

			if( ShortLivedListView.Items.Count == 0 )
			{
				return;
			}

			if( e.Column == ColumnToSortBy )
			{
				ColumnSortModeAscending = !ColumnSortModeAscending;
			}
			else
			{
				ColumnToSortBy = e.Column;
				ColumnSortModeAscending = false;
			}	

			int[] NewColumnMapping = new int[ ShortLivedColumnMapping.Length ];
			for( int MappingIndex = 0; MappingIndex < NewColumnMapping.Length; MappingIndex++ )
			{
				NewColumnMapping[ MappingIndex ] = ShortLivedColumnMapping[ MappingIndex ];

				if( ShortLivedColumnMapping[ MappingIndex ] < ShortLivedColumnMapping[ e.Column ] )
				{
					NewColumnMapping[ MappingIndex ]++;
				}
			}
			NewColumnMapping[ e.Column ] = 0;

			// copy items to a temp array because the ListViewItemCollection doesn't have a Sort method
			ListViewItem[] TempItems = new ListViewItem[ ShortLivedListView.Items.Count ];
			uint[] TempValues = new uint[ ShortLivedColumnMapping.Length ];
			for( int MappingIndex = 0; MappingIndex < TempItems.Length; MappingIndex++ )
			{
				FShortLivedCallStackTag Tag = ( FShortLivedCallStackTag )ShortLivedListView.Items[ MappingIndex ].Tag;
				for( int j = 0; j < Tag.ColumnValues.Length; j++ )
				{
					TempValues[ NewColumnMapping[ j ] ] = Tag.ColumnValues[ ShortLivedColumnMapping[ j ] ];
				}

				uint[] OldValues = Tag.ColumnValues;
				Tag.ColumnValues = TempValues;
				// reuse old array for next value swap
				TempValues = OldValues;

				TempItems[ MappingIndex ] = ShortLivedListView.Items[ MappingIndex ];
			}

			Array.Sort<ListViewItem>( TempItems, CompareShortLivedColumns );

			ShortLivedListView.BeginUpdate();
			ShortLivedListView.Items.Clear();
			ShortLivedListView.Items.AddRange( TempItems );
			ShortLivedListView.SetSortArrow( ColumnToSortBy, ColumnSortModeAscending );
			ShortLivedListView.EndUpdate();

			ShortLivedColumnMapping = NewColumnMapping;
		}

		private static int CompareShortLivedColumns( ListViewItem ItemA, ListViewItem ItemB )
		{
			uint[] AValues = ( ( FShortLivedCallStackTag )ItemA.Tag ).ColumnValues;
			uint[] BValues = ( ( FShortLivedCallStackTag )ItemB.Tag ).ColumnValues;

			for( int ValueIndex = 0; ValueIndex < AValues.Length; ValueIndex++ )
			{
				if( AValues[ ValueIndex ] < BValues[ ValueIndex ] )
				{
					return ColumnSortModeAscending ? -1 : 1;
				}
				else if( AValues[ ValueIndex ] > BValues[ ValueIndex ] )
				{
					return ColumnSortModeAscending ? 1 : -1;
				}
			}

			return 0;
		}
	}

	/// <summary> Used as the tag for items in the MainMProfWindow.ShortLivedListView and others. </summary>
	public class FShortLivedCallStackTag
	{
		public FCallStack CallStack;
		public uint[] ColumnValues;

		public FShortLivedCallStackTag( FCallStack InCallStack, uint[] InColumnValues )
		{
			CallStack = InCallStack;
			ColumnValues = InColumnValues;
		}
	}
}
