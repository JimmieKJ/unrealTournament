// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	public class CopyrightVerify
	{
		// A list of files that are exempt from the Copyright check
		// Should be all caps as the comparison is done w / a 
		// ToUpperInvariant version of the filename!
		static List<string> WhitelistFiles =
			new List<string>
			{
				"ENGINE\\PRIVATE\\SPEEDTREEWIND.CPP",
				"ENGINE\\PUBLIC\\SPEEDTREEWIND.H",
				"RESOURCES\\RESOURCE.H",
				"CORE\\PRIVATE\\WINRT\\THREADEMULATION.H",
				"CORE\\PRIVATE\\WINRT\\THREADEMULATION.CPP",
				"ENGINE\\SHADERS\\FXAA3_11.USF",
				"ENGINE\\SHADERS\\SHADERVERSION.USF",
			};

		/**
		 * See if the given file is whitelisted
		 * 
		 * @param	InFilename		The name of the file to check
		 * 
		 * @return	bool			true if it is whitelisted, false if not
		 */
		static bool IsWhitelistedFile(string InFilename)
		{
			string FilenameUpper = InFilename.ToUpperInvariant();
			foreach (string Whitelist in WhitelistFiles)
			{

				if (FilenameUpper.Contains(Whitelist) == true)
				{
					return true;
				}
			}
			return false;
		}

		static List<string> BadFiles = new List<string>();

		/**
		 * Process the given list of files, checking each one has the proper copyright.
		 * This REQUIRES that the first line in any file is the copyright line!!!
		 * 
		 * @param	InFiles			The list of files to process
		 * @param	OutBadFiles		OUTPUT - List of files that failed the copyright check (and why)
		 */
		static void ProcessFileList(List<FileInfo> InFiles, ref Dictionary<string,string> OutBadFiles)
		{
			foreach (FileInfo CheckFile in InFiles)
			{
				if (IsWhitelistedFile(CheckFile.FullName) == false)
				{
					using (StreamReader MyReader = new StreamReader(CheckFile.FullName))
					{
						// Allow for the copyright to be on either the first or second line for now...
						for (int LineNumber = 0; LineNumber < 2; LineNumber++)
						{
							string CurrLine = MyReader.ReadLine();
							if (CurrLine == null)
							{
								// Empty file!
								if (OutBadFiles.ContainsKey(CheckFile.FullName) == false)
								{
									OutBadFiles.Add(CheckFile.FullName, "Empty file                    ");
									string BadString = "Empty file                     - " + CheckFile.FullName;
									BadFiles.Add(BadString);
								}
								break;
							}
							else if (CurrLine.StartsWith("// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.") == true)
							{
								// The file has the correct copyright... do nothing
								break;
							}
							else if ((CurrLine == "") && (LineNumber == 0))
							{
								// Allow for the first line to be empty
							}
							else if (CurrLine.StartsWith("// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.") == false)
							{
								if (OutBadFiles.ContainsKey(CheckFile.FullName) == false)
								{
									OutBadFiles.Add(CheckFile.FullName, "Incorrect copyright           ");
									string BadString = "Incorrect copyright            - " + CheckFile.FullName;
									BadFiles.Add(BadString);
								}
								break;
							}
							else if (CurrLine != "")
							{
								// This is a file that has something other than the Copyright or a blank line for
								// the first line of the file.
								if (OutBadFiles.ContainsKey(CheckFile.FullName) == false)
								{
									OutBadFiles.Add(CheckFile.FullName, "First line !EMPTY & !COPYRIGHT");
									string BadString = "First line !EMPTY & !COPYRIGHT - " + CheckFile.FullName;
									BadFiles.Add(BadString);
								}
								break;
							}
						}
						MyReader.Close();
					}
				}
			}
		}

		static void ProcessXamlFileList(List<FileInfo> InFiles, ref Dictionary<string, string> OutBadFiles)
		{
			foreach (FileInfo CheckFile in InFiles)
			{
				using (StreamReader MyReader = new StreamReader(CheckFile.FullName))
				{
					string Line0 = MyReader.ReadLine();
					if (Line0.StartsWith("<!-- Copyright 1998-2011 Epic Games, Inc. All Rights Reserved. -->") == true)
					{
						if (OutBadFiles.ContainsKey(CheckFile.FullName) == false)
						{
							OutBadFiles.Add(CheckFile.FullName, "Incorrect copyright");
						}
					}
					MyReader.Close();
				}
			}
		}

		static void RunCopyrightVerificationOnFolder(string InDirectoryToCheck)
		{
			Log.TraceInformation("--------------------------------------------------------------------------------");
			Log.TraceInformation("Running copyright verification on {0}...", InDirectoryToCheck);

			// Gather all the files in the given directory
			DirectoryInfo DirInfo = new DirectoryInfo(InDirectoryToCheck);
			List<FileInfo> HeaderFiles = new List<FileInfo>(DirInfo.GetFiles("*.h", SearchOption.AllDirectories));
			List<FileInfo> CPPFiles = new List<FileInfo>(DirInfo.GetFiles("*.cpp", SearchOption.AllDirectories));
			List<FileInfo> CSharpFiles = new List<FileInfo>(DirInfo.GetFiles("*.cs", SearchOption.AllDirectories));
			List<FileInfo> InlineFiles = new List<FileInfo>(DirInfo.GetFiles("*.inl", SearchOption.AllDirectories));
			List<FileInfo> ShaderFiles = new List<FileInfo>(DirInfo.GetFiles("*.usf", SearchOption.AllDirectories));
			List<FileInfo> HLSLShaderFiles = new List<FileInfo>(DirInfo.GetFiles("*.hlsl", SearchOption.AllDirectories));
			List<FileInfo> GLSLShaderFiles = new List<FileInfo>(DirInfo.GetFiles("*.glsl", SearchOption.AllDirectories));
			List<FileInfo> MobileShaderFiles = new List<FileInfo>(DirInfo.GetFiles("*.msf", SearchOption.AllDirectories));
			List<FileInfo> XamlFiles = new List<FileInfo>(DirInfo.GetFiles("*.xaml", SearchOption.AllDirectories));

			Dictionary<string, string> BadCopyrightFiles = new Dictionary<string, string>();
			Log.TraceInformation("Processing Header files...");
			ProcessFileList(HeaderFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing CPP files...");
			ProcessFileList(CPPFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing CSharp files...");
			ProcessFileList(CSharpFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing Inline files...");
			ProcessFileList(InlineFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing Shader files...");
			ProcessFileList(ShaderFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing HLSLShader files...");
			ProcessFileList(HLSLShaderFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing GLSLShader files...");
			ProcessFileList(GLSLShaderFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing Mobile Shader files...");
			ProcessFileList(MobileShaderFiles, ref BadCopyrightFiles);
			Log.TraceInformation("Processing Xaml files...");
			ProcessXamlFileList(XamlFiles, ref BadCopyrightFiles);

			Log.TraceInformation("Completed!");
			Log.TraceInformation("Found {0} files with copyright issues.", BadCopyrightFiles.Count);
			foreach (KeyValuePair<string, string> BadFile in BadCopyrightFiles)
			{
				Log.TraceInformation("\tERROR: {0} - {1}", BadFile.Value, BadFile.Key);
			}
			Log.TraceInformation("--------------------------------------------------------------------------------");
		}

		public static void RunCopyrightVerification(string InRootSourceDirectory, string InGameName, bool bDumpToFile)
		{
			if (InRootSourceDirectory == "")
			{
				Log.TraceInformation("You must supply a directory!");
				return;
			}

			BadFiles.Clear();

			string ExecutingDirectory;
			ExecutingDirectory = Path.Combine(InRootSourceDirectory, "Runtime");
			RunCopyrightVerificationOnFolder(ExecutingDirectory);
			ExecutingDirectory = Path.Combine(InRootSourceDirectory, "Developer");
			RunCopyrightVerificationOnFolder(ExecutingDirectory);
			ExecutingDirectory = Path.Combine(InRootSourceDirectory, "Editor");
			RunCopyrightVerificationOnFolder(ExecutingDirectory);
			ExecutingDirectory = Path.Combine(InRootSourceDirectory, "..", "Shaders");
			RunCopyrightVerificationOnFolder(ExecutingDirectory);

			if ((InGameName != null) && (InGameName != ""))
			{
				ExecutingDirectory = Path.Combine(InRootSourceDirectory, "../../", InGameName, "Source");
				RunCopyrightVerificationOnFolder(ExecutingDirectory);
			}

			if (bDumpToFile == true)
			{
				// Write to the file even if there are no bad entires.
				// This will 'erase' previous runs.
				string DumpDirectory = Path.Combine(InRootSourceDirectory, "../Intermediate/Build/CopyrightVerification");
				Directory.CreateDirectory(DumpDirectory);
				string DumpFilename = Path.Combine(DumpDirectory, InGameName + "BadFiles.txt");
				using (StreamWriter MyWriter = new StreamWriter(DumpFilename))
				{
					if (BadFiles.Count > 0)
					{
						foreach (string OutputLine in BadFiles)
						{
							MyWriter.WriteLine(OutputLine);
						}
					}
					else
					{
						MyWriter.WriteLine("");
					}
					MyWriter.Close();
				}
			}
		}
	}
}
