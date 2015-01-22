// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Text.RegularExpressions;

    public class EMMarkdownAndHTMLTagsParser : EMTaggedElementsParser
    {
        private abstract class EMMarkdownAndHTMLStartOrEndTagMatch : StartOrEndTagMatch
        {
            public EMMarkdownAndHTMLStartOrEndTagMatch(int index, string text)
                : base(index, text)
            {
                IsHTMLTag = Text[0] == '<';
            }

            public bool IsHTMLTag { get; private set; }
            
            public bool IsIgnored { get { return !IsSupported(); } }
            private static readonly string[] SupportedMarkdownTagsList = new[] { "region", "include", "include_files", "relative", "relative_img", "object" };
            private static readonly string[] SupportedHTMLTagsList = new[]
                                                                         {
                                                                             "a", "b", "blockquote", "body", "br", "code",
                                                                             "col", "colgroup", "dd", "div", "dl", "dt",
                                                                             "em", "fieldset", "form", "frame", "h1", "h2",
                                                                             "h3", "h4", "h5", "h6", "head", "hr", "html",
                                                                             "i", "iframe", "img", "input", "label", "li",
                                                                             "link", "ol", "optgroup", "option", "p",
                                                                             "param", "pre", "q", "select", "small",
                                                                             "span", "strong", "style", "table", "tbody",
                                                                             "td", "textarea", "tfoot", "th", "thead",
                                                                             "title", "tr", "tt", "u", "ul"
                                                                         };

            private bool IsSupported()
            {
                return (IsMarkdownTag && SupportedMarkdownTagsList.Contains(Name.ToLower())) || (IsHTMLTag && SupportedHTMLTagsList.Contains(Name.ToLower()));
            }

            public abstract string Name { get; }
            public bool IsMarkdownTag { get { return !IsHTMLTag; } }
        }

        private class EMMarkdownAndHTMLStartTagMatch : EMMarkdownAndHTMLStartOrEndTagMatch, IStartTagMatch
        {
            private static readonly Regex HTMLTagContentPattern = new Regex(@"^<(?<name>\w+)\s*(?<attributesString>[^>\/]+)?(?<tagEnding>\/\s*)?>$", RegexOptions.Compiled);
            private static readonly Regex MarkdownTagContentPattern = new Regex(@"^\[(?<name>\w+):(?<attributesString>[^:\]]+)\]$", RegexOptions.Compiled);

            public EMMarkdownAndHTMLStartTagMatch(int index, string text)
                : base(index, text)
            {
                var parsedTag = IsHTMLTag
                                    ? HTMLTagContentPattern.Match(text)
                                    : MarkdownTagContentPattern.Match(text);

                if (!parsedTag.Success)
                {
                    throw new ArgumentException(string.Format("Not a proper tag definition. Provided '{0}'.", text), "text");
                }

                name = parsedTag.Groups["name"].Value;
                AttributesString = parsedTag.Groups["attributesString"].Value;
                HasEnding = (IsHTMLTag
                             && (!parsedTag.Groups["tagEnding"].Success && !HTMLNoEndingList.Contains(name.ToLower())))
                            || (IsMarkdownTag && !MarkdownNoEndingList.Contains(name.ToLower()));
            }

            public bool HasEnding { get; private set; }
            public string AttributesString { get; private set; }

            private readonly string name;
            public override string Name { get { return name; } }

            private static readonly string[] MarkdownNoEndingList = new[] { "include", "include_files", "relative", "relative_img" };
            private static readonly string[] HTMLNoEndingList = new[] { "br", "hr" };
        }

        private class EMMarkdownAndHTMLEndTagMatch : EMMarkdownAndHTMLStartOrEndTagMatch, IEndTagMatch
        {
            private static readonly Regex HTMLTagContentPattern = new Regex(@"^<\/(?<name>\w+)\s*>", RegexOptions.Compiled);
            private static readonly Regex MarkdownTagContentPattern = new Regex(@"^\[\/(?<name>\w+)(:[^:\]]+)?\]", RegexOptions.Compiled);

            public EMMarkdownAndHTMLEndTagMatch(int index, string text)
                : base(index, text)
            {
                var parsedTag = IsHTMLTag
                    ? HTMLTagContentPattern.Match(text)
                    : MarkdownTagContentPattern.Match(text);

                if (!parsedTag.Success)
                {
                    throw new ArgumentException(string.Format("Not a proper tag definition. Provided '{0}'.", text), "text");
                }

                name = parsedTag.Groups["name"].Value;
            }

            private readonly string name;
            public override string Name { get { return name; } }
        }

        private readonly bool spanParser;

        public EMMarkdownAndHTMLTagsParser(Func<int, Location> getLocationDelegate, bool spanParser = false)
            : base(false, getLocationDelegate)
        {
            this.spanParser = spanParser;
        }

        protected override List<ITagMatch> GetTags(string text)
        {
            var output = new List<ITagMatch>();

            AppendTags(output, text);

            return output;
        }

        private void AppendTags(List<ITagMatch> output, string text)
        {
            char endContentReadingChar = ' ';
            int startPos = -1;

            StringBuilder content = null;

            for (var i = 0; i < text.Length; ++i)
            {
                var c = text[i];

                if (content == null)
                {
                    if (((c == '[' && text.Length > i + 1 && text[i + 1] != '[') || (c == '<' && text.Length > i + 1 && text[i + 1] != '<')) && (spanParser || i == 0 || text[i - 1] == '\n'))
                    {
                        content = new StringBuilder();
                        endContentReadingChar = !spanParser ? '\n' : (c == '[' ? ']' : '>');
                        startPos = i;
                    }
                }

                if (content != null)
                {
                    content.Append(c);

                    if (c == endContentReadingChar || i == text.Length - 1)
                    {
                        var str = content.ToString().Trim();

                        try
                        {
                            EMMarkdownAndHTMLStartOrEndTagMatch tag;

                            if (str.Length > 1 && str[1] == '/')
                            {
                                tag = new EMMarkdownAndHTMLEndTagMatch(startPos, str);
                            }
                            else
                            {
                                tag = new EMMarkdownAndHTMLStartTagMatch(startPos, str);
                            }

                            if (!tag.IsIgnored)
                            {
                                output.Add(tag);
                            }
                        }
                        catch (ArgumentException)
                        {
                            // ignore invalid tags
                        }

                        content = null;
                    }
                }
            }
        }

        protected override EMTaggedElementMatch CreateElement(string text, ITagMatch start, ITagMatch end)
        {
            var tagElement = base.CreateElement(text, start, end);

            var mStart = start as EMMarkdownAndHTMLStartTagMatch;

            if (!spanParser && !tagElement.Text.Contains("\n"))
            {
                return null;
            }

            return mStart.IsHTMLTag
                ? (EMTaggedElementMatch)
                new EMRawHTMLMatch(
                    tagElement.Index,
                    tagElement.Text,
                    mStart.Name,
                    tagElement.ContentStart,
                    tagElement.ContentLength,
                    mStart.AttributesString)
                : new EMMarkdownTaggedElementMatch(
                    tagElement.Index,
                    tagElement.Text,
                    mStart.Name,
                    tagElement.ContentStart,
                    tagElement.ContentLength,
                    mStart.AttributesString);
        }

        protected override bool VerifyMatch(string text, ITagMatch start, ITagMatch end)
        {
            var mStart = start as EMMarkdownAndHTMLStartTagMatch;
            var mEnd = end as EMMarkdownAndHTMLEndTagMatch;

            return mStart.IsHTMLTag == mEnd.IsHTMLTag && string.Equals(mStart.Name, mEnd.Name, StringComparison.InvariantCultureIgnoreCase);
        }

        protected override EMTaggedElementMatch CreateNoEndElement(ITagMatch start)
        {
            var mStart = start as EMMarkdownAndHTMLStartTagMatch;

            return mStart.IsMarkdownTag
                ? (EMTaggedElementMatch)
                new EMMarkdownTaggedElementMatch(
                    start.Index, start.Text, mStart.Name, 0, 0, mStart.AttributesString)
                : new EMRawHTMLMatch(
                    start.Index, start.Text, mStart.Name, 0, 0, mStart.AttributesString);
        }

        public override EMElement Create(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            if (match is EMRawHTMLMatch)
            {
                var htmlMatch = match as EMRawHTMLMatch;
                if (htmlMatch.Name.ToLower() == "code")
                {
                    return new EMCodeBlock(doc, origin, parent, htmlMatch.Content, true);
                }

                var element = new EMRawHTML(doc, origin, parent, htmlMatch.Name, htmlMatch.AttributesString);

                element.Elements.Parse(Markdown.Outdent(htmlMatch.Content, element.Elements.TextMap), data);

                return element;
            }

            if (match is EMMarkdownTaggedElementMatch)
            {
                var markdownMatch = match as EMMarkdownTaggedElementMatch;

                switch (markdownMatch.Name.ToLower())
                {
                    case "region":
                        return EMRegion.CreateRegion(origin, doc, parent, data, markdownMatch, markdownMatch.Parameters);
                    case "include":
                        return EMInclude.CreateFromText(markdownMatch.Text, doc, origin, parent, data);
                    case "include_files":
                        return EMInclude.CreateIncludeFilesFromText(markdownMatch.Text, doc, origin, parent, data);
                    case "relative":
                        return EMRelativeLink.CreateFromText(markdownMatch.Parameters, doc, origin, parent, data);
                    case "relative_img":
                        return EMRelativeLink.CreateFromText(markdownMatch.Parameters, doc, origin, parent, data, true);
                    case "object":
                        return EMObject.CreateFromText(markdownMatch.Parameters, markdownMatch, doc, origin, parent, data);
                    default:
                        return EMErrorElement.Create(doc, origin, parent, data, "UnsupportedTaggedMarkdownElement");
                }
            }

            return EMErrorElement.Create(doc, origin, parent, data, "UnknownMatchTypeForTaggedElementsParser");
        }
    }
}
