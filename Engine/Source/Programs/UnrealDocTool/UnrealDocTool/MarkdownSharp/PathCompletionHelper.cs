// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;

namespace MarkdownSharp
{
    using System.IO;
    using System.Security;
    using System.Text.RegularExpressions;
    using MarkdownSharp.EpicMarkdown;

    [Flags]
    public enum PathElementType
    {
        Image       = 0x01,
        Bookmark    = 0x02,
        Attachment  = 0x04,
        Folder      = 0x08,
        Excerpt     = 0x10,
        Variable    = 0x20
    }

    public class PathElement
    {
        public string Text { get; private set; }
        public PathElementType Type { get; private set; }

        public PathElement(string text, PathElementType type)
        {
            Text = text;
            Type = type;
        }

        public override string ToString()
        {
            return string.Format("{0} {1}", Type.ToString(), Text);
        }
    }

    public static class PathCompletionHelper
    {
        public static List<PathElement> GetSuggestionList(EMDocument doc, string userPath, PathElementType options)
        {
            var list = new List<PathElement>();
            var emptyInputPath = false;

            if (string.IsNullOrWhiteSpace(userPath))
            {
                emptyInputPath = true;
                userPath = "";
            }

            var langId = LangIdPattern.Match(Path.GetFileName(doc.LocalPath)).Groups["langId"].Value;

            var m = RootKeywordPattern.Match(userPath);

            userPath = m.Success
                           ? userPath.Replace(m.Value, GetSourceDir(doc.LocalPath))
                           : Path.Combine(Path.GetDirectoryName(doc.LocalPath), userPath);

            DirectoryInfo userDir;

            try
            {
                userDir = new DirectoryInfo(userPath);
            }
            catch (Exception e)
            {
                if (e is ArgumentNullException || e is SecurityException || e is ArgumentException
                    || e is PathTooLongException)
                {
                    // ignore and output empty list
                    return list;
                }

                throw;
            }

            if (userDir.Exists)
            {
                if ((options & (PathElementType.Image | PathElementType.Attachment | PathElementType.Folder)) > 0)
                {
                    // File system infos.

                    var dirs = userDir.GetDirectories();

                    if (options.HasFlag(PathElementType.Image))
                    {
                        var imagesDirs = dirs.Where(d => d.Name.ToLower() == "images").ToArray();

                        if (imagesDirs.Length > 0)
                        {
                            list.AddRange(
                                imagesDirs[0].GetFiles().Select(f => new PathElement(f.Name, PathElementType.Image)));
                        }
                    }

                    if (options.HasFlag(PathElementType.Attachment))
                    {
                        var attachmentsDirs = dirs.Where(d => d.Name.ToLower() == "attachments").ToArray();

                        if (attachmentsDirs.Length > 0)
                        {
                            list.AddRange(
                                attachmentsDirs[0].GetFiles()
                                                  .Select(f => new PathElement(f.Name, PathElementType.Attachment)));
                        }
                    }

                    if (options.HasFlag(PathElementType.Folder))
                    {
                        if (emptyInputPath)
                        {
                            list.Add(new PathElement("%ROOT%", PathElementType.Folder));
                        }

                        list.AddRange(
                            dirs.Where(d => d.Name.ToLower() != "attachments" && d.Name.ToLower() != "images")
                                .Select(d => new PathElement(d.Name, PathElementType.Folder)));
                    }
                }

                if ((options & (PathElementType.Bookmark | PathElementType.Excerpt | PathElementType.Variable)) > 0)
                {
                    var files = userDir.GetFiles(string.Format("*.{0}.udn", langId));

                    if (files.Length > 0)
                    {
                        var linkedDocument = files[0].FullName.Equals(doc.LocalPath, StringComparison.InvariantCultureIgnoreCase)
                                                 ? doc
                                                 : doc.TransformationData.ProcessedDocumentCache.Get(files[0].FullName);

                        if (options.HasFlag(PathElementType.Bookmark) && emptyInputPath)
                        {
                            list.AddRange(linkedDocument.GetBookmarkKeys().Select(n => new PathElement(n, PathElementType.Bookmark)));
                        }

                        if (options.HasFlag(PathElementType.Excerpt))
                        {
                            list.AddRange(linkedDocument.GetExcerptNames().Select(n => new PathElement(n, PathElementType.Excerpt)));
                        }

                        if (options.HasFlag(PathElementType.Variable))
                        {
                            list.AddRange(linkedDocument.GetVariableNames().Select(n => new PathElement(n, PathElementType.Variable)));
                        }
                    }
                }
            }

            return list;
        }

        private static string GetSourceDir(string udnFilePath)
        {
            var sourceDir = Path.GetDirectoryName(udnFilePath);

            while (!sourceDir.EndsWith("Source"))
            {
                sourceDir = Path.GetDirectoryName(sourceDir);
            }

            return sourceDir;
        }

        private static readonly Regex RootKeywordPattern = new Regex(@"\%root\%", RegexOptions.IgnoreCase | RegexOptions.Compiled);
        private static readonly Regex LangIdPattern = new Regex(@"^.+\.(?<langId>[A-Z]{3})\.udn$", RegexOptions.Compiled);
    }
}
