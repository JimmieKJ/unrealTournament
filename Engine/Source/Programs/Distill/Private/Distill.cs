// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

using Ionic.Zip;
using Tools.DotNETCommon.XmlHandler;

namespace Tools.Distill
{
	/// <summary>
	/// Distill - a build copying utility
	/// </summary>
	class NamespaceDoc
	{
	}
	
	/// <summary>A container for all the available options</summary>
	public class DistillOptions
	{
		/// <summary>The name of the game to use (including the game extension)</summary>
		public string Game = "Engine";
		/// <summary>A list of all games, excluding the selected game. Games are automatically discovered.</summary>
		public List<string> NotGames = null;
		/// <summary>The source of the file soup</summary>
		public string Source = "";
		/// <summary>The destination of the files</summary>
		public string Destination = "";
		/// <summary>The destination zip file</summary>
		public string ZipName = "";
		/// <summary>The FTP site to upload to</summary>
		public string FTPSite = "";
		/// <summary>The folder on the FTP site to upload to</summary>
		public string FTPFolder = "";
		/// <summary>The FTP user to upload to the FTP site</summary>
		public string FTPUser = "";
		/// <summary>The FTP password for the FTP user</summary>
		public string FTPPass = "";
		/// <summary>The platform to distill files for</summary>
		public string Platform = "Windows";
		/// <summary>Additional platforms which should be included</summary>
		public List<string> IncPlatforms = new List<string>();
		/// <summary>A list of all the platforms, excluding the selected platform. Defined in the KnownPlatforms section of the settings xml</summary>
		public List<string> NotPlatforms = null;
		/// <summary>The language to distill files for</summary>
		public string Region = "INT";
		/// <summary>A list of all languages, excluding the selected language. Languages are automatically discovered.</summary>
		public List<string> NotLanguages = null;
		/// <summary>The name of the set of tags to manipulate files</summary>
		public string TagSet = "Default";
		/// <summary>The xml file to load</summary>
		public string DistillSettings;

		/// <summary></summary>
		public bool bAuthenticate = false;
		/// <summary></summary>
		public bool bChecksum = false;
		/// <summary></summary>
		public bool bForce = false;
		/// <summary></summary>
		public bool bLog = false;
		/// <summary></summary>
		public bool bNoCopy = false;
		/// <summary></summary>
		public bool bTOC = false;
		/// <summary></summary>
		public bool bVerify = false;
		/// <summary></summary>
		public bool bWhitelistSignatures = false;
	}

	/// <summary>A utility assembly to handle copying, zipping, authenticating, and FTPing of builds</summary>
	public partial class Distill
	{
		/// <summary>A parsed version of the commandline options</summary>
		public static DistillOptions Options = new DistillOptions();

		private static void ShowUsage()
		{
			Log( "Distills a build out of the file soup to and/or from one or more locations.", ConsoleColor.Gray );
			Log( "", ConsoleColor.Gray );
			Log( "Distill.exe [-Options] [-Game=<Name>] [-Source=<Path>] [-Destination=<Path>]", ConsoleColor.Gray );
			Log( "", ConsoleColor.Gray );
			Log( "Options:", ConsoleColor.Gray );
			Log( "  -a (-Authenticate)   : Verify the local files have the same checksums as in the TOC", ConsoleColor.Gray );
			Log( "  -c (-Checksum)       : Generate SHA256 checksums when creating the TOC", ConsoleColor.Gray );
			Log( "  -Destination=<Name>  : Destination UNC path", ConsoleColor.Gray );
			Log( "  -f (-Force)          : Force copying of all files regardless of time stamp", ConsoleColor.Gray );
			Log( "  -FTPSite=<Name>      : FTP site to send to", ConsoleColor.Gray );
			Log( "  -FTPFolder=<Name>    : FTP root folder", ConsoleColor.Gray );
			Log( "  -FTPUser=<Name>      : FTP user for the FTP site", ConsoleColor.Gray );
			Log( "  -FTPPass=<Name>      : FTP password for the user", ConsoleColor.Gray );
			Log( "  -Game=<Name>         : Name of the game to be copied (typically ending in 'Game')", ConsoleColor.Gray );
			Log( "  -l (-Log)            : More verbose logging output", ConsoleColor.Gray );
			Log( "  -n (-NoCopy)         : Do not sync files, preview only", ConsoleColor.Gray );
			Log( "  -TOC                 : Save a Table of Contents (TOC) to disk.", ConsoleColor.Gray );
			Log( "  -Platform=<Name>     : Specify platform <Platform> to be used", ConsoleColor.Gray);
			Log( "                         Can be one of: Windows, IPhone, Android etc. Default: Windows.", ConsoleColor.Gray);
			Log( "  -IncPlatform=<Name>  : Specify platform which should not be excluded through %NOTPLATFORM% macros", ConsoleColor.Gray);
			Log( "  -Region=<Name>       : Three letter code determining packages to copy [Default is INT]", ConsoleColor.Gray);
			Log( "                       : Used for copying loc packages, all packages of the type _LOC.upk", ConsoleColor.Gray );
			Log( "  -Source=<Name>       : Specifies an alternate source for the data.", ConsoleColor.Gray );
			Log( "  -TagSet=<Name>       : Only sync files in tag set <TagSet>. See Distill.xml for the tag sets.", ConsoleColor.Gray );
			Log( "  -v (-Verify)         : Verify that the SHAs match between source and destination", ConsoleColor.Gray );
			Log( "                         Use in combination with -a", ConsoleColor.Gray );
			Log( "  -w (-Whitelist)      : Only include exes and dlls with a whitelisted digital signature", ConsoleColor.Gray );
			Log( "  -ZipName=<Name>      : Destination zip file", ConsoleColor.Gray );
			Log( "", ConsoleColor.Gray );
		}

