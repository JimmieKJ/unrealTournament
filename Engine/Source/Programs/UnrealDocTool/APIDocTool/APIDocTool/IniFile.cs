// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public class IniSection
	{
		public string Name;
		public Dictionary<string, string> Values = new Dictionary<string,string>();

		public IniSection(string InName)
		{
			Name = InName;
		}

		public void Set(string Key, string Value)
		{
			string KeyInvariant = Key.ToLowerInvariant();
			Values.Remove(KeyInvariant);
			Values.Add(KeyInvariant, Value);
		}

		public void Append(string Key, string Value)
		{
			string KeyInvariant = Key.ToLowerInvariant();

			string ExistingValue;
			if (Values.TryGetValue(KeyInvariant, out ExistingValue))
			{
				Values.Remove(KeyInvariant);
				Values.Add(KeyInvariant, ExistingValue + "\n" + Value);
			}
			else
			{
				Values.Add(KeyInvariant, Value);
			}
		}

		public string Find(string Key)
		{
			string Result;
			return Values.TryGetValue(Key.ToLowerInvariant(), out Result) ? Result : null;
		}
	}

	public class IniFile
	{
		public List<IniSection> Sections = new List<IniSection>();
		public Dictionary<string, IniSection> NameToSection = new Dictionary<string, IniSection>();

		public IniFile()
		{
		}

		public string FindValue(string KeyPath)
		{
			int EndIndex = KeyPath.LastIndexOf('.');
			if (EndIndex != -1)
			{
				IniSection Section = FindSection(KeyPath.Substring(0, EndIndex));
				if (Section != null)
				{
					string Value = Section.Find(KeyPath.Substring(EndIndex + 1));
					if(Value != null) return Value;
				}
			}
			return null;
		}

		public string FindValueOrDefault(string KeyPath, string DefaultValue)
		{
			string Value = FindValue(KeyPath);
			return (Value != null)? Value : DefaultValue;
		}

		public IniSection FindSection(string Name)
		{
			IniSection Section;
			if (NameToSection.TryGetValue(Name.ToLowerInvariant(), out Section))
			{
				return Section;
			}
			else
			{
				return null;
			}
		}

		public static IniFile Read(string InputPath)
		{
			IniFile File = new IniFile();
			using (StreamReader Reader = new StreamReader(InputPath))
			{
				IniSection CurrentSection = null;
				while (!Reader.EndOfStream)
				{
					string Line = Reader.ReadLine().Trim();
					if (Line.Length > 0 && !Line.StartsWith(";"))
					{
						// Check if it's a section heading or key/value pair
						if (Line[0] == '[')
						{
							if (Line[Line.Length - 1] != ']') throw new InvalidDataException();

							string Name = Line.Substring(1, Line.Length - 2);
							CurrentSection = new IniSection(Name);

							File.Sections.Add(CurrentSection);
							if(File.NameToSection.ContainsKey(Name.ToLowerInvariant()))
							{
								Console.WriteLine("{0} has already been added", Name);
							}
							File.NameToSection.Add(Name.ToLowerInvariant(), CurrentSection);
						}
						else
						{
							string Key, Value;

							// Split the line into key/value
							int ValueIdx = Line.IndexOf('=');
							if(ValueIdx == -1)
							{
								Key = Line;
								Value = "";
							}
							else
							{
								Key = Line.Substring(0, ValueIdx);
								Value = Line.Substring(ValueIdx + 1);
							}

							// If it's an append operation, add the existing value
							if(Key.Length > 0 && Key[0] == '+')
							{
								CurrentSection.Append(Key.Substring(1), Value);
							}
							else
							{
								CurrentSection.Set(Key, Value);
							}
						}
					}
				}
			}
			return File;
		}
	}
}
