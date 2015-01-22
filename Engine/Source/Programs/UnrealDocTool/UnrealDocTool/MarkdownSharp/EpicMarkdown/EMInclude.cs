// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.IO;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.EpicMarkdown.PathProviders;
    using MarkdownSharp.Preprocessor;

    public class EMInclude : EMElement
    {
        public EMInclude(EMDocument doc, EMElementOrigin origin, EMElement parent, EMExcerpt excerpt, int headerOffset = 1, bool languageChanged = false, bool includeTOC = true)
            : base(doc, origin, parent)
        {
            Excerpt = excerpt;
            HeaderOffset = headerOffset;
            LanguageChanged = languageChanged;
            IncludeTOC = includeTOC;
        }

        public EMExcerpt Excerpt { get; private set; }
        public int HeaderOffset { get; private set; }
        public bool LanguageChanged { get; private set; }
        public bool IncludeTOC { get; private set; }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(IncludePattern, Create);
        }

        public static EMElement CreateFromText(
            string text, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return Create(IncludePattern.Match(text), doc, origin, parent, data);
        }

        private static EMElement Create(
            Match match,
            EMDocument doc, 
            EMElementOrigin origin,
            EMElement parent,
            TransformationData data)
        {
            var includeFileFolderName = "";
            var includeRegion = "";

            var thisOffset = 0;

            if (match.Groups["includeFileRegion"].Value.Contains('#'))
            {
                includeFileFolderName = match.Groups["includeFileRegion"].Value.Split('#')[0];
                includeRegion = match.Groups["includeFileRegion"].Value.Split('#')[1];

                if (!includeRegion.StartsWith("doxygen"))
                {
                    includeRegion = includeRegion.ToLower();
                }
            }
            else
            {
                includeFileFolderName = match.Groups["includeFileRegion"].Value;
            }

            if (includeFileFolderName.ToUpper().Contains("%ROOT%"))
            {
                includeFileFolderName = ".";
            }

            if (String.IsNullOrWhiteSpace(includeFileFolderName))
            {
                if (String.IsNullOrWhiteSpace(includeRegion))
                {
                    // Error cannot have no region and no file specified.
                    return EMErrorElement.Create(
                        data.Document,
                        origin,
                        parent,
                        data,
                        "ExcerptRegionToIncludeWhenNoFileGiven");
                }

                // Assume that this is a reference to a location in this file.
                includeFileFolderName = doc.GetAbsoluteMarkdownPath();
            }

            if (!String.IsNullOrWhiteSpace(match.Groups["offsetValue"].Value)
                && !int.TryParse(match.Groups["offsetValue"].Value, out thisOffset))
            {
                return EMErrorElement.Create(
                    data.Document,
                    origin,
                    parent,
                    data,
                    "ValueMustBeNumber");
            }

            try
            {
                bool languageChanged;

                var excerpt =
                    ExcerptsManager.Get(data.ProcessedDocumentCache,
                        Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, includeFileFolderName),
                        data.CurrentFolderDetails.Language,
                        includeRegion,
                        out languageChanged);

                return new EMInclude(doc, origin, parent, excerpt, thisOffset, languageChanged);
            }
            catch (ExcerptsManagerException e)
            {
                return EMErrorElement.Create(doc, origin, parent, data, e.MessageId, e.MessageArgs);
            }
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            Excerpt.Content.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }

        private static readonly Regex IncludePattern =
            new Regex(
                string.Format(
                    @"^[ ]{{0,{0}}}\[Include:(?<includeFileRegion>[^\]\n]*?)[ ]*(?:\]|(?:\(offset:[ ]*(?<offsetValue>[^\)\n]*?)[ ]*\)[ ]*\]))",
                    Markdown.TabWidth - 1),
                RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            includesStack.Push(this);

            if (addHashedPathBookmarkToOutput)
            {
                builder.Append(Templates.Bookmark.Render(Hash.FromAnonymousObject(new { bookmark = EMLocalDocumentPath.DocumentPathId(Excerpt.Document.LocalPath.ToLower()) })));
            }

            Excerpt.Content.AppendHTML(builder, includesStack, data);

            includesStack.Pop();
        }

        private bool addHashedPathBookmarkToOutput = false;

        public void AddHashedPathBookmarkToOutput()
        {
            addHashedPathBookmarkToOutput = true;
        }

        private static readonly Regex IncludeFilesPattern = new Regex(@"^\[INCLUDE_FILES:(?<specs>.+)\]$", RegexOptions.Compiled);

        public static EMElement CreateIncludeFilesFromText(string text, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var specs = Preprocessor.UnescapeChars(IncludeFilesPattern.Match(text).Groups["specs"].Value, true);

            var paths = GetIncludesFromPathSpecs(specs, data);

            var inclusions = new EMElements(doc, origin, parent);

            var i = 0;

            foreach (var path in paths)
            {
                var pathOrigin = new EMElementOrigin(i, "");

                try
                {
                    bool languageChanged;

                    var excerpt =
                        ExcerptsManager.Get(data.ProcessedDocumentCache,
                            Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, path),
                            data.CurrentFolderDetails.Language,
                            "",
                            out languageChanged);

                    inclusions.Add(new EMInclude(doc, pathOrigin, inclusions, excerpt, 0, languageChanged, false));
                }
                catch (ExcerptsManagerException e)
                {
                    inclusions.Add(EMErrorElement.Create(doc, pathOrigin, inclusions, data, e.MessageId, e.MessageArgs));
                }

                ++i;
            }

            return inclusions;
        }

        private static IEnumerable<string> GetIncludesFromPathSpecs(string includePathSpecs, TransformationData data)
        {
            var output = new List<string>();

            foreach (var spec in includePathSpecs.Split(';'))
            {
                var path = Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, spec);

                if (Path.GetFileName(path) == "*")
                {
                    AppendRecursively(new DirectoryInfo(Path.GetDirectoryName(path)), output);
                }
                else
                {
                    output.Add(path);
                }
            }

            return output;
        }

        private static void AppendRecursively(DirectoryInfo dir, ICollection<string> output)
        {
            if (dir.GetFiles("*.udn").Length != 0)
            {
                output.Add(dir.FullName);
            }

            foreach (var child in dir.GetDirectories())
            {
                AppendRecursively(child, output);
            }
        }
    }
}
