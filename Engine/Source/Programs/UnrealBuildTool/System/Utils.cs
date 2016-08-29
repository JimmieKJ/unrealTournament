// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Serialization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Linq;
using System.Management;
using Tools.DotNETCommon.CaselessDictionary;
using System.Web.Script.Serialization;

namespace UnrealBuildTool
{
	/// <summary>
	/// Holds information about the current engine version
	/// </summary>
	[Serializable]
	public class BuildVersion
	{
		public int MajorVersion;
		public int MinorVersion;
		public int PatchVersion;
		public int Changelist;
		public int CompatibleChangelist;
		public int IsLicenseeVersion;
		public string BranchName;

		/// <summary>
		/// Try to read a version file from disk
		/// </summary>
		/// <param name="Version">The version information</param>
		/// <returns>True if the version was read sucessfully, false otherwise</returns>
		public static bool TryRead(out BuildVersion Version)
		{
			return TryRead(GetDefaultFileName(), out Version);
		}

		/// <summary>
		/// Try to read a version file from disk
		/// </summary>
		/// <param name="Version">The version information</param>
		/// <returns>True if the version was read sucessfully, false otherwise</returns>
		public static bool TryRead(string FileName, out BuildVersion Version)
		{
			JsonObject Object;
			if (!JsonObject.TryRead(FileName, out Object))
			{
				Version = null;
				return false;
			}
			return TryParse(Object, out Version);
		}

		/// <summary>
		/// Get the default path to the build.version file on disk
		/// </summary>
		/// <returns>Path to the Build.version file</returns>
		public static string GetDefaultFileName()
		{
			return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Build.version").FullName;
		}

		/// <summary>
		/// Parses a build version from a JsonObject
		/// </summary>
		/// <param name="Object">The object to read from</param>
		/// <param name="Version">The resulting version field</param>
		/// <returns>True if the build version could be read, false otherwise</returns>
		public static bool TryParse(JsonObject Object, out BuildVersion Version)
		{
			BuildVersion NewVersion = new BuildVersion();
			if (!Object.TryGetIntegerField("MajorVersion", out NewVersion.MajorVersion) || !Object.TryGetIntegerField("MinorVersion", out NewVersion.MinorVersion) || !Object.TryGetIntegerField("PatchVersion", out NewVersion.PatchVersion))
			{
				Version = null;
				return false;
			}

			Object.TryGetIntegerField("Changelist", out NewVersion.Changelist);
			if(NewVersion.Changelist != 0)
			{
				if(!Object.TryGetIntegerField("CompatibleChangelist", out NewVersion.CompatibleChangelist))
				{
					NewVersion.CompatibleChangelist = NewVersion.Changelist;
				}
			}
			Object.TryGetIntegerField("IsLicenseeVersion", out NewVersion.IsLicenseeVersion);
			Object.TryGetStringField("BranchName", out NewVersion.BranchName);

			Version = NewVersion;
			return true;
		}

		/// <summary>
		/// Exports this object as Json
		/// </summary>
		/// <param name="Object">The object to read from</param>
		/// <param name="Version">The resulting version field</param>
		/// <returns>True if the build version could be read, false otherwise</returns>
		public void Write(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				WriteProperties(Writer);
				Writer.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Exports this object as Json
		/// </summary>
		/// <param name="Object">The object to read from</param>
		/// <param name="Version">The resulting version field</param>
		/// <returns>True if the build version could be read, false otherwise</returns>
		public void WriteProperties(JsonWriter Writer)
		{
			Writer.WriteValue("MajorVersion", MajorVersion);
			Writer.WriteValue("MinorVersion", MinorVersion);
			Writer.WriteValue("PatchVersion", PatchVersion);
			Writer.WriteValue("Changelist", Changelist);
			Writer.WriteValue("CompatibleChangelist", CompatibleChangelist);
			Writer.WriteValue("IsLicenseeVersion", IsLicenseeVersion);
			Writer.WriteValue("BranchName", BranchName);
		}
	}

	/// 
	/// Log Event Type
	///
	public enum LogEventType
	{
		Fatal = 1,
		Error = 2,
		Warning = 4,
		Console = 8,
		Log = 16,
		Verbose = 32,
		VeryVerbose = 64
	}

	/// <summary>
	/// Utility functions
	/// </summary>
	public static class Utils
	{
		/// <summary>
		/// Whether we are currently running on Mono platform.  We cache this statically because it is a bit slow to check.
		/// </summary>
		public static readonly bool IsRunningOnMono = Type.GetType("Mono.Runtime") != null;

		/// <summary>
		/// Searches for a flag in a set of command-line arguments.
		/// </summary>
		public static bool ParseCommandLineFlag(string[] Arguments, string FlagName, out int ArgumentIndex)
		{
			// Find an argument with the given name.
			for (ArgumentIndex = 0; ArgumentIndex < Arguments.Length; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex].ToUpperInvariant();
				if (Argument == FlagName.ToUpperInvariant())
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Regular expression to match $(ENV) and/ or %ENV% environment variables.
		/// </summary>
		static Regex EnvironmentVariableRegex = new Regex(@"\$\((.*?)\)|\%(.*?)\%", RegexOptions.None);

		/// <summary>
		/// Resolves $(ENV) and/ or %ENV% to the value of the environment variable in the passed in string.
		/// </summary>
		/// <param name="InString">String to resolve environment variable in.</param>
		/// <returns>String with environment variable expanded/ resolved.</returns>
		public static string ResolveEnvironmentVariable(string InString)
		{
			string Result = InString;

			// Try to find $(ENV) substring.
			Match M = EnvironmentVariableRegex.Match(InString);

			// Iterate over all matches, resolving the match to an environment variable.
			while (M.Success)
			{
				// Convoluted way of stripping first and last character and '(' in the case of $(ENV) to get to ENV
				string EnvironmentVariable = M.ToString();
				if (EnvironmentVariable.StartsWith("$") && EnvironmentVariable.EndsWith(")"))
				{
					EnvironmentVariable = EnvironmentVariable.Substring(1, EnvironmentVariable.Length - 2).Replace("(", "");
				}

				if (EnvironmentVariable.StartsWith("%") && EnvironmentVariable.EndsWith("%"))
				{
					EnvironmentVariable = EnvironmentVariable.Substring(1, EnvironmentVariable.Length - 2);
				}

				// Resolve environment variable.				
				Result = Result.Replace(M.ToString(), Environment.GetEnvironmentVariable(EnvironmentVariable));

				// Move on to next match. Multiple environment variables are handled correctly by regexp.
				M = M.NextMatch();
			}

			return Result;
		}

		/// <summary>
		/// Expands variables in $(VarName) format in the given string. Variables are retrieved from the given dictionary, or through the environment of the current process.
		/// Any unknown variables are ignored.
		/// </summary>
		/// <param name="InputString">String to search for variable names</param>
		/// <param name="Variables">Lookup of variable names to values</param>
		/// <returns>String with all variables replaced</returns>
		public static string ExpandVariables(string InputString, Dictionary<string, string> AdditionalVariables = null)
		{
			string Result = InputString;
			for (int Idx = Result.IndexOf("$("); Idx != -1; Idx = Result.IndexOf("$(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 2);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 2, EndIdx - (Idx + 2));

				// Find the value for it, either from the dictionary or the environment block
				string Value;
				if (AdditionalVariables == null || !AdditionalVariables.TryGetValue(Name, out Value))
				{
					Value = Environment.GetEnvironmentVariable(Name);
					if (Value == null)
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value + Result.Substring(EndIdx + 1);
			}
			return Result;
		}

		/// <summary>
		/// This is a faster replacement of File.ReadAllText. Code snippet based on code
		/// and analysis by Sam Allen
		/// http://dotnetperls.com/Content/File-Handling.aspx
		/// </summary>
		/// <param name="SourceFile"> Source file to fully read and convert to string</param>
		/// <returns>Textual representation of file.</returns>
		public static string ReadAllText(string SourceFile)
		{
			using (StreamReader Reader = new StreamReader(SourceFile, System.Text.Encoding.UTF8))
			{
				return Reader.ReadToEnd();
			}
		}

		/// <summary>
		/// Reads the specified environment variable
		/// </summary>
		/// <param name="VarName"> the environment variable to read</param>
		/// <param name="bDefault">the default value to use if missing</param>
		/// <returns>the value of the environment variable if found and the default value if missing</returns>
		public static bool GetEnvironmentVariable(string VarName, bool bDefault)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				// Convert the string to its boolean value
				return Convert.ToBoolean(Value);
			}
			return bDefault;
		}

		/// <summary>
		/// Reads the specified environment variable
		/// </summary>
		/// <param name="VarName"> the environment variable to read</param>
		/// <param name="Default">the default value to use if missing</param>
		/// <returns>the value of the environment variable if found and the default value if missing</returns>
		public static string GetStringEnvironmentVariable(string VarName, string Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Value;
			}
			return Default;
		}

