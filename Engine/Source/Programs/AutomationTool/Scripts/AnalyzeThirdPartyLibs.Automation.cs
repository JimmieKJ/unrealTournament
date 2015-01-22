// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Linq;

/*
 - You can also use the full program to test compiling all or a subset of libs:
 - From Engine/Build/BatchFiles, do:
 -   RunUAT AnalyzeThirdPartyLibs [-libs=lib1+lib2+lib3] [-changelist=NNNN]
 */

class PlatformLibraryInfo
{
	public List<string> PathParticles = new List<string>();
	public List<string> Manifest = new List<string>();
	public long TotalSize = 0;
	public string PlatformName;

	public bool PartOfPlatform(string Filename)
	{
		foreach (string Particle in PathParticles)
		{
			if (Filename.IndexOf(Particle, StringComparison.InvariantCultureIgnoreCase) != -1)
			{
				return true;
			}
		}

		return (PathParticles.Count == 0) ? true : false;
	}

	public PlatformLibraryInfo(string PlatformName, params string[] Values)
	{
		this.PlatformName = PlatformName;
		PathParticles.AddRange(Values);
	}

	public void AddFile(string Filename, long Size)
	{
		Manifest.Add(Filename);
		TotalSize += Size;
	}
};


class ThirdPartyLibraryInfo
{
	public List<string> Manifest = new List<string>();

	public long GetSize(List<PlatformLibraryInfo> Platforms)
	{
		long TotalSize = 0;

		foreach (string Filename in Manifest)
		{
			FileInfo FI = new FileInfo(Filename);
			long Size = FI.Length;

			foreach (PlatformLibraryInfo Platform in Platforms)
			{
				if (Platform.PartOfPlatform(Filename))
				{
					Platform.AddFile(Filename, Size);
				}
			}

			TotalSize += Size;
		}

		return TotalSize;
	}

	public void FindLargeFiles(List<string> AllowedExtensions, long MinSize)
	{
		foreach (string Filename in Manifest)
		{
			FileInfo FI = new FileInfo(Filename);
			long Size = FI.Length;

			if (Size > MinSize)
			{
				bool bAllowed = false;
				foreach (string Extension in AllowedExtensions)
				{
					if (Filename.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase))
					{
						bAllowed = true;
						break;
					}
				}

				if (!bAllowed)
				{
					CommandUtils.Log("WARNING: {0} is {1} with an unexpected extension", Filename, AnalyzeThirdPartyLibs.ToMegabytes(Size));
				}
			}
		}
	}

	public ThirdPartyLibraryInfo(string Root)
	{
		string[] Files = Directory.GetFiles(Root, "*.*", SearchOption.AllDirectories);
		Manifest.AddRange(Files);
	}
}

[Help("Analyzes third party libraries")]
[Help("Libs", "[Optional] + separated list of libraries to compile; if not specified this job will build all libraries it can find builder scripts for")]
[Help("Changelist", "[Optional] a changelist to check out into; if not specified, a changelist will be created")]
class AnalyzeThirdPartyLibs : BuildCommand
{
	// path to the third party directory
	static private string LibDir = "Engine/Source/ThirdParty";

	// batch/script file to look for when compiling
	public static string ToMegabytes(long Size)
	{
		double SizeKB = Size / 1024.0;
		double SizeMB = SizeKB / 1024.0;

		return String.Format("{0:N2} MB", SizeMB);
	}

