// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using MarkdownSharp.EpicMarkdown.Parsers;
using MarkdownSharp.EpicMarkdown.Parsers.SpanParser;

namespace MarkdownSharp.EpicMarkdown
{
    using System;

    public class EMSpanElements : EMContentElement
    {
        public EMSpanElements(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {
            
        }

        public override void Parse(List<TextFragment> fragments, TransformationData data)
        {
            ParseElements(data, fragments, new EMMarkdownAndHTMLTagsParser(GetLocation, true));
            ParseElements(data, fragments, new EMSpanParser(GetLocation));
            
            foreach (var fragment in fragments)
            {
                var formattedText = new EMFormattedText(Document, new EMElementOrigin(fragment.Start, fragment.Text), this);

                formattedText.Parse(fragment.Text, data);

                Add(formattedText);
            }
        }
    }
}
