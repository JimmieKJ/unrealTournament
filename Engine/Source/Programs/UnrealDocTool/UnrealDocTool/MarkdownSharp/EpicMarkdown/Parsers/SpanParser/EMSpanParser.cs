// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown.Parsers.SpanParser
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text.RegularExpressions;

    public class EMSpanParser : EMTaggedElementsParser
    {
        private class EMBracketedImagesAndLinksMatch : EMTaggedElementMatch
        {
            public string Parameters { get; private set; }

            public EMBracketedImagesAndLinksMatch(EMTaggedElementMatch baseMatch, string parameters)
                : base(baseMatch.Index, baseMatch.Text, baseMatch.ContentStart, baseMatch.ContentLength)
            {
                Parameters = parameters;
            }
        }

        private class EMDecorationMatch : EMTaggedElementMatch
        {
            public EMDecorationType Type { get; private set; }

            public EMDecorationMatch(EMTaggedElementMatch match, EMDecorationType type)
                : base(match.Index, match.Text, match.ContentStart, match.ContentLength)
            {
                Type = type;
            }
        }

        public EMSpanParser(Func<int, Location> getLocationDelegate)
            : base(true, getLocationDelegate)
        {

        }

        public override EMElement Create(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            if (match is EMBracketedImagesAndLinksMatch)
            {
                return CreateBracketed(match, doc, origin, parent, data);
            }

            if (match is EMDecorationMatch)
            {
                return CreateDecoration(match, doc, origin, parent, data);
            }

            throw new InvalidOperationException("Should not happend!");
        }

        private static EMElement CreateDecoration(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var decorationMatch = match as EMDecorationMatch;

            var element = new EMDecorationElement(doc, origin, parent, decorationMatch.Type);
            element.Content.Parse(decorationMatch.ContentStart, decorationMatch.Content, data);

            return element;
        }

        private static EMElement CreateBracketed(
            IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var taggedMatch = match as EMBracketedImagesAndLinksMatch;

            // Check if image.
            if (taggedMatch.Text.StartsWith("!") && !string.IsNullOrWhiteSpace(taggedMatch.Parameters))
            {
                EMElement outElement = null;

                var fragments = new List<TextFragment>() { new TextFragment(origin.Start, match.Text) };

                EMParsingHelper.ParseElements(
                    data, doc, parent, fragments, EMImage.GetParser(), element => { outElement = element; });

                if (outElement == null)
                {
                    string imageError = "Could not find image in given text: " + origin.Text;
                    throw new InvalidOperationException(imageError);
                }

                return outElement;
            }

            // Check if shortcut ref
            if (string.IsNullOrWhiteSpace(taggedMatch.Parameters))
            {
                return EMLink.CreateFromAnchorRefShortcut(
                    EMLink.AnchorRefShortcut.Match(taggedMatch.Text), doc, origin, parent, data);
            }

            // If not an image and not a shortcut link, then must have been other link.

            // Parse span content for internal elements.
            var span = new EMSpanElements(doc, origin, parent);
            span.Parse(taggedMatch.ContentStart, taggedMatch.Content, data);

            // Check if ref link
            return taggedMatch.Parameters.StartsWith("[")
                       ? EMLink.CreateFromAnchorRef(doc, origin, parent, data, span, taggedMatch.Parameters)
                       : EMLink.CreateFromInline(doc, origin, parent, data, span, taggedMatch.Parameters);
        }

        protected override EMTaggedElementMatch CreateNoEndElement(ITagMatch start)
        {
            throw new NotSupportedException("Bracketed images, links or decorations have to be ended.");
        }

        protected override EMTaggedElementMatch CreateElement(string text, ITagMatch start, ITagMatch end)
        {
            var tagged = base.CreateElement(text, start, end);

            if (start is EMBracketedStartOrEndTag)
            {
                return new EMBracketedImagesAndLinksMatch(tagged, (end as EMBracketedEnd).Params);
            }

            if (start is EMDecorationTag)
            {
                return new EMDecorationMatch(tagged, (start as EMDecorationTag).Type);
            }

            throw new InvalidOperationException("Should not happend!");
        }

        protected override bool VerifyMatch(string text, ITagMatch start, ITagMatch end)
        {
            if (start is EMBracketedStart && end is EMBracketedEnd)
            {
                if (end.Text == "]")
                {
                    return text.IndexOf(" ", start.Index, end.Index - start.Index) == -1;
                }

                return true;
            }

            if (start is EMDecorationTag && end is EMDecorationTag)
            {
                var decorStart = start as EMDecorationTag;
                var decorEnd = end as EMDecorationTag;

                if (decorStart.Type != decorEnd.Type || decorStart.Text[0] != decorEnd.Text[0])
                {
                    return false;
                }

                return true;
            }

            return false;
        }

        protected override List<ITagMatch> GetTags(string text)
        {
            var output = new List<ITagMatch>();

            EMBracketedStartOrEndTag.Append(output, text);
            EMDecorationTag.Append(output, text);

            return output;
        }
    }
}
