// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using MarkdownMode.Properties;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode
{
    using System.Threading;

    using MarkdownMode.ToolWindow;

    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.Preprocessor;
    using System.Text.RegularExpressions;

    [Export(typeof(ITaggerProvider))]
    [TagType(typeof(IOutliningRegionTag))]
    [ContentType(ContentType.Name)]
    sealed class OutliningTaggerProvider : ITaggerProvider
    {
        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            return buffer.Properties.GetOrCreateSingletonProperty(() => new OutliningTagger(buffer)) as ITagger<T>;
        }
    }

    sealed class OutliningTagger : ITagger<IOutliningRegionTag>
    {
        readonly ITextBuffer buffer;
        ReaderWriterLockSlim sectionsLock = new ReaderWriterLockSlim();
        List<ITrackingSpan> sections;

        public OutliningTagger(ITextBuffer buffer)
        {
            this.buffer = buffer;
            sections = new List<ITrackingSpan>();
        }

        public void ReparseFile(UDNParsingResults results)
        {
            CreateDocumentSections(results);

            if (TagsChanged != null)
            {
                TagsChanged(
                    this,
                    new SnapshotSpanEventArgs(
                        new SnapshotSpan(results.ParsedSnapshot, 0, results.ParsedSnapshot.Length)));
            }
        }

        private static Regex TrimmingPattern = new Regex(@"\A\s*(?<content>.+?)\s*\z", RegexOptions.Compiled | RegexOptions.Singleline);

        private void CreateDocumentSections(UDNParsingResults results)
        {
            var doc = results.Document;

            var elements =
                doc.Data.GetElements<EMElement>().Where(
                    e => !(e is EMContentElement) && e.Document == doc);

            var originalText = doc.Text;

            var bounds = new List<Interval>();

            foreach (var element in elements)
            {
                try
                {
                    var bound = element.GetOriginalTextBounds();

                    if (bound.Start < 0 || bound.End > originalText.Length)
                    {
                        continue;
                    }

                    bounds.Add(bound);
                }
                catch (UnableToDetectOriginalPositionException)
                {
                    // ignore
                }
            }

            foreach (var preprocessedBound in doc.GetPreprocessedTextBounds())
            {
                bounds.Add(preprocessedBound.Bound);
            }

            var newSections = new List<ITrackingSpan>();

            foreach (var bound in bounds)
            {
                var boundedText = originalText.Substring(bound.Start, bound.End - bound.Start + 1);
                var match = TrimmingPattern.Match(boundedText);

                if (match.Groups["content"].Value.Count(e => e == '\n') > 0) // is multiline element
                {
                    newSections.Add(results.ParsedSnapshot.CreateTrackingSpan(
                        new Span(bound.Start + match.Groups["content"].Index, match.Groups["content"].Length),
                        SpanTrackingMode.EdgeExclusive));
                }
            }

            var comboData = UDNDocRunningTableMonitor.CurrentUDNDocView.NavigateToComboData;
            for (var i = 0; i < comboData.Count; ++i)
            {
                newSections.Add(comboData.GetSection(i));
            }

            sectionsLock.EnterWriteLock();
            try
            {
                sections.Clear();
                sections.AddRange(newSections);
            }
            finally
            {
                sectionsLock.ExitWriteLock();
            }
        }

        public IEnumerable<ITagSpan<IOutliningRegionTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            sectionsLock.EnterReadLock();
            try
            {
                if (Settings.Default.DisableAllParsing || sections == null || sections.Count == 0 || spans.Count == 0)
                {
                    yield break;
                }

                var snapshot = spans[0].Snapshot;

                if (snapshot.TextBuffer != sections.First().TextBuffer)
                {
                    // stop if spans and sections doesn't belong to the same TextBuffer
                    yield break;
                }

                foreach (var section in sections)
                {
                    var sectionSpan = section.GetSpan(snapshot);

                    if (!spans.IntersectsWith(new NormalizedSnapshotSpanCollection(sectionSpan)))
                    {
                        continue;
                    }

                    var firstLine = sectionSpan.Start.GetContainingLine().GetText().TrimStart(' ', '\t', '#');

                    var collapsedHintText = sectionSpan.Length > 250
                                                ? snapshot.GetText(sectionSpan.Start, 247) + "..."
                                                : sectionSpan.GetText();

                    var tag = new OutliningRegionTag(firstLine, collapsedHintText);

                    yield return new TagSpan<IOutliningRegionTag>(sectionSpan, tag);
                }
            }
            finally
            {
                sectionsLock.ExitReadLock();
            }
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
    }
}
