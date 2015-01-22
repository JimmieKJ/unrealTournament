// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.Serialization;

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.Text;

    using DotLiquid;

    [Serializable]
    public class EMPathVerificationException : Exception
    {
        public EMPathVerificationException()
            : this(null)
        {
        }

        public EMPathVerificationException(string message)
            : base(message)
        {
        }

        protected EMPathVerificationException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }

        public int AddToErrorList(TransformationData data, string originalText)
        {
            var errorId = data.ErrorList.Count;
            data.ErrorList.Add(Markdown.GenerateError(Message, MessageClass.Error, originalText, errorId, data));

            return errorId;
        }

        public void AddToErrorListAndAppend(StringBuilder builder, TransformationData data, string originalText)
        {
            var errorId = AddToErrorList(data, originalText);

            if (data.Markdown.ThisIsPreview)
            {
                builder.Append(
                    Templates.ErrorHighlight.Render(
                        Hash.FromAnonymousObject(new { errorText = originalText, errorId })));
            }
        }
    }
}
