// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/// <summary>
	/// Equivalent of FConfigCacheIni. Parses ini files.
	/// </summary>
	public class ConfigCacheIni
	{

		// command class for being able to create config caches over and over without needing to read the ini files
		[DebuggerDisplay("{Name}")]
		class CachedSection
		{
			public string Name;
			public List<CachedKeyValuePair> KeyValuePairs = new List<CachedKeyValuePair>();
		}

		[DebuggerDisplay("{LastAction}: {Key}={Value}")]
		class CachedKeyValuePair
		{
			public string Key;
			public string Value;
			public ParseAction LastAction;
		}

		// cached ini files
		static Dictionary<FileReference, List<CachedSection>> FileCache = new Dictionary<FileReference, List<CachedSection>>();
		static Dictionary<string, ConfigCacheIni> IniCache = new Dictionary<string, ConfigCacheIni>();
		static Dictionary<string, ConfigCacheIni> BaseIniCache = new Dictionary<string, ConfigCacheIni>();
		static List<string> RequiredSections = new List<string>(){"AppxManifest", "CommonSettings", "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "/Script/AndroidPlatformEditor.AndroidSDKSettings",
																	"/Script/BuildSettings.BuildSettings", "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "/Script/WindowsTargetPlatform.WindowsTargetSettings",
																	"/Script/UnrealEd.ProjectPackagingSettings", "/Script/PS4PlatformEditor.PS4TargetSettings", "/Script/XboxOnePlatformEditor.XboxOneTargetSettings",
																	"/Script/HTML5PlatformEditor.HTML5TargetSettings","PS4SymbolServer","/Script/EngineSettings.GeneralProjectSettings",
                                                                    "/Script/UnrealEd.ProjectPackagingSettings", "InstalledPlatforms", "OnlineSubsystemGooglePlay.Store"};

		// static creation functions for ini files
		public static ConfigCacheIni CreateConfigCacheIni(UnrealTargetPlatform Platform, string BaseIniName, DirectoryReference ProjectDirectory, DirectoryReference EngineDirectory = null)
		{
			if (EngineDirectory == null)
			{
				EngineDirectory = UnrealBuildTool.EngineDirectory;
			}

			// cache base ini for use as the seed for the rest
			if (!BaseIniCache.ContainsKey(BaseIniName))
			{
				BaseIniCache.Add(BaseIniName, new ConfigCacheIni(UnrealTargetPlatform.Unknown, BaseIniName, null, EngineDirectory, EngineOnly: true));
			}

			// build the new ini and cache it for later re-use
			ConfigCacheIni BaseCache = BaseIniCache[BaseIniName];
			string Key = GetIniPlatformName(Platform) + BaseIniName + EngineDirectory.FullName + (ProjectDirectory != null ? ProjectDirectory.FullName : "");
			if (!IniCache.ContainsKey(Key))
			{
				IniCache.Add(Key, new ConfigCacheIni(Platform, BaseIniName, ProjectDirectory, EngineDirectory, BaseCache: BaseCache));
			}
			return IniCache[Key];
		}

		/// <summary>
		/// List of values (or a single value)
		/// </summary>
		public class IniValues : List<string>
		{
			public IniValues()
			{
			}
			public IniValues(IniValues Other)
				: base(Other)
			{
			}
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
			{ }
			public IniSection(IniSection Other)
				: this()
			{
				foreach (KeyValuePair<string, IniValues> Pair in Other)
				{
					Add(Pair.Key, new IniValues(Pair.Value));
				}
			}
			public override string ToString()
			{
				return "IniSection";
			}
		}

		/// <summary>
		/// True if we are loading a hierarchy of config files that should be merged together
		/// </summary>
		bool bIsMergingConfigs;

		/// <summary>
		/// All sections parsed from ini file
		/// </summary>
		Dictionary<string, IniSection> Sections;

		/// <summary>
		/// Constructor. Parses a single ini file. No Platform settings, no engine hierarchy. Do not use this with ini files that have hierarchy!
		/// </summary>
		/// <param name="Filename">The ini file to load</param>
		public ConfigCacheIni(FileReference Filename)
		{
			Init(Filename);
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified project.  No Platform settings.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		public ConfigCacheIni(string BaseIniName, string ProjectDirectory, string EngineDirectory = null)
		{
			Init(UnrealTargetPlatform.Unknown, BaseIniName, (ProjectDirectory == null) ? null : new DirectoryReference(ProjectDirectory), (EngineDirectory == null) ? null : new DirectoryReference(EngineDirectory));
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified project.  No Platform settings.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		public ConfigCacheIni(string BaseIniName, DirectoryReference ProjectDirectory, DirectoryReference EngineDirectory = null)
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
			Init(Platform, BaseIniName, (ProjectDirectory == null) ? null : new DirectoryReference(ProjectDirectory), (EngineDirectory == null) ? null : new DirectoryReference(EngineDirectory));
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified platform and project.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		public ConfigCacheIni(UnrealTargetPlatform Platform, string BaseIniName, DirectoryReference ProjectDirectory, DirectoryReference EngineDirectory = null, bool EngineOnly = false, ConfigCacheIni BaseCache = null)
		{
			Init(Platform, BaseIniName, ProjectDirectory, EngineDirectory, EngineOnly, BaseCache);
		}

		private void InitCommon()
		{
			Sections = new Dictionary<string, IniSection>(StringComparer.InvariantCultureIgnoreCase);
		}

		private void Init(FileReference IniFileName)
		{
			InitCommon();
			bIsMergingConfigs = false;
			ParseIniFile(IniFileName);
		}

		private void Init(UnrealTargetPlatform Platform, string BaseIniName, DirectoryReference ProjectDirectory, DirectoryReference EngineDirectory, bool EngineOnly = false, ConfigCacheIni BaseCache = null)
		{
			InitCommon();
			bIsMergingConfigs = true;
			if (EngineDirectory == null)
			{
				EngineDirectory = UnrealBuildTool.EngineDirectory;
			}

			if (BaseCache != null)
			{
				foreach (KeyValuePair<string, IniSection> Pair in BaseCache.Sections)
				{
					Sections.Add(Pair.Key, new IniSection(Pair.Value));
				}
			}
			if (EngineOnly)
			{
				foreach (FileReference IniFileName in EnumerateEngineIniFileNames(EngineDirectory, BaseIniName))
				{
					if (IniFileName.Exists())
					{
						ParseIniFile(IniFileName);
					}
				}
			}
			else
			{
				foreach (FileReference IniFileName in EnumerateCrossPlatformIniFileNames(ProjectDirectory, EngineDirectory, Platform, BaseIniName, BaseCache != null))
				{
					if (IniFileName.Exists())
					{
						ParseIniFile(IniFileName);
					}
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
			if (!RequiredSections.Contains(SectionName))
			{
				throw new BuildException("Section Name - {0} - is not being cached. Add the specific section to the RequiredSections list in EngineConfiguration.cs", SectionName);
			}
			return Section;
		}

		/// <summary>
		/// Finds values associated with the specified key (does not copy the list)
		/// </summary>
		private bool GetList(string SectionName, string Key, out IniValues Value)
		{
			bool Result = false;
			IniSection Section = FindSection(SectionName);
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
			if (start != 1 && end != -1 && start < end)
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
			if (GetString(SectionName, Key, out temp))
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
				// We use Add rather than New when we're not merging config files together in order 
				// to mimic the behavior of the C++ config cache when loading a single file
				return (bIsMergingConfigs) ? ParseAction.New : ParseAction.Add;
			}
		}

		/// <summary>
		/// Loads and parses ini file.
		/// </summary>
		public void ParseIniFile(FileReference Filename)
		{
			// Read the cached sections from the ini file
			List<CachedSection> Sections;
			if (!FileCache.TryGetValue(Filename, out Sections))
			{
				try
				{
					string[] IniLines = File.ReadAllLines(Filename.FullName);
					Sections = ParseIniSections(Filename, IniLines);
				}
				catch (Exception ex)
				{
					Console.WriteLine("Error reading ini file: " + Filename + " Exception: " + ex.Message);
				}
				FileCache[Filename] = Sections;
			}

			// Apply all the cached sections
			if(Sections != null)
			{
				foreach(CachedSection Section in Sections)
				{
					IniSection CurrentSection = FindOrAddSection(Section.Name);
					foreach(CachedKeyValuePair KeyValuePair in Section.KeyValuePairs)
					{
						ProcessKeyValuePair(CurrentSection, KeyValuePair.Key, KeyValuePair.Value, KeyValuePair.LastAction);
					}
				}
			}
		}

		/// <summary>
		/// Parse an ini file into a list of replayable commands
		/// </summary>
		private List<CachedSection> ParseIniSections(FileReference Filename, string[] Lines)
		{
			List<CachedSection> Sections = new List<CachedSection>();
			CachedSection CurrentSection = null;
			for(int Idx = 0; Idx < Lines.Length; Idx++)
			{
				int LineNumber = Idx + 1;

				// Read the next line, merging multiline strings together
				string TrimmedLine = Lines[Idx].Trim();
				while(TrimmedLine.EndsWith("\\"))
				{
					TrimmedLine = TrimmedLine.Substring(0, TrimmedLine.Length - 1).TrimEnd();
					if(++Idx >= Lines.Length)
					{
						break;
					}
					TrimmedLine += Lines[Idx].Trim();
				}

				// Check it's not an empty line, or comment
				if(TrimmedLine.Length > 0 && !TrimmedLine.StartsWith(";") && !TrimmedLine.StartsWith("//"))
				{
					string SectionName;
					if (TryParseSectionName(Filename, LineNumber, TrimmedLine, out SectionName))
					{
						// Parse the new section to add key/value pairs to
						if(RequiredSections.Contains(SectionName))
						{
							CurrentSection = new CachedSection(){ Name = SectionName };
							Sections.Add(CurrentSection);
						}
						else
						{
							CurrentSection = null;
						}
					}
					else if(CurrentSection != null)
					{
						// Parse the key/value pair and add it to the current section
						CachedKeyValuePair KeyValuePair;
						if(TryParseKeyValuePair(Filename, LineNumber, TrimmedLine, out KeyValuePair))
						{
							CurrentSection.KeyValuePairs.Add(KeyValuePair);
						}
					}
				}
			}
			return Sections;
		}

		/// <summary>
		/// Parse the section name from an INI file line, and create a new section object.
		/// </summary>
		private bool TryParseSectionName(FileReference Filename, int LineNumber, string TrimmedLine, out string SectionName)
		{
			if(!TrimmedLine.StartsWith("["))
			{
				SectionName = null;
				return false;
			}

			int EndIdx = TrimmedLine.Length;
			if(TrimmedLine.EndsWith("]"))
			{
				EndIdx--;
			}
			else
			{
				Log.TraceWarning("Mismatched brackets when parsing section name in {0}, line {1}: {2}", Filename, LineNumber, TrimmedLine);
			}

			SectionName = TrimmedLine.Substring(1, EndIdx - 1).Trim();
			if(SectionName.Length == 0)
			{
				Log.TraceWarning("Empty section name when parsing {0}, line {1}: {2}", Filename, LineNumber, TrimmedLine);
			}

			return true;
		}

		/// <summary>
		/// Splits a line into key and value
		/// </summary>
		private bool TryParseKeyValuePair(FileReference Filename, int LineNumber, string TrimmedLine, out CachedKeyValuePair KeyValuePair)
		{
			// Figure out what to do with this line
			ParseAction Action = GetActionForLine(ref TrimmedLine);
			if(Action == ParseAction.None)
			{
				KeyValuePair = null;
				return false;
			}

			// Parse a key value pair
			int EqualsIdx = TrimmedLine.IndexOf('=');
			if (EqualsIdx < 0)
			{
				Log.TraceWarning("Failed to find value when parsing {0}, line {1}: {2}", Filename, LineNumber, TrimmedLine);
				KeyValuePair = null;
				return false;
			}
			string Key = TrimmedLine.Substring(0, EqualsIdx).TrimEnd();
			if (Key.Length == 0)
			{
				Log.TraceWarning("Empty key when parsing {0}, line {1}: {2}", Filename, LineNumber, TrimmedLine);
				KeyValuePair = null;
				return false;
			}

			string Value = TrimmedLine.Substring(EqualsIdx + 1).TrimStart();
			if (Value.Length >= 2 && Value.StartsWith("\"") && Value.EndsWith("\""))
			{
				Value = Value.Substring(1, Value.Length - 2);
			}

			// Create the new command
			KeyValuePair = new CachedKeyValuePair(){ Key = Key, Value = Value, LastAction = Action };
			return true;
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
							int ExistingIndex = Value.FindIndex(X => (String.Compare(SingleValue, X, true) == 0));
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
		private IniSection FindOrAddSection(string SectionName)
		{
			IniSection CurrentSection;
			if (Sections.TryGetValue(SectionName, out CurrentSection) == false)
			{
				CurrentSection = new IniSection();
				Sections.Add(SectionName, CurrentSection);
			}
			return CurrentSection;
		}

		/// <summary>
		/// Returns a list of INI filenames for the engine
		/// </summary>
		private static IEnumerable<FileReference> EnumerateEngineIniFileNames(DirectoryReference EngineDirectory, string BaseIniName)
		{
			// Engine/Config/Base.ini (included in every ini type, required)
			yield return FileReference.Combine(EngineDirectory, "Config", "Base.ini");

			// Engine/Config/Base* ini
			yield return FileReference.Combine(EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

			// Engine/Config/NotForLicensees/Base* ini
			yield return FileReference.Combine(EngineDirectory, "Config", "NotForLicensees", "Base" + BaseIniName + ".ini");
		}


		/// <summary>
		/// Returns a list of INI filenames for the given project
		/// </summary>
		private static IEnumerable<FileReference> EnumerateCrossPlatformIniFileNames(DirectoryReference ProjectDirectory, DirectoryReference EngineDirectory, UnrealTargetPlatform Platform, string BaseIniName, bool SkipEngine)
		{
			if (!SkipEngine)
			{
				// Engine/Config/Base.ini (included in every ini type, required)
				yield return FileReference.Combine(EngineDirectory, "Config", "Base.ini");

				// Engine/Config/Base* ini
				yield return FileReference.Combine(EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

				// Engine/Config/NotForLicensees/Base* ini
				yield return FileReference.Combine(EngineDirectory, "Config", "NotForLicensees", "Base" + BaseIniName + ".ini");

				// NOTE: 4.7: See comment in GetSourceIniHierarchyFilenames()
				// Engine/Config/NoRedist/Base* ini
				// yield return Path.Combine(EngineDirectory, "Config", "NoRedist", "Base" + BaseIniName + ".ini");
			}

			if (ProjectDirectory != null)
			{
				// Game/Config/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Config", "Default" + BaseIniName + ".ini");

				// Game/Config/NotForLicensees/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Config", "NotForLicensees", "Default" + BaseIniName + ".ini");

				// Game/Config/NoRedist/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Config", "NoRedist", "Default" + BaseIniName + ".ini");
			}

			string PlatformName = GetIniPlatformName(Platform);
			if (Platform != UnrealTargetPlatform.Unknown)
			{
				// Engine/Config/Platform/Platform* ini
				yield return FileReference.Combine(EngineDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");

				if (ProjectDirectory != null)
				{
					// Game/Config/Platform/Platform* ini
					yield return FileReference.Combine(ProjectDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");
				}
			}

			DirectoryReference UserSettingsFolder = Utils.GetUserSettingDirectory(); // Match FPlatformProcess::UserSettingsDir()
			if(UserSettingsFolder != null)
			{
				// <AppData>/UE4/EngineConfig/User* ini
				yield return FileReference.Combine(UserSettingsFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			// Some user accounts (eg. SYSTEM on Windows) don't have a home directory. Ignore them if Environment.GetFolderPath() returns an empty string.
			string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			if (!String.IsNullOrEmpty(PersonalFolder))
			{
				DirectoryReference PersonalConfigFolder; // Match FPlatformProcess::UserDir()
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					PersonalConfigFolder = new DirectoryReference(Path.Combine(PersonalFolder, "Documents"));
				}
				else if (Environment.OSVersion.Platform == PlatformID.Unix)
				{
					PersonalConfigFolder = new DirectoryReference(Path.Combine(PersonalFolder, "Documents"));
				}
				else
				{
					PersonalConfigFolder = new DirectoryReference(PersonalFolder);
				}

				// <Documents>/UE4/EngineConfig/User* ini
				yield return FileReference.Combine(PersonalConfigFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			// Game/Config/User* ini
			if (ProjectDirectory != null)
			{
				yield return FileReference.Combine(ProjectDirectory, "Config", "User" + BaseIniName + ".ini");
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
