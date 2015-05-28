// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web;
using System.Xml;
using DoxygenLib;
using System.IO.Compression;

namespace APIDocTool
{
	static class Utility
	{
		static HashSet<string> AllLinkPaths = new HashSet<string>();

		public static void AddUnique<T>(this List<T> Items, T Item)
		{
			if(!Items.Contains(Item))
			{
				Items.Add(Item);
			}
		}

		public static void AddToDictionaryList<T, U>(T Key, U Value, Dictionary<T, List<U>> Dict)
		{
			List<U> ItemList;
			if (!Dict.TryGetValue(Key, out ItemList))
			{
				ItemList = new List<U>();
				Dict.Add(Key, ItemList);
			}
			ItemList.Add(Value);
		}

		public static void MoveToFront<T>(List<T> Items, Predicate<T> Selector)
		{
			int MinIdx = 0;
			for (int Idx = Items.Count - 1; Idx > MinIdx; Idx--)
			{
				if (Selector(Items[Idx]))
				{
					T Item = Items[Idx];
					Items.RemoveAt(Idx);
					Items.Insert(MinIdx, Item);
					MinIdx++;
				}
			}
		}

		public static void SafeCreateDirectory(string Path)
		{
			if (!Directory.Exists(Path))
			{
				Directory.CreateDirectory(Path);
			}
		}

		// Returns false if delete fails
		public static bool SafeDeleteFile(string Path)
		{
			bool bResult = true;
			if (File.Exists(Path))
			{
				FileAttributes Attributes = File.GetAttributes(Path);
				if ((Attributes & FileAttributes.ReadOnly) != 0)
				{
					File.SetAttributes(Path, Attributes & ~FileAttributes.ReadOnly);
				}
				try
				{
					File.Delete(Path);
				}
				catch (IOException Ex)
				{					
					Console.WriteLine("Failed to delete '{0}':\n{1}", Path, Ex);
					bResult = false;
				}
			}
			return bResult;
		}

		// Returns false if delete fails
		public static bool SafeDeleteDirectory(string Dir)
		{
			bool bResult = true;
			if (Directory.Exists(Dir))
			{
				try
				{
					Directory.Delete(Dir, true);
				}
				catch (IOException Ex)
				{
					Console.WriteLine("Failed to delete directory '{0}':\n{1}", Dir, Ex);
					bResult = false;
				}
			}
			return bResult;
		}

		public static void SafeDeleteDirectoryContents(string Dir, bool bIncludeSubDirectories)
		{
			if (Directory.Exists(Dir))
			{
				foreach (string File in Directory.EnumerateFiles(Dir))
				{
					SafeDeleteFile(File);
				}
				if (bIncludeSubDirectories)
				{
					foreach (string SubDir in Directory.EnumerateDirectories(Dir))
					{
						SafeDeleteDirectoryContents(SubDir, true);
//						Directory.Delete(SubDir, true);
					}
				}
			}
		}

		public static void SafeCopyFile(string SrcPath, string DstPath)
		{
			File.Copy(SrcPath, DstPath, true);
			File.SetAttributes(DstPath, File.GetAttributes(DstPath) & ~FileAttributes.ReadOnly); 
		}

		public static void SafeCopyFiles(string SrcDir, string DstDir, string Wildcard, bool bRecursive)
		{
			SafeCopyFiles(new DirectoryInfo(SrcDir), DstDir, Wildcard, bRecursive);
		}

		public static void SafeCopyFiles(DirectoryInfo SrcDirInfo, string DstDir, string Wildcard, bool bRecursive)
		{
			SafeCreateDirectory(DstDir);

			foreach (FileInfo SrcFileInfo in SrcDirInfo.EnumerateFiles(Wildcard, SearchOption.TopDirectoryOnly))
			{
				SafeCopyFile(SrcFileInfo.FullName, Path.Combine(DstDir, SrcFileInfo.Name));
			}

			if (bRecursive)
			{
				foreach (DirectoryInfo SrcSubDirInfo in SrcDirInfo.EnumerateDirectories())
				{
					string DstSubDir = Path.Combine(DstDir, SrcSubDirInfo.Name);
					SafeCopyFiles(SrcSubDirInfo, DstSubDir, Wildcard, true);
				}
			}
		}

		public static void FindRelativeContents(string BaseDir, string Wildcard, bool bRecursive, List<string> FilePaths, List<string> DirectoryPaths)
		{
			string BaseRelativeDir = Path.GetDirectoryName(Wildcard);
			DirectoryInfo DirInfo = new DirectoryInfo(Path.Combine(BaseDir, BaseRelativeDir));
			FindRelativeContents(DirInfo, BaseRelativeDir, Path.GetFileName(Wildcard), bRecursive, FilePaths, DirectoryPaths);
		}

