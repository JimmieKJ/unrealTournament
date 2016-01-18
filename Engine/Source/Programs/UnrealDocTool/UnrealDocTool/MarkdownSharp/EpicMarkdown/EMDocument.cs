// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using DotLiquid;
using MarkdownSharp.FileSystemUtils;
using MarkdownSharp.Preprocessor;

namespace MarkdownSharp.EpicMarkdown
{
    using System;
    using System.Linq;
    using System.Text;

    using MarkdownSharp.EpicMarkdown.PathProviders;
    using MarkdownSharp.EpicMarkdown.Traversing;

    public class EMDocument
    {
        private object thisLock = new object();

        public TransformationData TransformationData { get; private set; }
        public string LocalPath { get; private set; }
        public PreprocessedTextLocationMap TextMap { get { return preprocessedData.TextMap; } }
        public PreprocessedTextLocationMap ExcerptTextMap { get { return preprocessedData.ExcerptTextMap; } }
        public PreprocessedData PreprocessedData { get { return preprocessedData; } }
        public string Text { get; private set; }

        public EMDocument(string path, TransformationData data)
        {
            LocalPath = path;
            TransformationData = data;
            preprocessedData = new PreprocessedData(this, data.ProcessedDocumentCache);
            publish = false;
        }

        // empty document constructor
        private EMDocument()
        {
            LocalPath = null;
            TransformationData = null;
            data = new EMElements(this, new EMElementOrigin(0, ""), null);
            preprocessedData = null;
            publish = true;
        }

        private readonly PreprocessedData preprocessedData;

        private string preprocessedText;
        private bool publish;

        private EMElements data;
        public EMElements Data
        {
            get
            {
                if (!publish)
                {
                    return null;
                }

                // lazy parsing
                if (data == null)
                {
                    ForceParsing();
                }

                return data;
            }
        }

        private void ForceParsing()
        {
            var elements = new EMElements(this, new EMElementOrigin(0, preprocessedText), null);

            elements.Parse(preprocessedText, TransformationData);

            data = elements;
        }

        public List<PreprocessedTextBound> GetPreprocessedTextBounds()
        {
            return PreprocessedData != null
                       ? PreprocessedData.PreprocessedTextBounds
                       : new List<PreprocessedTextBound>();
        }

        public VariableManager Variables { get { return preprocessedData.Variables; } }
        public MetadataManager Metadata { get { return preprocessedData.Metadata; } }
        public ExcerptsManager Excerpts { get { return preprocessedData.Excerpts; } }
        public ReferenceLinksManager ReferenceLinks { get { return preprocessedData.ReferenceLinks; } }

        public static EMDocument Empty = new EMDocument();

        public void Parse(string text, bool preprocessOnly = false, bool full = true)
        {
            Text = text;

            string relativePath = null;

            if (LocalPath != TransformationData.Document.LocalPath)
            {
                relativePath = GetAbsoluteMarkdownPath();
            }

            preprocessedText = TransformationData.Markdown.Preprocessor.Preprocess(text, TransformationData, preprocessedData, relativePath, full);
            publish = Markdown.CheckAvailability(TransformationData);

            if (!publish)
            {
                TransformationData.ErrorList.Add(
                    new ErrorDetail(Language.Message("CouldNotPublish"), MessageClass.Info, "", "", 0, 0));
            }
            else
            {
                if (!preprocessOnly)
                {
                    ForceParsing();
                }
            }
        }

        public override string ToString()
        {
            return LocalPath;
        }

		public bool PerformStrictConversion()
		{
			return !GetAbsoluteMarkdownPath().Replace('\\', '/').StartsWith("API/");
		}

        public string GetAbsolutePath(string relativePath)
        {
            return
                new FileInfo(Path.Combine(
                    TransformationData.CurrentFolderDetails.AbsoluteMarkdownPath, relativePath)).FullName;
        }

        public string GetAbsoluteMarkdownPath()
        {
            return
                FileHelper.GetRelativePath(
                    TransformationData.CurrentFolderDetails.AbsoluteMarkdownPath,
                    Path.GetDirectoryName(LocalPath));
        }

