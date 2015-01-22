// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    public class EMErrorElement : EMElement
    {
        private EMErrorElement(EMDocument doc, EMElementOrigin origin, EMElement parent, EMReadingMessage error, TransformationData data)
            : base(doc, origin, parent)
        {
            AddMessage(error, data);
        }

        public EMErrorElement(EMDocument doc, EMElementOrigin origin, EMElement parent, int errorId)
            : base(doc, origin, parent)
        {
            ErrorId = errorId;
        }

        public static EMErrorElement Create(
            EMDocument doc,
            EMElementOrigin origin,
            EMElement parent,
            TransformationData data,
            string messageId,
            params string[] messageArgs)
        {
            return new EMErrorElement(doc, origin, parent, new EMReadingMessage(MessageClass.Error, messageId, messageArgs), data);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.ErrorHighlight.Render(Hash.FromAnonymousObject(new { errorText = Origin.Text, errorId = ErrorId })));
        }
    }
}
