// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;
using System.Windows.Media;
using MarkdownSharp;

namespace MarkdownMode
{
    static class ClassificationFormats
    {
        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.tableofdata")]
        [Name("markdown.tableofdata")]
        [UserVisible(true)]
        sealed class TableOfData : ClassificationFormatDefinition
        {
            public TableOfData()
            {
                this.ForegroundColor = Colors.Firebrick;

                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownTableOfData");
                    };
            }
        }

        // Bold/italics

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.italics")]
        [Name("markdown.italics")]
        sealed class MarkdownItalicsFormat : ClassificationFormatDefinition
        {
            public MarkdownItalicsFormat() { this.IsItalic = true; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.bold")]
        [Name("markdown.bold")]
        sealed class MarkdownBoldFormat : ClassificationFormatDefinition
        {
            public MarkdownBoldFormat() { this.IsBold = true; }
        }

        // Headers

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header")]
        [Name("markdown.header")]
        [UserVisible(true)]
        sealed class MarkdownHeaderFormat : ClassificationFormatDefinition
        {
            public MarkdownHeaderFormat()
            {
                this.ForegroundColor = Colors.DarkBlue;

                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownHeader");
                    };
            }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h1")]
        [Name("markdown.header.h1")]
        sealed class MarkdownH1Format : ClassificationFormatDefinition
        {
            public MarkdownH1Format() { this.FontRenderingSize = 22; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h2")]
        [Name("markdown.header.h2")]
        sealed class MarkdownH2Format : ClassificationFormatDefinition
        {
            public MarkdownH2Format() { this.FontRenderingSize = 20; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h3")]
        [Name("markdown.header.h3")]
        sealed class MarkdownH3Format : ClassificationFormatDefinition
        {
            public MarkdownH3Format() { this.FontRenderingSize = 18; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h4")]
        [Name("markdown.header.h4")]
        sealed class MarkdownH4Format : ClassificationFormatDefinition
        {
            public MarkdownH4Format() { this.FontRenderingSize = 16; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h5")]
        [Name("markdown.header.h5")]
        sealed class MarkdownH5Format : ClassificationFormatDefinition
        {
            public MarkdownH5Format() { this.FontRenderingSize = 14; }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.header.h6")]
        [Name("markdown.header.h6")]
        sealed class MarkdownH6Format : ClassificationFormatDefinition
        {
            public MarkdownH6Format() { this.FontRenderingSize = 12; }
        }

        // Lists

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.list")]
        [Name("markdown.list")]
        sealed class MarkdownListFormat : ClassificationFormatDefinition
        {
            public MarkdownListFormat() { this.IsBold = true; this.ForegroundColor = Colors.LimeGreen; }
        }

        // Code/pre

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.block")]
        [Name("markdown.block")]
        [UserVisible(true)]
        [Order(Before = Priority.Default, After = "markdown.blockquote")] // Low priority
        sealed class MarkdownCodeFormat : ClassificationFormatDefinition
        {
            public MarkdownCodeFormat()
            {
                this.ForegroundColor = Colors.Teal;
                this.FontTypeface = new Typeface("Courier New");
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownCodeBlock");
                    };
            }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.pre")]
        [Name("markdown.pre")]
        [Order(Before = Priority.Default, After = "markdown.blockquote")] // Low priority
        sealed class MarkdownPreFormat : ClassificationFormatDefinition
        {
            public MarkdownPreFormat() { this.FontTypeface = new Typeface("Courier New"); }
        }

        // Quotes

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.blockquote")]
        [Name("markdown.blockquote")]
        [UserVisible(true)]
        [Order(Before = Priority.Default)] // Low priority
        sealed class MarkdownBlockquoteFormat : ClassificationFormatDefinition
        {
            public MarkdownBlockquoteFormat()
            {
                this.ForegroundColor = Colors.IndianRed;

                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownBlockquote");
                    };
            }
        }

        // Links

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.link")]
        [Name("markdown.link")]
        [Order(Before = Priority.Default, After = "markdown.blockquote")] // Low priority
        sealed class MarkdownLink : ClassificationFormatDefinition
        {
            public MarkdownLink()
            {
                this.ForegroundColor = Colors.Crimson;
                this.IsBold = true;
            }
        }

        // Images

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.image")]
        [Name("markdown.image")]
        [Order(Before = Priority.Default, After = "markdown.blockquote")] // Low priority
        sealed class MarkdownImage : ClassificationFormatDefinition
        {
            public MarkdownImage()
            {
                this.ForegroundColor = Colors.DarkGreen;
                this.IsBold = true;
            }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.horizontalrule")]
        [Name("markdown.horizontalrule")]
        [UserVisible(true)]
        sealed class MarkdownHorizontalRule : ClassificationFormatDefinition
        {
            public MarkdownHorizontalRule()
            {
                this.TextDecorations = System.Windows.TextDecorations.Strikethrough;
                this.IsBold = true;
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownHorizontalRule");
                    };
            }
        }

        // Epic types
        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.metadata")]
        [Name("markdown.metadata")]
        [UserVisible(true)]
        sealed class Metadata : ClassificationFormatDefinition
        {
            public Metadata()
            {
                this.IsBold = true;
                this.IsItalic = true;
                this.ForegroundColor = Colors.DarkGoldenrod;
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownMetadata");
                    };
            }
        }

        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.tableofcontents")]
        [Name("markdown.tableofcontents")]
        [UserVisible(true)]
        sealed class TableOfContents : ClassificationFormatDefinition
        {
            public TableOfContents()
            {
                this.IsBold = true;
                this.ForegroundColor = Colors.Indigo;
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownTOC");
                    };
            }
        }


        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.fencedcodeblock")]
        [Name("markdown.fencedcodeblock")]
        [UserVisible(true)]
        sealed class FencedCodeBlock : ClassificationFormatDefinition
        {
            public FencedCodeBlock()
            {
                this.ForegroundColor = Colors.DarkSlateGray;
                this.FontTypeface = new Typeface("Courier New");
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownFencedCodeBlock");
                    };
            }
        }

        
        [Export(typeof(EditorFormatDefinition))]
        [ClassificationType(ClassificationTypeNames = "markdown.htmlcomment")]
        [Name("markdown.htmlcomment")]
        [UserVisible(true)]
        sealed class HTMLComment : ClassificationFormatDefinition
        {
            public HTMLComment()
            {
                this.ForegroundColor = Colors.Green;
                Language.Loaded += () =>
                    {
                        this.DisplayName = Language.Message("MarkdownHTMLComment");
                    };
            }
        }
    }
}
