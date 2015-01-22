// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Xml.Serialization;

namespace GitDependencies
{
	class Program
	{
		class AsyncDownloadState
		{
			public int NumFiles;
			public int NumFilesRead;
			public long NumBytesRead;
			public string LastDecompressError;
			public int NumFailingDownloads;
			public string LastDownloadError;
		}

		enum OverwriteMode
		{
			Unchanged,
			Prompt,
			Force,
		}

		const string IncomingFileSuffix = ".incoming";
		const string TempManifestExtension = ".tmp";
		
		static int Main(string[] Args)
		{
			// Build the argument list. Remove any double-hyphens from the start of arguments for conformity with other Epic tools.
			List<string> ArgsList = new List<string>();
			foreach (string Arg in Args)
			{
				ArgsList.Add(Arg.StartsWith("--")? Arg.Substring(1) : Arg);
			}

			// Parse the parameters
			int NumThreads = int.Parse(ParseParameter(ArgsList, "-threads=", "4"));
			int MaxRetries = int.Parse(ParseParameter(ArgsList, "-max-retries=", "4"));
			bool bDryRun = ParseSwitch(ArgsList, "-dry-run");
			bool bHelp = ParseSwitch(ArgsList, "-help");
			string RootPath = ParseParameter(ArgsList, "-root=", Path.GetFullPath(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "../../..")));

			// Parse the overwrite mode
			OverwriteMode Overwrite = OverwriteMode.Unchanged;
			if(ParseSwitch(ArgsList, "-prompt"))
			{
				Overwrite = OverwriteMode.Prompt;
			}
			else if(ParseSwitch(ArgsList, "-force"))
			{
				Overwrite = OverwriteMode.Force;
			}

			// Setup network proxy from argument list or environment variable
			string ProxyUrl = ParseParameter(ArgsList, "-proxy=", null);
			string ProxyUsername = ParseParameter(ArgsList, "-proxy-user=", null);
			string ProxyPassword = ParseParameter(ArgsList, "-proxy-password=", null);
			if(String.IsNullOrEmpty(ProxyUrl))
			{
				ProxyUrl = Environment.GetEnvironmentVariable("HTTP_PROXY");
			}

			// Parse all the default exclude filters
			HashSet<string> ExcludeFolders = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);
			if(!ParseSwitch(ArgsList, "-all"))
			{
				if(Environment.OSVersion.Platform != PlatformID.Win32NT)
				{
					ExcludeFolders.Add("Win32");
					ExcludeFolders.Add("Win64");
				}
				if(Environment.OSVersion.Platform != PlatformID.MacOSX && !(Environment.OSVersion.Platform == PlatformID.Unix && Directory.Exists("/Applications") && Directory.Exists("/System")))
				{
					ExcludeFolders.Add("Mac");
				}
				if(Environment.GetEnvironmentVariable("EMSCRIPTEN") == null)
				{
					ExcludeFolders.Add("HTML5");
				}
				if(Environment.GetEnvironmentVariable("NDKROOT") == null)
				{
					ExcludeFolders.Add("Android");
				}
			}

			// Parse all the explicit include filters
			foreach(string IncludeFolder in ParseParameters(ArgsList, "-include="))
			{
				ExcludeFolders.Remove(IncludeFolder.Replace('\\', '/').Trim('/'));
			}

			// Parse all the explicit exclude filters
			foreach(string ExcludeFolder in ParseParameters(ArgsList, "-exclude="))
			{
				ExcludeFolders.Add(ExcludeFolder.Replace('\\', '/').Trim('/'));
			}

			// If there are any more parameters, print an error
			foreach(string RemainingArg in ArgsList)
			{
				Log.WriteLine("Invalid command line parameter: {0}", RemainingArg);
				Log.WriteLine();
				bHelp = true;
			}

			// Print the help message
			if(bHelp)
			{
				Log.WriteLine("Usage:");
				Log.WriteLine("   GitDependencies [options]");
				Log.WriteLine();
				Log.WriteLine("Options:");
				Log.WriteLine("   --all                         Sync all folders");
				Log.WriteLine("   --include=<X>                 Include binaries in folders called <X>");
				Log.WriteLine("   --exclude=<X>                 Exclude binaries in folders called <X>");
				Log.WriteLine("   --prompt                      Prompts for whether to overwrite modified workspace files");
				Log.WriteLine("   --force                       Overwrite modified dependency files in the workspace");
				Log.WriteLine("   --root=<PATH>                 Specifies the path to the directory to sync with");
				Log.WriteLine("   --threads=X                   Use X threads when downloading new files");
				Log.WriteLine("   --dry-run                     Print a list of outdated files, but don't do anything");
				Log.WriteLine("   --max-retries                 Set the maximum number of retries for downloading files");
				Log.WriteLine("   --proxy=<URI>                 Set http proxy URL or address");
				Log.WriteLine("   --proxy-user=<username>       Set proxy username");
				Log.WriteLine("   --proxy-password=<password>   Set proxy password");
				if (ExcludeFolders.Count > 0)
				{
					Log.WriteLine();
					Log.WriteLine("Current excluded folders: {0}", String.Join(", ", ExcludeFolders));
				}
				return 0;
			}

