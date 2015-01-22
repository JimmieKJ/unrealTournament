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
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;
using SpellChecker.Definitions;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    /// <summary>
    /// Provides tags for text files.
    /// </summary>
    internal class PlainTextTagger : ITagger<NaturalTextTag>
    {
        #region Private Fields
        private ITextBuffer _buffer;
        #endregion

        #region MEF Imports / Exports
        [Export(typeof(ITaggerProvider))]
        [ContentType("plaintext")]
        [TagType(typeof(NaturalTextTag))]
        internal class PlainTextTaggerProvider : ITaggerProvider
        {
            public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
            {
                if (buffer == null)
                {
                    throw new ArgumentNullException("buffer");
                }

                return new PlainTextTagger(buffer) as ITagger<T>;
            }
        }
        #endregion

        #region Constructor
        /// <summary>
        /// Constructor for Natural Text Tagger.
        /// </summary>
        /// <param name="buffer">Relevant buffer.</param>
        public PlainTextTagger(ITextBuffer buffer)
        {
            _buffer = buffer;
        }
        #endregion

        #region ITagger<INaturalTextTag> Members
        public IEnumerable<ITagSpan<NaturalTextTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            foreach (var snapshotSpan in spans)
            {
                yield return new TagSpan<NaturalTextTag>(
                        snapshotSpan,
                        new NaturalTextTag()
                        );

            }
        }

#pragma warning disable 67
        public event EventHandler<SnapshotSpanEventArgs> TagsChanged;
#pragma warning restore 67
        #endregion
    }

    class NaturalTextTag : INaturalTextTag
    {
    }
}
