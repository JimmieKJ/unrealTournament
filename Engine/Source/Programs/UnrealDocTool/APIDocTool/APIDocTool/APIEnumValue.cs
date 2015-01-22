// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.IO;

namespace APIDocTool
{
    public class APIEnumValue
    {
		public readonly XmlNode Node;
		public readonly string Id;
		public readonly string Name;
		public string BriefDescription;
		public string FullDescription;
		public readonly string Initializer;

        public APIEnumValue(XmlNode InNode) 
        {
			Node = InNode;
			Id = Node.Attributes["id"].Value;
			Name = Node.SelectSingleNode("name").InnerText;

			XmlNode InitializerNode = Node.SelectSingleNode("initializer");
			Initializer = (InitializerNode == null)? null : InitializerNode.InnerText;
		}

		public void Link()
		{
			APIMember.ParseBriefAndFullDescription(Node, out BriefDescription, out FullDescription);
		}

		public UdnListItem GetListItem()
		{
			return new UdnListItem(Name, BriefDescription, null);
		}
	}
}
