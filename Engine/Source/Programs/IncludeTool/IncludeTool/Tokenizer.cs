using IncludeTool.Support;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IncludeTool
{
	/// <summary>
	/// Partial tokenizer for C++ source files. Provides functions for navigating a source file skipping whitespace, comments, and so on when required.
	/// </summary>
	static class Tokenizer
	{
#if false
		/// <summary>
		/// List of multi-character symbolic tokens that we want to parse out
		/// </summary>
		static readonly string[] SymbolicTokens =
		{
			"==",
			"!=",
			"<=",
			">=",
			"++",
			"--",
			"->",
			"::",
			"&&",
			"||",
			"##",
			"<<",
			">>",
			"...",
		};

		/// <summary>
		/// Extracts a list of tokens between two positions
		/// </summary>
		/// <param name="Text">The text buffer to read from</param>
		/// <param name="BeginLineIdx">The initial line</param>
		/// <param name="BeginColumnIdx">The initial column</param>
		/// <param name="EndLineIdx">The final line</param>
		/// <param name="EndColumnIdx">The final column</param>
		/// <returns>Array of tokens</returns>
		public static string[] ExtractTokens(TextBuffer Text, int BeginLineIdx, int BeginColumnIdx, int EndLineIdx, int EndColumnIdx)
		{
			int LineIdx = BeginLineIdx;
			int ColumnIdx = BeginColumnIdx;

			List<string> Tokens = new List<string>();
			while (LineIdx < EndLineIdx || (LineIdx == EndLineIdx && ColumnIdx < EndColumnIdx))
			{
				string Token = ReadToken(Text, ref LineIdx, ref ColumnIdx);
				Tokens.Add(Token);
			}
			return Tokens.ToArray();
		}

		/// <summary>
		/// Extracts a list of tokens between two positions
		/// </summary>
		/// <param name="Text">The text buffer to read from</param>
		/// <param name="Begin">The initial position</param>
		/// <param name="End">The final position</param>
		/// <returns>Array of tokens</returns>
		public static string[] ExtractTokens(TextBuffer Text, TextLocation Begin, TextLocation End)
		{
			return ExtractTokens(Text, Begin.LineIdx, Begin.ColumnIdx, End.LineIdx, End.ColumnIdx);
		}

		/// <summary>
		/// Advances the given position past any horizontal whitespace or comments
		/// </summary>
		/// <param name="Text">Buffer to read from</param>
		/// <param name="LineIdx">The initial line index</param>
		/// <param name="ColumnIdx">The initial column index</param>
		public static void SkipHorizontalWhitespace(TextBuffer Text, ref int LineIdx, ref int ColumnIdx)
		{
			while (LineIdx < Text.Lines.Length)
			{
				// Quickly skip over trivial whitespace
				string Line = Text.Lines[LineIdx];
				while (ColumnIdx < Line.Length && (Line[ColumnIdx] == ' ' || Line[ColumnIdx] == '\t' || Line[ColumnIdx] == '\v'))
				{
					ColumnIdx++;
				}

				// Look at what's next
				char Character = Text[LineIdx, ColumnIdx];
				if (Character == '\\' && Text[LineIdx, ColumnIdx + 1] == '\n')
				{
					LineIdx++;
					ColumnIdx = 0;
				}
				else if (Character == '/' && Text[LineIdx, ColumnIdx + 1] == '/')
				{
					ColumnIdx = Text.Lines[LineIdx].Length;
				}
				else if (Character == '/' && Text[LineIdx, ColumnIdx + 1] == '*')
				{
					ColumnIdx += 2;
					while (Text[LineIdx, ColumnIdx] != '*' || Text[LineIdx, ColumnIdx + 1] != '/')
					{
						Text.MoveNext(ref LineIdx, ref ColumnIdx);
					}
					ColumnIdx += 2;
				}
				else
				{
					break;
				}
			}
		}

		/// <summary>
		/// Advances the given position past any horizontal whitespace or comments
		/// </summary>
		/// <param name="Text">Buffer to read from</param>
		/// <param name="Location">The current location</param>
		public static void SkipHorizontalWhitespace(TextBuffer Text, ref TextLocation Location)
		{
			SkipHorizontalWhitespace(Text, ref Location.LineIdx, ref Location.ColumnIdx);
		}

		/// <summary>
		/// Reads the indent prior to the given position
		/// </summary>
		/// <param name="Text">Buffer to read from</param>
		/// <param name="Location">The current location</param>
		public static string PeekLeadingIndent(TextBuffer Text, TextLocation Location)
		{
			int ColumnIdx = Location.ColumnIdx;
			while (ColumnIdx > 0 && Char.IsWhiteSpace(Text[Location.LineIdx, ColumnIdx - 1]))
			{
				ColumnIdx--;
			}
			return Text.Lines[Location.LineIdx].Substring(ColumnIdx, Location.ColumnIdx - ColumnIdx);
		}

		/// <summary>
		/// Peeks the horizontal whitespace at the given location
		/// </summary>
		/// <param name="Text">Buffer to read from</param>
		/// <param name="Location">The current location</param>
		/// <returns>Horizontal whitepsace at the current position</returns>
		public static string PeekHorizontalWhitespace(TextBuffer Text, TextLocation Location)
		{
			SkipHorizontalWhitespace(Text, ref Location.LineIdx, ref Location.ColumnIdx);
			return Text.Lines[Location.LineIdx].Substring(0, Location.ColumnIdx);
		}

		/// <summary>
		/// Moves to the start of the next line. Treats escaped newlines as part of the current line.
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		public static void MoveToNextLine(TextBuffer Text, ref int LineIdx, ref int ColumnIdx)
		{
			while(LineIdx < Text.Lines.Length && Text.Lines[LineIdx].EndsWith("\\"))
			{
				LineIdx++;
			}
			if(LineIdx < Text.Lines.Length)
			{
				LineIdx++;
			}
			ColumnIdx = 0;
		}

		/// <summary>
		/// Moves to the start of the next line. Treats escaped newlines as part of the current line.
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		public static void MoveToNextLine(TextBuffer Text, ref TextLocation Location)
		{
			MoveToNextLine(Text, ref Location.LineIdx, ref Location.ColumnIdx);
		}

		/// <summary>
		/// Reads a single token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		/// <returns>The next token, or null at the end of the file</returns>
		public static string ReadToken(TextBuffer Text, ref int LineIdx, ref int ColumnIdx)
		{
			SkipHorizontalWhitespace(Text, ref LineIdx, ref ColumnIdx);

			char Character = Text.ReadCharacter(ref LineIdx, ref ColumnIdx);
			if (Character == '\0')
			{
				return null;
			}
			else if (Character == '\'' || Character == '\"')
			{
				// String or character literal
				StringBuilder Builder = new StringBuilder();
				Builder.Append(Character);
				for (;;)
				{
					char NextCharacter = Text.ReadCharacter(ref LineIdx, ref ColumnIdx);
					Builder.Append(NextCharacter);
					if (NextCharacter == '\\')
					{
						Builder.Append(Text.ReadCharacter(ref LineIdx, ref ColumnIdx));
					}
					else if (NextCharacter == Character)
					{
						break;
					}
				}
				return Builder.ToString();
			}
			else if (Char.IsLetter(Character) || Character == '_')
			{
				// Identifier
				string Line = Text.Lines[LineIdx];

				int StartIdx = ColumnIdx - 1;
				while (ColumnIdx < Line.Length && (Char.IsLetterOrDigit(Line[ColumnIdx]) || Line[ColumnIdx] == '_' || Line[ColumnIdx] == '$')) // NB. Microsoft extension for preprocessor identifiers; used by SAL
				{
					ColumnIdx++;
				}

				return Line.Substring(StartIdx, ColumnIdx - StartIdx);
			}
			else if (Char.IsDigit(Character) || (Character == '.' && Char.IsDigit(Text[LineIdx, ColumnIdx])))
			{
				// pp-number token
				StringBuilder Builder = new StringBuilder();
				for (;;)
				{
					Builder.Append(Character);
					Character = Text[LineIdx, ColumnIdx];
					if (!Char.IsLetterOrDigit(Character) && Character != '.')
					{
						if (Character != '+' || Character != '-' || Char.ToLower(Builder[Builder.Length - 1]) != 'e')
						{
							break;
						}
					}
					Text.MoveNext(ref LineIdx, ref ColumnIdx);
				}
				return Builder.ToString();
			}
			else
			{
				// Try to read a symbol
				if (ColumnIdx > 0)
				{
					for (int Idx = 0; Idx < SymbolicTokens.Length; Idx++)
					{
						string Token = SymbolicTokens[Idx];
						for (int Length = 0; Text[LineIdx, ColumnIdx + Length - 1] == Token[Length]; Length++)
						{
							if (Length + 1 == Token.Length)
							{
								ColumnIdx += Length;
								return Token;
							}
						}
					}
				}

				// Otherwise just return a single character
				return Character.ToString();
			}
		}

		/// <summary>
		/// Reads a single token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <returns>The next token, or null at the end of the file</returns>
		public static string ReadToken(TextBuffer Text, ref TextLocation Location)
		{
			return ReadToken(Text, ref Location.LineIdx, ref Location.ColumnIdx);
		}

		/// <summary>
		/// Tries to read a system include token, quoted by arrow brackets
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <param name="Token">On success, the vlaue of the tokens</param>
		/// <returns>True if an include token was read, false otherwise</returns>
		public static bool TryReadSystemIncludeToken(TextBuffer Text, ref TextLocation Location, out string Token)
		{
			if(!TryReadTokens(Text, ref Location, "<"))
			{
				Token = null;
				return false;
			}

			StringBuilder Builder = new StringBuilder("<");
			while(Builder[Builder.Length - 1] != '>')
			{
				Builder.Append(Text.ReadCharacter(ref Location));
			}
			Token = Builder.ToString();

			return true;
		}

		/// <summary>
		/// Peeks a single token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		/// <returns>The next token, or null at the end of the file</returns>
		public static string PeekToken(TextBuffer Text, int LineIdx, int ColumnIdx)
		{
			return ReadToken(Text, ref LineIdx, ref ColumnIdx);
		}

		/// <summary>
		/// Reads a single token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <returns>The next token, or null at the end of the file</returns>
		public static string PeekToken(TextBuffer Text, TextLocation Location)
		{
			return PeekToken(Text, Location.LineIdx, Location.ColumnIdx);
		}

		/// <summary>
		/// Checks if the next tokens in a text buffer match a given list
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		/// <param name="Tokens">The sequence of tokens to look for</param>
		/// <returns>True if the sequence of tokens matches, false otherwise</returns>
		public static bool PeekTokens(TextBuffer Text, int LineIdx, int ColumnIdx, params string[] Tokens)
		{
			return TryReadTokens(Text, ref LineIdx, ref ColumnIdx, Tokens);
		}

		/// <summary>
		/// Checks if the next tokens in a text buffer match a given list
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <param name="Tokens">The sequence of tokens to look for</param>
		/// <returns>True if the sequence of tokens matches, false otherwise</returns>
		public static bool PeekTokens(TextBuffer Text, TextLocation Location, params string[] Tokens)
		{
			return PeekTokens(Text, Location.LineIdx, Location.ColumnIdx, Tokens);
		}

		/// <summary>
		/// Tries to read a specific token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		/// <param name="Token">The token to try to read</param>
		/// <returns>True if the token was read, false otherwise</returns>
		public static bool TryReadToken(TextBuffer Text, ref int LineIdx, ref int ColumnIdx, string Token)
		{
			return TryReadTokens(Text, ref LineIdx, ref ColumnIdx, Token);
		}

		/// <summary>
		/// Tries to read a specific token from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <param name="Token">The token to try to read</param>
		/// <returns>True if the token was read, false otherwise</returns>
		public static bool TryReadToken(TextBuffer Text, ref TextLocation Location, string Token)
		{
			return TryReadTokens(Text, ref Location.LineIdx, ref Location.ColumnIdx, Token);
		}

		/// <summary>
		/// Tries to read a sequence of tokens from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="LineIdx">The current line index</param>
		/// <param name="ColumnIdx">The current column index</param>
		/// <param name="Tokens">The tokens to try to read</param>
		/// <returns>True if the tokens were read, false otherwise</returns>
		public static bool TryReadTokens(TextBuffer Text, ref int LineIdx, ref int ColumnIdx, params string[] Tokens)
		{
			int NewLineIdx = LineIdx;
			int NewColumnIdx = ColumnIdx;

			for (int Idx = 0; Idx < Tokens.Length; Idx++)
			{
				string Token = ReadToken(Text, ref NewLineIdx, ref NewColumnIdx);
				if (Token != Tokens[Idx]) return false;
			}

			LineIdx = NewLineIdx;
			ColumnIdx = NewColumnIdx;

			return true;
		}

		/// <summary>
		/// Tries to read a sequence of tokens from a text buffer
		/// </summary>
		/// <param name="Text">The buffer to read from</param>
		/// <param name="Location">The current position</param>
		/// <param name="Tokens">The tokens to try to read</param>
		/// <returns>True if the tokens were read, false otherwise</returns>
		public static bool TryReadTokens(TextBuffer Text, ref TextLocation Location, params string[] Tokens)
		{
			return TryReadTokens(Text, ref Location.LineIdx, ref Location.ColumnIdx, Tokens);
		}

		/// <summary>
		/// Check if the given token is an identifier
		/// </summary>
		/// <param name="Token">The token to check</param>
		/// <returns>True if the given token is an identifier</returns>
		public static bool IsIdentifierToken(string Token)
		{
			return Token.Length > 0 && (Char.IsLetter(Token[0]) || Token[0] == '_');
		}
#endif
	}
}
