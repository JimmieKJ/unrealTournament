// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Windows.Forms.DataVisualization.Charting;
using System.IO;

namespace NetworkProfiler
{
	public partial class MainWindow : Form
	{
		/** Currently selected frame.					*/
		int CurrentFrame = 0;
		/** Current network stream.						*/
		NetworkStream CurrentNetworkStream = null;
		/** Current active actor filter.				*/
		string CurrentActorFilter = "";
		/** Current active property filter.				*/
		string CurrentPropertyFilter = "";
		/** Current active RPC filter.					*/
		string CurrentRPCFilter = "";

		public MainWindow()
		{
			InitializeComponent();

			// Set default state.
			SetDefaultLineView();

			// Force the columns to the way we want them
			SetupColumns( ActorListView,	new String[]	{ "Total Size (KBytes)", "Count", "Average Size (Bytes)", "Average Size (Bits)", "Time (ms)", "Actor Class" });
			SetupColumns( PropertyListView, new String[]	{ "Total Size (KBytes)", "Count", "Average Size (Bytes)", "Average Size (Bits)", "Time (ms)", "Property" });
			SetupColumns( RPCListView,		new String[]	{ "Total Size (KBytes)", "Count", "Average Size (Bytes)", "Average Size (Bits)", "Time (ms)", "RPC" });
		}

		private void SetDefaultLineView()
		{
			// Set default state.
			foreach( var Series in NetworkChart.Series )
			{
				Series.Enabled = false;
			}

			for( int i = 0; i < ChartListBox.Items.Count; ++i )
			{
				ChartListBox.SetItemChecked(i, false);
			}

			NetworkChart.Series["PropertySize"].Enabled = true;
			NetworkChart.Series["PropertySize"].ChartType = SeriesChartType.FastLine;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("PropertySize"), true);

			NetworkChart.Series["RPCSize"].Enabled = true;
			NetworkChart.Series["RPCSize"].ChartType = SeriesChartType.FastLine;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("RPCSize"), true);

