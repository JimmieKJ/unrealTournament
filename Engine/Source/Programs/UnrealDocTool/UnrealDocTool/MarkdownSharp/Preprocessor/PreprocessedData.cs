// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace MarkdownSharp.Preprocessor
{
    using MarkdownSharp.EpicMarkdown;

    public class PreprocessedData
    {
        public PreprocessedData(EMDocument document, ProcessedDocumentCache manager)
        {
            Excerpts = new ExcerptsManager(this);
            Variables = new VariableManager(this);
            Metadata = new MetadataManager(this);
            ReferenceLinks = new ReferenceLinksManager(this);
            TextMap = new PreprocessedTextLocationMap();
            PreprocessedTextBounds = new List<PreprocessedTextBound>();

            Document = document;

            this.manager = manager;
        }

        public ExcerptsManager Excerpts { get; private set; }
        public VariableManager Variables { get; private set; }
        public MetadataManager Metadata { get; private set; }
        public ReferenceLinksManager ReferenceLinks { get; private set; }
        public EMDocument Document { get; private set; }
        public PreprocessedTextLocationMap TextMap { get; private set; }
        public PreprocessedTextLocationMap ExcerptTextMap { get; set; }

        public List<PreprocessedTextBound> PreprocessedTextBounds { get; private set; }

        private readonly ProcessedDocumentCache manager;
    }
}