		public static void FindRelativeContents(DirectoryInfo DirInfo, string RelativePath, string Wildcard, bool bRecursive, List<string> FilePaths, List<string> DirectoryPaths)
		{
			if (RelativePath.Length > 0)
			{
				DirectoryPaths.Add(RelativePath);
			}

			foreach (FileInfo FileInfo in DirInfo.EnumerateFiles(Wildcard, SearchOption.TopDirectoryOnly))
			{
				FilePaths.Add(Path.Combine(RelativePath, FileInfo.Name));
			}

			if (bRecursive)
			{
				foreach (DirectoryInfo SubDirInfo in DirInfo.EnumerateDirectories())
				{
					FindRelativeContents(SubDirInfo, Path.Combine(RelativePath, SubDirInfo.Name), Wildcard, bRecursive, FilePaths, DirectoryPaths);
				}
			}
		}

		public static XmlDocument ReadXmlDocument(string Path)
		{
			XmlDocument Document = new XmlDocument();
			Document.Load(Path);
			return Document;
		}

		public static XmlDocument TryReadXmlDocument(string Path)
		{
			try
			{
				return ReadXmlDocument(Path);
			}
			catch(FileNotFoundException)
			{
				Console.WriteLine("Couldn't find file '{0}'", Path);
				return null;
			}
			catch (XmlException Ex)
			{
				Console.WriteLine("Failed to read '{0}':\n{1}", Path, Ex);
				return null;
			}
		}

		public static string MakeLinkPath(string Base, string Name)
		{
			// Get the desired file name
			string DesiredName = SanitizeFilename(Name);
	
			// Decrease the maximum new path length as the path gets longer, to avoid hitting MAX_PATH on Windows.
			int MaxNameLength = 32;
			if (Base.Length + DesiredName.Length > 80) MaxNameLength = 16;
			if (Base.Length + DesiredName.Length > 120) MaxNameLength = 8;

			// Truncate the name if necessary
			if (DesiredName.Length > MaxNameLength)
			{
				DesiredName = DesiredName.Substring(0, MaxNameLength) + "-";
			}

			// Get the combined path
			string DesiredPath = Path.Combine(Base, DesiredName);

			// Uniquify the path
			string UniquePath = DesiredPath;
			for (int Suffix = 1; !AllLinkPaths.Add(UniquePath.ToLowerInvariant()); Suffix++)
			{
				UniquePath = String.Format("{0}_{1}", DesiredPath, Suffix);
			}

			return UniquePath;
		}

		public static string MakePagePath(string OutputDirectory, string LinkPath)
		{
			string PageOutputDirectory = Path.Combine(OutputDirectory, LinkPath);
			if (!Directory.Exists(PageOutputDirectory))
			{
				Directory.CreateDirectory(PageOutputDirectory);
			}

			string PageOutputFile = Path.Combine(PageOutputDirectory, Path.GetFileName(OutputDirectory) + ".INT.udn");
			return PageOutputFile;
		}
	
		public static string SanitizeFilename(string Filename)
		{
			string Result = "";
			for (int Idx = 0; Idx < Filename.Length; Idx++)
			{
				if (Char.IsLetterOrDigit(Filename[Idx]))
				{
					Result += Filename[Idx];
				}
				else if(!Char.IsWhiteSpace(Filename[Idx]))
				{
					Result += "_";
				}
			}
			Result = Result.TrimEnd('_');
			if(Result.Equals("CON", StringComparison.CurrentCultureIgnoreCase))
			{
				Result += "_";
			}
			return Result;
		}

		public static void Add<Key, Value>(this Dictionary<Key, List<Value>> Map, Key InKey, Value InValue)
		{
			List<Value> Values;
			if (!Map.TryGetValue(InKey, out Values))
			{
				Values = new List<Value>();
				Map.Add(InKey, Values);
			}
			Values.Add(InValue);
		}

		public static bool IsNullOrWhitespace(string Text)
		{
			return Text == null || Text.Trim().Length == 0;
		}

		public static string EscapeText(string Text)
		{
			return Markdown.EscapeText(Text);
		}

