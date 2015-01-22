// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Threading;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;
using SpellChecker.Definitions;
using Microsoft.VisualStudio.Text.Editor;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    [Export(typeof(IViewTaggerProvider))]
    [ContentType("any")]
    [TagType(typeof(MisspellingTag))]
    sealed class SpellingTaggerProvider : IViewTaggerProvider
    {
        [Import]
        IViewTagAggregatorFactoryService AggregatorFactory = null;

        [Import]
        ISpellingDictionaryService SpellingDictionaryFactory = null;

        public ITagger<T> CreateTagger<T>(ITextView textView, ITextBuffer buffer) where T : ITag
        {
            if (textView.TextBuffer != buffer)
                return null;

            SpellingTagger spellingTagger;
            if (textView.Properties.TryGetProperty(typeof(SpellingTagger), out spellingTagger))
                return spellingTagger as ITagger<T>;

            var dictionary = SpellingDictionaryFactory.GetDictionary(buffer);
            var naturalTextAggregator = AggregatorFactory.CreateTagAggregator<INaturalTextTag>(textView, 
                                                                                               TagAggregatorOptions.MapByContentType);
            var urlAggregator = AggregatorFactory.CreateTagAggregator<IUrlTag>(textView);
            spellingTagger = new SpellingTagger(buffer, textView, naturalTextAggregator, urlAggregator, dictionary);
            textView.Properties[typeof(SpellingTagger)] = spellingTagger;

            return spellingTagger as ITagger<T>;
        }
    }

    class MisspellingTag : IMisspellingTag
    {
        public MisspellingTag(SnapshotSpan span, IEnumerable<string> suggestions)
        {
            Span = span.Snapshot.CreateTrackingSpan(span, SpanTrackingMode.EdgeExclusive);
            Suggestions = suggestions;
        }

        public ITrackingSpan Span { get; private set; }
        public IEnumerable<string> Suggestions { get; private set; }

        public ITagSpan<MisspellingTag> ToTagSpan(ITextSnapshot snapshot)
        {
            return new TagSpan<MisspellingTag>(Span.GetSpan(snapshot), this);
        }
    }

    sealed class SpellingTagger : ITagger<MisspellingTag>
    {
        ITextBuffer _buffer;
        ITagAggregator<INaturalTextTag> _naturalTextAggregator;
        ITagAggregator<IUrlTag> _urlAggregator;
        Dispatcher _dispatcher;
        ISpellingDictionary _dictionary;

        List<SnapshotSpan> _dirtySpans;

        object _dirtySpanLock = new object();
        volatile List<MisspellingTag> _misspellings;

        Thread _updateThread;
        DispatcherTimer _timer;

        bool _isClosed;

        public SpellingTagger(ITextBuffer buffer,
                              ITextView view,
                              ITagAggregator<INaturalTextTag> naturalTextAggregator,
                              ITagAggregator<IUrlTag> urlAggregator,
                              ISpellingDictionary dictionary)
        {
            _isClosed = false;
            _buffer = buffer;
            _naturalTextAggregator = naturalTextAggregator;
            _urlAggregator = urlAggregator;
            _dispatcher = Dispatcher.CurrentDispatcher;
            _dictionary = dictionary;

            _dirtySpans = new List<SnapshotSpan>();
            _misspellings = new List<MisspellingTag>();

            _buffer.Changed += BufferChanged;
            _naturalTextAggregator.TagsChanged += AggregatorTagsChanged;
            _urlAggregator.TagsChanged += AggregatorTagsChanged;
            _dictionary.DictionaryUpdated += DictionaryUpdated;

            view.Closed += ViewClosed;

            // To start with, the entire buffer is dirty
            // Split this into chunks, so we update pieces at a time
            ITextSnapshot snapshot = _buffer.CurrentSnapshot;

            foreach (var line in snapshot.Lines)
                AddDirtySpan(line.Extent);
        }

        void ViewClosed(object sender, EventArgs e)
        {
            _isClosed = true;

            if (_timer != null)
                _timer.Stop();
            if (_buffer != null)
                _buffer.Changed -= BufferChanged;
            if (_naturalTextAggregator != null)
                _naturalTextAggregator.Dispose();
            if (_urlAggregator != null)
                _urlAggregator.Dispose();
            if (_dictionary != null)
                _dictionary.DictionaryUpdated -= DictionaryUpdated;
        }

        void AggregatorTagsChanged(object sender, TagsChangedEventArgs e)
        {
            if (_isClosed)
                return;

            NormalizedSnapshotSpanCollection dirtySpans = e.Span.GetSpans(_buffer.CurrentSnapshot);

            if (dirtySpans.Count == 0)
                return;

            foreach (var span in dirtySpans)
                AddDirtySpan(span);
        }

        void DictionaryUpdated(object sender, SpellingEventArgs e)
        {
            if (_isClosed)
                return;

            ITextSnapshot snapshot = _buffer.CurrentSnapshot;

            // If the word is null, it means the entire dictionary was updated and we
            // need to reparse the entire file.
            if (e.Word == null)
            {
                foreach (var line in snapshot.Lines)
                    AddDirtySpan(line.Extent);
                return;
            }

            List<MisspellingTag> currentMisspellings = _misspellings;

            foreach (var misspelling in currentMisspellings)
            {
                SnapshotSpan span = misspelling.Span.GetSpan(snapshot);

                if (span.GetText() == e.Word)
                    AddDirtySpan(span);
            }
        }

        void BufferChanged(object sender, TextContentChangedEventArgs e)
        {
            if (_isClosed)
                return;

            ITextSnapshot snapshot = e.After;

            foreach (var change in e.Changes)
            {
                SnapshotSpan changedSpan = new SnapshotSpan(snapshot, change.NewSpan);

                var startLine = changedSpan.Start.GetContainingLine();
                var endLine = (startLine.EndIncludingLineBreak < changedSpan.End) ? changedSpan.End.GetContainingLine() : startLine;

                AddDirtySpan(new SnapshotSpan(startLine.Start, endLine.End));
            }
        }

        #region Helpers

        NormalizedSnapshotSpanCollection GetNaturalLanguageSpansForDirtySpan(SnapshotSpan dirtySpan)
        {
            if (_isClosed || dirtySpan.IsEmpty)
                return new NormalizedSnapshotSpanCollection();

            ITextSnapshot snapshot = dirtySpan.Snapshot;
            var spans = new NormalizedSnapshotSpanCollection(
                _naturalTextAggregator.GetTags(dirtySpan)
                                      .SelectMany(tag => tag.Span.GetSpans(snapshot))
                                      .Select(s => s.Intersection(dirtySpan))
                                      .Where(s => s.HasValue && !s.Value.IsEmpty)
                                      .Select(s => s.Value));

            // Now, subtract out IUrlTag spans, since we never want to spell check URLs
            var urlSpans = new NormalizedSnapshotSpanCollection(
                _urlAggregator.GetTags(spans)
                              .SelectMany(tagSpan => tagSpan.Span.GetSpans(snapshot)));

            return NormalizedSnapshotSpanCollection.Difference(spans, urlSpans);
        }

        void AddDirtySpan(SnapshotSpan span)
        {
            if (span.IsEmpty)
                return;

            lock (_dirtySpanLock)
            {
                _dirtySpans.Add(span);
                ScheduleUpdate();
            }
        }

        void ScheduleUpdate()
        {
            if (_isClosed)
                return;

            if (_timer == null)
            {
                _timer = new DispatcherTimer(DispatcherPriority.ApplicationIdle, _dispatcher)
                {
                    Interval = TimeSpan.FromMilliseconds(500)
                };

                _timer.Tick += GuardedStartUpdateThread;
            }

            _timer.Stop();
            _timer.Start();
        }

        void GuardedStartUpdateThread(object sender, EventArgs e)
        {
            try
            {
                StartUpdateThread(sender, e);
            }
            catch (Exception ex)
            {
                Debug.Fail("Exception!" + ex.Message);
                // If anything fails during the handling of a dispatcher tick, just ignore it.  If we don't guard against those exceptions, the
                // user will see a crash.
            }
        }

        void StartUpdateThread(object sender, EventArgs e)
        {
            // If an update is currently running, wait until the next timer tick
            if (_isClosed || _updateThread != null && _updateThread.IsAlive)
                return;

            _timer.Stop();

            List<SnapshotSpan> dirtySpans;
            lock (_dirtySpanLock)
            {
                dirtySpans = new List<SnapshotSpan>(_dirtySpans);
                _dirtySpans = new List<SnapshotSpan>();

                if (dirtySpans.Count == 0)
                    return;
            }

            // Normalize the dirty spans
            ITextSnapshot snapshot = _buffer.CurrentSnapshot;
            var normalizedSpans = new NormalizedSnapshotSpanCollection(dirtySpans.Select(s => s.TranslateTo(snapshot, SpanTrackingMode.EdgeInclusive)));

            _updateThread = new Thread(GuardedCheckSpellings)
            {
                Name = "Spell Check",
                Priority = ThreadPriority.BelowNormal
            };

            if (!_updateThread.TrySetApartmentState(ApartmentState.STA))
                Debug.Fail("Unable to set thread apartment state to STA, things *will* break.");

            _updateThread.Start(normalizedSpans);
        }

        void GuardedCheckSpellings(object dirtySpansObject)
        {
            if (_isClosed)
                return;

            try
            {
                IEnumerable<SnapshotSpan> dirtySpans = dirtySpansObject as IEnumerable<SnapshotSpan>;
                if (dirtySpans == null)
                {
                    Debug.Fail("Being asked to check a null list of dirty spans.  What gives?");
                    return;
                }

                CheckSpellings(dirtySpans);
            }
            catch (Exception ex)
            {
                Debug.Fail("Exception!" + ex.Message);
                // If anything fails in the background thread, just ignore it.  It's possible that the background thread will run
                // on VS shutdown, at which point calls into WPF throw exceptions.  If we don't guard against those exceptions, the
                // user will see a crash on exit.
            }
            finally
            {
                try
                {
                    Dispatcher.CurrentDispatcher.InvokeShutdown();
                }
                catch (Exception)
                {
                    // Again, ignore this failure.
                }
            }
        }

        void CheckSpellings(IEnumerable<SnapshotSpan> dirtySpans)
        {
            TextBox textBox = new TextBox();
            textBox.SpellCheck.IsEnabled = true;

            ITextSnapshot snapshot = _buffer.CurrentSnapshot;

            foreach (var dirtySpan in dirtySpans)
            {
                var dirty = dirtySpan.TranslateTo(snapshot, SpanTrackingMode.EdgeInclusive);

                // We have to go back to the UI thread to get natural text spans
                List<SnapshotSpan> naturalTextSpans = new List<SnapshotSpan>();
                OnForegroundThread(() => naturalTextSpans = GetNaturalLanguageSpansForDirtySpan(dirty).ToList());

                var naturalText = new NormalizedSnapshotSpanCollection(
                    naturalTextSpans.Select(span => span.TranslateTo(snapshot, SpanTrackingMode.EdgeInclusive)));

                List<MisspellingTag> currentMisspellings = new List<MisspellingTag>(_misspellings);
                List<MisspellingTag> newMisspellings = new List<MisspellingTag>();

                int removed = currentMisspellings.RemoveAll(tag => tag.ToTagSpan(snapshot).Span.OverlapsWith(dirty));
                newMisspellings.AddRange(GetMisspellingsInSpans(naturalText, textBox));
               
                // Also remove empties
                removed += currentMisspellings.RemoveAll(tag => tag.ToTagSpan(snapshot).Span.IsEmpty);

                // If anything has been updated, we need to send out a change event
                if (newMisspellings.Count != 0 || removed != 0)
                {
                    currentMisspellings.AddRange(newMisspellings);

                    _dispatcher.Invoke(new Action(() =>
                        {
                            if (_isClosed)
                                return;

                            _misspellings = currentMisspellings;

                            var temp = TagsChanged;
                            if (temp != null)
                                temp(this, new SnapshotSpanEventArgs(dirty));

                        }));
                }
            }
            
            lock (_dirtySpanLock)
            {
                if (_isClosed)
                    return;

                if (_dirtySpans.Count != 0)
                    _dispatcher.BeginInvoke(new Action(() => ScheduleUpdate()));
            }
        }
        
        IEnumerable<MisspellingTag> GetMisspellingsInSpans(NormalizedSnapshotSpanCollection spans, TextBox textBox)
        {
            foreach (var span in spans)
            {
                string text = span.GetText();

                foreach (var word in GetWordsInText(text))
                {
                    string textToParse = span.Snapshot.GetText(span.Start + word.Start, word.Length);

                    if (!ProbablyARealWord(textToParse))
                        continue;

                    // Now pass these off to WPF.
                    textBox.Text = textToParse;

                    int nextSearchIndex = 0;
                    int nextSpellingErrorIndex = -1;

                    while (-1 != (nextSpellingErrorIndex = textBox.GetNextSpellingErrorCharacterIndex(nextSearchIndex, LogicalDirection.Forward)))
                    {
                        var spellingError = textBox.GetSpellingError(nextSpellingErrorIndex);
                        int length = textBox.GetSpellingErrorLength(nextSpellingErrorIndex);

                        // Work around what looks to be a WPF bug; if the spelling error is followed by a 's, then include that in the error span.
                        string nextChars = textToParse.Substring(nextSpellingErrorIndex + length).ToLowerInvariant();
                        if (nextChars.StartsWith("'s"))
                            length += 2;

                        SnapshotSpan errorSpan = new SnapshotSpan(span.Start + word.Start + nextSpellingErrorIndex, length);

                        if (ProbablyARealWord(errorSpan.GetText()) && !_dictionary.ShouldIgnoreWord(errorSpan.GetText()))
                        {
                            yield return new MisspellingTag(errorSpan, spellingError.Suggestions.ToArray());
                        }

                        nextSearchIndex = nextSpellingErrorIndex + length;
                        if (nextSearchIndex >= textToParse.Length)
                            break;
                    }
                }
            }
        }

        // Determine if the word is likely a real word, and not any of the following:
        // 1) Words that are CamelCased, since those are probably not "real" words to begin with.
        // 2) Things that look like filenames (contain a "." followed by something other than a "."). We may miss a few "real" misspellings
        //    here due to a missed space after a period, but that's acceptable.
        // 3) Words that include digits
        // 4) Words that include underscores
        // 5) Words in ALL CAPS
        static internal bool ProbablyARealWord(string word)
        {
            if (string.IsNullOrWhiteSpace(word))
                return false;

            word = word.Trim();

            // Check digits/underscores
            if (word.Any(c => c == '_' || char.IsDigit(c)))
                return false;

            // Check for a . in the middle of the word
            int firstDot = word.IndexOf('.');
            if (firstDot >= 0 && firstDot < word.Length - 1 && word.Skip(firstDot + 1).Any(c => !IsWordBreakCharacter(c)))
                return false;

            // CamelCase/UPPER
            char firstLetter = word.FirstOrDefault(c => char.IsLetter(c));
            if (firstLetter != 0)
            {
                int toSkip = word.IndexOf(firstLetter);
                if (toSkip >= 0 && toSkip < word.Length - 1 && word.Skip(toSkip + 1).Any(c => char.IsUpper(c)))
                    return false;
            }

            return true;
        }

        static internal IEnumerable<Microsoft.VisualStudio.Text.Span> GetWordsInText(string text)
        {
            if (string.IsNullOrWhiteSpace(text))
                yield break;

            // We need to break this up for WPF, because it is *incredibly* slow at checking the spelling
            for (int i = 0; i < text.Length; i++)
            {
                if (IsWordBreakCharacter(text[i]))
                    continue;

                int end = i;
                for (; end < text.Length; end++)
                {
                    if (IsWordBreakCharacter(text[end]))
                        break;
                }

                yield return Microsoft.VisualStudio.Text.Span.FromBounds(i, end);
                i = end - 1;
            }
        }

        // Word break characters.  Specifically, exclude _.'
        const string wordBreakChars = ",/<>?;:\"[]\\{}|-=+~!@#$%^&*() \t";
        static bool IsWordBreakCharacter(char c)
        {
            return wordBreakChars.Contains(c) || char.IsWhiteSpace(c);
        }

        void OnForegroundThread(Action action, DispatcherPriority priority = DispatcherPriority.ApplicationIdle)
        {
            _dispatcher.Invoke(action, priority);
        }

        #endregion

        #region Tagging implementation

        public IEnumerable<ITagSpan<MisspellingTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            if (_isClosed || spans.Count == 0)
                yield break;

            List<MisspellingTag> currentMisspellings = _misspellings;

            if (currentMisspellings.Count == 0)
                yield break;

            ITextSnapshot snapshot = spans[0].Snapshot;

            foreach (var misspelling in currentMisspellings)
            {
                var tagSpan = misspelling.ToTagSpan(snapshot);
                if (tagSpan.Span.Length == 0)
                    continue;

                if (spans.IntersectsWith(new NormalizedSnapshotSpanCollection(tagSpan.Span)))
                    yield return tagSpan;
            }
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
        
        #endregion
    }
}
