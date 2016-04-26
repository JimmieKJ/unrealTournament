// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace Tools.DotNETCommonPrivate.HarvestEnvVars
{
	// Struct which represents a key-value.  Conceptually private, but the serializer doesn't like it to be actually private.
	[Serializable]
	[DebuggerDisplay("\\{{Key}={Value}\\}")]
	public struct EnvVar
	{
		#pragma warning disable 0649

		[XmlAttribute("Key")]
		public string Key;

		[XmlAttribute("Value")]
		public string Value;

		#pragma warning restore 0649
	}
}

namespace Tools.DotNETCommon.HarvestEnvVars
{
	public static class HarvestEnvVars
	{
		public enum EPathOverride
		{
			None,
			User,
		}

		private static XmlSerializer EnvVarListSerializer = XmlSerializer.FromTypes(new Type[]{ typeof(List<Tools.DotNETCommonPrivate.HarvestEnvVars.EnvVar>) })[0];

		public static CaselessDictionary.CaselessDictionary<string> HarvestEnvVarsFromBatchFile(string BatchFileName, string BatchFileParameters, EPathOverride PathOverride)
		{
			if (!File.Exists(BatchFileName))
			{
				throw new Exception(string.Format("Tools.DotNETCommon.HarvestEnvVars.HarvestEnvVars.HarvestEnvVarsFromBatchFile: BatchFile {0} does not exist!", BatchFileName));
			}

			// Create a wrapper batch file that echoes environment variables to a text file
			string EnvOutputFileName      = Path.GetTempFileName();
			string EnvReaderBatchFileName = EnvOutputFileName + ".bat";
			try
			{
				var EnvVarsToXMLExePath = Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "EnvVarsToXML.exe");

				// Convert every path to short filenames to ensure we don't accidentally write out a non-ASCII batch file
				var ShortBatchFileName       = FileSystem.FileSystem.GetShortPathName(BatchFileName);
				var ShortEnvOutputFileName   = Path.Combine("%~dp0%", Path.GetFileName(EnvOutputFileName));
				var ShortEnvVarsToXMLExePath = FileSystem.FileSystem.GetShortPathName(EnvVarsToXMLExePath);

				var EnvReaderBatchFileContent = new StringBuilder();

				// Run 'vcvars32.bat' (or similar x64 version) to set environment variables
				EnvReaderBatchFileContent.AppendFormat("call \"{0}\" {1}", ShortBatchFileName, BatchFileParameters).AppendLine();

				// Pipe all environment variables to a file where we can read them in.
				// We use a separate executable which runs after the batch file because we want to capture
				// the environment after it has been set, and there's no easy way of doing this, and parsing
				// the output of the set command is problematic when the vars contain non-ASCII characters.
				EnvReaderBatchFileContent.AppendFormat("\"{0}\" \"{1}\"", ShortEnvVarsToXMLExePath, ShortEnvOutputFileName).AppendLine();

				FileInfo TempFileInfo = new FileInfo(EnvReaderBatchFileName);
				if (TempFileInfo.Exists)
				{
					TempFileInfo.IsReadOnly = false;
					TempFileInfo.Delete();
					TempFileInfo.Refresh();
				}

				Directory.CreateDirectory(Path.GetDirectoryName(EnvReaderBatchFileName));
				File.WriteAllText(EnvReaderBatchFileName, EnvReaderBatchFileContent.ToString());
			}
			catch (Exception Ex)
			{
				throw new Exception(string.Format("Failed to create temporary batch file {0} to harvest environment variables (\"{1}\")", EnvReaderBatchFileName, Ex.Message), Ex);
			}

			// process needs to be disposed when done
			using (var BatchFileProcess = new Process())
			{
				// Run the batch file using cmd.exe with the /U option, to force Unicode output. Many locales have non-ANSI characters in system paths.
				var StartInfo = BatchFileProcess.StartInfo;
				StartInfo.FileName        = Path.Combine(Environment.SystemDirectory, "cmd.exe");
				StartInfo.Arguments       = String.Format("/U /C \"{0}\"", EnvReaderBatchFileName);
				StartInfo.CreateNoWindow  = true;
				StartInfo.UseShellExecute = false;

				// Override path variable if it can be particularly long
				if (PathOverride == EPathOverride.User)
				{
					string NewPathVariable = Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.Machine) ?? "";
					if (String.IsNullOrEmpty(NewPathVariable))
					{
						NewPathVariable = Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.User);
					}
					else
					{
						NewPathVariable += ";" + Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.User);
					}
					StartInfo.EnvironmentVariables["PATH"] = NewPathVariable;
				}

				// Try to launch the process, and produce a friendly error message if it fails.
				try
				{
					// Start the process up and then wait for it to finish
					BatchFileProcess.Start();
					BatchFileProcess.WaitForExit();
				}
				catch (Exception Ex)
				{
					throw new Exception(string.Format("Failed to start local process for action (\"{0}\"): {1} {2}", Ex.Message, StartInfo.FileName, StartInfo.Arguments), Ex);
				}
			}

			// Accept chars which are technically not valid XML - they were written out by XmlSerializer anyway!
			var Settings = new XmlReaderSettings();
			Settings.CheckCharacters  = false;

			List<Tools.DotNETCommonPrivate.HarvestEnvVars.EnvVar> EnvVars;
			try
			{
				using (var Stream = new StreamReader(EnvOutputFileName))
				using (var Reader = XmlReader.Create(Stream, Settings))
				{
					EnvVars = (List<Tools.DotNETCommonPrivate.HarvestEnvVars.EnvVar>)EnvVarListSerializer.Deserialize(Reader);
				}
			}
			catch (Exception Ex)
			{
				throw new Exception(string.Format("Failed to read environment variables from XML file: {0}", EnvOutputFileName), Ex);
			}

			// Clean up the temporary files we created earlier on, so the temp directory doesn't fill up
			// with these guys over time
			try
			{
				File.Delete(EnvOutputFileName);
			}
			catch (Exception)
			{
				// Unable to delete the temporary file.  Not a big deal.
			}

			try
			{
				File.Delete(EnvReaderBatchFileName);
			}
			catch (Exception)
			{
				// Unable to delete the temporary file.  Not a big deal.
			}

			var Result = new CaselessDictionary.CaselessDictionary<string>();
			foreach (var Var in EnvVars)
			{
				Result.Add(Var.Key, Var.Value);
			}

			return Result;
		}
	}
}
