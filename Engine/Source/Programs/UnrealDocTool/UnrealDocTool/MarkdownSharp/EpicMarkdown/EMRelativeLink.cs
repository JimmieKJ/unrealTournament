// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.PathProviders;
    using MarkdownSharp.Preprocessor;

    public class EMRelativeLink : EMElement
    {
        private readonly EMPathProvider path;

        private EMRelativeLink(EMDocument doc, EMElementOrigin origin, EMElement parent, EMPathProvider path)
            : base(doc, origin, parent)
        {
            this.path = path;
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            try
            {
                builder.Append(path.GetPath(data).Replace("\\", "/"));
            }
            catch (EMPathVerificationException e)
            {
                e.AddToErrorListAndAppend(builder, data, Origin.Text);
            }
        }

        public static EMElement CreateFromText(string text, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data, bool imageLink = false)
        {
            try
            {
                var path = EMPathProvider.CreatePath(text, doc, data);

                return new EMRelativeLink(doc, origin, parent, path);
            }
            catch (EMPathVerificationException e)
            {
                return new EMErrorElement(doc, origin, parent, e.AddToErrorList(data, origin.Text));
            }
        }
    }
}
