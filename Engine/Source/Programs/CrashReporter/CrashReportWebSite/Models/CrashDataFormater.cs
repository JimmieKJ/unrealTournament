// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels
{
	class CustomFuncComparer : IEqualityComparer<string>
	{
		public bool Equals( string x, string y )
		{
			return y.IndexOf( x, StringComparison.InvariantCultureIgnoreCase ) != -1;
		}

		public int GetHashCode( string obj )
		{
			return obj.GetHashCode();
		}
	}

	/// <summary>
	/// A class representing the line of a callstack.
	/// </summary>
	public class CallStackEntry
	{
		/// <summary>The raw line of the callstack.</summary>
		public string RawCallStackLine = "";
		/// <summary>The module the crash occurred in.</summary>
		public string ModuleName = "";
		/// <summary>The full path of the source file.</summary>
		private string FullFilePath = "";
		/// <summary>The function name.</summary>
		public string FunctionName = "";
		/// <summary>The line number of the crash.</summary>
		public int LineNumber;

		/// <summary>
		/// Blank constructor.
		/// </summary>
		public CallStackEntry()
		{
		}

		/// <summary>
		/// Set the properties of a call stack line.
		/// </summary>
		/// <param name="InRawCallStackLine">The unformatted line of the callstack.</param>
		/// <param name="InModuleName">The module name the call stack entry appears in.</param>
		/// <param name="InFilePath">The full file path of the source file.</param>
		/// <param name="InFunctionName">The name of the function Class::Function() + x bytes.</param>
		/// <param name="InLineNumber">The line number in the source file.</param>
		public CallStackEntry( string InRawCallStackLine, string InModuleName, string InFilePath, string InFunctionName, int InLineNumber )
		{
			RawCallStackLine = InRawCallStackLine;
			ModuleName = InModuleName;
			FullFilePath = InFilePath;
			FunctionName = InFunctionName;
			LineNumber = InLineNumber;
		}

		/// <summary>
		/// An accessor to return just the file name from the full path.
		/// </summary>
		public string FileName
		{
			get
			{
				if( FullFilePath.Length > 0 )
				{
					return Path.GetFileName( FullFilePath ) + ":" + LineNumber;
				}

				return "";
			}
		}

		/// <summary>
		/// An accessor to return the file path.
		/// </summary>
		public string FilePath
		{
			get 
			{
				if( FullFilePath.Length > 0 )
				{
					return FullFilePath + ":" + LineNumber;
				}

				return "";
			}
		}

		/// <summary>
		/// Trim large function names.
		/// </summary>
		/// <param name="MaxLength">The maximum number of characters to return.</param>
		/// <returns>The first MaxLength characters of the function name.</returns>
		public string GetTrimmedFunctionName( int MaxLength )
		{
			return FunctionName.Substring( 0, Math.Min( FunctionName.Length, MaxLength ) );
		}

		/// <summary> </summary>
		public override string ToString()
		{
			return string.Format( "{0}!{1} [{2}:{3}]", this.ModuleName, this.FunctionName, this.FileName, this.LineNumber );
		}
	}

	/// <summary>
	/// A class to represent a parsed call stack, and helper functions to return various visualizations.
	/// </summary>
	public class CallStackContainer
	{
		/// <summary>The maximum number of call stack lines to parse.</summary>
		public static readonly int MaxLinesToParse = 64;

		/// <summary>A list of parse call stack lines.</summary>
		private List<CallStackEntry> LocalCallStackEntries = new List<CallStackEntry>();

		/// <summary>Whether to display the callstack completely raw.</summary>
		public bool bDisplayUnformattedCallStack;
		/// <summary>Whether to display module names.</summary>
		public bool bDisplayModuleNames;
		/// <summary>Whether to display function names.</summary>
		public bool bDisplayFunctionNames;
		/// <summary>Whether to display the file names.</summary>
		public bool bDisplayFileNames;
		/// <summary>Whether to display the full path name.</summary>
		public bool bDisplayFilePathNames;

		/// <summary>
		/// A property to allow the web pages to access the processed callstack entries.
		/// </summary>
		public List<CallStackEntry> CallStackEntries
		{
			get
			{
				return LocalCallStackEntries;
			}
			set
			{
				LocalCallStackEntries = value;
			}
		}

		/// <summary>
		/// Construct a processed callstack.
		/// </summary>
		/// <param name="CurrentCrash"></param>
		public CallStackContainer( Crash CurrentCrash )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId=" + CurrentCrash.Id + ")" ) )
			{
				ParseCallStack( CurrentCrash ); 
			}
		}

		/// <summary>
		/// Find the module name that was most likely the source of the crash.
		/// </summary>
		/// <returns>The module name that we detected as the source of the crash.</returns>
		public string GetModuleName()
		{
			string ModuleName = "<Unknown>";

			// Find the category of the crash
			if( CallStackEntries.Count > 0 )
			{
				// Default to the first module. This is valid for crashes
				ModuleName = CallStackEntries[0].ModuleName;
			}

			return ModuleName;
		}


		/// <summary>Parsing regex copied from UE3 branch</summary>
		static readonly Regex OldCallstackFormat = new Regex(@"([^(]*[(][^)]*[)])([^\[]*([\[][^\]]*[\]]))*", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled);

		/// <summary>
		/// Parse a raw callstack line of a crash submitted before the UE4 upgrade
		/// </summary>
		/// <param name="Line">Line of callstack</param>
		/// <remarks>Code paraphrased from UE3 version of this file</remarks>
		void ParseUE3FormatCallstackLine(string Line)
		{
			Match CurMatch = OldCallstackFormat.Match(Line);

			if (!CurMatch.Success || CurMatch.Value == "")
			{
				return;
			}
			string FuncName = CurMatch.Groups[1].Value;
			string FilePath = "";
			int LineNumber = -1;

			Group ExtraInfo = CurMatch.Groups[CurMatch.Groups.Count - 1];

			if (ExtraInfo.Success)
			{
				const string FILE_START = "[File=";
				//const string PATH_START = "[in "; could be used to dig out module

				foreach (Capture CurCapture in ExtraInfo.Captures)
				{
					if (CurCapture.Value.StartsWith(FILE_START, StringComparison.OrdinalIgnoreCase))
					{
						// -1 cuts off closing ]
						FilePath = CurCapture.Value.Substring(FILE_START.Length, CurCapture.Length - FILE_START.Length - 1);
						int LineNumSeparator = FilePath.LastIndexOf(':');
						string LineNumStr = "";

						if (LineNumSeparator != -1)
						{
							LineNumStr = FilePath.Substring(LineNumSeparator + 1);
							FilePath = FilePath.Substring(0, LineNumSeparator);
						}

						int.TryParse(LineNumStr, out LineNumber);
					}
				}
			}
			CallStackEntries.Add( new CallStackEntry( Line, "<unknown module>", FilePath, FuncName, LineNumber ) );
		}


		/// <summary>Expression to check if a line is in the current call-stack format</summary>
		static readonly Regex NewCallstackFormat = new Regex( @".*?\(.*?\)", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled );
		//.*?\(.*?\).*?\[.*?\]

		/// <summary>
		/// Parse a raw callstack into a pattern
		/// </summary>
		/// <param name="CurrentCrash">The crash with a raw callstack to parse.</param>
		private void ParseCallStack( Crash CurrentCrash )
		{
			// Everything is disabled by default
			bDisplayUnformattedCallStack = false;
			bDisplayModuleNames = false;
			bDisplayFunctionNames = false;
			bDisplayFileNames = false;
			bDisplayFilePathNames = false;

			bool bSkipping = false;
			string LineToSkipUpto = "";
			switch( CurrentCrash.CrashType )
			{
			case 2:
				bSkipping = true;
				LineToSkipUpto = "FDebug::AssertFailed";
				break;
			case 3:
				bSkipping = true;
				LineToSkipUpto = "FDebug::";
				break;
			}

			if (string.IsNullOrEmpty(CurrentCrash.RawCallStack))
			{
				return;
			}

			CallStackEntries.Clear();

			// Store off a pre split array of call stack lines
			string[] RawCallStackLines = CurrentCrash.RawCallStack.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);

			int Middle = RawCallStackLines.Length / 2;

			// Support older callstacks uploaded before UE4 upgrade
			if( !NewCallstackFormat.Match( RawCallStackLines[Middle] ).Success )
			{
				foreach (string CurrentLine in RawCallStackLines)
				{
					// Exit if we've hit the max number of lines we want
					if (CallStackEntries.Count >= MaxLinesToParse)
					{
						break;
					}

					ParseUE3FormatCallstackLine(CurrentLine);
				}
				return;
			}

			foreach (string CurrentLine in RawCallStackLines)
			{
				// Exit if we've hit the max number of lines we want
				if( CallStackEntries.Count >= MaxLinesToParse )
				{
					break;
				}

				if( bSkipping )
				{
					if( CurrentLine.Contains( LineToSkipUpto ) )
					{
						bSkipping = false;
					}
				}

				if( bSkipping )
				{
					continue;
				}

				string ModuleName = "<Unknown>";
				string FuncName = "<Unknown>";
				string FilePath = "";
				int LineNumber = 0;

				// 
				// Generic sample line "UE4_Engine!UEngine::Exec() {+ 21105 bytes} [d:\depot\ue4\engine\source\runtime\engine\private\unrealengine.cpp:2777]"
				// 
				// Mac
				// thread_start()  Address = 0x7fff87ae141d (filename not found) [in libsystem_pthread.dylib]
				// 
				// Linux
				// Unknown!AFortPlayerController::execServerSaveLoadoutData(FFrame&, void*) + some bytes

				int ModuleSeparator = CurrentLine.IndexOf( '!' );
				int PlusOffset = CurrentLine.IndexOf( " + " );
				int OpenFuncSymbol = CurrentLine.IndexOf( '(' );
				int CloseFuncSymbol = CurrentLine.IndexOf( ')' );
				int OpenBracketOffset = CurrentLine.IndexOf( '[' );
				int CloseBracketOffset = CurrentLine.LastIndexOf( ']' );

				int MacModuleStart = CurrentLine.IndexOf( "[in " );
				int MacModuleEnd = MacModuleStart > 0 ? CurrentLine.IndexOf( "]", MacModuleStart ) : 0;

				bool bLinux = CurrentCrash.PlatformName.Contains( "Linux" );
				bool bMac = CurrentCrash.PlatformName.Contains( "Mac" );
				bool bWindows = CurrentCrash.PlatformName.Contains( "Windows" );


				// Parse out the juicy info from the line of the callstack
				if( ModuleSeparator > 0 )
				{
					ModuleName = CurrentLine.Substring( 0, ModuleSeparator ).Trim();
					if( OpenFuncSymbol > ModuleSeparator && CloseFuncSymbol > OpenFuncSymbol )
					{
						// Grab the function name if it exists
						FuncName = CurrentLine.Substring( ModuleSeparator + 1, OpenFuncSymbol - ModuleSeparator - 1 ).Trim();
						FuncName += "()";

						// Grab the source file
						if( OpenBracketOffset > CloseFuncSymbol && CloseBracketOffset > OpenBracketOffset && (bWindows || bLinux) )
						{
							string FileLinePath = CurrentLine.Substring( OpenBracketOffset + 1, CloseBracketOffset - OpenBracketOffset - 1 ).Trim();

							FilePath = FileLinePath.TrimEnd( "0123456789:".ToCharArray() );

							if (FileLinePath.Length > FilePath.Length + 1)
							{
								int SourceLine = 0;
								if (int.TryParse(FileLinePath.Substring(FilePath.Length + 1), out SourceLine))
								{
									LineNumber = SourceLine;
								}
							}
						}
					}
				}
				else if( bWindows )
				{
					// Grab the module name if there is no function name
					int WhiteSpacePos = CurrentLine.IndexOf( ' ' );
					ModuleName = WhiteSpacePos > 0 ? CurrentLine.Substring( 0, WhiteSpacePos ) : CurrentLine;
				}

				if( bMac && MacModuleStart > 0 && MacModuleEnd > 0 )
				{
					int AddressOffset = CurrentLine.IndexOf( "Address =" );
					int OpenFuncSymbolMac = AddressOffset > 0 ? CurrentLine.Substring( 0, AddressOffset ).LastIndexOf( '(' ) : 0;
					if( OpenFuncSymbolMac > 0 )
					{
						FuncName = CurrentLine.Substring( 0, OpenFuncSymbolMac ).Trim();
						FuncName += "()";
					}

					ModuleName = CurrentLine.Substring( MacModuleStart + 3, MacModuleEnd - MacModuleStart - 3 ).Trim();
				}

				// Remove callstack entries that match any of these functions.
				var FuncsToRemove = new HashSet<string>( new string[] 
				{ 
					"RaiseException", 
					"FDebug::", 		
					"Error::Serialize",
					"FOutputDevice::Logf",
					"FMsg::Logf",
					"ReportCrash",
					"NewReportEnsure",
					"EngineCrashHandler", // Generic crash handler for all platforms
				} );

				bool Contains = FuncsToRemove.Contains( FuncName, new CustomFuncComparer() );
				if( !Contains )
				{
					CallStackEntries.Add( new CallStackEntry( CurrentLine, ModuleName, FilePath, FuncName, LineNumber ) );
				}
			}
		}

		/// <summary>
		/// Format a callstack based on the requested parameters.
		/// </summary>
		/// <returns>An HTML representing a callstack in a readable format.</returns>
		public string GetFormattedCallStack()
		{
			StringBuilder FormattedCallStack = new StringBuilder();
			FormattedCallStack.Append( "<br>" );

			foreach( CallStackEntry CurrentEntry in CallStackEntries )
			{
				// Add in the module name
				if( bDisplayModuleNames )
				{
					FormattedCallStack.AppendFormat( "<b><font color=\"#151B8D\">" + CurrentEntry.ModuleName + "!</font></b>" );
				}

				// Add in the function name
				if( bDisplayFunctionNames )
				{
					FormattedCallStack.AppendFormat( "<b><font color=\"#151B8D\">" + CurrentEntry.FunctionName + "</font></b>" );
				}

				// Add in the full file path if requested
				if( CurrentEntry.FilePath.Length > 0 )
				{
					if( bDisplayFilePathNames )
					{
						FormattedCallStack.Append( " --- <b>" + CurrentEntry.FilePath + ":" + CurrentEntry.LineNumber + "</b>" );
					}
					else if( bDisplayFileNames )
					{
						FormattedCallStack.Append( " --- <b>" + Path.GetFileName( CurrentEntry.FilePath ) + ":" + CurrentEntry.LineNumber + "</b>" );
					}
				}

				FormattedCallStack.Append( "<br>" );
			}

			return FormattedCallStack.ToString();
		}

		/// <returns>A list of functions calls.</returns>
		public List<string> GetFunctionCalls()
		{
			var Result = new List<string>();
			foreach( CallStackEntry CurrentEntry in CallStackEntries )
			{
				Result.Add( CurrentEntry.FunctionName );
			}
			return Result;
		}

		/// <returns>A list of functions calls.</returns>
		public List<string> GetFunctionCallsForJira()
		{
			var Result = new List<string>();
			foreach( CallStackEntry CurrentEntry in CallStackEntries )
			{
				if( CurrentEntry.FileName != null && CurrentEntry.FileName.Length > 3 )
				{
					Result.Add( string.Format( "{0}!{1} [{2}]", CurrentEntry.ModuleName, CurrentEntry.FunctionName, CurrentEntry.FileName ) );
				}
				else
				{
					Result.Add( string.Format( "{0}!{1}", CurrentEntry.ModuleName, CurrentEntry.FunctionName ) );
				}
			}
			return Result;
		}
	}
}
