// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMHorizontalRule : EMElement
    {
        private EMHorizontalRule(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {
            
        }

        public override string GetClassificationString()
        {
            return "markdown.horizontalrule";
        }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(HorizontalRulesPattern, Create);
        }

        private static EMElement Create(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return new EMHorizontalRule(doc, origin, parent);
        }

        private static readonly Regex HorizontalRulesPattern = new Regex(@"
            ^[ ]{0,3}         # Leading space
                ([-*_])       # $1: First marker
                (           # Repeated marker group
                    [ ]{0,2}  # Zero, one, or two spaces.
                    \1        # Marker character
                ){2,}         # Group repeated at least twice
                [ ]*          # Trailing spaces
                $             # End of line.
            ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.HorizontalRule.Render());
        }
    }
}
