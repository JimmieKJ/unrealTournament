// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;

    public class EMRegion : EMElementWithElements
    {
        private readonly bool singleLine;

        public EMRegion(EMDocument doc, EMElementOrigin origin, EMElement parent, EMMarkdownTaggedElementMatch match, string regionParam, bool singleLine = false)
            : base(doc, origin, parent, match.ContentStart, match.ContentLength)
        {
            this.singleLine = singleLine;
            RegionParam = regionParam;
        }

        public string RegionParam { get; private set; }

        public static EMElement CreateRegion(EMElementOrigin origin, EMDocument doc, EMElement parent, TransformationData data, EMMarkdownTaggedElementMatch match, string regionParameters)
        {
            var content = new EMRegion(doc, origin, parent, match, regionParameters);

            content.Elements.Parse(0, Markdown.OutdentIfPossible(match.Content, content.Elements.TextMap), data);

            return content;
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(
                Templates.Region.Render(
                    Hash.FromAnonymousObject(
                        new { regionParameters = RegionParam, regionContent = Elements.GetInnerHTML(includesStack, data), singleLine })));
        }
    }
}
