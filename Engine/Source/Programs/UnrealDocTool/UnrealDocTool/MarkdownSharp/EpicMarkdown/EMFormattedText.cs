// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Globalization;
    using System.IO;

    public class EMFormattedText : EMElement
    {
        public EMFormattedText(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {
            
        }

        public string Content { get; private set; }

        public void Parse(string content, TransformationData data)
        {
            Content = Markdown.Unescape(data.Markdown.RunSpanGamut(content, data));
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Content);
        }
    }
}
