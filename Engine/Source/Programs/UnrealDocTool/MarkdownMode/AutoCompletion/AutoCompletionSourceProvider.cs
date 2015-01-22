// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;

using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Text.Operations;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode.AutoCompletion
{
    [Export(typeof(ICompletionSourceProvider))]
    [ContentType("markdown")]
    [Name("MarkdownMode autocompletion")]
    public class AutoCompletionSourceProvider : ICompletionSourceProvider
    {
        [Import]
        public ITextStructureNavigatorSelectorService TextNavigatorService { get; private set; }

        [Import]
        public IGlyphService GlyphService { get; private set; }

        public ICompletionSource TryCreateCompletionSource(ITextBuffer textBuffer)
        {
            return new AutoCompletionSource(textBuffer, this);
        }
    }
}
