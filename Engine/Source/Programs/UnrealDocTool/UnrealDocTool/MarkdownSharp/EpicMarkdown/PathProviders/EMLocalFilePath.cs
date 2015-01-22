// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.IO;
    using System.Text.RegularExpressions;

    using MarkdownSharp.FileSystemUtils;
    using MarkdownSharp.Preprocessor;

    public class EMLocalFilePath : EMPathProvider
    {
        private static readonly Regex ImageExtensionPattern = new Regex(
            @"\.png|\.gif|\.tga|\.bmp|\.jpg", RegexOptions.IgnoreCase | RegexOptions.Compiled);

        private bool changedLanguage;
        public override bool ChangedLanguage { get { return changedLanguage;} }
        public bool IsImage { get; private set; }

        private readonly string absolutePath;
        private readonly string dstRelative;

        public EMLocalFilePath(string userPath, EMDocument doc, TransformationData data, Func<string, string> outputFileNameConversion = null)
        {
            userPath = Preprocessor.UnescapeChars(userPath, true);

            if (userPath.StartsWith("%ROOT%"))
            {
                userPath = userPath.Replace("%ROOT%", ".");
            }

            if (outputFileNameConversion == null)
            {
                outputFileNameConversion = NoConversion;
            }

            var localized = data.CurrentFolderDetails.Language != "INT";
            IsImage = ImageExtensionPattern.IsMatch(userPath);

            var sourceDocDir = GetDirectoryName(userPath, doc, data);
            var fileName = Path.GetFileName(userPath);

            absolutePath = GetAbsoluteFilePath(sourceDocDir, fileName, data, localized);
            dstRelative = localized
                              ? Path.Combine(
                                  GetFolderTypeName(),
                                  sourceDocDir,
                                  data.CurrentFolderDetails.Language,
                                  outputFileNameConversion(Path.GetFileName(userPath)))
                              : Path.Combine(
                                  GetFolderTypeName(),
                                  sourceDocDir,
                                  outputFileNameConversion(Path.GetFileName(userPath)));

            if (!File.Exists(absolutePath))
            {
                if (localized)
                {
                    absolutePath = GetAbsoluteFilePath(sourceDocDir, fileName, data, false);
                    changedLanguage = File.Exists(absolutePath);
                }

                if (!localized || !changedLanguage)
                {
                    throw new EMPathVerificationException(
                        Language.Message(
                            string.Format(
                                "{0}FileNotFoundIn{1}INTDir",
                                IsImage ? "Image" : "Attachment",
                                data.CurrentFolderDetails.Language != "INT" ? "LanguageOr" : ""),
                            absolutePath));
                }

                dstRelative = Path.Combine(
                    GetFolderTypeName(),
                    sourceDocDir,
                    outputFileNameConversion(Path.GetFileName(userPath)));
            }
        }

        private static string NoConversion(string fileName)
        {
            return fileName;
        }

        private string GetAbsoluteFilePath(string sourceDocDir, string fileName, TransformationData data, bool localized)
        {
            return Path.Combine(
                GetFileFolder(data, sourceDocDir, localized), fileName);
        }

        private string GetDirectoryName(string userPath, EMDocument doc, TransformationData data)
        {
            var dir = Path.GetDirectoryName(userPath);

            if (string.IsNullOrWhiteSpace(dir))
            {
                dir = FileHelper.GetRelativePath(
                    data.CurrentFolderDetails.AbsoluteMarkdownPath, Path.GetDirectoryName(doc.LocalPath));
            }

            return dir;
        }

        private string GetFileFolder(TransformationData data, string location, bool localized)
        {
            return localized
                       ? Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, location,
                           GetFolderTypeName(), data.CurrentFolderDetails.Language)
                       : Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, location, GetFolderTypeName());
        }

        private string GetFolderTypeName()
        {
            return IsImage ? "images" : "attachments";
        }

        public string GetAbsolutePath(TransformationData data)
        {
            return Normalizer.NormalizePath(Markdown.Unescape(Path.Combine(data.CurrentFolderDetails.AbsoluteHTMLPath, dstRelative)));
        }

        public override string GetPath(TransformationData data)
        {
            return Normalizer.NormalizePath(Markdown.Unescape(Path.Combine(data.CurrentFolderDetails.RelativeHTMLPath, dstRelative)));
        }

        public string GetSource()
        {
            return absolutePath;
        }

        private static readonly Regex FilePathPattern = new Regex(@"\.\w+$", RegexOptions.Compiled);

        public static bool Verify(string userPath)
        {
            return FilePathPattern.IsMatch(userPath);
        }
    }
}
