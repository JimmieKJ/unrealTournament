// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Text;
using System.Collections;
using MarkdownSharp.EpicMarkdown;
using System.Threading;

namespace MarkdownMode.ToolWindow
{
    public class NavigateToComboData
    {
        private ReaderWriterLockSlim dataLock = new ReaderWriterLockSlim();

        public NavigateToComboData(IWpfTextView textView, IVsUIShell uiShell)
        {
            this.textView = textView;
            this.uiShell = uiShell;
            CurrentItem = 0;

            this.textView.Caret.PositionChanged += (sender, args) => SetSectionComboToCaretPosition();

            MarkdownSharp.Language.Loaded += () =>
                {
                    if (UDNDocRunningTableMonitor.CurrentUDNDocView != null)
                    {
                        RefreshComboItems(UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.Results);
                    }
                };
        }

        public string[] GetCurrentItemsSet()
        {
            dataLock.EnterReadLock();

            try
            {
                return comboElements.ToArray();
            }
            finally
            {
                dataLock.ExitReadLock();
            }
        }

        public string GetCurrentItemValue()
        {
            dataLock.EnterReadLock();
            try
            {
                return comboElements.Count == 0 ? "" : comboElements[CurrentItem];
            }
            finally
            {
                dataLock.ExitReadLock();
            }
        }

        public int CurrentItem { get; private set; }

        public int Count
        {
            get
            {
                dataLock.EnterReadLock();
                try
                {
                    return sections.Count;
                }
                finally
                {
                    dataLock.ExitReadLock();
                }
            }
        }

        internal ITrackingSpan GetSection(int selectedIndex)
        {
            dataLock.EnterReadLock();
            try
            {
                return sections[selectedIndex];
            }
            finally
            {
                dataLock.ExitReadLock();
            }
        }

        private void SetSectionComboToCaretPosition()
        {
            dataLock.EnterReadLock();
            try
            {
                CurrentItem = 0;

                for (var i = sections.Count - 1; i >= 0; i--)
                {
                    var span = sections[i].GetSpan(textView.TextSnapshot);

                    if (span.Contains(textView.Selection.Start.Position))
                    {
                        CurrentItem = i + 1;
                        break;
                    }
                }
            }
            finally
            {
                dataLock.ExitReadLock();
            }

            uiShell.UpdateCommandUI(1);
        }

        public void RefreshComboItems(UDNParsingResults results)
        {
            dataLock.EnterWriteLock();
            try
            {
                var blockInfos = GetHeaderBlocks(results);

                sections.Clear();
                comboElements.Clear();

                // First, add an item for "Top of the file"
                comboElements.Add(MarkdownSharp.Language.Message("TopOfFile"));

                foreach (var blockInfo in blockInfos)
                {
                    var header = blockInfo.BlockElement as EMHeader;

                    if (blockInfo.Span.Start < 0 || blockInfo.Span.Start >= results.ParsedSnapshot.Length || blockInfo.Span.End < 0
                        || blockInfo.Span.End >= results.ParsedSnapshot.Length)
                    {
                        continue;
                    }

                    var trackingSpan = results.ParsedSnapshot.CreateTrackingSpan(blockInfo.Span, SpanTrackingMode.EdgeExclusive);
                    var comboText = string.Format("{0}{1}", new string('-', header.Level - 1), header.Text);

                    if (trackingSpan == null)
                    {
                        continue;
                    }

                    sections.Add(trackingSpan);
                    comboElements.Add(comboText);
                }
            }
            finally
            {
                dataLock.ExitWriteLock();
            }

            SetSectionComboToCaretPosition();
        }

        private static IEnumerable<BlockInfo> GetHeaderBlocks(UDNParsingResults results)
        {
            var doc = results.Document;

            if (doc == null)
            {
                return new List<BlockInfo>();
            }

            var headers = doc.Data.GetElements<EMHeader>();

            if (headers.Count == 0)
            {
                return new List<BlockInfo>();
            }

            var blocks = new List<BlockInfo>();
            var lastHeaderStartAndElement = Tuple.Create<int, EMElement>(0, null);
            var text = doc.Text;

            foreach (var header in headers.Where(h => h.Document == doc))
            {
                var bounds = header.GetOriginalTextBounds();

                if (lastHeaderStartAndElement.Item2 != null)
                {
                    var lastNonWhitespace = -1;

                    for (var i = bounds.Start - 1; i >= 0; --i)
                    {
                        if (text[i] != ' ' && text[i] != '\n' && text[i] != '\r' && text[i] != '\t')
                        {
                            lastNonWhitespace = i;
                            break;
                        }
                    }

                    if (lastNonWhitespace != -1 && lastNonWhitespace < bounds.Start)
                    {
                        blocks.Add(
                            new BlockInfo(
                                lastHeaderStartAndElement.Item2,
                                new Span(lastHeaderStartAndElement.Item1, lastNonWhitespace - lastHeaderStartAndElement.Item1 + 1)));
                    }
                }

                lastHeaderStartAndElement = Tuple.Create(bounds.Start, header as EMElement);
            }

            blocks.Add(
                new BlockInfo(
                    lastHeaderStartAndElement.Item2,
                    new Span(
                        lastHeaderStartAndElement.Item1,
                        text.Length
                        - lastHeaderStartAndElement.Item1)));

            return blocks;
        }

        readonly IWpfTextView textView;
        readonly List<ITrackingSpan> sections = new List<ITrackingSpan>();
        readonly List<string> comboElements = new List<string>();
        readonly IVsUIShell uiShell;
    }

}
