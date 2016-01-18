// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using System.Text;

namespace UnrealBuildTool
{
	class VCEnvironment
	{
		public readonly CPPTargetPlatform Platform;             // The platform the envvars have been initialized for
		public readonly string BaseVSToolPath;       // The path to Visual Studio's /Common7/Tools directory.
		public readonly string VSToolPath32Bit;      // The path to the 32bit platform tool binaries.
		public readonly string VSToolPath64Bit;      // The path to the 64bit platform tool binaries.
		public readonly string WindowsSDKDir;        // Installation folder of the Windows SDK, e.g. C:\Program Files\Microsoft SDKs\Windows\v6.0A\
		public readonly string WindowsSDKExtensionDir;  // Installation folder of the Windows SDK Extensions, e.g. C:\Program Files (x86)\Windows SDKs\10
		public readonly string WindowsSDKLibVersion;
		public readonly string NetFxSDKExtensionDir;    // Installation folder of the NetFx SDK, since that is split out from platform SDKs >= v10
		public readonly Version WindowsSDKExtensionHeaderLibVersion;  // 10.0.9910.0 for instance...
		public readonly string CompilerPath;         // The path to the linker for linking executables
		public readonly Version CLExeVersion;         // The version of cl.exe we're running
		public readonly string LinkerPath;           // The path to the linker for linking executables
		public readonly string LibraryLinkerPath;    // The path to the linker for linking libraries
		public readonly string ResourceCompilerPath; // The path to the resource compiler
		public readonly string VisualCppDir;         // Installation folder for Visual C++, eg. C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC.
		public readonly string UniversalCRTDir;      // For Visual Studio 2015; the path to the universal CRT.
		public readonly string UniversalCRTVersion;  // For Visual Studio 2015; the universal CRT version to use.

		private string _MSBuildPath = null;
		public string MSBuildPath // The path to MSBuild
		{
			get
			{
				if (_MSBuildPath == null)
				{
					_MSBuildPath = GetMSBuildToolPath();
				}
				return _MSBuildPath;
			}
		}

		/// <summary>
		/// Initializes environment variables required by toolchain. Different for 32 and 64 bit.
		/// </summary>
		public static VCEnvironment SetEnvironment(CPPTargetPlatform Platform, bool bSupportWindowsXP)
		{
			if (EnvVars != null && EnvVars.Platform == Platform)
			{
				return EnvVars;
			}

			EnvVars = new VCEnvironment(Platform, bSupportWindowsXP);
			return EnvVars;
		}

		private VCEnvironment(CPPTargetPlatform InPlatform, bool bSupportWindowsXP)
		{
			Platform = InPlatform;

			// If Visual Studio is not installed, the Windows SDK path will be used, which also happens to be the same
			// directory. (It installs the toolchain into the folder where Visual Studio would have installed it to).
			BaseVSToolPath = WindowsPlatform.GetVSComnToolsPath();
			if (string.IsNullOrEmpty(BaseVSToolPath))
			{
				throw new BuildException("Visual Studio 2012, 2013 or 2015 must be installed in order to build this target.");
			}

			WindowsSDKDir = FindWindowsSDKInstallationFolder(Platform, bSupportWindowsXP);
			WindowsSDKLibVersion = FindWindowsSDKLibVersion(WindowsSDKDir);
			WindowsSDKExtensionDir = FindWindowsSDKExtensionInstallationFolder();
			NetFxSDKExtensionDir = FindNetFxSDKExtensionInstallationFolder();
			VisualCppDir = FindVisualCppInstallationFolder(WindowsPlatform.Compiler);
			WindowsSDKExtensionHeaderLibVersion = FindWindowsSDKExtensionLatestVersion(WindowsSDKExtensionDir);
			UniversalCRTDir = FindUniversalCRTInstallationFolder();
			UniversalCRTVersion = FindUniversalCRTVersion(UniversalCRTDir);

			VSToolPath32Bit = GetVSToolPath32Bit(BaseVSToolPath);
			VSToolPath64Bit = GetVSToolPath64Bit(BaseVSToolPath);

			// Compile using 64 bit tools for 64 bit targets, and 32 for 32.
			string CompilerVSToolPath = (Platform == CPPTargetPlatform.Win64 || Platform == CPPTargetPlatform.UWP) ? VSToolPath64Bit : VSToolPath32Bit;

			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker and can handle large linking jobs)
			string LinkerVSToolPath = VSToolPath64Bit;

