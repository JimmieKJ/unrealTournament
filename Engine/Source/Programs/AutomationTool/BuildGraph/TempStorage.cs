using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Serialization;
using UnrealBuildTool;
using AutomationTool;

namespace AutomationTool
{
	/// <summary>
	/// Information about a single file in temp storage
	/// </summary>
	[DebuggerDisplay("{RelativePath}")]
	public class TempStorageFile
	{
		/// <summary>
		/// The path of the file, relative to the engine root. Stored using forward slashes.
		/// </summary>
		[XmlAttribute]
		public string RelativePath;

		/// <summary>
		/// The last modified time of the file, in UTC ticks since the Epoch.
		/// </summary>
		[XmlAttribute]
		public long LastWriteTimeUtcTicks;

		/// <summary>
		/// Length of the file
		/// </summary>
		[XmlAttribute]
		public long Length;

		/// <summary>
		/// Default constructor, for XML serialization.
		/// </summary>
		private TempStorageFile()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="FileInfo">File to be added</param>
		/// <param name="RootDir">Root directory to store paths relative to</param>
		public TempStorageFile(FileInfo FileInfo, DirectoryReference RootDir)
		{
			// Check the file exists and is in the right location
			FileReference File = new FileReference(FileInfo);
			if(!File.IsUnderDirectory(RootDir))
			{
				throw new AutomationException("Attempt to add file to temp storage manifest that is outside the root directory ({0})", File.FullName);
			}
			if(!FileInfo.Exists)
			{
				throw new AutomationException("Attempt to add file to temp storage manifest that does not exist ({0})", File.FullName);
			}

			RelativePath = File.MakeRelativeTo(RootDir).Replace(Path.DirectorySeparatorChar, '/');
			LastWriteTimeUtcTicks = FileInfo.LastWriteTimeUtc.Ticks;
			Length = FileInfo.Length;
		}

		/// <summary>
		/// Compare stored for this file with the one on disk, and output an error if they differ.
		/// </summary>
		/// <param name="RootDir">Root directory for this branch</param>
		/// <returns>True if the files are identical, false otherwise</returns>
		public bool Compare(DirectoryReference RootDir)
		{
			FileReference LocalFile = ToFileReference(RootDir);

			// Get the local file info, and check it exists
			FileInfo Info = new FileInfo(LocalFile.FullName);
			if(!Info.Exists)
			{
				CommandUtils.LogError("Missing file from manifest - {0}", RelativePath);
				return false;
			}

			// Check the size matches
			if(Info.Length != Length)
			{
				CommandUtils.LogError("File size differs from manifest - {0} is {1} bytes, expected {2} bytes", RelativePath, Info.Length, Length);
				return false;
			}

			// Check the timestamp of the file matches. On FAT filesystems writetime has a two seconds resolution (see http://msdn.microsoft.com/en-us/library/windows/desktop/ms724290%28v=vs.85%29.aspx)
			long TimeDifference = Info.LastWriteTimeUtc.Ticks - LastWriteTimeUtcTicks;
			if(TimeDifference < -2 || TimeDifference > +2)
			{
				DateTime ExpectedLocal = new DateTime(LastWriteTimeUtcTicks, DateTimeKind.Utc).ToLocalTime();
				if(RequireMatchingTimestamps())
				{
					CommandUtils.LogError("File date/time mismatch for {0} - was {1}, expected {2}", RelativePath, Info.LastWriteTime, ExpectedLocal);
					return false;
				}
				CommandUtils.Log("File date/time mismatch for {0} - was {1}, expected {2}", RelativePath, Info.LastWriteTime, ExpectedLocal);
			}

			return true;
		}

		/// <summary>
		/// Whether we should compare timestamps for this file. Some build products are harmlessly overwritten as part of the build process, so we flag those here.
		/// </summary>
		/// <returns>True if we should compare the file's timestamp, false otherwise</returns>
		bool RequireMatchingTimestamps()
		{
			return RelativePath.IndexOf("/Binaries/DotNET/", StringComparison.InvariantCultureIgnoreCase) == -1 && RelativePath.IndexOf("/Binaries/Mac/", StringComparison.InvariantCultureIgnoreCase) == -1;
		}

		/// <summary>
		/// Gets a local file reference for this file, given a root directory to base it from.
		/// </summary>
		/// <param name="RootDir">The local root directory</param>
		/// <returns>Reference to the file</returns>
		public FileReference ToFileReference(DirectoryReference RootDir)
		{
			return FileReference.Combine(RootDir, RelativePath.Replace('/', Path.DirectorySeparatorChar));
		}
	}

