// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    public class EMReadingMessage
    {
        public EMReadingMessage(MessageClass messageClass, string messageId, params string[] messageArgs)
        {
            this.messageId = messageId;
            this.messageArgs = messageArgs;

            MessageClass = messageClass;
        }

        public int Id { get; private set; }
        public MessageClass MessageClass { get; private set; }
        public string Message { get { return Language.Message(messageId, messageArgs); } }

        private readonly string messageId;
        private readonly string[] messageArgs;
    }
}