		/// <summary>
		/// Reads the specified environment variable
		/// </summary>
		/// <param name="VarName"> the environment variable to read</param>
		/// <param name="Default">the default value to use if missing</param>
		/// <returns>the value of the environment variable if found and the default value if missing</returns>
		public static double GetEnvironmentVariable(string VarName, double Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Convert.ToDouble(Value);
			}
			return Default;
		}

		/// <summary>
		/// Reads the specified environment variable
		/// </summary>
		/// <param name="VarName"> the environment variable to read</param>
		/// <param name="Default">the default value to use if missing</param>
		/// <returns>the value of the environment variable if found and the default value if missing</returns>
		public static string GetEnvironmentVariable(string VarName, string Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Value;
			}
			return Default;
		}

		/// <summary>
		/// Try to launch a local process, and produce a friendly error message if it fails.
		/// </summary>
		public static int RunLocalProcess(Process LocalProcess)
		{
			int ExitCode = -1;

			// release all process resources
			using (LocalProcess)
			{
				LocalProcess.StartInfo.UseShellExecute = false;
				LocalProcess.StartInfo.RedirectStandardOutput = true;
				LocalProcess.StartInfo.RedirectStandardError = true;

				try
				{
					// Start the process up and then wait for it to finish
					LocalProcess.Start();
					LocalProcess.BeginOutputReadLine();
					LocalProcess.BeginErrorReadLine();
					LocalProcess.WaitForExit();
					ExitCode = LocalProcess.ExitCode;
				}
				catch (Exception ex)
				{
					throw new BuildException(ex, "Failed to start local process for action (\"{0}\"): {1} {2}", ex.Message, LocalProcess.StartInfo.FileName, LocalProcess.StartInfo.Arguments);
				}
			}

			return ExitCode;
		}

		/// <summary>
		/// Runs a local process and pipes the output to the log
		/// </summary>
		public static int RunLocalProcessAndLogOutput(ProcessStartInfo StartInfo)
		{
			Process LocalProcess = new Process();
			LocalProcess.StartInfo = StartInfo;
			LocalProcess.OutputDataReceived += (Sender, Line) => { if (Line != null && Line.Data != null) Log.TraceInformation(Line.Data); };
			LocalProcess.ErrorDataReceived += (Sender, Line) => { if (Line != null && Line.Data != null) Log.TraceError(Line.Data); };
			return RunLocalProcess(LocalProcess);
		}

		/// <summary>
		/// Runs a command line process, and returns simple StdOut output. This doesn't handle errors or return codes
		/// </summary>
		/// <returns>The entire StdOut generated from the process as a single trimmed string</returns>
		/// <param name="Command">Command to run</param>
		/// <param name="Args">Arguments to Command</param>
		public static string RunLocalProcessAndReturnStdOut(string Command, string Args)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo(Command, Args);
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.CreateNoWindow = true;

			string FullOutput = "";
			using (Process LocalProcess = Process.Start(StartInfo))
			{
				StreamReader OutputReader = LocalProcess.StandardOutput;
				// trim off any extraneous new lines, helpful for those one-line outputs
				FullOutput = OutputReader.ReadToEnd().Trim();
			}

