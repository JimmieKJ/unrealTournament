// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMHeader : EMBookmark
    {
        public EMHeader(EMDocument doc, EMElementOrigin origin, EMElement parent, int level, string text, bool isOptional = false)
            : base(doc, origin, parent, ToName(text))
        {
            Text = text;
            Level = level;
            IsOptional = isOptional;
        }

        private static readonly Regex WhitespacePattern = new Regex(@"\s+", RegexOptions.Compiled);

        private static string ToName(string text)
        {
            return WhitespacePattern.Replace(text, "");
        }

        private static readonly Regex HeaderSetext = new Regex(@"
                ^(!{2}[ ]*)? # $1 Optional header starts with two ! marks and should not be in TOC. 
                ([\S ]+?)        # $2 The Header
                [ ]*
                \r?\n
                (=+|-+)      # $3 = string of ='s or -'s
                [ ]*
                (?=\r|\n)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static readonly Regex HeaderAtx = new Regex(@"
                ^(\#{1,6})       # $1 = string of #'s
                (!{2}[ ]*)?      # $2 Optional header starts with two ! marks and should not be in TOC.
                [ ]*
                (.+?)           # $3 = Header text
                [ ]*
                \#*             # optional closing #'s (not counted)
                (?=\r|\n)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static new List<EMRegexParser> GetParser()
        {
            return new List<EMRegexParser>
                       {
                           new EMRegexParser(HeaderAtx, CreateFromAtxMatch),
                           new EMRegexParser(HeaderSetext, CreateFromSetextMatch)
                       };
        }

        private static EMHeader CreateFromAtxMatch(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var headerText = match.Groups[3].Value;

            if (headerText.Contains("ifndef") || headerText.Contains("endif"))
            {
                return null;
            }

            var level = match.Groups[1].Value.Length;
            var isOptional = string.IsNullOrWhiteSpace(match.Groups[2].Value);

            headerText = Markdown.Unescape(data.Markdown.RunSpanGamut(headerText, data));

            return new EMHeader(doc, origin, parent, level, headerText, isOptional);
        }

        private static EMHeader CreateFromSetextMatch(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var headerText = match.Groups[2].Value;
            var level = match.Groups[3].Value.StartsWith("=") ? 1 : 2;
            var isOptional = string.IsNullOrWhiteSpace(match.Groups[1].Value);

            headerText = Markdown.Unescape(data.Markdown.RunSpanGamut(headerText, data));

            return new EMHeader(doc, origin, parent, level, headerText, isOptional);
        }

        public int GetLevelWithOffsets(Stack<EMInclude> includesStack)
        {
            return includesStack.Sum(include => include.HeaderOffset) + Level;
        }

        public string Text { get; private set; }
        public int Level { get; private set; }
        public bool IsOptional { get; private set; }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(
                Templates.Header.Render(
                    Hash.FromAnonymousObject(new { level = GetLevelWithOffsets(includesStack), uniqueKey = UniqueKey, text = Text })));
        }

        public override string GetClassificationString()
        {
            return string.Format("markdown.header.h{0}", Math.Min(Level, 6));
        }
    }
}
