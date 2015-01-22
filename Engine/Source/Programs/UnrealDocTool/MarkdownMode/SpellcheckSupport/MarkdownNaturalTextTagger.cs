// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using SpellChecker.Definitions;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Tagging;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode.SpellcheckSupport
{
    [Export(typeof(ITaggerProvider))]
    [ContentType(ContentType.Name)]
    [TagType(typeof(NaturalTextTag))]
    class MarkdownNaturalTextTaggerProvider : ITaggerProvider
    {
        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            return new MarkdownNaturalTextTagger() as ITagger<T>;
        }
    }

    class MarkdownNaturalTextTagger : ITagger<NaturalTextTag>
    {
        public IEnumerable<ITagSpan<NaturalTextTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            foreach (var snapshotSpan in spans)
            {
                yield return new TagSpan<NaturalTextTag>(snapshotSpan, new NaturalTextTag());
            }
        }

#pragma warning disable 67
        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
#pragma warning restore 67
    }

    class NaturalTextTag : INaturalTextTag { }
}
