/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Diagnostics;
using System.Drawing;
using System.Text.RegularExpressions;

namespace MemoryProfiler2 
{
	/// <summary> Class parsing snapshot into a tree from a given root node. </summary>
	public static class FCallGraphTreeViewParser
	{
        private static List<int> SelectedNodeIndices = new List<int>();
        private static string SearchText;
        private static bool bMatchCase;
        private static Regex SearchRegex;

		/// <summary> Parent address to use when creating call graph. </summary>
		public static int ParentFunctionIndex;

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ParseSnapshot( TreeView CallGraphTreeView, List<FCallStackAllocationInfo> CallStackList, bool bShouldSortBySize, string FilterText, bool bInvertCallStacks )
		{
			// Progress bar.
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;
			long ProgressInterval = CallStackList.Count / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			
			OwnerWindow.UpdateStatus( "Updating call graph for " + OwnerWindow.CurrentFilename );

			CallGraphTreeView.BeginUpdate();

			// Clear out existing nodes and add two root nodes. One for regular call stacks and one for truncated ones.
			CallGraphTreeView.Nodes.Clear();
			TreeNode RegularNode = new TreeNode( "Full Callstacks" );
			TreeNode TruncatedNode = new TreeNode( "Truncated Callstacks" );
			CallGraphTreeView.Nodes.Add(RegularNode);
			CallGraphTreeView.Nodes.Add(TruncatedNode);

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FCallGraphTreeViewParser.ParseSnapshot" ) )
			{
				// Iterate over all call graph paths and add them to the graph.
				foreach( FCallStackAllocationInfo AllocationInfo in CallStackList )
				{
					// Update progress bar.
					if( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FCallGraphTreeViewParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					// Add this call graph to the tree view.
					FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ];
					// Split the tree into full and truncated callstacks.
					TreeNode RootNode = CallStack.bIsTruncated ? TruncatedNode : RegularNode;
					// Don't bother with callstacks that don't have a contribution.
					if( ( ( AllocationInfo.Count != 0 ) || ( AllocationInfo.Size != 0 ) )
						// Apply filter based on text representation of address.
						&& CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
					{
						// Add call stack to proper part of graph.
						AddCallStackToGraph( RootNode, CallStack, AllocationInfo, ParentFunctionIndex, bInvertCallStacks );
					}
				}
			}

			// Update the node text by prepending memory usage and allocation count.
			UpdateNodeText( RegularNode );
			UpdateNodeText( TruncatedNode );

			// Last but not least, set the node sorter property to sort nodes.
			if( bShouldSortBySize )
			{
				CallGraphTreeView.TreeViewNodeSorter = new FNodeSizeSorter();
			}
			else
			{
				CallGraphTreeView.TreeViewNodeSorter = new FNodeCountSorter();
			}

			CallGraphTreeView.EndUpdate();

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		private static void AddCallStackToGraph( TreeNode RootNode, FCallStack CallStack, FCallStackAllocationInfo AllocationInfo, int ParentFunctionIndex, bool bInvertCallStacks )
		{
			// Used to determine whether it is okay to add address to the graph. An index of -1 means we don't care about parenting.
			bool bAllowNodeUpdate = ParentFunctionIndex == -1;
			// Iterate over each address and add it to the tree view.
			TreeNode CurrentNode = RootNode;
			for( int AdressIndex = 0; AdressIndex < CallStack.AddressIndices.Count; AdressIndex++ )
			{
				int AddressIndex;
				if( bInvertCallStacks )
				{
					AddressIndex = CallStack.AddressIndices[ CallStack.AddressIndices.Count - 1 - AdressIndex ];
				}
				else
				{
					AddressIndex = CallStack.AddressIndices[ AdressIndex ];
				}

				// Filter based on function if wanted. This means we only include callstacks that contain the function
				// and we also reparent the callstack to start at the first occurence of the function.
				if( ParentFunctionIndex != -1 && !bAllowNodeUpdate )
				{
					bAllowNodeUpdate = FStreamInfo.GlobalInstance.CallStackAddressArray[ AddressIndex ].FunctionIndex == ParentFunctionIndex;
				}

				if( bAllowNodeUpdate )
				{
					// Update the node for this address. The return value of the function will be the new parent node.
					CurrentNode = UpdateNodeAndPayload( CurrentNode, AddressIndex, AllocationInfo );
				}
			}
		}

		private static TreeNode UpdateNodeAndPayload( TreeNode ParentNode, int AddressIndex, FCallStackAllocationInfo AllocationInfo )
		{
            FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex];

			// Iterate over existing nodes to see whether there is a match.
			foreach( TreeNode Node in ParentNode.Nodes )
			{
				FNodePayload Payload = (FNodePayload) Node.Tag;
				// If there is a match, update the allocation size and return the current node.
				if( FStreamInfo.GlobalInstance.CallStackAddressArray[Payload.AddressIndex].FunctionIndex == FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex].FunctionIndex )
				{
					Payload.AllocationSize += AllocationInfo.Size;
					Payload.AllocationCount += AllocationInfo.Count;
                    Payload.CallStacks.Add(CallStack);
					// Return current node as parent for next iteration.
					return Node;
				}
			}

			// If we made it here it means that we need to add a new node.
			string NodeName = FStreamInfo.GlobalInstance.NameArray[FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex].FunctionIndex];
			TreeNode NewNode = new TreeNode( NodeName );
			