	/// <summary>
	/// Information about a single file in temp storage
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public class TempStorageZipFile
	{
		/// <summary>
		/// Name of this file, including extension
		/// </summary>
		[XmlAttribute]
		public string Name;

		/// <summary>
		/// Length of the file in bytes
		/// </summary>
		[XmlAttribute]
		public long Length;

		/// <summary>
		/// Default constructor, for XML serialization
		/// </summary>
		private TempStorageZipFile()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Info">FileInfo for the zip file</param>
		public TempStorageZipFile(FileInfo Info)
		{
			Name = Info.Name;
			Length = Info.Length;
		}
	}

	/// <summary>
	/// A manifest storing information about build products for a node's output
	/// </summary>
	public class TempStorageManifest
	{
		/// <summary>
		/// List of output files
		/// </summary>
		[XmlArray]
		[XmlArrayItem("File")]
		public TempStorageFile[] Files;

		/// <summary>
		/// List of compressed archives containing the given files
		/// </summary>
		[XmlArray]
		[XmlArrayItem("ZipFile")]
		public TempStorageZipFile[] ZipFiles;

		/// <summary>
		/// Construct a static Xml serializer to avoid throwing an exception searching for the reflection info at runtime
		/// </summary>
		static XmlSerializer Serializer = XmlSerializer.FromTypes(new Type[]{ typeof(TempStorageManifest) })[0];

		/// <summary>
		/// Construct an empty temp storage manifest
		/// </summary>
		private TempStorageManifest()
		{
		}

		/// <summary>
		/// Creates a manifest from a flat list of files (in many folders) and a BaseFolder from which they are rooted.
		/// </summary>
		/// <param name="InFiles">List of full file paths</param>
		/// <param name="RootDir">Root folder for all the files. All files must be relative to this RootDir.</param>
		public TempStorageManifest(FileInfo[] InFiles, DirectoryReference RootDir)
		{
			Files = InFiles.Select(x => new TempStorageFile(x, RootDir)).ToArray();
		}

		/// <summary>
		/// Gets the total size of the files stored in this manifest
		/// </summary>
		/// <returns>The total size of all files</returns>
		public long GetTotalSize()
		{
			long Result = 0;
			foreach(TempStorageFile File in Files)
			{
				Result += File.Length;
			}
			return Result;
		}

		/// <summary>
		/// Load a manifest from disk
		/// </summary>
		/// <param name="File">File to load</param>
		static public TempStorageManifest Load(FileReference File)
		{
			using(StreamReader Reader = new StreamReader(File.FullName))
			{
				return (TempStorageManifest)Serializer.Deserialize(Reader);
			}
		}

		/// <summary>
		/// Saves a manifest to disk
		/// </summary>
		/// <param name="File">File to save</param>
		public void Save(FileReference File)
		{
			using(StreamWriter Writer = new StreamWriter(File.FullName))
			{
				Serializer.Serialize(Writer, this);
			}
		}
	}

	/// <summary>
	/// Tracks the state of the current build job using the filesystem, allowing jobs to be restarted after a failure or expanded to include larger targets, and 
	/// providing a proxy for different machines executing parts of the build in parallel to transfer build products and share state as part of a build system.
	/// 
	/// If a shared temp storage directory is provided - typically a mounted path on a network share - all build products potentially needed as inputs by another node
	/// are compressed and copied over, along with metadata for them (see TempStorageFile) and flags for build events that have occurred (see TempStorageEvent).
	/// 
	/// The local temp storage directory contains the same information, with the exception of the archived build products. Metadata is still kept to detect modified 
	/// build products between runs. If data is not present in local temp storage, it's retrieved from shared temp storage and cached in local storage.
	/// </summary>
	class TempStorage
	{
		/// <summary>
		/// Root directory for this branch.
		/// </summary>
		DirectoryReference RootDir;

		/// <summary>
		/// The local temp storage directory (typically somewhere under /Engine/Saved directory).
		/// </summary>
		DirectoryReference LocalDir;

		/// <summary>
		/// The shared temp storage directory; typically a network location. May be null.
		/// </summary>
		DirectoryReference SharedDir;

		/// <summary>
		/// Whether to allow writes to shared storage
		/// </summary>
		bool bWriteToSharedStorage;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InRootDir">Root directory for this branch</param>
		/// <param name="InLocalDir">The local temp storage directory.</param>
		/// <param name="InSharedDir">The shared temp storage directory. May be null.</param>
		public TempStorage(DirectoryReference InRootDir, DirectoryReference InLocalDir, DirectoryReference InSharedDir, bool bInWriteToSharedStorage)
		{
			RootDir = InRootDir;
			LocalDir = InLocalDir;
			SharedDir = InSharedDir;
			bWriteToSharedStorage = bInWriteToSharedStorage;
		}

