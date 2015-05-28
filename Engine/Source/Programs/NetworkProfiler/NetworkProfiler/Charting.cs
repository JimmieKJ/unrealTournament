// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.



using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms.DataVisualization;
using System.Windows.Forms.DataVisualization.Charting;
using System.Drawing;

// Microsoft Chart Controls Add-on for Microsoft Visual Studio 2008
//
// http://www.microsoft.com/downloads/details.aspx?familyid=1D69CE13-E1E5-4315-825C-F14D33A303E9&displaylang=en

namespace NetworkProfiler
{
	class ChartParser
	{
		public static void ParseStreamIntoChart( NetworkStream NetworkStream, Chart NetworkChart, string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var StartTime = DateTime.UtcNow;

			NetworkChart.BeginInit();

			// Reset existing data.
			foreach( var Series in NetworkChart.Series )
			{
				Series.Points.Clear();
			}
			NetworkChart.ResetAutoValues();
			NetworkChart.Invalidate();

			NetworkChart.ChartAreas[0].AxisX.ScrollBar.IsPositionedInside = false;
			NetworkChart.ChartAreas[0].AxisX.ScrollBar.ButtonStyle = ScrollBarButtonStyles.All;
			NetworkChart.ChartAreas[0].AxisX.ScrollBar.Size = 15;
			NetworkChart.ChartAreas[0].AxisX.ScrollBar.ButtonColor = Color.LightGray;

			NetworkChart.ChartAreas[0].AxisY.ScrollBar.IsPositionedInside = false;
			NetworkChart.ChartAreas[0].AxisY.ScrollBar.ButtonStyle = ScrollBarButtonStyles.All;
			NetworkChart.ChartAreas[0].AxisY.ScrollBar.Size = 15;
			NetworkChart.ChartAreas[0].AxisY.ScrollBar.ButtonColor = Color.LightGray;

			int FrameCounter = 0;
			foreach( PartialNetworkStream RawFrame in NetworkStream.Frames )
			{
				PartialNetworkStream Frame = RawFrame.Filter(ActorFilter,PropertyFilter,RPCFilter);

				if( Frame.EndTime == Frame.StartTime )
				{
					throw new InvalidOperationException();
				}
				float OneOverDeltaTime = 1 / (Frame.EndTime - Frame.StartTime);

				NetworkChart.Series["ActorCount"].Points.AddXY( FrameCounter, Frame.ActorCount );
				NetworkChart.Series["ActorCountSec"].Points.AddXY( FrameCounter, Frame.ActorCount * OneOverDeltaTime );
				NetworkChart.Series["PropertyCount"].Points.AddXY( FrameCounter, Frame.PropertyCount );
				NetworkChart.Series["PropertyCountSec"].Points.AddXY( FrameCounter, Frame.PropertyCount * OneOverDeltaTime );
				NetworkChart.Series["PropertySize"].Points.AddXY( FrameCounter, Frame.ReplicatedSizeBits / 8 );
				NetworkChart.Series["PropertySizeSec"].Points.AddXY( FrameCounter, Frame.ReplicatedSizeBits / 8 * OneOverDeltaTime );
				NetworkChart.Series["RPCCount"].Points.AddXY( FrameCounter, Frame.RPCCount );
				NetworkChart.Series["RPCCountSec"].Points.AddXY( FrameCounter, Frame.RPCCount * OneOverDeltaTime );
				NetworkChart.Series["RPCSize"].Points.AddXY( FrameCounter, Frame.RPCSizeBits / 8 );
				NetworkChart.Series["RPCSizeSec"].Points.AddXY( FrameCounter, Frame.RPCSizeBits / 8 * OneOverDeltaTime );
				NetworkChart.Series["ExportBunchCount"].Points.AddXY( FrameCounter, Frame.ExportBunchCount );
				NetworkChart.Series["ExportBunchSize"].Points.AddXY( FrameCounter, Frame.ExportBunchSizeBits / 8 );
				NetworkChart.Series["MustBeMappedGuidsCount"].Points.AddXY( FrameCounter, Frame.MustBeMappedGuidCount / 8 );
				NetworkChart.Series["MustBeMappedGuidsSize"].Points.AddXY( FrameCounter, Frame.MustBeMappedGuidSizeBits / 8 );
				NetworkChart.Series["SendAckCount"].Points.AddXY( FrameCounter, Frame.SendAckCount );
				NetworkChart.Series["SendAckCountSec"].Points.AddXY( FrameCounter, Frame.SendAckCount * OneOverDeltaTime );
				NetworkChart.Series["SendAckSize"].Points.AddXY( FrameCounter, Frame.SendAckSizeBits / 8 );
				NetworkChart.Series["SendAckSizeSec"].Points.AddXY( FrameCounter, Frame.SendAckSizeBits / 8 * OneOverDeltaTime );
				NetworkChart.Series["ContentBlockHeaderSize"].Points.AddXY( FrameCounter, Frame.ContentBlockHeaderSizeBits / 8 );
				NetworkChart.Series["ContentBlockFooterSize"].Points.AddXY( FrameCounter, Frame.ContentBlockFooterSizeBits / 8 );
				NetworkChart.Series["PropertyHandleSize"].Points.AddXY( FrameCounter, Frame.PropertyHandleSizeBits / 8 );
				NetworkChart.Series["SendBunchCount"].Points.AddXY( FrameCounter, Frame.SendBunchCount );
				NetworkChart.Series["SendBunchCountSec"].Points.AddXY( FrameCounter, Frame.SendBunchCount * OneOverDeltaTime );
				NetworkChart.Series["SendBunchSize"].Points.AddXY( FrameCounter, Frame.SendBunchSizeBits / 8 );
				NetworkChart.Series["SendBunchSizeSec"].Points.AddXY( FrameCounter, Frame.SendBunchSizeBits / 8 * OneOverDeltaTime );
				NetworkChart.Series["SendBunchHeaderSize"].Points.AddXY( FrameCounter, Frame.SendBunchHeaderSizeBits / 8 );
				NetworkChart.Series["GameSocketSendSize"].Points.AddXY( FrameCounter, Frame.UnrealSocketSize );
				NetworkChart.Series["GameSocketSendSizeSec"].Points.AddXY( FrameCounter, Frame.UnrealSocketSize * OneOverDeltaTime );
				NetworkChart.Series["GameSocketSendCount"].Points.AddXY( FrameCounter, Frame.UnrealSocketCount );
				NetworkChart.Series["GameSocketSendCountSec"].Points.AddXY( FrameCounter, Frame.UnrealSocketCount * OneOverDeltaTime );
				NetworkChart.Series["MiscSocketSendSize"].Points.AddXY( FrameCounter, Frame.OtherSocketSize );
				NetworkChart.Series["MiscSocketSendSizeSec"].Points.AddXY( FrameCounter, Frame.OtherSocketSize * OneOverDeltaTime );
				NetworkChart.Series["MiscSocketSendCount"].Points.AddXY( FrameCounter, Frame.OtherSocketCount );
				NetworkChart.Series["MiscSocketSendCountSec"].Points.AddXY( FrameCounter, Frame.OtherSocketCount * OneOverDeltaTime );								
				int OutgoingBandwidth = Frame.UnrealSocketSize + Frame.OtherSocketSize + NetworkStream.PacketOverhead * (Frame.UnrealSocketCount + Frame.OtherSocketCount);
				NetworkChart.Series["OutgoingBandwidthSize"].Points.AddXY( FrameCounter, OutgoingBandwidth );
				NetworkChart.Series["OutgoingBandwidthSizeSec"].Points.AddXY( FrameCounter, OutgoingBandwidth * OneOverDeltaTime );
				NetworkChart.Series["ActorReplicateTimeInMS"].Points.AddXY(FrameCounter, Frame.ActorReplicateTimeInMS);

				if( Frame.NumEvents > 0 )
				{
					NetworkChart.Series["Events"].Points.AddXY( FrameCounter, 0 );
				}

				FrameCounter++;
			}

			NetworkChart.DataManipulator.FinancialFormula(FinancialFormula.MovingAverage,"30","GameSocketSendSizeSec","GameSocketSendSizeAvgSec");			
			NetworkChart.DataManipulator.FinancialFormula(FinancialFormula.MovingAverage,"30","OutgoingBandwidthSizeSec","OutgoingBandwidthSizeAvgSec");						

			NetworkChart.ChartAreas["DefaultChartArea"].RecalculateAxesScale();
			
			NetworkChart.EndInit();

            Console.WriteLine("Adding data to chart took {0} seconds", (DateTime.UtcNow - StartTime).TotalSeconds);
		}
	}
}
