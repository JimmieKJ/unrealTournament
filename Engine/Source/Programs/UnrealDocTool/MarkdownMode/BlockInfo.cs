// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Text;

namespace MarkdownMode
{
    using MarkdownSharp.EpicMarkdown;

    public class BlockInfo
    {
        public BlockInfo(EMElement blockElement, Span span)
        {
            BlockElement = blockElement;
            Span = span;
        }

        public EMElement BlockElement { get; private set; }
        public Span Span { get; private set; }
    }

}
