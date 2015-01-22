// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using UnrealBuildTool;

namespace AutomationTool
{
    public partial class CommandUtils
    {
        /// <summary>
        /// Given a path to a file, strips off the base directory part of the path
        /// </summary>
        /// <param name="FilePath">The full path</param>
        /// <param name="BaseDirectory">The base directory, which must be the first part of the path</param>
        /// <returns>The part of the path after the base directory</returns>
        public static string StripBaseDirectory(string InFilePath, string InBaseDirectory)
        {
            var FilePath = CombinePaths(InFilePath);
            var BaseDirectory = CombinePaths(InBaseDirectory);
            if (!FilePath.StartsWith(BaseDirectory, StringComparison.InvariantCultureIgnoreCase))
            {
                throw new AutomationException("Cannot strip the base directory {0} from {1} because it doesn't start with the base directory.", BaseDirectory, FilePath);
            }
            if (BaseDirectory.EndsWith("/") || BaseDirectory.EndsWith("\\"))
            {
                return FilePath.Substring(BaseDirectory.Length);
            }
            return FilePath.Substring(BaseDirectory.Length + 1);
        }


        /// <summary>
        /// Given a path to a "source" file, re-roots the file path to be located under the "destination" folder.  The part of the source file's path after the root folder is unchanged.
        /// </summary>
        /// <param name="FilePath"></param>
        /// <param name="BaseDirectory"></param>
        /// <param name="NewBaseDirectory"></param>
        /// <returns></returns>
        public static string MakeRerootedFilePath(string FilePath, string BaseDirectory, string NewBaseDirectory)
        {
            var RelativeFile = StripBaseDirectory(FilePath, BaseDirectory);
            var DestFile = CombinePaths(NewBaseDirectory, RelativeFile);
            return DestFile;
        }
    }
    public class Distiller
    {
        public string SourceBaseDir;
        public string DestBaseDir;
        public string DestBaseDirSymbols;
        public DateTime DestinationTime;
        public List<UnrealTargetPlatform> AllLegalPlatforms;
        public List<string> RejectStrings;

        public Distiller(string InDestBaseDir, string InDestBaseDirSymbols = null, string InSourceBaseDir = null, DateTime? InDestinationTime = null, List<UnrealTargetPlatform> InAllLegalPlatforms = null, bool AllowNoRedist = false)
        {
            DestBaseDir = CommandUtils.CombinePaths(InDestBaseDir);
            SourceBaseDir = InSourceBaseDir;
            if (SourceBaseDir == null)
            {
                SourceBaseDir = CommandUtils.CmdEnv.LocalRoot;
            }
            SourceBaseDir = CommandUtils.CombinePaths(SourceBaseDir);
            if (InDestinationTime == null)
            {
                DestinationTime = DateTime.Now;
            }
            else
            {
                DestinationTime = InDestinationTime.Value;
            }
            DestBaseDirSymbols = InDestBaseDirSymbols;
            if (DestBaseDirSymbols != null)
            {
                DestBaseDirSymbols = CommandUtils.CombinePaths(DestBaseDirSymbols);
            }
            AllLegalPlatforms = InAllLegalPlatforms;
            if (AllLegalPlatforms == null)
            {
                AllLegalPlatforms = new List<UnrealTargetPlatform>();
                foreach (UnrealTargetPlatform Plat in Enum.GetValues(typeof(UnrealTargetPlatform)))
                {
                    AllLegalPlatforms.Add(Plat);
                }
            }
            RejectStrings = new List<string>
            {
                CommandUtils.CombinePaths("/CarefullyRedist/"),
                CommandUtils.CombinePaths("/NotForLicensees/"),
            };
            if (!AllowNoRedist)
            {
                RejectStrings.Add(CommandUtils.CombinePaths("/NoRedist/"));
            }
            foreach (UnrealTargetPlatform Plat in Enum.GetValues(typeof(UnrealTargetPlatform)))
            {
                if (!AllLegalPlatforms.Contains(Plat))
                {
                    RejectStrings.Add(CommandUtils.CombinePaths("/" + Plat.ToString() + "/"));
                }
            }
            if (!AllLegalPlatforms.Contains(UnrealTargetPlatform.Win64) && !AllLegalPlatforms.Contains(UnrealTargetPlatform.Win32))
            {
                RejectStrings.Add(CommandUtils.CombinePaths("/Windows/"));
            }
        }

        public static bool RejectFileOnSubstring(string FileToCopy, string RejectString, string Root = null)
        {
            var Search = CommandUtils.CombinePaths(FileToCopy);
            if (Root == null)
            {
                Root = CommandUtils.CmdEnv.LocalRoot;
            }
            if (Search.StartsWith(CommandUtils.CombinePaths(Root), StringComparison.InvariantCultureIgnoreCase))
            {
                Search = CommandUtils.CombinePaths("/", CommandUtils.StripBaseDirectory(FileToCopy, Root)); // lots of rejects are like "/path/" so we need to be sure we start with a slash
            }
            if (Search.IndexOf(CommandUtils.CombinePaths(RejectString), 0, StringComparison.InvariantCultureIgnoreCase) >= 0)
            {
                return true;
            }
            return false;
        }

