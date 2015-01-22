// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.EpicMarkdown.PathProviders;
    using MarkdownSharp.Preprocessor;
    using MarkdownSharp.EpicMarkdown.Traversing;

    public struct Location
    {
        public int Line;
        public int Column;
        public string Source;
    }

    public abstract class EMContentElement : EMElement
    {
        private readonly PreprocessedTextLocationMap localTextMap;
        public PreprocessedTextLocationMap TextMap { get { return localTextMap; } }

        protected EMContentElement(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {
            localTextMap = new PreprocessedTextLocationMap();
        }

        public void Add(EMElement element)
        {
            if (element != null)
            {
                elements.Add(element.Origin.Start, element);
            }
        }

        protected Location GetLocation(int position)
        {
            var origPosition = GetOriginalPosition(position, PositionRounding.Down);
            if (!InExcerpt())
            {
                origPosition = Document.TextMap.GetOriginalPosition(origPosition, PositionRounding.Down);
            }
            else
            {
                origPosition = Document.ExcerptTextMap.GetOriginalPosition(origPosition, PositionRounding.Down);
            }

            Location loc;
            loc.Source = Document.LocalPath;
            GetLineAndColumn(Document.Text, origPosition, out loc.Line, out loc.Column);
            
            return loc;
        }

        private static void GetLineAndColumn(string text, int position, out int line, out int column)
        {
            line = 1;
            column = 1;
            for (var i = 0; i < position; ++i)
            {
                if (text[i] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else if (text[i] == '\r')
                {
                    // ignore it
                }
                else
                {
                    ++column;
                }
            }
        }

        public void Clear()
        {
            elements.Clear();
        }

        public int ElementsCount { get { return elements.Count; } }
        public SortedList<int, EMElement> Elements { get { return elements; } } 

        readonly SortedList<int, EMElement> elements = new SortedList<int, EMElement>();

        protected void ParseElements(TransformationData data, EMDocument doc, List<TextFragment> fragments, IEnumerable<IParser> parsers)
        {
            foreach (var parser in parsers)
            {
                ParseElements(data, doc, fragments, parser);
            }
        }

        protected void ParseElements(TransformationData data, List<TextFragment> fragments, IEnumerable<IParser> parsers)
        {
            ParseElements(data, Document, fragments, parsers);
        }

        protected void ParseElements(TransformationData data, EMDocument doc, List<TextFragment> fragments, IParser parser)
        {
            EMParsingHelper.ParseElements(data, doc, this, fragments, parser, Add);
        }

        protected void ParseElements(TransformationData data, List<TextFragment> fragments, IParser parser)
        {
            EMParsingHelper.ParseElements(data, Document, this, fragments, parser, Add);
        }

        public override void ForEachWithContext<TContext>(TContext context)
        {
            context.PreChildrenVisit(this);

            foreach (var element in elements)
            {
                element.Value.ForEachWithContext(context);
            }

            context.PostChildrenVisit(this);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            foreach (var element in elements)
            {
                element.Value.AppendHTML(builder, includesStack, data);
            }
        }

        public List<EMElement> GetElements(Func<EMElement, bool> predicate, int minLevel, int maxLevel)
        {
            var output = new List<EMElement>();

            ForEach(
                element =>
                {
                    if (predicate(element))
                    {
                        output.Add(element);
                    }
                }, minLevel, maxLevel);

            return output;
        }

        public List<EMElement> GetElements(Func<EMElement, bool> predicate)
        {
            return GetElements(predicate, 0, int.MaxValue);
        }

        public List<TEMType> GetElements<TEMType>(int minLevel, int maxLevel) where TEMType : EMElement
        {
            return GetElements((e) => e is TEMType, minLevel, maxLevel).Cast<TEMType>().ToList();
        }

        public List<TEMType> GetElements<TEMType>(int maxLevel) where TEMType : EMElement
        {
            return GetElements<TEMType>(0, maxLevel);
        }

        public List<TEMType> GetElements<TEMType>() where TEMType : EMElement
        {
            return GetElements<TEMType>(0, int.MaxValue);
        }

        public string GetInnerHTML(Stack<EMInclude> includesStack, TransformationData data)
        {
            var content = new StringBuilder();

            AppendHTML(content, includesStack, data);

            return content.ToString();
        }

        public override Interval GetPreprocessedTextBounds()
        {
            var start = Parent == null
                            ? Origin.GetNonWhitespaceStart() + Origin.Start
                            : Parent.GetOriginalPosition(Origin.GetNonWhitespaceStart() + Origin.Start, PositionRounding.Down);
            var end = Parent == null
                          ? Origin.GetNonWhitespaceEnd() + Origin.Start
                          : Parent.GetOriginalPosition(Origin.GetNonWhitespaceEnd() + Origin.Start, PositionRounding.Up);

            Interval interval;
            interval.Start = start;
            interval.End = end;

            return interval;
        }

        public override int GetOriginalPosition(int position, PositionRounding rounding)
        {
            position = TextMap.GetOriginalPosition(position, rounding) + Origin.Start;

            if (Parent != null)
            {
                var startOriginalPosition = Parent.GetOriginalPosition(position, rounding);

                return startOriginalPosition;
            }

            return position;
        }

        public void Parse(string text, TransformationData data)
        {
            Parse(0, text, data);
        }

        public void Parse(int startIndex, string text, TransformationData data)
        {
            text = data.ProcessedDocumentCache.Variables.ReplaceVariables(Document, text, data, localTextMap);

            if (string.IsNullOrWhiteSpace(text))
            {
                return;
            }

            var fragments = new List<TextFragment> { new TextFragment(startIndex, text) };

            Parse(fragments, data);
        }

        public abstract void Parse(List<TextFragment> fragments, TransformationData data);
    }
}
