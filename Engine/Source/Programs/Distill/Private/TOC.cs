// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace Tools.Distill
{
	/// <summary>A container class for the information stored in the TOC about a single file</summary>
	public class TOCEntry
	{
		/// <summary>A cached FileInfo for easy access later</summary>
		[XmlIgnore]
		public FileInfo Info = null;

		/// <summary>The path of the file relative to the root of the branch</summary>
		[XmlAttribute]
		public string Name = "";

		/// <summary>The size of the file in bytes</summary>
		[XmlAttribute]
		public long Size = 0;

		/// <summary>The time the file was last written to in 100ns UTC ticks</summary>
		[XmlAttribute]
		public long LastWriteTimeUTC = DateTime.MinValue.Ticks;

		/// <summary>The forty byte SHA256 checksum in base 64 notation (optional)</summary>
		[XmlAttribute]
		public string Hash = "";

		/// <summary>An empty constructor for XML serialisation support</summary>
		public TOCEntry()
		{
		}

		/// <summary>An Equals override that just checks the name</summary>
		/// <param name="Other">The TOCEntry to compare against</param>
		/// <returns>True if the Name fields in both objects are identical</returns>
		public override bool Equals( object Other )
		{
			TOCEntry TOCOther = ( TOCEntry )Other;
			return Name == TOCOther.Name;
		}

		/// <summary>A GetHashCode override to suppress the warning</summary>
		/// <returns>A hash code</returns>
		public override int GetHashCode()
		{
			return base.GetHashCode();
		}

		/// <summary>A ToString() override to ease debugging</summary>
		/// <returns>Relative path of the file</returns>
		public override string ToString()
		{
			return Name;
		}

		/// <summary>A constructor that extracts all the required information from the FileInfo, and optionally generates a SHA256 hash</summary>
		/// <param name="InInfo">The FileInfo of the file to create a TOCEntry for</param>
		public TOCEntry( FileInfo InInfo )
		{
			if( InInfo.Exists )
			{
				Info = InInfo;
				Name = Info.FullName.Substring( Distill.Options.Source.Length + 1 ).Replace( "\\", "/" );
				Size = Info.Exists ? Info.Length : 0;
				LastWriteTimeUTC = Info.LastWriteTimeUtc.Ticks;

				if( Distill.Options.bChecksum )
				{
					byte[] HashData = Crypto.CreateChecksum( Info );
					Hash = Convert.ToBase64String( HashData );
				}
			}
		}
	}

	/// <summary>A container class for a list of TOCEntries</summary>
	public class TOC
	{
		/// <summary>A list of table of contents entries; one per file</summary>
		[XmlArray]
		public List<TOCEntry> Entries = null;

		/// <summary>An empty constructor for XML serialisation support</summary>
		public TOC()
		{
		}

		/// <summary>A constructor that generates the table of contents entries from a list of FileInfos</summary>
		/// <param name="Files">A list of FileInfo classes containing files to create TOCEntries for</param>
		public TOC( List<FileInfo> Files )
		{
			Entries = new List<TOCEntry>( Files.Select( x => new TOCEntry( x ) ) );
		}

		/// <summary>A compare function that lists the differences between two TOC files</summary>
		/// <param name="Other">The TOC class to compare against</param>
		/// <remarks>The files referenced in the newly created TOC are compared against the files referenced in the old TOC. All files in both TOCs have their SHA256 checksums compared, 
		/// then files that no longer exist are listed, then new files that did not originally exist are listed.</remarks>
		public void Compare( TOC Other )
		{
			// Work out which files have different SHA256 hash values
			List<string> HashDifferences =
			(
				from Entry in Entries
				from OtherEntry in Other.Entries
				where Entry.Name == OtherEntry.Name
				where Entry.Hash != OtherEntry.Hash 
				select Entry.Name
			).ToList();

			if( HashDifferences.Count == 0 )
			{
				Distill.Log( "No files with mismatched hash values", ConsoleColor.Green );
			}
			else
			{
				Distill.Log( HashDifferences.Count + " file(s) with mismatched hash values", ConsoleColor.Red );
				HashDifferences.ForEach( x => Distill.Log( " ... mismatched hash: " + x.ToString(), ConsoleColor.Red ) );
			}

			// Work out which files do not currently exist
			List<string> MissingFiles = 
			(
				from Entry in Entries
				where !Other.Entries.Contains( Entry )
				select Entry.Name
			).ToList();

			if( MissingFiles.Count == 0 )
			{
				Distill.Log( "No missing files", ConsoleColor.Green );
			}
			else
			{
				Distill.Log( MissingFiles.Count + " missing file(s)", ConsoleColor.Red );
				MissingFiles.ForEach( x => Distill.Log( " ... missing: " + x.ToString(), ConsoleColor.Red ) );
			}

			// Work out which files have been created 
			List<string> NewFiles =
			(
				from Entry in Other.Entries
				where !Entries.Contains( Entry )
				select Entry.Name
			).ToList();

			if( NewFiles.Count == 0 )
			{
				Distill.Log( "No new files", ConsoleColor.Green );
			}
			else
			{
				Distill.Log( NewFiles.Count + " new file(s)", ConsoleColor.Red );
				NewFiles.ForEach( x => Distill.Log( " ... new: " + x.ToString(), ConsoleColor.Red ) );
			}
		}
	}
}
