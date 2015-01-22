// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace MarkdownSharp.EpicMarkdown
{
    using MarkdownSharp.EpicMarkdown.Parsers;

    public sealed class EMElements : EMContentElement
    {
        public EMElements(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {

        }

        public override void Parse(List<TextFragment> fragments, TransformationData data)
        {
            ParseElements(data, fragments, new EMMarkdownAndHTMLTagsParser(GetLocation));
            ParseElements(data, fragments, EMHeader.GetParser());
            ParseElements(data, fragments, EMInclude.GetParser());
            ParseElements(data, fragments, EMTOCInline.GetParser());
            ParseElements(data, fragments, EMBookmark.GetParser());
            ParseElements(data, fragments, EMHorizontalRule.GetParser());
            ParseElements(data, fragments, EMList.GetParser());
            ParseElements(data, fragments, EMTable.GetParser());
            ParseElements(data, fragments, EMCodeBlock.GetParser());
            ParseElements(data, fragments, EMBlockQuotes.GetParser());
            ParseElements(data, fragments, EMParagraph.GetParser());
        }
    }
}
