// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/// <summary>
	/// Helper class for finding 3rd party headers included in public engine headers.
	/// </summary>
	public class ThirdPartyHeaderFinder
	{
		/// <summary>
		/// List of all third party headers from the current game's modules
		/// </summary>
		static List<Header> ThirdPartyHeaders;

		/// <summary>
		/// List of all engine headers referenced by the current game.
		/// </summary>
		static List<Header> EngineHeaders;

		/// <summary>
		/// Helper class for storing headers included from public engine headers
		/// </summary>
		class IncludePath
		{
			/// <summary>
			/// Path of public engine header includes from which the other header is included.
			/// </summary>
			public string PublicHeaderPath;

			/// <summary>
			/// The other header included from the public engine header (may be third party or private)
			/// </summary>
			public Header OtherHeader;

			public IncludePath(string InPublicPath, Header Other)
			{
				PublicHeaderPath = InPublicPath;
				OtherHeader = Other;
			}
		}

		/// <summary>
		/// Helper class for storing info on a header file.
		/// </summary>
		class Header
		{
			/// <summary>
			/// Relative path to the header file.
			/// </summary>
			public string Path;

			/// <summary>
			/// Name (with extension) of the header file.
			/// </summary>
			public string Name;

			/// <summary>
			/// If true this header is a public engine header file.
			/// </summary>
			public bool IsPublic;

			/// <summary>
			/// Contents of the header file.
			/// </summary>
			public string Contents;

			/// <summary>
			/// Pregenerated variations of includes.
			/// </summary>
			public string[] TestIncludes;

			public Header(string InPath, bool InPublic)
			{
				Path = InPath;
				Name = System.IO.Path.GetFileName(InPath);
				IsPublic = InPublic;
				TestIncludes = new string[] 
				{ 
					"\"" + Name + "\"",
					"\\" + Name + "\"",
					"/" + Name + "\"",
					"<" + Name + ">",
					"\\" + Name + ">",
					"/" + Name + ">"
				};
			}

			public override string ToString()
			{
				return Path;
			}

			public override int GetHashCode()
			{
				return Path.GetHashCode();
			}
		}

		/// <summary>
		/// Find all third party and private header includes in public engine headers.
		/// </summary>
		public static void FindThirdPartyIncludes(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture)
		{
			Log.TraceInformation("Looking for third party header includes in public engine header files (this may take a few minutes)...");

			TargetInfo Target = new TargetInfo(Platform, Configuration, Architecture);
			List<string> UncheckedModules = new List<string>();
			EngineHeaders = new List<Header>();
			ThirdPartyHeaders = new List<Header>();

			// Create a rules assembly for the engine
			RulesAssembly EngineRulesAssembly = RulesCompiler.CreateEngineRulesAssembly();

			// Find all modules referenced by the current target
			List<FileReference> ModuleFileNames = RulesCompiler.FindAllRulesSourceFiles(RulesCompiler.RulesFileType.Module, GameFolders: null, ForeignPlugins: null, AdditionalSearchPaths: null);
			foreach (FileReference ModuleFileName in ModuleFileNames)
			{
				string ModuleName = Path.GetFileNameWithoutExtension(ModuleFileName.GetFileNameWithoutExtension());
				try
				{
					ModuleRules RulesObject = EngineRulesAssembly.CreateModuleRules(ModuleName, Target);
					bool bEngineHeaders = RulesObject.Type != ModuleRules.ModuleType.External;
					foreach (string SystemIncludePath in RulesObject.PublicSystemIncludePaths)
					{
						FindHeaders(SystemIncludePath, bEngineHeaders ? EngineHeaders : ThirdPartyHeaders, bEngineHeaders);
					}
					foreach (string PublicIncludePath in RulesObject.PublicIncludePaths)
					{
						FindHeaders(PublicIncludePath, bEngineHeaders ? EngineHeaders : ThirdPartyHeaders, bEngineHeaders);
					}
				}
				catch (Exception)
				{
					// Ignore, some modules may fail here.
					UncheckedModules.Add(ModuleName);
				}
			}

			// Search for illegal includes.
			List<IncludePath> ThirdPartyIncludes = new List<IncludePath>();
			List<IncludePath> PrivateIncludes = new List<IncludePath>();
			CheckIfThirdPartyHeadersAreIncluded(ThirdPartyIncludes, PrivateIncludes);

			// List all of the included 3rd party headers unless their name matches with any of the engine header.
			if (ThirdPartyIncludes.Count > 0)
			{
				// Remove ambiguous headers
				for (int IncludeIndex = ThirdPartyIncludes.Count - 1; IncludeIndex >= 0; --IncludeIndex)
				{
					if (FindHeader(EngineHeaders, ThirdPartyIncludes[IncludeIndex].OtherHeader.Name) != null)
					{
						ThirdPartyIncludes.RemoveAt(IncludeIndex);
					}
				}
				if (ThirdPartyIncludes.Count > 0)
				{
					Log.TraceInformation("Warning: Found {0} third party header includes in public engine headers. Third party headers should only be included in private engine headers.", ThirdPartyIncludes.Count);
					foreach (IncludePath HeaderPath in ThirdPartyIncludes)
					{
						if (FindHeader(EngineHeaders, HeaderPath.OtherHeader.Name) == null)
						{
							Log.TraceInformation("{0} includes {1}.", HeaderPath.PublicHeaderPath, HeaderPath.OtherHeader.Path);
						}
					}
				}
			}

			// List all private engine headers included from public engine headers
			if (PrivateIncludes.Count > 0)
			{
				Log.TraceInformation("Warning: Found {0} private engine header includes in public engine headers. Private engine headers should not be included in public engine headers.", PrivateIncludes.Count);
			}
			foreach (IncludePath HeaderPath in PrivateIncludes)
			{
				Log.TraceInformation("{0} includes {1}.", HeaderPath.PublicHeaderPath, HeaderPath.OtherHeader.Path);
			}
			if (PrivateIncludes.Count == 0 && ThirdPartyIncludes.Count == 0)
			{
				Log.TraceInformation("Finished looking for third party includes. Nothing found.");
			}
			if (UncheckedModules.Count > 0)
			{
				Log.TraceInformation("Warning: The following modules could not be checked (exception while trying to create ModuleRules object):");
				for (int ModuleIndex = 0; ModuleIndex < UncheckedModules.Count; ++ModuleIndex)
				{
					Log.TraceInformation("  {0}", UncheckedModules[ModuleIndex]);
				}
			}
		}

		static void FindHeaders(string path, List<Header> headers, bool bPublic)
		{
			try
			{
				string[] HeaderFiles = Directory.GetFiles(path, "*.h", SearchOption.TopDirectoryOnly);
				for (int i = 0; i < HeaderFiles.Length; ++i)
				{
					headers.Add(new Header(HeaderFiles[i], bPublic));
				}
			}
			catch (Exception)
			{
				// Ignore all exceptions. Some paths may be invalid.
			}
		}

		static Header FindHeader(List<Header> Headers, string HeaderName)
		{
			for (int Index = 0; Index < Headers.Count; ++Index)
			{
				if (String.Compare(Headers[Index].Name, HeaderName, true) == 0)
				{
					return Headers[Index];
				}
			}
			return null;
		}

		static int FindNextInclude(string HeaderContents, int StartIndex)
		{
			int HashIndex = StartIndex;
			while ((HashIndex = HeaderContents.IndexOf('#', HashIndex)) >= 0)
			{
				int IncludeIndex = HashIndex + 1;
				for (; IncludeIndex < HeaderContents.Length && Char.IsWhiteSpace(HeaderContents[IncludeIndex]); ++IncludeIndex) ;
				if ((IncludeIndex + 7) < HeaderContents.Length && HeaderContents.Substring(IncludeIndex, 7) == "include")
				{
					return HashIndex;
				}
				else
				{
					HashIndex++;
				}
			}
			return -1;
		}

		static bool IsAnyThirdPartyHeaderIncluded(Header EngineHeader, HashSet<Header> VisitedHeaders, string HeaderPath, List<IncludePath> ThirdPartyIncludes, List<IncludePath> PrivateIncludes)
		{
			// Load the header contents if it hasn't been already
			string HeaderContents = EngineHeader.Contents;
			if (String.IsNullOrEmpty(HeaderContents))
			{
				StreamReader HeaderReader = new StreamReader(EngineHeader.Path);
				EngineHeader.Contents = HeaderReader.ReadToEnd();
				HeaderContents = EngineHeader.Contents;
				HeaderReader.Close();
				HeaderReader.Dispose();
			}

			bool Included = false;

			// Check if any of the third party headers is included in the engine header
			for (int HeaderIndex = 0; HeaderIndex < ThirdPartyHeaders.Count; ++HeaderIndex)
			{
				Header ThirdPartyHeader = ThirdPartyHeaders[HeaderIndex];

				// Search for third party headers
				for (int TestIndex = 0; TestIndex < ThirdPartyHeader.TestIncludes.Length; ++TestIndex)
				{
					if (HeaderContents.IndexOf(ThirdPartyHeader.TestIncludes[TestIndex], StringComparison.InvariantCultureIgnoreCase) >= 0)
					{
						ThirdPartyIncludes.Add(new IncludePath(EngineHeader.Path, ThirdPartyHeader));
						Included = true;
					}
				}
			}

			// Mark this header as visited
			VisitedHeaders.Add(EngineHeader);

			// Recursively check for other includes
			int IncludeIndex = -1;
			while ((IncludeIndex = FindNextInclude(HeaderContents, IncludeIndex + 1)) >= 0)
			{
				// Get the included header name
				int FirstQuoteIndex = HeaderContents.IndexOf('\"', IncludeIndex + 1);
				if (FirstQuoteIndex == -1)
				{
					FirstQuoteIndex = HeaderContents.IndexOf('<', IncludeIndex + 1);
				}
				if (FirstQuoteIndex > IncludeIndex)
				{
					int LastQuoteIndex = HeaderContents.IndexOf('\"', FirstQuoteIndex + 1);
					if (LastQuoteIndex == -1)
					{
						LastQuoteIndex = HeaderContents.IndexOf('>', FirstQuoteIndex + 1);
					}

					if (FirstQuoteIndex > -1 && LastQuoteIndex > -1)
					{
						string IncludedHeaderName = HeaderContents.Substring(FirstQuoteIndex + 1, LastQuoteIndex - FirstQuoteIndex - 1);
						if (String.IsNullOrEmpty(IncludedHeaderName) == false)
						{
							// Find the included header in the list of all headers
							IncludedHeaderName = Path.GetFileName(IncludedHeaderName);
							Header IncludedHeader = FindHeader(EngineHeaders, IncludedHeaderName);
							if (IncludedHeader != null && IncludedHeader.IsPublic == false && VisitedHeaders.Contains(IncludedHeader) == false)
							{
								// Recursively check for other includes
								IsAnyThirdPartyHeaderIncluded(IncludedHeader, VisitedHeaders, HeaderPath + " -> " + IncludedHeader.Name, ThirdPartyIncludes, PrivateIncludes);

								// Remember Private headers included from Public headers
								if (EngineHeader.IsPublic)
								{
									PrivateIncludes.Add(new IncludePath(HeaderPath, IncludedHeader));
								}
							}
						}
					}
				}
			}
			return Included;
		}

		static void CheckIfThirdPartyHeadersAreIncluded(List<IncludePath> ThirdPartyIncludes, List<IncludePath> PrivateIncludes)
		{
			foreach (Header HeaderFile in EngineHeaders)
			{
				if (HeaderFile.IsPublic)
				{
					HashSet<Header> VisitedHeaders = new HashSet<Header>();
					IsAnyThirdPartyHeaderIncluded(HeaderFile, VisitedHeaders, HeaderFile.Path, ThirdPartyIncludes, PrivateIncludes);
				}
			} // For each engine header
		}
	}
}