		private static void ParseArguments( string[] Arguments )
		{
			for( int ArgumentIndex = 0; ArgumentIndex < Arguments.Length; ArgumentIndex++ )
			{
				if( Arguments[ArgumentIndex].Contains( "=" ) )
				{
					string[] Pair = Arguments[ArgumentIndex].Split( "=".ToCharArray() );
					if( Pair.Length == 2 )
					{
						switch( Pair[0].ToLower() )
						{
						case "-distillsettings":
							Options.DistillSettings = Pair[1];
							break;

						case "-destination":
							Options.Destination = Pair[1];
							break;

						case "-ftpsite":
							Options.FTPSite = Pair[1];
							break;

						case "-ftpfolder":
							Options.FTPFolder = Pair[1];
							break;

						case "-game":
							Options.Game = Pair[1];
							break;

						case "-ftpuser":
							Options.FTPUser = Pair[1];
							break;

						case "-ftppass":
							Options.FTPPass = Pair[1];
							break;

						case "-platform":
							Options.Platform = Pair[1];
							break;

						case "-incplatform":
							Options.IncPlatforms.Add(Pair[1]);
							break;

						case "-region":
							Options.Region = Pair[1];
							break;

						case "-source":
							Options.Source = Pair[1];
							break;

						case "-tagset":
							Options.TagSet = Pair[1];
							break;

						case "-zipname":
							Options.ZipName = Pair[1];
							break;

						default:
							Warning( "Unknown option: " + Pair[0] );
							break;
						}
					}
				}
				else 
				{
					switch( Arguments[ ArgumentIndex ].ToLower() )
					{
						case "-a":
							Options.bAuthenticate = true;
							break;

						case "-c":
							Options.bChecksum = true;
							break;

						case "-f":
							Options.bForce = true;
							break;

						case "-l":
							Options.bLog = true;
							break;

						case "-n":
							Options.bNoCopy = true;
							break;

						case "-toc":
							Options.bTOC = true;
							break;

						case "-v":
							Options.bVerify = true;
							break;

						case "-w":
							Options.bWhitelistSignatures = true;
							break;

						default:
							Warning( "Unknown option: " + Arguments[ ArgumentIndex ] );
							break;

					}
				}
			}
		}

		private static bool ValidateOptions( DistillSettings Settings )
		{
			if( Settings.FileGroups == null || Settings.KnownPlatforms == null || Settings.TagSets == null )
			{
				Error( "Failed to load configuration file" );
				return false;
			}

			// Fix the case of the platform
			foreach( PlatformInfo Platform in Settings.KnownPlatforms )
			{
				if( Platform.Name.ToUpper() == Options.Platform.ToUpper() )
				{
					Options.Platform = Platform.Name;
					break;
				}
			}

			if( ( Options.bAuthenticate || Options.bVerify ) && !Options.bChecksum )
			{
				Warning( " ... enabling checksums (-c) to allow verification and authentication" );
				Options.bChecksum = true;
			}

			return true;
		}

		/// <summary>Write a line of important text to the console</summary>
		/// <param name="Line">The line of text to write to the console</param>
		/// <param name="Color">The colour to use when writing the text</param>
		public static void Log( string Line, ConsoleColor Color )
		{
			Console.ForegroundColor = Color;
			Console.WriteLine( Line );
		}

		/// <summary>Write a line of important text to the console, but suppress the line feed</summary>
		/// <param name="Line">The line of text to write to the console</param>
		/// <param name="Color">The colour to use when writing the text</param>
		public static void ProgressLog( string Line, ConsoleColor Color )
		{
			Console.ForegroundColor = Color;
			Console.Write( "\r" + Line );
		}

		/// <summary>Write a line of text to the console if verbose logging is enabled</summary>
		/// <param name="Line">The line of text to write to the console</param>
		/// <param name="Color">The colour to use when writing the text</param>
		public static void DetailLog( string Line, ConsoleColor Color )
		{
			if( Options.bLog )
			{
				Console.ForegroundColor = Color;
				Console.WriteLine( Line );
			}
		}

