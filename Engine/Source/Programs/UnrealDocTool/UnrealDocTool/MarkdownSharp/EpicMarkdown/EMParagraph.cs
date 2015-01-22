// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMParagraph : EMElement
    {
        private readonly EMSpanElements content;

        private EMParagraph(EMDocument doc, EMElementOrigin origin, EMElement parent, EMSpanElements content)
            : base(doc, origin, parent)
        {
            this.content = content;
        }

        private static readonly Regex Pattern = new Regex(@"(?<=(\s*\n){2}|^(\s*\n)?)(?<content>(.*[\w`].*(\n|$))+)(?=(\s*\n)|$)", RegexOptions.Compiled);

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(Pattern, CreateParagraph);
        }

        private static EMElement CreateParagraph(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var contentGroup = match.Groups["content"];
            var spanContent = new EMSpanElements(doc, new EMElementOrigin(origin.Start, contentGroup.Value), parent);
            spanContent.Parse(contentGroup.Value, data);

            return new EMParagraph(doc, origin, parent, spanContent);
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            content.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.Paragraph.Render(Hash.FromAnonymousObject(new { content = content.GetInnerHTML(includesStack, data) })));
        }
    }
}
