// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers.SpanParser
{
    using System.Text.RegularExpressions;

    public abstract class EMBracketedStartOrEndTag : StartOrEndTagMatch
    {
        protected EMBracketedStartOrEndTag(Match match)
            : base(match.Index, match.Value)
        {

        }

        private static readonly Regex BeginPattern = new Regex(@"!?\[", RegexOptions.Compiled);
        private static readonly Regex EndPattern = new Regex(@"\](\s*(?<params>(\([^)]+\))+|\[\w+\]))?", RegexOptions.Compiled);

        private static void AppendTags(List<ITagMatch> tags, string text, Regex pattern, Func<Match, ITagMatch> creator)
        {
            tags.AddRange(from Match m in pattern.Matches(text) select creator(m));
        }

        public static void Append(List<ITagMatch> output, string text)
        {
            AppendTags(output, text, EndPattern, m => new EMBracketedEnd(m));
            AppendTags(output, text, BeginPattern, m => new EMBracketedStart(m));
        }
    }

    public class EMBracketedStart : EMBracketedStartOrEndTag, IStartTagMatch
    {
        public EMBracketedStart(Match match)
            : base(match)
        {
        }

        public bool HasEnding { get { return true; } }
    }

    public class EMBracketedEnd : EMBracketedStartOrEndTag, IEndTagMatch
    {
        public EMBracketedEnd(Match match)
            : base(match)
        {
            Params = match.Groups["params"].Value;
        }

        public string Params { get; private set; }
    }
}
