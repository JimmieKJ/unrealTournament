// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    [Export(typeof(ITaggerProvider))]
    [ContentType("html")]
    [TagType(typeof(NaturalTextTag))]
    internal class HtmlTextTaggerProvider : ITaggerProvider
    {
        [Import]
        IClassifierAggregatorService ClassifierAggregatorService { get; set; }

        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            return new HtmlTextTagger(buffer, ClassifierAggregatorService.GetClassifier(buffer)) as ITagger<T>;
        }
    }

    internal class HtmlTextTagger : ITagger<NaturalTextTag>
    {
        private ITextBuffer _buffer;
        private IClassifier _aggregator;

        public HtmlTextTagger(ITextBuffer buffer, IClassifier aggregator)
        {
            _buffer = buffer;
            _aggregator = aggregator;
        }

        public IEnumerable<ITagSpan<NaturalTextTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            NormalizedSnapshotSpanCollection classifiedSpans = new NormalizedSnapshotSpanCollection(
                spans.SelectMany(span => _aggregator.GetClassificationSpans(span))
                     .Select(c => c.Span));

            NormalizedSnapshotSpanCollection plainSpans = NormalizedSnapshotSpanCollection.Difference(spans, classifiedSpans);

            foreach (var span in plainSpans)
            {
                yield return new TagSpan<NaturalTextTag>(span, new NaturalTextTag());
            }
        }

#pragma warning disable 67
        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
#pragma warning restore 67
    }
}
