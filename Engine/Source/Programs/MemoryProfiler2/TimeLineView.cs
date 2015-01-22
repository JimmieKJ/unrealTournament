/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Windows.Forms.DataVisualization;
using System.Windows.Forms.DataVisualization.Charting;

namespace MemoryProfiler2
{
	public static class FTimeLineChartView
	{
		/// <summary> Memory allocated calculated by the profiler. </summary>
		static public string AllocatedMemory = "Allocated Memory";

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ClearView()
		{
			for( int SerieIndex = 0; SerieIndex < OwnerWindow.TimeLineChart.Series.Count; SerieIndex++ )
			{
				Series ChartSeries = OwnerWindow.TimeLineChart.Series[ SerieIndex ];
				if( ChartSeries != null )
				{
					ChartSeries.Points.Clear();
				}
			}
		}

		private static void AddSeriesV3( Chart TimeLineChart, string SeriesName, FStreamSnapshot Snapshot, ESliceTypesV3 SliceType, bool bVisible )
		{
			Series ChartSeries = TimeLineChart.Series[ SeriesName ];
			ChartSeries.Enabled = bVisible;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				foreach( FMemorySlice Slice in Snapshot.OverallMemorySlice )
				{
					DataPoint Point = new DataPoint();
					Point.YValues[ 0 ] = Slice.GetSliceInfoV3( SliceType ) / ( 1024.0 * 1024.0 );

					ChartSeries.Points.Add( Point );
				}
			}
		}

		private static void AddSeriesV4( Chart TimeLineChart, string SeriesName, FStreamSnapshot Snapshot, ESliceTypesV4 SliceType, bool bVisible )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = bVisible;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				foreach( FMemorySlice Slice in Snapshot.OverallMemorySlice )
				{
					DataPoint Point = new DataPoint();
					Point.YValues[0] = Slice.GetSliceInfoV4( SliceType ) / ( 1024.0 * 1024.0 );

					ChartSeries.Points.Add( Point );
				}
			}
		}

		private static void AddEmptySeries( Chart TimeLineChart, string SeriesName, int NumPoints )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = false;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				for( int Index = 0; Index < NumPoints; Index++ )
				{
					ChartSeries.Points.Add( 0 );
				}
			}
		}

		private static void AddFakeSeries( Chart TimeLineChart, string SeriesName, int NumPoints, int Value )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = true;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				for( int Index = 0; Index < NumPoints; Index++ )
				{
					ChartSeries.Points.Add( Value );
				}
			}
		}

		public static void ParseSnapshot( Chart TimeLineChart, FStreamSnapshot Snapshot )
		{
			// Progress bar.
			OwnerWindow.UpdateStatus( "Updating time line view for " + OwnerWindow.CurrentFilename );

			TimeLineChart.Annotations.Clear();

			if( FStreamToken.Version >= 4 )
			{
				AddSeriesV4( TimeLineChart, FTimeLineChartView.AllocatedMemory, Snapshot, ESliceTypesV4.OverallAllocatedMemory, true );
				AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.PlatformUsedPhysical, Snapshot, ESliceTypesV4.PlatformUsedPhysical, true );

				AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.MemoryProfilingOverhead, Snapshot, ESliceTypesV4.MemoryProfilingOverhead, true );
				AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedWasteCurrent, Snapshot, ESliceTypesV4.BinnedWasteCurrent, true );
				AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedSlackCurrent, Snapshot, ESliceTypesV4.BinnedSlackCurrent, true );
				AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedUsedCurrent, Snapshot, ESliceTypesV4.BinnedUsedCurrent, true );
				
				AddEmptySeries( TimeLineChart, "Image Size", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "OS Overhead", Snapshot.OverallMemorySlice.Count );

				AddEmptySeries( TimeLineChart, "Virtual Used", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Virtual Slack", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Virtual Waste", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Physical Used", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Physical Slack", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Physical Waste", Snapshot.OverallMemorySlice.Count );

				AddEmptySeries( TimeLineChart, "Host Used", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Host Slack", Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, "Host Waste", Snapshot.OverallMemorySlice.Count );
			}
			else
			{
				bool bIsXbox360 = FStreamInfo.GlobalInstance.Platform == EPlatformType.Xbox360;
				bool bIsPS3 = FStreamInfo.GlobalInstance.Platform == EPlatformType.PS3;

				AddSeriesV3( TimeLineChart, FTimeLineChartView.AllocatedMemory, Snapshot, ESliceTypesV3.OverallAllocatedMemory, true );

				// Total used will be displayed as "Used Physical".
				AddSeriesV3( TimeLineChart, FMemoryAllocationStatsV4.PlatformUsedPhysical, Snapshot, ESliceTypesV3.TotalUsed, true );

				AddSeriesV3( TimeLineChart, "Image Size", Snapshot, ESliceTypesV3.ImageSize, bIsXbox360 || bIsPS3 );
				AddSeriesV3( TimeLineChart, "OS Overhead", Snapshot, ESliceTypesV3.OSOverhead, bIsXbox360 );

				AddSeriesV3( TimeLineChart, "Virtual Used", Snapshot, ESliceTypesV3.CPUUsed, true );
				AddSeriesV3( TimeLineChart, "Virtual Slack", Snapshot, ESliceTypesV3.CPUSlack, true );
				AddSeriesV3( TimeLineChart, "Virtual Waste", Snapshot, ESliceTypesV3.CPUWaste, true );
				AddSeriesV3( TimeLineChart, "Physical Used", Snapshot, ESliceTypesV3.GPUUsed, bIsPS3 || bIsXbox360 );
				AddSeriesV3( TimeLineChart, "Physical Slack", Snapshot, ESliceTypesV3.GPUSlack, bIsXbox360 );
				AddSeriesV3( TimeLineChart, "Physical Waste", Snapshot, ESliceTypesV3.GPUWaste, bIsPS3 || bIsXbox360 );

				AddSeriesV3( TimeLineChart, "Host Used", Snapshot, ESliceTypesV3.HostUsed, bIsPS3 );
				AddSeriesV3( TimeLineChart, "Host Slack", Snapshot, ESliceTypesV3.HostSlack, bIsPS3 );
				AddSeriesV3( TimeLineChart, "Host Waste", Snapshot, ESliceTypesV3.HostWaste, bIsPS3 );

				//AddEmptySeries( TimeLineChart, FMemoryAllocationStatsV4.PlatformUsedPhysical, Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, FMemoryAllocationStatsV4.BinnedWasteCurrent, Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, FMemoryAllocationStatsV4.BinnedSlackCurrent, Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, FMemoryAllocationStatsV4.BinnedUsedCurrent, Snapshot.OverallMemorySlice.Count );
				AddEmptySeries( TimeLineChart, FMemoryAllocationStatsV4.MemoryProfilingOverhead, Snapshot.OverallMemorySlice.Count );
			}	
		}

		public static bool AddCustomSnapshot( Chart TimeLineChart, CursorEventArgs Event )
		{
			if( TimeLineChart.Series[FTimeLineChartView.AllocatedMemory].Points.Count == 0 )
			{
				return false;
			}
			else 
			{
				VerticalLineAnnotation A = new VerticalLineAnnotation();
				A.AxisX = Event.Axis;
				A.AnchorX = ( int )Event.NewSelectionStart + 1;
				A.ToolTip = "Snapshot";
				A.IsInfinitive = true;
				TimeLineChart.Annotations.Add( A );
				return true;
			}
		}
	}
}

