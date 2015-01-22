// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.IO;
    using System.Text.RegularExpressions;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;
    using DotLiquid;
    using MarkdownSharp.EpicMarkdown.Traversing;

    [LiquidType("Children", "ChildrenCount", "Level", "Text", "Url", "VirtualBranch")]
    public class TOCBranch
    {
        public List<TOCBranch> Children { get; private set; }
        public int ChildrenCount { get { return Children.Count; } }
        public int Level { get; private set; }
        public string Text { get; private set; }
        public string Url { get; private set; }
        public bool VirtualBranch { get; private set; }

        // root constructor
        private TOCBranch()
            : this(0, null, null, true)
        {
                       
        }

        public TOCBranch(int level, string text, string url, bool virtualBranch = false)
        {
            Children = new List<TOCBranch>();
            Level = level;
            Text = text;
            Url = url;
            VirtualBranch = virtualBranch;
        }


        class TOCTreeBuildingContext : ITraversingContext
        {
            private readonly Action<EMHeader, Stack<EMInclude>> action;
            private readonly Stack<EMInclude> includesStack;

            public TOCTreeBuildingContext(Action<EMHeader, Stack<EMInclude>> action)
            {
                this.action = action;
                includesStack = new Stack<EMInclude>();
            }

            public void PreChildrenVisit(ITraversable traversedElement)
            {
                if (traversedElement is EMInclude)
                {
                    includesStack.Push(traversedElement as EMInclude);
                }
            }

            public void PostChildrenVisit(ITraversable traversedElement)
            {
                if (traversedElement is EMHeader)
                {
                    action(traversedElement as EMHeader, includesStack);
                }

                if (traversedElement is EMInclude)
                {
                    includesStack.Pop();
                }
            }
        }

        public static TOCBranch GetFromDocument(EMDocument linkedDocument, int startLevel, int endLevel)
        {
            var root = new TOCBranch();

            linkedDocument.Data.ForEachWithContext(
                new TOCTreeBuildingContext(
                    (header, includesStack) =>
                    {
                        var level = header.GetLevelWithOffsets(includesStack);

                        if(level >= startLevel && level <= endLevel)
                        {
                            root.Add(level, header.Text, "#" + header.UniqueKey);
                        }
                    }));

            root.DeleteNonRootVirtualBranches();

            return root;
        }

        private void DeleteNonRootVirtualBranches()
        {
            if (ChildrenCount == 0)
            {
                return;
            }

            var newChildren = new List<TOCBranch>();

            foreach (var child in Children)
            {
                child.DeleteNonRootVirtualBranches();

                if (child.VirtualBranch)
                {
                    foreach (var innerChild in child.Children)
                    {
                        newChildren.Add(innerChild);
                    }
                }
            }

            if (newChildren.Count != 0)
            {
                Children = newChildren;
            }
        }

        private void AddVirtual(int level)
        {
            if (Level == level - 1)
            {
                Children.Add(new TOCBranch(level, null, null, true));
            }
            else
            {
                Children.Last().AddVirtual(level);
            }
        }

        private void Add(int htmlLevel, string text, string url)
        {
            if (Level == htmlLevel - 1)
            {
                Children.Add(new TOCBranch(htmlLevel, text, url));
            }
            else
            {
                if (ChildrenCount == 0)
                {
                    AddMissingLevels(htmlLevel - 1);
                }
            
                Children.Last().Add(htmlLevel, text, url);
            }
        }

        private void AddMissingLevels(int toWhichLevel)
        {
            for (var i = Level + 1; i <= toWhichLevel; ++i)
            {
                AddVirtual(i);
            }
        }
    }

    public class EMTOCInline : EMElement
    {
        private readonly int startLevel;
        private readonly int endLevel;
        private readonly EMDocument linkedDocument;
        private readonly string path;

        private EMTOCInline(EMDocument doc, EMElementOrigin origin, EMElement parent, int startLevel, int endLevel, EMDocument linkedDocument, string path)
            : base(doc, origin, parent)
        {
            this.startLevel = startLevel;
            this.endLevel = endLevel != 0 ? endLevel : int.MaxValue;
            this.linkedDocument = linkedDocument;
            this.path = path;
        }

        public override string GetClassificationString()
        {
            return "markdown.tableofcontents";
        }

        private const string TOCPatternWithoutFencing = @"(?<content>((?<path>\w+([\\/]+\w+)*[\\/]*):)?toc\s*(\((?<params>\w+:\d+(\s+\w+:\d+)*)\))?)";
        public static readonly Regex TOCPatternVariableLike = new Regex(@"\%" + TOCPatternWithoutFencing + @"\%", RegexOptions.Compiled | RegexOptions.IgnoreCase);
        public static readonly Regex TOCPattern = new Regex(@"(?<=^)\[" + TOCPatternWithoutFencing + @"\]", RegexOptions.Compiled | RegexOptions.Multiline | RegexOptions.IgnoreCase);
        private static readonly Regex SpacePattern = new Regex(@"\s+", RegexOptions.Compiled);

        public static string ConvertVariableLikeTOCInlines(string text, PreprocessedTextLocationMap map)
        {
            var evaluator = new MatchEvaluator(match => "[" + match.Groups["content"].Value + "]");

            return map == null
                       ? TOCPatternVariableLike.Replace(text, evaluator)
                       : Preprocessor.Replace(TOCPatternVariableLike, text, evaluator, map);
        }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(TOCPattern, CreateTOCInline);
        }

        private static EMElement CreateTOCInline(
            Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var start = 0;
            var end = 0;
            
            SpacePattern.Split(match.Groups["params"].Value).ToList().ForEach(
                param =>
                    {
                        if (string.IsNullOrWhiteSpace(param))
                        {
                            return;
                        }

                        var parts = param.Split(':');
                        var name = parts[0];
                        var value = int.Parse(parts[1]);

                        switch (name)
                        {
                            case "start":
                                start = value;
                                break;
                            case "end":
                                end = value;
                                break;
                        }
                    });

            var path = match.Groups["path"].Value;
            ClosestFileStatus status;
            var linkedDoc = data.ProcessedDocumentCache.GetClosest(path, out status);

            if (status == ClosestFileStatus.FileMissing)
            {
                return EMErrorElement.Create(doc, origin, parent, data, "BadIncludeOrMissingMarkdownFile", path);
            }

            if (status == ClosestFileStatus.ChangedToIntVersion || status == ClosestFileStatus.ExactMatch)
            {
                EMElement output = new EMTOCInline(doc, origin, parent, start, end, linkedDoc, path);

                if (status == ClosestFileStatus.ChangedToIntVersion)
                {
                    output.AddMessage(new EMReadingMessage(MessageClass.Info, "BadIncludeOrMissingMarkdownFileINTUsed", path), data);
                }

                return output;
            }

            // should never happen!
            throw new InvalidOperationException("Bad linking file status.");
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            if (includesStack.Count > 0 && !includesStack.First().IncludeTOC)
            {
                return;
            }

            var isLocalTOC = linkedDocument == Document.TransformationData.Document;

            builder.Append(RenderTOC(
                startLevel,
                endLevel,
                string.IsNullOrWhiteSpace(path)
                    ? ""
                    : Path.Combine(
                        data.CurrentFolderDetails.RelativeHTMLPath,
                        data.CurrentFolderDetails.Language,
                        path,
                        "index.html"),
                !isLocalTOC));
        }

        public string RenderTOC(int startLevel, int endLevel, string filePrefix, bool includeTitle = true)
        {
            var start = startLevel == 0 ? 1 : startLevel;
            var end = endLevel == 0 ? int.MaxValue : endLevel;

            var title = "";

            if (includeTitle && !linkedDocument.PreprocessedData.Variables.TryGet("title", out title))
            {
                title = Templates.ErrorHighlight.Render(Hash.FromAnonymousObject(
                    new
                    {
                        errorText = Language.Message("VariableOrMetadataNotFoundInFile", "title", filePrefix)
                    }));
            }

            return Templates.Toc.Render(Hash.FromAnonymousObject(
                new
                {
                    includeTitle,
                    titleUrl = "#H1TitleId",
                    titleText = title,
                    filePrefix,
                    toc = GetTOCTree(start, end)
                }));
        }

        private TOCBranch GetTOCTree(int startLevel, int endLevel)
        {
            return TOCBranch.GetFromDocument(linkedDocument, startLevel, endLevel);
        }
    }
}
