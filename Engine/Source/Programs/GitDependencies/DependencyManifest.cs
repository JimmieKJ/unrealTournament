// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using System.ComponentModel;

namespace GitDependencies
{
	[DebuggerDisplay("{Name}")]
	public class DependencyFile
	{
		[XmlAttribute]
		public string Name;

		[XmlAttribute]
		public string Hash;

		[XmlAttribute]
		[DefaultValue(false)]
		public bool IsExecutable;
	}

	[DebuggerDisplay("{Hash}")]
	public class DependencyBlob
	{
		[XmlAttribute]
		public string Hash;

		[XmlAttribute]
		public long Size;

		[XmlAttribute]
		public string PackHash;

		[XmlAttribute]
		public long PackOffset;
	}

	[DebuggerDisplay("{Hash}")]
	public class DependencyPack
	{
		[XmlAttribute]
		public string Hash;

		[XmlAttribute]
		public long Size;

		[XmlAttribute]
		public long CompressedSize;

		[XmlAttribute]
		public string RemotePath;
	}

	public class DependencyManifest
	{
		[XmlAttribute]
		public string BaseUrl = "http://cdn.unrealengine.com/dependencies";

		[XmlAttribute]
		[DefaultValue(false)]
		public bool IgnoreProxy;

		[XmlArrayItem("File")]
		public DependencyFile[] Files = new DependencyFile[0];

		[XmlArrayItem("Blob")]
		public DependencyBlob[] Blobs = new DependencyBlob[0];

		[XmlArrayItem("Pack")]
		public DependencyPack[] Packs = new DependencyPack[0];
	}
}
