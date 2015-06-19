// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Exception when parsing ini files
	/// </summary>
	public class IniParsingException : Exception
	{
		public IniParsingException(string Message)
			: base(Message)
		{}
		public IniParsingException(string Format, params object[] Args)
			: base(String.Format(Format, Args))
		{}
	}

	/// <summary>
	/// Equivalent of FConfigCacheIni. Parses ini files.
	/// </summary>
	public class ConfigCacheIni
	{
		/// <summary>
		/// List of values (or a single value)
		/// </summary>
		public class IniValues : List<string>
		{
			public override string ToString()
			{
				return String.Join(",", ToArray());
			}
		}

		/// <summary>
		/// Ini section (map of keys and values)
		/// </summary>
		public class IniSection : Dictionary<string, IniValues>
		{
			public IniSection()
				: base(StringComparer.InvariantCultureIgnoreCase)
			{}
			public override string ToString()
			{
				return "IniSection";
			}
		}

		/// <summary>
		/// All sections parsed from ini file
		/// </summary>
		Dictionary<string, IniSection> Sections;

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified project.  No Platform settings.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		public ConfigCacheIni(string BaseIniName, string ProjectDirectory, string EngineDirectory = null)
		{
			Init(UnrealTargetPlatform.Unknown, BaseIniName, ProjectDirectory, EngineDirectory);
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified platform and project.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		public ConfigCacheIni(UnrealTargetPlatform Platform, string BaseIniName, string ProjectDirectory, string EngineDirectory = null)
		{
			Init(Platform, BaseIniName, ProjectDirectory, EngineDirectory);
		}

		private void Init(UnrealTargetPlatform Platform, string BaseIniName, string ProjectDirectory, string EngineDirectory)
		{
			Sections = new Dictionary<string, IniSection>(StringComparer.InvariantCultureIgnoreCase);
			if (String.IsNullOrEmpty(EngineDirectory))
			{
				EngineDirectory = BuildConfiguration.RelativeEnginePath;
			}

			foreach (var IniFileName in EnumerateCrossPlatformIniFileNames(ProjectDirectory, EngineDirectory, Platform, BaseIniName))
			{
				if (File.Exists(IniFileName))
				{
					ParseIniFile(IniFileName);
				}
			}
		}

		/// <summary>
		/// Finds a section in INI
		/// </summary>
		/// <param name="SectionName"></param>
		/// <returns>Found section or null</returns>
		public IniSection FindSection(string SectionName)
		{
			IniSection Section;
			Sections.TryGetValue(SectionName, out Section);
			return Section;
		}

		/// <summary>
		/// Finds values associated with the specified key (does not copy the list)
		/// </summary>
		private bool GetList(string SectionName, string Key, out IniValues Value)
		{
			bool Result = false;
			var Section = FindSection(SectionName);
			Value = null;
			if (Section != null)
			{
				if (Section.TryGetValue(Key, out Value))
				{
					Result = true;
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets all values associated with the specified key
		/// </summary>
		/// <param name="SectionName">Section where the key is located</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Copy of the list containing all values associated with the specified key</param>
		/// <returns>True if the key exists</returns>
		public bool GetArray(string SectionName, string Key, out List<string> Value)
		{
			Value = null;
			IniValues ValueList;
			bool Result = GetList(SectionName, Key, out ValueList);
			if (Result)
			{
				Value = new List<string>(ValueList);
			}
			return Result;
		}

		/// <summary>
		/// Gets a single string value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetString(string SectionName, string Key, out string Value)
		{
			Value = String.Empty;
			IniValues ValueList;
			bool Result = GetList(SectionName, Key, out ValueList);
			if (Result && ValueList != null && ValueList.Count > 0)
			{
				Value = ValueList[0];
				Result = true;
			}
			else
			{
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Gets a single bool value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetBool(string SectionName, string Key, out bool Value)
		{			
			Value = false;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				// C# Boolean type expects "False" or "True" but since we're not case sensitive, we need to suppor that manually
				if (String.Compare(TextValue, "true", true) == 0 || String.Compare(TextValue, "1") == 0)
				{
					Value = true;
				}
				else if (String.Compare(TextValue, "false", true) == 0 || String.Compare(TextValue, "0") == 0)
				{
					Value = false;
				}
				else
				{
					// Failed to parse
					Result = false;
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets a single Int32 value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetInt32(string SectionName, string Key, out int Value)
		{
			Value = 0;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Int32.TryParse(TextValue, out Value);
			}
			return Result;
		}

		/// <summary>
		/// Gets a single GUID value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetGUID(string SectionName, string Key, out Guid Value)
		{
			Value = Guid.Empty;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
                string HexString = "";
                if (TextValue.Contains("A=") && TextValue.Contains("B=") && TextValue.Contains("C=") && TextValue.Contains("D="))
                {
                    char[] Separators = new char[] { '(', ')', '=', ',', ' ', 'A', 'B', 'C', 'D' };
                    string[] ComponentValues = TextValue.Split(Separators, StringSplitOptions.RemoveEmptyEntries);
                    if (ComponentValues.Length == 4)
                    {
                        for (int ComponentIndex = 0; ComponentIndex < 4; ComponentIndex++)
                        {
                            int IntegerValue;
                            Result &= Int32.TryParse(ComponentValues[ComponentIndex], out IntegerValue);
                            HexString += IntegerValue.ToString("X8");
                        }
                    }
                }
                else
                {
                    HexString = TextValue;
                }

                try
                {
                    Value = Guid.ParseExact(HexString, "N");
                    Result = true;
                }
                catch (Exception)
                {
                    Result = false;
                }
			}
			return Result;
		}

		/// <summary>
		/// Gets a single float value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetSingle(string SectionName, string Key, out float Value)
		{
			Value = 0.0f;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Single.TryParse(TextValue, out Value);
			}
			return Result;
		}

		/// <summary>
		/// Gets a single double value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetDouble(string SectionName, string Key, out double Value)
		{
			Value = 0.0;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Double.TryParse(TextValue, out Value);
			}
			return Result;
		}

        private static bool ExtractPath(string Source, out string Path)
        {
            int start = Source.IndexOf('"');
            int end = Source.LastIndexOf('"');    
            if(start != 1 && end != -1 && start < end)
            {
                ++start;
                Path = Source.Substring(start, end - start);
                return true;
            }
            else
            {
                Path = "";
            }

            return false;
        }

        public bool GetPath(string SectionName, string Key, out string Value)
        {
            string temp;
            if(GetString(SectionName, Key, out temp))
            {
                return ExtractPath(temp, out Value);
            }
            else
            {
                Value = "";
            }

            return false;
        }

		/// <summary>
		/// List of actions that can be performed on a single line from ini file
		/// </summary>
		enum ParseAction
		{
			None,
			New,
			Add,
			Remove
		}

		/// <summary>
		/// Checks what action should be performed on a single line from ini file
		/// </summary>
		private ParseAction GetActionForLine(ref string Line)
		{
			if (String.IsNullOrEmpty(Line) || Line.StartsWith(";") || Line.StartsWith("//"))
			{
				return ParseAction.None;
			}
			else if (Line.StartsWith("-"))
			{
				Line = Line.Substring(1).TrimStart();
				return ParseAction.Remove;
			}
			else if (Line.StartsWith("+"))
			{
				Line = Line.Substring(1).TrimStart();
				return ParseAction.Add;
			}
			else
			{
				return ParseAction.New;
			}
		}

		/// <summary>
		/// Loads and parses ini file.
		/// </summary>
		public void ParseIniFile(string Filename)
		{
			String[] IniLines = null;
			try
			{
				IniLines = File.ReadAllLines(Filename);
			}
			catch (Exception ex)
			{
				Console.WriteLine("Error reading ini file: " + Filename + " Exception: " + ex.Message);
			}
			if (IniLines != null)
			{
				IniSection CurrentSection = null;

				// Line Index for exceptions
				var LineIndex = 1;
				var bMultiLine = false;
				var SingleValue = "";
				var Key = "";
				var LastAction = ParseAction.None;

				// Parse each line
				foreach (var Line in IniLines)
				{
					var TrimmedLine = Line.Trim();
					// Multiline value support
					bool bWasMultiLine = bMultiLine;
					bMultiLine = TrimmedLine.EndsWith("\\");
					if (bMultiLine)
					{
						TrimmedLine = TrimmedLine.Substring(0, TrimmedLine.Length - 1).TrimEnd();
					}
					if (!bWasMultiLine)
					{
						if (TrimmedLine.StartsWith("["))
						{
							CurrentSection = FindOrAddSection(TrimmedLine, Filename, LineIndex);
							LastAction = ParseAction.None;
						}
						else
						{
							if (LastAction != ParseAction.None)
							{
								throw new IniParsingException("Parsing new key/value pair when the previous one has not yet been processed ({0}, {1}) in {2}, line {3}: {4}", Key, SingleValue, Filename, LineIndex, TrimmedLine);
							}
							// Check if the line is empty or a comment, also remove any +/- markers
							LastAction = GetActionForLine(ref TrimmedLine);
							if (LastAction != ParseAction.None)
							{
								if (CurrentSection == null)
								{
									throw new IniParsingException("Trying to parse key/value pair that doesn't belong to any section in {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
								}
								ParseKeyValuePair(TrimmedLine, Filename, LineIndex, out Key, out SingleValue);
							}
						}
					}
					if (bWasMultiLine)
					{
						SingleValue += TrimmedLine;
					}
					if (!bMultiLine && LastAction != ParseAction.None)
					{
						ProcessKeyValuePair(CurrentSection, Key, SingleValue, LastAction);
						LastAction = ParseAction.None;
						SingleValue = "";
						Key = "";
					}
					LineIndex++;
				}
			}
		}

		/// <summary>
		/// Splits a line into key and value
		/// </summary>
		private static void ParseKeyValuePair(string TrimmedLine, string Filename, int LineIndex, out string Key, out string Value)
		{			
			var AssignIndex = TrimmedLine.IndexOf('=');
			if (AssignIndex < 0)
			{
				throw new IniParsingException("Failed to find value when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			Key = TrimmedLine.Substring(0, AssignIndex).Trim();
			if (String.IsNullOrEmpty(Key))
			{
				throw new IniParsingException("Empty key when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			Value = TrimmedLine.Substring(AssignIndex + 1).Trim();
			if (Value.StartsWith("\""))
			{
				// Remove quotes
				var QuoteEnd = Value.LastIndexOf('\"');
				if (QuoteEnd == 0)
				{
					throw new IniParsingException("Mismatched quotes when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
				}
				Value = Value.Substring(1, Value.Length - 2);
			}
		}

		/// <summary>
		/// Processes parsed key/value pair
		/// </summary>
		private static void ProcessKeyValuePair(IniSection CurrentSection, string Key, string SingleValue, ParseAction Action)
		{
			switch (Action)
			{
				case ParseAction.New:
					{
						// New/replace
						IniValues Value;
						if (CurrentSection.TryGetValue(Key, out Value) == false)
						{
							Value = new IniValues();
							CurrentSection.Add(Key, Value);
						}
						Value.Clear();
						Value.Add(SingleValue);						
					}
					break;
				case ParseAction.Add:
					{
						IniValues Value;
						if (CurrentSection.TryGetValue(Key, out Value) == false)
						{
							Value = new IniValues();
							CurrentSection.Add(Key, Value);
						}
						Value.Add(SingleValue);
					}
					break;
				case ParseAction.Remove:
					{
						IniValues Value;
						if (CurrentSection.TryGetValue(Key, out Value))
						{
							var ExistingIndex = Value.FindIndex(X => (String.Compare(Key, X, true) == 0));
							if (ExistingIndex >= 0)
							{
								Value.RemoveAt(ExistingIndex);
							}
						}
					}
					break;
			}
		}

		/// <summary>
		/// Finds an existing section or adds a new one
		/// </summary>
		private IniSection FindOrAddSection(string TrimmedLine, string Filename, int LineIndex)
		{
			var SectionEndIndex = TrimmedLine.IndexOf(']');
			if (SectionEndIndex != (TrimmedLine.Length - 1))
			{
				throw new IniParsingException("Mismatched brackets when parsing section name in {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			var SectionName = TrimmedLine.Substring(1, TrimmedLine.Length - 2);
			if (String.IsNullOrEmpty(SectionName))
			{
				throw new IniParsingException("Empty section name when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			IniSection CurrentSection;
			if (Sections.TryGetValue(SectionName, out CurrentSection) == false)
			{
				CurrentSection = new IniSection();
				Sections.Add(SectionName, CurrentSection);
			}			
			return CurrentSection;
		}

		/// <summary>
		/// Returns a list of INI filenames for the given project
		/// </summary>
		private static IEnumerable<string> EnumerateCrossPlatformIniFileNames(string ProjectDirectory, string EngineDirectory, UnrealTargetPlatform Platform, string BaseIniName)
		{
			// Engine/Config/Base.ini (included in every ini type, required)
			yield return Path.Combine(EngineDirectory, "Config", "Base.ini");

			// Engine/Config/Base* ini
			yield return Path.Combine(EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

			// Engine/Config/NotForLicensees/Base* ini
			yield return Path.Combine(EngineDirectory, "Config", "NotForLicensees", "Base" + BaseIniName + ".ini");

			// NOTE: 4.7: See comment in GetSourceIniHierarchyFilenames()
			// Engine/Config/NoRedist/Base* ini
			// yield return Path.Combine(EngineDirectory, "Config", "NoRedist", "Base" + BaseIniName + ".ini");

			if(!String.IsNullOrEmpty(ProjectDirectory))
			{
				// Game/Config/Default* ini
				yield return Path.Combine(ProjectDirectory, "Config", "Default" + BaseIniName + ".ini");
			
				// Game/Config/NotForLicensees/Default* ini
				yield return Path.Combine(ProjectDirectory, "Config", "NotForLicensees", "Default" + BaseIniName + ".ini");

				// Game/Config/NoRedist/Default* ini
				yield return Path.Combine(ProjectDirectory, "Config", "NoRedist", "Default" + BaseIniName + ".ini");
			}
			
			string PlatformName = GetIniPlatformName(Platform);			
			if (Platform != UnrealTargetPlatform.Unknown)
			{
				// Engine/Config/Platform/Platform* ini
				yield return Path.Combine(EngineDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");

				if(!String.IsNullOrEmpty(ProjectDirectory))
				{
					// Game/Config/Platform/Platform* ini
					yield return Path.Combine(ProjectDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");
				}
			}

			string UserSettingsFolder = Utils.GetUserSettingDirectory(); // Match FPlatformProcess::UserSettingsDir()
			string PersonalFolder = null; // Match FPlatformProcess::UserDir()
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				PersonalFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Documents");
			}
			else if (Environment.OSVersion.Platform == PlatformID.Unix)
			{
				PersonalFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Documents");
			}
			else
			{
				PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			}

			// <AppData>/UE4/EngineConfig/User* ini
			yield return Path.Combine(UserSettingsFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			// <Documents>/UE4/EngineConfig/User* ini
			yield return Path.Combine(PersonalFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			
			// Game/Config/User* ini
            if (!String.IsNullOrEmpty(ProjectDirectory))
            {
				yield return Path.Combine(ProjectDirectory, "Config", "User" + BaseIniName + ".ini");
			}
		}

		/// <summary>
		/// Returns the platform name to use as part of platform-specific config files
		/// </summary>
		private static string GetIniPlatformName(UnrealTargetPlatform TargetPlatform)
		{
			if (TargetPlatform == UnrealTargetPlatform.Win32 || TargetPlatform == UnrealTargetPlatform.Win64)
			{
				return "Windows";
			}
			else
			{
				return Enum.GetName(typeof(UnrealTargetPlatform), TargetPlatform);
			}
		}
	}
}
