// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using DoxygenLib;

namespace APIDocTool
{
	class APIFunctionGroup : APIMember
	{
		public readonly APIFunctionType FunctionType;

		public APIFunctionGroup(APIPage InParent, APIFunctionKey InKey, IEnumerable<DoxygenEntity> InEntities) 
			: base(InParent, InKey.Name, Utility.MakeLinkPath(InParent.LinkPath, InKey.GetLinkName()))
		{
			FunctionType = InKey.Type;

			// Build a list of prototypes for each function node, to be used for sorting
			List<DoxygenEntity> Entities = new List<DoxygenEntity>(InEntities.OrderBy(x => x.Node.SelectNodes("param").Count));

			// Create the functions
			int LinkIndex = 1;
			foreach(DoxygenEntity Entity in Entities)
			{
				string NewLinkPath = Utility.MakeLinkPath(LinkPath, String.Format("{0}", LinkIndex));
				APIFunction Function = new APIFunction(this, Entity, InKey, NewLinkPath);
				Children.Add(Function);
				LinkIndex++;
			}
		}

		public APIFunctionGroup(APIPage InParent, APIFunctionKey InKey, IEnumerable<APIFunction> InFunctions)
			: base(InParent, InKey.Name, Utility.MakeLinkPath(InParent.LinkPath, InKey.GetLinkName()))
		{
			FunctionType = InKey.Type;
			Children.AddRange(InFunctions);
		}

		public IEnumerable<APIFunction> Overloads
		{
			get { return Children.OfType<APIFunction>().OrderBy(x => x.LinkPath); }
		}

		public bool IsDeprecated()
		{
			return Name.EndsWith("_DEPRECATED");
		}

		public override void Link()
		{
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
			if (FunctionType != APIFunctionType.UnaryOperator && FunctionType != APIFunctionType.BinaryOperator)
			{
				base.AddToManifest(Manifest);
			}
		}

		public override void AddChildrenToManifest(UdnManifest Manifest)
		{
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader(Name, PageCrumbs, "Overload list");

				Writer.EnterTag("[REGION:members]");
				APIFunction.WriteList(Writer, Overloads, Overloads.First().FunctionType != APIFunctionType.Constructor);
				Writer.LeaveTag("[/REGION]");
			}
		}
	}
}
