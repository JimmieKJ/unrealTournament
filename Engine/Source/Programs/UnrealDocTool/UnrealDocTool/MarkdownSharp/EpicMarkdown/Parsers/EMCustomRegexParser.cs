// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Text.RegularExpressions;

    public class EMCustomRegexParser<TMatch> : IParser where TMatch : IMatch
    {
        private readonly Regex pattern;
        private readonly Func<TMatch, EMDocument, EMElementOrigin, EMElement, TransformationData, EMElement> elementCreator;
        private readonly Func<Match, TMatch> matchCreator;

        public EMCustomRegexParser(Regex pattern, Func<TMatch, EMDocument, EMElementOrigin, EMElement, TransformationData, EMElement> elementCreator, Func<Match, TMatch> matchCreator)
        {
            this.pattern = pattern;
            this.elementCreator = elementCreator;
            this.matchCreator = matchCreator;
        }

        public IEnumerable<IMatch> TextMatches(string text)
        {
            var matches = new List<IMatch>();

            foreach (Match m in pattern.Matches(text))
            {
                var match = matchCreator(m);

                if (match == null)
                {
                    continue;
                }

                matches.Add(match);
            }

            return matches;
        }

        public EMElement Create(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            return elementCreator((TMatch) match, doc, origin, parent, data);
        }
    }
}
