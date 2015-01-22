// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics;
using Microsoft.VisualStudio.Text;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    class LineProgress
    {
        private ITextSnapshotLine _snapshotLine;
        private string _lineText;
        private int _linePosition;
        private List<SnapshotSpan> _naturalTextSpans;
        private int naturalTextStart = -1;

        public State State { get; set; }

        public LineProgress(ITextSnapshotLine line, State state, List<SnapshotSpan> naturalTextSpans)
        {
            _snapshotLine = line;
            _lineText = line.GetText();
            _linePosition = 0;
            _naturalTextSpans = naturalTextSpans;

            State = state;
        }

        public bool EndOfLine
        {
            get { return _linePosition >= _snapshotLine.Length; }
        }

        public char Char()
        {
            return _lineText[_linePosition];
        }

        public char NextChar()
        {
            return _linePosition < _snapshotLine.Length - 1 ?
                _lineText[_linePosition + 1] :
                (char)0;
        }

        public char NextNextChar()
        {
            return _linePosition < _snapshotLine.Length - 2 ?
                _lineText[_linePosition + 2] :
                (char)0;
        }

        public void Advance(int count = 1)
        {
            _linePosition += count;
        }

        public void AdvanceToEndOfLine()
        {
            _linePosition = _snapshotLine.Length;
        }

        public void StartNaturalText()
        {
            Debug.Assert(naturalTextStart == -1, "Called StartNaturalText() twice without call to EndNaturalText()?");
            naturalTextStart = _linePosition;
        }

        public void EndNaturalText()
        {
            Debug.Assert(naturalTextStart != -1, "Called EndNaturalText() without StartNaturalText()?");
            if (_naturalTextSpans != null && _linePosition > naturalTextStart)
            {
                _naturalTextSpans.Add(new SnapshotSpan(_snapshotLine.Start + naturalTextStart, _linePosition - naturalTextStart));
            }
            naturalTextStart = -1;
        }
    }
}