// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace APIDocTool
{
	class BuildTarget
	{
		public List<string> Definitions = new List<string>();
		public List<string> IncludePaths = new List<string>();
		public List<string> DirectoryNames = new List<string>();

		public BuildTarget(string BaseDirectoryName, string InputFileName)
		{
			XmlDocument Document = new XmlDocument();
			Document.Load(InputFileName);

			foreach (XmlNode Node in Document.SelectNodes("BuildSet/Environments/Environment/Tools/Tool"))
			{
				XmlAttribute ToolPathAttr = Node.Attributes["Path"];
				if (ToolPathAttr != null && String.Compare(Path.GetFileName(ToolPathAttr.InnerText), "cl.exe") == 0)
				{
					string Name = Node.Attributes["Name"].InnerText;
					string Params = Node.Attributes["Params"].InnerText;

					// Parse a list of tokens
					List<string> Tokens = new List<string>();
					Tokens.AddRange(TokenizeArguments(Params));

					// Expand response files
					for(int Idx = 0; Idx < Tokens.Count; Idx++)
					{
						if(Tokens[Idx].StartsWith("@"))
						{
							List<string> NewTokens = File.ReadAllLines(Tokens[Idx].Substring(1)).SelectMany(Line => TokenizeArguments(Line)).ToList();
							Tokens.RemoveAt(Idx);
							Tokens.InsertRange(Idx, NewTokens);
						}
					}

					// Parse it into a list of options, removing any that don't apply
					List<string> FileNames = new List<string>();
					for (int Idx = 0; Idx < Tokens.Count; Idx++)
					{
						if (Tokens[Idx].StartsWith("/Fo") || Tokens[Idx].StartsWith("/Fp") || Tokens[Idx].StartsWith("/Yu"))
						{
							Idx++;
						}
						else if (Tokens[Idx] == "/D")
						{
							Definitions.AddUnique(Tokens[++Idx]);
						}
						else if (Tokens[Idx] == "/I")
						{
							IncludePaths.AddUnique(ResolveDirectory(BaseDirectoryName, Tokens[++Idx]));
						}
						else if (!Tokens[Idx].StartsWith("/"))
						{
							DirectoryNames.AddUnique(Path.GetDirectoryName(ResolveFile(BaseDirectoryName, Tokens[Idx])));
						}
					}
				}
			}
		}

		static IEnumerable<string> TokenizeArguments(string Params)
		{
			for (int Idx = 0; Idx < Params.Length; Idx++)
			{
				if (!Char.IsWhiteSpace(Params[Idx]))
				{
					StringBuilder Token = new StringBuilder();
					while (Idx < Params.Length && !Char.IsWhiteSpace(Params[Idx]))
					{
						if (Params[Idx] == '"')
						{
							Idx++;
							while (Params[Idx] != '"') Token.Append(Params[Idx++]);
							Idx++;
						}
						else
						{
							Token.Append(Params[Idx++]);
						}
					}
					yield return Token.ToString();
				}
			}
		}

		static string ResolveFile(string BaseDirectoryName, string FileName)
		{
			string NewFileName = FileName;
			if(!Path.IsPathRooted(NewFileName))
			{
				NewFileName = Path.Combine(BaseDirectoryName, NewFileName);
			}
			return new FileInfo(NewFileName).FullName;
		}

		static string ResolveDirectory(string BaseDirectoryName, string DirectoryName)
		{
			string NewDirectoryName = ExpandEnvironmentVariables(DirectoryName).TrimEnd('/', '\\');
			if(!Path.IsPathRooted(NewDirectoryName))
			{
				NewDirectoryName = Path.Combine(BaseDirectoryName, NewDirectoryName);
			}
			return new DirectoryInfo(NewDirectoryName).FullName;
		}

		static string ExpandEnvironmentVariables(string Text)
		{
			int StartIdx = -1;
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Text[Idx] == '$' && (Idx + 1 < Text.Length && Text[Idx + 1] == '('))
				{
					// Save the start of a variable name
					StartIdx = Idx;
				}
				else if(Text[Idx] == ')' && StartIdx != -1)
				{
					// Replace the variable
					string Name = Text.Substring(StartIdx + 2, Idx - (StartIdx + 2));
					string Value = Environment.GetEnvironmentVariable(Name);
					if (Value != null)
					{
						Text = Text.Substring(0, StartIdx) + Value + Text.Substring(Idx + 1);
						Idx = StartIdx + Value.Length - 1;
					}
					StartIdx = -1;
				}
			}
			return Text;
		}
	}
}
