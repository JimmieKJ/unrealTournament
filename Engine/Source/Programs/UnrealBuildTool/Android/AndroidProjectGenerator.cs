// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class AndroidProjectGenerator : UEPlatformProjectGenerator
	{
		static bool CheckedForNsight = false;		// whether we have checked for a recent enough version of Nsight yet
		static bool NsightInstalled = false;		// true if a recent enough version of Nsight is installed
		static int NsightVersionCode = 0;			// version code matching detected Nsight

		/// <summary>
		/// Check to see if a recent enough version of Nsight is installed.
		/// </summary>
		bool IsNsightInstalled(VCProjectFileFormat ProjectFileFormat)
		{
			// cache the results since this gets called a number of times
			if (CheckedForNsight)
			{
				return NsightInstalled;
			}

			CheckedForNsight = true;

			// NOTE: there is now a registry key that we can use instead at:
			//			HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\NVIDIA Corporation\Nsight Tegra\Version

			string ProgramFilesPath = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);

			string PlatformToolsetVersion = VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(ProjectFileFormat);
			if (String.IsNullOrEmpty(PlatformToolsetVersion))
			{
				// future maintainer: add toolset version and verify that the rest of the msbuild path, version, and location in ProgramFiles(x86) is still valid
				Log.TraceInformation("Android project generation needs to be updated for this version of Visual Studio.");
				return false;
			}

			// build the path to where the Nsight DLL we'll be checking should sit
			string NsightDllPath = Path.Combine(ProgramFilesPath, @"MSBuild\Microsoft.Cpp\v4.0", PlatformToolsetVersion, @"Platforms\Tegra-Android\Nvidia.Build.CPPTasks.Tegra-Android.Extensibility.dll");

			if (!File.Exists(NsightDllPath))
			{
				return false;
			}

			// grab the version info from the DLL
			FileVersionInfo NsightVersion = FileVersionInfo.GetVersionInfo(NsightDllPath);

			if (NsightVersion.ProductMajorPart > 3)
			{
				// Mark as Nsight 3.1 (project will be updated)
				NsightVersionCode = 11;
				NsightInstalled = true;
			}
			else if (NsightVersion.ProductMajorPart == 3)
			{
				// Nsight 3.0 supported
				NsightVersionCode = 9;
				NsightInstalled = true;

				if (NsightVersion.ProductMinorPart >= 1)
				{
					// Nsight 3.1+ should be valid (will update project if newer)
					NsightVersionCode = 11;
				}
			}
			else if (NsightVersion.ProductMajorPart == 2)
			{
				// Nsight 2.0+ should be valid
				NsightVersionCode = 6;
				NsightInstalled = true;
			}
			else if ((NsightVersion.ProductMajorPart == 1) && (NsightVersion.ProductMinorPart >= 5))
			{
				// Nsight 1.5+ should be valid
				NsightVersionCode = 6;
				NsightInstalled = true;
			}

			if (!NsightInstalled)
			{
				Log.TraceInformation("\nNsight Tegra {0}.{1} found, but Nsight Tegra 1.5 or higher is required for debugging support.", NsightVersion.ProductMajorPart, NsightVersion.ProductMinorPart);
			}

			return NsightInstalled;
		}

		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Android.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Android, this);
		}

		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			// Debugging, etc. are dependent on the TADP being installed
			return IsNsightInstalled(ProjectFileFormat);
		}

		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.Android)
			{
				return "Tegra-Android";
			}

			return InPlatform.ToString();
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public override string GetAdditionalVisualStudioPropertyGroups(UnrealTargetPlatform InPlatform, VCProjectFileFormat ProjectFileFormat)
		{
			if (!IsNsightInstalled(ProjectFileFormat))
			{
				return base.GetAdditionalVisualStudioPropertyGroups(InPlatform, ProjectFileFormat);
			}

			return "	<PropertyGroup Label=\"NsightTegraProject\">" + ProjectFileGenerator.NewLine +
					"		<NsightTegraProjectRevisionNumber>" + NsightVersionCode.ToString() + "</NsightTegraProjectRevisionNumber>" + ProjectFileGenerator.NewLine +
					"	</PropertyGroup>" + ProjectFileGenerator.NewLine;
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public override string GetVisualStudioPlatformConfigurationType(UnrealTargetPlatform InPlatform, VCProjectFileFormat ProjectFileFormat)
		{
			if (!IsNsightInstalled(ProjectFileFormat))
			{
				return base.GetVisualStudioPlatformConfigurationType(InPlatform, ProjectFileFormat);
			}

			return "ExternalBuildSystem";
		}

		/// <summary>
		/// Return the platform toolset string to write into the project configuration
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The custom configuration section for the project file; Empty string if it doesn't require one</returns>
		public override string GetVisualStudioPlatformToolsetString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat InProjectFileFormat)
		{
			if (!IsNsightInstalled(InProjectFileFormat))
			{
				return "\t\t<PlatformToolset>" + VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(InProjectFileFormat) + "</PlatformToolset>" + ProjectFileGenerator.NewLine;
			}

			return "\t\t<PlatformToolset>" + VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(InProjectFileFormat) + "</PlatformToolset>" + ProjectFileGenerator.NewLine
				+ "\t\t<AndroidNativeAPI>UseTarget</AndroidNativeAPI>" + ProjectFileGenerator.NewLine;
		}

		/// <summary>
		/// Return any custom paths for VisualStudio this platform requires
		/// This include ReferencePath, LibraryPath, LibraryWPath, IncludePath and ExecutablePath.
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="TargetType">  The type of target (game or program)</param>
		/// <returns>string    The custom path lines for the project file; Empty string if it doesn't require one</returns>
		public override string GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetRules.TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			if (!IsNsightInstalled(InProjectFileFormat))
			{
				return base.GetVisualStudioPathsEntries(InPlatform, InConfiguration, TargetType, TargetRulesPath, ProjectFilePath, NMakeOutputPath, InProjectFileFormat);
			}

			// NOTE: We are intentionally overriding defaults for these paths with empty strings.  We never want Visual Studio's
			//       defaults for these fields to be propagated, since they are version-sensitive paths that may not reflect
			//       the environment that UBT is building in.  We'll set these environment variables ourselves!
			// NOTE: We don't touch 'ExecutablePath' because that would result in Visual Studio clobbering the system "Path"
			//       environment variable

			//@todo android: clean up debug path generation
			string GameName = TargetRulesPath.GetFileNameWithoutExtension();
			GameName = Path.GetFileNameWithoutExtension(GameName);


			// intermediate path for Engine or Game's intermediate
			string IntermediateDirectoryPath;
			IntermediateDirectoryPath = Path.GetDirectoryName(NMakeOutputPath.FullName) + "/../../Intermediate/Android/APK";

			// string for <OverrideAPKPath>
			string APKPath = Path.Combine(
				Path.GetDirectoryName(NMakeOutputPath.FullName),
				Path.GetFileNameWithoutExtension(NMakeOutputPath.FullName) + "-armv7-es2.apk");

			// string for <BuildXmlPath> and <AndroidManifestPath>
			string BuildXmlPath = IntermediateDirectoryPath;
			string AndroidManifestPath = Path.Combine(IntermediateDirectoryPath, "AndroidManifest.xml");

			// string for <AdditionalLibraryDirectories>
			string AdditionalLibDirs = "";
			AdditionalLibDirs += IntermediateDirectoryPath + @"\obj\local\armeabi-v7a";
			AdditionalLibDirs += ";" + IntermediateDirectoryPath + @"\obj\local\x86";
			AdditionalLibDirs += @";$(AdditionalLibraryDirectories)";

			string PathsLines =
				"		<IncludePath />" + ProjectFileGenerator.NewLine +
				"		<ReferencePath />" + ProjectFileGenerator.NewLine +
				"		<LibraryPath />" + ProjectFileGenerator.NewLine +
				"		<LibraryWPath />" + ProjectFileGenerator.NewLine +
				"		<SourcePath />" + ProjectFileGenerator.NewLine +
				"		<ExcludePath />" + ProjectFileGenerator.NewLine +
				"		<AndroidAttach>False</AndroidAttach>" + ProjectFileGenerator.NewLine +
				"		<DebuggerFlavor>AndroidDebugger</DebuggerFlavor>" + ProjectFileGenerator.NewLine +
				"		<OverrideAPKPath>" + APKPath + "</OverrideAPKPath>" + ProjectFileGenerator.NewLine +
				"		<AdditionalLibraryDirectories>" + AdditionalLibDirs + "</AdditionalLibraryDirectories>" + ProjectFileGenerator.NewLine +
				"		<BuildXmlPath>" + BuildXmlPath + "</BuildXmlPath>" + ProjectFileGenerator.NewLine +
				"		<AndroidManifestPath>" + AndroidManifestPath + "</AndroidManifestPath>" + ProjectFileGenerator.NewLine;

			return PathsLines;
		}
	}
}