			// Create payload for the node and associate it.
            FNodePayload NewPayload = new FNodePayload(AddressIndex, AllocationInfo.Size, AllocationInfo.Count, CallStack);
			NewNode.Tag = NewPayload;
			
			// Add to parent node and return new node as subsequent parent.
			ParentNode.Nodes.Add( NewNode );
			return NewNode;
		}

		private static void UpdateNodeText( TreeNode RootNode )
		{
			// Compile a list of all nodes in the graph without recursion.
			List<TreeNode> TreeNodes = new List<TreeNode>();
			TreeNodes.Add(RootNode);
			int NodeIndex = 0;
			while( NodeIndex < TreeNodes.Count )
			{
				foreach( TreeNode Node in TreeNodes[NodeIndex].Nodes )
				{
					TreeNodes.Add( Node );
				}
				NodeIndex++;
			}

			// Iterate over all nodes and prepend size in KByte.
			foreach( TreeNode Node in TreeNodes )
			{
				// Some nodes like root node won't have a tag.
				if( Node.Tag != null )
				{
					FNodePayload Payload = Node.Tag as FNodePayload;
                    string size = MainWindow.FormatSizeString(Payload.AllocationSize);

					Node.Text = size + "  " + Payload.AllocationCount + " Allocations  " + Node.Text;
				}
				// Count down work remaining.
				NodeIndex--;
			}
		}

		public static TreeNode FindNode( TreeView CallGraphTreeView, string InText, bool bTextIsRegex, bool bInMatchCase )
		{
			SetUpSearchVariables( InText, bTextIsRegex, bInMatchCase );

			SelectedNodeIndices.Clear();
			TreeNode Node = CallGraphTreeView.SelectedNode;
			while( Node != null )
			{
				SelectedNodeIndices.Add( Node.Index );
				Node = Node.Parent;
			}

			return FindNodeAux( CallGraphTreeView, CallGraphTreeView.Nodes, SelectedNodeIndices );
		}

		private static TreeNode FindNodeAux( TreeView CallGraphTreeView, TreeNodeCollection TreeNodes, List<int> StartIndices )
		{
			int StartIndex = 0;

			if( StartIndices.Count > 0 )
			{
				StartIndex = StartIndices[ StartIndices.Count - 1 ];
				StartIndices.RemoveAt( StartIndices.Count - 1 );

				TreeNode node = FindNodeAux( CallGraphTreeView, TreeNodes[ StartIndex ].Nodes, StartIndices );
				if( node != null )
				{
					return node;
				}

				if( StartIndices.Count == 0 )
				{
					// StartIndex is the node we wanted to start after, so continue search from StartIndex + 1
					StartIndex++;
				}
			}

			for( int NodeIndex = StartIndex; NodeIndex < TreeNodes.Count; NodeIndex++ )
			{
				TreeNode Node = TreeNodes[ NodeIndex ];
				bool bNodeMatch = false;
				if( SearchRegex != null )
				{
					bNodeMatch = SearchRegex.Match( Node.Text ).Success;
				}
				else
				{
					if( bMatchCase )
					{
						bNodeMatch = Node.Text.IndexOf( SearchText, StringComparison.InvariantCulture ) != -1;
					}
					else
					{
						bNodeMatch = Node.Text.IndexOf( SearchText, StringComparison.InvariantCultureIgnoreCase ) != -1;
					}
				}

				if( bNodeMatch )
				{
					CallGraphTreeView.SelectedNode = Node;
					return Node;
				}

				Node = FindNodeAux( CallGraphTreeView, Node.Nodes, StartIndices );
				if( Node != null )
				{
					return Node;
				}
			}

			return null;
		}

