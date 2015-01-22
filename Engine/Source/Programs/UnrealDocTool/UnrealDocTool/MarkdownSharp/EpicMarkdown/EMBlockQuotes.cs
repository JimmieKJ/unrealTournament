// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using DotLiquid;

namespace MarkdownSharp.EpicMarkdown
{
    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMBlockQuotes : EMElementWithElements
    {
        private EMBlockQuotes(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent, 0, origin.Length)
        {
        }

        public override string GetClassificationString()
        {
            return "markdown.blockquote";
        }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(BlockquotePattern, Create);
        }

        private static EMElement Create(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var bqTextGroup = match.Groups[1];
            var bqText = bqTextGroup.Value;

            bqText = Regex.Replace(bqText, @"^[ ]*>[ ]?", "", RegexOptions.Multiline);       // trim one level of quoting
            bqText = Regex.Replace(bqText, @"^[ ]+$", "", RegexOptions.Multiline);           // trim whitespace-only lines

            var bq = new EMBlockQuotes(doc, origin, parent);

            bq.Elements.Parse(origin.Start + bqTextGroup.Index, bqText, data);

            return bq;
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            var content = Elements.GetInnerHTML(includesStack, data);

            content = NewlinePattern.Replace(content, "  ");

            // These leading spaces screw with <pre> content, so we need to fix that:
            content = PreTagPattern.Replace(content, BlockQuotePreLeadingSpacesFix);

            builder.Append(Templates.Blockquote.Render(Hash.FromAnonymousObject(new { value = content })));
        }

        public override string ToString()
        {
            var text = base.ToString();

            text = NewlinePattern.Replace(text, "  ");

            // These leading spaces screw with <pre> content, so we need to fix that:
            text = PreTagPattern.Replace(text, BlockQuotePreLeadingSpacesFix);

            return Templates.Blockquote.Render(Hash.FromAnonymousObject(new { value = text }));            
        }

        private static string BlockQuotePreLeadingSpacesFix(Match match)
        {
            return LeadingSpacesPattern.Replace(match.Groups[1].Value, "");
        }

        private static readonly Regex LeadingSpacesPattern = new Regex(@"^  ", RegexOptions.Compiled | RegexOptions.Multiline);
        private static readonly Regex PreTagPattern = new Regex(@"(\s*<pre>.+?</pre>)", RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace | RegexOptions.Singleline);
        private static readonly Regex NewlinePattern = new Regex(@"^", RegexOptions.Compiled | RegexOptions.Multiline);
        private static readonly Regex BlockquotePattern = new Regex(@"
            (                           # Wrap whole match in $1
                (
                ^[ ]*>[ ]?              # '>' at the start of a line
                    .+\n?               # rest of the first line
                (.+\n)*                 # subsequent consecutive lines
                \n*                     # blanks
                )+
            )", RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);
    }
}
