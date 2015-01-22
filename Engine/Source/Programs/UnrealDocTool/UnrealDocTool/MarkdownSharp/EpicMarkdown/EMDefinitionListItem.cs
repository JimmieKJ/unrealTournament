// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown
{
    using System;
    using System.Collections.Generic;
    using System.Text;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMDefinitionListItem : EMElement
    {
        public EMSpanElements Declaration { get; private set; }
        public EMElements Elements { get; private set; }

        private EMDefinitionListItem(EMDocument doc, EMElementOrigin origin, EMElement parent)
            : base(doc, origin, parent)
        {

        }

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(Pattern, CreateItem);
        }

        private static EMElement CreateItem(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var item = new EMDefinitionListItem(doc, origin, parent);

            item.Parse(doc, match, data);

            return item;
        }

        public void Parse(EMDocument doc, Match match, TransformationData data)
        {
            var itemContent = match.Groups["listItemContent"];

            var lineEnding = itemContent.Value.IndexOf('\n');
            var definitionSeparator = itemContent.Value.LastIndexOf(':', lineEnding);

            var dt = itemContent.Value.Substring(0, definitionSeparator > 0 ? definitionSeparator : itemContent.Length);

            Declaration = new EMSpanElements(doc, new EMElementOrigin(itemContent.Index - Origin.Start, dt), this);
            Declaration.Parse(dt, data);

            if (definitionSeparator > 0)
            {
                var dd = itemContent.Value.Substring(definitionSeparator + 1);

                Elements = new EMElements(doc, new EMElementOrigin(itemContent.Index + definitionSeparator + 1 - Origin.Start, dd), this);
                Elements.Parse(0, Markdown.Outdent(dd, Elements.TextMap), data);
            }
            else
            {
                Elements = null;
            }
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            Declaration.ForEachWithContext(context);

            if (Elements != null)
            {
                Elements.ForEachWithContext(context);
            }

            context.PostChildrenVisit(this);
        }

        private readonly static Regex Pattern = new Regex(@"\$[ ]+(?<listItemContent>[^\$]+)",
                RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline);

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(
                Templates.ListDefinitionItem.Render(
                    Hash.FromAnonymousObject(
                        new
                            {
                                declaration = Declaration.GetInnerHTML(includesStack, data),
                                content = Elements != null ? Elements.GetInnerHTML(includesStack, data) : ""
                            })));
        }
    }
}