		/// <summary>Write a warning line to the console</summary>
		/// <param name="Line">The warning to report</param>
		public static void Warning( string Line )
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine( "WARNING: " + Line );
		}

		/// <summary>Write an error line o the console</summary>
		/// <param name="Line">The error to report</param>
		public static void Error( string Line )
		{
			Console.ForegroundColor = ConsoleColor.Red;
			Console.WriteLine( "ERROR: " + Line );
			ErrorCode = 1;
		}

		/// <summary>Authenticate the local files against the checksums in the TOC</summary>
		/// <param name="TableOfContents">Freshly created TOC from the local file system</param>
		public static void Authenticate( TOC TableOfContents )
		{
			string TOCFileName = Path.Combine( Options.Source, Options.Game, "TOC.xml" );
			TOC OriginalTableOfContents = XmlHandler.ReadXml<TOC>( TOCFileName );
			if( OriginalTableOfContents.Entries != null )
			{
				OriginalTableOfContents.Compare( TableOfContents );
			}
			else
			{
				Error( "Failed to load TOC: " + TOCFileName );
			}
		}

		static int ErrorCode = 0;

		/// <summary>The main entry point of the program</summary>
		/// <param name="Arguments">The arguments passed on on the commandline</param>
		/// <returns>Zero on success, non zero for failure</returns>
		private static int Main( string[] Arguments )
		{

			if( Arguments.Length == 0 )
			{
				ShowUsage();
				return 1;
			}

			// Set the current directory to the root of the branch
			Environment.CurrentDirectory = Path.Combine( Environment.CurrentDirectory, "..", "..", ".." );
			Options.Source = Environment.CurrentDirectory;
			Options.Destination = Environment.CurrentDirectory;

			// Remember console color for restoring on exit
			ConsoleColor OriginalConsoleColor = Console.ForegroundColor;

			// Parse the command line for settings
			ParseArguments( Arguments );

			// Load the Distill.xml file and the game specific version
			string SettingsLocation = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), Options.DistillSettings);
			if (File.Exists(SettingsLocation) == false)
			{
				Error("Unable to find distill file " + Options.DistillSettings);
				Console.ForegroundColor = OriginalConsoleColor;
				return -1;
			}

			DistillSettings Settings = XmlHandler.ReadXml<DistillSettings>( SettingsLocation );

			// Find the known languages based on the folders in Engine Localization
			Settings.KnownLanguages = FindKnownLanguages();

			// Ensure the settings are valid
			if( !ValidateOptions( Settings ) )
			{
				Console.ForegroundColor = OriginalConsoleColor;
				return -1;
			}

			// Get the set of tags we wish to copy
			TagSet Set = GetTagSet( Settings );
			if( Set == null )
			{
				Console.ForegroundColor = OriginalConsoleColor;
				return -1;
			}

			// Get a list of filesets that includes all the unexpanded macros
			List<FileSet> DecoratedFileSets = GetFileSets( Settings, Set );
			if( DecoratedFileSets.Count == 0 )
			{
				Error( "No file sets for game, platform, tagset combination!" );
				Console.ForegroundColor = OriginalConsoleColor;
				return -1;
			}

			// Expand out all the macros
			List<FileSet> FileSets = ExpandMacros( DecoratedFileSets, Settings );
			// Get the files referenced by the filesets
			List<FileInfo> Files = GetFiles( FileSets );
			// Create a TOC
			TOC TableOfContents = new TOC( Files );
			Files = null;

			if( Options.bAuthenticate )
			{
				// If we're authenticating, compare the newly created TOC from files with the existing one on disk
				Authenticate( TableOfContents );
			}
			else
			{
				// Only write a TOC if we're copying locally to somewhere else (the most common case)
				string TOCFileName = Path.Combine( Options.Source, Options.Game, "TOC.xml" );
				if( Options.bTOC && (Options.Source == Environment.CurrentDirectory) )
				{
					Log( " ... writing TOC: " + TOCFileName, ConsoleColor.Cyan );
					XmlHandler.WriteXml<TOC>( TableOfContents, TOCFileName, "" );
				}

				// Copy the TOC if it exists
				if (Options.bTOC)
				{
					FileInfo TOCFileInfo = new FileInfo(TOCFileName);
					if (TOCFileInfo.Exists)
					{
						TableOfContents.Entries.Add(new TOCEntry(TOCFileInfo));
					}
					else
					{
						Error(" ... Expected a TOC but there isn't one to copy: " + TOCFileName); 
					}
				}

				// Copy files
				ZipFiles( TableOfContents );
				CopyFiles( TableOfContents );
				FTPFiles( TableOfContents );

				Log( "Distill process successful!", ConsoleColor.Green );
			}

			// Restore console color
			Console.ForegroundColor = OriginalConsoleColor;
			return ErrorCode;
		}
	}
}
