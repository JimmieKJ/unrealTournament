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
using System.Windows.Media;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;
using SpellChecker.Definitions;
using Microsoft.VisualStudio.Text.Adornments;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    /// <summary>
    /// Squiggle tag for misspelled words.
    /// </summary>
    internal class SpellSquiggleTag : ErrorTag
    {
        public SpellSquiggleTag(string squiggleType) : base(squiggleType) { }
    }

    /// <summary>
    /// Tagger for Spelling squiggles.
    /// </summary>
    internal class SquiggleTagger : ITagger<IErrorTag>, IDisposable
    {
        #region Private Fields
        private ITextBuffer _buffer;
        private ITagAggregator<IMisspellingTag> _misspellingAggregator;
        private bool disposed = false;
        internal const string SpellingErrorType = "Spelling Error";
        #endregion

        #region MEF Imports / Exports

        /// <summary>
        /// Defines colors for the spelling squiggles.
        /// </summary>
        [Export(typeof(EditorFormatDefinition))]
        [Name(SquiggleTagger.SpellingErrorType)]
        [Order(After = Priority.High)]
        [UserVisible(true)]
        internal class SpellingErrorClassificationFormatDefinition : EditorFormatDefinition
        {
            public SpellingErrorClassificationFormatDefinition()
            {
                this.ForegroundColor = Colors.Red;
                this.BackgroundCustomizable = false;
                this.DisplayName = "Spelling Error";
            }
        }

#pragma warning disable 414
        [Export(typeof(ErrorTypeDefinition))]
        [Name(SquiggleTagger.SpellingErrorType)]
        [DisplayName(SquiggleTagger.SpellingErrorType)]
        ErrorTypeDefinition SpellingErrorTypeDefinition = null;
#pragma warning restore 414

        /// <summary>
        /// MEF connector for the Spell checker squiggles.
        /// </summary>
        [Export(typeof(IViewTaggerProvider))]
        [ContentType("any")]
        [TagType(typeof(SpellSquiggleTag))]
        internal class SquiggleTaggerProvider : IViewTaggerProvider
        {
            [Import]
            internal IViewTagAggregatorFactoryService TagAggregatorFactory { get; set; }

            #region ITaggerProvider
            public ITagger<T> CreateTagger<T>(ITextView textView, ITextBuffer buffer) where T : ITag
            {
                // If this view isn't editable, then there isn't a good reason to be showing these.
                if (!textView.Roles.Contains(PredefinedTextViewRoles.Editable) || !textView.Roles.Contains(PredefinedTextViewRoles.PrimaryDocument))
                    return null;

                // Make sure we only tagging top buffer
                if (buffer != textView.TextBuffer)
                    return null;

                return new SquiggleTagger(buffer, TagAggregatorFactory.CreateTagAggregator<IMisspellingTag>(textView)) as ITagger<T>;
            }
            #endregion
        }
        #endregion

        #region Constructors
        public SquiggleTagger(ITextBuffer buffer, ITagAggregator<IMisspellingTag> misspellingAggregator)
        {
            _buffer = buffer;
            _misspellingAggregator = misspellingAggregator;
            _misspellingAggregator.TagsChanged += (sender, args) =>
            {
                foreach (var span in args.Span.GetSpans(_buffer))
                    RaiseTagsChangedEvent(span);
            };
        }

        #endregion

        #region ITagger<SpellSquiggleTag> Members
        /// <summary>
        /// Returns tags on demand.
        /// </summary>
        /// <param name="spans">Spans collection to get tags for.</param>
        /// <returns>Squiggle tags in provided spans.</returns>
        public IEnumerable<ITagSpan<IErrorTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            if (spans.Count == 0)
                yield break;

            ITextSnapshot snapshot = spans[0].Snapshot;

            foreach (var misspelling in _misspellingAggregator.GetTags(spans))
            {
                var misspellingSpans = misspelling.Span.GetSpans(snapshot);
                if (misspellingSpans.Count != 1)
                    continue;

                SnapshotSpan errorSpan = misspellingSpans[0];

                yield return new TagSpan<IErrorTag>(
                                errorSpan,
                                new SpellSquiggleTag(SquiggleTagger.SpellingErrorType));
            }
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
        #endregion

        #region IDisposable
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (!this.disposed)
            {
                if (disposing)
                {
                    _misspellingAggregator.Dispose();
                }

                disposed = true;
            }
        }
        #endregion

        #region Event Handlers
        void SpellingChangedHandler(object sender, SpellingEventArgs e)
        {
            ITextSnapshot snapshot = _buffer.CurrentSnapshot;
            RaiseTagsChangedEvent(new SnapshotSpan(snapshot, new Span(0, snapshot.Length)));
        }
        #endregion

        #region Helpers
        private void RaiseTagsChangedEvent(SnapshotSpan subjectSpan)
        {
            EventHandler<SnapshotSpanEventArgs> handler = this.TagsChanged;
            if (handler != null)
            {
                handler(this, new SnapshotSpanEventArgs(subjectSpan));
            }
        }

        #endregion
    }
}
