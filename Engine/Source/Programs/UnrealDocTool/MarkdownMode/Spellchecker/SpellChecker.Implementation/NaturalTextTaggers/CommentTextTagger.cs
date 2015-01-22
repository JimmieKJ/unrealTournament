//***************************************************************************
//
//    Copyright (c) Microsoft Corporation. All rights reserved.
//    This code is licensed under the Visual Studio SDK license terms.
//    THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
//    ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
//    IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
//    PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//***************************************************************************

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.Globalization;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;
using System.Linq;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    [Export(typeof(ITaggerProvider))]
    [ContentType("code")]
    [TagType(typeof(NaturalTextTag))]
    internal class CommentTextTaggerProvider : ITaggerProvider
    {
        [Import]
        internal IClassifierAggregatorService ClassifierAggregatorService { get; set; }

        [Import]
        internal IBufferTagAggregatorFactoryService TagAggregatorFactory { get; set; }

        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            if (buffer == null)
                throw new ArgumentNullException("buffer");

            // Due to an issue with the built-in C# classifier, we avoid using it.
            if (buffer.ContentType.IsOfType("csharp"))
                return new CSharpCommentTextTagger(buffer) as ITagger<T>;

            var classifierAggregator = ClassifierAggregatorService.GetClassifier(buffer);

            return new CommentTextTagger(buffer, classifierAggregator) as ITagger<T>;
        }
    }

    internal class CommentTextTagger : ITagger<NaturalTextTag>, IDisposable
    {
        ITextBuffer _buffer;
        IClassifier _classifier;

        public CommentTextTagger(ITextBuffer buffer, IClassifier classifier)
        {
            _buffer = buffer;
            _classifier = classifier;

            classifier.ClassificationChanged += ClassificationChanged;
        }

        public IEnumerable<ITagSpan<NaturalTextTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            if (_classifier == null || spans == null || spans.Count == 0)
                yield break;

            ITextSnapshot snapshot = spans[0].Snapshot;

            foreach (var snapshotSpan in spans)
            {
                Debug.Assert(snapshotSpan.Snapshot.TextBuffer == _buffer);
                foreach (ClassificationSpan classificationSpan in _classifier.GetClassificationSpans(snapshotSpan))
                {
                    string name = classificationSpan.ClassificationType.Classification.ToLowerInvariant();

                    if ((name.Contains("comment") || name.Contains("string")) &&
                       !(name.Contains("xml doc tag")))
                    {
                        yield return new TagSpan<NaturalTextTag>(
                                classificationSpan.Span,
                                new NaturalTextTag()
                                );
                    }
                }
            }
        }

        void ClassificationChanged(object sender, ClassificationChangedEventArgs e)
        {
            var temp = TagsChanged;
            if (temp != null)
                temp(this, new SnapshotSpanEventArgs(e.ChangeSpan));
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;

        public void Dispose()
        {
            if (_classifier != null)
                _classifier.ClassificationChanged -= ClassificationChanged;
        }
    }
}
