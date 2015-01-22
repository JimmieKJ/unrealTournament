// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode
{
    static class ClassificationTypes
    {
        // Base definition for all other classification types

        [Export]
        [Name("markdown")]
        internal static ClassificationTypeDefinition MarkdownClassificationDefinition = null;

        // Bold/italics

        [Export]
        [Name("markdown.italics")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownItalicsDefinition = null;

        [Export]
        [Name("markdown.bold")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownBoldDefinition = null;

        [Export]
        [Name("markdown.bolditalics")]
        [BaseDefinition("markdown.bold")]
        [BaseDefinition("markdown.italics")]
        internal static ClassificationTypeDefinition MarkdownBoldItalicsDefinition = null;

        // Headers

        [Export]
        [Name("markdown.header")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownHeaderDefinition = null;

        [Export]
        [Name("markdown.header.h1")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH1Definition = null;

        [Export]
        [Name("markdown.header.h2")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH2Definition = null;

        [Export]
        [Name("markdown.header.h3")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH3Definition = null;

        [Export]
        [Name("markdown.header.h4")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH4Definition = null;

        [Export]
        [Name("markdown.header.h5")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH5Definition = null;

        [Export]
        [Name("markdown.header.h6")]
        [BaseDefinition("markdown.header")]
        internal static ClassificationTypeDefinition MarkdownH6Definition = null;

        // Lists

        [Export]
        [Name("markdown.list")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownListDefinition = null;

        [Export]
        [Name("markdown.list.unordered")]
        [BaseDefinition("markdown.list")]
        internal static ClassificationTypeDefinition MarkdownUnorderedListDefinition = null;

        [Export]
        [Name("markdown.list.ordered")]
        [BaseDefinition("markdown.list")]
        internal static ClassificationTypeDefinition MarkdownOrderedListDefinition = null;

        // Code/pre

        [Export]
        [Name("markdown.pre")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownPreDefinition = null;

        [Export]
        [Name("markdown.block")]
        [BaseDefinition("markdown.pre")]
        internal static ClassificationTypeDefinition MarkdownCodeDefinition = null;

        // Quotes

        [Export]
        [Name("markdown.blockquote")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdowBlockquoteDefinition = null;

        // Links

        [Export]
        [Name("markdown.label")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownLabel = null;

        [Export]
        [Name("markdown.link")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownLinkDefinition = null;

        // Link URLs

        [Export]
        [Name("markdown.url")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownLinkUrlDefinition = null;

        [Export]
        [Name("markdown.url.inline")]
        [BaseDefinition("markdown.url")]
        internal static ClassificationTypeDefinition MarkdownInlineLinkDefinition = null;

        [Export]
        [Name("markdown.url.definition")]
        [BaseDefinition("markdown.url")]
        internal static ClassificationTypeDefinition MarkdownDefinitionLinkDefinition = null;

        // Images

        [Export]
        [Name("markdown.image")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownImageDefinition = null;

        // Miscellaneous

        [Export]
        [Name("markdown.horizontalrule")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition MarkdownHorizontalRuleDefinition = null;

        // Epic Types

        [Export]
        [Name("markdown.metadata")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition Metadata = null;

        [Export]
        [Name("markdown.tableofcontents")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition TableOfContents = null;

        [Export]
        [Name("markdown.tableofdata")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition TableOfData = null;

        [Export]
        [Name("markdown.list.definition")]
        [BaseDefinition("markdown.list")]
        internal static ClassificationTypeDefinition DefinitonLists = null;

        [Export]
        [Name("markdown.fencedcodeblock")]
        [BaseDefinition("markdown.pre")]
        internal static ClassificationTypeDefinition FencedCode = null;

        [Export]
        [Name("markdown.htmlcomment")]
        [BaseDefinition("markdown")]
        internal static ClassificationTypeDefinition HTMLComment = null;


    }
}