			CompilerPath = GetCompilerToolPath(CompilerVSToolPath);
			CLExeVersion = FindCLExeVersion(CompilerPath);
			LinkerPath = GetLinkerToolPath(LinkerVSToolPath);
			LibraryLinkerPath = GetLibraryLinkerToolPath(LinkerVSToolPath);
			ResourceCompilerPath = GetResourceCompilerToolPath(Platform, bSupportWindowsXP);

			// Manually determine the compile environment
			List<string> IncludePaths = GetVisualCppIncludePaths(VisualCppDir, UniversalCRTDir, UniversalCRTVersion, NetFxSDKExtensionDir, WindowsSDKDir, WindowsSDKLibVersion);
			List<string> LibraryPaths = GetVisualCppLibraryPaths(VisualCppDir, UniversalCRTDir, UniversalCRTVersion, NetFxSDKExtensionDir, WindowsSDKDir, WindowsSDKLibVersion, Platform);

			// We ensure an extra trailing slash because of a user getting an odd error where the paths seemed to get concatenated wrongly:
			//
			// C:\Programme\Microsoft Visual Studio 12.0\Common7\Tools../../VC/bin/x86_amd64/vcvarsx86_amd64.bat
			//
			// https://answers.unrealengine.com/questions/233640/unable-to-create-project-files-for-48-preview-3.html
			//
			bool bUse64BitCompiler = Platform == CPPTargetPlatform.Win64 || Platform == CPPTargetPlatform.UWP;
			string BaseToolPathWithTrailingSlash = BaseVSToolPath.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
			string VCVarsBatchFile = Path.Combine(BaseToolPathWithTrailingSlash, bUse64BitCompiler ? @"..\..\VC\bin\x86_amd64\vcvarsx86_amd64.bat" : "vsvars32.bat");
			if (Platform == CPPTargetPlatform.UWP && UWPPlatform.bBuildForStore)
			{
				Utils.SetEnvironmentVariablesFromBatchFile(VCVarsBatchFile, "store");
			}
			else
			{
				Utils.SetEnvironmentVariablesFromBatchFile(VCVarsBatchFile);
			}

			// When targeting Windows XP on Visual Studio 2012+, we need to override the Windows SDK include and lib path set
			// by the batch file environment (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
			if (bSupportWindowsXP)
			{
				// Lib and bin folders have a x64 subfolder for 64 bit development.
				var ConfigSuffix = (Platform == CPPTargetPlatform.Win64) ? "\\x64" : "";

				Environment.SetEnvironmentVariable("PATH", Utils.ResolveEnvironmentVariable(WindowsSDKDir + "bin" + ConfigSuffix + ";%PATH%"));
				Environment.SetEnvironmentVariable("LIB", Utils.ResolveEnvironmentVariable(WindowsSDKDir + "lib" + ConfigSuffix + ";%LIB%"));
				Environment.SetEnvironmentVariable("INCLUDE", Utils.ResolveEnvironmentVariable(WindowsSDKDir + "include;%INCLUDE%"));
			}

			// Check the environment matches up with the environment variables set by the batch files. For now, this is academic, but once we've established this as reliable, 
			// start using them in preference. 
			// NOTE: We skip this step for Windows XP, because the batch file actually initializes them incorrectly (and we override them above).
			if (!bSupportWindowsXP)
			{
				string IncludePathsString = String.Join(";", IncludePaths) + ";";
				CompareEnvironmentVariable("INCLUDE", IncludePathsString, bSupportWindowsXP);

				string LibraryPathsString = String.Join(";", LibraryPaths) + ";";
				CompareEnvironmentVariable("LIB", LibraryPathsString, bSupportWindowsXP);
			}
		}

