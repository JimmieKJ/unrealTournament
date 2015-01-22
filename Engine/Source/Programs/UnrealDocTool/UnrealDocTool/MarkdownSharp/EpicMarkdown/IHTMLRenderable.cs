// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    public interface IHTMLRenderable
    {
        void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data);
    }
}
