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

    public class EMListItem : EMElementWithElements
    {
        private EMListItem(EMDocument doc, EMElementOrigin origin, EMElement parent, EMListType type)
            : base(doc, origin, parent, 0, origin.Length)
        {
        }

        public static EMRegexParser GetOrderedParser()
        {
            return new EMRegexParser(OrderedPattern, (match, doc, origin, parent, data) => CreateItem(EMListType.Ordered, doc, match, origin, parent, data));
        }

        public static EMRegexParser GetUnorderedParser()
        {
            return new EMRegexParser(UnorderedPattern, (match, doc, origin, parent, data) => CreateItem(EMListType.Unordered, doc, match, origin, parent, data));
        }

        private static EMElement CreateItem(EMListType type, EMDocument doc, Match match, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var itemGroup = match.Groups[4];

            var item = new EMListItem(doc, origin, parent, type);

            // adding offset to text map
            item.Elements.TextMap.ApplyChanges(new List<PreprocessingTextChange>() { new PreprocessingTextChange(0, itemGroup.Index - match.Index, 0) });

            item.Elements.Parse(0, Markdown.Outdent(itemGroup.Value, item.Elements.TextMap), data);

            return item;
        }

        private const string PatternTemplate = 
              @"(\n)?                      # leading line = $1
                (^[ ]*)                    # leading whitespace = $2
                ({0}) [ ]+                 # list marker = $3
                ((?s:.*?)                  # list item text = $4
                (\n{{1,}}))      
                (?= \n* (\z | \2 ({0}) [ ]+))";

        private static readonly Regex UnorderedPattern = new Regex(
            string.Format(PatternTemplate, Markdown._markerUL),
            RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline);

        private static readonly Regex OrderedPattern = new Regex(
            string.Format(PatternTemplate, Markdown._markerOL),
            RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline);

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.ListItem.Render(Hash.FromAnonymousObject(new { content = Elements.GetInnerHTML(includesStack, data) })));
        }
    }
}