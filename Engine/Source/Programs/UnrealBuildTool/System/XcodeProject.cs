// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Info needed to make a file a member of specific group
	/// </summary>
	class XcodeSourceFile : ProjectFile.SourceFile
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public XcodeSourceFile(string InitFilePath, string InitRelativeBaseFolder)
			: base(InitFilePath, InitRelativeBaseFolder)
		{
			FileGUID = XcodeProjectFileGenerator.MakeXcodeGuid();
			FileRefGUID = XcodeProjectFileGenerator.MakeXcodeGuid();
		}

		/// <summary>
		/// File GUID for use in Xcode project
		/// </summary>
		public string FileGUID
		{
			get;
			private set;
		}

		public void ReplaceGUIDS(string NewFileGUID, string NewFileRefGUID)
		{
			FileGUID = NewFileGUID;
			FileRefGUID = NewFileRefGUID;
		}

		/// <summary>
		/// File reference GUID for use in Xcode project
		/// </summary>
		public string FileRefGUID
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// Represents a group of files shown in Xcode's project navigator as a folder
	/// </summary>
	class XcodeFileGroup
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public XcodeFileGroup(string InName, string InPath, bool InReference = false)
		{
			GroupName = InName;
			GroupPath = InPath;
			GroupGuid = XcodeProjectFileGenerator.MakeXcodeGuid();
			bReference = InReference;
		}

		/// <summary>
		/// Appends a group entry (and, recusrively, it's children) to Contents. Should be called while generating PBXGroup section.
		/// </summary>
		public void Append(ref StringBuilder Contents, bool bFilesOnly = false)
		{
			if (bReference)
			{
				return;
			}

			if (!bFilesOnly)
			{
				Contents.Append(string.Format("\t\t{0} = {{{1}", GroupGuid, ProjectFileGenerator.NewLine));
				Contents.Append("\t\t\tisa = PBXGroup;" + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\tchildren = (" + ProjectFileGenerator.NewLine);

				foreach (XcodeFileGroup Group in Children.Values)
				{
					Contents.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", Group.GroupGuid, Group.GroupName, ProjectFileGenerator.NewLine));
				}
			}

			foreach (XcodeSourceFile File in Files)
			{
				Contents.Append(string.Format("\t\t\t\t{0} /* {1} */,{2}", File.FileRefGUID, Path.GetFileName(File.FilePath), ProjectFileGenerator.NewLine));
			}

			if (!bFilesOnly)
			{
				Contents.Append("\t\t\t);" + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\tname = \"" + GroupName + "\";" + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\tpath = \"" + GroupPath + "\";" + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t\tsourceTree = \"<group>\";" + ProjectFileGenerator.NewLine);
				Contents.Append("\t\t};" + ProjectFileGenerator.NewLine);

				foreach (XcodeFileGroup Group in Children.Values)
				{
					Group.Append(ref Contents);
				}
			}
		}

		public string GroupGuid;
		public string GroupName;
		public string GroupPath;
		public Dictionary<string, XcodeFileGroup> Children = new Dictionary<string, XcodeFileGroup>();
		public List<XcodeSourceFile> Files = new List<XcodeSourceFile>();
		public bool bReference;
	}

	class XcodeProjectFile : ProjectFile
	{
		public string UE4CmdLineRunFileGuid = null;
		public string UE4CmdLineRunFileRefGuid = null;

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public XcodeProjectFile(string InitFilePath)
			: base( InitFilePath )
		{
		}

		/** Gets Xcode file category based on its extension */
		private string GetFileCategory(string Extension)
		{
			// @todo Mac: Handle more categories
			switch (Extension)
			{
			case ".framework":
				return "Frameworks";
			default:
				return "Sources";
			}
		}

		/** Gets Xcode file type based on its extension */
		private string GetFileType(string Extension)
		{
			// @todo Mac: Handle more file types
			switch (Extension)
			{
			case ".c":
			case ".m":
				return "sourcecode.c.objc";
			case ".cc":
			case ".cpp":
			case ".mm":
				return "sourcecode.cpp.objcpp";
			case ".h":
			case ".inl":
			case ".pch":
				return "sourcecode.c.h";
			case ".framework":
				return "wrapper.framework";
			default:
				return "file.text";
			}
		}

		/** Returns true if Extension is a known extension for files containing source code */
		private bool IsSourceCode(string Extension)
		{
			return Extension == ".c" || Extension == ".cc" || Extension == ".cpp" || Extension == ".m" || Extension == ".mm";
		}

		private bool IsPartOfUE4XcodeHelperTarget(XcodeSourceFile SourceFile)
		{
			string FileExtension = Path.GetExtension(SourceFile.FilePath);

			if (IsSourceCode(FileExtension))// || GetFileType(FileExtension) == "sourcecode.c.h") @todo: It seemed that headers need to be added to project for live issues detection to work in them
			{
				foreach (string PlatformName in Enum.GetNames(typeof(UnrealTargetPlatform)))
				{
					string AltName = PlatformName == "Win32" || PlatformName == "Win64" ? "windows" : PlatformName.ToLower();
					if ((SourceFile.FilePath.ToLower().Contains("/" + PlatformName.ToLower() + "/") || SourceFile.FilePath.ToLower().Contains("/" + AltName + "/")) && PlatformName != "Mac")
					{
						// UE4XcodeHelper is Mac only target, so skip other platforms files
						return false;
					}
					else if (SourceFile.FilePath.EndsWith("SimplygonMeshReduction.cpp") || SourceFile.FilePath.EndsWith("MeshBoneReduction.cpp") || SourceFile.FilePath.EndsWith("Android.cpp")
						|| SourceFile.FilePath.EndsWith("Amazon.cpp") || SourceFile.FilePath.EndsWith("FacebookModule.cpp") || SourceFile.FilePath.EndsWith("SDL_angle.c")
						|| SourceFile.FilePath.Contains("VisualStudioSourceCodeAccess") || SourceFile.FilePath.Contains("AndroidDevice") || SourceFile.FilePath.Contains("IOSDevice")
						|| SourceFile.FilePath.Contains("WindowsDevice") || SourceFile.FilePath.Contains("WindowsMoviePlayer") || SourceFile.FilePath.EndsWith("IOSTapJoy.cpp"))
					{
						// @todo: We need a way to filter out files that use SDKs we don't have
						return false;
					}
				}

				return true;
			}

			return false;
		}

		/**
		 * Returns a project navigator group to which the file should belong based on its path.
		 * Creates a group tree if it doesn't exist yet.
		 */
		public XcodeFileGroup FindGroupByFullPath(ref Dictionary<string, XcodeFileGroup> Groups, string FullPath)
		{
			string RelativePath = (FullPath == XcodeProjectFileGenerator.MasterProjectRelativePath) ? "" : Utils.MakePathRelativeTo(FullPath, XcodeProjectFileGenerator.MasterProjectRelativePath);
			if (RelativePath.StartsWith(".."))
			{
				string UE4Dir = Path.GetFullPath(Directory.GetCurrentDirectory() + "../../..");
				RelativePath = Utils.MakePathRelativeTo(FullPath, UE4Dir);
			}

			string[] Parts = RelativePath.Split(Path.DirectorySeparatorChar);
			string CurrentPath = "";
			Dictionary<string, XcodeFileGroup> CurrentParent = Groups;

			foreach (string Part in Parts)
			{
				XcodeFileGroup CurrentGroup;

				if (CurrentPath != "")
				{
					CurrentPath += Path.DirectorySeparatorChar;
				}

				CurrentPath += Part;

				if (!CurrentParent.ContainsKey(CurrentPath))
				{
					CurrentGroup = new XcodeFileGroup(Path.GetFileName(CurrentPath), CurrentPath);
					CurrentParent.Add(CurrentPath, CurrentGroup);
				}
				else
				{
					CurrentGroup = CurrentParent[CurrentPath];
				}

				if (CurrentPath == RelativePath)
				{
					return CurrentGroup;
				}

				CurrentParent = CurrentGroup.Children;
			}

			return null;
		}

		/// <summary>
		/// Allocates a generator-specific source file object
		/// </summary>
		/// <param name="InitFilePath">Path to the source file on disk</param>
		/// <param name="InitProjectSubFolder">Optional sub-folder to put the file in.  If empty, this will be determined automatically from the file's path relative to the project file</param>
		/// <returns>The newly allocated source file object</returns>
		public override SourceFile AllocSourceFile(string InitFilePath, string InitProjectSubFolder)
		{
			if (Path.GetFileName(InitFilePath) == ".DS_Store")
			{
				return null;
			}
			return new XcodeSourceFile(InitFilePath, InitProjectSubFolder);
		}

		/**
		 * Generates bodies of all sections that contain a list of source files plus a dictionary of project navigator groups.
		 */
		public void GenerateSectionsContents(ref string PBXBuildFileSection, ref string PBXFileReferenceSection,
											 ref string PBXSourcesBuildPhaseSection, ref Dictionary<string, XcodeFileGroup> Groups)
		{
			StringBuilder FileSection = new StringBuilder();
			StringBuilder ReferenceSection = new StringBuilder();
			StringBuilder BuildPhaseSection = new StringBuilder();

			foreach (var CurSourceFile in SourceFiles)
			{
				XcodeSourceFile SourceFile = CurSourceFile as XcodeSourceFile;
				string FileName = Path.GetFileName(SourceFile.FilePath);
				string FileExtension = Path.GetExtension(FileName);
				string FilePath = Utils.MakePathRelativeTo(SourceFile.FilePath, XcodeProjectFileGenerator.MasterProjectRelativePath);
				string FilePathMac = Utils.CleanDirectorySeparators(FilePath, '/');

				// Xcode doesn't parse the project file correctly if the build files don't use the same file reference ID,
				// so you can't just manually add an entry for UE4CmdLineRun.m, because the ID will conflict with the
				// ID that gets generated above implicitly by "XcodeSourceFile SourceFile = CurSourceFile as XcodeSourceFile;"
				// At the same time, it is necessary to manually add an entry for it, because we're compiling the unit test with
				// exactly one source file.
				// This is ugly, but the project file generator wasn't designed to handle special cases like this,
				// so we're left with little choice.
				if (FileName == "UE4CmdLineRun.m")
				{
					SourceFile.ReplaceGUIDS(UE4CmdLineRunFileGuid, UE4CmdLineRunFileRefGuid);
				}

				if (IsGeneratedProject)
				{
					FileSection.Append(string.Format("\t\t{0} /* {1} in {2} */ = {{isa = PBXBuildFile; fileRef = {3} /* {1} */; }};" + ProjectFileGenerator.NewLine,
													 SourceFile.FileGUID,
													 FileName,
													 GetFileCategory(FileExtension),
													 SourceFile.FileRefGUID));
				}

				ReferenceSection.Append(string.Format("\t\t{0} /* {1} */ = {{isa = PBXFileReference; lastKnownFileType = {2}; name = \"{1}\"; path = \"{3}\"; sourceTree = SOURCE_ROOT; }};" + ProjectFileGenerator.NewLine,
													  SourceFile.FileRefGUID,
													  FileName,
													  GetFileType(FileExtension),
													  FilePathMac));

				if (IsPartOfUE4XcodeHelperTarget(SourceFile))
				{
					BuildPhaseSection.Append("\t\t\t\t" + SourceFile.FileGUID + " /* " + FileName + " in Sources */," + ProjectFileGenerator.NewLine);
				}

				string GroupPath = Path.GetFullPath(Path.GetDirectoryName(SourceFile.FilePath));
				XcodeFileGroup Group = FindGroupByFullPath(ref Groups, GroupPath);
				if (Group != null)
				{
					Group.Files.Add(SourceFile);
				}
			}

			PBXBuildFileSection += FileSection.ToString();
			PBXFileReferenceSection += ReferenceSection.ToString();
			PBXSourcesBuildPhaseSection += BuildPhaseSection.ToString();
		}
	}
}