		public static string GetBriefDescription(string Description)
		{
			// If it's short anyway, just use the full description
			if(Description.Length < 100)
			{
				return Description;
			}

			// Find the first newline, or period that's followed by a space and not within any type of paren
			int Nesting = 0;
			for(int Idx = 0; Idx < Description.Length; Idx++)
			{
				switch(Description[Idx])
				{
					case '[':
					case '(':
					case '{':
						Nesting++;
						break;
					case ']':
					case ')':
					case '}':
						Nesting--;
						break;
					case '\r':
					case '\n':
						if(Nesting == 0)
						{
							return Description.Substring(0, Idx);
						}
						break;
					case '.':
						if(Nesting == 0 && (Idx + 1 == Description.Length || Char.IsWhiteSpace(Description[Idx + 1])))
						{
							return Description.Substring(0, Idx + 1);
						}
						break;
					case ' ':
						if(MatchSubstring(Description, Idx + 1, "eg.") || MatchSubstring(Description, Idx + 1, "ie."))
						{
							Idx += 3;
						}
						else if(MatchSubstring(Description, Idx + 1, "e.g.") || MatchSubstring(Description, Idx + 1, "i.e.") || MatchSubstring(Description, Idx + 1, "wrt."))
						{
							Idx += 4;
						}
						break;
				}
			}

			// Otherwise just chop it
			return Description;
		}

		public static bool MatchSubstring(string Text, int StartIdx, string Substring)
		{
			if(StartIdx + Substring.Length > Text.Length)
			{
				return false;
			}
			for(int Idx = 0; Idx < Substring.Length; Idx++)
			{
				if(Text[StartIdx + Idx] != Substring[Idx]) return false;
			}
			return true;
		}

		public static string ClampStringLength(string Text, int MaxLength)
		{
			if (Text.Length <= MaxLength)
			{
				return Text;
			}

			int Depth = 0;
			int Length = 0;
			int LastBreakIdx = 0;

			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == '[')
				{
					Depth++;
				}
				else if(Text[Idx] == ']')
				{
					Depth--;
				}
				else
				{
					if(Char.IsWhiteSpace(Text[Idx]))
					{
						LastBreakIdx = Idx;
						while (Idx < Text.Length && Char.IsWhiteSpace(Text[Idx])) Idx++;
					}
					if (Depth == 0)
					{
						Length++;
					}
					if (Length >= MaxLength - 3)
					{
						if (LastBreakIdx == 0)
						{
							return Text.Substring(0, Idx) + "...";
						}
						else
						{
							return Text.Substring(0, LastBreakIdx) + "...";
						}
					}
				}
			}

