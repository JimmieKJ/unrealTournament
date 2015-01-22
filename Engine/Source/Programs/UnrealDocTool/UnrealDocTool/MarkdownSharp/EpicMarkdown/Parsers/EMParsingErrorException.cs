// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Runtime.Serialization;

    [Serializable]
    public class EMParsingErrorException : Exception
    {
        public string Path { get; private set; }
        public int Line { get; private set; }
        public int Column { get; private set; }

        public EMParsingErrorException()
            : this(null, null, 0, 0)
        {
        }

        public EMParsingErrorException(string message, string path, int line, int column)
            : this(message, path, line, column, null)
        {
        }

        public EMParsingErrorException(string message, string path, int line, int column, Exception innerException)
            : base(message, innerException)
        {
            Path = path;
            Line = line;
            Column = column;
        }

        protected EMParsingErrorException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
            Path = info.GetString("Path");
            Line = info.GetInt32("Line");
            Column = info.GetInt32("Column");
        }

        public override void GetObjectData(SerializationInfo info, StreamingContext context)
        {
            base.GetObjectData(info, context);

            info.AddValue("Path", Path);
            info.AddValue("Line", Line);
            info.AddValue("Column", Column);
        }
    }
}