        public bool RejectFile(string FileToCopy, string Root = null)
        {
            bool Result = false;
            foreach (var RejectOn in RejectStrings)
            {
                if (RejectFileOnSubstring(FileToCopy, RejectOn, Root == null ? SourceBaseDir : Root))
                {
                    Result = true;
                    break;
                }
            }
            if (Result)
            {
                CommandUtils.Log("Rejecting file {0}", FileToCopy);
            }
            return Result;
        }

        public static bool IsSymbolFile(UnrealTargetPlatform ForPlatform, string FileToCopy)
        {
            var DebugExts = Platform.Platforms[ForPlatform].GetDebugFileExtentions();
            foreach (var DebugExt in DebugExts)
            {
                if (Path.GetExtension(FileToCopy).Equals(DebugExt, StringComparison.InvariantCultureIgnoreCase))
                {
                    return true;
                }
            }
            return false;
        }

        public bool IsSymbolFile(string FileToCopy)
        {
            foreach (var ForPlatform in AllLegalPlatforms)
            {
                if (IsSymbolFile(ForPlatform, FileToCopy))
                {
                    return true;
                }
            }
            return false;
        }

        public string CopyFileToDest(string FileToCopy, bool MoveSymbols = true)
        {
            if (!CommandUtils.FileExists_NoExceptions(FileToCopy))
            {
                throw new AutomationException("Can't distill {0}; it doesn't exist", FileToCopy);
            }
            {
                FileInfo FileInfo = new FileInfo(FileToCopy);
                FileToCopy = CommandUtils.CombinePaths(FileInfo.FullName);
            }

            string Dest;
            if (MoveSymbols && !String.IsNullOrEmpty(DestBaseDirSymbols) && IsSymbolFile(FileToCopy))
            {
                Dest = CommandUtils.MakeRerootedFilePath(FileToCopy, SourceBaseDir, DestBaseDirSymbols);
            }
            else
            {
                Dest = CommandUtils.MakeRerootedFilePath(FileToCopy, SourceBaseDir, DestBaseDir);
            }
            if (CommandUtils.FileExists_NoExceptions(Dest))
            {
                throw new AutomationException("Can't distill to {0}; it already exists", Dest);
            }
            CommandUtils.CopyFile(FileToCopy, Dest);

            {
                FileInfo FileInfo = new FileInfo(Dest);
                FileInfo.IsReadOnly = false;
                FileInfo.CreationTime = DestinationTime;
                FileInfo.LastWriteTime = DestinationTime;
                Dest = CommandUtils.CombinePaths(FileInfo.FullName);
            }
            if (!CommandUtils.FileExists_NoExceptions(Dest))
            {
                throw new AutomationException("Couldn't distill {0} to {1}", FileToCopy, Dest);
            }
            return Dest;
        }

        public List<string> Distill(string FullPathAndWildcard, bool bRecursive = false, bool bAllowMissing = false, bool MoveSymbols = true, params string[] Exclusions)
        {
            var FilesToCopy = new List<string>();

            string AbsFile = CommandUtils.CombinePaths(FullPathAndWildcard);
            CommandUtils.Log("Distilling {0}", AbsFile);
            string PathOnly = Path.GetDirectoryName(AbsFile);
            if (!CommandUtils.DirectoryExists_NoExceptions(PathOnly))
            {
                if (bAllowMissing)
                {
                    return FilesToCopy;
                }
                throw new AutomationException("Path {0} was used to distill {1} but it doesn't exist.", PathOnly, AbsFile);
            }
            var Files = CommandUtils.FindFiles(Path.GetFileName(AbsFile), bRecursive, PathOnly);
            var Exclude = new List<string>();
            foreach (var Excl in Exclusions)
            {
                if (!Excl.Contains("/") && !Excl.Contains("\\"))
                {
                    var ExFiles = CommandUtils.FindFiles(Excl, bRecursive, PathOnly);
                    Exclude.AddRange(ExFiles);
                }
            }

            foreach (var FileToCopy in Files)
            {
                bool bOk = !Exclude.Contains(FileToCopy) && !RejectFile(FileToCopy, PathOnly);
                foreach (var Excl in Exclusions)
                {
                    if (Excl.Contains("/") || Excl.Contains("\\"))
                    {
                        if (Excl.Contains("*") || Excl.Contains("?"))
                        {
                            throw new AutomationException("Exclusion {0} is illegal, must either be a substring or a wildcard without a path", Excl);
                        }
                        bOk = bOk && !RejectFileOnSubstring(FileToCopy, Excl, PathOnly);
                    }
                }
                if (bOk)
                {
                    FilesToCopy.Add(CopyFileToDest(FileToCopy, MoveSymbols));
                }
            }
            if (FilesToCopy.Count < 1 && !bAllowMissing)
            {
                throw new AutomationException("Distill {0} did not produce any files. Recursive option was {1}.", AbsFile, bRecursive.ToString());
            }
            return FilesToCopy;
        }
    }
}
