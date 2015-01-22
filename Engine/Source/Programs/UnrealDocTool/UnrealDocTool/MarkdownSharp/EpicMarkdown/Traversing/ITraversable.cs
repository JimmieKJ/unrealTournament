// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown.Traversing
{
    public interface ITraversable
    {
        void ForEachWithContext<TContext>(TContext context) where TContext : ITraversingContext;
    }
}
