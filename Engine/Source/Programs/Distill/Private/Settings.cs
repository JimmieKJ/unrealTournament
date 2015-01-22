// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace Tools.Distill
{
	/// <summary>A container class for the details of the platform</summary>
	public class PlatformInfo
	{
		/// <summary>The name of the platform</summary>
		[XmlAttribute]
		public string Name;

		/// <summary>Whether this platform has a case sensitive file system</summary>
		[XmlAttribute]
		[DefaultValue( false )]
		public bool bCaseSensitiveFileSystem;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public PlatformInfo()
		{
			bCaseSensitiveFileSystem = false;
		}

		/// <summary>A debugging override</summary>
		/// <returns>A string of the name of the platform</returns>
		public override string ToString()
		{
			return Name;
		}
	}

	/// <summary>Groups of tags so that a sync can be specified with one name, and do multiple FileGroup tags</summary>
	public class TagSet
	{
		/// <summary>The name of the tagset</summary>
		[XmlAttribute]
		public string Name;

		/// <summary>An array of tags that make up this tagset</summary>
		[XmlArray]
		public string[] Tags;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public TagSet()
		{
		}

		/// <summary>A debugging override</summary>
		/// <returns>A string of the name of the tagset</returns>
		public override string ToString()
		{
			return Name;
		}
	}

	/// <summary>A set of files to include</summary>
	public class FileSet
	{
		/// <summary>Wildcard that specifies some files to sync</summary>
		[XmlAttribute]
		public string Path;

		/// <summary>Should this set of files to a recursive sync</summary>
		[XmlAttribute]
		[DefaultValue( false )]
		public bool bIsRecursive;

		/// <summary>Optional filter to apply to files (array of filenames) to not copy them</summary>
		[XmlArray]
		public string[] FilterOutFiles;

		/// <summary>Optional filter to apply to folders (array of folder names) to not copy them</summary>
		[XmlArray]
		public string[] FilterOutFolders;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public FileSet()
		{
			bIsRecursive = false;
		}

		/// <summary>A debugging override</summary>
		/// <returns>A string of the fileset path</returns>
		public override string ToString()
		{
			return Path;
		}
	}

	/// <summary>A group of filesets</summary>
	public class FileGroup
	{
		/// <summary>Platform for this group. Can also be * for any (same as null), or Console for non-PC platforms</summary>
		[XmlAttribute]
		public string[] Platform;

		/// <summary>Tag for this group.</summary>
		[XmlAttribute]
		public string Tag;

		/// <summary>List of file infos for the group</summary>
		[XmlArray]
		public FileSet[] Files;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public FileGroup()
		{
		}
	}

	/// <summary>A container class for all information about which files to copy</summary>
	public class DistillSettings
	{
		/// <summary>List of known languages (populated automatically)</summary>
		[XmlIgnore]
		public string[] KnownLanguages;

		/// <summary>List of known platforms</summary>
		[XmlArray]
		public PlatformInfo[] KnownPlatforms;

		/// <summary>List of TagSet objects</summary>
		[XmlArray]
		public TagSet[] TagSets;

		/// <summary>Set of file groups for syncing</summary>
		[XmlArray]
		public FileGroup[] FileGroups;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public DistillSettings()
		{
		}
	}
}
