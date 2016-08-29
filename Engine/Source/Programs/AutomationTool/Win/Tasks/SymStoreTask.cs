using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.IO;
using System.Threading;
using System.Xml;

namespace Win.Automation
{
	/// <summary>
	/// Utility class which creates a file and obtains an exclusive lock on it. Used as a mutex between processes on different machines through a network share.
	/// </summary>
	class LockFile : IDisposable
	{
		/// <summary>
		/// The open file stream to the lock file
		/// </summary>
		FileStream Stream;

		/// <summary>
		/// If set, the file that will be deleted when the object is disposed. Used to clean up the lock file once we're done with it.
		/// </summary>
		FileReference FileToDelete;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="File">The file to use for a lock</param>
		/// <param name="TimeoutSeconds">Amount of time to retry waiting for the lock. Throws an exception if this time is exceeded.</param>
		public LockFile(FileReference File, int TimeoutSeconds)
		{
			File.Directory.CreateDirectory();

			for (int NumSeconds = 0; ; NumSeconds++)
			{
				try
				{
					Stream = new FileStream(File.FullName, FileMode.Create, FileAccess.Write, FileShare.Read);
					Stream.Write(Encoding.UTF8.GetBytes(Environment.MachineName));
					Stream.Flush();
					FileToDelete = File;
					break;
				}
				catch (IOException)
				{
					// File already exists; retry
				}

				if (NumSeconds > TimeoutSeconds)
				{
					throw new AutomationException("Couldn't create lock file '{0}' after {1} attempts", File.FullName, NumSeconds);
				}

				if (NumSeconds == 0)
				{
					CommandUtils.Log("Waiting for lock file '{0}' to be removed...", File.FullName);
				}
				else if ((NumSeconds % 30) == 0)
				{
					CommandUtils.LogWarning("Still Waiting for lock file '{0}' after {1} seconds.", File.FullName, NumSeconds);
				}

				Thread.Sleep(1000);
			}
		}

		/// <summary>
		/// Dispose of the lock file.
		/// </summary>
		public void Dispose()
		{
			if (Stream != null)
			{
				Stream.Dispose();
				Stream = null;
			}
			if (FileToDelete != null)
			{
				FileToDelete.Delete();
				FileToDelete = null;
			}
		}
	}

	/// <summary>
	/// Parameters for a task that uploads symbols to a symbol server
	/// </summary>
	public class SymStoreTaskParameters
	{
		/// <summary>
		/// List of output files. PDBs will be extracted from this list.
		/// </summary>
		[TaskParameter]
		public string Files;

		/// <summary>
		/// Output directory for the compressed symbols.
		/// </summary>
		[TaskParameter]
		public string StoreDir;

		/// <summary>
		/// Name of the product for the symbol store records.
		/// </summary>
		[TaskParameter]
		public string Product;
	}

	/// <summary>
	/// Task which strips symbols from a set of files
	/// </summary>
	[TaskElement("SymStore", typeof(SymStoreTaskParameters))]
	public class SymStoreTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SymStoreTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public SymStoreTask(SymStoreTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Find the matching files
			FileReference[] Files = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet).ToArray();
			Files = Files.Where(x => x.HasExtension(".pdb")).ToArray();

			// Get the symbol store directory
			DirectoryReference StoreDir = ResolveDirectory(Parameters.StoreDir);

			// Get the SYMSTORE.EXE path, using the latest SDK version we can find.
			FileReference SymStoreExe;
			if(!TryGetSymStoreExe("v10.0", out SymStoreExe) && !TryGetSymStoreExe("v8.1", out SymStoreExe) && !TryGetSymStoreExe("v8.0", out SymStoreExe))
			{
				CommandUtils.LogError("Couldn't find SYMSTORE.EXE in any Windows SDK installation");
				return false;
			}

			// Create a lock file in the destination directory
			FileReference LockFile = FileReference.Combine(StoreDir, "lock");
			using (FileStream LockFileStream = CreateLockFile(LockFile, 5 * 60))
			{
				foreach (FileReference File in Files)
				{
					CommandUtils.RunAndLog(SymStoreExe.FullName, String.Format("add /f \"{0}\" /s \"{1}\" /t \"{2}\"", File.FullName, StoreDir.FullName, Parameters.Product));
				}
			}
			LockFile.Delete();
			return true;
		}

		/// <summary>
		/// Try to get the SYMSTORE.EXE path from the given Windows SDK version
		/// </summary>
		/// <param name="SdkVersion">The SDK version string</param>
		/// <param name="SymStoreExe">Receives the path to symstore.exe if found</param>
		/// <returns>True if found, false otherwise</returns>
		static bool TryGetSymStoreExe(string SdkVersion, out FileReference SymStoreExe)
		{
			// Try to get the SDK installation directory
			string SdkFolder = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
			if (SdkFolder == null)
			{
				SdkFolder = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
				if (SdkFolder == null)
				{
					SdkFolder = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
					if (SdkFolder == null)
					{
						SymStoreExe = null;
						return false;
					}
				}
			}

			// Check for the 64-bit toolchain first, then the 32-bit toolchain
			FileReference CheckSymStoreExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x64", "SymStore.exe");
			if (!CheckSymStoreExe.Exists())
			{
				CheckSymStoreExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x86", "SymStore.exe");
				if (!CheckSymStoreExe.Exists())
				{
					SymStoreExe = null;
					return false;
				}
			}

			SymStoreExe = CheckSymStoreExe;
			return true;
		}

        /// <summary>
        /// Output this task out to an XML writer.
        /// </summary>
        public override void Write(XmlWriter Writer)
        {
            Write(Writer, Parameters);
        }

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Files);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Creates a lock file, or waits for 
		/// </summary>
		/// <param name="FileName"></param>
		/// <returns></returns>
		static FileStream CreateLockFile(FileReference LockFile, int TimeoutSeconds)
		{
			LockFile.Directory.CreateDirectory();

			int NumRetries = 0;
			int MaxRetries = TimeoutSeconds;
			for (;;)
			{
				try
				{
					FileStream Stream = new FileStream(LockFile.FullName, FileMode.Create, FileAccess.Write, FileShare.Read);
					Stream.Write(Encoding.UTF8.GetBytes(Environment.MachineName));
					Stream.Flush();
					return Stream;
				}
				catch (IOException)
				{
					// File already exists; retry
				}

				NumRetries++;
				if (NumRetries > MaxRetries)
				{
					throw new AutomationException("Couldn't create lock file '{0}' after {1} attempts", LockFile.FullName, MaxRetries);
				}
				if (NumRetries == 1)
				{
					CommandUtils.Log("Waiting for lock file '{0}' to be removed...", LockFile.FullName);
				}
				else if ((NumRetries % 30) == 0)
				{
					CommandUtils.LogWarning("Still Waiting for lock file '{0}' after {1} retries.", LockFile.FullName, NumRetries);
				}
				Thread.Sleep(1000);
			}
		}
	}
}