			NetworkChart.Series["Events"].Enabled = true;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("Events"), true);

			NetworkChart.Series["ExportBunchSize"].ChartType = SeriesChartType.FastLine;
			NetworkChart.Series["MustBeMappedGuidsSize"].ChartType = SeriesChartType.FastLine;
			NetworkChart.Series["SendBunchHeaderSize"].ChartType = SeriesChartType.FastLine;
			NetworkChart.Series["ContentBlockHeaderSize"].ChartType = SeriesChartType.FastLine;
			NetworkChart.Series["ContentBlockFooterSize"].ChartType = SeriesChartType.FastLine;
			NetworkChart.Series["PropertyHandleSize"].ChartType = SeriesChartType.FastLine;
		}

		private void SetDefaultStackedBunchSizeView()
		{
			foreach( var Series in NetworkChart.Series )
			{
				Series.Enabled = false;
			}

			for( int i = 0; i < ChartListBox.Items.Count; ++i )
			{
				ChartListBox.SetItemChecked(i, false);
			}

			NetworkChart.Series["SendBunchSize"].Enabled = true;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("SendBunchSize"), true);

			NetworkChart.Series["PropertySize"].Enabled = true;
			NetworkChart.Series["PropertySize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("PropertySize"), true);

			NetworkChart.Series["RPCSize"].Enabled = true;
			NetworkChart.Series["RPCSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("RPCSize"), true);

			NetworkChart.Series["ExportBunchSize"].Enabled = true;
			NetworkChart.Series["ExportBunchSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("ExportBunchSize"), true);

			NetworkChart.Series["MustBeMappedGuidsSize"].Enabled = true;
			NetworkChart.Series["MustBeMappedGuidsSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("MustBeMappedGuidsSize"), true);

			NetworkChart.Series["SendBunchHeaderSize"].Enabled = true;
			NetworkChart.Series["SendBunchHeaderSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("SendBunchHeaderSize"), true);

			NetworkChart.Series["ContentBlockHeaderSize"].Enabled = true;
			NetworkChart.Series["ContentBlockHeaderSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("ContentBlockHeaderSize"), true);

			NetworkChart.Series["ContentBlockFooterSize"].Enabled = true;
			NetworkChart.Series["ContentBlockFooterSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("ContentBlockFooterSize"), true);

			NetworkChart.Series["PropertyHandleSize"].Enabled = true;
			NetworkChart.Series["PropertyHandleSize"].ChartType = SeriesChartType.StackedArea;
			ChartListBox.SetItemChecked(NetworkChart.Series.IndexOf("PropertyHandleSize"), true);
		}

		private void SetupColumns(ListView ListView, String[] Headers)
		{
			ListView.Columns.Clear();
			foreach (var Header in Headers)
			{
				AddColumn(ListView, Header);
			}

			ListView.AutoResizeColumns(ColumnHeaderAutoResizeStyle.ColumnContent);
			ListView.AutoResizeColumns(ColumnHeaderAutoResizeStyle.HeaderSize);
		}

		private void AddColumn( ListView ListView, String HeaderText )
		{
			ColumnHeader header = new ColumnHeader();
			header.Text = HeaderText;
			ListView.Columns.Add(header);
		}

		private void ChangeNetworkStream( Stream ParserStream )
		{
			try
			{
				CurrentNetworkStream = StreamParser.Parse(ParserStream);							
				StreamParser.ParseStreamIntoListView(CurrentNetworkStream,CurrentNetworkStream.ActorNameToSummary,ActorListView);
				StreamParser.ParseStreamIntoListView(CurrentNetworkStream,CurrentNetworkStream.PropertyNameToSummary,PropertyListView);
				StreamParser.ParseStreamIntoListView(CurrentNetworkStream,CurrentNetworkStream.RPCNameToSummary,RPCListView);
				ChartParser.ParseStreamIntoChart( CurrentNetworkStream, NetworkChart, CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter );
			}
			catch (System.Exception)
			{
				CurrentNetworkStream = null;
				foreach( var Series in NetworkChart.Series )
				{
					Series.Points.Clear();
				}
			}
		}

		private void OpenButton_Click(object sender, EventArgs e)
		{			
			// Create a file open dialog for selecting the .nprof file.
			OpenFileDialog OpenFileDialog = new OpenFileDialog();
			OpenFileDialog.Title = "Open the profile data file from the game's 'Profiling' folder";
			OpenFileDialog.Filter = "Profiling Data (*.nprof)|*.nprof";
			OpenFileDialog.RestoreDirectory = false;
			OpenFileDialog.SupportMultiDottedExtensions = true;
			// Parse it if user didn't cancel.
			if( OpenFileDialog.ShowDialog() == DialogResult.OK )
			{
				// Create binary reader and file info object from filename.
				using( var ParserStream = File.OpenRead(OpenFileDialog.FileName) )
				{
					ChangeNetworkStream( ParserStream );
				}
			}			
		}

		private void NetworkChart_MouseClick(object sender, MouseEventArgs e)
		{
			// Toggle which axis to select and zoom on right click.
			if( e.Button == MouseButtons.Right )
			{
				bool Temp = NetworkChart.ChartAreas["DefaultChartArea"].CursorX.IsUserEnabled;				 
				NetworkChart.ChartAreas["DefaultChartArea"].CursorX.IsUserEnabled = NetworkChart.ChartAreas["DefaultChartArea"].CursorY.IsUserEnabled;
				NetworkChart.ChartAreas["DefaultChartArea"].CursorY.IsUserEnabled = Temp;

				Temp = NetworkChart.ChartAreas["DefaultChartArea"].CursorX.IsUserSelectionEnabled;				 
				NetworkChart.ChartAreas["DefaultChartArea"].CursorX.IsUserSelectionEnabled = NetworkChart.ChartAreas["DefaultChartArea"].CursorY.IsUserSelectionEnabled;
				NetworkChart.ChartAreas["DefaultChartArea"].CursorY.IsUserSelectionEnabled = Temp;
			}
		}

		private void ChartListBox_SelectedValueChanged(object sender, EventArgs e)
		{			
			for( int i=0; i<ChartListBox.Items.Count; i++ )
			{
				NetworkChart.Series[i].Enabled = ChartListBox.GetItemChecked(i);
			}
			// Reset scale based on visible graphs.
			NetworkChart.ChartAreas["DefaultChartArea"].RecalculateAxesScale();
			NetworkChart.ResetAutoValues();
		}

		/**
		 * Single clicking in the graph will change the summary to be the current frame.
		 */
		private void NetworkChart_CursorPositionChanged(object sender, System.Windows.Forms.DataVisualization.Charting.CursorEventArgs e)
		{
			CurrentFrame = (int) e.NewPosition;

			if( CurrentNetworkStream != null 
			&&	CurrentFrame >= 0 
			&&	CurrentFrame < CurrentNetworkStream.Frames.Count )
			{
				SummaryTextBox.Lines = CurrentNetworkStream.Frames[CurrentFrame].Filter(CurrentActorFilter,CurrentPropertyFilter,CurrentRPCFilter).ToStringArray();
#if true
				DetailTextBox.Lines = CurrentNetworkStream.Frames[CurrentFrame].Filter(CurrentActorFilter,CurrentPropertyFilter,CurrentRPCFilter).ToDetailedStringArray(CurrentActorFilter,CurrentPropertyFilter,CurrentRPCFilter);
#else
				// Show a performance info
				DetailTextBox.Lines = CurrentNetworkStream.Frames[CurrentFrame].Filter( CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter ).ToActorPerformanceString( CurrentNetworkStream, CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter );
#endif
           
				// Populate the new actor performance view
				CurrentNetworkStream.Frames[CurrentFrame].Filter(CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter).ToActorPerformanceView(CurrentNetworkStream, ActorPerformanceView, CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter);
            }
		}

		/**
		 * Selection dragging on the X axis will update the summary to be current selection.
		 */ 
		private void NetworkChart_SelectionRangeChanged(object sender, CursorEventArgs e)
		{
			if ((CurrentNetworkStream == null) || (CurrentNetworkStream.Frames.Count == 0))
			{
				return;
			}

			if( e.Axis.AxisName == AxisName.X )
			{
				int SelectionStart	= Math.Max(
										0,
										(int) NetworkChart.ChartAreas["DefaultChartArea"].AxisX.ScaleView.ViewMinimum);
				int SelectionEnd	= Math.Min(
										CurrentNetworkStream.Frames.Count, 
										(int) NetworkChart.ChartAreas["DefaultChartArea"].AxisX.ScaleView.ViewMaximum);

				// Create a partial network stream with the new selection to get the summary.
				PartialNetworkStream Selection = new PartialNetworkStream(  
														CurrentNetworkStream.Frames.GetRange(SelectionStart,SelectionEnd-SelectionStart), 
														CurrentNetworkStream.NameIndexUnreal,
														1 / 30.0f );
				SummaryTextBox.Lines = Selection.Filter(CurrentActorFilter,CurrentPropertyFilter,CurrentRPCFilter).ToStringArray();
			}
		}

		private void ApplyFiltersButton_Click(object sender, EventArgs e)
		{			
            CurrentActorFilter = ActorFilterTextBox.Text;
			CurrentPropertyFilter = PropertyFilterTextBox.Text;
			CurrentRPCFilter = RPCFilterTextBox.Text;
			ChartParser.ParseStreamIntoChart( CurrentNetworkStream, NetworkChart, CurrentActorFilter, CurrentPropertyFilter, CurrentRPCFilter );
		}


		/**
		 * Comparator used to sort the columns of a list view in a specified order. Internally
		 * falls back to using the string comparator for its sorting if it can't convert text to number.
		 */
		private class ListViewItemComparer : System.Collections.IComparer 
		{
			/** Column to sort by		*/
			public int			SortColumn;
			/** Sort order for column	*/
			public SortOrder	SortOrder;
		
			/**
			 * Constructor
			 * 
			 * @param	InSortColumn	Column to sort by
			 * @param	InSortOrder		Sort order to use (either ascending or descending)	
			 */
			public ListViewItemComparer(int InSortColumn, SortOrder InSortOrder) 
			{
				SortColumn = InSortColumn;
				SortOrder = InSortOrder;
			}
			
			/**
			 * Compare function
			 */
			public int Compare( object A, object B ) 
			{
				string StringA = ((ListViewItem) A).SubItems[SortColumn].Text;
				string StringB = ((ListViewItem) B).SubItems[SortColumn].Text;
				int SortValue = 0;

				// Try converting to number, if that fails fall back to text compare
				double DoubleA = 0;
				double DoubleB = 0;
				if( Double.TryParse( StringA, out DoubleA ) && Double.TryParse( StringB, out DoubleB ) )
				{
					SortValue = Math.Sign( DoubleA - DoubleB );
				}
				else
				{
					SortValue = String.Compare( StringA, StringB );	
				}

				if( SortOrder == SortOrder.Descending )
				{
					return -SortValue;
				}
				else
				{
					return SortValue;
				}
			}
		}

		/**
		 * Helper function for dealing with list view column sorting
		 * 
		 * @param	RequestedSortColumn		Column to sort
		 * @param	ListView				ListView to sort
		 */
		private void HandleListViewSorting( int RequestedSortColumn, ListView ListView )
		{
			if( ListView.ListViewItemSorter != null )
			{
				var Comparer = ListView.ListViewItemSorter as ListViewItemComparer;
				// Change sort order if we're clicking on already sorted column.
				if( Comparer.SortColumn == RequestedSortColumn )
				{
					Comparer.SortOrder = Comparer.SortOrder == SortOrder.Descending ? SortOrder.Ascending : SortOrder.Descending;
				}
				// Sort by requested column, descending by default
				else
				{
					Comparer.SortColumn = RequestedSortColumn;
					Comparer.SortOrder = SortOrder.Descending;
				}
			}
			else
			{
				ListView.ListViewItemSorter = new ListViewItemComparer( RequestedSortColumn, SortOrder.Descending );
			}

			ListView.Sort();
		}


		private void PropertyListView_ColumnClick(object sender, ColumnClickEventArgs e)
		{
			HandleListViewSorting( e.Column, PropertyListView );
		}

		private void RPCListView_ColumnClick(object sender, ColumnClickEventArgs e)
		{
			HandleListViewSorting( e.Column, RPCListView );
		}

		private void ActorListView_ColumnClick(object sender, ColumnClickEventArgs e)
		{
			HandleListViewSorting( e.Column, ActorListView );
		}

		private void LineViewRadioButton_CheckChanged(object sender, EventArgs e)
		{
			if (LineViewRadioButton.Checked)
			{
				SetDefaultLineView();
			}
		}

		private void StackedBunchSizeRadioButton_CheckChanged(object sender, EventArgs e)
		{
			if (StackedBunchSizeRadioButton.Checked)
			{
				SetDefaultStackedBunchSizeView();
			}
		}
	}
}
