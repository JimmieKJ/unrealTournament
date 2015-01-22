// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System;
    using System.Text.RegularExpressions;

    public class EMRegexParser : EMCustomRegexParser<EMRegexMatch>
    {
        public EMRegexParser(Regex pattern, Func<Match, EMDocument, EMElementOrigin, EMElement, TransformationData, EMElement> creatorDelegate, Func<Match, EMRegexMatch> parserMatchCreator = null)
            : base(pattern, (m, doc, o, p, d) => creatorDelegate(m.Match, doc, o, p, d), parserMatchCreator ?? StandardParserMatchCreator)
        {
        }

        private static EMRegexMatch StandardParserMatchCreator(Match m)
        {
            return new EMRegexMatch(m);
        }
    }
}
