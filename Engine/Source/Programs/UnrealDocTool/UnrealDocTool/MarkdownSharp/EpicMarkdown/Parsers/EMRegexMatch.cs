// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Text.RegularExpressions;

    public class EMRegexMatch : IMatch
    {
        public EMRegexMatch(Match match)
        {
            Match = match;
        }

        public Match Match { get; private set; }
        public int Index { get { return Match.Index; } }
        public string Text { get { return Match.Value; } }
    }
}
