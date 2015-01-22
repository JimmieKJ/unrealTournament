// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace MarkdownSharp.EpicMarkdown
{
    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;

    public class EMObjectParam : EMElementWithElements
    {
        public EMObjectParam(EMDocument doc, EMElementOrigin origin, EMElement parent, EMMarkdownTaggedElementMatch match, string paramName)
            : base(doc, origin, parent, match.ContentStart, match.ContentLength)
        {
            ParamName = paramName;
        }

        public string ParamName { get; private set; }

        public static EMObjectParam CreateParam(EMElementOrigin origin, EMDocument doc, EMElement parent, TransformationData data, EMMarkdownTaggedElementMatch match, string paramName, string paramIndentation)
        {
            var content = new EMObjectParam(doc, origin, parent, match, paramName);

            var text =
                Preprocessor.Replace(
                    new Regex(
                        "^" + Regex.Escape(paramIndentation), RegexOptions.Multiline),
                    content.Elements.Origin.Text,
                    "",
                    content.Elements.TextMap);

            text = Preprocessor.Trim(text, '\n', content.Elements.TextMap);

            text = Markdown.OutdentIfPossible(text, content.Elements.TextMap);

            content.Elements.Parse(0, text, data);

            return content;
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Elements.GetInnerHTML(includesStack, data));
        }
    }
}