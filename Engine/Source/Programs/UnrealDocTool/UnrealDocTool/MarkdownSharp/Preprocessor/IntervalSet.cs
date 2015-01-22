// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace MarkdownSharp.Preprocessor
{
    public struct Interval
    {
        public int Start;
        public int End;

        public override string ToString()
        {
            return string.Format("<{0}, {1})", Start, End);
        }
    }

    public class IntervalSet
    {
        public List<Interval> Set { get; private set; }

        public IntervalSet(List<Interval> intervals)
        {
            Set = intervals;
        }

        public void Invert()
        {
            var newSet = new List<Interval>();
            var start = int.MinValue;

            foreach (var i in Set)
            {
                var end = i.Start;

                if (start != end)
                {
                    newSet.Add(new Interval() { Start = start, End = end });
                }

                start = i.End;
            }

            if (start != int.MaxValue)
            {
                newSet.Add(new Interval() { Start = start, End = int.MaxValue });
            }

            Set = newSet;
        }

        public static IntervalSet Remove(IntervalSet a, IntervalSet b)
        {
            return IntersectWith(a, b, (inA, inB) => inA && !inB);
        }

        public static IntervalSet Intersection(IntervalSet a, IntervalSet b)
        {
            return IntersectWith(a, b, (inA, inB) => inA && inB);
        }

        public static IntervalSet Union(IntervalSet a, IntervalSet b)
        {
            return IntersectWith(a, b, (inA, inB) => inA || inB);
        }

        struct BoundOwner
        {
            public bool A;
            public bool B;
        }

        static void AddToSortedBounds(SortedList<int, BoundOwner> sortedBounds, int position, bool setA)
        {
            BoundOwner value;

            if (sortedBounds.TryGetValue(position, out value))
            {
                value.A ^= setA;
                value.B ^= !setA;

                sortedBounds[position] = value;
            }
            else
            {
                value.A = setA;
                value.B = !setA;

                sortedBounds.Add(position, value);
            }
        }

        // Note that this algorithm won't merge subsequent result subsets.
        // E.g. result may look like [<2, 3); <3, 5)]. This property is known
        // and is used by Remove sets operation in ProcessedTextLocationMap.
        // In case of refactoring/rewriting this function, please consider
        // modifing new sets generation algorithm in location map.
        private static IntervalSet IntersectWith(IntervalSet a, IntervalSet b, Func<bool, bool, bool> resultSetPredicate)
        {
            var sortedBounds = new SortedList<int, BoundOwner>();

            foreach (var i in a.Set)
            {
                AddToSortedBounds(sortedBounds, i.Start, true);
                AddToSortedBounds(sortedBounds, i.End, true);
            }

            foreach (var i in b.Set)
            {
                AddToSortedBounds(sortedBounds, i.Start, false);
                AddToSortedBounds(sortedBounds, i.End, false);
            }

            var inA = false;
            var inB = false;
            var inResult = false;
            var start = 0;

            var cList = new List<Interval>();

            foreach (var sortedBound in sortedBounds)
            {
                if (inResult && start != sortedBound.Key)
                {
                    cList.Add(new Interval() { Start = start, End = sortedBound.Key });
                    inResult = false;
                }

                if (sortedBound.Value.A)
                {
                    inA = !inA;
                }

                if (sortedBound.Value.B)
                {
                    inB = !inB;
                }

                if (!inResult && resultSetPredicate(inA, inB))
                {
                    start = sortedBound.Key;
                    inResult = true;
                }
            }

            return new IntervalSet(cList);
        }
    }
}
