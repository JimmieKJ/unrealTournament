// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Traversing
{
    public class RenderingContext : ITraversingContext
    {
        private readonly Action<ITraversable, StringBuilder, Stack<EMInclude>> renderer;

        public RenderingContext(Action<ITraversable, StringBuilder, Stack<EMInclude>> renderer)
        {
            this.renderer = renderer;
        }

        private readonly Stack<EMInclude> includesStack = new Stack<EMInclude>();
        private readonly StringBuilder builder = new StringBuilder();

        public void PreChildrenVisit(ITraversable traversedElement)
        {
            if (traversedElement is EMInclude)
            {
                includesStack.Push(traversedElement as EMInclude);
            }
        }

        public void PostChildrenVisit(ITraversable traversedElement)
        {
            renderer(traversedElement, builder, includesStack);

            if (traversedElement is EMInclude)
            {
                includesStack.Pop();
            }
        }

        public override string ToString()
        {
            return builder.ToString();
        }
    }
}
