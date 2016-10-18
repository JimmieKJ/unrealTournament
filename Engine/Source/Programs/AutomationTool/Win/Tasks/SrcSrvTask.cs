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
	/// Parameters for a task that uploads symbols to a symbol server
	/// </summary>
	public class SrcSrvTaskParameters
	{
		/// <summary>
		/// List of output files. PDBs will be extracted from this list.
		/// </summary>
		[TaskParameter]
		public string BinaryFiles;

		/// <summary>
		/// List of source files to index and embed into the PDBs.
		/// </summary>
		[TaskParameter]
		public string SourceFiles;

		/// <summary>
		/// Branch to base all the depot source files from
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// Changelist to sync files from
		/// </summary>
		[TaskParameter]
		public int Change;
	}

	/// <summary>
	/// Task which strips symbols from a set of files
	/// </summary>
	[TaskElement("SrcSrv", typeof(SrcSrvTaskParameters))]
	public class SrcSrvTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		SrcSrvTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public SrcSrvTask(SrcSrvTaskParameters InParameters)
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
			FileReference[] PdbFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.BinaryFiles, TagNameToFileSet).Where(x => x.HasExtension(".pdb")).ToArray();

			// Find all the matching source files
			FileReference[] SourceFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.SourceFiles, TagNameToFileSet).ToArray();

			// Get the PDBSTR.EXE path, using the latest SDK version we can find.
			FileReference PdbStrExe;
			if (!TryGetPdbStrExe("v10.0", out PdbStrExe) && !TryGetPdbStrExe("v8.1", out PdbStrExe) && !TryGetPdbStrExe("v8.0", out PdbStrExe))
			{
				CommandUtils.LogError("Couldn't find PDBSTR.EXE in any Windows SDK installation");
				return false;
			}

			// Get the path to the generated SRCSRV.INI file
			FileReference SrcSrvIni = FileReference.Combine(CommandUtils.RootDirectory, "Engine", "Intermediate", "SrcSrv.ini");
			SrcSrvIni.Directory.CreateDirectory();

			// Generate the SRCSRV.INI file
			using (StreamWriter Writer = new StreamWriter(SrcSrvIni.FullName))
			{
				Writer.WriteLine("SRCSRV: ini------------------------------------------------");
				Writer.WriteLine("VERSION=1");
				Writer.WriteLine("VERCTRL=Perforce");
				Writer.WriteLine("SRCSRV: variables------------------------------------------");
				Writer.WriteLine("SRCSRVTRG=%sdtrg%");
				Writer.WriteLine("SRCSRVCMD=%sdcmd%");
				Writer.WriteLine("SDCMD=p4.exe print -o %srcsrvtrg% \"{0}/%var2%@{1}\"", Parameters.Branch.TrimEnd('/'), Parameters.Change);
				Writer.WriteLine("SDTRG=%targ%\\{0}\\{1}\\%fnbksl%(%var2%)", Parameters.Branch.Replace('/', '+'), Parameters.Change);
				Writer.WriteLine("SRCSRV: source files ---------------------------------------");
				foreach (FileReference SourceFile in SourceFiles)
				{
					string RelativeSourceFile = SourceFile.MakeRelativeTo(CommandUtils.RootDirectory);
					Writer.WriteLine("{0}*{1}", SourceFile.FullName, RelativeSourceFile.Replace('\\', '/'));
				}
				Writer.WriteLine("SRCSRV: end------------------------------------------------");
			}

			// Embed the data in the PDB files
			foreach(FileReference PdbFile in PdbFiles)
			{
				CommandUtils.Run(PdbStrExe.FullName, String.Format("-w -p:\"{0}\" -i:\"{1}\" -s:srcsrv", PdbFile.FullName, SrcSrvIni.FullName));
			}
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
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.BinaryFiles))
			{
				yield return TagName;
			}
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.SourceFiles))
			{
				yield return TagName;
			}
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
		/// Try to get the PDBSTR.EXE path from the given Windows SDK version
		/// </summary>
		/// <param name="SdkVersion">The SDK version string</param>
		/// <param name="SymStoreExe">Receives the path to symstore.exe if found</param>
		/// <returns>True if found, false otherwise</returns>
		static bool TryGetPdbStrExe(string SdkVersion, out FileReference PdbStrExe)
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
						PdbStrExe = null;
						return false;
					}
				}
			}

			// Check for the 64-bit toolchain first, then the 32-bit toolchain
			FileReference CheckPdbStrExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x64", "SrcSrv", "PdbStr.exe");
			if (!CheckPdbStrExe.Exists())
			{
				CheckPdbStrExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x86", "SrcSrv", "PdbStr.exe");
				if (!CheckPdbStrExe.Exists())
				{
					PdbStrExe = null;
					return false;
				}
			}

			PdbStrExe = CheckPdbStrExe;
			return true;
		}
	}
}
