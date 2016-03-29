using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace FortSync
{
	public class BuildUserMetadata
	{
		public string User;
		public BuildQuality Quality;
		public string Message;

		public static bool TryRead(string FileName, out BuildUserMetadata OutMetadata, TextWriter Log)
		{
			// Try to read the file
			string[] Lines;
			try
			{
				Lines = File.ReadAllLines(FileName);
			}
			catch(Exception Ex)
			{
				Log.WriteException(Ex, "Couldn't read {0}", FileName);
				OutMetadata = null;
				return false;
			}

			// Read all the key-value pairs from the file
			Dictionary<string, string> Pairs = new Dictionary<string,string>();
			foreach(string Line in Lines)
			{
				int EqualsIdx = Line.IndexOf('=');
				if(EqualsIdx != -1)
				{
					Pairs.Add(Line.Substring(0, EqualsIdx).Trim(), Line.Substring(EqualsIdx + 1).Trim());
				}
			}

			// Separate out the strings
			BuildUserMetadata Metadata = new BuildUserMetadata();
			if(Pairs.TryGetValue("User", out Metadata.User) && Pairs.TryGetValue("Message", out Metadata.Message))
			{
				string Quality;
				if(Pairs.TryGetValue("Quality", out Quality) && Enum.TryParse<BuildQuality>(Quality, out Metadata.Quality))
				{
					OutMetadata = Metadata;
					return true;
				}
			}

			//
			OutMetadata = null;
			return false;
		}

		public void Write(string FileName, TextWriter Log)
		{
			// Write the file out to a temporary location first, then move it into place as a transaction
			Log.WriteLine("Updating {0}...", FileName);
			try
			{
				string TempFileName = FileName + ".tmp";
				using(StreamWriter Writer = new StreamWriter(TempFileName))
				{
					Writer.WriteLine("User={0}", User);
					Writer.WriteLine("Quality={0}", Quality);
					Writer.WriteLine("Message={0}", Message);
				}
				File.Delete(FileName);
				File.Move(TempFileName, FileName);
			}
			catch(Exception Ex)
			{
				Log.WriteLine("Failed to write {0}.", FileName);
				foreach(string Line in Ex.ToString().Split('\n'))
				{
					Log.WriteLine(Line);
				}
			}
		}
	}

	public class BuildMetadataService
	{
		string RootFolder;
		Dictionary<int, List<BuildUserMetadata>> Builds = new Dictionary<int,List<BuildUserMetadata>>();
		Dictionary<string, Tuple<DateTime, BuildUserMetadata>> CachedMetadata = new Dictionary<string,Tuple<DateTime,BuildUserMetadata>>();

		public BuildMetadataService(string InRootFolder)
		{
			RootFolder = InRootFolder;
		}

		public void Update(IEnumerable<int> Changelists, TextWriter Log)
		{
			int MinChangelist = Changelists.Min();
			int MaxChangelist = Changelists.Max();

			Builds = new Dictionary<int,List<BuildUserMetadata>>();
			foreach(DirectoryInfo BaseDirectory in EnumerateMetadataDirectories(RootFolder, MinChangelist, MaxChangelist))
			{
				foreach(FileInfo FileInfo in BaseDirectory.EnumerateFiles("*.txt"))
				{
					int Changelist;
					if(ParseChangelistFromFileName(FileInfo.Name, out Changelist) && Changelist >= MinChangelist && Changelist <= MaxChangelist)
					{
						BuildUserMetadata Metadata;
						if(CacheMetadata(FileInfo, out Metadata, Log))
						{
							List<BuildUserMetadata> UserMetadataList;
							if(!Builds.TryGetValue(Changelist, out UserMetadataList))
							{
								UserMetadataList = new List<BuildUserMetadata>();
								Builds.Add(Changelist, UserMetadataList);
							}
							UserMetadataList.Add(Metadata);
						}
					}
				}
			}
		}

		IEnumerable<DirectoryInfo> EnumerateMetadataDirectories(string MetadataPath, int MinChangelist, int MaxChangelist)
		{
			for(int BaseChangelist = (MinChangelist / 1000) * 1000; BaseChangelist <= MaxChangelist; BaseChangelist += 1000)
			{
				DirectoryInfo BaseDirectory = new DirectoryInfo(Path.Combine(MetadataPath, String.Format("{0}xxx", BaseChangelist / 1000)));
				if(BaseDirectory.Exists)
				{
					yield return BaseDirectory;
				}
			}
		}

		bool ParseChangelistFromFileName(string Name, out int Changelist)
		{
			if(!Name.StartsWith("CL-"))
			{
				Changelist = -1;
				return false;
			}

			int UserIdx = Name.IndexOf('-', 3);
			if(UserIdx == -1)
			{
				Changelist = -1;
				return false;
			}

			return int.TryParse(Name.Substring(3, UserIdx - 3), out Changelist);
		}

		bool CacheMetadata(FileInfo File, out BuildUserMetadata OutMetadata, TextWriter Log)
		{
			// Check if it's already in the cache
			Tuple<DateTime, BuildUserMetadata> CacheItem;
			if(CachedMetadata.TryGetValue(File.FullName, out CacheItem) && CacheItem.Item1 == File.LastWriteTimeUtc)
			{
				OutMetadata = CacheItem.Item2;
				return true;
			}

			// Try to read it from disk
			if(BuildUserMetadata.TryRead(File.FullName, out OutMetadata, Log))
			{
				CachedMetadata.Add(File.FullName, new Tuple<DateTime,BuildUserMetadata>(File.LastWriteTimeUtc, OutMetadata));
				return true;
			}

			// Update the cache
			return false;
		}
	}
}
