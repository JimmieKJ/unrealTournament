// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    public interface IMatch
    {
        int Index { get; }
        string Text { get; }
    }
}
