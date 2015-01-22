// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MarkdownSharp.Preprocessor
{
    public enum PreprocessedTextType
    {
        Metadata,
        FilteredOut,
        Comment,
        Reference
    }

    public class PreprocessedTextBound
    {
        public Interval Bound { get; private set; }
        public PreprocessedTextType Type { get; private set; }

        public PreprocessedTextBound(PreprocessedTextType type, Interval bound)
        {
            Bound = bound;
            Type = type;
        }

        public PreprocessedTextBound(PreprocessedTextType type, int start, int end)
        {
            Interval bound;

            bound.Start = start;
            bound.End = end;

            Bound = bound;
            Type = type;
        }
    }
}
