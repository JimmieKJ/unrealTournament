// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace APIDocTool
{
	public class APIHierarchyNode
	{
		public string Label;
		public string LinkPath;
		public bool bShowButton = false;
		public List<APIHierarchyNode> Children = new List<APIHierarchyNode>();

		public APIHierarchyNode()
		{
		}

		public APIHierarchyNode(string InLabel, bool bInShowButton)
		{
			Label = InLabel;
			bShowButton = bInShowButton;
		}

		public APIHierarchyNode(string InLabel, string InLinkPath, bool bInShowButton)
		{
			Label = InLabel;
			LinkPath = InLinkPath;
			bShowButton = bInShowButton;
		}

		public int CountChildren()
		{
			int Total = 0;
			foreach (APIHierarchyNode Child in Children)
			{
				Total += Child.CountChildren() + 1;
			}
			return Total;
		}

		public APIHierarchyNode AddRoot(APIRecord Record)
		{
			// Try to find the "primary" path to a root object; ignoring interfaces and non-UObject base classes
			List<APIRecord> PrimaryHierarchy = new List<APIRecord>();
			for (APIRecord ParentRecord = Record; ParentRecord != null; )
			{
				// Add the current record
				PrimaryHierarchy.Add(ParentRecord);

				// Try to find a UObject base class
				List<APIRecord> Candidates = new List<APIRecord>();
				foreach (KeyValuePair<XmlNode, APIRecord> BaseRecord in ParentRecord.BaseRecords)
				{
					if (BaseRecord.Key.InnerText.StartsWith("U") || BaseRecord.Key.InnerText.StartsWith("A"))
					{
						Candidates.Add(BaseRecord.Value);
					}
				}

				// If we didn't get one, add everything else
				if (Candidates.Count == 0)
				{
					foreach (KeyValuePair<XmlNode, APIRecord> BaseRecord in ParentRecord.BaseRecords)
					{
						Candidates.Add(BaseRecord.Value);
					}
				}

				// Move to the next record
				ParentRecord = (Candidates.Count == 1) ? Candidates[0] : null;
			}

			// Create the hidden root node of the hierarchy
			APIHierarchyNode NextNode = this;

			// Show the list from root; show a flat list of base classes for the last record
			if (PrimaryHierarchy.Last().BaseRecords.Count > 0)
			{
				foreach (KeyValuePair<XmlNode, APIRecord> BaseRecord in PrimaryHierarchy.Last().BaseRecords)
				{
					if (BaseRecord.Value == null)
					{
						NextNode.Children.Add(new APIHierarchyNode(BaseRecord.Key.InnerText, false));
					}
					else
					{
						NextNode.Children.Add(new APIHierarchyNode(BaseRecord.Value.FullName, BaseRecord.Value.LinkPath, false));
					}
				}
				NextNode = NextNode.Children.Last();
			}

			// Append the rest of the primary hierarchy
			for (int Idx = PrimaryHierarchy.Count - 1; Idx > 0; Idx--)
			{
				APIRecord ParentRecord = PrimaryHierarchy[Idx];
				NextNode.Children.Add(new APIHierarchyNode(ParentRecord.FullName, ParentRecord.LinkPath, false));
				NextNode = NextNode.Children.Last();
			}

			// Add the current record
			NextNode.Children.Add(new APIHierarchyNode(Record.FullName, false));
			return NextNode.Children.Last();
		}

		public void AddChildren(APIRecord Record)
		{
			foreach (APIRecord DerivedRecord in Record.DerivedRecords)
			{
				APIHierarchyNode DerivedNode = new APIHierarchyNode(DerivedRecord.FullName, DerivedRecord.LinkPath, true);
				Children.Add(DerivedNode);
				DerivedNode.AddChildren(DerivedRecord);
			}
		}

		public APIHierarchyNode AddRoot(APIFunction Function)
		{
			APIHierarchyNode RootNode = this;

			// Add a single chain down to the root
			for(; Function.Reimplements.Count == 1; Function = Function.Reimplements[0])
			{
				APIHierarchyNode NextNode = new APIHierarchyNode(Function.Reimplements[0].FullName + Utility.EscapeText("()"), Function.Reimplements[0].LinkPath, false);
				NextNode.Children.Add(RootNode);
				RootNode = NextNode;
			}

			// Add all the base functions
			APIHierarchyNode NewRootNode = new APIHierarchyNode();
			if (Function.Reimplements.Count == 0)
			{
				NewRootNode.Children.Add(RootNode);
			}
			else
			{
				foreach(APIFunction NextFunction in Function.Reimplements)
				{
					NewRootNode.Children.Add(new APIHierarchyNode(NextFunction.FullName + Utility.EscapeText("()"), NextFunction.LinkPath, false));
				}
				NewRootNode.Children.Last().Children.Add(RootNode);
			}
			return NewRootNode;
		}

		public void AddChildren(APIFunction Function)
		{
			foreach (APIFunction ReimpFunction in Function.ReimplementedBy)
			{
				APIHierarchyNode ReimpNode = new APIHierarchyNode(ReimpFunction.FullName + Utility.EscapeText("()"), ReimpFunction.LinkPath, true);
				Children.Add(ReimpNode);
				ReimpNode.AddChildren(ReimpFunction);
			}
		}
	}

	public class APIHierarchy : APIPage
	{
		public string Description;
		public APIHierarchyNode RootNode;

		public APIHierarchy(APIPage InParent, string InName, string InDescription, APIHierarchyNode InRootNode)
			: base(InParent, InName)
		{
			Description = InDescription;
			RootNode = InRootNode;
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				// Write the page header
				Writer.WritePageHeader(Name, PageCrumbs, Description);
				Writer.WriteLine();

				Writer.EnterTag("[REGION:hierarchy]");
				WriteHierarchy(Writer, RootNode, "hrch");
				Writer.LeaveTag("[/REGION]");
			}
		}

		public static APIHierarchy Build(APIPage Parent, IEnumerable<APIRecord> Records)
		{
			// Create a root node
			APIHierarchyNode RootNode = new APIHierarchyNode();

			// Build a set of all the records to include in the hierarchy
			HashSet<APIRecord> KnownRecords = new HashSet<APIRecord>();
			foreach (APIRecord Record in Records)
			{
				KnownRecords.Add(Record);
			}

			// Filter out all the records which don't have another record in the hierarchy
			foreach (APIRecord Record in Records)
			{
				if (!Record.bIsTemplateSpecialization)
				{
					if (Record.BaseRecords == null || !Record.BaseRecords.Exists(y => KnownRecords.Contains(y.Value)))
					{
						APIHierarchyNode NewNode = new APIHierarchyNode(Record.FullName, Record.LinkPath, true);
						NewNode.AddChildren(Record);
						RootNode.Children.Add(NewNode);
					}
				}
			}

			// Return the new page
			return new APIHierarchy(Parent, "Class Hierarchy", "Class Hierarchy", RootNode);
		}

		public override void GatherReferencedPages(List<APIPage> Pages)
		{
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
		}

		public static void WriteHierarchy(UdnWriter Writer, APIHierarchyNode RootNode, string Id)
		{
			Writer.WriteLine("<table class=\"{0}\" cellspacing=\"0\" id=\"{1}\">", RootNode.bShowButton ? "hierarchy-table-collapsed" : "hierarchy-table", Id);
			for(int Idx = 0; Idx < RootNode.Children.Count; Idx++)
			{
				APIHierarchyNode Node = RootNode.Children[Idx];
				string ChildId = String.Format("{0}_{1}", Id, Idx + 1);

				// Write the start of the row
				Writer.WriteLine("<tr>");

				// Write the button or spacer
				Writer.EnterTag("<td class=\"hierarchy-button-cell\">");
				if (Node.Children.Count == 0 || !Node.bShowButton)
				{
					Writer.WriteObject("HierarchySpacer");
				}
				else
				{
					Writer.EnterObject("HierarchyButton");
					Writer.WriteParamLiteral("id", ChildId);
					Writer.LeaveObject();
				}
				Writer.LeaveTag("</td>");

				// Write the label
				Writer.EnterTag("<td class=\"hierarchy-label-cell\">");
				if (Node.LinkPath == null)
				{
					Writer.WriteObject("HierarchyLabel", "name", Node.Label);
				}
				else
				{
					Writer.WriteObject("HierarchyLabelLinked", "name", Node.Label, "link", "[RELATIVE:" + Node.LinkPath + "]");
				}

				// Write the contents row
				if (Node.Children.Count > 0)
				{
					WriteHierarchy(Writer, Node, ChildId);
				}
				Writer.LeaveTag("</td>");

				// Write the end of the row
				Writer.WriteLine("</tr>");
			}
			Writer.WriteLine("</table>");
		}
	}
}