		/// <summary>
		/// Check that an environment variable is the same as one derived from keys in the registry, and send a telemetry event if it's not
		/// </summary>
		private void CompareEnvironmentVariable(string VariableName, string RegistryValue, bool bSupportWindowsXP)
		{
			string EnvironmentValue = Environment.GetEnvironmentVariable(VariableName);
			Telemetry.SendEvent("CompareEnvironmentVariable",
				"Name", VariableName,
				"Match", (EnvironmentValue == RegistryValue).ToString(),
				"EnvironmentValue", EnvironmentValue,
				"RegistryValue", RegistryValue,
				"Platform", Platform.ToString(),
				"UWPBuildForStore", UWPPlatform.bBuildForStore.ToString(),
				"WinXP", bSupportWindowsXP.ToString(),
				"Compiler", WindowsPlatform.Compiler.ToString(),
				"UseWindowsSDK10", WindowsPlatform.bUseWindowsSDK10.ToString());
		}

		/// <returns>The path to Windows SDK directory for the specified version.</returns>
		private static string FindWindowsSDKInstallationFolder(CPPTargetPlatform InPlatform, bool bSupportWindowsXP)
		{
			// When targeting Windows XP on Visual Studio 2012+, we need to point at the older Windows SDK 7.1A that comes
			// installed with Visual Studio 2012 Update 1. (http://blogs.msdn.com/b/vcblog/archive/2012/10/08/10357555.aspx)
			string Version;
			if (bSupportWindowsXP)
			{
				Version = "v7.1A";
			}
			else switch (WindowsPlatform.Compiler)
				{
					case WindowsCompiler.VisualStudio2015:
						if (WindowsPlatform.bUseWindowsSDK10)
						{
							Version = "v10.0";
						}
						else
						{
							Version = "v8.1";
						}
						break;

					case WindowsCompiler.VisualStudio2013:
						Version = "v8.1";
						break;

					case WindowsCompiler.VisualStudio2012:
						Version = "v8.0";
						break;

					default:
						throw new BuildException("Unexpected compiler setting when trying to determine Windows SDK folder");
				}

			// Based on VCVarsQueryRegistry
			string FinalResult = null;
			foreach (string IndividualVersion in Version.Split('|'))
			{
				var Result = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null)
					?? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null)
					?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + IndividualVersion, "InstallationFolder", null);

				if (Result != null)
				{
					FinalResult = (string)Result;
					break;
				}
			}
			if (FinalResult == null)
			{
				throw new BuildException("Windows SDK {0} must be installed in order to build this target.", Version);
			}

			return FinalResult;
		}

		/// <summary>
		/// Gets the version of the Windows SDK libraries to use. As per VCVarsQueryRegistry.bat, this is the directory name that sorts last.
		/// </summary>
		static string FindWindowsSDKLibVersion(string WindowsSDKDir)
		{
			string WindowsSDKLibVersion;
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2012)
			{
				WindowsSDKLibVersion = "win8";
			}
			else if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
			{
				DirectoryInfo IncludeDir = new DirectoryInfo(Path.Combine(WindowsSDKDir, "include"));
				if (!IncludeDir.Exists)
				{
					throw new BuildException("Couldn't find Windows 10 SDK include directory ({0})", IncludeDir.FullName);
				}

				DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories().OrderBy(x => x.Name).LastOrDefault();
				if (LatestIncludeDir == null)
				{
					throw new BuildException("No Windows 10 SDK versions found under ({0})", IncludeDir.FullName);
				}

				WindowsSDKLibVersion = LatestIncludeDir.Name;
			}
			else
			{
				WindowsSDKLibVersion = "winv6.3";
			}
			return WindowsSDKLibVersion;
		}

		private static string FindNetFxSDKExtensionInstallationFolder()
		{
			string Version;
			switch (WindowsPlatform.Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					Version = "4.6";
					break;

				default:
					return string.Empty;
			}
			string FinalResult = string.Empty;
			var Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Microsoft SDKs\NETFXSDK\" + Version, "KitsInstallationFolder", null)
					  ?? Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\NETFXSDK\" + Version, "KitsInstallationFolder", null);

			if (Result != null)
			{
				FinalResult = ((string)Result).TrimEnd('\\');
			}
			return FinalResult;
		}

		private static string FindWindowsSDKExtensionInstallationFolder()
		{
			string Version;
			switch (WindowsPlatform.Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					if (WindowsPlatform.bUseWindowsSDK10)
					{
						Version = "v10.0";
					}
					else
					{
						return string.Empty;
					}
					break;

				default:
					return string.Empty;
			}

			// Based on VCVarsQueryRegistry
			string FinalResult = null;
			{
				var Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows SDKs\" + Version, "InstallationFolder", null)
						  ?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows SDKs\" + Version, "InstallationFolder", null);
				if (Result == null)
				{
					Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null)
						  ?? Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null);
				}
				if (Result != null)
				{
					FinalResult = ((string)Result).TrimEnd('\\');
				}

			}
			if (FinalResult == null)
			{
				FinalResult = string.Empty;
			}

			return FinalResult;
		}

		static Version FindWindowsSDKExtensionLatestVersion(string WindowsSDKExtensionDir)
		{
			Version LatestVersion = new Version(0, 0, 0, 0);

			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 &&
				WindowsPlatform.bUseWindowsSDK10 &&
				!string.IsNullOrEmpty(WindowsSDKExtensionDir) &&
				Directory.Exists(WindowsSDKExtensionDir))
			{
				string IncludesBaseDirectory = Path.Combine(WindowsSDKExtensionDir, "include");
				if (Directory.Exists(IncludesBaseDirectory))
				{
					string[] IncludeVersions = Directory.GetDirectories(IncludesBaseDirectory);
					foreach (string IncludeVersion in IncludeVersions)
					{
						string VersionString = Path.GetFileName(IncludeVersion);
						Version FoundVersion;
						if (Version.TryParse(VersionString, out FoundVersion) && FoundVersion > LatestVersion)
						{
							LatestVersion = FoundVersion;
						}
					}
				}
			}
			return LatestVersion;
		}

		/// <summary>
		/// Gets the path to the 32bit tool binaries.
		/// </summary>
		static string GetVSToolPath32Bit(string BaseVSToolPath)
		{
			return Path.Combine(BaseVSToolPath, "../../VC/bin");
		}

		/// <summary>
		/// Gets the path to the 64bit tool binaries.
		/// </summary>
		static string GetVSToolPath64Bit(string BaseVSToolPath)
		{
			// Use the native 64-bit compiler if present, otherwise use the amd64-on-x86 compiler. VS2012 Express only includes the latter.
			var Result = Path.Combine(BaseVSToolPath, "../../VC/bin/amd64");
			if (Directory.Exists(Result))
			{
				return Result;
			}

			return Path.Combine(BaseVSToolPath, "../../VC/bin/x86_amd64");
		}

		/// <summary>
		/// Gets the path to the compiler.
		/// </summary>
		static string GetCompilerToolPath(string PlatformVSToolPath)
		{
			// If we were asked to use Clang, then we'll redirect the path to the compiler to the LLVM installation directory
			if (WindowsPlatform.bCompileWithClang)
			{
				string Result;
				if (WindowsPlatform.bUseVCCompilerArgs)
				{
					// Use regular Clang compiler on Windows
					Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LLVM", "msbuild-bin", "cl.exe");
					if (!File.Exists(Result))
					{
						Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "msbuild-bin", "cl.exe");
					}
				}
				else
				{
					// Use 'clang-cl', a wrapper around Clang that supports Visual C++ compiler command-line arguments
					Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LLVM", "bin", "clang.exe");
					if (!File.Exists(Result))
					{
						Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "bin", "clang.exe");
					}
				}
				if (!File.Exists(Result))
				{
					throw new BuildException("Clang was selected as the Windows compiler, but LLVM/Clang does not appear to be installed.  Could not find: " + Result);
				}

				return Result;
			}

			return Path.Combine(PlatformVSToolPath, "cl.exe");
		}

		/// <returns>The version of the compiler.</returns>
		private static Version FindCLExeVersion(string CompilerExe)
		{
			var ExeVersionInfo = FileVersionInfo.GetVersionInfo(CompilerExe);
			if (ExeVersionInfo == null)
			{
				throw new BuildException("Failed to read the version number of: " + CompilerExe);
			}

			return new Version(ExeVersionInfo.FileMajorPart, ExeVersionInfo.FileMinorPart, ExeVersionInfo.FileBuildPart, ExeVersionInfo.FilePrivatePart);
		}

		/// <summary>
		/// Gets the path to the linker.
		/// </summary>
		static string GetLinkerToolPath(string PlatformVSToolPath)
		{
			// If we were asked to use Clang, then we'll redirect the path to the compiler to the LLVM installation directory
			if (WindowsPlatform.bCompileWithClang && WindowsPlatform.bAllowClangLinker)
			{
				var Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LLVM", "bin", "lld.exe");
				if (!File.Exists(Result))
				{
					Result = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "bin", "lld.exe");
				}
				if (!File.Exists(Result))
				{
					throw new BuildException("Clang was selected as the Windows compiler, but LLVM/Clang does not appear to be installed.  Could not find: " + Result);
				}

				return Result;
			}

			return Path.Combine(PlatformVSToolPath, "link.exe");
		}

		/// <summary>
		/// Gets the path to the library linker.
		/// </summary>
		static string GetLibraryLinkerToolPath(string PlatformVSToolPath)
		{
			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker)
			//@todo.WIN32: Using the 64-bit linker appears to be broken at the moment.
			return Path.Combine(PlatformVSToolPath, "lib.exe");
		}

		/// <summary>
		/// Gets the path to the resource compiler's rc.exe for the specified platform.
		/// </summary>
		string GetResourceCompilerToolPath(CPPTargetPlatform Platform, bool bSupportWindowsXP)
		{
			// 64 bit -- we can use the 32 bit version to target 64 bit on 32 bit OS.
			if (Platform == CPPTargetPlatform.Win64 || Platform == CPPTargetPlatform.UWP)
			{
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
				{
					return Path.Combine(WindowsSDKExtensionDir, "bin/x64/rc.exe");
				}
				else
				{
					return Path.Combine(WindowsSDKDir, "bin/x64/rc.exe");
				}
			}

			// @todo UWP: Verify that Windows XP will compile using VS 2015 (it should be supported)
			if (!bSupportWindowsXP)	// Windows XP requires use to force Windows SDK 7.1 even on the newer compiler, so we need the old path RC.exe
			{
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
				{
					return Path.Combine(WindowsSDKExtensionDir, "bin/x86/rc.exe");
				}
				else
				{
					return Path.Combine(WindowsSDKDir, "bin/x86/rc.exe");
				}
			}
			return Path.Combine(WindowsSDKDir, "bin/rc.exe");
		}

		/// <summary>
		/// Gets the path to MSBuild.
		/// </summary>
		static string GetMSBuildToolPath()
		{
			string FrameworkDirectory = Environment.GetEnvironmentVariable("FrameworkDir");
			string FrameworkVersion = Environment.GetEnvironmentVariable("FrameworkVersion");
			if (FrameworkDirectory == null || FrameworkVersion == null)
			{
				throw new BuildException("NOTE: Please ensure that 64bit Tools are installed with DevStudio - there is usually an option to install these during install");
			}

			return Path.Combine(FrameworkDirectory, FrameworkVersion, "MSBuild.exe");
		}

		/// <summary>
		/// Gets the Visual C++ installation folder from the registry
		/// </summary>
		static string FindVisualCppInstallationFolder(WindowsCompiler Version)
		{
			// Get the version string
			string VisualCppVersion;
			switch (WindowsPlatform.Compiler)
			{
				case WindowsCompiler.VisualStudio2015:
					VisualCppVersion = "14.0";
					break;
				case WindowsCompiler.VisualStudio2013:
					VisualCppVersion = "12.0";
					break;
				case WindowsCompiler.VisualStudio2012:
					VisualCppVersion = "11.0";
					break;
				default:
					throw new BuildException("Unexpected compiler version when trying to determine Visual C++ installation folder");
			}

			// Read the registry value
			object Value =
				Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7", VisualCppVersion, null) ??
				Registry.GetValue("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7", VisualCppVersion, null) ??
				Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7", VisualCppVersion, null) ??
				Registry.GetValue("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7", VisualCppVersion, null);

			string InstallDir = Value as string;
			if (InstallDir == null)
			{
				throw new BuildException("Visual C++ must be installed to build this target.");
			}
			return InstallDir;
		}

		/// <summary>
		/// Finds the directory containing the Universal CRT installation. Returns null for Visual Studio versions before 2015
		/// </summary>
		static string FindUniversalCRTInstallationFolder()
		{
			if (WindowsPlatform.Compiler != WindowsCompiler.VisualStudio2015)
			{
				return null;
			}

			object Value =
				Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) ??
				Registry.GetValue("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) ??
				Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) ??
				Registry.GetValue("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null);

			return (Value as string);
		}

		/// <summary>
		/// Gets the version of the Universal CRT to use. As per VCVarsQueryRegistry.bat, this is the directory name that sorts last.
		/// </summary>
		static string FindUniversalCRTVersion(string UniversalCRTDir)
		{
			string UniversalCRTVersion = null;
			if (UniversalCRTDir != null)
			{
				DirectoryInfo IncludeDir = new DirectoryInfo(Path.Combine(UniversalCRTDir, "include"));
				if (IncludeDir.Exists)
				{
					DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories().OrderBy(x => x.Name).LastOrDefault();
					if (LatestIncludeDir != null)
					{
						UniversalCRTVersion = LatestIncludeDir.Name;
					}
				}
			}
			return UniversalCRTVersion;
		}

		/// <summary>
		/// Sets the Visual C++ INCLUDE environment variable
		/// </summary>
		static List<string> GetVisualCppIncludePaths(string VisualCppDir, string UniversalCRTDir, string UniversalCRTVersion, string NetFXSDKDir, string WindowsSDKDir, string WindowsSDKLibVersion)
		{
			List<string> IncludePaths = new List<string>();

			// Add the standard Visual C++ include paths
			string StdIncludeDir = Path.Combine(VisualCppDir, "INCLUDE");
			if (Directory.Exists(StdIncludeDir))
			{
				IncludePaths.Add(StdIncludeDir);
			}
			string AtlMfcIncludeDir = Path.Combine(VisualCppDir, "ATLMFC", "INCLUDE");
			if (Directory.Exists(AtlMfcIncludeDir))
			{
				IncludePaths.Add(AtlMfcIncludeDir);
			}

			// Add the universal CRT paths
			if (!String.IsNullOrEmpty(UniversalCRTDir) && !String.IsNullOrEmpty(UniversalCRTVersion))
			{
				IncludePaths.Add(Path.Combine(UniversalCRTDir, "include", UniversalCRTVersion, "ucrt"));
			}

			// Add the NETFXSDK include path
			if (!String.IsNullOrEmpty(NetFXSDKDir))
			{
				IncludePaths.Add(Path.Combine(NetFXSDKDir, "include", "um")); // 2015
			}

			// Add the Windows SDK paths
			if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 && WindowsPlatform.bUseWindowsSDK10)
			{
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "shared"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "um"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", WindowsSDKLibVersion, "winrt"));
			}
			else
			{
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "shared"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "um"));
				IncludePaths.Add(Path.Combine(WindowsSDKDir, "include", "winrt"));
			}

			// Add the existing include paths
			string ExistingIncludePaths = Environment.GetEnvironmentVariable("INCLUDE");
			if (ExistingIncludePaths != null)
			{
				IncludePaths.AddRange(ExistingIncludePaths.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
			}

			// Set the environment variable
			return IncludePaths;
		}

		/// <summary>
		/// Sets the Visual C++ LIB environment variable
		/// </summary>
		static List<string> GetVisualCppLibraryPaths(string VisualCppDir, string UniversalCRTDir, string UniversalCRTVersion, string NetFXSDKDir, string WindowsSDKDir, string WindowsSDKLibVersion, CPPTargetPlatform Platform)
		{
			List<string> LibraryPaths = new List<string>();

			// Add the standard Visual C++ library paths
			if (Platform == CPPTargetPlatform.UWP && UWPPlatform.bBuildForStore && WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
			{
				string StoreLibraryDir = Path.Combine(VisualCppDir, "LIB", "amd64", "store");
				if (Directory.Exists(StoreLibraryDir))
				{
					LibraryPaths.Add(StoreLibraryDir);
				}
			}
			else if (Platform == CPPTargetPlatform.Win32)
			{
				string StdLibraryDir = Path.Combine(VisualCppDir, "LIB");
				if (Directory.Exists(StdLibraryDir))
				{
					LibraryPaths.Add(StdLibraryDir);
				}
				string AtlMfcLibraryDir = Path.Combine(VisualCppDir, "ATLMFC", "LIB");
				if (Directory.Exists(AtlMfcLibraryDir))
				{
					LibraryPaths.Add(AtlMfcLibraryDir);
				}
			}
			else
			{
				string StdLibraryDir = Path.Combine(VisualCppDir, "LIB", "amd64");
				if (Directory.Exists(StdLibraryDir))
				{
					LibraryPaths.Add(StdLibraryDir);
				}
				string AtlMfcLibraryDir = Path.Combine(VisualCppDir, "ATLMFC", "LIB", "amd64");
				if (Directory.Exists(AtlMfcLibraryDir))
				{
					LibraryPaths.Add(AtlMfcLibraryDir);
				}
			}

			// Add the Universal CRT
			if (!String.IsNullOrEmpty(UniversalCRTDir) && !String.IsNullOrEmpty(UniversalCRTVersion))
			{
				if (Platform == CPPTargetPlatform.Win32)
				{
					LibraryPaths.Add(Path.Combine(UniversalCRTDir, "lib", UniversalCRTVersion, "ucrt", "x86"));
				}
				else
				{
					LibraryPaths.Add(Path.Combine(UniversalCRTDir, "lib", UniversalCRTVersion, "ucrt", "x64"));
				}
			}

			// Add the NETFXSDK include path
			if (!String.IsNullOrEmpty(NetFXSDKDir))
			{
				if (Platform == CPPTargetPlatform.Win32)
				{
					LibraryPaths.Add(Path.Combine(NetFXSDKDir, "lib", "um", "x86"));
				}
				else
				{
					LibraryPaths.Add(Path.Combine(NetFXSDKDir, "lib", "um", "x64"));
				}
			}

			// Add the standard Windows SDK paths
			if (Platform == CPPTargetPlatform.Win32)
			{
				LibraryPaths.Add(Path.Combine(WindowsSDKDir, "lib", WindowsSDKLibVersion, "um", "x86"));
			}
			else
			{
				LibraryPaths.Add(Path.Combine(WindowsSDKDir, "lib", WindowsSDKLibVersion, "um", "x64"));
			}

			// Add the existing library paths
			string ExistingLibraryPaths = Environment.GetEnvironmentVariable("LIB");
			if (ExistingLibraryPaths != null)
			{
				LibraryPaths.AddRange(ExistingLibraryPaths.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
			}

			return LibraryPaths;
		}

		static VCEnvironment EnvVars = null;
	}
}