		public static int HighlightAllNodes( TreeView CallGraphTreeView, string InText, bool bTextIsRegex, bool bInMatchCase, out TreeNode FirstResult, out long AllocationSize, out int AllocationCount )
		{
			SetUpSearchVariables( InText, bTextIsRegex, bInMatchCase );

			FNodeSearchState NodeSearchState = new FNodeSearchState();
			FirstResult = null;
			int ResultCount = HighlightAllNodesAux( CallGraphTreeView, CallGraphTreeView.Nodes, NodeSearchState, ref FirstResult );

			if( FirstResult != null )
			{
				FirstResult.EnsureVisible();
			}

			AllocationSize = NodeSearchState.AllocationSize;
			AllocationCount = NodeSearchState.AllocationCount;

			return ResultCount;
		}

		private static int HighlightAllNodesAux( TreeView CallGraphTreeView, TreeNodeCollection TreeNodes, FNodeSearchState SearchState, ref TreeNode FirstResult )
		{
			int ResultCount = 0;

			for( int NodeIndex = 0; NodeIndex < TreeNodes.Count; NodeIndex++ )
			{
				TreeNode N = TreeNodes[ NodeIndex ];
				bool bNodeMatch = false;
				if( SearchRegex != null )
				{
					bNodeMatch = SearchRegex.Match( N.Text ).Success;
				}
				else
				{
					if( bMatchCase )
					{
						bNodeMatch = N.Text.IndexOf( SearchText, StringComparison.InvariantCulture ) != -1;
					}
					else
					{
						bNodeMatch = N.Text.IndexOf( SearchText, StringComparison.InvariantCultureIgnoreCase ) != -1;
					}
				}

				if( bNodeMatch )
				{
					if( FirstResult == null )
					{
						FirstResult = N;
					}

					N.BackColor = Color.CornflowerBlue;
					N.EnsureVisible();
					ResultCount++;

					FNodePayload payload = ( FNodePayload )N.Tag;
					if( !SearchState.CallStacks.Contains( payload.CallStacks[ 0 ] ) )
					{
						// if one callstack from this node is new, then all must be, due to the way the graph is arranged
						// and the order in which it is searched

						SearchState.CallStacks.AddRange( payload.CallStacks );
						SearchState.AllocationCount += payload.AllocationCount;
						SearchState.AllocationSize += payload.AllocationSize;
					}
				}

				ResultCount += HighlightAllNodesAux( CallGraphTreeView, N.Nodes, SearchState, ref FirstResult );
			}

			return ResultCount;
		}

		private static List<TreeNode> Worklist = new List<TreeNode>();

