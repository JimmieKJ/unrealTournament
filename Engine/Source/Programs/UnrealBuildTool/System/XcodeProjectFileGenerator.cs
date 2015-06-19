// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a folder within the master project (e.g. Visual Studio solution)
	/// </summary>
	class XcodeProjectFolder : MasterProjectFolder
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public XcodeProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
			// Generate a unique GUID for this folder
			// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
			//       matches identically with the pre-existing file
			FolderGUID = Guid.NewGuid();
		}


		/// GUID for this folder
		public Guid FolderGUID
		{
			get;
			private set;
		}
	}

	public enum XcodeTargetType
	{
		Project,
		Legacy,
		Native,
		XCTest,
		XcodeHelper
	}

	/// <summary>
	/// Represents a build target
	/// </summary>
	public class XcodeProjectTarget
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the target that may be built.</param>
		/// <param name="InType">Specifies the type of the targtet to generate.</param>
		/// <param name="InProductName">Specifies the name of the executable if it differs from the InName</param>
		/// <param name="InTargetPlatform">Name of the target that may be built.</param>
		/// <param name="InDependencies">Name of the target that may be built.</param>
		/// <param name="bHasPlist">Name of the target that may be built.</param>
		public XcodeProjectTarget(string InDisplayName, string InTargetName, XcodeTargetType InType, string InTargetFilePath, string InProductName = "", UnrealTargetPlatform InTargetPlatform = UnrealTargetPlatform.Mac, bool bInIsMacOnly = false, List<XcodeTargetDependency> InDependencies = null, bool bHasPlist = false, List<XcodeFrameworkRef> InFrameworks = null)
		{
			DisplayName = InDisplayName;
			TargetName = InTargetName;
			Type = InType;
			ProductName = InProductName;
			TargetFilePath = InTargetFilePath;
			TargetPlatform = InTargetPlatform;
			bIsMacOnly = bInIsMacOnly;
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			BuildConfigGuild = XcodeProjectFileGenerator.MakeXcodeGuid();
			SourcesPhaseGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			ResourcesPhaseGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			FrameworksPhaseGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			ShellScriptPhaseGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			ProductGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			DebugConfigGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			DevelopmentConfigGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			ShippingConfigGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			TestConfigGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			DebugGameConfigGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			Dependencies = InDependencies == null ? new List<XcodeTargetDependency>() : InDependencies;
			FrameworkRefs = InFrameworks == null ? new List<XcodeFrameworkRef>() : InFrameworks;
			// Meant to prevent adding plists that don't belong to the target, like in the case of Mac builds.
			if (bHasPlist)
			{
				PlistGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			}
		}

		public string DisplayName;					// Name displayed in Xcode's UI.
		public string TargetName;					// Actual name of the target.
		public XcodeTargetType Type;
		public UnrealTargetPlatform TargetPlatform;	//Mac, IOS
		public bool bIsMacOnly;
		public string ProductName;
		public string TargetFilePath;

		public string Guid;
		public string BuildConfigGuild;
		public string SourcesPhaseGuid;
		public string ResourcesPhaseGuid;
		public string FrameworksPhaseGuid;
		public string ShellScriptPhaseGuid;
		public string ProductGuid;
		public string DebugConfigGuid;
		public string DevelopmentConfigGuid;
		public string ShippingConfigGuid;
		public string TestConfigGuid;
		public string DebugGameConfigGuid;
		public string PlistGuid;
		public List<XcodeTargetDependency> Dependencies; // Target dependencies used for chaining Xcode target builds.
		public List<XcodeFrameworkRef> FrameworkRefs;   // Frameworks included in this target.

		public bool HasUI; // true if generates an .app instead of a console exe

		public override string ToString ()
		{
			return string.Format ("[XcodeProjectTarget: {0}, {1}]", DisplayName, Type);
		}
	}

	/// <summary>
	/// Represents a target section.
	/// </summary>
	public class XcodeTargetDependency
	{
		public XcodeTargetDependency(string InLegacyTargetName, string InLegacyTargetGuid, string InContainerItemProxyGuid)
		{
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			LegacyTargetName = InLegacyTargetName;
			LegacyTargetGuid = InLegacyTargetGuid;
			ContainerItemProxyGuid = InContainerItemProxyGuid;
		}
		public string Guid;
		public string LegacyTargetName;
		public string LegacyTargetGuid;
		public string ContainerItemProxyGuid;
	}

	/// <summary>
	/// Represents an item proxy section.
	/// </summary>
	public class XcodeContainerItemProxy
	{
		public XcodeContainerItemProxy(string InProjectGuid, string InLegacyTargetGuid, string InTargetName)
		{
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			ProjectGuid = InProjectGuid;
			LegacyTargetGuid = InLegacyTargetGuid;
			TargetName = InTargetName;
		}
		public string Guid;
		public string ProjectGuid;
		public string LegacyTargetGuid;
		public string TargetName;
	}

	/// <summary>
	/// Represents a Framework.
	/// </summary>
	public class XcodeFramework
	{
		public XcodeFramework(string InName, string InPath, string InSourceTree)
		{
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			Name = InName;
			Path = InPath;
			SourceTree = InSourceTree;
		}
	    public string Name;
	    public string Path;
	    public string SourceTree;
	    public string Guid;
	}

	/// <summary>
	/// Represents a reference to a Framework.
	/// </summary>
	public class XcodeFrameworkRef
	{
		public XcodeFrameworkRef(XcodeFramework InFramework)
		{
			Guid = XcodeProjectFileGenerator.MakeXcodeGuid();
			Framework = InFramework;
		}
		public XcodeFramework Framework;
		public string Guid;
	}

	class XcodeProjectFileGenerator : ProjectFileGenerator
	{
		// always seed the random number the same, so multiple runs of the generator will generate the same project
		static Random Rand = new Random(0);

		/**
		 * Make a random Guid string usable by Xcode (24 characters exactly)
		 */
		public static string MakeXcodeGuid()
		{
			string Guid = "";

			byte[] Randoms = new byte[12];
			Rand.NextBytes(Randoms);
			for (int Index = 0; Index < 12; Index++)
			{
				Guid += Randoms[Index].ToString("X2");
			}

			return Guid;
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".xcodeproj";
			}
		}

		public override void CleanProjectFiles(string InMasterProjectRelativePath, string InMasterProjectName, string InIntermediateProjectFilesPath)
		{
			//@todo Mac. Implement this function...
		}

		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile(string InitFilePath)
		{
			return new XcodeProjectFile(InitFilePath);
		}

		/// ProjectFileGenerator interface
		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new XcodeProjectFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}

		protected override bool WriteMasterProjectFile(ProjectFile UBTProject)
		{
			bool bSuccess = true;
			return bSuccess;
		}

		/// <summary>
		/// Appends the groups section.
		/// </summary>
		/// <param name="Contents">StringBuilder object to append groups string to</param>
		/// <param name="Groups">Dictionary of all project groups</param>
        /// <param name="Frameworks">Frameworks referenced</param>
		private void AppendGroups(ref StringBuilder Contents, ref Dictionary<string, XcodeFileGroup> Groups, List<XcodeProjectTarget> Targets, List<XcodeFramework> Frameworks)
		{
			Contents.Append("/* Begin PBXGroup section */" + ProjectFileGenerator.NewLine);

			// Append main group
			MainGroupGuid = MakeXcodeGuid();
			Contents.Append(string.Format("\t\t{0} = {{{1}", MainGroupGuid, ProjectFileGenerator.NewLine));
			Contents.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);

			foreach (XcodeFileGroup Group in Groups.Values)
			{
				if (!string.IsNullOrEmpty(Group.GroupName))
				{
					Contents.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", Group.GroupGuid, Group.GroupName, ProjectFileGenerator.NewLine));
				}
			}

			foreach (XcodeProjectTarget Target in Targets)
			{
				if (!string.IsNullOrEmpty(Target.PlistGuid) && File.Exists(Path.Combine(RootRelativePath, Target.TargetName + "/Build/IOS/" + Target.TargetName + "-Info.plist")))
				{
					Contents.Append("\t\t\t\t" + Target.PlistGuid + " /* " + Target.TargetName + "-Info.plist */," + ProjectFileGenerator.NewLine);
				}
			}

			ProductRefGroupGuid = MakeXcodeGuid();
            FrameworkGroupGuid = MakeXcodeGuid();
			Contents.Append(string.Format("\t\t\t\t{0} /* Products */,{1}", ProductRefGroupGuid, ProjectFileGenerator.NewLine));
            Contents.Append(string.Format("\t\t\t\t{0} /* Frameworks */,{1}", FrameworkGroupGuid, ProjectFileGenerator.NewLine));
			if (Groups.ContainsKey(""))
			{
				Groups[""].Append(ref Contents, bFilesOnly: true);
			}

			Contents.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t};" + ProjectFileGenerator.NewLine);

			// Add products group
			Contents.Append(string.Format("\t\t{0} = {{{1}", ProductRefGroupGuid, ProjectFileGenerator.NewLine));
			Contents.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);

			foreach (XcodeProjectTarget Target in Targets)
			{
				if (IsXcodeTargetTypeNative(Target.Type))
				{
					Contents.Append("\t\t\t\t" + Target.ProductGuid + " /* " + Target.ProductName + " */," + ProjectFileGenerator.NewLine);
				}
			}
			Contents.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t\tname = Products;" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
			Contents.Append("\t\t};" + ProjectFileGenerator.NewLine);

            // Add Frameworks group
            Contents.Append(string.Format("\t\t{0} = {{{1}", FrameworkGroupGuid, ProjectFileGenerator.NewLine));
            Contents.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
            Contents.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);
            foreach (XcodeFramework Framework in Frameworks)
            {
                Contents.Append("\t\t\t\t" + Framework.Guid + " /* " + Framework.Name + " */," + ProjectFileGenerator.NewLine);
            }
            Contents.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
            Contents.Append("\t\t\tname = Frameworks;" + ProjectFileGenerator.NewLine);
            Contents.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
            Contents.Append("\t\t};" + ProjectFileGenerator.NewLine);

			foreach (XcodeFileGroup Group in Groups.Values)
			{
				if (Group.GroupName != "")
				{
					Group.Append(ref Contents);
				}
			}

			Contents.Append("/* End PBXGroup section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);
		}

		private static bool IsXcodeTargetTypeNative(XcodeTargetType Type)
		{
			return Type == XcodeTargetType.Native || Type == XcodeTargetType.XcodeHelper || Type == XcodeTargetType.XCTest;
		}

		/// <summary>
		/// Appends a target to targets section.
		/// </summary>
		/// <param name="Contents">StringBuilder object to append target string to</param>
		/// <param name="Target">Target to append</param>
		private void AppendTarget(ref StringBuilder Contents, XcodeProjectTarget Target)
		{
			string TargetType = IsXcodeTargetTypeNative(Target.Type) ? "Native" : Target.Type.ToString();
			Contents.Append(
				"\t\t" + Target.Guid + " /* " + Target.DisplayName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = PBX" + TargetType + "Target;" + ProjectFileGenerator.NewLine);

			if (Target.Type == XcodeTargetType.Legacy)
			{
				string UProjectPath = "";
				if (UnrealBuildTool.HasUProjectFile() && !string.IsNullOrEmpty(Target.TargetFilePath) && Utils.IsFileUnderDirectory(Target.TargetFilePath, UnrealBuildTool.GetUProjectPath()))
				{
					if (MasterProjectRelativePath == UnrealBuildTool.GetUProjectPath())
					{
						UProjectPath = " " + "\\\"$(PROJECT_DIR)/" + Path.GetFileName(UnrealBuildTool.GetUProjectFile()) + "\\\"";
					}
					else
					{
						UProjectPath = " " + "\\\"" + UnrealBuildTool.GetUProjectFile() + "\\\"";
					}
				}
				// Xcode provides $ACTION argument for determining if we are building or cleaning a project
				Contents.Append("\t\t\tbuildArgumentsString = \"$(ACTION) " + Target.TargetName + " $(PLATFORM_NAME) $(CONFIGURATION)" + UProjectPath + "\";" + ProjectFileGenerator.NewLine);
			}

			Contents.Append("\t\t\tbuildConfigurationList = " + Target.BuildConfigGuild + " /* Build configuration list for PBX" + TargetType + "Target \"" + Target.DisplayName + "\" */;" + ProjectFileGenerator.NewLine);

			Contents.Append("\t\t\tbuildPhases = (" + ProjectFileGenerator.NewLine);

			if (IsXcodeTargetTypeNative(Target.Type))
			{
				Contents.Append("\t\t\t\t" + Target.SourcesPhaseGuid + " /* Sources */," + ProjectFileGenerator.NewLine);
				//Contents.Append("\t\t\t\t" + Target.ResourcesPhaseGuid + " /* Resources */," + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\t\t" + Target.FrameworksPhaseGuid + " /* Frameworks */," + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\t\t" + Target.ShellScriptPhaseGuid + " /* ShellScript */," + ProjectFileGenerator.NewLine);
			}

			Contents.Append("\t\t\t);" + ProjectFileGenerator.NewLine);

			if (Target.Type == XcodeTargetType.Legacy)
			{
				string UE4Dir = Path.GetFullPath(Directory.GetCurrentDirectory() + "../../..");
				if (bGeneratingRocketProjectFiles)
				{
					Contents.Append("\t\t\tbuildToolPath = \"" + UE4Dir + "/Engine/Build/BatchFiles/Mac/RocketBuild.sh\";" + ProjectFileGenerator.NewLine);
				}
				else
				{
					Contents.Append("\t\t\tbuildToolPath = \"" + UE4Dir + "/Engine/Build/BatchFiles/Mac/Build.sh\";" + ProjectFileGenerator.NewLine);
				}
				Contents.Append("\t\t\tbuildWorkingDirectory = \"" + UE4Dir + "\";" + ProjectFileGenerator.NewLine);
			}

			// This binds the "Run" targets to the "Build" targets.
			Contents.Append("\t\t\tdependencies = (" + ProjectFileGenerator.NewLine);
			foreach (XcodeTargetDependency Dependency in Target.Dependencies)
			{
				Contents.Append("\t\t\t\t" + Dependency.Guid + " /* PBXTargetDependency */" + ProjectFileGenerator.NewLine);
			}
			Contents.Append(
				"\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\tname = \"" + Target.DisplayName + "\";" + ProjectFileGenerator.NewLine);

			if (Target.Type == XcodeTargetType.Legacy)
			{
				Contents.Append("\t\t\tpassBuildSettingsInEnvironment = 0;" + ProjectFileGenerator.NewLine);
			}

			Contents.Append("\t\t\tproductName = \"" + Target.DisplayName + "\";" + ProjectFileGenerator.NewLine);

			if (Target.Type == XcodeTargetType.XcodeHelper)
			{
				Contents.Append(
					"\t\t\tproductReference = " + Target.ProductGuid + " /* " + Target.ProductName + " */;" + ProjectFileGenerator.NewLine +
					"\t\t\tproductType = \"com.apple.product-type.library.static\";" + ProjectFileGenerator.NewLine);
			}
			else if (Target.Type == XcodeTargetType.Native)
			{
				Contents.Append(
					"\t\t\tproductReference = " + Target.ProductGuid + " /* " + Target.ProductName + " */;" + ProjectFileGenerator.NewLine +
					"\t\t\tproductType = \"com.apple.product-type.application\";" + ProjectFileGenerator.NewLine);
			}
			else if (Target.Type == XcodeTargetType.XCTest)
			{
				Contents.Append(
					"\t\t\tproductReference = " + Target.ProductGuid + " /* " + Target.ProductName + " */;" + ProjectFileGenerator.NewLine +
					"\t\t\tproductType = \"com.apple.product-type.bundle.unit-test\";" + ProjectFileGenerator.NewLine);
			}
			Contents.Append("\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendProjectConfig(ref StringBuilder Contents, string ConfigName, string ConfigGuid, string PreprocessorDefinitions, string HeaderSearchPaths)
		{
			string EngineSubdir = (bGeneratingGameProjectFiles || bGeneratingRocketProjectFiles) ? "" : "Engine/";
			Contents.Append(
				"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
				PreprocessorDefinitions +
				HeaderSearchPaths +
				"\t\t\t\tALWAYS_SEARCH_USER_PATHS = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = \"c++0x\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_ENABLE_CPP_RTTI = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_WARN_CHECK_SWITCH_STATEMENTS = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSUPPORTED_PLATFORMS = \"macosx\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tONLY_ACTIVE_ARCH = " + (ConfigName == "Debug" ? "YES;" : "NO;") + ProjectFileGenerator.NewLine +
				"\t\t\t\tUSE_HEADERMAP = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSDKROOT = macosx;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSYMROOT = " + EngineSubdir + "Intermediate/Build;" + ProjectFileGenerator.NewLine +
				"\t\t\t};" + ProjectFileGenerator.NewLine +
				"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendMacBuildConfig(ref StringBuilder Contents, string ConfigName, string ConfigGuid, bool bIsMacOnly)
		{
			string UE4Dir = Path.GetFullPath(Directory.GetCurrentDirectory() + "../../..");
			if (bIsMacOnly)
			{
				Contents.Append(
					"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
					"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
					"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCOMBINE_HIDPI_IMAGES = YES;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tVALID_ARCHS = \"x86_64\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tPRODUCT_NAME = \"$(TARGET_NAME)\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tMACOSX_DEPLOYMENT_TARGET = 10.9;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSDKROOT = macosx;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tGCC_PREFIX_HEADER = \"" + UE4Dir + "/Engine/Source/Editor/UnrealEd/Public/UnrealEd.h\";" + ProjectFileGenerator.NewLine +
					"\t\t\t};" + ProjectFileGenerator.NewLine +
					"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
					"\t\t};" + ProjectFileGenerator.NewLine);
			}
			else
			{
				IOSPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS) as IOSPlatform;
				BuildPlat.SetUpProjectEnvironment(UnrealTargetPlatform.IOS);

				Contents.Append(
					"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
					"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
					"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCOMBINE_HIDPI_IMAGES = YES;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tVALID_ARCHS = \"x86_64 arm64 armv7 armv7s\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSUPPORTED_PLATFORMS = \"macosx iphoneos iphonesimulator\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\t\"PRODUCT_NAME[sdk=macosx*]\" = \"$(TARGET_NAME)\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\t\"PRODUCT_NAME[sdk=iphoneos*]\" = \"$(TARGET_NAME)\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\t\"PRODUCT_NAME[sdk=iphonesimulator*]\" = \"$(TARGET_NAME)-simulator\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tMACOSX_DEPLOYMENT_TARGET = 10.9;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tIPHONEOS_DEPLOYMENT_TARGET = " + BuildPlat.GetRunTimeVersion() + ";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tTARGETED_DEVICE_FAMILY = \"" + BuildPlat.GetRunTimeDevices() + "\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCONFIGURATION_BUILD_DIR = \"Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSDKROOT = macosx;" + ProjectFileGenerator.NewLine +
					"\t\t\t\t\"SDKROOT[arch=x86_64]\" = macosx;" + ProjectFileGenerator.NewLine +
					"\t\t\t\t\"SDKROOT[arch=arm*]\" = iphoneos;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;" + ProjectFileGenerator.NewLine +
					"\t\t\t\tGCC_PREFIX_HEADER = \"" + UE4Dir + "/Engine/Source/Editor/UnrealEd/Public/UnrealEd.h\";" + ProjectFileGenerator.NewLine +
					"\t\t\t};" + ProjectFileGenerator.NewLine +
					"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
					"\t\t};" + ProjectFileGenerator.NewLine);
			}
		}

		private void AppendIOSBuildConfig(ref StringBuilder Contents, string ConfigName, string ConfigGuid)
		{
			IOSPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS) as IOSPlatform;
			BuildPlat.SetUpProjectEnvironment(UnrealTargetPlatform.IOS);

			Contents.Append(
				"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"PRODUCT_NAME[sdk=iphoneos*]\" = \"$(TARGET_NAME)\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"PRODUCT_NAME[sdk=iphonesimulator*]\" = \"$(TARGET_NAME)-simulator\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSUPPORTED_PLATFORMS = \"iphoneos iphonesimulator\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCONFIGURATION_BUILD_DIR = \"Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tIPHONEOS_DEPLOYMENT_TARGET = " + BuildPlat.GetRunTimeVersion() + ";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tTARGETED_DEVICE_FAMILY = \"" + BuildPlat.GetRunTimeDevices() + "\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSDKROOT = iphoneos;" + ProjectFileGenerator.NewLine +
				"\t\t\t};" + ProjectFileGenerator.NewLine +
				"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendIOSRunConfig(ref StringBuilder Contents, string ConfigName, string ConfigGuid, string TargetName, string EngineRelative, string GamePath, bool bIsUE4Game, bool IsAGame, bool bIsUE4Client)
		{
			IOSPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS) as IOSPlatform;
			BuildPlat.SetUpProjectEnvironment(UnrealTargetPlatform.IOS);

			Contents.Append(
				"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"PRODUCT_NAME[sdk=iphoneos*]\" = \"" + TargetName + "\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"PRODUCT_NAME[sdk=iphonesimulator*]\" = \"" + TargetName + "-simulator\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCONFIGURATION_BUILD_DIR = \"Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSUPPORTED_PLATFORMS = \"iphoneos iphonesimulator\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tIPHONEOS_DEPLOYMENT_TARGET = " + BuildPlat.GetRunTimeVersion() + ";" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Developer\";" + ProjectFileGenerator.NewLine);

			string InfoPlistPath = "";
			if (bIsUE4Game)
			{
				InfoPlistPath = EngineRelative + "Engine/Intermediate/IOS/" + TargetName + "-Info.plist";
				Contents.Append(
					"\t\t\t\tCODE_SIGN_RESOURCE_RULES_PATH = \"" + EngineRelative + "Engine/Build/iOS/XcodeSupportFiles/CustomResourceRules.plist\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tINFOPLIST_FILE = \"" + InfoPlistPath + "\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSYMROOT = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tOBJROOT = \"" + EngineRelative + "Engine/Intermediate/IOS/build\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
			}
			else if (bIsUE4Client)
			{
				InfoPlistPath = EngineRelative + "Engine/Intermediate/IOS/UE4Game-Info.plist";
				Contents.Append(
					"\t\t\t\tCODE_SIGN_RESOURCE_RULES_PATH = \"" + EngineRelative + "Engine/Build/iOS/XcodeSupportFiles/CustomResourceRules.plist\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tINFOPLIST_FILE = \"" + InfoPlistPath + "\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSYMROOT = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tOBJROOT = \"" + EngineRelative + "Engine/Intermediate/IOS/build\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
			}
			else if (IsAGame)
			{
				InfoPlistPath = GamePath + "/Intermediate/IOS/" + TargetName + "-Info.plist";
				Contents.Append(
					"\t\t\t\tCODE_SIGN_RESOURCE_RULES_PATH = \"" + EngineRelative + "Engine/Build/iOS/XcodeSupportFiles/CustomResourceRules.plist\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tINFOPLIST_FILE = \"" + InfoPlistPath + "\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSYMROOT = \"" + GamePath + "/Binaries/IOS\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tOBJROOT = \"" + GamePath + "/Intermediate/IOS/build\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + GamePath + "/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
			}
			else
			{
				if (string.IsNullOrEmpty(GamePath))
				{
					InfoPlistPath = EngineRelative + "Engine/Intermediate/IOS/" + TargetName + "-Info.plist";
				}
				else
				{
					InfoPlistPath = GamePath + "/Intermediate/IOS/" + TargetName + "-Info.plist";
				}
				Contents.Append(
					"\t\t\t\tCODE_SIGN_RESOURCE_RULES_PATH = \"" + EngineRelative + "Engine/Build/iOS/XcodeSupportFiles/CustomResourceRules.plist\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tINFOPLIST_FILE = \"" + InfoPlistPath + "\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tSYMROOT = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tOBJROOT = \"" + EngineRelative + "Engine/Intermediate/IOS/build\";" + ProjectFileGenerator.NewLine +
					"\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + EngineRelative + "Engine/Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine);
			}
			Contents.Append(
				"\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tHEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tUSER_HEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tTARGETED_DEVICE_FAMILY = \"" + BuildPlat.GetRunTimeDevices() + "\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSDKROOT = iphoneos;" + ProjectFileGenerator.NewLine +
				"\t\t\t};" + ProjectFileGenerator.NewLine +
				"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine);

			// Prepare a temp Info.plist file so Xcode has some basic info about the target immediately after opening the project.
			// This is needed for the target to pass the settings validation before code signing. UBT will overwrite this plist file later, with proper contents.
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				if (!File.Exists(InfoPlistPath))
				{
					string ProjectPath = GamePath;
					string GameName = TargetName;
					if (string.IsNullOrEmpty (ProjectPath))
					{
						ProjectPath = EngineRelative + "Engine";
					}
					if (bIsUE4Game)
					{
						ProjectPath = EngineRelative + "Engine";
						GameName = "UE4Game";
					}
					Directory.CreateDirectory(Path.GetDirectoryName(InfoPlistPath));
					IOS.UEDeployIOS.GeneratePList(ProjectPath, bIsUE4Game, GameName, TargetName, EngineRelative + "Engine", ProjectPath + "/Binaries/IOS/Payload");
				}
			}
		}

		private void AppendIOSXCTestConfig(ref StringBuilder Contents, string ConfigName, string ConfigGuid, string TargetName, string EngineSubdir, string EngineRelative)
		{
			Contents.Append(
				"\t\t" + ConfigGuid + " /* " + ConfigName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = XCBuildConfiguration;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildSettings = {" + ProjectFileGenerator.NewLine +
				"\t\t\t\tALWAYS_SEARCH_USER_PATHS = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tBUNDLE_LOADER = \"" + EngineSubdir + "Binaries/IOS/Payload/" + TargetName + ".app/" + TargetName + "\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = \"gnu++0x\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCLANG_CXX_LIBRARY = \"libc++\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCLANG_ENABLE_MODULES = YES;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCLANG_ENABLE_OBJC_ARC = YES;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCODE_SIGN_IDENTITY = \"iPhone Developer\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCODE_SIGN_RESOURCE_RULES_PATH = \"" + EngineRelative + "Engine/Build/iOS/XcodeSupportFiles/CustomResourceRules.plist\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCOPY_PHASE_STRIP = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tFRAMEWORK_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\t\"$(SDKROOT)/Developer/Library/Frameworks\"," + ProjectFileGenerator.NewLine +
				"\t\t\t\t\t\"$(inherited)\"," + ProjectFileGenerator.NewLine +
				"\t\t\t\t\t\"$(DEVELOPER_FRAMEWORKS_DIR)\"," + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_C_LANGUAGE_STANDARD = gnu99;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tDYNAMIC_NO_PIC = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tOPTIMIZATION_LEVEL = 0;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t\t\"DEBUG=1\"," + ProjectFileGenerator.NewLine +
				"\t\t\t\t\t\"$(inherited)\"," + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_SYMBOLS_PRIVATE_EXTERN = NO;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tINFOPLIST_FILE = \"" + EngineRelative + "Engine/Build/IOS/UE4CmdLineRun/UE4CmdLineRun-Info.plist\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSYMROOT = \"" + EngineSubdir + "Binaries/IOS\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tOBJROOT = \"" + EngineSubdir + "Intermediate/IOS/build\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tCONFIGURATION_BUILD_DIR = \"" + EngineSubdir + "Binaries/IOS/Payload\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSUPPORTED_PLATFORMS = \"iphoneos iphonesimulator\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tIPHONEOS_DEPLOYMENT_TARGET = 7.0;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tONLY_ACTIVE_ARCH = " + (ConfigName == "Debug" ? "YES;" : "NO;") + ProjectFileGenerator.NewLine +
				"\t\t\t\tPRODUCT_NAME = \"$(TARGET_NAME)\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tSDKROOT = iphoneos;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tTEST_HOST = \"$(BUNDLE_LOADER)\";" + ProjectFileGenerator.NewLine +
				"\t\t\t\tWRAPPER_EXTENSION = xctest;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tHEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t\tUSER_HEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\t};" + ProjectFileGenerator.NewLine +
				"\t\t\tname = " + ConfigName + ";" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine);
		}

		private void AppendSingleConfig(ref StringBuilder Contents, XcodeProjectTarget Target, string ConfigName, string ConfigGuid, string PreprocessorDefinitions, string HeaderSearchPaths,
			string EngineRelative, string GamePath, bool bIsUE4Game, bool IsAGame, bool bIsUE4Client)
		{
			if (Target.Type == XcodeTargetType.Project)
			{
				AppendProjectConfig(ref Contents, ConfigName, ConfigGuid, PreprocessorDefinitions, HeaderSearchPaths);
			}
			else
			{
				if (Target.TargetPlatform == UnrealTargetPlatform.Mac)
				{
					AppendMacBuildConfig(ref Contents, ConfigName, ConfigGuid, Target.bIsMacOnly);
				}
				else
				{
					if (Target.Type == XcodeTargetType.Legacy)
					{
						AppendIOSBuildConfig(ref Contents, ConfigName, ConfigGuid);
					}
					else
					{
						if (Target.Type != XcodeTargetType.XCTest)
						{
							AppendIOSRunConfig(ref Contents, ConfigName, ConfigGuid, Target.TargetName, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
						}
						else
						{
							string EngineSubdir = (bGeneratingGameProjectFiles || bGeneratingRocketProjectFiles) ? "" : "Engine/";
							AppendIOSXCTestConfig(ref Contents, ConfigName, ConfigGuid, Target.TargetName, EngineSubdir, EngineRelative);
						}
					}
				}
			}
		}

		/// <summary>
		/// Appends a build configuration section for specific target.
		/// </summary>
		/// <param name="Contents">StringBuilder object to append build configuration string to</param>
		/// <param name="Target">Target for which we generate the build configuration</param>
		/// <param name="HeaderSearchPaths">Optional string with header search paths section</param>
		/// <param name="PreprocessorDefinitions">Optional string with preprocessor definitions section</param>
		private void AppendBuildConfigs(ref StringBuilder Contents, XcodeProjectTarget Target, string HeaderSearchPaths = "", string PreprocessorDefinitions = "")
		{
			List<string> GameFolders = UEBuildTarget.DiscoverAllGameFolders();

			bool IsAGame = false;
			string GamePath = null;
			string GameNameFromClientName = Target.TargetName.EndsWith("Client") ? Target.TargetName.Remove(Target.TargetName.LastIndexOf("Client")) + "Game" : null;
			bool bIsUE4Game = Target.TargetName.Equals("UE4Game", StringComparison.InvariantCultureIgnoreCase);
			bool bIsUE4Client = Target.TargetName.Equals("UE4Client", StringComparison.InvariantCultureIgnoreCase);
			string EngineRelative = "";

			IUEToolChain Toolchain = UEToolChain.GetPlatformToolChain(CPPTargetPlatform.IOS);
			foreach (string GameFolder in GameFolders)
			{
				if (File.Exists(Path.Combine(GameFolder, Target.TargetName+".uproject")))
				{
					IsAGame = true;
					GamePath = Toolchain.ConvertPath(Path.GetFullPath(GameFolder));
					break;
				}
				else if (!string.IsNullOrEmpty(GameNameFromClientName) && GameFolder.EndsWith(GameNameFromClientName))
				{
					GamePath = Toolchain.ConvertPath(Path.GetFullPath(GameFolder));
					break;
				}
			}

			EngineRelative = Path.GetFullPath(EngineRelativePath + "/../");
			EngineRelative = Toolchain.ConvertPath(EngineRelative);

			if (!bGeneratingRocketProjectFiles)
			{
				AppendSingleConfig(ref Contents, Target, "Debug", Target.DebugConfigGuid, PreprocessorDefinitions, HeaderSearchPaths, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
				AppendSingleConfig(ref Contents, Target, "Test", Target.TestConfigGuid, PreprocessorDefinitions, HeaderSearchPaths, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
			}
			AppendSingleConfig(ref Contents, Target, "DebugGame", Target.DebugGameConfigGuid, PreprocessorDefinitions, HeaderSearchPaths, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
			AppendSingleConfig(ref Contents, Target, "Development", Target.DevelopmentConfigGuid, PreprocessorDefinitions, HeaderSearchPaths, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
			AppendSingleConfig(ref Contents, Target, "Shipping", Target.ShippingConfigGuid, PreprocessorDefinitions, HeaderSearchPaths, EngineRelative, GamePath, bIsUE4Game, IsAGame, bIsUE4Client);
		}

		/// <summary>
		/// Appends a build configuration list section for specific target.
		/// </summary>
		/// <param name="Contents">StringBuilder object to append build configuration list string to</param>
		/// <param name="Target">Target for which we generate the build configuration list</param>
		private void AppendConfigList(ref StringBuilder Contents, XcodeProjectTarget Target)
		{
			string TargetType = IsXcodeTargetTypeNative(Target.Type) ? "Native" : Target.Type.ToString();
			string TypeName = Target.Type == XcodeTargetType.Project ? "PBXProject" : "PBX" + TargetType + "Target";

			if (!bGeneratingRocketProjectFiles)
			{
				Contents.Append(
					"\t\t" + Target.BuildConfigGuild + " /* Build configuration list for " + TypeName + " \"" + Target.DisplayName + "\" */ = {" + ProjectFileGenerator.NewLine +
					"\t\t\tisa = XCConfigurationList;" + ProjectFileGenerator.NewLine +
					"\t\t\tbuildConfigurations = (" + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.DebugConfigGuid + " /* Debug */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.DebugGameConfigGuid + " /* DebugGame */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.DevelopmentConfigGuid + " /* Development */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.ShippingConfigGuid + " /* Shipping */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.TestConfigGuid + " /* Test */," + ProjectFileGenerator.NewLine +
					"\t\t\t);" + ProjectFileGenerator.NewLine +
					"\t\t\tdefaultConfigurationIsVisible = 0;" + ProjectFileGenerator.NewLine +
					"\t\t\tdefaultConfigurationName = Development;" + ProjectFileGenerator.NewLine +
					"\t\t};" + ProjectFileGenerator.NewLine);
			}
			else
			{
				Contents.Append(
					"\t\t" + Target.BuildConfigGuild + " /* Build configuration list for " + TypeName + " \"" + Target.DisplayName + "\" */ = {" + ProjectFileGenerator.NewLine +
					"\t\t\tisa = XCConfigurationList;" + ProjectFileGenerator.NewLine +
					"\t\t\tbuildConfigurations = (" + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.DebugGameConfigGuid + " /* DebugGame */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.DevelopmentConfigGuid + " /* Development */," + ProjectFileGenerator.NewLine +
					"\t\t\t\t" + Target.ShippingConfigGuid + " /* Shipping */," + ProjectFileGenerator.NewLine +
					"\t\t\t);" + ProjectFileGenerator.NewLine +
					"\t\t\tdefaultConfigurationIsVisible = 0;" + ProjectFileGenerator.NewLine +
					"\t\t\tdefaultConfigurationName = Development;" + ProjectFileGenerator.NewLine +
					"\t\t};" + ProjectFileGenerator.NewLine);
			}
		}

		private void AppendBuildPhase(ref string Contents, string Guid, string BuildPhaseName, string Files, List<string> FileRefs)
		{
			Contents += "\t\t" + Guid + " /* " + BuildPhaseName + " */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = PBX" + BuildPhaseName + "BuildPhase;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildActionMask = 2147483647;" + ProjectFileGenerator.NewLine +
				"\t\t\tfiles = (" + ProjectFileGenerator.NewLine;

			if (Files != null)
			{
				Contents += Files;
			}

			Contents += "\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\trunOnlyForDeploymentPostprocessing = 0;" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine;
		}

		private void AppendBuildPhases(List<XcodeProjectTarget> ProjectTargets, ref string PBXBuildFileSection, ref string PBXFileReferenceSection, ref string PBXSourcesBuildPhaseSection,
			ref string PBXResourcesBuildPhaseSection, ref string PBXFrameworksBuildPhaseSection, ref string PBXShellScriptBuildPhaseSection, string TestFrameworkFiles, string UE4CmdLineRunMFileGuid)
		{
			foreach (XcodeProjectTarget Target in ProjectTargets)
			{
				if (IsXcodeTargetTypeNative(Target.Type))
				{
					// Generate Build references and Framework references for each Framework.
					string FrameworkFiles = "";
					foreach (XcodeFrameworkRef FrameworkRef in Target.FrameworkRefs)
					{
						XcodeFramework Framework = FrameworkRef.Framework;

						PBXBuildFileSection += "\t\t" + FrameworkRef.Guid + " /* " + Framework.Name + " in Frameworks */ = {"
							+ "isa = PBXBuildFile; "
							+ "fileRef = " + Framework.Guid
							+ " /* " + Framework.Name + " */;"
							+ " };" + ProjectFileGenerator.NewLine;

						FrameworkFiles += "\t\t\t\t" + FrameworkRef.Guid + " /* " + Framework.Name + " in Frameworks */," + ProjectFileGenerator.NewLine;
					}

					AppendBuildPhase(ref PBXResourcesBuildPhaseSection, Target.ResourcesPhaseGuid, "Resources", null, null);

					string Sources = null;

					if (Target.Type == XcodeTargetType.XCTest)
					{
						// Add the xctest framework.
						FrameworkFiles += TestFrameworkFiles;

						// Normally every project either gets all source files compiled into it or none.
						// We just want one file: UE4CmdLineRun.m
						Sources = "\t\t\t\t" + UE4CmdLineRunMFileGuid + " /* UE4CmdLineRun.m in Sources */," + ProjectFileGenerator.NewLine;
					}

					AppendBuildPhase(ref PBXSourcesBuildPhaseSection, Target.SourcesPhaseGuid, "Sources", Sources, null);

					AppendBuildPhase(ref PBXFrameworksBuildPhaseSection, Target.FrameworksPhaseGuid, "Frameworks", FrameworkFiles, null);

					string PayloadDir = "Engine";
					if (Target.Type == XcodeTargetType.Native && Target.TargetName != "UE4Game")
					{
						PayloadDir = Target.TargetName;
					}
					// add a script for preventing errors during Archive builds
					// this copies the contents of the app generated by UBT into the archiving app
					PBXShellScriptBuildPhaseSection += "\t\t" + Target.ShellScriptPhaseGuid + " /* ShellScript */ = {" + ProjectFileGenerator.NewLine +
						"\t\t\tisa = PBXShellScriptBuildPhase;" + ProjectFileGenerator.NewLine +
						"\t\t\tbuildActionMask = 2147483647;" + ProjectFileGenerator.NewLine +
						"\t\t\tfiles = (" + ProjectFileGenerator.NewLine +
						"\t\t\t);" + ProjectFileGenerator.NewLine +
						"\t\t\trunOnlyForDeploymentPostprocessing = 0;" + ProjectFileGenerator.NewLine +
						"\t\t\tshellPath = /bin/sh;" + ProjectFileGenerator.NewLine +
						"\t\t\tshellScript = \"if [ $DEPLOYMENT_LOCATION = \\\"YES\\\" ]\\nthen\\ncp -R " + PayloadDir +"/Binaries/IOS/Payload/" + Target.ProductName + "/ $DSTROOT/$LOCAL_APPS_DIR/" + Target.ProductName + "\\nfi\";" + ProjectFileGenerator.NewLine +
						"\t\t};" + ProjectFileGenerator.NewLine;
				}
			}
		}

		private void AddPreGeneratedProject(ref string PBXBuildFileSection, ref string PBXFileReferenceSection, ref string PBXSourcesBuildPhaseSection,
			ref Dictionary<string, XcodeFileGroup> Groups, string ProjectPath)
		{
			var ProjectFileName = Utils.MakePathRelativeTo(ProjectPath, MasterProjectRelativePath);
			var XcodeProject = new XcodeProjectFile(ProjectFileName);
			string ProjectDirectory = Path.GetDirectoryName(ProjectPath);
			XcodeProject.AddFilesToProject(SourceFileSearch.FindFiles(new List<string>() { ProjectDirectory }, ExcludeNoRedistFiles: bExcludeNoRedistFiles, SubdirectoryNamesToExclude:new List<string>() { "obj" }, SearchSubdirectories:true, IncludePrivateSourceCode:true), null);
			XcodeProject.GenerateSectionsContents(ref PBXBuildFileSection, ref PBXFileReferenceSection, ref PBXSourcesBuildPhaseSection, ref Groups);
		}

		private void AppendSourceFiles(List<XcodeProjectTarget> ProjectTargets, ref string PBXBuildFileSection, ref string PBXFileReferenceSection, ref string PBXSourcesBuildPhaseSection,
			ref Dictionary<string, XcodeFileGroup> Groups, XcodeProjectTarget UE4XcodeHelperTarget, string UE4CmdLineRunMFileGuid, ref List<string> IncludeDirectories,
			ref List<string> SystemIncludeDirectories, ref List<string> PreprocessorDefinitions)
		{
			PBXSourcesBuildPhaseSection += "\t\t" + UE4XcodeHelperTarget.SourcesPhaseGuid + " /* Sources */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = PBXSourcesBuildPhase;" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildActionMask = 2147483647;" + ProjectFileGenerator.NewLine +
				"\t\t\tfiles = (" + ProjectFileGenerator.NewLine;

			PBXFileReferenceSection += "\t\t" + UE4XcodeHelperTarget.ProductGuid + " /* " + UE4XcodeHelperTarget.ProductName + " */ = {isa = PBXFileReference; explicitFileType = archive.ar; includeInIndex = 0; path = " + UE4XcodeHelperTarget.ProductName + "; sourceTree = BUILT_PRODUCTS_DIR; };" + ProjectFileGenerator.NewLine;
			foreach (var CurProject in GeneratedProjectFiles)
			{
				bool bHasTarget = false;
				foreach (var TargetFile in CurProject.ProjectTargets)
				{
					if (TargetFile.TargetFilePath != null)
					{
						string TargetFileName = Path.GetFileNameWithoutExtension(TargetFile.TargetFilePath);
						string TargetFileMinusTarget = TargetFileName.Substring(0, TargetFileName.LastIndexOf(".Target"));
						foreach (XcodeProjectTarget Target in ProjectTargets)
						{
							if (TargetFileMinusTarget == Target.TargetName)
							{
								bHasTarget = true;
								break;
							}
						}
						if (bHasTarget)
						{
							break;
						}
					}
				}

				if (!bHasTarget)
				{
					continue;
				}

				XcodeProjectFile XcodeProject = CurProject as XcodeProjectFile;
				if (XcodeProject == null)
				{
					continue;
				}

				// Necessary so that GenerateSectionContents can use the same GUID instead of
				// auto-generating one for this special-case file.
				XcodeProject.UE4CmdLineRunFileGuid = UE4CmdLineRunMFileGuid;
				XcodeProject.UE4CmdLineRunFileRefGuid = UE4CmdLineRunMFileRefGuid;

				if (bGeneratingRunIOSProject)
				{
					foreach (var CurSourceFile in XcodeProject.SourceFiles)
					{
						XcodeSourceFile SourceFile = CurSourceFile as XcodeSourceFile;
						string GroupPath = Path.GetFullPath(Path.GetDirectoryName(SourceFile.FilePath));
						XcodeFileGroup Group = XcodeProject.FindGroupByFullPath(ref Groups, GroupPath);
						if (Group != null)
						{
							Group.bReference = true;
						}
					}
				}
				else
				{
					XcodeProject.GenerateSectionsContents(ref PBXBuildFileSection, ref PBXFileReferenceSection, ref PBXSourcesBuildPhaseSection, ref Groups);
				}

				foreach (var CurPath in XcodeProject.IntelliSenseIncludeSearchPaths)
				{
					AddIncludeDirectory(ref IncludeDirectories, CurPath, Path.GetDirectoryName(XcodeProject.ProjectFilePath));
				}

				foreach (var CurPath in XcodeProject.IntelliSenseSystemIncludeSearchPaths)
				{
					AddIncludeDirectory(ref SystemIncludeDirectories, CurPath, Path.GetDirectoryName(XcodeProject.ProjectFilePath));
				}

				foreach (var CurDefinition in XcodeProject.IntelliSensePreprocessorDefinitions)
				{
					string Definition = CurDefinition;
					string AlternateDefinition = Definition.Contains("=0") ? Definition.Replace("=0", "=1") : Definition.Replace("=1", "=0");
					if (Definition.Equals("WITH_EDITORONLY_DATA=0") || Definition.Equals("WITH_DATABASE_SUPPORT=1"))
					{
						Definition = AlternateDefinition;
					}
					if (!PreprocessorDefinitions.Contains(Definition) && !PreprocessorDefinitions.Contains(AlternateDefinition) && !Definition.StartsWith("UE_ENGINE_DIRECTORY") && !Definition.StartsWith("ORIGINAL_FILE_NAME"))
					{
						PreprocessorDefinitions.Add(Definition);
					}
				}
			}
			PBXSourcesBuildPhaseSection += "\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\trunOnlyForDeploymentPostprocessing = 0;" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine;

			if (!bGeneratingRocketProjectFiles)
			{
				// Add UnrealBuildTool to the master project
				string ProjectPath = System.IO.Path.Combine(System.IO.Path.Combine(EngineRelativePath, "Source"), "Programs", "UnrealBuildTool", "UnrealBuildTool.csproj");
				AddPreGeneratedProject(ref PBXBuildFileSection, ref PBXFileReferenceSection, ref PBXSourcesBuildPhaseSection, ref Groups, ProjectPath);
			}
		}

		private void AppendReferenceGroups(ref string PBXFileReferenceSection, ref Dictionary<string, XcodeFileGroup> Groups)
		{
			string RelativePath = "../../../";

			foreach (XcodeFileGroup Group in Groups.Values)
			{
				if (!string.IsNullOrEmpty(Group.GroupName))
				{
					Group.bReference = true;
					string GroupPath = RelativePath + Group.GroupPath;

					// Add File reference.
					PBXFileReferenceSection += "\t\t" + Group.GroupGuid + " /* " + Group.GroupName + " */ = {"
											+ "isa = PBXFileReference; lastKnownFileType = folder; "
											+ "name = " + Group.GroupName + "; "
											+ "path = " + Utils.CleanDirectorySeparators(GroupPath, '/') + "; sourceTree = \"<group>\"; };" + ProjectFileGenerator.NewLine;
				}
			}
		}

		/// <summary>
		/// Creates the container item proxy section to be added to the PBX Project.
		/// </summary>
		/// <param name="PBXTargetDependencySection">String to be modified with the container item proxy objects.</param>
		/// <param name="TargetDependencies">List of container item proxies to add.</param>
		private void CreateContainerItemProxySection(ref string PBXContainerItemProxySection, List<XcodeContainerItemProxy> ContainerItemProxies)
		{
			foreach(XcodeContainerItemProxy ContainerItemProxy in ContainerItemProxies)
			{
				PBXContainerItemProxySection += "\t\t" + ContainerItemProxy.Guid + " /* PBXContainerItemProxy */ = {" + ProjectFileGenerator.NewLine +
												"\t\t\tisa = PBXContainerItemProxy;" + ProjectFileGenerator.NewLine +
												"\t\t\tcontainerPortal = " + ContainerItemProxy.ProjectGuid + " /* Project object */;" + ProjectFileGenerator.NewLine +
												"\t\t\tproxyType = 1;" + ProjectFileGenerator.NewLine +
												"\t\t\tremoteGlobalIDString = " + ContainerItemProxy.LegacyTargetGuid + ";" + ProjectFileGenerator.NewLine +
												"\t\t\tremoteInfo = \"" + ContainerItemProxy.TargetName + "\";" + ProjectFileGenerator.NewLine +
												"\t\t};" + ProjectFileGenerator.NewLine;
			}
		}

		/// <summary>
		/// Creates the target dependency section to be added to the PBX Project.
		/// </summary>
		/// <param name="PBXTargetDependencySection">String to be modified with the target dependency objects.</param>
		/// <param name="TargetDependencies">List of target dependencies to add.</param>
		private void CreateTargetDependencySection(ref string PBXTargetDependencySection, List<XcodeTargetDependency> TargetDependencies)
		{
			foreach(XcodeTargetDependency TargetDependency in TargetDependencies)
			{
				PBXTargetDependencySection +=	"\t\t" + TargetDependency.Guid + " /* PBXTargetDependency */ = {" + ProjectFileGenerator.NewLine +
												"\t\t\tisa = PBXTargetDependency;" + ProjectFileGenerator.NewLine +
												"\t\t\ttarget = " + TargetDependency.LegacyTargetGuid + " /* " + TargetDependency.LegacyTargetName + " */;" + ProjectFileGenerator.NewLine +
												"\t\t\ttargetProxy = " + TargetDependency.ContainerItemProxyGuid + " /* PBXContainerItemProxy */;" + ProjectFileGenerator.NewLine +
												"\t\t};" + ProjectFileGenerator.NewLine;
			}
		}

		/// Adds the include directory to the list, after converting it to relative to UE4 root
		private void AddIncludeDirectory(ref List<string> IncludeDirectories, string IncludeDir, string ProjectDir)
		{
			string FullProjectPath = Path.GetFullPath(ProjectFileGenerator.MasterProjectRelativePath);
			string FullPath = "";
			if (IncludeDir.StartsWith("/") && !IncludeDir.StartsWith(FullProjectPath))
			{
				// Full path to a fulder outside of project
				FullPath = IncludeDir;
			}
			else
			{
				FullPath = Path.GetFullPath(Path.Combine(ProjectDir, IncludeDir));
				FullPath = Utils.MakePathRelativeTo(FullPath, FullProjectPath);
				FullPath = FullPath.TrimEnd('/');
			}
			if ( !IncludeDirectories.Contains(FullPath) )
			{
				IncludeDirectories.Add(FullPath);
			}
		}

		private void PopulateTargets(List<string> GamePaths, List<XcodeProjectTarget> ProjectTargets, List<XcodeContainerItemProxy> ContainerItemProxies, List<XcodeTargetDependency> TargetDependencies, XcodeProjectTarget ProjectTarget, List<XcodeFramework> Frameworks)
		{
			foreach (string TargetPath in GamePaths)
			{
				string TargetName = Utils.GetFilenameWithoutAnyExtensions(Path.GetFileName(TargetPath));
				bool WantProjectFileForTarget = true;
				bool IsEngineTarget = false;
				if (bGeneratingGameProjectFiles || bGeneratingRocketProjectFiles)
				{
					// Check to see if this is an Engine target.  That is, the target is located under the "Engine" folder
					string TargetFileRelativeToEngineDirectory = Utils.MakePathRelativeTo(TargetPath, Path.Combine(EngineRelativePath), AlwaysTreatSourceAsDirectory: false);
					if (!TargetFileRelativeToEngineDirectory.StartsWith("..") && !Path.IsPathRooted(TargetFileRelativeToEngineDirectory))
					{
						// This is an engine target
						IsEngineTarget = true;
					}

					if (IsEngineTarget)
					{
						if (!IncludeEngineSource)
						{
							// We were asked to exclude engine modules from the generated projects
							WantProjectFileForTarget = false;
						}
						if (bGeneratingGameProjectFiles && this.GameProjectName == TargetName)
						{
							WantProjectFileForTarget = true;
						}
					}

					if (bGeneratingRocketProjectFiles && TargetName.EndsWith("Server"))
					{
						WantProjectFileForTarget = false;
					}
				}

				if (WantProjectFileForTarget)
				{
					string TargetFilePath;
					var Target = new TargetInfo(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Development);
					var TargetRulesObject = RulesCompiler.CreateTargetRules(TargetName, Target, false, out TargetFilePath);
					List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
					TargetRulesObject.GetSupportedPlatforms(ref SupportedPlatforms);
					LinkEnvironmentConfiguration LinkConfiguration = new LinkEnvironmentConfiguration();
					CPPEnvironmentConfiguration CPPConfiguration = new CPPEnvironmentConfiguration();
					TargetRulesObject.SetupGlobalEnvironment(Target, ref LinkConfiguration, ref CPPConfiguration);

					if (!LinkConfiguration.bIsBuildingConsoleApplication)
					{
						TargetsThatNeedApp.Add(TargetName);
					}

					// if the project is not an engine project check to make sure we have the correct name
					string DisplayName = TargetName;
					if (!IsEngineTarget && TargetRulesObject.Type != TargetRules.TargetType.Program && TargetRulesObject.Type != TargetRules.TargetType.Client)
					{
						List<UProjectInfo> AllGames = UProjectInfo.FilterGameProjects(true, bGeneratingGameProjectFiles ? GameProjectName : null);
						UProjectInfo ProjectInfo = FindGameContainingFile (AllGames, TargetFilePath);
						if (ProjectInfo != null)
						{
							DisplayName = ProjectInfo.GameName;
							if (TargetName.Contains("Editor"))
							{
								DisplayName += "Editor";
							}
							else if (TargetName.Contains("Server"))
							{
								DisplayName += "Server";
							}
						}
					}

					// @todo: Remove target platform param and merge Mac and iOS targets. For now BuildTarget knows how to build iOS, but cannot run iOS apps, so we need separate DeployTarget.
					bool bIsMacOnly = !SupportedPlatforms.Contains(UnrealTargetPlatform.IOS);

					XcodeProjectTarget BuildTarget = new XcodeProjectTarget(DisplayName + " - Mac", TargetName, XcodeTargetType.Legacy, TargetFilePath, "", UnrealTargetPlatform.Mac, bIsMacOnly);
					if (!bGeneratingRunIOSProject)
					{
						ProjectTargets.Add(BuildTarget);
					}

					if (ProjectFilePlatform.HasFlag(XcodeProjectFilePlatform.iOS) && SupportedPlatforms.Contains(UnrealTargetPlatform.IOS))
					{
						if ((bGeneratingRocketProjectFiles && TargetName == "UE4Game") || bGeneratingRunIOSProject)
						{
							// Generate Framework references.
							List<XcodeFrameworkRef> FrameworkRefs = new List<XcodeFrameworkRef>();
							foreach (XcodeFramework Framework in Frameworks)
							{
								FrameworkRefs.Add(new XcodeFrameworkRef(Framework));
							}

							XcodeProjectTarget IOSDeployTarget = new XcodeProjectTarget(DisplayName + " - iOS", TargetName, XcodeTargetType.Native, TargetFilePath, TargetName + ".app", UnrealTargetPlatform.IOS, false, null, true, FrameworkRefs);
							ProjectTargets.Add(IOSDeployTarget);
						}
						else
						{
							XcodeContainerItemProxy ContainerProxy = new XcodeContainerItemProxy(ProjectTarget.Guid, BuildTarget.Guid, BuildTarget.DisplayName);
							XcodeTargetDependency TargetDependency = new XcodeTargetDependency(BuildTarget.DisplayName, BuildTarget.Guid, ContainerProxy.Guid);
							XcodeProjectTarget IOSDeployTarget = new XcodeProjectTarget(DisplayName + " - iOS", TargetName, XcodeTargetType.Native, TargetFilePath, TargetName + ".app", UnrealTargetPlatform.IOS, false, new List<XcodeTargetDependency>() { TargetDependency }, true);
							ProjectTargets.Add(IOSDeployTarget);
							ContainerItemProxies.Add(ContainerProxy);
							TargetDependencies.Add(TargetDependency);
						}
					}
				}
			}
		}

		/// <summary>
		/// Writes a scheme file that holds user-specific info needed for debugging.
		/// </summary>
		/// <param name="Target">Target for which we write the scheme file</param>
		/// <param name="ExeExtension">Extension of the executable used to run and debug this target (".app" for bundles, "" for command line apps</param>
		/// <param name="ExeBaseName">Base name of the executable used to run and debug this target</param>
		/// <param name="Args">List of command line arguments for running this target</param>
		private void WriteSchemeFile(string XcodeProjectPath, XcodeProjectTarget Target, string ExeExtension = "", string ExeBaseName = "", List<string> Args = null)
		{
			if (ExeBaseName == "")
			{
				ExeBaseName = Target.TargetName;
			}

			string TargetBinariesFolder;
			if (ExeBaseName.StartsWith("/"))
			{
				TargetBinariesFolder = Path.GetDirectoryName(ExeBaseName);
				ExeBaseName = Path.GetFileName(ExeBaseName);
			}
			else
			{
				TargetBinariesFolder = Path.GetDirectoryName(Directory.GetCurrentDirectory());

				if (ExeBaseName == Target.TargetName && ExeBaseName.EndsWith("Game") && ExeExtension == ".app")
				{
					TargetBinariesFolder = TargetBinariesFolder.Replace("/Engine", "/") + ExeBaseName + "/Binaries/";
				}
				else
				{
					TargetBinariesFolder += "/Binaries/";
				}

				// append the platform directory
				TargetBinariesFolder += Target.TargetPlatform.ToString();
			}

			string SchemeFileContent =
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + ProjectFileGenerator.NewLine +
				"<Scheme" + ProjectFileGenerator.NewLine +
				"  LastUpgradeVersion = \"0500\"" + ProjectFileGenerator.NewLine +
				"  version = \"1.3\">" + ProjectFileGenerator.NewLine +
				"  <BuildAction" + ProjectFileGenerator.NewLine +
				"     parallelizeBuildables = \"YES\"" + ProjectFileGenerator.NewLine +
				"     buildImplicitDependencies = \"YES\">" + ProjectFileGenerator.NewLine +
				"     <BuildActionEntries>" + ProjectFileGenerator.NewLine +
				"        <BuildActionEntry" + ProjectFileGenerator.NewLine +
				"           buildForTesting = \"YES\"" + ProjectFileGenerator.NewLine +
				"           buildForRunning = \"YES\"" + ProjectFileGenerator.NewLine +
				"           buildForProfiling = \"YES\"" + ProjectFileGenerator.NewLine +
				"           buildForArchiving = \"YES\"" + ProjectFileGenerator.NewLine +
				"           buildForAnalyzing = \"YES\">" + ProjectFileGenerator.NewLine +
				"           <BuildableReference" + ProjectFileGenerator.NewLine +
				"              BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine +
				"              BlueprintIdentifier = \"" + Target.Guid + "\"" + ProjectFileGenerator.NewLine +
				"              BuildableName = \"" + Target.ProductName + "\"" + ProjectFileGenerator.NewLine +
				"              BlueprintName = \"" + Target.DisplayName + "\"" + ProjectFileGenerator.NewLine +
				"              ReferencedContainer = \"container:" + MasterProjectName + ".xcodeproj\">" + ProjectFileGenerator.NewLine +
				"           </BuildableReference>" + ProjectFileGenerator.NewLine +
				"        </BuildActionEntry>" + ProjectFileGenerator.NewLine +
				"     </BuildActionEntries>" + ProjectFileGenerator.NewLine +
				"  </BuildAction>" + ProjectFileGenerator.NewLine +
				"  <TestAction" + ProjectFileGenerator.NewLine +
				"      selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"" + ProjectFileGenerator.NewLine +
				"     selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"" + ProjectFileGenerator.NewLine +
				"     shouldUseLaunchSchemeArgsEnv = \"YES\"" + ProjectFileGenerator.NewLine +
				"      buildConfiguration = \"Debug" + (bGeneratingRocketProjectFiles ? "Game" : "") + "\">" + ProjectFileGenerator.NewLine +
				"     <Testables>" + ProjectFileGenerator.NewLine +
				"        <TestableReference" + ProjectFileGenerator.NewLine +
				"           skipped = \"NO\">" + ProjectFileGenerator.NewLine +
				"           <BuildableReference" + ProjectFileGenerator.NewLine +
				"              BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine +
				"              BlueprintIdentifier = \"" + UE4CmdLineGuid + "\"" + ProjectFileGenerator.NewLine +
				"              BuildableName = \"UE4CmdLineRun.xctest\"" + ProjectFileGenerator.NewLine +
				"              BlueprintName = \"UE4CmdLineRun\"" + ProjectFileGenerator.NewLine +
				"              ReferencedContainer = \"container:" + MasterProjectName + ".xcodeproj\">" + ProjectFileGenerator.NewLine +
				"           </BuildableReference>" + ProjectFileGenerator.NewLine +
				"        </TestableReference>" + ProjectFileGenerator.NewLine +
				"     </Testables>" + ProjectFileGenerator.NewLine +
				"     <MacroExpansion>" + ProjectFileGenerator.NewLine +
				"        <BuildableReference" + ProjectFileGenerator.NewLine +
				"           BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine +
				"           BlueprintIdentifier = \"" + Target.Guid + "\"" + ProjectFileGenerator.NewLine +
				"           BuildableName = \"" + Target.ProductName + "\"" + ProjectFileGenerator.NewLine +
				"           BlueprintName = \"" + Target.DisplayName + "\"" + ProjectFileGenerator.NewLine +
				"           ReferencedContainer = \"container:" + MasterProjectName + ".xcodeproj\">" + ProjectFileGenerator.NewLine +
				"        </BuildableReference>" + ProjectFileGenerator.NewLine +
				"     </MacroExpansion>" + ProjectFileGenerator.NewLine +
				"  </TestAction>" + ProjectFileGenerator.NewLine +
				"  <LaunchAction" + ProjectFileGenerator.NewLine +
				"     selectedDebuggerIdentifier = \"Xcode.DebuggerFoundation.Debugger.LLDB\"" + ProjectFileGenerator.NewLine +
				"     selectedLauncherIdentifier = \"Xcode.DebuggerFoundation.Launcher.LLDB\"" + ProjectFileGenerator.NewLine +
				"     launchStyle = \"0\"" + ProjectFileGenerator.NewLine +
				"     useCustomWorkingDirectory = \"NO\"" + ProjectFileGenerator.NewLine +
				"     buildConfiguration = \"Debug" + (bGeneratingRocketProjectFiles ? "Game" : "") + "\"" + ProjectFileGenerator.NewLine +
				"     ignoresPersistentStateOnLaunch = \"NO\"" + ProjectFileGenerator.NewLine +
				"     debugDocumentVersioning = \"YES\"" + ProjectFileGenerator.NewLine +
				"     allowLocationSimulation = \"YES\">" + ProjectFileGenerator.NewLine;
			if (Target.TargetPlatform == UnrealTargetPlatform.Mac)
			{
				// For non-rocket projects the default Run targtet is always in Debug config, so we always add -Mac-Debug suffix.
				// For rocket projects, we have DebugGame config, so the editor runs in Development mode (no suffix), but the game in Debug (so it needs suffix).
				string ExeConfigSuffix = (!bGeneratingRocketProjectFiles || !ExeBaseName.EndsWith("Editor")) ? "-Mac-Debug" : "";
				SchemeFileContent += "     <PathRunnable" + ProjectFileGenerator.NewLine +
									 "        FilePath = \"" + TargetBinariesFolder + "/" + ExeBaseName + ExeConfigSuffix + ExeExtension + "\">" + ProjectFileGenerator.NewLine +
									 "     </PathRunnable>" + ProjectFileGenerator.NewLine;
			}
			else
			{
				SchemeFileContent +=
					"     <BuildableProductRunnable>" + ProjectFileGenerator.NewLine +
					"        <BuildableReference" + ProjectFileGenerator.NewLine +
					"           BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine +
					"           BlueprintIdentifier = \"" + Target.Guid + "\"" + ProjectFileGenerator.NewLine +
					"           BuildableName = \"" + Target.ProductName + "\"" + ProjectFileGenerator.NewLine +
					"           BlueprintName = \"" + Target.DisplayName + "\"" + ProjectFileGenerator.NewLine +
					"           ReferencedContainer = \"container:" + MasterProjectName + ".xcodeproj\">" + ProjectFileGenerator.NewLine +
					"        </BuildableReference>" + ProjectFileGenerator.NewLine +
					"     </BuildableProductRunnable>" + ProjectFileGenerator.NewLine;
			}

			if (Args != null)
			{
				SchemeFileContent += "     <CommandLineArguments>" + ProjectFileGenerator.NewLine;

				Args.Add("-debug");

				foreach (string Arg in Args)
				{
					SchemeFileContent +=
						"        <CommandLineArgument" + ProjectFileGenerator.NewLine +
						"           argument = \"" + Arg + "\"" + ProjectFileGenerator.NewLine +
						"           isEnabled = \"YES\">" + ProjectFileGenerator.NewLine +
						"        </CommandLineArgument>" + ProjectFileGenerator.NewLine;
				}
				SchemeFileContent += "      </CommandLineArguments>" + ProjectFileGenerator.NewLine;
			}

			string Runnable = TargetBinariesFolder + "/" + ExeBaseName + ExeExtension;
			SchemeFileContent +=
				"     <AdditionalOptions>" + ProjectFileGenerator.NewLine +
				"     </AdditionalOptions>" + ProjectFileGenerator.NewLine +
				"  </LaunchAction>" + ProjectFileGenerator.NewLine +
				"  <ProfileAction" + ProjectFileGenerator.NewLine +
				"     shouldUseLaunchSchemeArgsEnv = \"YES\"" + ProjectFileGenerator.NewLine +
				"     savedToolIdentifier = \"\"" + ProjectFileGenerator.NewLine +
				"     useCustomWorkingDirectory = \"NO\"" + ProjectFileGenerator.NewLine +
				"     buildConfiguration = \"Development\"" + ProjectFileGenerator.NewLine +
				"     debugDocumentVersioning = \"YES\">" + ProjectFileGenerator.NewLine +
				"     <PathRunnable" + ProjectFileGenerator.NewLine +
				"        FilePath = \"" + Runnable + "\">" + ProjectFileGenerator.NewLine +
				"     </PathRunnable>" + ProjectFileGenerator.NewLine +
				"     <BuildableProductRunnable>" + ProjectFileGenerator.NewLine +
				"         <BuildableReference" + ProjectFileGenerator.NewLine +
				"             BuildableIdentifier = \"primary\"" + ProjectFileGenerator.NewLine +
				"	          BlueprintIdentifier = \"" + Target.Guid + "\"" + ProjectFileGenerator.NewLine +
				"             BuildableName = \"" + ExeBaseName + ExeExtension + "\"" + ProjectFileGenerator.NewLine +
				"             BlueprintName = \"" + ExeBaseName + "\"" + ProjectFileGenerator.NewLine +
				"             ReferencedContainer = \"container:UE4.xcodeproj\">" + ProjectFileGenerator.NewLine +
				"         </BuildableReference>" + ProjectFileGenerator.NewLine +
				"     </BuildableProductRunnable>" + ProjectFileGenerator.NewLine +
				"  </ProfileAction>" + ProjectFileGenerator.NewLine +
				"  <AnalyzeAction" + ProjectFileGenerator.NewLine +
				"     buildConfiguration = \"Debug" + (bGeneratingRocketProjectFiles ? "Game" : "") + "\">" + ProjectFileGenerator.NewLine +
				"  </AnalyzeAction>" + ProjectFileGenerator.NewLine +
				"  <ArchiveAction" + ProjectFileGenerator.NewLine +
				"     buildConfiguration = \"Development\"" + ProjectFileGenerator.NewLine +
				"     revealArchiveInOrganizer = \"YES\">" + ProjectFileGenerator.NewLine +
				"  </ArchiveAction>" + ProjectFileGenerator.NewLine +
				"</Scheme>" + ProjectFileGenerator.NewLine;

			string SchemesDir = XcodeProjectPath + "/xcuserdata/" + Environment.UserName + ".xcuserdatad/xcschemes";
			if (!Directory.Exists(SchemesDir))
			{
				Directory.CreateDirectory(SchemesDir);
			}

			string SchemeFilePath = SchemesDir + Path.DirectorySeparatorChar + Target.DisplayName + ".xcscheme";
			File.WriteAllText(SchemeFilePath, SchemeFileContent);
		}

		private void WriteSchemes(string XcodeProjectPath, List<XcodeProjectTarget> ProjectTargets)
		{
			// Delete obsolete schemes
			string SchemesDir = XcodeProjectPath + "/xcuserdata/" + Environment.UserName + ".xcuserdatad/xcschemes";
			if (Directory.Exists(SchemesDir))
			{
				var ObsoleteSchemes = Directory.GetFiles(SchemesDir, "*iOS (*.xcscheme", SearchOption.AllDirectories);
				foreach (string SchemeFile in ObsoleteSchemes)
				{
					File.Delete(SchemeFile);
				}
			}

			// write scheme files for targets
			foreach (var Target in ProjectTargets)
			{
				if (Target.Type == XcodeTargetType.Project)
				{
					continue;
				}

				if (UnrealBuildTool.HasUProjectFile() && !string.IsNullOrEmpty(Target.TargetFilePath) && Utils.IsFileUnderDirectory(Target.TargetFilePath, UnrealBuildTool.GetUProjectPath()))
				{
					if (Target.TargetName.EndsWith("Editor"))
					{
						WriteSchemeFile(XcodeProjectPath, Target, ".app", "UE4Editor", new List<string>(new string[] { "&quot;" + UnrealBuildTool.GetUProjectFile() + "&quot;" }));
					}
					else
					{
						string ProjectBinariesDir = UnrealBuildTool.GetUProjectPath() + "/Binaries/Mac/";
						WriteSchemeFile(XcodeProjectPath, Target, ".app", ProjectBinariesDir + Target.TargetName, new List<string>(new string[] { "&quot;" + UnrealBuildTool.GetUProjectFile() + "&quot;" }));
					}
				}
				else if (Target.TargetName.EndsWith("Game"))
				{
					string ExeBaseName = Target.TargetName;
					List<string> Args = null;
					if (Target.TargetPlatform == UnrealTargetPlatform.Mac)
					{
						Args = new List<string>(new string[] { Target.TargetName });
						ExeBaseName = "UE4";
					}

					WriteSchemeFile(XcodeProjectPath, Target, ".app", ExeBaseName, Args);
				}
				else if (Target.TargetName.EndsWith("Editor") && Target.TargetName != "UE4Editor")
				{
					string GameName = Target.TargetName.Replace("Editor", "");
					WriteSchemeFile(XcodeProjectPath, Target, ".app", "UE4Editor", new List<string>(new string[] { GameName }));
				}
				else if (TargetsThatNeedApp.Contains(Target.TargetName) || (Target.TargetPlatform == UnrealTargetPlatform.IOS))
				{
					WriteSchemeFile(XcodeProjectPath, Target, ".app");
				}
				else
				{
					WriteSchemeFile(XcodeProjectPath, Target);
				}
			}
		}

		private void WriteProject(StringBuilder XcodeProjectFileContent, List<XcodeProjectTarget> ProjectTargets)
		{
			// @todo: Windows and Mac should store the project in the same folder
			string PathBranch = (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac) ? (!string.IsNullOrEmpty(UnrealBuildTool.GetUProjectPath()) ? "/Intermediate/IOS" : "/Engine/Intermediate/IOS") : "";
			var XcodeProjectPath = MasterProjectRelativePath + PathBranch + "/" + MasterProjectName + ".xcodeproj";
			if (bGeneratingGameProjectFiles && GameProjectName == "UE4Game")
			{
				XcodeProjectPath = UnrealBuildTool.GetUProjectPath() + "/" + MasterProjectName + ".xcodeproj";
			}
			if (!Directory.Exists(XcodeProjectPath))
			{
				Directory.CreateDirectory(XcodeProjectPath);
			}

			// load the existing project
			string InnerProjectFileName = XcodeProjectPath + "/project.pbxproj";
			string ExistingProjectContents = "";
			if (File.Exists(InnerProjectFileName))
			{
				ExistingProjectContents = File.ReadAllText(InnerProjectFileName);
			}

			// compare it to the new project
			string NewProjectContents = XcodeProjectFileContent.ToString();
			if (ExistingProjectContents != NewProjectContents)
			{
				Log.TraceInformation("Saving updated project file...");
				File.WriteAllText(InnerProjectFileName, NewProjectContents);
			}
			else
			{
				Log.TraceInformation("Skipping project file write, as it didn't change...");
			}

			WriteSchemes(XcodeProjectPath, ProjectTargets);
		}

		private void WriteWorkspace()
		{
			string PathBranch = (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac) ? (!string.IsNullOrEmpty(UnrealBuildTool.GetUProjectPath()) ? "/Intermediate/IOS" : "/Engine/Intermediate/IOS") : "";
			var XcodeProjectPath = MasterProjectRelativePath + PathBranch + "/" + MasterProjectName + ".xcodeproj";
			if (bGeneratingGameProjectFiles && GameProjectName == "UE4Game")
			{
				XcodeProjectPath = UnrealBuildTool.GetUProjectPath() + "/" + MasterProjectName + ".xcodeproj";
			}
			if (!Directory.Exists(XcodeProjectPath))
			{
				Directory.CreateDirectory(XcodeProjectPath);
			}

			// create the workspace folder
			XcodeProjectPath += "/project.xcworkspace";
			if (!Directory.Exists(XcodeProjectPath))
			{
				Directory.CreateDirectory(XcodeProjectPath);
			}
		}

		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected override bool WriteProjectFiles()
		{
			Log.TraceInformation("Generating project file...");

			// Setup project file content
			var XcodeProjectFileContent = new StringBuilder();

			XcodeProjectFileContent.Append(
				"// !$*UTF8*$!" + ProjectFileGenerator.NewLine +
				"{" + ProjectFileGenerator.NewLine +
				"\tarchiveVersion = 1;" + ProjectFileGenerator.NewLine +
				"\tclasses = {" + ProjectFileGenerator.NewLine +
				"\t};" + ProjectFileGenerator.NewLine +
				"\tobjectVersion = 46;" + ProjectFileGenerator.NewLine +
				"\tobjects = {" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			// attempt to determine targets for the project
			List<XcodeProjectTarget> ProjectTargets = new List<XcodeProjectTarget> ();
			// add mandatory ones
			XcodeProjectTarget UE4ProjectTarget = new XcodeProjectTarget ("UE4", "UE4", XcodeTargetType.Project, "");
			XcodeProjectTarget UE4XcodeHelperTarget = new XcodeProjectTarget ("UE4XcodeHelper", "UE4XcodeHelper", XcodeTargetType.XcodeHelper, "", "libUE4XcodeHelper.a");
			ProjectTargets.AddRange(new XcodeProjectTarget[] { UE4ProjectTarget, UE4XcodeHelperTarget });

			if (ProjectFilePlatform.HasFlag(XcodeProjectFilePlatform.iOS))
			{
				XcodeProjectTarget UE4CmdLineRunTarget = new XcodeProjectTarget("UE4CmdLineRun", "UE4CmdLineRun", XcodeTargetType.XCTest, "", "UE4CmdLineRun.xctest", UnrealTargetPlatform.IOS);
				ProjectTargets.Add(UE4CmdLineRunTarget);
				// This GUID will be referenced by each app's test action.
				UE4CmdLineGuid = UE4CmdLineRunTarget.Guid;
			}

			List<XcodeTargetDependency> TargetDependencies = new List<XcodeTargetDependency>();
			List<XcodeContainerItemProxy> ContainerItemProxies = new List<XcodeContainerItemProxy>();
            List<XcodeFramework> Frameworks = new List<XcodeFramework>();
            Frameworks.Add(new XcodeFramework("OpenGLES.framework", "System/Library/Frameworks/OpenGLES.framework", "SDKROOT"));
			// @todo metal: putting this into the project will make for VERY slow Metal runtime by default...
//			Frameworks.Add(new XcodeFramework("Metal.framework", "System/Library/Frameworks/Metal.framework", "SDKROOT"));

			XcodeFramework XCTestFramework = new XcodeFramework("XCTest.framework", "Library/Frameworks/XCTest.framework", "DEVELOPER_DIR");
            Frameworks.Add(XCTestFramework);

			var AllTargets = DiscoverTargets();

			PopulateTargets(AllTargets, ProjectTargets, ContainerItemProxies, TargetDependencies, UE4ProjectTarget, Frameworks);

			Log.TraceInformation(string.Format ("Found {0} targets!", ProjectTargets.Count));

			string PBXBuildFileSection = "/* Begin PBXBuildFile section */" + ProjectFileGenerator.NewLine;
			string PBXFileReferenceSection = "/* Begin PBXFileReference section */" + ProjectFileGenerator.NewLine;
			string PBXResourcesBuildPhaseSection = "/* Begin PBXResourcesBuildPhase section */" + ProjectFileGenerator.NewLine;
			string PBXSourcesBuildPhaseSection = "/* Begin PBXSourcesBuildPhase section */" + ProjectFileGenerator.NewLine;
			string PBXFrameworksBuildPhaseSection = "/* Begin PBXFrameworksBuildPhase section */" + ProjectFileGenerator.NewLine;
			string PBXShellScriptBuildPhaseSection = "/* Begin PBXShellScriptBuildPhase section */" + ProjectFileGenerator.NewLine;
			Dictionary<string, XcodeFileGroup> Groups = new Dictionary<string, XcodeFileGroup>();
			List<string> IncludeDirectories = new List<string>();
			List<string> SystemIncludeDirectories = new List<string>();
			List<string> PreprocessorDefinitions = new List<string>();

            foreach (XcodeFramework Framework in Frameworks)
            {
                // Add file references.
                PBXFileReferenceSection += "\t\t" + Framework.Guid + " /* " + Framework.Name + " */ = {"
                                         + "isa = PBXFileReference; "
                                         + "lastKnownFileType = wrapper.framework; "
                                         + "name = " + Framework.Name + "; "
                                         + "path = " + Framework.Path + "; "
                                         + "sourceTree = " + Framework.SourceTree + "; "
                                         + "};" + ProjectFileGenerator.NewLine;
            }

			// Set up all the test guids that need to be explicitly referenced later
			string UE4CmdLineRunMFileGuid = MakeXcodeGuid();
			UE4CmdLineRunMFileRefGuid = MakeXcodeGuid();
			string XCTestBuildFileGUID = MakeXcodeGuid();

			string TestFrameworkFiles = "\t\t\t\t" + XCTestBuildFileGUID + " /* XCTest.framework in Frameworks */," + ProjectFileGenerator.NewLine;
			PBXBuildFileSection += "\t\t" + XCTestBuildFileGUID + " /* XCTest.framework in Frameworks */ = {isa = PBXBuildFile; "
				+ "fileRef = " + XCTestFramework.Guid + " /* UE4CmdLineRun.m */; };" + ProjectFileGenerator.NewLine;

			AppendBuildPhases(ProjectTargets, ref PBXBuildFileSection, ref PBXFileReferenceSection, ref PBXSourcesBuildPhaseSection, ref PBXResourcesBuildPhaseSection, ref PBXFrameworksBuildPhaseSection,
				ref PBXShellScriptBuildPhaseSection, TestFrameworkFiles, UE4CmdLineRunMFileGuid);

			AppendSourceFiles(ProjectTargets, ref PBXBuildFileSection, ref PBXFileReferenceSection, ref PBXSourcesBuildPhaseSection, ref Groups, UE4XcodeHelperTarget, UE4CmdLineRunMFileGuid,
				ref IncludeDirectories, ref SystemIncludeDirectories, ref PreprocessorDefinitions);

			foreach (XcodeProjectTarget Target in ProjectTargets)
			{
				if ((Target.Type == XcodeTargetType.Native || Target.Type == XcodeTargetType.XCTest))
				{
					string FileType = Target.Type == XcodeTargetType.Native ? "wrapper.application" : "wrapper.cfbundle";
					string PayloadDir = "Engine";
					if (Target.Type == XcodeTargetType.Native && Target.TargetName != "UE4Game")
					{
						PayloadDir = Target.TargetName;
					}

					PBXFileReferenceSection += "\t\t" + Target.ProductGuid + " /* " + Target.ProductName + " */ = {isa = PBXFileReference; explicitFileType = " + FileType + "; includeInIndex = 0; path = " + PayloadDir + "/Binaries/IOS/Payload/" + Target.ProductName + "; sourceTree = \"<group>\"; };" + ProjectFileGenerator.NewLine;
					if (!string.IsNullOrEmpty(Target.PlistGuid))
					{
						PBXFileReferenceSection += "\t\t" + Target.PlistGuid + " /* " + Target.TargetName + "-Info.plist */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = text.plist.xml; name = \"" + Target.TargetName + "-Info.plist\"; path = " + Target.TargetName + "/Build/IOS/" + Target.TargetName + "-Info.plist; sourceTree = \"<group>\"; };" + ProjectFileGenerator.NewLine;
					}
				}
			}

			if (bGeneratingRunIOSProject)
			{
				AppendReferenceGroups(ref PBXFileReferenceSection, ref Groups);
			}

			PBXBuildFileSection += "/* End PBXBuildFile section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;
			PBXFileReferenceSection += "/* End PBXFileReference section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;

			PBXResourcesBuildPhaseSection += "/* End PBXResourcesBuildPhase section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;
			PBXSourcesBuildPhaseSection += "/* End PBXSourcesBuildPhase section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;
			PBXFrameworksBuildPhaseSection += "/* End PBXFrameworksBuildPhase section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;
			PBXShellScriptBuildPhaseSection += "/* End PBXShellScriptBuildPhase section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;

			string PBXTargetDependencySection = "/* Begin PBXTargetDependency section */" + ProjectFileGenerator.NewLine;
			CreateTargetDependencySection(ref PBXTargetDependencySection, TargetDependencies);
			PBXTargetDependencySection += "/* End PBXTargetDependency section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;

			string PBXContainerItemProxySection = "/* Begin PBXContainerItemProxy section */" + ProjectFileGenerator.NewLine;
			CreateContainerItemProxySection(ref PBXContainerItemProxySection, ContainerItemProxies);
			PBXContainerItemProxySection += "/* End PBXContainerItemProxy section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine;

			XcodeProjectFileContent.Append(PBXBuildFileSection);
			XcodeProjectFileContent.Append(PBXFileReferenceSection);
			XcodeProjectFileContent.Append(PBXContainerItemProxySection);
			XcodeProjectFileContent.Append(PBXTargetDependencySection);

			AppendGroups(ref XcodeProjectFileContent, ref Groups, ProjectTargets , Frameworks);

			XcodeProjectFileContent.Append(PBXShellScriptBuildPhaseSection);
			XcodeProjectFileContent.Append(PBXSourcesBuildPhaseSection);
			XcodeProjectFileContent.Append(PBXFrameworksBuildPhaseSection);

			XcodeProjectFileContent.Append("/* Begin PBXLegacyTarget section */" + ProjectFileGenerator.NewLine);
			foreach (var Target in ProjectTargets)
			{
				if (Target.Type == XcodeTargetType.Legacy)
				{
					AppendTarget (ref XcodeProjectFileContent, Target);
				}
			}
			XcodeProjectFileContent.Append("/* End PBXLegacyTarget section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			XcodeProjectFileContent.Append("/* Begin PBXNativeTarget section */" + ProjectFileGenerator.NewLine);
			AppendTarget(ref XcodeProjectFileContent, UE4XcodeHelperTarget);

			foreach (XcodeProjectTarget Target in ProjectTargets)
			{
				if (Target.Type == XcodeTargetType.Native || Target.Type == XcodeTargetType.XCTest)
				{
					AppendTarget(ref XcodeProjectFileContent, Target);
				}
			}
			XcodeProjectFileContent.Append("/* End PBXNativeTarget section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			XcodeProjectFileContent.Append(
				"/* Begin PBXProject section */" + ProjectFileGenerator.NewLine +
				"\t\t" + UE4ProjectTarget.Guid + " /* Project object */ = {" + ProjectFileGenerator.NewLine +
				"\t\t\tisa = PBXProject;" + ProjectFileGenerator.NewLine +
				"\t\t\tattributes = {" + ProjectFileGenerator.NewLine +
				"\t\t\t\tLastUpgradeCheck = 0510;" + ProjectFileGenerator.NewLine +
				"\t\t\t\tORGANIZATIONNAME = EpicGames;" + ProjectFileGenerator.NewLine +
				"\t\t\t};" + ProjectFileGenerator.NewLine +
				"\t\t\tbuildConfigurationList = " + UE4ProjectTarget.BuildConfigGuild + " /* Build configuration list for PBXProject \"" + UE4ProjectTarget.DisplayName + "\" */;" + ProjectFileGenerator.NewLine +
				"\t\t\tcompatibilityVersion = \"Xcode 3.2\";" + ProjectFileGenerator.NewLine +
				"\t\t\tdevelopmentRegion = English;" + ProjectFileGenerator.NewLine +
				"\t\t\thasScannedForEncodings = 0;" + ProjectFileGenerator.NewLine +
				"\t\t\tknownRegions = (" + ProjectFileGenerator.NewLine +
				"\t\t\t\ten," + ProjectFileGenerator.NewLine +
				"\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t\tmainGroup = " + MainGroupGuid + ";" + ProjectFileGenerator.NewLine +
				"\t\t\tproductRefGroup = " + ProductRefGroupGuid + " /* Products */;" + ProjectFileGenerator.NewLine +
				"\t\t\tprojectDirPath = \"\";" + ProjectFileGenerator.NewLine +
				"\t\t\tprojectRoot = \"\";" + ProjectFileGenerator.NewLine +
				"\t\t\ttargets = (" + ProjectFileGenerator.NewLine
			);

			foreach (var Target in ProjectTargets)
			{
				if (Target != UE4ProjectTarget)
				{
					XcodeProjectFileContent.AppendLine(string.Format ("\t\t\t\t{0} /* {1} */,", Target.Guid, Target.DisplayName));
				}
			}

			XcodeProjectFileContent.Append(
				"\t\t\t);" + ProjectFileGenerator.NewLine +
				"\t\t};" + ProjectFileGenerator.NewLine +
				"/* End PBXProject section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine
			);

			string PreprocessorDefinitionsString = "\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (" + ProjectFileGenerator.NewLine;
			foreach (string Definition in PreprocessorDefinitions)
			{
				PreprocessorDefinitionsString += "\t\t\t\t\t\"" + Definition + "\"," + ProjectFileGenerator.NewLine;
			}
			PreprocessorDefinitionsString += "\t\t\t\t\t\"MONOLITHIC_BUILD=1\"," + ProjectFileGenerator.NewLine;
			PreprocessorDefinitionsString += "\t\t\t\t);" + ProjectFileGenerator.NewLine;

			string HeaderSearchPaths = "\t\t\t\tHEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine;
			foreach (string Path in SystemIncludeDirectories)
			{
				HeaderSearchPaths += "\t\t\t\t\t\"" + Path + "\"," + ProjectFileGenerator.NewLine;
			}
			HeaderSearchPaths += "\t\t\t\t);" + ProjectFileGenerator.NewLine;
			HeaderSearchPaths += "\t\t\t\tUSER_HEADER_SEARCH_PATHS = (" + ProjectFileGenerator.NewLine;
			foreach (string Path in IncludeDirectories)
			{
				HeaderSearchPaths += "\t\t\t\t\t\"" + Path + "\"," + ProjectFileGenerator.NewLine;
			}
			HeaderSearchPaths += "\t\t\t\t);" + ProjectFileGenerator.NewLine;

			XcodeProjectFileContent.Append("/* Begin XCBuildConfiguration section */" + ProjectFileGenerator.NewLine);
			AppendBuildConfigs(ref XcodeProjectFileContent, UE4ProjectTarget, HeaderSearchPaths, PreprocessorDefinitionsString);
			foreach (var Target in ProjectTargets)
			{
				if (Target.Type != XcodeTargetType.Project && Target.Type != XcodeTargetType.XcodeHelper)
				{
					AppendBuildConfigs(ref XcodeProjectFileContent, Target);
				}
			}

			AppendBuildConfigs(ref XcodeProjectFileContent, UE4XcodeHelperTarget);

			XcodeProjectFileContent.Append("/* End XCBuildConfiguration section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			XcodeProjectFileContent.Append("/* Begin XCConfigurationList section */" + ProjectFileGenerator.NewLine);

			foreach (var Target in ProjectTargets)
			{
				if (Target.Type != XcodeTargetType.XcodeHelper)
				{
					AppendConfigList(ref XcodeProjectFileContent, Target);
				}
			}

			AppendConfigList(ref XcodeProjectFileContent, UE4XcodeHelperTarget);

			XcodeProjectFileContent.Append("/* End XCConfigurationList section */" + ProjectFileGenerator.NewLine + ProjectFileGenerator.NewLine);

			XcodeProjectFileContent.Append(
				"\t};" + ProjectFileGenerator.NewLine +
				"\trootObject = " + UE4ProjectTarget.Guid + " /* Project object */;" + ProjectFileGenerator.NewLine +
				"}" + ProjectFileGenerator.NewLine);

			WriteProject(XcodeProjectFileContent, ProjectTargets);

			// TODO: add any content for any files in the workspace here
			WriteWorkspace();

			return true;
		}

		[Flags]
		public enum XcodeProjectFilePlatform
		{
			Mac = 1 << 0,
			iOS = 1 << 1,
			All = Mac | iOS
		}

		/// Which platforms we should generate targets for
		static public XcodeProjectFilePlatform ProjectFilePlatform = XcodeProjectFilePlatform.All;

		/// Should we generate a special project to use for iOS signing instead of a normal one
		static public bool bGeneratingRunIOSProject = false;

		/// <summary>
		/// Configures project generator based on command-line options
		/// </summary>
		/// <param name="Arguments">Arguments passed into the program</param>
		/// <param name="IncludeAllPlatforms">True if all platforms should be included</param>
		protected override void ConfigureProjectFileGeneration(string[] Arguments, ref bool IncludeAllPlatforms)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms);
			ProjectFilePlatform = IncludeAllPlatforms ? XcodeProjectFilePlatform.All : XcodeProjectFilePlatform.Mac;

			foreach (var CurArgument in Arguments)
			{
				if (CurArgument.ToLowerInvariant() == "-iosdeployonly")
				{
					bGeneratingRunIOSProject = true;
					ProjectFilePlatform = XcodeProjectFilePlatform.iOS;
					break;
				}
			}
		}

		private string UE4CmdLineGuid;
		private string MainGroupGuid;
		private string ProductRefGroupGuid;
		private string FrameworkGroupGuid;
		private string UE4CmdLineRunMFileRefGuid;

		// List of targets that have a user interface.
		static List<string> TargetsThatNeedApp = new List<string>();
	}
}
