// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Text;

using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;

namespace MarkdownMode.AutoCompletion
{
    public class AutoCompletionSource : ICompletionSource
    {
        public ITextBuffer TextBuffer { get; private set; }
        public AutoCompletionSourceProvider SourceProvider { get; private set; }

        public AutoCompletionSource(ITextBuffer textBuffer, AutoCompletionSourceProvider sourceProvider)
        {
            this.TextBuffer = textBuffer;
            this.SourceProvider = sourceProvider;

            var catalog = new AggregateCatalog();
            catalog.Catalogs.Add(new AssemblyCatalog(typeof(AutoCompletionSource).Assembly));

            container = new CompositionContainer(catalog);

            container.ComposeParts(this);
        }

        public void Dispose()
        {

        }

        public void AugmentCompletionSession(ICompletionSession session, IList<CompletionSet> completionSets)
        {
            var span = GetLineSpan(this, session);

            foreach (var autoCompletion in autoCompletions)
            {
                autoCompletion.AugmentCompletionSession(span, completionSets);
            }
        }

        private static SnapshotSpan GetLineSpan(AutoCompletionSource source, ICompletionSession session)
        {
            var currentPoint = (session.TextView.Caret.Position.BufferPosition) - 1;
            var lineStart = source.TextBuffer.CurrentSnapshot.GetText().LastIndexOf('\n', currentPoint) + 1;

            return new SnapshotSpan(source.TextBuffer.CurrentSnapshot, lineStart, currentPoint - lineStart + 1);
        }

        // This had to be suppressed to avoid the warning as the C# compiler
        // does not detect MEF's ImportMany cases, which look for it as not
        // assigned fields.
        #pragma warning disable 0649
        [ImportMany(typeof(IAutoCompletion))]
        private IEnumerable<IAutoCompletion> autoCompletions;
        #pragma warning restore 0649

        private readonly CompositionContainer container;
    }
}
