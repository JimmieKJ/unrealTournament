// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.Preprocessor
{
    using System.Runtime.Serialization;
    using System.Text.RegularExpressions;

    public enum PositionRounding
    {
        Up,
        Down,
        None
    }

    public class PreprocessingTextChange
    {
        public PreprocessingTextChange(int index, int removeCharsCount, int addCharsCount)
        {
            Index = index;
            RemoveCharsCount = removeCharsCount;
            AddCharsCount = addCharsCount;
        }

        public int Index { get; private set; }
        public int RemoveCharsCount { get; private set; }
        public int AddCharsCount { get; private set; }

        public static string MatchEvaluatorWithTextChangeEmitWrapper(Match match,
            MatchEvaluator evaluator, List<PreprocessingTextChange> changes)
        {
            var output = evaluator(match);

            changes.Add(new PreprocessingTextChange(match.Index, match.Length, output.Length));

            return output;
        }

        public static string MatchEvaluatorWithTextChangeEmitWrapper(Match match,
            string replacement, List<PreprocessingTextChange> changes)
        {
            changes.Add(new PreprocessingTextChange(match.Index, match.Length, replacement.Length));

            return replacement;
        }

        public static PreprocessingTextChange CreateRemove(Capture capture)
        {
            return new PreprocessingTextChange(capture.Index, capture.Length, 0);
        }
    }

    class PreprocessedTextSubset
    {
        public int Index { get; set; }
        public int Length { get; set; }
        public int OriginalTextShift { get; set; }

        public override string ToString()
        {
            return string.Format("<{0}, {1}) {2}", Index, Index + Length, OriginalTextShift);
        }
    }

    public class PreprocessedTextLocationMap
    {
        private PreprocessedTextSubset[] textSubsets;

        public PreprocessedTextLocationMap()
        {
            textSubsets = new[] { new PreprocessedTextSubset() { Index = 0, Length = int.MaxValue / 2, OriginalTextShift = 0 } };
        }

        public PreprocessedTextLocationMap(PreprocessedTextLocationMap other)
        {
            var subsets = other.textSubsets;

            textSubsets = new PreprocessedTextSubset[subsets.Length];
            subsets.CopyTo(textSubsets, 0);
        }

        public void ApplyChanges(List<PreprocessingTextChange> changes)
        {
            if (changes.Count == 0)
            {
                return;
            }

            var newTextSubsets = ApplyRemoves(changes);
            ApplyShifts(changes, newTextSubsets);

            // if everything was fine then replace
            textSubsets = newTextSubsets.OrderBy(s => s.Index).ToArray();
        }

        private void ApplyShifts(List<PreprocessingTextChange> changes, PreprocessedTextSubset[] subsets)
        {
            var subsetId = 0;
            var summaryShift = 0;

            foreach (var change in changes)
            {
                while (subsetId < subsets.Length && subsets[subsetId].Index < change.Index)
                {
                    var offset = GetOffset(subsets[subsetId].Index) + summaryShift;
                    subsets[subsetId].OriginalTextShift = offset;
                    subsets[subsetId].Index += summaryShift;
                    subsetId++;
                }

                if (subsetId == subsets.Length)
                {
                    break;
                }

                summaryShift += change.AddCharsCount - change.RemoveCharsCount;
            }

            while (subsetId < subsets.Length)
            {
                var offset = GetOffset(subsets[subsetId].Index) + summaryShift;
                subsets[subsetId].OriginalTextShift = offset;
                subsets[subsetId].Index += summaryShift;
                subsetId++;
            }
        }

        private int GetOffset(int position)
        {
            return GetSubset(position).OriginalTextShift;
        }

        public int GetOriginalPosition(int position, PositionRounding rounding)
        {
            var subset = GetSubset(position);

            if (subset == null)
            {
                switch (rounding)
                {
                    case PositionRounding.None:
                        throw new UnableToDetectOriginalPositionException();

                    case PositionRounding.Up:
                        subset = GetClosestHigherSubset(position);

                        return subset == null ? int.MaxValue : subset.Index - 1 - subset.OriginalTextShift;

                    case PositionRounding.Down:
                        subset = GetClosestLowerSubset(position);

                        return subset == null ? 0 : subset.Index + subset.Length - subset.OriginalTextShift;

                    default:
                        throw new NotSupportedException("Unsupported original position rounding option.");
                }
            }

            return position - subset.OriginalTextShift;
        }

        class GetSubsetBinarySearchComparer : IComparer<PreprocessedTextSubset>
        {
            int position;

            public GetSubsetBinarySearchComparer(int position)
            {
                this.position = position;
            }

            public int Compare(PreprocessedTextSubset subset, int position)
            {
                if (subset.Index <= position && position < subset.Index + subset.Length)
                {
                    return 0;
                }

                return subset.Index - position;
            }

            public int Compare(PreprocessedTextSubset x, PreprocessedTextSubset y)
            {
                return x != null ? Compare(x, position) : -Compare(y, position);
            }
        }

        private PreprocessedTextSubset GetSubset(int position)
        {
            var index = Array.BinarySearch(textSubsets, null, new GetSubsetBinarySearchComparer(position));

            if (index < 0)
            {
                return null;
            }

            return textSubsets[index];
        }

        private PreprocessedTextSubset GetClosestLowerSubset(int position)
        {
            for (var subsetId = textSubsets.Length - 1; subsetId >= 0; --subsetId)
            {
                var subset = textSubsets[subsetId];

                if (position >= subset.Index + subset.Length)
                {
                    return subset;
                }
            }

            return null;
        }

        private PreprocessedTextSubset GetClosestHigherSubset(int position)
        {
            for (var subsetId = 0; subsetId < textSubsets.Length; ++subsetId)
            {
                var subset = textSubsets[subsetId];

                if (position < subset.Index)
                {
                    return subset;
                }
            }

            return null;
        }

        private PreprocessedTextSubset[] ApplyRemoves(List<PreprocessingTextChange> changes)
        {
            // get intervals from subsets
            var intervals =
                textSubsets.Select(
                    subset =>
                    new Interval() { Start = subset.Index, End = subset.Index + subset.Length })
                           .ToList();

            // get remove intervals from changes
            var removeIntervals =
                changes.Select(
                    change => new Interval() { Start = change.Index, End = change.Index + change.RemoveCharsCount })
                       .ToList();

            return
                IntervalSet.Remove(new IntervalSet(intervals), new IntervalSet(removeIntervals))
                           .Set.Select(
                               interval =>
                               new PreprocessedTextSubset()
                                   {
                                       Index = interval.Start,
                                       Length = interval.End - interval.Start
                                   })
                           .ToArray();
        }
    }

    [Serializable]
    public class UnableToDetectOriginalPositionException : Exception
    {
        public UnableToDetectOriginalPositionException()
            : this(null)
        {
        }

        public UnableToDetectOriginalPositionException(string message)
            : this(message, null)
        {
        }

        public UnableToDetectOriginalPositionException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        protected UnableToDetectOriginalPositionException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }
    }
}
