// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;

    public class EMCodeBlockParserMatch : IMatch
    {
        public EMCodeBlockParserMatch(int index, string text, string content)
        {
            Index = index;
            Text = text;
            Content = content;
        }

        public int Index { get; private set; }

        public string Text { get; private set; }

        public string Content { get; private set; }
    }

    public class EMCodeBlockParser : IParser
    {
        private static readonly Regex CodeBlockBreaks = new Regex(@"^[ ]{0,3}[^\s][^\n]*\n?$|(^\s*\n){2,}|\A|\z", RegexOptions.Compiled | RegexOptions.Multiline);

        public IEnumerable<IMatch> TextMatches(string text)
        {
            var codeBlockMatches = new List<EMCodeBlockParserMatch>();

            var breaks = CodeBlockBreaks.Matches(text).Cast<Match>().ToArray();

            var lastBr = breaks[0];

            for (var i = 1; i < breaks.Length; ++i)
            {
                var br = breaks[i];
                var start = lastBr.Index + lastBr.Length;
                var length = br.Index - start;

                lastBr = br;

                var potentialCodeBlock = text.Substring(start, length);

                if (string.IsNullOrWhiteSpace(potentialCodeBlock))
                {
                    continue;
                }

                codeBlockMatches.Add(CreateMatch(start, potentialCodeBlock));
            }

            return codeBlockMatches;
        }

        public EMElement Create(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return new EMCodeBlock(doc, origin, parent, (match as EMCodeBlockParserMatch).Content);
        }

        private static EMCodeBlockParserMatch CreateMatch(int index, string text)
        {
            var codeBlock = Markdown.Outdent(Normalizer.TrimNewLines(text));
            codeBlock = Markdown.EncodeCode(codeBlock);
            codeBlock = Normalizer.TrimNewLines(codeBlock);

            return new EMCodeBlockParserMatch(index, text, Markdown.Unescape(codeBlock));
        }
    }

    public class EMFencedCodeBlockParser : EMCustomRegexParser<EMCodeBlockParserMatch>
    {
        private static readonly Regex FencedPattern = new Regex(@"
                    ^([ ]*)~{3,}\n                #Start of fenced code block spaces followed by at least three ~
                    ((\s|\S)*?(?=^[ ]*~{3,}))   #Match all text until we reach the end of the fenced code block
                    ^[ ]*~{3,}                  #End of fenced code block spaces followed by at least three ~
        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static EMCodeBlockParserMatch CreateFencedMatch(Match match)
        {
            var outdentSpaces = match.Groups[1].Value;
            var codeBlock = match.Groups[2].Value;

            if (string.IsNullOrWhiteSpace(codeBlock))
            {
                return null;
            }

            codeBlock = Regex.Replace(codeBlock, "^" + outdentSpaces, "", RegexOptions.Multiline);
            codeBlock = Markdown.EncodeCode(codeBlock);
            codeBlock = Normalizer.TrimNewLines(codeBlock);

            return new EMCodeBlockParserMatch(match.Index, Normalizer.TrailingWhitespaceRemove(match.Value), Markdown.Unescape(codeBlock));
        }

        private static EMElement CreateCodeBlock(EMCodeBlockParserMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return new EMCodeBlock(doc, origin, parent, match.Content);
        }

        public EMFencedCodeBlockParser()
            : base(FencedPattern, CreateCodeBlock, CreateFencedMatch)
        {

        }
    }

    public class EMCodeBlock : EMElement
    {
        public override string GetClassificationString()
        {
            return "markdown.block";
        }

        public EMCodeBlock(EMDocument doc, EMElementOrigin origin, EMElement parent, string rawText, bool singleLine = false)
            : base(doc, origin, parent)
        {
            this.rawText = rawText;
            this.singleLine = singleLine;
        }

        public static List<IParser> GetParser()
        {
            return new List<IParser> { new EMCodeBlockParser(), new EMFencedCodeBlockParser() };
        }

        private readonly string rawText;

        private readonly bool singleLine;

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            if (singleLine)
            {
                builder.Append(
                    Templates.CustomTag.Render(Hash.FromAnonymousObject(new { tagName = "code", content = Markdown.Unescape(rawText) })).Trim());
            }
            else
            {
                builder.Append(
                    Templates.PrettyPrintCodeBlock.Render(Hash.FromAnonymousObject(new { content = rawText })));
            }
        }
    }
}