			// Register a delegate to clear the status text if we use ctrl-c to quit
			Console.CancelKeyPress += delegate { Log.FlushStatus(); };

			// Update the tree. Make sure we clear out the status line if we quit for any reason (eg. ctrl-c)
			if(!UpdateWorkingTree(bDryRun, RootPath, ExcludeFolders, NumThreads, MaxRetries, ProxyUrl, ProxyUsername, ProxyPassword, Overwrite))
			{
				return 1;
			}
			return 0;
		}

		static bool ParseSwitch(List<string> ArgsList, string Name)
		{
			for(int Idx = 0; Idx < ArgsList.Count; Idx++)
			{
				if(String.Compare(ArgsList[Idx], Name, true) == 0)
				{
					ArgsList.RemoveAt(Idx);
					return true;
				}
			}
			return false;
		}

		static string ParseParameter(List<string> ArgsList, string Prefix, string Default)
		{
			string Value = Default;
			for(int Idx = 0; Idx < ArgsList.Count; Idx++)
			{
				if(ArgsList[Idx].StartsWith(Prefix, StringComparison.CurrentCultureIgnoreCase))
				{
					Value = ArgsList[Idx].Substring(Prefix.Length);
					ArgsList.RemoveAt(Idx);
					break;
				}
			}
			return Value;
		}

		static IEnumerable<string> ParseParameters(List<string> ArgsList, string Prefix)
		{
			for(;;)
			{
				string Value = ParseParameter(ArgsList, Prefix, null);
				if(Value == null)
				{
					break;
				}
				yield return Value;
			}
		}

		static bool UpdateWorkingTree(bool bDryRun, string RootPath, HashSet<string> ExcludeFolders, int NumThreads, int MaxRetries, string ProxyUrl, string ProxyUsername, string ProxyPassword, OverwriteMode Overwrite)
		{
			// Start scanning on the working directory 
			if(ExcludeFolders.Count > 0)
			{
				Log.WriteLine("Checking dependencies (excluding {0})...", String.Join(", ", ExcludeFolders));
			}
			else
			{
				Log.WriteLine("Checking dependencies...");
			}

			// Figure out the path to the working manifest
			string WorkingManifestPath = Path.Combine(RootPath, ".ue4dependencies");

			// Recover from any interrupted transaction to the working manifest, by moving the temporary file into place.
			string TempWorkingManifestPath = WorkingManifestPath + TempManifestExtension;
			if(File.Exists(TempWorkingManifestPath) && !File.Exists(WorkingManifestPath) && !SafeMoveFile(TempWorkingManifestPath, WorkingManifestPath))
			{
				return false;
			}

			// Read the initial manifest, or create a new one
			WorkingManifest CurrentManifest;
			if(!File.Exists(WorkingManifestPath) || !ReadXmlObject(WorkingManifestPath, out CurrentManifest))
			{
				CurrentManifest = new WorkingManifest();
			}

			// Remove all the in-progress download files left over from previous runs
			foreach(WorkingFile InitialFile in CurrentManifest.Files)
			{
				if(InitialFile.Timestamp == 0)
				{
					string IncomingFilePath = Path.Combine(RootPath, InitialFile.Name + IncomingFileSuffix);
					if(File.Exists(IncomingFilePath) && !SafeDeleteFile(IncomingFilePath))
					{
						return false;
					}
				}
			}

			// Find all the manifests and push them into dictionaries
			Dictionary<string, DependencyFile> TargetFiles = new Dictionary<string,DependencyFile>(StringComparer.InvariantCultureIgnoreCase);
			Dictionary<string, DependencyBlob> TargetBlobs = new Dictionary<string,DependencyBlob>(StringComparer.InvariantCultureIgnoreCase);
			Dictionary<string, DependencyPack> TargetPacks = new Dictionary<string,DependencyPack>(StringComparer.InvariantCultureIgnoreCase);
			foreach(string BaseFolder in Directory.EnumerateDirectories(RootPath))
			{
				string BuildFolder = Path.Combine(BaseFolder, "Build");
				if(Directory.Exists(BuildFolder))
				{
					foreach(string ManifestFileName in Directory.EnumerateFiles(BuildFolder, "*.gitdeps.xml"))
					{
						// Read this manifest
						DependencyManifest NewTargetManifest;
						if(!ReadXmlObject(ManifestFileName, out NewTargetManifest))
						{
							return false;
						}

						// Add all the files, blobs and packs into the shared dictionaries
						foreach(DependencyFile NewFile in NewTargetManifest.Files)
						{
							TargetFiles[NewFile.Name] = NewFile;
						}
						foreach(DependencyBlob NewBlob in NewTargetManifest.Blobs)
						{
							TargetBlobs[NewBlob.Hash] = NewBlob;
						}
						foreach(DependencyPack NewPack in NewTargetManifest.Packs)
						{
							TargetPacks[NewPack.Hash] = NewPack;
						}
					}
				}
			}

			// Find all the existing files in the working directory from previous runs. Use the working manifest to cache hashes for them based on timestamp, but recalculate them as needed.
			Dictionary<string, WorkingFile> CurrentFileLookup = new Dictionary<string, WorkingFile>();
			foreach(WorkingFile CurrentFile in CurrentManifest.Files)
			{
				// Update the hash for this file
				string CurrentFilePath = Path.Combine(RootPath, CurrentFile.Name);
				if(File.Exists(CurrentFilePath))
				{
					long LastWriteTime = File.GetLastWriteTimeUtc(CurrentFilePath).Ticks;
					if(LastWriteTime != CurrentFile.Timestamp)
					{
						CurrentFile.Hash = ComputeHashForFile(CurrentFilePath);
						CurrentFile.Timestamp = LastWriteTime;
					}
					CurrentFileLookup.Add(CurrentFile.Name, CurrentFile);
				}
			}

			// Also add all the untracked files which already exist, but weren't downloaded by this program
			foreach (DependencyFile TargetFile in TargetFiles.Values) 
			{
				if(!CurrentFileLookup.ContainsKey(TargetFile.Name))
				{
					string CurrentFilePath = Path.Combine(RootPath, TargetFile.Name);
					if(File.Exists(CurrentFilePath))
					{
						WorkingFile CurrentFile = new WorkingFile();
						CurrentFile.Name = TargetFile.Name;
						CurrentFile.Hash = ComputeHashForFile(CurrentFilePath);
						CurrentFile.Timestamp = File.GetLastWriteTimeUtc(CurrentFilePath).Ticks;
						CurrentFileLookup.Add(CurrentFile.Name, CurrentFile);
					}
				}
			}

			// Build a list of all the filtered target files
			List<DependencyFile> FilteredTargetFiles = new List<DependencyFile>();
			foreach(DependencyFile TargetFile in TargetFiles.Values)
			{
				if(!IsExcludedFolder(TargetFile.Name, ExcludeFolders))
				{
					FilteredTargetFiles.Add(TargetFile);
				}
			}

			// Create a list of files which need to be updated, and a list of the executable files in the 
			List<DependencyFile> FilesToDownload = new List<DependencyFile>();

			// Create a new working manifest for the working directory, moving over files that we already have. Add any missing dependencies into the download queue.
			WorkingManifest NewWorkingManifest = new WorkingManifest();
			foreach(DependencyFile TargetFile in FilteredTargetFiles)
			{
				WorkingFile NewFile;
				if(CurrentFileLookup.TryGetValue(TargetFile.Name, out NewFile) && NewFile.Hash == TargetFile.Hash)
				{
					// Update the expected hash to match what we're looking for
					NewFile.ExpectedHash = TargetFile.Hash;

					// Move the existing file to the new working set
					CurrentFileLookup.Remove(NewFile.Name);
				}
				else
				{
					// Create a new working file
					NewFile = new WorkingFile();
					NewFile.Name = TargetFile.Name;
					NewFile.ExpectedHash = TargetFile.Hash;

					// Add it to the download list
					FilesToDownload.Add(TargetFile);
				}
				NewWorkingManifest.Files.Add(NewFile);
			}

			// Print out everything that we'd change in a dry run
			if(bDryRun)
			{
				HashSet<string> NewFiles = new HashSet<string>(FilesToDownload.Select(x => x.Name));
				foreach(string RemoveFile in CurrentFileLookup.Keys.Where(x => !NewFiles.Contains(x)))
				{
					Log.WriteLine("Remove {0}", RemoveFile);
				}
				foreach(string UpdateFile in CurrentFileLookup.Keys.Where(x => NewFiles.Contains(x)))
				{
					Log.WriteLine("Update {0}", UpdateFile);
				}
				foreach(string AddFile in NewFiles.Where(x => !CurrentFileLookup.ContainsKey(x)))
				{
					Log.WriteLine("Add {0}", AddFile);
				}
				return true;
			}

			// Delete any files which are no longer needed
			List<WorkingFile> TamperedFiles = new List<WorkingFile>();
			foreach(WorkingFile FileToRemove in CurrentFileLookup.Values)
			{
				if(Overwrite != OverwriteMode.Force && FileToRemove.Hash != FileToRemove.ExpectedHash)
				{
					TamperedFiles.Add(FileToRemove);
				}
				else if(!SafeDeleteFile(Path.Combine(RootPath, FileToRemove.Name)))
				{
					return false;
				}
			}

			// Warn if there were any files that have been tampered with, and allow the user to choose whether to overwrite them
			bool bOverwriteTamperedFiles = true;
			if(TamperedFiles.Count > 0 && Overwrite != OverwriteMode.Force)
			{
				// List the files that have changed
				Log.WriteError("The following file(s) have been modified:");
				foreach(WorkingFile TamperedFile in TamperedFiles)
				{
					Log.WriteError("  {0}", TamperedFile.Name);
				}

				// Figure out whether to overwrite the files
				if(Overwrite == OverwriteMode.Unchanged)
				{
					Log.WriteError("Re-run with the --force parameter to overwrite them.");
					bOverwriteTamperedFiles = false;
				}
				else
				{
					Log.WriteStatus("Would you like to overwrite your changes (y/n)? ");
					ConsoleKeyInfo KeyInfo = Console.ReadKey(false);
					bOverwriteTamperedFiles = (KeyInfo.KeyChar == 'y' || KeyInfo.KeyChar == 'Y');
					Log.FlushStatus();
				}
			}

			// Overwrite any tampered files, or remove them from the download list
			if(bOverwriteTamperedFiles)
			{
				foreach(WorkingFile TamperedFile in TamperedFiles)
				{
					if(!SafeDeleteFile(Path.Combine(RootPath, TamperedFile.Name)))
					{
						return false;
					}
				}
			}
			else
			{
				foreach(WorkingFile TamperedFile in TamperedFiles)
				{
					DependencyFile TargetFile;
					if(TargetFiles.TryGetValue(TamperedFile.Name, out TargetFile))
					{
						TargetFiles.Remove(TamperedFile.Name);
						FilesToDownload.Remove(TargetFile);
					}
				}
			}

			// Write out the new working manifest, so we can track any files that we're going to download. We always verify missing files on startup, so it's ok that things don't exist yet.
			if(!WriteWorkingManifest(WorkingManifestPath, TempWorkingManifestPath, NewWorkingManifest))
			{
				return false;
			}

			// If there's nothing to do, just print a simpler message and exit early
			if(FilesToDownload.Count > 0)
			{
				// Download all the new dependencies
				if(!DownloadDependencies(RootPath, FilesToDownload, TargetBlobs.Values, TargetPacks.Values, NumThreads, MaxRetries, ProxyUrl, ProxyUsername, ProxyPassword))
				{
					return false;
				}

				// Update all the timestamps and hashes for the output files
				foreach(WorkingFile NewFile in NewWorkingManifest.Files)
				{
					if(NewFile.Hash != NewFile.ExpectedHash)
					{
						string NewFileName = Path.Combine(RootPath, NewFile.Name);
						NewFile.Hash = NewFile.ExpectedHash;
						NewFile.Timestamp = File.GetLastWriteTimeUtc(NewFileName).Ticks;
					}
				}

				// Rewrite the manifest with the results
				if(!WriteWorkingManifest(WorkingManifestPath, TempWorkingManifestPath, NewWorkingManifest))
				{
					return false;
				}
			}

			// Update all the executable permissions
			if(!SetExecutablePermissions(RootPath, FilteredTargetFiles))
			{
				return false;
			}

			return true;
		}

		static bool SetExecutablePermissions(string RootDir, IEnumerable<DependencyFile> Files)
		{
			// Try to load the Mono Posix assembly. If it doesn't exist, we're on Windows.
			Assembly MonoPosix;
			try
			{
				MonoPosix = Assembly.Load("Mono.Posix, Version=4.0.0.0, Culture=neutral, PublicKeyToken=0738eb9f132ed756");
			}
			catch(FileNotFoundException)
			{
				return true;
			}

			// Dynamically find all the types and methods for Syscall.stat and Syscall.chmod
			Type SyscallType = MonoPosix.GetType("Mono.Unix.Native.Syscall");
			if(SyscallType == null)
			{
				Log.WriteError("Couldn't find Syscall type");
				return false;
			}
            MethodInfo StatMethod = SyscallType.GetMethod ("stat");
			if(StatMethod == null)
			{
				Log.WriteError("Couldn't find Mono.Unix.Native.Syscall.stat method");
				return false;
			}
			MethodInfo ChmodMethod = SyscallType.GetMethod("chmod");
			if(ChmodMethod == null)
			{
				Log.WriteError("Couldn't find Mono.Unix.Native.Syscall.chmod method");
				return false;
			}
			Type StatType = MonoPosix.GetType("Mono.Unix.Native.Stat");
			if(StatType == null)
			{
				Log.WriteError("Couldn't find Mono.Unix.Native.Stat type");
				return false;
			}
			FieldInfo StatModeField = StatType.GetField("st_mode");
			if(StatModeField == null)
			{
				Log.WriteError("Couldn't find Mono.Unix.Native.Stat.st_mode field");
				return false;
			}

			// Update all the executable permissions
			const uint ExecutableBits = (1 << 0) | (1 << 3) | (1 << 6);
			foreach(DependencyFile File in Files)
			{
				if(File.IsExecutable)
				{
					string FileName = Path.Combine(RootDir, File.Name);

					// Call Syscall.stat(Filename, out Stat)
					object[] StatArgs = new object[]{ FileName, null };
					int StatResult = (int)StatMethod.Invoke(null, StatArgs);
					if(StatResult != 0)
					{
						Log.WriteError("Stat() call for {0} failed with error {1}", File.Name, StatResult);
						return false;
					}

					// Get the current permissions
					uint CurrentPermissions = (uint)StatModeField.GetValue(StatArgs[1]);

					// The desired permissions should be executable for every read group
					uint NewPermissions = CurrentPermissions | ((CurrentPermissions >> 2) & ExecutableBits);

					// Update them if they don't match
					if (CurrentPermissions != NewPermissions)
					{
						int ChmodResult = (int)ChmodMethod.Invoke(null, new object[]{ FileName, NewPermissions });
						if(ChmodResult != 0)
						{
							Log.WriteError("Chmod() call for {0} failed with error {1}", File.Name, ChmodResult);
							return false;
						}
					}
				}
			}
			return true;
		}

		static bool IsExcludedFolder(string Name, IEnumerable<string> ExcludeFolders)
		{
			foreach(string ExcludeFolder in ExcludeFolders)
			{
				if(Name.IndexOf("/" + ExcludeFolder + "/", StringComparison.CurrentCultureIgnoreCase) != -1)
				{
					return true;
				}
			}
			return false;
		}

		static bool DownloadDependencies(string RootPath, IEnumerable<DependencyFile> RequiredFiles, IEnumerable<DependencyBlob> Blobs, IEnumerable<DependencyPack> Packs, int NumThreads, int MaxRetries, string ProxyUrl, string ProxyUsername, string ProxyPassword)
		{
			// Make sure we can actually open the right number of connections
			ServicePointManager.DefaultConnectionLimit = NumThreads;

			// Build a lookup for the files that need updating from each blob
			Dictionary<string, List<DependencyFile>> BlobToFiles = new Dictionary<string,List<DependencyFile>>();
			foreach(DependencyFile RequiredFile in RequiredFiles)
			{
				List<DependencyFile> FileList;
				if(!BlobToFiles.TryGetValue(RequiredFile.Hash, out FileList))
				{
					FileList = new List<DependencyFile>();
					BlobToFiles.Add(RequiredFile.Hash, FileList);
				}
				FileList.Add(RequiredFile);
			}

			// Find all the required blobs
			DependencyBlob[] RequiredBlobs = Blobs.Where(x => BlobToFiles.ContainsKey(x.Hash)).ToArray();

			// Build a lookup for the files that need updating from each blob
			Dictionary<string, List<DependencyBlob>> PackToBlobs = new Dictionary<string,List<DependencyBlob>>();
			foreach(DependencyBlob RequiredBlob in RequiredBlobs)
			{
				List<DependencyBlob> BlobList = new List<DependencyBlob>();
				if(!PackToBlobs.TryGetValue(RequiredBlob.PackHash, out BlobList))
				{
					BlobList = new List<DependencyBlob>();
					PackToBlobs.Add(RequiredBlob.PackHash, BlobList);
				}
				BlobList.Add(RequiredBlob);
			}

			// Find all the required packs
			DependencyPack[] RequiredPacks = Packs.Where(x => PackToBlobs.ContainsKey(x.Hash)).ToArray();

			// Get temporary filenames for all the files we're going to download
			Dictionary<DependencyPack, string> DownloadFileNames = new Dictionary<DependencyPack,string>();
			foreach(DependencyPack Pack in RequiredPacks)
			{
				DownloadFileNames.Add(Pack, Path.GetTempFileName());
			}

			// Setup the async state
			AsyncDownloadState State = new AsyncDownloadState();
			State.NumFiles = RequiredFiles.Count();
			long NumBytesTotal = RequiredPacks.Sum(x => x.CompressedSize);
			ConcurrentQueue<DependencyPack> DownloadQueue = new ConcurrentQueue<DependencyPack>(RequiredPacks);
			ConcurrentQueue<DependencyPack> DecompressQueue = new ConcurrentQueue<DependencyPack>();

			// Create all the worker threads
			Thread[] WorkerThreads = new Thread[NumThreads];
			for(int Idx = 0; Idx < NumThreads; Idx++)
			{
				WorkerThreads[Idx] = new Thread(x => DownloadWorker(RootPath, DownloadQueue, DecompressQueue, DownloadFileNames, PackToBlobs, BlobToFiles, State, MaxRetries, ProxyUrl, ProxyUsername, ProxyPassword));
				WorkerThreads[Idx].Start();
			}

			// Create the decompression thread
			Thread DecompressionThread = new Thread(x => DecompressWorker(RootPath, DecompressQueue, DownloadFileNames, PackToBlobs, BlobToFiles, State));
			DecompressionThread.Start();

			// Tick the status message until we've finished or ended with an error. Use a circular buffer to average out the speed over time.
			long[] NumBytesReadBuffer = new long[60];
			for(int BufferIdx = 0, NumFilesReportedRead = 0; NumFilesReportedRead < State.NumFiles && State.NumFailingDownloads < NumThreads && State.LastDecompressError == null; BufferIdx = (BufferIdx + 1) % NumBytesReadBuffer.Length)
			{
				const int TickInterval = 100;

				long NumBytesRead = Interlocked.Read(ref State.NumBytesRead);
				float NumBytesPerSecond = (float)Math.Max(NumBytesRead - NumBytesReadBuffer[BufferIdx], 0) * 1000.0f / (NumBytesReadBuffer.Length * TickInterval);
				NumFilesReportedRead = State.NumFilesRead;
				Log.WriteStatus("Received {0}/{1} files ({2:0.0}/{3:0.0}mb; {4:0.00}mb/s; {5}%)...", NumFilesReportedRead, State.NumFiles, (NumBytesRead / (1024.0 * 1024.0)) + 0.0999999, (NumBytesTotal / (1024.0 * 1024.0)) + 0.0999999, (NumBytesPerSecond / (1024.0 * 1024.0)) + 0.0099, (NumBytesRead * 100) / NumBytesTotal);
				NumBytesReadBuffer[BufferIdx] = NumBytesRead;

				Thread.Sleep(TickInterval);
			}

			// If we finished with an error, try to clean up and return
			if(State.NumFilesRead < State.NumFiles)
			{
				DecompressionThread.Abort();
				foreach(Thread WorkerThread in WorkerThreads)
				{
					WorkerThread.Abort();
				}
				Log.WriteError("{0}", (State.LastDecompressError != null)? State.LastDecompressError : State.LastDownloadError);
				foreach(string FileName in DownloadFileNames.Values)
				{
					try { File.Delete(FileName); } catch (Exception) { }
				}
				return false;
			}

			// Join all the threads
			DecompressionThread.Join();
			foreach(Thread WorkerThread in WorkerThreads)
			{
				WorkerThread.Join();
			}
			Log.FlushStatus();
			return true;
		}

		static void DecompressWorker(string RootPath, ConcurrentQueue<DependencyPack> DecompressQueue, Dictionary<DependencyPack, string> DownloadFileNames, Dictionary<string, List<DependencyBlob>> PackToBlobs, Dictionary<string, List<DependencyFile>> BlobToFiles, AsyncDownloadState State)
		{
			while(State.NumFilesRead < State.NumFiles)
			{
				// Remove the next file from the queue, or wait before polling again
				DependencyPack NextPack;
				if(!DecompressQueue.TryDequeue(out NextPack))
				{
					Thread.Sleep(100);
					continue;
				}

				// Get the filename for the downloaded pack
				string PackFileName = DownloadFileNames[NextPack];

				// Extract all the files from this pack file to their final locations
				foreach(DependencyBlob Blob in PackToBlobs[NextPack.Hash])
				{
					foreach(DependencyFile File in BlobToFiles[Blob.Hash])
					{
						string OutputFileName = Path.Combine(RootPath, File.Name);
						try
						{
							ExtractBlob(PackFileName, Blob, OutputFileName);
						}
						catch(Exception Ex)
						{
							Interlocked.CompareExchange(ref State.LastDecompressError, String.Format("Failed to extract '{0}': {1}", OutputFileName, Ex.Message), null);
							return;
						}
						Interlocked.Increment(ref State.NumFilesRead);
					}
				}

				// Delete the pack file now that we're finished with it. Doesn't matter much if it fails.
				try { System.IO.File.Delete(PackFileName); } catch(Exception) { }
			}
		}

		static void DownloadWorker(string RootPath, ConcurrentQueue<DependencyPack> DownloadQueue, ConcurrentQueue<DependencyPack> DecompressQueue, Dictionary<DependencyPack, string> DownloadFileNames, Dictionary<string, List<DependencyBlob>> PackToBlobs, Dictionary<string, List<DependencyFile>> BlobToFiles, AsyncDownloadState State, int MaxRetries, string ProxyUrl, string ProxyUsername, string ProxyPassword)
		{
			int Retries = 0;
			while(State.NumFilesRead < State.NumFiles)
			{
				// Remove the next file from the download queue, or wait before polling again
				DependencyPack NextPack;
				if(!DownloadQueue.TryDequeue(out NextPack))
				{
					Thread.Sleep(100);
					continue;
				}

				// Get the temporary file to download to
				string PackFileName = DownloadFileNames[NextPack];

				// Format the URL for it
				string Url = String.Format("http://cdn.unrealengine.com/dependencies/{0}/{1}", NextPack.RemotePath, NextPack.Hash);

				// Try to download the file
				long RollbackSize = 0;
				try
				{
					// Download the file and queue it for decompression
					DownloadFileAndVerifyHash(Url, ProxyUrl, ProxyUsername, ProxyPassword, PackFileName, NextPack.Hash, Size => { RollbackSize += Size; Interlocked.Add(ref State.NumBytesRead, Size); });
					DecompressQueue.Enqueue(NextPack);

					// If we were failing, decrement the number of failing threads
					if(Retries > MaxRetries)
					{
						Interlocked.Decrement(ref State.NumFailingDownloads);
						Retries = 0;
					}
				}
				catch(Exception Ex)
				{
					// Rollback the byte count and add the file back into the download queue
					Interlocked.Add(ref State.NumBytesRead, -RollbackSize);
					DownloadQueue.Enqueue(NextPack);

					// If we've retried enough times already, set the error message. 
					if(Retries++ == MaxRetries)
					{
						Interlocked.Increment(ref State.NumFailingDownloads);
						State.LastDownloadError = String.Format("Failed to download '{0}' to '{1}': {2} ({3})", Url, PackFileName, Ex.Message, Ex.GetType().Name);
					}
				}
			}
		}

		static void DownloadFileAndVerifyHash(string Url, string ProxyUrl, string ProxyUsername, string ProxyPassword, string PackFileName, string ExpectedHash, NotifyReadDelegate NotifyRead)
		{
			// Create the web request
			WebRequest Request = WebRequest.Create(Url);
			if(String.IsNullOrEmpty(ProxyUrl))
			{
				Request.Proxy = null;
			}
			else if (String.IsNullOrEmpty(ProxyUsername))
			{
                Request.Proxy = new WebProxy(ProxyUrl);
			}
            else
            {
                Request.Proxy = new WebProxy(ProxyUrl, true, null, new NetworkCredential(ProxyUsername, ProxyPassword));
            }

			// Get the response
			using(WebResponse Response = Request.GetResponse())
			{
				// Download the file, decompressing and hashing it as we go
				SHA1Managed Hasher = new SHA1Managed();
				using(FileStream OutputStream = File.OpenWrite(PackFileName))
				{
					CryptoStream HashOutputStream = new CryptoStream(OutputStream, Hasher, CryptoStreamMode.Write);
					using(NotifyReadStream InputStream = new NotifyReadStream(Response.GetResponseStream(), NotifyRead))
					{
						GZipStream DecompressedStream = new GZipStream(InputStream, CompressionMode.Decompress, true);
						DecompressedStream.CopyTo(HashOutputStream);
					}
					HashOutputStream.FlushFinalBlock();
				}

				// Check the hash was what we expected
				string Hash = BitConverter.ToString(Hasher.Hash).ToLower().Replace("-", "");
				if(Hash != ExpectedHash)
				{
					throw new InvalidDataException(String.Format("Incorrect hash for {0} - expected {1}, got {2}", Url, ExpectedHash, Hash));
				}
			}
		}

		static void ExtractBlob(string PackFileName, DependencyBlob Blob, string OutputFileName)
		{
			// Create the output folder
			Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));

			// Copy the data to the output file
			SHA1Managed Hasher = new SHA1Managed();
			using(FileStream OutputStream = File.OpenWrite(OutputFileName + IncomingFileSuffix))
			{
				CryptoStream HashOutputStream = new CryptoStream(OutputStream, Hasher, CryptoStreamMode.Write);
				using(FileStream InputStream = File.OpenRead(PackFileName))
				{
					// Seek to the right position
					InputStream.Seek(Blob.PackOffset, SeekOrigin.Begin);

					// Extract the data
					byte[] Buffer = new byte[16384];
					for(long RemainingSize = Blob.Size; RemainingSize > 0; )
					{
						int ReadSize = InputStream.Read(Buffer, 0, (int)Math.Min(RemainingSize, (long)Buffer.Length));
						if(ReadSize == 0) throw new InvalidDataException("Unexpected end of file");
						HashOutputStream.Write(Buffer, 0, ReadSize);
						RemainingSize -= ReadSize;
					}
				}
				HashOutputStream.FlushFinalBlock();
			}

			// Check the hash was what we expected
			string Hash = BitConverter.ToString(Hasher.Hash).ToLower().Replace("-", "");
			if(Hash != Blob.Hash)
			{
				throw new InvalidDataException("Incorrect hash value");
			}

			// Move the file to its final position
			File.Move(OutputFileName + IncomingFileSuffix, OutputFileName);
		}

		public static bool ReadXmlObject<T>(string FileName, out T NewObject)
		{
			try
			{
				XmlSerializer Serializer = new XmlSerializer(typeof(T));
				using(StreamReader Reader = new StreamReader(FileName))
				{
					NewObject = (T)Serializer.Deserialize(Reader);
				}
				return true;
			}
			catch(Exception Ex)
			{
				Log.WriteError("Failed to read '{0}': {1}", FileName, Ex.ToString());
				NewObject = default(T);
				return false;
			}
		}

		public static bool WriteXmlObject<T>(string FileName, T XmlObject)
		{
			try
			{
				XmlSerializer Serializer = new XmlSerializer(typeof(T));
				using(StreamWriter Writer = new StreamWriter(FileName))
				{
					Serializer.Serialize(Writer, XmlObject);
				}
				return true;
			}
			catch(Exception Ex)
			{
				Log.WriteError("Failed to write file '{0}': {1}", FileName, Ex.Message);
				return false;
			}
		}

		public static bool WriteWorkingManifest(string FileName, string TemporaryFileName, WorkingManifest Manifest)
		{
			if(!WriteXmlObject(TemporaryFileName, Manifest))
			{
				return false;
			}
			if(!SafeModifyFileAttributes(TemporaryFileName, FileAttributes.Hidden, 0))
			{
				return false;
			}
			if(!SafeDeleteFile(FileName))
			{
				return false;
			}
			if(!SafeMoveFile(TemporaryFileName, FileName))
			{
				return false;
			}
			return true;
		}

		public static bool SafeModifyFileAttributes(string FileName, FileAttributes AddAttributes, FileAttributes RemoveAttributes)
		{
			try
			{
				File.SetAttributes(FileName, (File.GetAttributes(FileName) | AddAttributes) & ~RemoveAttributes);
				return true;
			}
			catch(IOException)
			{
				Log.WriteError("Failed to set attributes for file '{0}'", FileName);
				return false;
			}
		}

		public static bool SafeCreateDirectory(string DirectoryName)
		{
			try
			{
				Directory.CreateDirectory(DirectoryName);
				return true;
			}
			catch(IOException)
			{
				Log.WriteError("Failed to create directory '{0}'", DirectoryName);
				return false;
			}
		}

		public static bool SafeDeleteFile(string FileName)
		{
			try
			{
				File.Delete(FileName);
				return true;
			}
			catch(IOException)
			{
				Log.WriteError("Failed to delete file '{0}'", FileName);
				return false;
			}
		}

		public static bool SafeMoveFile(string SourceFileName, string TargetFileName)
		{
			try
			{
				File.Move(SourceFileName, TargetFileName);
				return true;
			}
			catch(IOException)
			{
				Log.WriteError("Failed to rename '{0}' to '{1}'", SourceFileName, TargetFileName);
				return false;
			}
		}

		public static string ComputeHashForFile(string FileName)
		{
			using(FileStream InputStream = File.OpenRead(FileName))
			{
				byte[] Hash = new SHA1CryptoServiceProvider().ComputeHash(InputStream);
				return BitConverter.ToString(Hash).ToLower().Replace("-", "");
			}
		}
	}
}
