// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using MarkdownSharp.Preprocessor;

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.IO;
    using System.Text.RegularExpressions;

    using MarkdownSharp.FileSystemUtils;

    public class EMLocalDocumentPath : EMPathProvider
    {
        public readonly EMDocument currentDocument;

        private static readonly Regex DocumentPathPattern = new Regex("^(?<path>[^#]*)(#(?<bookmark>[^#]+))?$", RegexOptions.Compiled);

        public string DocumentPath { get; private set; }
        public string BookmarkName { get; private set; }

        private readonly bool changedLanguage;
        public override bool ChangedLanguage { get { return changedLanguage; } }

        public EMLocalDocumentPath(string userPath, EMDocument currentDocument, TransformationData data)
        {
            userPath = Preprocessor.Preprocessor.UnescapeChars(userPath, true);

            if (userPath.StartsWith("%ROOT%"))
            {
                userPath = userPath.Replace("%ROOT%", ".");
            }

            this.currentDocument = currentDocument;
            var match = DocumentPathPattern.Match(userPath);

            if (!match.Success)
            {
                throw new EMPathVerificationException(Language.Message("InvalidLocalDocumentPath", userPath));
            }

            DocumentPath = match.Groups["path"].Value;

            changedLanguage = false;

            if (string.IsNullOrWhiteSpace(DocumentPath))
            {
                DocumentPath = currentDocument.LocalPath;
            }
            else
            {
                DirectoryInfo docDir;
                try
                {
                    docDir = new DirectoryInfo(Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, DocumentPath));
                }
                catch (Exception e)
                {
                    if (e is NotSupportedException || e is ArgumentException)
                    {
                        throw new EMPathVerificationException(Language.Message("InvalidLocalDocumentPath", userPath));
                    }

                    throw;
                }

                if (!docDir.Exists)
                {
                    throw new EMPathVerificationException(Language.Message("BadLinkOrMissingMarkdownFileForLink", DocumentPath));
                }

                var docFiles = docDir.GetFiles(string.Format("*.{0}.udn", data.CurrentFolderDetails.Language));

                if (docFiles.Length == 0)
                {
                    if (data.CurrentFolderDetails.Language != "INT")
                    {
                        docFiles = docDir.GetFiles("*.INT.udn");
                        changedLanguage = docFiles.Length != 0;
                    }

                    if (data.CurrentFolderDetails.Language == "INT" || !ChangedLanguage)
                    {
                        throw new EMPathVerificationException(Language.Message("BadLinkOrMissingMarkdownFileForLink", DocumentPath));
                    }
                }

                DocumentPath = docFiles[0].FullName;
            }

            BookmarkName = match.Groups["bookmark"].Value;

            if (string.IsNullOrWhiteSpace(BookmarkName))
            {
                BookmarkName = null;
            }
        }

        public override string GetPath(TransformationData data)
        {
            if (isInternalOutput)
            {
                if (IsBookmark)
                {
                    return "index.html#" + BookmarkName;
                }
                else
                {
                    return "index.html#" + DocumentPathId(DocumentPath.ToLower());
                }
            }

            var path =
                Path.Combine(
                    FileHelper.GetRelativePath(
                        Path.GetDirectoryName(data.Document.LocalPath), Path.GetDirectoryName(DocumentPath)),
                    "index.html");

            if (IsBookmark)
            {
                var bookmark = data.ProcessedDocumentCache.Get(DocumentPath).GetBookmark(BookmarkName);
                if (bookmark == null)
                {
                    throw new EMPathVerificationException(Language.Message("BookmarkNotFound", BookmarkName));
                }

                path += "#" + bookmark.UniqueKey;
            }

            return Normalizer.NormalizePath(path);
        }

        public static string DocumentPathId(string documentPath)
        {
            return documentPath.GetHashCode().ToString();
        }

        public override bool IsBookmark { get { return BookmarkName != null; } }

        private bool isInternalOutput = false;

        public void ConvertToInternalOutput()
        {
            isInternalOutput = true;
        }
    }
}
