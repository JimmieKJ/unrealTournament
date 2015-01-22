// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers
{
    using System.Text.RegularExpressions;

    public interface ITagMatch
    {
        int Index { get; }
        int Length { get; }
        string Text { get; }
    }

    public interface IStartOrEndTagMatch : ITagMatch
    {
        bool IsStart { get; }
    }

    public interface IStartTagMatch : IStartOrEndTagMatch
    {
        bool HasEnding { get; }
    }

    public interface IEndTagMatch : IStartOrEndTagMatch
    {
    }

    public class EMRawHTMLMatch : EMTaggedElementMatch
    {
        public EMRawHTMLMatch(int index, string text, string name, int contentStart, int contentLength, string attributesString)
            : base(index, text, contentStart, contentLength)
        {
            AttributesString = attributesString;
            Name = name;
        }

        public string AttributesString { get; private set; }
        public string Name { get; private set; }
    }

    public class EMMarkdownTaggedElementMatch : EMTaggedElementMatch
    {
        public EMMarkdownTaggedElementMatch(int index, string text, string name, int contentStart, int contentLength, string parameters)
            : base(index, text, contentStart, contentLength)
        {
            Parameters = parameters;
            Name = name;
        }

        public string Parameters { get; private set; }
        public string Name { get; private set; }
    }

    public class EMTaggedElementMatch : IMatch
    {
        public EMTaggedElementMatch(int index, string text, int contentStart, int contentLength)
        {
            Index = index;
            Text = text;
            ContentStart = contentStart;
            ContentLength = contentLength;
        }

        public int Index { get; private set; }
        public string Text { get; private set; }
        public int ContentStart { get; private set; }
        public int ContentLength { get; private set; }
        public string Content { get { return ContentLength == 0 ? "" : Text.Substring(ContentStart, ContentLength); } }
    }

    public abstract class TagMatch : ITagMatch
    {
        protected TagMatch(int index, string text)
        {
            Index = index;
            Text = text;
        }

        public int Index { get; private set; }

        public string Text { get; private set; }

        public int Length { get { return Text.Length; } }
    }

    public abstract class StartOrEndTagMatch : TagMatch, IEndTagMatch
    {
        protected StartOrEndTagMatch(int index, string text)
            : base(index, text)
        {
        }

        public virtual bool IsStart { get { return this is IStartTagMatch; } }
    }

    public abstract class EMTaggedElementsParser : IParser
    {
        private readonly bool ignoreNoClosingError;
        private readonly Func<int, Location> getLocationDelegate;

        protected EMTaggedElementsParser(bool ignoreNoClosingError = false, Func<int, Location> getLocationDelegate = null)
        {
            this.ignoreNoClosingError = ignoreNoClosingError;
            this.getLocationDelegate = getLocationDelegate;
        }

        public virtual IEnumerable<IMatch> TextMatches(string text)
        {
            var tags = GetTags(text);

            // return to speed-up
            if (tags.Count == 0)
            {
                return new List<IMatch>();
            }

            var sortedList = new SortedList<int, ITagMatch>();
            
            foreach (var tag in tags)
            {
                sortedList.Add(tag.Index, tag);
            }

            sortedList = DeleteOverlapping(sortedList);

            var matchedPairs = new List<Tuple<ITagMatch, ITagMatch>>();
            var startsStack = new Stack<ITagMatch>();

            foreach (var tag in sortedList.Select(pair => pair.Value))
            {
                if (tag is IStartOrEndTagMatch)
                {
                    var startOrEndTag = tag as IStartOrEndTagMatch;

                    if (startOrEndTag.IsStart)
                    {
                        var start = startOrEndTag as IStartTagMatch;
                        if (start.HasEnding)
                        {
                            startsStack.Push(start);
                        }
                        else if (startsStack.Count == 0)
                        {
                            matchedPairs.Add(new Tuple<ITagMatch, ITagMatch>(start, start));
                        }
                    }
                    else
                    {
                        if (startsStack.Count > 0)
                        {
                            ITagMatch start;
                            var end = startOrEndTag as IEndTagMatch;

                            if (ignoreNoClosingError)
                            {
                                start = NonstrickStartTagSearch(startsStack, end, text);

                                if(start == null)
                                {
                                    continue;
                                }
                            }
                            else
                            {
                                start = startsStack.Pop();

                                if (!VerifyMatch(text, start, end))
                                {
                                    throw CreateParsingErrorExceptionAtPosition(Language.Message("TagMismatch", GetErrorString(start), GetErrorString(end)), start.Index);
                                }
                            }

                            matchedPairs.Add(new Tuple<ITagMatch, ITagMatch>(start, end));
                        }
                        else if (!ignoreNoClosingError)
                        {
                            throw CreateParsingErrorExceptionAtPosition(Language.Message("RedundantEndingTag", GetErrorString(tag)), tag.Index);
                        }
                    }
                }
                else
                {
                    startsStack.Push(tag);
                }
            }

            // non-StartOrEnd tags parsing e.g. **bold**
            var nonStartEndStartsStack = new Stack<ITagMatch>();
            var potentialEnds = startsStack.Where(e => !(e is IStartOrEndTagMatch)).ToList();
            potentialEnds.Reverse();

            foreach (var potentialEnd in potentialEnds)
            {
                var start = NonstrickStartTagSearch(nonStartEndStartsStack, potentialEnd, text);

                if (start != null && nonStartEndStartsStack.Count == 0)
                {
                    matchedPairs.Add(new Tuple<ITagMatch, ITagMatch>(start, potentialEnd));
                }
                else
                {
                    nonStartEndStartsStack.Push(potentialEnd);
                }
            }

            if (!ignoreNoClosingError && startsStack.Count > 0)
            {
                var start = startsStack.Pop();

                throw CreateParsingErrorExceptionAtPosition(Language.Message("MissingEndingTag", GetErrorString(start)), start.Index);
            }

            return CreateLevelZeroElements(text, matchedPairs);
        }

        private EMParsingErrorException CreateParsingErrorExceptionAtPosition(string message, int position)
        {
            var line = 0;
            var column = 0;
            var path = "";

            if (getLocationDelegate != null)
            {
                var output = getLocationDelegate(position);

                path = output.Source;
                line = output.Line;
                column = output.Column;
            }

            return new EMParsingErrorException(message, path, line, column);
        }

        private string GetErrorString(ITagMatch tag)
        {
            if (getLocationDelegate != null)
            {
                var pos = getLocationDelegate(tag.Index);

                return string.Format("'{0}' at {1}:{2}", tag.Text, pos.Line, pos.Column);
            }

            return tag.Text;
        }

        private IEnumerable<IMatch> CreateLevelZeroElements(string text, List<Tuple<ITagMatch, ITagMatch>> matchedPairs)
        {
            var output = new List<EMTaggedElementMatch>();

            // sort by start index
            matchedPairs.Sort((a, b) => (a.Item1.Index < b.Item1.Index ? -1 : (a.Item1.Index == b.Item1.Index ? 0 : 1)));

            var ignoreUntil = -1;
            
            foreach(var pair in matchedPairs)
            {
                if (pair.Item1.Index <= ignoreUntil)
                {
                    continue;
                }

                var taggedMatch = (pair.Item1 == pair.Item2)
                        ? CreateNoEndElement(pair.Item1)
                        : CreateElement(text, pair.Item1, pair.Item2);

                // If tagged match is null, then it means it was ignored.
                if (taggedMatch == null)
                {
                    continue;
                }

                output.Add(taggedMatch);
                ignoreUntil = pair.Item2.Index;
            }

            return output;
        }

        private ITagMatch NonstrickStartTagSearch(Stack<ITagMatch> startsStack, ITagMatch end, string text)
        {
            ITagMatch start = null;

            foreach (var potentialStart in startsStack)
            {
                if (VerifyMatch(text, potentialStart, end))
                {
                    start = potentialStart;
                    break;
                }
            }

            if (start != null)
            {
                while (start != startsStack.Pop())
                {
                }
            }

            return start;
        }

        private SortedList<int, ITagMatch> DeleteOverlapping(SortedList<int, ITagMatch> sortedList)
        {
            var limit = 0;

            var output = new SortedList<int, ITagMatch>();

            foreach (var element in sortedList)
            {
                if (limit > element.Key)
                {
                    continue;
                }

                output.Add(element.Key, element.Value);
                limit = element.Value.Index + element.Value.Length;
            }

            return output;
        }

        public abstract EMElement Create(IMatch match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data);

        protected virtual bool VerifyMatch(string text, ITagMatch start, ITagMatch end)
        {
            // Matched by default.
            return true;
        }

        protected virtual EMTaggedElementMatch CreateElement(string text, ITagMatch start, ITagMatch end)
        {
            var originText = text.Substring(start.Index, end.Index - start.Index + end.Length);

            return new EMTaggedElementMatch(start.Index, originText, start.Length, end.Index - start.Index - start.Length);
        }

        protected virtual EMTaggedElementMatch CreateNoEndElement(ITagMatch start)
        {
            return new EMTaggedElementMatch(start.Index, start.Text, 0, 0);
        }

        protected abstract List<ITagMatch> GetTags(string text);
    }
}
