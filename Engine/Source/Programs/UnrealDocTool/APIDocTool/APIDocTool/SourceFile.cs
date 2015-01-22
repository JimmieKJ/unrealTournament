// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	public struct SourceToken
	{
		public SourceFile File;
		public int Offset;
		public int Length;

		public SourceToken(SourceFile InFile, int InOffset, int InLength)
		{
			File = InFile;
			Offset = InOffset;
			Length = InLength;
		}

		public bool IsIdentifier
		{
			get { return Char.IsLetter(Text[0]) || Text[0] == '_'; }
		}

		public bool IsLiteral
		{
			get { string TextCopy = Text; return TextCopy[0] == '\"' || TextCopy[0] == '\'' || Char.IsDigit(TextCopy[0]) || (TextCopy.Length >= 2 && TextCopy[0] == '.' && Char.IsDigit(TextCopy[1])); }
		}

		public string Text
		{
			get { return File.Text.Substring(Offset, Length); }
		}

		public int EndOffset
		{
			get { return Offset + Length; }
		}

		public override string ToString()
		{
			return Text;
		}
	}

	public struct SourceTokenRef
	{
		public SourceFile File;
		public int Index;

		public SourceTokenRef(SourceFile InFile, int InIndex)
		{
			File = InFile;
			Index = InIndex;
		}

		public int Offset
		{
			get { return File.Tokens[Index].Offset; }
		}

		public int Length
		{
			get { return File.Tokens[Index].Length; }
		}

		public int EndOffset
		{
			get { return File.Tokens[Index].EndOffset; }
		}

		public static SourceTokenRef operator ++(SourceTokenRef Ref)
		{
			return Ref + 1;
		}

		public static SourceTokenRef operator --(SourceTokenRef Ref)
		{
			return Ref - 1;
		}

		public static SourceTokenRef operator +(SourceTokenRef Ref, int Offset)
		{
			return new SourceTokenRef(Ref.File, Ref.Index + Offset);
		}

		public static SourceTokenRef operator -(SourceTokenRef Ref, int Offset)
		{
			return new SourceTokenRef(Ref.File, Ref.Index - Offset);
		}

		public bool Matches(string Text)
		{
			return ToString() == Text;
		}

		public override string ToString()
		{
			return File.Tokens[Index].ToString();
		}
	}

	public class SourceFile
	{
		public readonly string Path;
		public readonly string Text;
		public readonly SourceToken[] Tokens;
		public readonly int[] LineOffsets;
		public readonly int[] LineToToken;

		public SourceFile(string InPath, string InText)
		{
			Path = InPath;
			Text = InText;

			// Build a list of tokens
			List<SourceToken> Tokens = new List<SourceToken>();
			for(int Position = 0; Position < Text.Length; )
			{
				if(Char.IsWhiteSpace(Text[Position]))
				{
					// Skip whitespace
					Position++;
				}
				else if(Text[Position] == '/' && Text[Position + 1] == '/')
				{
					// Skip C++ style comment
					Position += 2;
					while(Position < Text.Length && Text[Position] != '\n' && Text[Position] != '\r')
					{
						Position++;
					}
				}
				else if(Text[Position] == '/' && Text[Position + 1] == '*')
				{
					// Skip C style comment
					Position += 4;
					while(Text[Position - 2] != '*' || Text[Position - 1] != '/')
					{
						Position++;
					}
				}
				else
				{
					int StartPosition = Position;
					if (Text[Position] == '\'' || Text[Position] == '\"')
					{
						// It's a character or string literal. 
						char Terminator = Text[Position++];
						while (Text[Position] != Terminator)
						{
							if (Text[Position] == '\\')
							{
								Position += 2;
							}
							else
							{
								Position++;
							}
						}
						Position++;
					}
					else if(Char.IsDigit(Text[Position]) || (Text[Position] == '.' && Char.IsDigit(Text[Position + 1])))
					{
						// It's a numeric token
						Position++;
						for(;;)
						{
							if((Text[Position] == 'e' || Text[Position] == 'E') && (Text[Position + 1] == '+' || Text[Position + 1] == '-'))
							{
								Position += 2;
							}
							else if(Char.IsLetterOrDigit(Text[Position]) || Text[Position] == '.')
							{
								Position++;
							}
							else
							{
								break;
							}
						}
					}
					else if(Char.IsLetter(Text[Position]) || Text[Position] == '_')
					{
						// It's an identifier
						Position++;
						while(Position < Text.Length && (Char.IsLetterOrDigit(Text[Position]) || Text[Position] == '_'))
						{
							Position++;
						}
					}
					else
					{
						// Just take a single character, we don't really care what symbol it is
						Position++;
					}
					Tokens.Add(new SourceToken(this, StartPosition, Position - StartPosition));
				}
			}
			this.Tokens = Tokens.ToArray();

			// Build a map from line to source offsets
			List<int> LineOffsets = new List<int> { 0 };
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Text[Idx] == '\r' || Text[Idx] == '\n')
				{
					char OtherNewline = (char)(Text[Idx] ^ '\r' ^ '\n');
					if (Idx + 1 < Text.Length && Text[Idx + 1] == OtherNewline) Idx++;
					LineOffsets.Add(Idx + 1);
				}
			}
			this.LineOffsets = LineOffsets.ToArray();

			// Build a map of line to token
			LineToToken = new int[LineOffsets.Count];
			for (int Idx = 1; Idx < LineOffsets.Count; Idx++)
			{
				int TokenIdx = LineToToken[Idx - 1];
				while(TokenIdx < Tokens.Count && Tokens[TokenIdx].Offset < LineOffsets[Idx])
				{
					TokenIdx++;
				}
				LineToToken[Idx] = TokenIdx;
			}
		}

		public int GetTokenAt(int Line, int Column)
		{
			if (Line + 1 > LineOffsets.Length - 1)
			{
				return Tokens.Length - 2;
			}

			int Offset = LineOffsets[Line - 1] + (Column - 1);

			int Index = LineToToken[Line - 1];
			while (Index < Tokens.Length && Offset > Tokens[Index].Offset + Tokens[Index].Length)
			{
				Index++;
			}
			return Index;
		}

		public override string ToString()
		{
			return Path;
		}
	}

	public static class SourceFileCache
	{
		static Dictionary<string, SourceFile> Files = new Dictionary<string, SourceFile>();

		public static SourceFile Read(string InPath)
		{
			string FullPath = Path.GetFullPath(InPath);

			SourceFile Result = null;
			if (!Files.TryGetValue(FullPath, out Result) && File.Exists(FullPath))
			{
				string Text = File.ReadAllText(FullPath);
				Result = new SourceFile(FullPath, Text);
				Files.Add(FullPath, Result);
			}

			return Result;
		}
	}
}
