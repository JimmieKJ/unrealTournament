// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode
{
    static class ContentType
    {
        public const string Name = "markdown";

        [Export]
        [Name(Name)]
        [DisplayName("Markdown")]
        [BaseDefinition("code")]
        public static ContentTypeDefinition MarkdownModeContentType = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".mkd")]
        public static FileExtensionToContentTypeDefinition MkdFileExtension = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".md")]
        public static FileExtensionToContentTypeDefinition MdFileExtension = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".udn")]
        public static FileExtensionToContentTypeDefinition UDNFileExtension = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".mdown")]
        public static FileExtensionToContentTypeDefinition MdownFileExtension = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".mkdn")]
        public static FileExtensionToContentTypeDefinition MkdnFileExtension = null;

        [Export]
        [ContentType(Name)]
        [FileExtension(".markdown")]
        public static FileExtensionToContentTypeDefinition MarkdownFileExtension = null;
    }
}