        public EMBookmark GetBookmark(string name)
        {
            var bookmarks = Data.GetElements<EMBookmark>();
            var key = name.ToLower();

            var bookmark = bookmarks.FirstOrDefault(e => e.UniqueKey == key);

            return bookmark == null ? bookmarks.FirstOrDefault(e => e.UniqueKey == key) : bookmark;
        }

        public string GetRelativeTargetPath(EMDocument current)
        {
            var relPath = FileHelper.GetRelativePath(Path.GetDirectoryName(current.LocalPath), Path.GetDirectoryName(LocalPath));
            return string.IsNullOrWhiteSpace(relPath) ? "" : Path.Combine(relPath, "index.html").Replace('\\', '/');
        }

        public List<string> GetUsedImagesPaths()
        {
            var images = Data.GetElements<EMImage>();

            var paths = new HashSet<string>();

            foreach (var image in images)
            {
                var path = Path.GetFullPath(image.Path.GetSource());
                if (!paths.Contains(path))
                {
                    paths.Add(path);
                }
            }

            return paths.ToList();
        }

        public List<string> GetBookmarkKeys()
        {
            return Data.GetElements<EMBookmark>().Select(b => b.UniqueKey).ToList();
        }

        public List<string> GetExcerptNames()
        {
            return Excerpts.GetExcerptNames();
        }

        public List<string> GetVariableNames()
        {
            return Variables.GetVariableNames();
        }

        public void FlattenInternalLinks(TransformationData data)
        {
            if (Data == null)
            {
                return;
            }

            var bookmarks = new Dictionary<string, EMBookmark>();

            Data.GetElements<EMBookmark>().ForEach((bookmark) =>
            {
                if (bookmarks.ContainsKey(bookmark.UniqueKey))
                {
                    var errorId = data.ErrorList.Count;
                    data.ErrorList.Add(
                        Markdown.GenerateError(
                            Language.Message("ConflictingBookmarkNameOrHeaderId", bookmark.UniqueKey),
                            MessageClass.Info,
                            bookmark.Origin.Text,
                            errorId,
                            data));
                    return;
                }

                bookmarks.Add(bookmark.UniqueKey, bookmark);
            });

            var inclusions = new Dictionary<string, EMInclude>();

            Data.GetElements<EMInclude>().ForEach(e =>
            {
                if (!e.Excerpt.IsDocumentExcerpt)
                {
                    return;
                }

                e.AddHashedPathBookmarkToOutput();
                var inclusionPath = e.Excerpt.Document.LocalPath.ToLower();

                if(!inclusions.ContainsKey(inclusionPath))
                {
                    inclusions.Add(inclusionPath, e);
                }
            });

            foreach (
                var link in
                    Data.GetElements(
                        e =>
                        (e is EMLink) && (e as EMLink).Path is EMLocalDocumentPath).Cast<EMLink>())
            {
                var path = link.Path as EMLocalDocumentPath;
                var id = path.BookmarkName;

                if ((path.IsBookmark && (path.DocumentPath != path.currentDocument.LocalPath || !bookmarks.ContainsKey(id)))
                    || (!path.IsBookmark && !inclusions.ContainsKey(path.DocumentPath.ToLower())))
                {
                    if (data.NonDynamicHTMLOutput)
                    {
                        link.ConvertToJustTextOutput();
                    }
                }
                else
                {
                    link.ConvertToInternal();
                }
            }
        }

        public string Render()
        {
            var elements = Data;

            if (elements == null || TransformationData == null)
            {
                return "";
            }

            return (TransformationData.Markdown.DoAddHTMLWrapping(
                    Preprocessor.Preprocessor.UnescapeChars(elements.GetInnerHTML(new Stack<EMInclude>(), TransformationData)), TransformationData) + "\n")
                    .Replace("\n", Environment.NewLine);
        }

        public string ThreadSafeRender()
        {
            lock (thisLock)
            {
                return Render();
            }
        }

        public string GetAbsoluteTargetPath()
        {
            return new FileInfo(Path.Combine(
                TransformationData.CurrentFolderDetails.AbsoluteHTMLPath,
                TransformationData.CurrentFolderDetails.Language,
                TransformationData.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf,
                GetRelativeTargetPath(TransformationData.Document))).FullName;
        }
    }
}
