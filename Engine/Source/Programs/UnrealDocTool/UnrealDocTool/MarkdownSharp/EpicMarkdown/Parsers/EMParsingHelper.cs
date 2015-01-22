// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Text.RegularExpressions;

    using MarkdownSharp.Preprocessor;

    public interface ITextFragment
    {
        int Start { get; }
        int End { get; }
    }

    public class TextFragment : ITextFragment
    {
        public TextFragment(int start, string text)
        {
            Text = text;
            Start = start;
        }

        public string Text { get; private set; }
        public int Start { get; private set; }
        public int End { get { return Start + Text.Length - 1; } }

        public TextFragment Subfragment(int start, int end)
        {
            var localStart = start - Start;
            var length = end - start + 1;

            return new TextFragment(start, Text.Substring(localStart, length));
        }
    }

    public static class EMParsingHelper
    {
        public static void ParseElements(
            TransformationData data,
            EMDocument doc,
            EMElement parent,
            List<TextFragment> fragments,
            List<IParser> parsers,
            Action<EMElement> elementEmitter)
        {
            foreach (var parser in parsers)
            {
                ParseElements(data, doc, parent, fragments, parser, elementEmitter);
            }
        }

        public static void ParseElements(
            TransformationData data,
            EMDocument doc,
            EMElement parent,
            string text,
            List<IParser> parsers,
            Action<EMElement> elementEmitter)
        {
            var fragments = new List<TextFragment> { new TextFragment(0, text) };

            ParseElements(data, doc, parent, fragments, parsers, elementEmitter);
        }

        public static void ParseElements(
            TransformationData data,
            EMDocument doc,
            EMElement parent,
            string text,
            IParser parser,
            Action<EMElement> elementEmitter)
        {
            var fragments = new List<TextFragment> { new TextFragment(0, text) };

            ParseElements(data, doc, parent, fragments, parser, elementEmitter);
        }

        public static void ParseElements(TransformationData data, EMDocument doc, EMElement parent, List<TextFragment> fragments, IParser parser, Action<EMElement> elementEmitter)
        {
            var newFragments = new List<TextFragment>();

            foreach (var fragment in fragments)
            {
                var origins = new List<ITextFragment>();

                try
                {
                    foreach (var match in parser.TextMatches(fragment.Text))
                    {
                        var elementOrigin = new EMElementOrigin(fragment.Start + match.Index, match.Text);

                        try
                        {
                            elementEmitter(parser.Create(match, doc, elementOrigin, parent, data));
                            origins.Add(elementOrigin);
                        }
                        catch (EMSkipParsingException)
                        {

                        }
                    }

                    newFragments.AddRange(SplitFragment(fragment, origins));
                }
                catch (EMParsingErrorException e)
                {
                    data.ErrorList.Add(new ErrorDetail(e.Message, MessageClass.Error, "", "", e.Line, e.Column, e.Path != data.Document.LocalPath ? e.Path : null));
                }
            }

            fragments.Clear();
            fragments.AddRange(newFragments);
        }

        private static IEnumerable<TextFragment> SplitFragment(TextFragment fragment, List<ITextFragment> processedFragments)
        {
            if (processedFragments.Count == 0)
            {
                return new List<TextFragment> { fragment };
            }

            var fragmentSet =
                new IntervalSet(
                    new List<Interval>() { new Interval() { Start = fragment.Start, End = fragment.End + 1 } });

            var processedFragmentsSet = new IntervalSet(processedFragments.Select(fr => new Interval() { Start = fr.Start, End = fr.End + 1 }).ToList());

            var outputIntervals = IntervalSet.Remove(fragmentSet, processedFragmentsSet);

            return outputIntervals.Set.Select(
                i => new TextFragment(i.Start, fragment.Text.Substring(i.Start - fragment.Start, i.End - i.Start))).ToList();
        }

        public static void Cutout(List<TextFragment> fragments, Regex cutoutPattern)
        {
            var newFragments = new List<TextFragment>();

            foreach (var fragment in fragments)
            {
                var cutoutFragments =
                    (from Match m in cutoutPattern.Matches(fragment.Text)
                     select new TextFragment(m.Index, m.Value) as ITextFragment).ToList();

                newFragments.AddRange(SplitFragment(fragment, cutoutFragments));
            }

            fragments.Clear();
            fragments.AddRange(newFragments);
        }
    }

    // exception for skipping current element parsing
    [Serializable]
    public class EMSkipParsingException : Exception
    {
        public EMSkipParsingException()
            : base("Skipping parsing of an element.")
        {
            
        }
    }
}
