// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;

    public class EMElementOrigin : ITextFragment
    {
        public EMElementOrigin(int originalTextStart, string originalText)
        {
            Start = originalTextStart;
            Text = originalText;
        }

        public int Start { get; private set; }
        public int Length { get { return Text.Length; } }
        public int End { get { return Start + Length - 1; } }

        public string Text { get; private set; }

        public static implicit operator EMElementOrigin(Match match)
        {
            return new EMElementOrigin(match.Index, match.Value);
        }

        public int GetNonWhitespaceStart()
        {
            for (var i = 0; i < Text.Length; ++i)
            {
                if (Text[i] != ' ' && Text[i] != '\t' && Text[i] != '\n' && Text[i] != '\r')
                {
                    return i;
                }
            }

            throw new UnableToDetectOriginalPositionException();
        }

        public int GetNonWhitespaceEnd()
        {
            for (var i = Text.Length - 1; i >= 0; --i)
            {
                if (Text[i] != ' ' && Text[i] != '\t' && Text[i] != '\n' && Text[i] != '\r')
                {
                    return i;
                }
            }

            throw new UnableToDetectOriginalPositionException();
        }
    }
}
