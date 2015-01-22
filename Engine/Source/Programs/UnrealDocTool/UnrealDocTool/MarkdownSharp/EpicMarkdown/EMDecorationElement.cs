// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    public enum EMDecorationType
    {
        Bold,
        Italic
    }

    public class EMDecorationElement : EMElement
    {
        private readonly EMDecorationType type;

        public EMSpanElements Content { get; private set; }

        public EMDecorationElement(EMDocument doc, EMElementOrigin origin, EMElement parent, EMDecorationType type)
            : base(doc, origin, parent)
        {
            this.type = type;
            Content = new EMSpanElements(doc, new EMElementOrigin(0, origin.Text), this);
        }

        public override string GetClassificationString()
        {
            switch (type)
            {
                case EMDecorationType.Bold:
                    return "markdown.bold";
                case EMDecorationType.Italic:
                    return "markdown.italics";
                default:
                    throw new NotSupportedException("Unsupported decoration type.");
            }
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

 	        Content.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(GetTemplate().Render(Hash.FromAnonymousObject(new { value = Content.GetInnerHTML(includesStack, data) })));
        }

        private TemplateFile GetTemplate()
        {
            switch (type)
            {
                case EMDecorationType.Bold:
                    return Templates.Strong;
                case EMDecorationType.Italic:
                    return Templates.Emphasis;
                default:
                    throw new NotSupportedException("Invalid decoration type " + type);
            }
        }
    }
}
