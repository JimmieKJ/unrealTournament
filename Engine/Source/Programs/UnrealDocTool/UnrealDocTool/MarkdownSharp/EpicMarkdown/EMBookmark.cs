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

    public class EMBookmark : EMElement
    {
        public string Name { get; private set; }
        public string UniqueKey { get; private set; }

        protected EMBookmark(EMDocument doc, EMElementOrigin origin, EMElement parent, string bookmark)
            : base(doc, origin, parent)
        {
            Name = bookmark;
            UniqueKey = MakeUniqueKey(Name.ToLower());
        }

        private static string MakeUniqueKey(string name)
        {
            name = name.Replace(" ", ""); // Replace spaces in bookmark references.
            name = name.Replace(".", "stop"); // Replace full stops in bookmark references to distinguish from linked files.
            name = Regex.Replace(name, @"<[^>]+>", ""); // Remove HTML tags
            name = name.Replace("\\", ""); // Remove backslashes from header.

            // remove parentheses
            name = name.Replace("(", "_");
            name = name.Replace(")", "_");

            return name;
        }

        private static readonly Regex Bookmark = new Regex(@"^\(\#(?<BookmarkName>[^\)]+)\)", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static EMRegexParser GetParser()
        {
            return new EMRegexParser(Bookmark, Create);
        }

        private static EMElement Create(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var bookmark = match.Groups["BookmarkName"].Value;

            return new EMBookmark(doc, origin, parent, bookmark);
        }

        private static readonly Regex NonWordCharsPattern = new Regex(@"[^\w]", RegexOptions.Compiled);

        public static string EscapeIdChars(string id)
        {
            return NonWordCharsPattern.Replace(id, "_");
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            builder.Append(Templates.Bookmark.Render(Hash.FromAnonymousObject(new { bookmark = UniqueKey })));
        }
    }
}
