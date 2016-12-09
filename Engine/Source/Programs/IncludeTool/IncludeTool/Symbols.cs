using IncludeTool.Support;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IncludeTool
{/*
	/// <summary>
	/// Enumeration for the type of a symbol
	/// </summary>
	enum SymbolType
	{
		Macro,
		Class,
		Struct,
		Enumeration,
	}

	[Serializable]
	class Symbol
	{
		public string Name;
		public SymbolType Type;
		public SourceFragment Fragment;
		public string[] ForwardDeclaration;

		public Symbol(string InName, SymbolType InType, SourceFragment InFragment)
		{
			Name = InName;
			Type = InType;
			Fragment = InFragment;
		}

		public override string ToString()
		{
			return String.Format("{0}: {1}", Type.ToString(), Name);
		}
	}

	class SymbolTable
	{
		Dictionary<string, Symbol> Lookup = new Dictionary<string,Symbol>();

		public SymbolTable()
		{
		}

		public void AddExports(SourceFragment Fragment)
		{
			TextBuffer Text = Fragment.File.Text;
			if(Text != null)
			{
				// Parse all the macro definitions
				for(int MarkupIdx = Fragment.MarkupMin; MarkupIdx < Fragment.MarkupMax; MarkupIdx++)
				{
					PreprocessorMarkup Directive = Fragment.File.Markup[MarkupIdx] as PreprocessorMarkup;
					if(Directive != null)
					{
						TextLocation Location = Directive.Location;
						if(Tokenizer.TryReadTokens(Text, ref Location, "#", "define"))
						{
							string MacroName = Tokenizer.ReadToken(Text, ref Location);
							TryAddSymbol(MacroName, SymbolType.Macro, Fragment);
						}
					}
				}

				// Parse the symbols 
				int SymbolMarkupIdx = Fragment.File.BodyMinIdx;
				int Depth = 0;
				ParseBalancedMarkup(Fragment.File, ref SymbolMarkupIdx, Fragment.MarkupMax, ref Depth, Fragment);
			}
		}

		bool ParseBalancedMarkup(SourceFile File, ref int MarkupIdx, int MarkupMax, ref int Depth, SourceFragment ForFragment)
		{
			// Read markup until reaching an 
			for(; MarkupIdx < MarkupMax; MarkupIdx++)
			{
				// Parse any code fields
				CodeMarkup Code = File.Markup[MarkupIdx] as CodeMarkup;
				if(Code != null)
				{
					SourceFragment SymbolsFragment = (ForFragment != null && MarkupIdx >= ForFragment.MarkupMin)? ForFragment : null;
					if(!ParseDeclarations(File, Code.Location, Code.EndLocation, ref Depth, SymbolsFragment))
					{
						return false;
					}
				}

				// Handle any nested conditions
				ConditionMarkup Condition = File.Markup[MarkupIdx] as ConditionMarkup;
				if(Condition != null)
				{
					// If this is the end of a condition, it's out of the current scope
					if(Condition.DepthDelta <= 0)
					{
						return true;
					}

					// Otherwise evaluate the whole block as part of this one
					if(!ParseBalancedConditionalMarkup(File, ref MarkupIdx, MarkupMax, ref Depth, ForFragment))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool ParseBalancedConditionalMarkup(SourceFile File, ref int MarkupIdx, int MarkupMax, ref int Depth, SourceFragment ForFragment)
		{
			// Get the first condition markup
			ConditionMarkup Condition = (ConditionMarkup)File.Markup[MarkupIdx];
			Debug.Assert(Condition.DepthDelta > 0);
			MarkupIdx++;

			// Save off the initial depth, and read the expected depth at the end of the first condition
			int InitialDepth = Depth;
			if(!ParseBalancedMarkup(File, ref MarkupIdx, MarkupMax, ref Depth, null))
			{
				return false;
			}

			// Check all the other branches of this condition match
			bool bHasElse = false;
			while(MarkupIdx < MarkupMax)
			{
				// Wait until we reach the end of the conditional block
				ConditionMarkup NextCondition = (ConditionMarkup)File.Markup[MarkupIdx];
				if(NextCondition.DepthDelta < 0)
				{
					return (bHasElse || Depth == InitialDepth);
				}

				// Otherwise it must be an 'else/elif' condition, or it would be consumed by calls to ParseMarkupDepth
				Debug.Assert(NextCondition.DepthDelta == 0);
				bHasElse |= (NextCondition.Type == PreprocessorConditionType.Else);
				MarkupIdx++;

				// Parse the new markup depth
				int NewDepth = InitialDepth;
				if(!ParseBalancedMarkup(File, ref MarkupIdx, MarkupMax, ref NewDepth, null) || NewDepth != Depth)
				{
					return false;
				}
			}
			return true;
		}

		void ParseBlockDepth(TextBuffer Text, TextLocation StartLocation, TextLocation EndLocation, ref int Depth)
		{
			for(TextLocation Location = StartLocation; Location < EndLocation && Depth >= 0; )
			{
				string Token = Tokenizer.ReadToken(Text, ref Location);
				if(Token == "{")
				{
					Depth++;
				}
				else if(Token == "}")
				{
					Depth--;
				}
			}
		}

		bool ParseDeclarations(SourceFile File, TextLocation StartLocation, TextLocation EndLocation, ref int Depth, SourceFragment ForFragment)
		{
			// Scan for all class or struct definitions
			for(TextLocation Location = StartLocation; Location < EndLocation && Depth >= 0; )
			{
				string Token = Tokenizer.ReadToken(File.Text, ref Location);
				if(Token == "{")
				{
					Depth++;
				}
				else if(Token == "}")
				{
					Depth--;
				}
				else if(Token == "enum")
				{
					TryParseExportedEnum(File.Text, Location, ForFragment);
				}
				else if(Token == "class" && Depth == 0 && ForFragment != null)
				{
					TryParseExportedRecord(File.Text, Location, Token, SymbolType.Class, ForFragment);
				}
				else if(Token == "struct" && Depth == 0 && ForFragment != null)
				{
					TryParseExportedRecord(File.Text, Location, Token, SymbolType.Struct, ForFragment);
				}
			}
			return Depth >= 0;
		}

		void TryParseExportedEnum(TextBuffer Text, TextLocation Location, SourceFragment AddToFragment)
		{
			string Token = ReadParserToken(Text, ref Location);
			if(Token == "class")
			{
				// Get the name
				string Name = ReadParserToken(Text, ref Location);

				// Get the next token
				Token = ReadParserToken(Text, ref Location);
				if(Token == ":" || Token == "{" || Token == ";")
				{
					// Create a symbol for it
					Symbol NewSymbol = TryAddSymbol(Name, SymbolType.Enumeration, AddToFragment);
					if(NewSymbol != null)
					{
						// Get the forward declaration part
						List<string> ForwardDeclaration = new List<string>();
						ForwardDeclaration.Add("enum");
						ForwardDeclaration.Add("class");
						ForwardDeclaration.Add(Name);
						while(Token != "{" && Token != ";")
						{
							ForwardDeclaration.Add(Token);
							Token = ReadParserToken(Text, ref Location);
						}
						ForwardDeclaration.Add(";");

						// Update the symbol
						NewSymbol.ForwardDeclaration = ForwardDeclaration.ToArray();
					}
				}
			}
		}

		void TryParseExportedRecord(TextBuffer Text, TextLocation Location, string Keyword, SymbolType Type, SourceFragment AddToFragment)
		{
			// Parse the class or struct name
			string Name = ReadParserToken(Text, ref Location);
			if(Name.EndsWith("_API"))
			{
				Name = ReadParserToken(Text, ref Location);
			}

			// Check that it's followed by something that can only be associated with a definition
			if(Tokenizer.IsIdentifierToken(Name))
			{
				string NextToken = ReadParserToken(Text, ref Location);
				if(NextToken == ":" || NextToken == "{")
				{
					Symbol NewSymbol = TryAddSymbol(Name, Type, AddToFragment);
					if(NewSymbol != null)
					{
						NewSymbol.ForwardDeclaration = new string[3];
						NewSymbol.ForwardDeclaration[0] = Keyword;
						NewSymbol.ForwardDeclaration[1] = Name;
						NewSymbol.ForwardDeclaration[2] = ";";
					}
				}
			}
		}

		static string ReadParserToken(TextBuffer Text, ref TextLocation Location)
		{
			for(;;)
			{
				string Token = Tokenizer.ReadToken(Text, ref Location);
				if(Token != "\n") return Token;
			}
		}

		Symbol TryAddSymbol(string Name, SymbolType Type, SourceFragment Fragment)
		{
			Symbol NewSymbol = null;
			if(!Lookup.ContainsKey(Name))
			{
				NewSymbol = new Symbol(Name, Type, Fragment);
				Lookup.Add(Name, NewSymbol);
			}
			return NewSymbol;
		}

		public Symbol[] FindImports(SourceFragment Fragment)
		{
			Dictionary<Symbol, bool> References = new Dictionary<Symbol,bool>();

			TextBuffer Text = Fragment.File.Text;
			if(Text != null && Fragment.MarkupMax > Fragment.MarkupMin)
			{
				for(TextLocation Location = Fragment.MinLocation; Location < Fragment.MaxLocation; )
				{
					Symbol MatchingSymbol;
					if(Lookup.TryGetValue(Tokenizer.ReadToken(Text, ref Location), out MatchingSymbol))
					{
						if(MatchingSymbol.Type == SymbolType.Macro)
						{
							if(!References.ContainsKey(MatchingSymbol))
							{
								References.Add(MatchingSymbol, true);
							}
						}
						else if(MatchingSymbol.Type == SymbolType.Enumeration)
						{
							bool bTypeOnly = !Tokenizer.TryReadToken(Text, ref Location, "::");
							if(References.ContainsKey(MatchingSymbol))
							{
								References[MatchingSymbol] &= bTypeOnly;
							}
							else
							{
								References.Add(MatchingSymbol, bTypeOnly);
							}
						}
						else if(MatchingSymbol.Type == SymbolType.Class || MatchingSymbol.Type == SymbolType.Struct)
						{
							bool bIsPtrRef = (Tokenizer.TryReadToken(Text, ref Location, "*") || Tokenizer.TryReadToken(Text, ref Location, "&"));
							if(References.ContainsKey(MatchingSymbol))
							{
								References[MatchingSymbol] &= bIsPtrRef;
							}
							else
							{
								References.Add(MatchingSymbol, bIsPtrRef);
							}
						}
					}
				}
			}

			return References.Where(x => x.Value).Select(x => x.Key).ToArray();
		}
	}*/
}
