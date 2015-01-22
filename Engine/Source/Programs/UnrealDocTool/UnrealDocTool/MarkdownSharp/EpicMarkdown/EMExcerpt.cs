// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Collections.Generic;

    using MarkdownSharp.Preprocessor;

    public class EMExcerpt : EMElement
    {
        public bool IsDocumentExcerpt { get; private set; }

        private Func<EMElements> lazyLoadingFunction;

        public EMExcerpt(EMDocument doc, int originPosition, int contentStart, string content, TransformationData data, bool isDocumentExcerpt)
            : base(doc, new EMElementOrigin(originPosition + contentStart, content), null)
        {
            this.IsDocumentExcerpt = isDocumentExcerpt;
            lazyLoadingFunction = () => ExcerptLazyParser(doc, this, content, data);
        }

        public static EMElements ExcerptLazyParser(EMDocument doc, EMElement parent, string content, TransformationData data)
        {
            content = Preprocessor.CutoutComments(content, null);

            var elements = new EMElements(doc, new EMElementOrigin(0, content), parent);

            var previousCurrentFolder = data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf;
            var newCurrentFolder = doc.GetAbsoluteMarkdownPath();

            data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf = newCurrentFolder;

            elements.Parse(content, data);

            data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf = previousCurrentFolder;

            return elements;
        }

        public override void AppendHTML(System.Text.StringBuilder builder, System.Collections.Generic.Stack<EMInclude> includesStack, TransformationData data)
        {
            // write nothing
        }

        private EMElements content;
        public EMElements Content
        {
            get
            {
                if (content == null)
                {
                    content = lazyLoadingFunction();
                    lazyLoadingFunction = null;
                }

                return content;
            }
        }

        public override string ToString()
        {
            return Content.ToString();
        }
    }
}