		/// <summary>
		/// Cleans all cached local state. We never remove shared storage.
		/// </summary>
		public void CleanLocal()
		{
			CommandUtils.DeleteDirectoryContents(LocalDir.FullName);
		}

		/// <summary>
		/// Cleans local build products for a given node. Does not modify shared storage.
		/// </summary>
		/// <param name="NodeName">Name of the node</param>
		public void CleanLocalNode(string NodeName)
		{
			DirectoryReference NodeDir = GetDirectoryForNode(LocalDir, NodeName);
			if(NodeDir.Exists())
			{
				CommandUtils.DeleteDirectoryContents(NodeDir.FullName);
				CommandUtils.DeleteDirectory_NoExceptions(NodeDir.FullName);
			}
		}

		/// <summary>
		/// Check whether the given node is complete
		/// </summary>
		/// <param name="NodeName">Name of the node</param>
		/// <returns>True if the node is complete</returns>
		public bool IsComplete(string NodeName)
		{
			// Check if it already exists locally
			FileReference LocalFile = GetCompleteMarkerFile(LocalDir, NodeName);
			if(LocalFile.Exists())
			{
				return true;
			}
			
			// Check if it exists in shared storage
			if(SharedDir != null)
			{
				FileReference SharedFile = GetCompleteMarkerFile(SharedDir, NodeName);
				if(SharedFile.Exists())
				{
					return true;
				}
			}

			// Otherwise we don't have any data
			return false;
		}

		/// <summary>
		/// Mark the given node as complete
		/// </summary>
		/// <param name="NodeName">Name of the node</param>
		public void MarkAsComplete(string NodeName)
		{
			// Create the marker locally
			FileReference LocalFile = GetCompleteMarkerFile(LocalDir, NodeName);
			LocalFile.Directory.CreateDirectory();
			File.OpenWrite(LocalFile.FullName).Close();

			// Create the marker in the shared directory
			if(SharedDir != null && bWriteToSharedStorage)
			{
				FileReference SharedFile = GetCompleteMarkerFile(SharedDir, NodeName);
				SharedFile.Directory.CreateDirectory();
				File.OpenWrite(SharedFile.FullName).Close();
			}
		}