		public static void UnhighlightAll( TreeView CallGraphTreeView )
		{
			Worklist.Clear();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				node.BackColor = Color.Transparent;

				foreach( TreeNode childNode in node.Nodes )
				{
					Worklist.Add( childNode );
				}
			}
		}

		public static List<FCallStack> GetHighlightedCallStacks( TreeView CallGraphTreeView )
		{
			List<FCallStack> Results = new List<FCallStack>();

			Worklist.Clear();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				if( node.BackColor != Color.Transparent )
				{
					FNodePayload payload = ( FNodePayload )node.Tag;
					Results.AddRange( payload.CallStacks );
				}
				else
				{
					// Only search children if this node isn't highlighted.
					// This makes the search go faster, and means that we don't
					// have to worry about duplicate callstacks in the list.
					foreach( TreeNode childNode in node.Nodes )
					{
						Worklist.Add( childNode );
					}
				}
			}

			return Results;
		}

		public static List<string> GetHighlightedNodesAsStrings( TreeView CallGraphTreeView )
		{
			List<string> Results = new List<string>();

			Worklist.Clear();
			foreach( TreeNode node in CallGraphTreeView.Nodes )
			{
				Worklist.Add( node );
			}

			while( Worklist.Count > 0 )
			{
				TreeNode Node = Worklist[ Worklist.Count - 1 ];
				Worklist.RemoveAt( Worklist.Count - 1 );

				if( Node.BackColor != Color.Transparent )
				{
					FNodePayload Payload = ( FNodePayload )Node.Tag;
					if( Payload != null )
					{
						string FunctionName = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ Payload.AddressIndex ].FunctionIndex ];
						int AllocationCount = Payload.AllocationCount;
						long AllocationSize = Payload.AllocationSize;
						Results.Add( FunctionName + "," + AllocationCount + "," + AllocationSize );
					}
				}
				else
				{
					// Only search children if this node isn't highlighted.
					// This makes the search go faster, and means that we don't
					// have to worry about duplicate callstacks in the list.
					foreach( TreeNode childNode in Node.Nodes )
					{
						Worklist.Add( childNode );
					}
				}
			}

			return Results;
		}

		private static void SetUpSearchVariables( string InText, bool bTextIsRegex, bool bInMatchCase )
		{
			bMatchCase = bInMatchCase;
			SearchText = InText;

			SearchRegex = null;
			if( bTextIsRegex )
			{
				RegexOptions options = RegexOptions.Compiled | RegexOptions.CultureInvariant;
				if( !bMatchCase )
				{
					options |= RegexOptions.IgnoreCase;
				}

				try
				{
					SearchRegex = new Regex( SearchText, options );
				}
				catch( ArgumentException e )
				{
					MessageBox.Show( "Invalid regular expression: " + e.Message );
					throw;
				}
			}
		}
    }

	/// <summary> TreeNode payload containing raw node stats about address and allocation size. </summary>
	public class FNodePayload
	{
		/// <summary> Index into stream info addresses array associated with node. </summary>
		public int AddressIndex;

		/// <summary> Cummulative size for this point in the graph. </summary>
		public long AllocationSize;

		/// <summary> Number of allocations at this point in the graph. </summary>
		public int AllocationCount;

		public List<FCallStack> CallStacks = new List<FCallStack>();

		/// <summary> Constructor, initializing all members with passed in values. </summary>
		public FNodePayload( int InAddressIndex, long InAllocationSize, int InAllocationCount, FCallStack CallStack )
		{
			AddressIndex = InAddressIndex;
			AllocationSize = InAllocationSize;
			AllocationCount = InAllocationCount;
			CallStacks.Add( CallStack );
		}
	}

	/// <summary> Node sorter that implements the IComparer interface. Nodes are sorted by size. </summary>
	public class FNodeSizeSorter : System.Collections.IComparer
	{
		public int Compare( object ObjectA, object ObjectB )
		{
			// We sort by size, which requires payload.
			TreeNode NodeA = ObjectA as TreeNode;
			TreeNode NodeB = ObjectB as TreeNode;
			FNodePayload PayloadA = NodeA.Tag as FNodePayload;
			FNodePayload PayloadB = NodeB.Tag as FNodePayload;

			// Can only sort if there is payload.
			if( PayloadA != null && PayloadB != null )
			{
				// Sort by size, descending.
				return Math.Sign( PayloadB.AllocationSize - PayloadA.AllocationSize );
			}
			// Treat missing payload as unsorted
			else
			{
				return 0;
			}
		}
	};

	/// <summary> Node sorter that implements the IComparer interface. Nodes are sorted by count. </summary>
	public class FNodeCountSorter : System.Collections.IComparer
	{
		public int Compare( object ObjectA, object ObjectB )
		{
			// We sort by size, which requires payload.
			TreeNode NodeA = ObjectA as TreeNode;
			TreeNode NodeB = ObjectB as TreeNode;
			FNodePayload PayloadA = NodeA.Tag as FNodePayload;
			FNodePayload PayloadB = NodeB.Tag as FNodePayload;

			// Can only sort if there is payload.
			if( PayloadA != null && PayloadB != null )
			{
				// Sort by size, descending.
				return Math.Sign( PayloadB.AllocationCount - PayloadA.AllocationCount );
			}
			// Treat missing payload as unsorted
			else
			{
				return 0;
			}
		}
	}

    public class FNodeSearchState
    {
        public List<FCallStack> CallStacks = new List<FCallStack>();
        public long AllocationSize;
        public int AllocationCount;

        public FNodeSearchState()
        {
        }
    }
}