			return Text;
		}

		public static bool MatchPath(string Path, string Filter)
		{
			return MatchSubPath(Path, 0, Filter, 0);
		}

		public static bool MatchPath(string Path, IEnumerable<string> Filters)
		{
			foreach (string Filter in Filters)
			{
				if (MatchPath(Path, Filter)) return true;
			}
			return false;
		}

		private static bool MatchSubPath(string Path, int PathIdx, string Filter, int FilterIdx)
		{
			for (; ; )
			{
				if (PathIdx == Path.Length && FilterIdx == Filter.Length)
				{
					return true;
				}
				else if(FilterIdx == Filter.Length)
				{
					return false;
				}
				else if (Filter[FilterIdx] == '*')
				{
					FilterIdx++;
					for(;;)
					{
						if (MatchSubPath(Path, PathIdx, Filter, FilterIdx)) return true;
						if (PathIdx == Path.Length || Path[PathIdx] == '\\') return false;
						PathIdx++;
					}
				}
				else if (FilterIdx + 3 <= Filter.Length && Filter[FilterIdx] == '.' && Filter[FilterIdx + 1] == '.' && Filter[FilterIdx + 2] == '.')
				{
					FilterIdx += 3;
					for (; ; )
					{
						if (MatchSubPath(Path, PathIdx, Filter, FilterIdx)) return true;
						if (PathIdx == Path.Length) return false;
						PathIdx++;
					}
				}
				else if(PathIdx == Path.Length)
				{
					return false;
				}
				else if (NormalizePathCharacter(Path[PathIdx]) == NormalizePathCharacter(Filter[FilterIdx]))
				{
					PathIdx++;
					FilterIdx++;
				}
				else
				{
					return false;
				}
			}
		}

		private static char NormalizePathCharacter(char C)
		{
			char Result = Char.ToLowerInvariant(C);
			if(Result == '/') Result = '\\';
			return Result;
		}

		public static void CreateTgzFromDir(string ArchivePath, string BaseDir)
		{
			List<string> InputFiles = new List<string>();
			Utility.FindFilesForTar(BaseDir, "", InputFiles, true);
			Utility.CreateTgzFromFiles(ArchivePath, BaseDir, InputFiles);
		}

		public static void CreateTgzFromFiles(string TarPath, string BaseDir, IEnumerable<string> InputFiles)
		{
			using (FileStream TarFileStream = new FileStream(TarPath, FileMode.Create))
			{
				using (GZipStream ZipStream = new GZipStream(TarFileStream, CompressionMode.Compress))
				{
					using (BinaryWriter TarWriter = new BinaryWriter(ZipStream))
					{
						foreach (string InputFile in InputFiles)
						{
							byte[] FileData = File.ReadAllBytes(Path.Combine(BaseDir, InputFile));
							WriteFileToTar(TarWriter, InputFile, FileData);
						}
						TarWriter.Write(new byte[1024]);
					}
				}
			}
		}

		public static void FindFilesForTar(string BaseDir, string BaseTarPath, List<string> Files, bool bRecursive)
		{
			DirectoryInfo Dir = new DirectoryInfo(Path.Combine(BaseDir, BaseTarPath));
			if (Dir.Exists)
			{
				FindFilesForTar(Dir, BaseTarPath, Files, bRecursive);
			}
		}

		public static void FindFilesForTar(DirectoryInfo Dir, string BaseTarPath, List<string> Files, bool bRecursive)
		{
			if (bRecursive)
			{
				foreach (DirectoryInfo ChildDir in Dir.GetDirectories())
				{
					FindFilesForTar(ChildDir, Path.Combine(BaseTarPath, ChildDir.Name), Files, bRecursive);
				}
			}
			foreach (FileInfo FileInfo in Dir.GetFiles())
			{
				Files.Add(Path.Combine(BaseTarPath, FileInfo.Name));
			}
		}

		const int TarFileNameFieldLength = 99;

		public static void WriteFileToTar(BinaryWriter Writer, string FilePath, byte[] FileData)
		{
			string NormalizedFilePath = FilePath.Replace('\\', '/');

			// Generate a long header record if necessary
			if (NormalizedFilePath.Length > TarFileNameFieldLength)
			{
				WriteTarHeader(Writer, "././@LongLink", NormalizedFilePath.Length + 1, 'L');

				byte[] FilePathBytes = new byte[NormalizedFilePath.Length + 1];
				EncodeTarString(FilePathBytes, 0, FilePathBytes.Length, NormalizedFilePath);

				WriteTarBlocks(Writer, FilePathBytes);
			}

			// Now write the normal file
			WriteTarHeader(Writer, NormalizedFilePath, FileData.Length, '0');
			WriteTarBlocks(Writer, FileData);
		}

		static void WriteTarHeader(BinaryWriter Writer, string FileName, int Length, char Type)
		{
			// Write the name
			byte[] Header = new byte[512];

			// Name
			EncodeTarString(Header, 0, TarFileNameFieldLength, FileName);

			// File mode
			EncodeTarValue(Header, 100, 7, 0x81ff);

			// UID
			EncodeTarValue(Header, 108, 7, 0);

			// GID
			EncodeTarValue(Header, 116, 7, 0);

			// File size
			EncodeTarValue(Header, 124, 12, Length);

			// File timestamp
			EncodeTarValue(Header, 136, 12, 0);

			// File type ('0' = Regular file)
			Header[156] = (byte)Type;

			// Generate the the checksum (seed the checksum as if the field was initialized with 8 spaces)
			int Checksum = (byte)' ' * 8;
			for (int Idx = 0; Idx < Header.Length; Idx++) Checksum += Header[Idx];
			EncodeTarValue(Header, 148, 7, Checksum);

			// Write the header
			Writer.Write(Header);
		}

		static void WriteTarBlocks(BinaryWriter Writer, byte[] Data)
		{
			Writer.Write(Data);
			Writer.Write(new byte[(512 - (Data.Length % 512)) % 512]);
		}

		static void EncodeTarString(byte[] Data, int Offset, int Length, string Text)
		{
			// Convert the Text to ASCII
			for (int Idx = 0; Idx < Math.Min(Length, Text.Length); Idx++)
			{
				if (Text[Idx] == 0 || Text[Idx] > 0x7f)
				{
					throw new Exception("Invalid character in TAR string");
				}
				else
				{
					Data[Offset + Idx] = (byte)Text[Idx];
				}
			}
		}

		static void EncodeTarValue(byte[] Data, int Offset, int Length, int Value)
		{
			// Write the value as an octal string followed by a space
			string ValueString = Convert.ToString(Value, 8) + " ";
			while (ValueString.Length < Length) ValueString = " " + ValueString;
			EncodeTarString(Data, Offset, Length, ValueString);
		}
	}
}
