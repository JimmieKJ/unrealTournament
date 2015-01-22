// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows;
using MarkdownMode.Properties;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Utilities;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.TextManager.Interop;
using Microsoft.VisualStudio.Shell.Interop;
using System.Text.RegularExpressions;
using MarkdownSharp;
using MarkdownMode.ToolWindow;
using MarkdownSharp.EpicMarkdown;
using MarkdownSharp.Preprocessor;
using System.Linq;

namespace MarkdownMode
{
    using System.Threading;

    [Export(typeof(IClassifierProvider))]
    [ContentType("markdown")]
    class MarkdownClassifierProvider : IClassifierProvider
    {
        [Import]
        IClassificationTypeRegistryService ClassificationRegistry = null;

        [Import]
        IVsEditorAdaptersFactoryService AdaptersFactory = null;

        public IClassifier GetClassifier(ITextBuffer buffer)
        {
            return buffer.Properties.GetOrCreateSingletonProperty(() => new MarkdownClassifier(buffer, ClassificationRegistry, AdaptersFactory));
        }
    }

    class MarkdownClassifier : IClassifier
    {
        IClassificationTypeRegistryService _classificationRegistry;
        IVsEditorAdaptersFactoryService _adapter;

        private static readonly Regex IndentedLine = new Regex(@"^(?:(?:\t|[ ]{4,})[ \t]*?(?=\S+))", RegexOptions.Compiled | RegexOptions.Singleline);
        private static readonly Regex NotIndentedLine = new Regex(@"^\S", RegexOptions.Compiled | RegexOptions.Singleline);

        public MarkdownClassifier(ITextBuffer buffer, IClassificationTypeRegistryService classificationRegistry, IVsEditorAdaptersFactoryService AdaptersFactory)
        {
            _classificationRegistry = classificationRegistry;
            _adapter = AdaptersFactory;
        }

        public event EventHandler<ClassificationChangedEventArgs> ClassificationChanged;

        private ReaderWriterLockSlim spansLock = new ReaderWriterLockSlim();
        private List<ClassificationSpan> spans = new List<ClassificationSpan>();

        public IList<ClassificationSpan> GetClassificationSpans(SnapshotSpan changedSpan)
        {
            if (Settings.Default.DisableAllParsing || Settings.Default.DisableHighlighting)
            {
                return new List<ClassificationSpan>();
            }

            // We will always refresh app spans so there is no reason
            // to calculate span intersections etc. Returning a copy
            // cause the internal one can be modified.

            spansLock.EnterReadLock();
            try
            {
                return new List<ClassificationSpan>(spans);
            }
            finally
            {
                spansLock.ExitReadLock();
            }
        }

        string GetClassificationStringForPreprocessedText(PreprocessedTextType type)
        {
            switch (type)
            {
                case PreprocessedTextType.Comment:
                    return "markdown.htmlcomment";
                case PreprocessedTextType.FilteredOut:
                    return null;
                case PreprocessedTextType.Metadata:
                    return "markdown.metadata";
                case PreprocessedTextType.Reference:
                    return "markdown.url.definition";
                default:
                    return null;
            }
        }

        public void Reclassify(UDNParsingResults results)
        {

            var boundsWithClassificationStrings = new List<Tuple<Interval, string>>();

            foreach (var element in
                results.Document.Data.GetElements<EMElement>()
                       .Where(
                           e =>
                           e.Document == results.Document
                           && !(e is EMContentElement)))
            {
                var classificationString = element.GetClassificationString();

                if (classificationString == null)
                {
                    continue;
                }

                var bound = element.GetOriginalTextBounds();

                if (bound.End > results.ParsedSnapshot.Length)
                {
                    continue;
                }

                boundsWithClassificationStrings.Add(Tuple.Create(bound, classificationString));
            }

            foreach (var preprocessedBound in results.Document.GetPreprocessedTextBounds())
            {
                var classificationString = GetClassificationStringForPreprocessedText(preprocessedBound.Type);

                if (classificationString == null)
                {
                    continue;
                }

                boundsWithClassificationStrings.Add(Tuple.Create(preprocessedBound.Bound, classificationString));
            }

            spansLock.EnterWriteLock();
            try
            {
                spans.Clear();

                foreach (var boundWithClassificationString in boundsWithClassificationStrings)
                {
                    var bound = boundWithClassificationString.Item1;
                    var classificationString = boundWithClassificationString.Item2;

                    spans.Add(
                        new ClassificationSpan(
                            new SnapshotSpan(
                                results.ParsedSnapshot,
                                bound.Start,
                                bound.End - bound.Start + 1),
                            _classificationRegistry.GetClassificationType(classificationString)));
                }
            }
            finally
            {
                spansLock.ExitWriteLock();
            }

            EnforceSpansRefresh(results);
        }

        public void EnforceSpansRefresh(UDNParsingResults results)
        {
            if (ClassificationChanged != null)
            {
                ClassificationChanged(
                    this,
                    new ClassificationChangedEventArgs(
                        new SnapshotSpan(
                            results.ParsedSnapshot,
                            0,
                            results.ParsedSnapshot.Length)));
            }
        }
    }
}