		/// <summary>
		/// Checks the integrity of the give node's local build products.
		/// </summary>
		/// <param name="NodeName">The node to retrieve build products for</param>
		/// <param name="OutputNames">List of output names from this node.</param>
		/// <returns>True if the node is complete and valid, false if not (and typically followed by a call to CleanNode()).</returns>
		public bool CheckLocalIntegrity(string NodeName, string[] OutputNames)
		{
			// If the node is not locally complete, fail immediately.
			FileReference CompleteMarkerFile = GetCompleteMarkerFile(LocalDir, NodeName);
			if(!CompleteMarkerFile.Exists())
			{
				return false;
			}

			// Check that each of the outputs match
			foreach(string OutputName in OutputNames)
			{
				// Check the local manifest exists
				FileReference LocalManifestFile = GetManifestFile(LocalDir, NodeName, OutputName);
				if(!LocalManifestFile.Exists())
				{
					return false;
				}

				// Check the local manifest matches the shared manifest
				if(SharedDir != null)
				{
					// Check the shared manifest exists
					FileReference SharedManifestFile = GetManifestFile(SharedDir, NodeName, OutputName);
					if(!SharedManifestFile.Exists())
					{
						return false;
					}

					// Check the manifests are identical, byte by byte
					byte[] LocalManifestBytes = File.ReadAllBytes(LocalManifestFile.FullName);
					byte[] SharedManifestBytes = File.ReadAllBytes(SharedManifestFile.FullName);
					if(!LocalManifestBytes.SequenceEqual(SharedManifestBytes))
					{
						return false;
					}
				}

				// Read the manifest and check the files
				TempStorageManifest LocalManifest = TempStorageManifest.Load(LocalManifestFile);
				if(LocalManifest.Files.Any(x => !x.Compare(RootDir)))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Saves the given files (that should be rooted at the branch root) to a shared temp storage manifest with the given temp storage node and game.
		/// </summary>
		/// <param name="NodeName">The node which these build products belong to</param>
		/// <param name="OutputName">The output name of the node.</param>
		/// <param name="BuildProducts">Array of build products to be archived</param>
		/// <param name="bPushToRemote">Allow skipping the copying of this manifest to shared storage, because it's not required by any other agent</param>
		/// <returns>The created manifest instance (which has already been saved to disk).</returns>
		public TempStorageManifest Archive(string NodeName, string OutputName, FileReference[] BuildProducts, bool bPushToRemote = true)
		{
			using(TelemetryStopwatch TelemetryStopwatch = new TelemetryStopwatch("StoreToTempStorage"))
			{
				// Create a manifest for the given build products
				FileInfo[] Files = BuildProducts.Select(x => new FileInfo(x.FullName)).ToArray();
				TempStorageManifest Manifest = new TempStorageManifest(Files, RootDir);

				// Create the local directory for this node
				DirectoryReference LocalNodeDir = GetDirectoryForNode(LocalDir, NodeName);
				LocalNodeDir.CreateDirectory();

				// Compress the files and copy to shared storage if necessary
				bool bRemote = SharedDir != null && bPushToRemote && bWriteToSharedStorage;
				if(bRemote)
				{
					// Create the shared directory for this node
					DirectoryReference SharedNodeDir = GetDirectoryForNode(SharedDir, NodeName);
					SharedNodeDir.CreateDirectory();

					// Zip all the build products
					FileInfo[] ZipFiles = ParallelZipFiles(Files, RootDir, SharedNodeDir, LocalNodeDir, OutputName);
					Manifest.ZipFiles = ZipFiles.Select(x => new TempStorageZipFile(x)).ToArray();

					// Save the shared manifest
					FileReference SharedManifestFile = GetManifestFile(SharedDir, NodeName, OutputName);
					CommandUtils.Log("Saving shared manifest to {0}", SharedManifestFile.FullName);
					Manifest.Save(SharedManifestFile);
				}

				// Save the local manifest
				FileReference LocalManifestFile = GetManifestFile(LocalDir, NodeName, OutputName);
				CommandUtils.Log("Saving local manifest to {0}", LocalManifestFile.FullName);
				Manifest.Save(LocalManifestFile);

				// Update the stats
				long ZipFilesTotalSize = (Manifest.ZipFiles == null)? 0 : Manifest.ZipFiles.Sum(x => x.Length);
				TelemetryStopwatch.Finish(string.Format("StoreToTempStorage.{0}.{1}.{2}.{3}.{4}.{5}.{6}", Files.Length, Manifest.GetTotalSize(), ZipFilesTotalSize, bRemote? "Remote" : "Local", 0, 0, OutputName));
				return Manifest;
			}
		}

		/// <summary>
		/// Retrieve an output of the given node. Fetches and decompresses the files from shared storage if necessary, or validates the local files.
		/// </summary>
		/// <param name="NodeName">The node to retrieve build products for</param>
		/// <param name="OutputName">The name of the node's output. May be null.</param>
		/// <returns>Manifest of the files retrieved</returns>
		public TempStorageManifest Retreive(string NodeName, string OutputName)
		{
			using(var TelemetryStopwatch = new TelemetryStopwatch("RetrieveFromTempStorage"))
			{
				// Get the path to the local manifest
				FileReference LocalManifestFile = GetManifestFile(LocalDir, NodeName, OutputName);
				bool bLocal = LocalManifestFile.Exists();

				// Read the manifest, either from local storage or shared storage
				TempStorageManifest Manifest;
				if(bLocal)
				{
					CommandUtils.Log("Reading shared manifest from {0}", LocalManifestFile.FullName);
					Manifest = TempStorageManifest.Load(LocalManifestFile);
				}
				else
				{
					// Check we have shared storage
					if(SharedDir == null)
					{
						throw new AutomationException("Missing local manifest for node - {0}", LocalManifestFile.FullName);
					}

					// Get the shared directory for this node
					FileReference SharedManifestFile = GetManifestFile(SharedDir, NodeName, OutputName);

					// Make sure the manifest exists
					if(!SharedManifestFile.Exists())
					{
						throw new AutomationException("Missing local or shared manifest for node - {0}", SharedManifestFile.FullName);
					}

					// Read the shared manifest
					CommandUtils.Log("Copying shared manifest from {0} to {1}", SharedManifestFile.FullName, LocalManifestFile.FullName);
					Manifest = TempStorageManifest.Load(SharedManifestFile);

					// Unzip all the build products
					DirectoryReference SharedNodeDir = GetDirectoryForNode(SharedDir, NodeName);
					FileInfo[] ZipFiles = Manifest.ZipFiles.Select(x => new FileInfo(FileReference.Combine(SharedNodeDir, x.Name).FullName)).ToArray();
					ParallelUnzipFiles(ZipFiles, RootDir);

					// Fix any Unix permissions/chmod issues, and update the timestamps to match the manifest. Zip files only use local time, and there's no guarantee it matches the local clock.
					foreach(TempStorageFile ManifestFile in Manifest.Files)
					{
						FileReference File = ManifestFile.ToFileReference(RootDir);
						if (Utils.IsRunningOnMono)
						{
							CommandUtils.FixUnixFilePermissions(File.FullName);
						}
						System.IO.File.SetLastWriteTimeUtc(File.FullName, new DateTime(ManifestFile.LastWriteTimeUtcTicks, DateTimeKind.Utc));
					}

					// Save the manifest locally
					LocalManifestFile.Directory.CreateDirectory();
					Manifest.Save(LocalManifestFile);
				}

				// Check all the local files are as expected
				bool bAllMatch = true;
				foreach(TempStorageFile File in Manifest.Files)
				{
					bAllMatch &= File.Compare(RootDir);
				}
				if(!bAllMatch)
				{
					throw new AutomationException("Files have been modified");
				}

				// Update the stats and return
				TelemetryStopwatch.Finish(string.Format("RetrieveFromTempStorage.{0}.{1}.{2}.{3}.{4}.{5}.{6}", Manifest.Files.Length, Manifest.Files.Sum(x => x.Length), bLocal? 0 : Manifest.ZipFiles.Sum(x => x.Length), bLocal? "Local" : "Remote", 0, 0, OutputName));
				return Manifest;
			}
		}

		/// <summary>
		/// Zips a set of files (that must be rooted at the given RootDir) to a set of zip files in the given OutputDir. The files will be prefixed with the given basename.
		/// </summary>
		/// <param name="Files">Fully qualified list of files to zip (must be rooted at RootDir).</param>
		/// <param name="RootDir">Root Directory where all files will be extracted.</param>
		/// <param name="OutputDir">Location to place the set of zip files created.</param>
		/// <param name="StagingDir">Location to create zip files before copying them to the OutputDir. If the OutputDir is on a remote file share, staging may be more efficient. Use null to avoid using a staging copy.</param>
		/// <param name="ZipBaseName">The basename of the set of zip files.</param>
		/// <returns>Some metrics about the zip process.</returns>
		/// <remarks>
		/// This function tries to zip the files in parallel as fast as it can. It makes no guarantees about how many zip files will be created or which files will be in which zip,
		/// but it does try to reasonably balance the file sizes.
		/// </remarks>
		private static FileInfo[] ParallelZipFiles(FileInfo[] InputFiles, DirectoryReference RootDir, DirectoryReference OutputDir, DirectoryReference StagingDir, string ZipBaseName)
		{
			// First get the sizes of all the files. We won't parallelize if there isn't enough data to keep the number of zips down.
			var FilesInfo = InputFiles
				.Select(InputFile => new { File = new FileReference(InputFile), FileSize = InputFile.Length })
				.ToList();

			// Profiling results show that we can zip 100MB quite fast and it is not worth parallelizing that case and creating a bunch of zips that are relatively small.
			const long MinFileSizeToZipInParallel = 1024 * 1024 * 100L;
			var bZipInParallel = FilesInfo.Sum(FileInfo => FileInfo.FileSize) >= MinFileSizeToZipInParallel;

			// order the files in descending order so our threads pick up the biggest ones first.
			// We want to end with the smaller files to more effectively fill in the gaps
			var FilesToZip = new ConcurrentQueue<FileReference>(FilesInfo.OrderByDescending(FileInfo => FileInfo.FileSize).Select(FileInfo => FileInfo.File));

			// We deliberately avoid Parallel.ForEach here because profiles have shown that dynamic partitioning creates
			// too many zip files, and they can be of too varying size, creating uneven work when unzipping later,
			// as ZipFile cannot unzip files in parallel from a single archive.
			// We can safely assume the build system will not be doing more important things at the same time, so we simply use all our logical cores,
			// which has shown to be optimal via profiling, and limits the number of resulting zip files to the number of logical cores.
			// 
			// Sadly, mono implementation of System.IO.Compression is really poor (as of 2015/Aug), causing OOM when parallel zipping a large set of files.
			// However, Ionic is MUCH slower than .NET's native implementation (2x+ slower in our build farm), so we stick to the faster solution on PC.
			// The code duplication in the threadprocs is unfortunate here, and hopefully we can settle on .NET's implementation on both platforms eventually.
			List<Thread> ZipThreads;

			ConcurrentBag<FileInfo> ZipFiles = new ConcurrentBag<FileInfo>();

			DirectoryReference ZipDir = StagingDir ?? OutputDir;
			if (Utils.IsRunningOnMono)
			{
				ZipThreads = (
					from CoreNum in Enumerable.Range(0, bZipInParallel ? Environment.ProcessorCount : 1)
					let ZipFileName = FileReference.Combine(ZipDir, string.Format("{0}{1}.zip", ZipBaseName, bZipInParallel ? "-" + CoreNum.ToString("00") : ""))
					select new Thread(() =>
					{
						// don't create the zip unless we have at least one file to add
						FileReference File;
						if (FilesToZip.TryDequeue(out File))
						{
							// Create one zip per thread using the given basename
							using (var ZipArchive = new Ionic.Zip.ZipFile(ZipFileName.FullName) { CompressionLevel = Ionic.Zlib.CompressionLevel.BestSpeed })
							{

								// pull from the queue until we are out of files.
								do
								{
									// use fastest compression. In our best case we are CPU bound, so this is a good tradeoff,
									// cutting overall time by 2/3 while only modestly increasing the compression ratio (22.7% -> 23.8% for RootEditor PDBs).
									// This is in cases of a super hot cache, so the operation was largely CPU bound.
									ZipArchive.AddFile(File.FullName, CommandUtils.ConvertSeparators(PathSeparator.Slash, File.Directory.MakeRelativeTo(RootDir)));
								} while (FilesToZip.TryDequeue(out File));
								ZipArchive.Save();
							}
							// if we are using a staging dir, copy to the final location and delete the staged copy.
							FileInfo ZipFile = new FileInfo(ZipFileName.FullName);
							if (StagingDir != null)
							{
								FileInfo NewZipFile = ZipFile.CopyTo(CommandUtils.MakeRerootedFilePath(ZipFile.FullName, StagingDir.FullName, OutputDir.FullName));
								ZipFile.Delete();
								ZipFile = NewZipFile;
							}
							ZipFiles.Add(ZipFile);
						}
					})).ToList();
			}
			else
			{
				ZipThreads = (
					from CoreNum in Enumerable.Range(0, bZipInParallel ? Environment.ProcessorCount : 1)
					let ZipFileName = FileReference.Combine(ZipDir, string.Format("{0}{1}.zip", ZipBaseName, bZipInParallel ? "-" + CoreNum.ToString("00") : ""))
					select new Thread(() =>
					{
						// don't create the zip unless we have at least one file to add
						FileReference File;
						if (FilesToZip.TryDequeue(out File))
						{
							// Create one zip per thread using the given basename
							using (var ZipArchive = System.IO.Compression.ZipFile.Open(ZipFileName.FullName, System.IO.Compression.ZipArchiveMode.Create))
							{

								// pull from the queue until we are out of files.
								do
								{
									// use fastest compression. In our best case we are CPU bound, so this is a good tradeoff,
									// cutting overall time by 2/3 while only modestly increasing the compression ratio (22.7% -> 23.8% for RootEditor PDBs).
									// This is in cases of a super hot cache, so the operation was largely CPU bound.
									// Also, sadly, mono appears to have a bug where nothing you can do will properly set the LastWriteTime on the created entry,
									// so we have to ignore timestamps on files extracted from a zip, since it may have been created on a Mac.
									ZipFileExtensions.CreateEntryFromFile(ZipArchive, File.FullName, CommandUtils.ConvertSeparators(PathSeparator.Slash, File.MakeRelativeTo(RootDir)), System.IO.Compression.CompressionLevel.Fastest);
								} while (FilesToZip.TryDequeue(out File));
							}
							// if we are using a staging dir, copy to the final location and delete the staged copy.
							FileInfo ZipFile = new FileInfo(ZipFileName.FullName);
							if (StagingDir != null)
							{
								FileInfo NewZipFile = ZipFile.CopyTo(CommandUtils.MakeRerootedFilePath(ZipFile.FullName, StagingDir.FullName, OutputDir.FullName));
								ZipFile.Delete();
								ZipFile = NewZipFile;
							}
							ZipFiles.Add(ZipFile);
						}
					})).ToList();
			}
			ZipThreads.ForEach(thread => thread.Start());
			ZipThreads.ForEach(thread => thread.Join());
			
			return ZipFiles.OrderBy(x => x.Name).ToArray();
		}

		/// <summary>
		/// Unzips a set of zip files with a given basename in a given folder to a given RootDir.
		/// </summary>
		/// <param name="RootDir">Root Directory where all files will be extracted.</param>
		/// <param name="FolderWithZipFiles">Folder containing the zip files to unzip. None of the zips should have the same file path in them.</param>
		/// <param name="ZipBasename">The basename of the set of zip files to unzip.</param>
		/// <returns>Some metrics about the unzip process.</returns>
		/// <remarks>
		/// The code is expected to be the used as the symmetrical inverse of <see cref="ParallelZipFiles"/>, but could be used independently, as long as the files in the zip do not overlap.
		/// </remarks>
		private static void ParallelUnzipFiles(FileInfo[] ZipFiles, DirectoryReference RootDir)
		{
			// Sadly, mono implemention of System.IO.Compression is really poor (as of 2015/Aug), causing OOM when parallel zipping a large set of files.
			// However, Ionic is MUCH slower than .NET's native implementation (2x+ slower in our build farm), so we stick to the faster solution on PC.
			// The code duplication in the threadprocs is unfortunate here, and hopefully we can settle on .NET's implementation on both platforms eventually.
			if (Utils.IsRunningOnMono)
			{
				Parallel.ForEach(ZipFiles,
					(ZipFile) =>
					{
						using (var ZipArchive = Ionic.Zip.ZipFile.Read(ZipFile.FullName))
						{
							ZipArchive.ExtractAll(RootDir.FullName, Ionic.Zip.ExtractExistingFileAction.OverwriteSilently);
						}
					});
			}
			else
			{
				Parallel.ForEach(ZipFiles,
					(ZipFile) =>
					{
						// unzip the files manually instead of caling ZipFile.ExtractToDirectory() because we need to overwrite readonly files. Because of this, creating the directories is up to us as well.
						using (var ZipArchive = System.IO.Compression.ZipFile.OpenRead(ZipFile.FullName))
						{
							foreach (var Entry in ZipArchive.Entries)
							{
								// Use CommandUtils.CombinePaths to ensure directory separators get converted correctly. On mono on *nix, if the path has backslashes it will not convert it.
								var ExtractedFilename = CommandUtils.CombinePaths(RootDir.FullName, Entry.FullName);
								// Zips can contain empty dirs. Ours usually don't have them, but we should support it.
								if (Path.GetFileName(ExtractedFilename).Length == 0)
								{
									Directory.CreateDirectory(ExtractedFilename);
								}
								else
								{
									// We must delete any existing file, even if it's readonly. .Net does not do this by default.
									if (File.Exists(ExtractedFilename))
									{
										InternalUtils.SafeDeleteFile(ExtractedFilename, true);
									}
									else
									{
										Directory.CreateDirectory(Path.GetDirectoryName(ExtractedFilename));
									}
									Entry.ExtractToFile(ExtractedFilename, true);
								}
							}
						}
					});
			}
		}

		/// <summary>
		/// Gets the directory used to store data for the given node
		/// </summary>
		/// <param name="BaseDir">A local or shared temp storage root directory.</param>
		/// <param name="NodeName">Name of the node</param>
		/// <returns>Directory to contain a node's data</returns>
		static DirectoryReference GetDirectoryForNode(DirectoryReference BaseDir, string NodeName)
		{
			return DirectoryReference.Combine(BaseDir, NodeName);
		}

		/// <summary>
		/// Gets the path to the manifest created for a node's output.
		/// </summary>
		/// <param name="BaseDir">A local or shared temp storage root directory.</param>
		/// <param name="NodeName">Name of the node to get the file for</param>
		/// <param name="OutputName">Name of the output to get the manifest for</param>
		static FileReference GetManifestFile(DirectoryReference BaseDir, string NodeName, string OutputName)
		{
			return FileReference.Combine(BaseDir, NodeName, String.Format("Output-{0}.xml", OutputName));
		}

		/// <summary>
		/// Gets the path to a file created to indicate that a node is complete, under the given base directory.
		/// </summary>
		/// <param name="BaseDir">A local or shared temp storage root directory.</param>
		/// <param name="NodeName">Name of the node to get the file for</param>
		static FileReference GetCompleteMarkerFile(DirectoryReference BaseDir, string NodeName)
		{
			return FileReference.Combine(GetDirectoryForNode(BaseDir, NodeName), "Complete");
		}
	}

	/// <summary>
	/// Automated tests for temp storage
	/// </summary>
	class TempStorageTests : BuildCommand
	{
		/// <summary>
		/// Run the automated tests
		/// </summary>
		public override void ExecuteBuild()
		{
			// Get all the shared directories
			DirectoryReference RootDir = new DirectoryReference(CommandUtils.CmdEnv.LocalRoot);

			DirectoryReference LocalDir = DirectoryReference.Combine(RootDir, "Engine", "Saved", "TestTempStorage-Local");
			CommandUtils.CreateDirectory_NoExceptions(LocalDir.FullName);
			CommandUtils.DeleteDirectoryContents(LocalDir.FullName);

			DirectoryReference SharedDir = DirectoryReference.Combine(RootDir, "Engine", "Saved", "TestTempStorage-Shared");
			CommandUtils.CreateDirectory_NoExceptions(SharedDir.FullName);
			CommandUtils.DeleteDirectoryContents(SharedDir.FullName);

			DirectoryReference WorkingDir = DirectoryReference.Combine(RootDir, "Engine", "Saved", "TestTempStorage-Working");
			CommandUtils.CreateDirectory_NoExceptions(WorkingDir.FullName);
			CommandUtils.DeleteDirectoryContents(WorkingDir.FullName);

			// Create the temp storage object
			TempStorage TempStore = new TempStorage(WorkingDir, LocalDir, SharedDir, true);

			// Create a working directory, and copy some source files into it
			DirectoryReference SourceDir = DirectoryReference.Combine(RootDir, "Engine", "Source", "Runtime");
			if(!CommandUtils.CopyDirectory_NoExceptions(SourceDir.FullName, WorkingDir.FullName, true))
			{
				throw new AutomationException("Couldn't copy {0} to {1}", SourceDir.FullName, WorkingDir.FullName);
			}

			// Save the default output
			Dictionary<FileReference, DateTime> DefaultOutput = SelectFiles(WorkingDir, 'a', 'f');
			TempStore.Archive("TestNode", null, DefaultOutput.Keys.ToArray(), false);

			Dictionary<FileReference, DateTime> NamedOutput = SelectFiles(WorkingDir, 'g', 'i');
			TempStore.Archive("TestNode", "NamedOutput", NamedOutput.Keys.ToArray(), true);
			
			// Check both outputs are still ok
			TempStorageManifest DefaultManifest = TempStore.Retreive("TestNode", null);
			CheckManifest(WorkingDir, DefaultManifest, DefaultOutput);

			TempStorageManifest NamedManifest = TempStore.Retreive("TestNode", "NamedOutput");
			CheckManifest(WorkingDir, NamedManifest, NamedOutput);

			// Delete local temp storage and the working directory and try again
			CommandUtils.Log("Clearing local folders...");
			CommandUtils.DeleteDirectoryContents(WorkingDir.FullName);
			CommandUtils.DeleteDirectoryContents(LocalDir.FullName);

			// First output should fail
			CommandUtils.Log("Checking default manifest is now unavailable...");
			bool bGotManifest = false;
			try
			{
				TempStore.Retreive("TestNode", null);
			}
			catch
			{
				bGotManifest = false;
			}
			if(bGotManifest)
			{
				throw new AutomationException("Did not expect shared temp storage manifest to exist");
			}

			// Second one should be fine
			TempStorageManifest NamedManifestFromShared = TempStore.Retreive("TestNode", "NamedOutput");
			CheckManifest(WorkingDir, NamedManifestFromShared, NamedOutput);
		}

		/// <summary>
		/// Enumerate all the files beginning with a letter within a certain range
		/// </summary>
		/// <param name="SourceDir">The directory to read from</param>
		/// <param name="CharRangeBegin">First character in the range to files to return</param>
		/// <param name="CharRangeEnd">Last character (inclusive) in the range of files to return</param>
		/// <returns>Mapping from filename to timestamp</returns>
		static Dictionary<FileReference, DateTime> SelectFiles(DirectoryReference SourceDir, char CharRangeBegin, char CharRangeEnd)
		{
			Dictionary<FileReference, DateTime> ArchiveFileToTime = new Dictionary<FileReference,DateTime>();
			foreach(FileInfo FileInfo in new DirectoryInfo(SourceDir.FullName).EnumerateFiles("*", SearchOption.AllDirectories))
			{
				char FirstCharacter = Char.ToLower(FileInfo.Name[0]);
				if(FirstCharacter >= CharRangeBegin && FirstCharacter <= CharRangeEnd)
				{
					ArchiveFileToTime.Add(new FileReference(FileInfo), FileInfo.LastWriteTimeUtc);
				}
			}
			return ArchiveFileToTime;
		}

		/// <summary>
		/// Checks that a manifest matches the files on disk
		/// </summary>
		/// <param name="RootDir">Root directory for relative paths in the manifest</param>
		/// <param name="Manifest">Manifest to check</param>
		/// <param name="Files">Mapping of filename to timestamp as expected in the manifest</param>
		static void CheckManifest(DirectoryReference RootDir, TempStorageManifest Manifest, Dictionary<FileReference, DateTime> Files)
		{
			if(Files.Count != Manifest.Files.Length)
			{
				throw new AutomationException("Number of files in manifest does not match");
			}
			foreach(TempStorageFile ManifestFile in Manifest.Files)
			{
				FileReference File = ManifestFile.ToFileReference(RootDir);
				if(!File.Exists())
				{
					throw new AutomationException("File in manifest does not exist");
				}

				DateTime OriginalTime;
				if(!Files.TryGetValue(File, out OriginalTime))
				{
					throw new AutomationException("File in manifest did not exist previously");
				}

				double DiffSeconds = (new FileInfo(File.FullName).LastWriteTimeUtc - OriginalTime).TotalSeconds;
				if(Math.Abs(DiffSeconds) > 2)
				{
					throw new AutomationException("Incorrect timestamp for {0}", ManifestFile.RelativePath);
				}
			}
		}
	}
}
