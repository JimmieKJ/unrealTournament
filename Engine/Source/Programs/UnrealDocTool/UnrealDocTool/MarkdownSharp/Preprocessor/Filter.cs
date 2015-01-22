// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace MarkdownSharp.Preprocessor
{
    using System.Runtime.Serialization;
    using System.Text.RegularExpressions;

    [Serializable]
    public class FilterPredicateException : Exception
    {
        public FilterPredicateException()
            : this(null, null)
        {
        }

        public FilterPredicateException(string message)
            : this(message, null)
        {
        }

        public FilterPredicateException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        protected FilterPredicateException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }
    }

    public class Filter
    {
        public Filter()
        {
        }

        class FilterElement
        {
            public Func<string, bool> Predicate;
            public Regex Pattern;
        }

        public void Add(string name, Func<string, bool> predicate)
        {
            elements.Add(new FilterElement() { Pattern = GetPattern(name), Predicate = predicate });
        }

        private static Regex GetPattern(string name)
        {
            return
                new Regex(
                    @"(?<begining>(?<prefixIndent>[ \t]*)\[" + Regex.Escape(name) + @":(?<param>[^\]]+)\]\s*\n)(?<content>(.|\n)+?)(?<ending>\n\s*\[/" + Regex.Escape(name) + @"(:\k<param>)?\])(?<postfixNewLine>\n*)",
                    RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);
        }

        public string FilterOut(string text, TransformationData data, PreprocessedData preprocessedData)
        {
            var map = preprocessedData.TextMap;

            try
            {
                foreach (var element in elements)
                {
                    var changes = new List<PreprocessingTextChange>();

                    text = element.Pattern.Replace(text, m => FilterEvaluator(m, element.Predicate, changes));

                    Preprocessor.AddChangesToBounds(map, changes, preprocessedData, PreprocessedTextType.FilteredOut);

                    map.ApplyChanges(changes);
                }
            }
            catch (FilterPredicateException e)
            {
                data.ErrorList.Add(Markdown.GenerateError(
                    e.Message, MessageClass.Error, "", 0, data));
            }

            return text;
        }

        private string FilterEvaluator(Match match, Func<string, bool> predicate, List<PreprocessingTextChange> changes)
        {
            if (predicate(match.Groups["param"].Value))
            {
                changes.Add(PreprocessingTextChange.CreateRemove(match.Groups["begining"]));
                changes.Add(PreprocessingTextChange.CreateRemove(match.Groups["ending"]));
                return match.Groups["content"].Value + match.Groups["postfixNewLine"].Value;
            }
            
            changes.Add(new PreprocessingTextChange(match.Index, match.Length, 0));
            return "";
        }

        private readonly List<FilterElement> elements = new List<FilterElement>();
    }
}
