// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown
{
    using System;
    using System.Collections.Generic;
    using System.Text;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.Preprocessor;

    public enum EMListType
    {
        Definition,
        Ordered,
        Unordered,
        Undefined
    }

    public class EMListParserMatch : IMatch
    {
        public EMListParserMatch(int index, string text, EMListType type, string content)
        {
            Index = index;
            Text = text;
            Type = type;
            Content = content;
        }

        public int Index { get; private set; }

        public string Text { get; private set; }

        public EMListType Type { get; private set; }

        public string Content { get; private set; }
    }

    public class EMListParser : EMCustomRegexParser<EMListParserMatch>
    {
        private const string UnorderedListHeaderPattern = @"[ ]{0,3}[-+*][ ]+";
        private const string OrderedListHeaderPattern = @"[ ]{0,3}\d+[.][ ]+";
        private const string DefinitionListHeaderPattern = @"[ ]{0,3}[$][ ]+";
        private const string ListPatternTemplate = @"
                (?<{0}>{1}).+(\n|$)
                ([ \t]*\n(?![ \t]*\n)|([ ]{{4,}}|({1})).+(\n|$)|(?<=.+\n).+(\n|$))*";

        private static readonly Regex ListPattern = new Regex(string.Format(@"
                (?<=\n|^)
                (?<content>
                    (
                        {0}
                    )
                    |
                    (
                        {1}
                    )
                    |
                    (
                        {2}
                    )
                )",
            string.Format(ListPatternTemplate, "unorderedListHeader", UnorderedListHeaderPattern),
            string.Format(ListPatternTemplate, "orderedListHeader", OrderedListHeaderPattern),
            string.Format(ListPatternTemplate, "definitionListHeader", DefinitionListHeaderPattern)
            ), RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        public EMListParser()
            : base(ListPattern, EMList.CreateList, ListParserMatchCreator)
        {
            
        }

        private static EMListParserMatch ListParserMatchCreator(Match m)
        {
            var listType = m.Groups["unorderedListHeader"].Success
                               ? EMListType.Unordered
                               : (m.Groups["orderedListHeader"].Success
                                      ? EMListType.Ordered
                                      : (m.Groups["definitionListHeader"].Success
                                             ? EMListType.Definition
                                             : EMListType.Undefined));

            if (listType == EMListType.Undefined)
            {
                // this should never happen!
                throw new InvalidOperationException("Bad list definition.");
            }

            return new EMListParserMatch(
                m.Index,
                Normalizer.TrailingWhitespaceRemove(m.Value),
                listType,
                Normalizer.TrailingWhitespaceRemove(m.Groups["content"].Value));
        }
    }

    public class EMListElements : EMContentElement
    {
        private readonly IParser parser;

        public EMListElements(EMDocument doc, EMElementOrigin origin, EMElement parent, EMListType type)
            : base(doc, origin, parent)
        {
            parser = GetParserByType(type);
        }

        public override void Parse(List<TextFragment> fragments, TransformationData data)
        {
            ParseElements(data, fragments, parser);
        }

        private static IParser GetParserByType(EMListType type)
        {
            switch (type)
            {
                case EMListType.Ordered:
                    return EMListItem.GetOrderedParser();
                case EMListType.Unordered:
                    return EMListItem.GetUnorderedParser();
                case EMListType.Definition:
                    return EMDefinitionListItem.GetParser();
                default:
                    throw new NotSupportedException("List type not supported.");
            }
        }
    }

    public class EMList : EMElement
    {
        private readonly EMListType type;
        private EMListElements content;

        protected EMList(EMDocument doc, EMElementOrigin origin, EMElement parent, EMListType type)
            : base(doc, origin, parent)
        {
            this.type = type;
        }

        public override string GetClassificationString()
        {
            switch (type)
            {
                case EMListType.Ordered:
                    return "markdown.list.ordered";
                case EMListType.Definition:
                    return "markdown.list.definition";
                case EMListType.Unordered:
                    return "markdown.list.unordered";
                default:
                    throw new NotSupportedException("List type not supported.");
            }
        }

        public static EMListParser GetParser()
        {
            return new EMListParser();
        }

        public static EMElement CreateList(EMListParserMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var list = new EMList(doc, origin, parent, match.Type);

            list.Parse(match.Text, data);

            return list;
        }

        public void Parse(string text, TransformationData data)
        {
            content = new EMListElements(Document, new EMElementOrigin(0, text), this, type);

            content.Parse(text + "\n", data);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            var tagName = type == EMListType.Ordered
                              ? "ol"
                              : (type == EMListType.Unordered ? "ul" : (type == EMListType.Definition ? "dl" : "error"));

            builder.Append(Templates.CustomTag.Render(Hash.FromAnonymousObject(new { tagName, content = GetInnerHTML(includesStack, data) })));
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            content.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }

        public string GetInnerHTML(Stack<EMInclude> includesStack, TransformationData data)
        {
            var builder = new StringBuilder();

            content.AppendHTML(builder, includesStack, data);

            return builder.ToString();
        }
    }
}