	public override void ExecuteBuild()
	{
		Log("************************* Analyze Third Party Libs");

		// figure out what batch/script to run
		switch (UnrealBuildTool.BuildHostPlatform.Current.Platform)
		{
			case UnrealTargetPlatform.Win64:
			case UnrealTargetPlatform.Mac:
			case UnrealTargetPlatform.Linux:
				break;
			default:
				throw new AutomationException("Unknown runtime platform!");
		}

		// go to the third party lib dir
		CommandUtils.PushDir(LibDir);

		// figure out what libraries to evaluate
		string LibsToEvaluateString = ParseParamValue("Libs");

		// Determine which libraries to evaluate
		List<string> LibsToEvaluate = new List<string>();
		if (string.IsNullOrEmpty(LibsToEvaluateString))
		{
			// loop over third party directories looking for the right batch files
			foreach (string Dir in Directory.EnumerateDirectories("."))
			{
				LibsToEvaluate.Add(Path.GetFileName(Dir));
			}
		}
		else
		{
			// just split up the param and make sure the batch file exists
			string[] Libs = LibsToEvaluateString.Split('+');
			foreach (string Dir in Libs)
			{
				LibsToEvaluate.Add(Path.GetFileName(Dir));
			}
		}

		// Make a list of platforms
		List<PlatformLibraryInfo> Platforms = new List<PlatformLibraryInfo>();
		Platforms.Add(new PlatformLibraryInfo("Windows", "Windows", "Win32", "Win64", "VS20"));
		Platforms.Add(new PlatformLibraryInfo("Mac", "Osx", "Mac"));
		Platforms.Add(new PlatformLibraryInfo("iOS", "IOS"));
		Platforms.Add(new PlatformLibraryInfo("Android", "Android"));
		Platforms.Add(new PlatformLibraryInfo("PS4", "PS4"));
		Platforms.Add(new PlatformLibraryInfo("XB1", "XBoxOne"));
		Platforms.Add(new PlatformLibraryInfo("HTML5", "HTML5"));
		Platforms.Add(new PlatformLibraryInfo("Linux", "Linux"));
		Platforms.Add(new PlatformLibraryInfo("WinRT", "WinRT"));

		Platforms.Add(new PlatformLibraryInfo("VS2012", "VS2012", "vs11"));
		Platforms.Add(new PlatformLibraryInfo("VS2013", "VS2013", "vs12"));

		List<long> LastSizes = new List<long>();
		foreach (var Platform in Platforms)
		{
			LastSizes.Add(0);
		}

		// now go through and evaluate each package
		long TotalSize = 0;
		foreach (string Lib in LibsToEvaluate)
		{
			ThirdPartyLibraryInfo Info = new ThirdPartyLibraryInfo(Lib);

			long Size = Info.GetSize(Platforms);

			Log("Library {0} is {1}", Lib, ToMegabytes(Size));

			long Total = 0;
			for (int Index = 0; Index < Platforms.Count; ++Index)
			{
				PlatformLibraryInfo Platform = Platforms[Index];
				long Growth = Platform.TotalSize - LastSizes[Index];
				Log("  {0} is {1}", Platform.PlatformName, ToMegabytes(Growth));

				LastSizes[Index] = Platform.TotalSize;
				Total += Growth;
			}
			Log("  Platform neutral is probably {0} (specific sum {1})", ToMegabytes(Size - Total), ToMegabytes(Total));

			TotalSize += Size;
		}


		// Make a list of known large file types
		List<string> LargeFileExtensions = new List<string>();
		LargeFileExtensions.AddRange(new string[] { ".pdb", ".a", ".lib", ".dll", ".dylib", ".bc", ".so" });

		// Hackery, look for big files (re-traverses everything)
		Log("----");
		foreach (string Lib in LibsToEvaluate)
		{
			ThirdPartyLibraryInfo Info = new ThirdPartyLibraryInfo(Lib);
			Info.FindLargeFiles(LargeFileExtensions, 1024 * 1024);
		}

		// Hackery, look for VS mixes (re-traverses everything)
		Log("----");
		long TotalShadow12 = 0;

		Log("Listing VS2012 directories that are shadowed by a VS2013 dir");
		foreach (string Lib in LibsToEvaluate)
		{
			string[] Dirs = Directory.GetDirectories(Lib, "*.*", SearchOption.AllDirectories);

			List<string> Dupes = new List<string>();

			foreach (string Dir in Dirs)
			{
				string VS2012 = "VS2012";
				string VS2013 = "VS2013";

				int Index = Dir.IndexOf(VS2013);
				if (Dir.EndsWith(VS2013))
				{
					string Prefix = Dir.Substring(0, Index);

					foreach (string OtherDir in Dirs)
					{
						if (OtherDir == (Prefix + VS2012))
						{
							Dupes.Add(OtherDir);
						}
					}
				}
			}

			foreach (string Dupe in Dupes)
			{
				long Size = 0;
				DirectoryInfo DI = new DirectoryInfo(Dupe);
				foreach (FileInfo FI in DI.EnumerateFiles("*.*", SearchOption.AllDirectories))
				{
					Size += FI.Length;
				}

				if (Size > 128 * 1024)
				{
					TotalShadow12 += Size;

					string GoodPath = (LibDir + "/" + Dupe).Replace("\\", "/");
					Log("{0}", GoodPath);
				}
			}
		}
		Log("OVERALL {0} of VS2012 files are shadowed by a VS2013 dir", ToMegabytes(TotalShadow12));

		Log("----");
		foreach (var Platform in Platforms)
		{
			Log("  {0} is {1} (estimate)", Platform.PlatformName, ToMegabytes(Platform.TotalSize));
		}
		Log("  OVERALL is {0} (accurate)", ToMegabytes(TotalSize));

		// undo the LibDir push
		CommandUtils.PopDir();


		Log("Listing VS2012 bin directories that are shadowed by a VS2013 dir");
		//string[] BinaryDirs
		//foreach (string Lib in BinaryDirs)
		{
			string BinDir = "Engine/Binaries";

			string[] Dirs = Directory.GetDirectories(BinDir, "*.*", SearchOption.AllDirectories);

			List<string> Dupes = new List<string>();

			foreach (string Dir in Dirs)
			{
				string VS2012 = "VS2012";
				string VS2013 = "VS2013";

				int Index = Dir.IndexOf(VS2013);
				if (Dir.EndsWith(VS2013))
				{
					string Prefix = Dir.Substring(0, Index);

					foreach (string OtherDir in Dirs)
					{
						if (OtherDir == (Prefix + VS2012))
						{
							Dupes.Add(OtherDir);
						}
					}
				}
			}

			foreach (string Dupe in Dupes)
			{
				long Size = 0;
				DirectoryInfo DI = new DirectoryInfo(Dupe);
				foreach (FileInfo FI in DI.EnumerateFiles("*.*", SearchOption.AllDirectories))
				{
					Size += FI.Length;
				}

//				if (Size > 128 * 1024)
				{
					TotalShadow12 += Size;

					string GoodPath = Dupe.Replace("\\", "/");
					Log("{0}", GoodPath);
				}
			}
		}

		PrintRunTime();
	}
}