			return FullOutput;
		}

		/// <summary>
		/// Given a list of supported platforms, returns a list of names of platforms that should not be supported
		/// </summary>
		/// <param name="SupportedPlatforms">List of supported platforms</param>
		/// <returns>List of unsupported platforms in string format</returns>
		public static List<string> MakeListOfUnsupportedPlatforms(List<UnrealTargetPlatform> SupportedPlatforms)
		{
			// Make a list of all platform name strings that we're *not* currently compiling, to speed
			// up file path comparisons later on
			List<string> OtherPlatformNameStrings = new List<string>();
			{
				// look at each group to see if any supported platforms are in it
				List<UnrealPlatformGroup> SupportedGroups = new List<UnrealPlatformGroup>();
				foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
				{
					// get the list of platforms registered to this group, if any
					List<UnrealTargetPlatform> Platforms = UEBuildPlatform.GetPlatformsInGroup(Group);
					if (Platforms != null)
					{
						// loop over each one
						foreach (UnrealTargetPlatform Platform in Platforms)
						{
							// if it's a compiled platform, then add this group to be supported
							if (SupportedPlatforms.Contains(Platform))
							{
								SupportedGroups.Add(Group);
							}
						}
					}
				}

				// loop over groups one more time, anything NOT in SupportedGroups is now unsuppored, and should be added to the output list
				foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
				{
					if (SupportedGroups.Contains(Group) == false)
					{
						OtherPlatformNameStrings.Add(Group.ToString());
					}
				}

				foreach (UnrealTargetPlatform CurPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (CurPlatform != UnrealTargetPlatform.Unknown)
					{
						bool ShouldConsider = true;

						// If we have a platform and a group with the same name, don't add the platform
						// to the other list if the same-named group is supported.  This is a lot of
						// lines because we need to do the comparisons as strings.
						string CurPlatformString = CurPlatform.ToString();
						foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
						{
							if (Group.ToString().Equals(CurPlatformString))
							{
								ShouldConsider = false;
								break;
							}
						}

						// Don't add our current platform to the list of platform sub-directory names that
						// we'll skip source files for
						if (ShouldConsider && !SupportedPlatforms.Contains(CurPlatform))
						{
							OtherPlatformNameStrings.Add(CurPlatform.ToString());
						}
					}
				}

				return OtherPlatformNameStrings;
			}
		}


		/// <summary>
		/// Takes a path string and makes all of the path separator characters consistent. Also removes unnecessary multiple separators.
		/// </summary>
		/// <param name="FilePath">File path with potentially inconsistent slashes</param>
		/// <returns>File path with consistent separators</returns>
		public static string CleanDirectorySeparators(string FilePath, char UseDirectorySeparatorChar = '\0')
		{
			StringBuilder CleanPath = null;
			if (UseDirectorySeparatorChar == '\0')
			{
				UseDirectorySeparatorChar = Environment.OSVersion.Platform == PlatformID.Unix ? '/' : '\\';
			}
			char PrevC = '\0';
			// Don't check for double separators until we run across a valid dir name. Paths that start with '//' or '\\' can still be valid.			
			bool bCanCheckDoubleSeparators = false;
			for (int Index = 0; Index < FilePath.Length; ++Index)
			{
				char C = FilePath[Index];
				if (C == '/' || C == '\\')
				{
					if (C != UseDirectorySeparatorChar)
					{
						C = UseDirectorySeparatorChar;
						if (CleanPath == null)
						{
							CleanPath = new StringBuilder(FilePath.Substring(0, Index), FilePath.Length);
						}
					}

					if (bCanCheckDoubleSeparators && C == PrevC)
					{
						if (CleanPath == null)
						{
							CleanPath = new StringBuilder(FilePath.Substring(0, Index), FilePath.Length);
						}
						continue;
					}
				}
				else
				{
					// First non-separator character, safe to check double separators
					bCanCheckDoubleSeparators = true;
				}

				if (CleanPath != null)
				{
					CleanPath.Append(C);
				}
				PrevC = C;
			}
			return CleanPath != null ? CleanPath.ToString() : FilePath;
		}


		/// <summary>
		/// Given a file path and a directory, returns a file path that is relative to the specified directory
		/// </summary>
		/// <param name="SourcePath">File path to convert</param>
		/// <param name="RelativeToDirectory">The directory that the source file path should be converted to be relative to.  If this path is not rooted, it will be assumed to be relative to the current working directory.</param>
		/// <param name="AlwaysTreatSourceAsDirectory">True if we should treat the source path like a directory even if it doesn't end with a path separator</param>
		/// <returns>Converted relative path</returns>
		public static string MakePathRelativeTo(string SourcePath, string RelativeToDirectory, bool AlwaysTreatSourceAsDirectory = false)
		{
			if (String.IsNullOrEmpty(RelativeToDirectory))
			{
				// Assume CWD
				RelativeToDirectory = ".";
			}

			string AbsolutePath = SourcePath;
			if (!Path.IsPathRooted(AbsolutePath))
			{
				AbsolutePath = Path.GetFullPath(SourcePath);
			}
			bool SourcePathEndsWithDirectorySeparator = AbsolutePath.EndsWith(Path.DirectorySeparatorChar.ToString()) || AbsolutePath.EndsWith(Path.AltDirectorySeparatorChar.ToString());
			if (AlwaysTreatSourceAsDirectory && !SourcePathEndsWithDirectorySeparator)
			{
				AbsolutePath += Path.DirectorySeparatorChar;
			}

			Uri AbsolutePathUri = new Uri(AbsolutePath);

			string AbsoluteRelativeDirectory = RelativeToDirectory;
			if (!Path.IsPathRooted(AbsoluteRelativeDirectory))
			{
				AbsoluteRelativeDirectory = Path.GetFullPath(AbsoluteRelativeDirectory);
			}

			// Make sure the directory has a trailing directory separator so that the relative directory that
			// MakeRelativeUri creates doesn't include our directory -- only the directories beneath it!
			if (!AbsoluteRelativeDirectory.EndsWith(Path.DirectorySeparatorChar.ToString()) && !AbsoluteRelativeDirectory.EndsWith(Path.AltDirectorySeparatorChar.ToString()))
			{
				AbsoluteRelativeDirectory += Path.DirectorySeparatorChar;
			}

			// Convert to URI form which is where we can make the relative conversion happen
			Uri AbsoluteRelativeDirectoryUri = new Uri(AbsoluteRelativeDirectory);

			// Ask the URI system to convert to a nicely formed relative path, then convert it back to a regular path string
			Uri UriRelativePath = AbsoluteRelativeDirectoryUri.MakeRelativeUri(AbsolutePathUri);
			string RelativePath = Uri.UnescapeDataString(UriRelativePath.ToString()).Replace('/', Path.DirectorySeparatorChar);

			// If we added a directory separator character earlier on, remove it now
			if (!SourcePathEndsWithDirectorySeparator && AlwaysTreatSourceAsDirectory && RelativePath.EndsWith(Path.DirectorySeparatorChar.ToString()))
			{
				RelativePath = RelativePath.Substring(0, RelativePath.Length - 1);
			}

			// Uri.MakeRelativeUri is broken in Mono 2.x and sometimes returns broken path
			if (IsRunningOnMono)
			{
				// Check if result is correct
				string TestPath = Path.GetFullPath(Path.Combine(AbsoluteRelativeDirectory, RelativePath));
				if (TestPath != AbsolutePath)
				{
					TestPath += "/";
					if (TestPath != AbsolutePath)
					{
						// Fix the path. @todo Mac: replace this hack with something better
						RelativePath = "../" + RelativePath;
					}
				}
			}

			return RelativePath;
		}


		/// <summary>
		/// Backspaces the specified number of characters, then displays a progress percentage value to the console
		/// </summary>
		/// <param name="Numerator">Progress numerator</param>
		/// <param name="Denominator">Progress denominator</param>
		/// <param name="NumCharsToBackspaceOver">Number of characters to backspace before writing the text.  This value will be updated with the length of the new progress string.  The first time progress is displayed, you should pass 0 for this value.</param>
		public static void DisplayProgress(int Numerator, int Denominator, ref int NumCharsToBackspaceOver)
		{
			// Backspace over previous progress value
			while (NumCharsToBackspaceOver-- > 0)
			{
				Console.Write("\b");
			}

			// Display updated progress string and keep track of how long it was
			float ProgressValue = Denominator > 0 ? ((float)Numerator / (float)Denominator) : 1.0f;
			string ProgressString = String.Format("{0}%", Math.Round(ProgressValue * 100.0f));
			NumCharsToBackspaceOver = ProgressString.Length;
			Console.Write(ProgressString);
		}


		/*
		 * Read and write classes with xml specifiers
		 */
		static private void UnknownAttributeDelegate(object sender, XmlAttributeEventArgs e)
		{
		}

		static private void UnknownNodeDelegate(object sender, XmlNodeEventArgs e)
		{
		}

		static public T ReadClass<T>(string FileName) where T : new()
		{
			T Instance = new T();
			StreamReader XmlStream = null;
			try
			{
				// Get the XML data stream to read from
				XmlStream = new StreamReader(FileName);

				// Creates an instance of the XmlSerializer class so we can read the settings object
				XmlSerializer Serialiser = new XmlSerializer(typeof(T));
				// Add our callbacks for unknown nodes and attributes
				Serialiser.UnknownNode += new XmlNodeEventHandler(UnknownNodeDelegate);
				Serialiser.UnknownAttribute += new XmlAttributeEventHandler(UnknownAttributeDelegate);

				// Create an object graph from the XML data
				Instance = (T)Serialiser.Deserialize(XmlStream);
			}
			catch (Exception E)
			{
				Log.TraceInformation(E.Message);
			}
			finally
			{
				if (XmlStream != null)
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

			return Instance;
		}

		static public bool WriteClass<T>(T Data, string FileName, string DefaultNameSpace)
		{
			bool bSuccess = true;
			StreamWriter XmlStream = null;
			try
			{
				FileInfo Info = new FileInfo(FileName);
				if (Info.Exists)
				{
					Info.IsReadOnly = false;
				}

				// Make sure the output directory exists
				Directory.CreateDirectory(Path.GetDirectoryName(FileName));

				XmlSerializerNamespaces EmptyNameSpace = new XmlSerializerNamespaces();
				EmptyNameSpace.Add("", DefaultNameSpace);

				XmlStream = new StreamWriter(FileName, false, Encoding.Unicode);
				XmlSerializer Serialiser = new XmlSerializer(typeof(T));

				// Add our callbacks for unknown nodes and attributes
				Serialiser.UnknownNode += new XmlNodeEventHandler(UnknownNodeDelegate);
				Serialiser.UnknownAttribute += new XmlAttributeEventHandler(UnknownAttributeDelegate);

				Serialiser.Serialize(XmlStream, Data, EmptyNameSpace);
			}
			catch (Exception E)
			{
				Log.TraceInformation(E.Message);
				bSuccess = false;
			}
			finally
			{
				if (XmlStream != null)
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

			return (bSuccess);
		}

		/// <summary>
		/// Returns true if the specified Process has been created, started and remains valid (i.e. running).
		/// </summary>
		/// <param name="p">Process object to test</param>
		/// <returns>True if valid, false otherwise.</returns>
		public static bool IsValidProcess(Process p)
		{
			// null objects are always invalid
			if (p == null)
				return false;
			// due to multithreading on Windows, lock the object
			lock (p)
			{
				// Mono has a specific requirement if testing for an alive process
				if (IsRunningOnMono)
					return p.Handle != IntPtr.Zero; // native handle to the process
				// on Windows, simply test the process ID to be non-zero. 
				// note that this can fail and have a race condition in threads, but the framework throws an exception when this occurs.
				try
				{
					return p.Id != 0;
				}
				catch { } // all exceptions can be safely caught and ignored, meaning the process is not started or has stopped.
			}
			return false;
		}

		/// <summary>
		/// Removes multi-dot extensions from a filename (i.e. *.automation.csproj)
		/// </summary>
		/// <param name="Filename">Filename to remove the extensions from</param>
		/// <returns>Clean filename.</returns>
		public static string GetFilenameWithoutAnyExtensions(string Filename)
		{
			Filename = Path.GetFileName(Filename);

			int DotIndex = Filename.IndexOf('.');
			if (DotIndex == -1)
			{
				return Filename; // No need to copy string
			}
			else
			{
				return Filename.Substring(0, DotIndex);
			}
		}

		/// <summary>
		/// Returns Filename with path but without extension.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>Path to the file with its extension removed.</returns>
		public static string GetPathWithoutExtension(string Filename)
		{
			if (!String.IsNullOrEmpty(Path.GetExtension(Filename)))
			{
				return Path.Combine(Path.GetDirectoryName(Filename), Path.GetFileNameWithoutExtension(Filename));
			}
			else
			{
				return Filename;
			}
		}


		/// <summary>
		/// Returns true if the specified file's path is located under the specified directory, or any of that directory's sub-folders.  Does not care whether the file or directory exist or not.  This is a simple string-based check.
		/// </summary>
		/// <param name="FilePath">The path to the file</param>
		/// <param name="Directory">The directory to check to see if the file is located under (or any of this directory's subfolders)</param>
		/// <returns></returns>
		public static bool IsFileUnderDirectory(string FilePath, string Directory)
		{
			string DirectoryPathPlusSeparator = Path.GetFullPath(Directory);
			if (!DirectoryPathPlusSeparator.EndsWith(Path.DirectorySeparatorChar.ToString()))
			{
				DirectoryPathPlusSeparator += Path.DirectorySeparatorChar;
			}
			return Path.GetFullPath(FilePath).StartsWith(DirectoryPathPlusSeparator, StringComparison.InvariantCultureIgnoreCase);
		}


		/// <summary>
		/// Given a path to a file, strips off the base directory part of the path
		/// </summary>
		/// <param name="FilePath">The full path</param>
		/// <param name="BaseDirectory">The base directory, which must be the first part of the path</param>
		/// <returns>The part of the path after the base directory</returns>
		public static string StripBaseDirectory(string FilePath, string BaseDirectory)
		{
			return CleanDirectorySeparators(FilePath).Substring(CleanDirectorySeparators(BaseDirectory).Length + 1);
		}


		/// <summary>
		/// Given a path to a "source" file, re-roots the file path to be located under the "destination" folder.  The part of the source file's path after the root folder is unchanged.
		/// </summary>
		/// <param name="FilePath"></param>
		/// <param name="BaseDirectory"></param>
		/// <param name="NewBaseDirectory"></param>
		/// <returns></returns>
		public static string MakeRerootedFilePath(string FilePath, string BaseDirectory, string NewBaseDirectory)
		{
			string RelativeFile = StripBaseDirectory(FilePath, BaseDirectory);
			string DestFile = Path.Combine(NewBaseDirectory, RelativeFile);
			return DestFile;
		}


		/// <summary>
		/// Correctly collapses any ../ or ./ entries in a path.
		/// </summary>
		/// <param name="InPath">The path to be collapsed</param>
		/// <returns>true if the path could be collapsed, false otherwise.</returns>
		public static bool CollapseRelativeDirectories(ref string InPath)
		{
			string LocalString = InPath;
			bool bHadBackSlashes = false;
			// look to see what kind of slashes we had
			if (LocalString.IndexOf("\\") != -1)
			{
				LocalString = LocalString.Replace("\\", "/");
				bHadBackSlashes = true;
			}

			string ParentDir = "/..";
			int ParentDirLength = ParentDir.Length;

			for (; ; )
			{
				// An empty path is finished
				if (string.IsNullOrEmpty(LocalString))
					break;

				// Consider empty paths or paths which start with .. or /.. as invalid
				if (LocalString.StartsWith("..") || LocalString.StartsWith(ParentDir))
					return false;

				// If there are no "/.."s left then we're done
				int Index = LocalString.IndexOf(ParentDir);
				if (Index == -1)
					break;

				int PreviousSeparatorIndex = Index;
				for (; ; )
				{
					// Find the previous slash
					PreviousSeparatorIndex = Math.Max(0, LocalString.LastIndexOf("/", PreviousSeparatorIndex - 1));

					// Stop if we've hit the start of the string
					if (PreviousSeparatorIndex == 0)
						break;

					// Stop if we've found a directory that isn't "/./"
					if ((Index - PreviousSeparatorIndex) > 1 && (LocalString[PreviousSeparatorIndex + 1] != '.' || LocalString[PreviousSeparatorIndex + 2] != '/'))
						break;
				}

				// If we're attempting to remove the drive letter, that's illegal
				int Colon = LocalString.IndexOf(":", PreviousSeparatorIndex);
				if (Colon >= 0 && Colon < Index)
					return false;

				LocalString = LocalString.Substring(0, PreviousSeparatorIndex) + LocalString.Substring(Index + ParentDirLength);
			}

			LocalString = LocalString.Replace("./", "");

			// restore back slashes now
			if (bHadBackSlashes)
			{
				LocalString = LocalString.Replace("/", "\\");
			}

			// and pass back out
			InPath = LocalString;
			return true;
		}

		/// <summary>
		/// Checks if given type implements given interface.
		/// </summary>
		/// <typeparam name="InterfaceType">Interface to check.</typeparam>
		/// <param name="TestType">Type to check.</param>
		/// <returns>True if TestType implements InterfaceType. False otherwise.</returns>
		public static bool ImplementsInterface<InterfaceType>(Type TestType)
		{
			return Array.IndexOf(TestType.GetInterfaces(), typeof(InterfaceType)) != -1;
		}

		/// <summary>
		/// Returns the User Settings Directory path. This matches FPlatformProcess::UserSettingsDir().
		/// NOTE: This function may return null. Some accounts (eg. the SYSTEM account on Windows) do not have a personal folder, and Jenkins
		/// runs using this account by default.
		/// </summary>
		public static DirectoryReference GetUserSettingDirectory()
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Library", "Application Support", "Epic"));
			}
			else if (Environment.OSVersion.Platform == PlatformID.Unix)
			{
				return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Epic"));
			}
			else
			{
				// Not all user accounts have a local application data directory (eg. SYSTEM, used by Jenkins for builds).
				string DirectoryName = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
				if(String.IsNullOrEmpty(DirectoryName))
				{
					return null;
				}
				else
				{
					return new DirectoryReference(DirectoryName);
				}
			}
		}

		/// <summary>
		/// Helper function to get the minimum of two comparable objects
		/// </summary>
		public static T Min<T>(T a, T b) where T : IComparable
		{
			return a.CompareTo(b) < 0 ? a : b;
		}

		/// <summary>
		/// Helper function to get the maximum of two comparable objects
		/// </summary>
		public static T Max<T>(T a, T b) where T : IComparable
		{
			return a.CompareTo(b) > 0 ? a : b;
		}

		/// <summary>
		/// Gets the number of physical cores, excluding hyper threading.
		/// </summary>
		/// <param name="NumCores">The number of physical cores, excluding hyper threading</param>
		/// <returns>The number of physical cores, or -1 if it could not be obtained</returns>
		public static int GetPhysicalProcessorCount()
		{
			// Can't use WMI queries on Mono; just fail.
			if (Utils.IsRunningOnMono)
			{
				return -1;
			}

			// On some systems this requires a hot fix to work so catch any exceptions
			try
			{
				int NumCores = 0;
				using (ManagementObjectSearcher Mos = new System.Management.ManagementObjectSearcher("Select * from Win32_Processor"))
				{
					ManagementObjectCollection MosCollection = Mos.Get();
					foreach (ManagementBaseObject Item in MosCollection)
					{
						NumCores += int.Parse(Item["NumberOfCores"].ToString());
					}
				}
				return NumCores;
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to get the number of Cores: {0}", Ex.ToString());
				Log.TraceWarning("Falling back to processor count.");
				return -1;
			}
		}
	}

	/// <summary>
	/// Class to display an incrementing progress percentage. Handles progress markup and direct console output.
	/// </summary>
	public class ProgressWriter : IDisposable
	{
		public static bool bWriteMarkup = false;

		bool bWriteToConsole;
		string Message;
		int NumCharsToBackspaceOver;
		string CurrentProgressString;

		public ProgressWriter(string InMessage, bool bInWriteToConsole)
		{
			Message = InMessage;
			bWriteToConsole = bInWriteToConsole;
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.Write(Message + " ");
			}
			Write(0, 100);
		}

		public void Dispose()
		{
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.WriteLine();
			}
		}

		public void Write(int Numerator, int Denominator)
		{
			float ProgressValue = Denominator > 0 ? ((float)Numerator / (float)Denominator) : 1.0f;
			string ProgressString = String.Format("{0}%", Math.Round(ProgressValue * 100.0f));
			if (ProgressString != CurrentProgressString)
			{
				CurrentProgressString = ProgressString;
				if (bWriteMarkup)
				{
					Log.WriteLine(LogEventType.Console, "@progress '{0}' {1}", Message, ProgressString);
				}
				else if (bWriteToConsole)
				{
					// Backspace over previous progress value
					while (NumCharsToBackspaceOver-- > 0)
					{
						Console.Write("\b");
					}

					// Display updated progress string and keep track of how long it was
					NumCharsToBackspaceOver = ProgressString.Length;
					Console.Write(ProgressString);
				}
			}
		}
	}

	/// <summary>
	/// UAT/UBT Custom log system.
	/// 
	/// This lets you use any TraceListeners you want, but you should only call the static 
	/// methods below, not call Trace.XXX directly, as the static methods
	/// This allows the system to enforce the formatting and filtering conventions we desire.
	///
	/// For posterity, we cannot use the Trace or TraceSource class directly because of our special log requirements:
	///   1. We possibly capture the method name of the logging event. This cannot be done as a macro, so must be done at the top level so we know how many layers of the stack to peel off to get the real function.
	///   2. We have a verbose filter we would like to apply to all logs without having to have each listener filter individually, which would require our string formatting code to run every time.
	///   3. We possibly want to ensure severity prefixes are logged, but Trace.WriteXXX does not allow any severity info to be passed down.
	/// </summary>
	static public class Log
	{
		/// <summary>
		/// Guard our initialization. Mainly used by top level exception handlers to ensure its safe to call a logging function.
		/// In general user code should not concern itself checking for this.
		/// </summary>
		private static bool bIsInitialized = false;
		/// <summary>
		/// When true, verbose loggin is enabled.
		/// </summary>
		private static LogEventType LogLevel = LogEventType.Log;
		/// <summary>
		/// When true, warnings and errors will have a WARNING: or ERROR: prexifx, respectively.
		/// </summary>
		private static bool bLogSeverity = false;
		/// <summary>
		/// When true, logs will have the calling mehod prepended to the output as MethodName:
		/// </summary>
		private static bool bLogSources = false;
		/// <summary>
		/// When true, will detect warnings and errors and set the console output color to yellow and red.
		/// </summary>
		private static bool bColorConsoleOutput = false;
		/// <summary>
		/// When configured, this tracks time since initialization to prepend a timestamp to each log.
		/// </summary>
		private static Stopwatch Timer;

		/// <summary>
		/// Expose the log level. This is a hack for ProcessResult.LogOutput, which wants to bypass our normal formatting scheme.
		/// </summary>
		public static bool bIsVerbose { get { return LogLevel >= LogEventType.Verbose; } }

		/// <summary>
		/// A collection of strings that have been already written once
		/// </summary>
		private static List<string> WriteOnceSet = new List<string>();

		/// <summary>
		/// Allows code to check if the log system is ready yet.
		/// End users should NOT need to use this. It pretty much exists
		/// to work around startup issues since this is a global singleton.
		/// </summary>
		/// <returns></returns>
		public static bool IsInitialized()
		{
			return bIsInitialized;
		}

		/// <summary>
		/// Allows us to change verbosity after initializing. This can happen since we initialize logging early, 
		/// but then read the config and command line later, which could change this value.
		/// </summary>
		/// <param name="bLogVerbose">Whether to log verbose logs.</param>
		public static void SetLoggingLevel(LogEventType InLogLevel)
		{
			Log.LogLevel = InLogLevel;
		}

		/// <summary>
		/// This class allows InitLogging to be called more than once to work around chicken and eggs issues with logging and parsing command lines (see UBT startup code).
		/// </summary>
		/// <param name="bLogTimestamps">If true, the timestamp from Log init time will be prepended to all logs.</param>
		/// <param name="bLogVerbose">If true, any Verbose log method method will not be ignored.</param>
		/// <param name="bLogSeverity">If true, warnings and errors will have a WARNING: and ERROR: prefix to them. </param>
		/// <param name="bLogSources">If true, logs will have the originating method name prepended to them.</param>
		/// <param name="TraceListeners">Collection of trace listeners to attach to the Trace.Listeners, in addition to the Default listener. The existing listeners (except the Default listener) are cleared first.</param>
		public static void InitLogging(bool bLogTimestamps, LogEventType InLogLevel, bool bLogSeverity, bool bLogSources, bool bColorConsoleOutput, IEnumerable<TraceListener> TraceListeners)
		{
			bIsInitialized = true;
			Timer = (bLogTimestamps && Timer == null) ? Stopwatch.StartNew() : null;
			Log.LogLevel = InLogLevel;
			Log.bLogSeverity = bLogSeverity;
			Log.bLogSources = bLogSources;
			Log.bColorConsoleOutput = bColorConsoleOutput;

			// ensure that if InitLogging is called more than once we don't stack listeners.
			// but always leave the default listener around.
			for (int ListenerNdx = 0; ListenerNdx < Trace.Listeners.Count; )
			{
				if (Trace.Listeners[ListenerNdx].GetType() != typeof(DefaultTraceListener))
				{
					Trace.Listeners.RemoveAt(ListenerNdx);
				}
				else
				{
					++ListenerNdx;
				}
			}
			// don't add any null listeners
			Trace.Listeners.AddRange(TraceListeners.Where(l => l != null).ToArray());
			Trace.AutoFlush = true;
		}

		/// <summary>
		/// Gets the name of the Method N levels deep in the stack frame. Used to trap what method actually made the logging call.
		/// Only used when bLogSources is true.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <returns>ClassName.MethodName</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static string GetSource(int StackFramesToSkip)
		{
			StackFrame Frame = new StackFrame(2 + StackFramesToSkip);
			System.Reflection.MethodBase Method = Frame.GetMethod();
			return String.Format("{0}.{1}", Method.DeclaringType.Name, Method.Name);
		}

		/// <summary>
		/// Converts a LogEventType into a log prefix. Only used when bLogSeverity is true.
		/// </summary>
		/// <param name="EventType"></param>
		/// <returns></returns>
		private static string GetSeverityPrefix(LogEventType Severity)
		{
			switch (Severity)
			{
				case LogEventType.Fatal:
					return "FATAL: ";
				case LogEventType.Error:
					return "ERROR: ";
				case LogEventType.Warning:
					return "WARNING: ";
				case LogEventType.Console:
					return "";
				case LogEventType.Verbose:
					return "VERBOSE: ";
				case LogEventType.VeryVerbose:
					return "VVERBOSE: ";
				default:
					return " ";
			}
		}

		/// <summary>
		/// Converts a LogEventType into a message code
		/// </summary>
		/// <param name="EventType"></param>
		/// <returns></returns>
		private static int GetMessageCode(LogEventType Severity)
		{
			return (int)Severity;
		}

		/// <summary>
		/// Formats message for logging. Enforces the configured options.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="CustomSource">Custom source string to use. Use the Class.Method string if null. Only used if bLogSources = true.</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Format">Message text format string</param>
		/// <param name="Args">Message text parameters</param>
		/// <returns>Formatted message</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static string FormatMessage(int StackFramesToSkip, string CustomSource, LogEventType Verbosity, string Format, params object[] Args)
		{
			return string.Format("{0}{1}{2}{3}",
					Timer != null ? String.Format("[{0:hh\\:mm\\:ss\\.fff}] ", Timer.Elapsed) : "",
					bLogSources ? string.Format("{0}", CustomSource == null ? GetSource(StackFramesToSkip) + ": ": CustomSource == string.Empty ? "" : CustomSource + ": ") : "",
					bLogSeverity ? GetSeverityPrefix(Verbosity) : "",
				// If there are no extra args, don't try to format the string, in case it has any format control characters in it (our LOCTEXT strings tend to).
					Args.Length > 0 ? string.Format(Format, Args) : Format);
		}

		/// <summary>
		/// Writes a formatted message to the console. All other functions should boil down to calling this method.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="CustomSource">Custom source string to use. Use the default if null.</param>
		/// <param name="bWriteOnce">If true, this message will be written only once</param>
		/// <param name="Verbosity">Message verbosity level. We only meaningfully use values up to Verbose</param>
		/// <param name="Format">Message format string.</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static void WriteLinePrivate(int StackFramesToSkip, string CustomSource, bool bWriteOnce, LogEventType Verbosity, string Format, params object[] Args)
		{
			if (!bIsInitialized)
			{
				throw new BuildException("Tried to using Logging system before it was ready");
			}

			// if we want this message only written one time, check if it was already written out
			if (bWriteOnce)
			{
				string Formatted = string.Format(Format, Args);
				if (WriteOnceSet.Contains(Formatted))
				{
					return;
				}

				WriteOnceSet.Add(Formatted);
			}

			if (Verbosity <= LogLevel)
			{
				// Do console color highlighting here.
				ConsoleColor DefaultColor = ConsoleColor.Gray;
				bool bIsWarning = false;
				bool bIsError = false;
				// don't try to touch the console unless we are told to color the output.
				if (bColorConsoleOutput)
				{
					DefaultColor = Console.ForegroundColor;
					bIsWarning = Verbosity == LogEventType.Warning;
					bIsError = Verbosity <= LogEventType.Error;
					// @todo mono - mono doesn't seem to initialize the ForegroundColor properly, so we can't restore it properly.
					// Avoid touching the console color unless we really need to.
					if (bIsWarning || bIsError)
					{
						Console.ForegroundColor = bIsWarning ? ConsoleColor.Yellow : ConsoleColor.Red;
					}
				}
				try
				{
					// @todo mono: mono has some kind of bug where calling mono recursively by spawning
					// a new process causes Trace.WriteLine to stop functioning (it returns, but does nothing for some reason).
					// work around this by simulating Trace.WriteLine on mono.
					// We use UAT to spawn UBT instances recursively a lot, so this bug can effectively
					// make all build output disappear outside of the top level UAT process.
					//					#if MONO
					lock (((System.Collections.ICollection)Trace.Listeners).SyncRoot)
					{
						foreach (TraceListener l in Trace.Listeners)
						{
							if (Verbosity != LogEventType.Log || (l as ConsoleTraceListener) == null || LogLevel >= LogEventType.Verbose)
							{
								l.WriteLine(FormatMessage(StackFramesToSkip + 1, CustomSource, Verbosity, Format, Args));
							}
							l.Flush();
						}
					}
					//					#else
					// Call Trace directly here. Trace ensures that our logging is threadsafe using the GlobalLock.
					//                    	Trace.WriteLine(FormatMessage(StackFramesToSkip + 1, CustomSource, Verbosity, Format, Args));
					//					#endif
				}
				finally
				{
					// make sure we always put the console color back.
					if (bColorConsoleOutput && (bIsWarning || bIsError))
					{
						Console.ForegroundColor = DefaultColor;
					}
				}
			}
		}

		/// <summary>
		/// Similar to Trace.WriteLineIf
		/// </summary>
		/// <param name="Condition"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineIf(bool Condition, LogEventType Verbosity, string Format, params object[] Args)
		{
			if (Condition)
			{
				WriteLinePrivate(1, null, false, Verbosity, Format, Args);
			}
		}

		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip to allow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="CustomSource">Custom source string to use. Use the default if null.</param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, null, false, Verbosity, Format, Args);
		}

		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip and a custom Source to alow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="CustomSource">Custom source string to use. Use the default if null.</param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, string CustomSource, LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, CustomSource, false, Verbosity, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLin
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, Verbosity, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.Error, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.Verbose, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformation(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.Console, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.Warning, Format, Args);
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.VeryVerbose, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLog(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, false, LogEventType.Log, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLin
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineOnce(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, Verbosity, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceErrorOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.Error, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.Verbose, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformationOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.Console, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarningOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.Warning, Format, Args);
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.VeryVerbose, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLogOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, null, true, LogEventType.Log, Format, Args);
		}
	}

	#region StreamUtils
	public static class StreamUtils
	{
		/// <summary>
		/// Read a stream into another, buffering in 4K chunks.
		/// </summary>
		/// <param name="output">this</param>
		/// <param name="input">the Stream to read from</param>
		/// <returns>same stream for expression chaining.</returns>
		public static Stream ReadFrom(this Stream output, Stream input)
		{
			long bytesRead;
			return output.ReadFrom(input, out bytesRead);
		}

		/// <summary>
		/// Read a stream into another, buffering in 4K chunks.
		/// </summary>
		/// <param name="output">this</param>
		/// <param name="input">the Stream to read from</param>
		/// <param name="totalBytesRead">returns bytes read</param>
		/// <returns>same stream for expression chaining.</returns>
		public static Stream ReadFrom(this Stream output, Stream input, out long totalBytesRead)
		{
			totalBytesRead = 0;
			const int BytesToRead = 4096;
			byte[] buf = new byte[BytesToRead];
			int bytesReadThisTime = 0;
			do
			{
				bytesReadThisTime = input.Read(buf, 0, BytesToRead);
				totalBytesRead += bytesReadThisTime;
				output.Write(buf, 0, bytesReadThisTime);
			} while (bytesReadThisTime != 0);
			return output;
		}

		/// <summary>
		/// Read stream into a new MemoryStream. Useful for chaining together expressions.
		/// </summary>
		/// <param name="input">Stream to read from.</param>
		/// <returns>memory stream that contains the stream contents.</returns>
		public static MemoryStream ReadIntoMemoryStream(this Stream input)
		{
			MemoryStream data = new MemoryStream(4096);
			data.ReadFrom(input);
			return data;
		}

		/// <summary>
		/// Writes the entire contents of a byte array to the stream.
		/// </summary>
		/// <param name="stream"></param>
		/// <param name="arr"></param>
		/// <returns></returns>
		public static Stream Write(this Stream stream, byte[] arr)
		{
			stream.Write(arr, 0, arr.Length);
			return stream;
		}
	}
	#endregion

	#region HashCodeHelper
	/// <summary>
	/// Helper for generating hashcodes for value types.
	/// </summary>
	public static class HashCodeHelper
	{
		private const int BasePrimeNumber = 691;
		private const int FieldMutatorPrimeNumber = 397;

		/// <summary>
		/// Helper to compute a reasonable hash code from a group of objects, typically base types, but could be anything.
		/// </summary>
		/// <typeparam name="T1"></typeparam>
		/// <param name="Value1"></param>
		/// <returns></returns>
		public static int GetHashCodeFromValues<T1>(T1 Value1)
		{
			int Result = BasePrimeNumber;
			if (Value1 != null)
			{
				Result *= Value1.GetHashCode() + FieldMutatorPrimeNumber;
			}
			return Result;
		}

		/// <summary>
		/// Helper to compute a reasonable hash code from a group of objects, typically base types, but could be anything.
		/// </summary>
		/// <typeparam name="T1"></typeparam>
		/// <typeparam name="T2"></typeparam>
		/// <param name="Value1"></param>
		/// <param name="Value2"></param>
		/// <returns></returns>
		public static int GetHashCodeFromValues<T1, T2>(T1 Value1, T2 Value2)
		{
			int Result = BasePrimeNumber;
			if (Value1 != null)
			{
				Result *= Value1.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value2 != null)
			{
				Result *= Value2.GetHashCode() + FieldMutatorPrimeNumber;
			}
			return Result;
		}

		/// <summary>
		/// Helper to compute a reasonable hash code from a group of objects, typically base types, but could be anything.
		/// </summary>
		/// <typeparam name="T1"></typeparam>
		/// <typeparam name="T2"></typeparam>
		/// <typeparam name="T3"></typeparam>
		/// <param name="Value1"></param>
		/// <param name="Value2"></param>
		/// <param name="Value3"></param>
		/// <returns></returns>
		public static int GetHashCodeFromValues<T1, T2, T3>(T1 Value1, T2 Value2, T3 Value3)
		{
			int Result = BasePrimeNumber;
			if (Value1 != null)
			{
				Result *= Value1.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value2 != null)
			{
				Result *= Value2.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value3 != null)
			{
				Result *= Value3.GetHashCode() + FieldMutatorPrimeNumber;
			}
			return Result;
		}

		/// <summary>
		/// Helper to compute a reasonable hash code from a group of objects, typically base types, but could be anything.
		/// </summary>
		/// <typeparam name="T1"></typeparam>
		/// <typeparam name="T2"></typeparam>
		/// <typeparam name="T3"></typeparam>
		/// <typeparam name="T4"></typeparam>
		/// <param name="Value1"></param>
		/// <param name="Value2"></param>
		/// <param name="Value3"></param>
		/// <param name="Value4"></param>
		/// <returns></returns>
		public static int GetHashCodeFromValues<T1, T2, T3, T4>(T1 Value1, T2 Value2, T3 Value3, T4 Value4)
		{
			int Result = BasePrimeNumber;
			if (Value1 != null)
			{
				Result *= Value1.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value2 != null)
			{
				Result *= Value2.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value3 != null)
			{
				Result *= Value3.GetHashCode() + FieldMutatorPrimeNumber;
			}
			if (Value4 != null)
			{
				Result *= Value4.GetHashCode() + FieldMutatorPrimeNumber;
			}
			return Result;
		}

		/// <summary>
		/// Helper to compute a reasonable hash code from a group of objects, typically base types, but could be anything.
		/// </summary>
		/// <param name="values"></param>
		/// <returns></returns>
		public static int GetHashCodeFromValues(params object[] values)
		{
			// prevent arithmetic overflow exceptions
			unchecked
			{
				return values.Where(elt => elt != null).Aggregate(BasePrimeNumber, (result, rhs) => result * (FieldMutatorPrimeNumber + rhs.GetHashCode()));
			}
		}
	}
	#endregion

	#region Scoped Timers

	/// <summary>
	/// Scoped timer, start is in the constructor, end in Dispose. Best used with using(var Timer = new ScopedTimer()). Suports nesting.
	/// </summary>
	public class ScopedTimer : IDisposable
	{
		DateTime StartTime;
		string TimerName;
		LogEventType Verbosity;
		static int Indent = 0;
		static object IndentLock = new object();

		public ScopedTimer(string Name, LogEventType InVerbosity = LogEventType.Verbose)
		{
			TimerName = Name;
			lock (IndentLock)
			{
				Indent++;
			}
			Verbosity = InVerbosity;
			StartTime = DateTime.UtcNow;
		}

		public void Dispose()
		{
			double TotalSeconds = (DateTime.UtcNow - StartTime).TotalSeconds;
			int LogIndent = 0;
			lock (IndentLock)
			{
				LogIndent = --Indent;
			}
			StringBuilder IndentText = new StringBuilder(LogIndent * 2);
			IndentText.Append(' ', LogIndent * 2);

			Log.WriteLine(Verbosity, "{0}{1} took {2}s", IndentText.ToString(), TimerName, TotalSeconds);
		}
	}

	/// <summary>
	/// Scoped, accumulative timer. Can have multiple instances. Best used as a static variable with Start() and End() 
	/// </summary>
	public class ScopedCounter : IDisposable
	{
		class Accumulator
		{
			public double Time;
			public LogEventType Verbosity;
		}
		DateTime StartTime;
		Accumulator ScopedAccumulator;
		string CounterName;

		static Dictionary<string, Accumulator> Accumulators = new Dictionary<string, Accumulator>();

		public ScopedCounter(string Name, LogEventType InVerbosity = LogEventType.Verbose)
		{
			CounterName = Name;
			if (!Accumulators.TryGetValue(CounterName, out ScopedAccumulator))
			{
				ScopedAccumulator = new Accumulator();
				Accumulators.Add(CounterName, ScopedAccumulator);
			}
			ScopedAccumulator.Verbosity = InVerbosity;
			StartTime = DateTime.UtcNow;
		}

		/// <summary>
		/// (Re-)Starts time measuring
		/// </summary>
		/// <returns></returns>
		public ScopedCounter Start()
		{
			StartTime = DateTime.UtcNow;
			return this;
		}

		/// <summary>
		/// Ends measuring time
		/// </summary>
		public void Dispose()
		{
			double TotalSeconds = (DateTime.UtcNow - StartTime).TotalSeconds;
			ScopedAccumulator.Time += TotalSeconds;
		}

		/// <summary>
		/// Logs the specified counters timings to the output
		/// </summary>
		/// <param name="Name">Timer name</param>
		static public void LogCounter(string Name)
		{
			Accumulator ExistingAccumulator;
			if (Accumulators.TryGetValue(Name, out ExistingAccumulator))
			{
				LogAccumulator(Name, ExistingAccumulator);
			}
			else
			{
				throw new Exception(String.Format("ScopedCounter {0} does not exist!", Name));
			}
		}

		static private void LogAccumulator(string Name, Accumulator Counter)
		{
			Log.WriteLine(Counter.Verbosity, "{0} took {1}s", Name, Counter.Time);
		}

		/// <summary>
		/// Dumps all counters to the log
		/// </summary>
		static public void DumpAllCounters()
		{
			foreach (KeyValuePair<string, Accumulator> Counter in Accumulators)
			{
				LogAccumulator(Counter.Key, Counter.Value);
			}
		}
	}

	#endregion

}
