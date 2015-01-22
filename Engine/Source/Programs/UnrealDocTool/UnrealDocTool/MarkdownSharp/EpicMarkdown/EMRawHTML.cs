// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text;
using System;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    public class EMRawHTML : EMElementWithElements
    {
        private readonly string name;
        private readonly string attributesString;

        public EMRawHTML(EMDocument doc, EMElementOrigin origin, EMElement parent, string name, string attributesString)
            : base(doc, origin, parent, 0, origin.Length)
        {
            this.name = name;
            this.attributesString = attributesString;
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.CustomTag.Render(Hash.FromAnonymousObject(
                new
                    {
                        tagName = name,
                        attributesList = (String.IsNullOrEmpty(attributesString)? attributesString : " " + attributesString),
                        content = Elements.GetInnerHTML(includesStack, data)
                    })));
        }
    }
}
