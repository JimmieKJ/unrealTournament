// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using MarkdownSharp.EpicMarkdown.Traversing;
    using MarkdownSharp.Preprocessor;

    public abstract class EMElement : ITraversable, IHTMLRenderable
    {
        protected EMElement(EMDocument doc, EMElementOrigin origin, EMElement parent = null)
        {
            Origin = origin;
            Parent = parent;
            Document = doc;

            Messages = new List<EMReadingMessage>();
        }

        public EMElementOrigin Origin { get; private set; }
        public EMElement Parent { get; private set; }
        public EMDocument Document { get; private set; }

        public virtual Interval GetPreprocessedTextBounds()
        {
            Interval output;

            output.Start = Origin.GetNonWhitespaceStart() + Origin.Start;
            output.End = Origin.GetNonWhitespaceEnd() + Origin.Start;

            if (Parent != null)
            {
                output.Start = Parent.GetOriginalPosition(output.Start, PositionRounding.Down);
                output.End = Parent.GetOriginalPosition(output.End, PositionRounding.Up);
            }

            return output;
        }

        public virtual int GetOriginalPosition(int position, PositionRounding rounding)
        {
            if (Parent != null)
            {
                var originalPosition = Parent.GetOriginalPosition(position + Origin.Start, rounding);

                return originalPosition;
            }

            return position + Origin.Start;
        }

        public void ReplaceWith(EMElement element)
        {
            if (Parent == null)
            {
                throw new InvalidOperationException("Unable to replace, because there is no parent.");
            }

            if (!(Parent is EMContentElement))
            {
                throw new NotSupportedException("Not supported parent type.");
            }

            var contentElement = Parent as EMContentElement;
            var foundKey = -1;

            foreach (var pair in contentElement.Elements)
            {
                if (pair.Value != this)
                {
                    continue;
                }

                foundKey = pair.Key;
                break;
            }

            contentElement.Elements.Remove(foundKey);
            contentElement.Add(element);
        }

        protected bool InExcerpt()
        {
            var current = this;

            while (current != null)
            {
                if (current is EMExcerpt)
                {
                    return true;
                }

                current = current.Parent;
            }

            return false;
        }

        public void AddMessage(EMReadingMessage message, TransformationData data)
        {
            if (message.MessageClass == MessageClass.Error || message.MessageClass == MessageClass.Warning)
            {
                HadReadingProblem = true;

                ErrorId = data.ErrorList.Count;

                data.ErrorList.Add(Markdown.GenerateError(
                    message.Message, message.MessageClass, Origin.Text, ErrorId, data));
            }

            Messages.Add(message);
        }

        public List<EMReadingMessage> Messages { get; private set; }
        public bool HadReadingProblem { get; private set; }
        public int ErrorId { get; protected set; }

        public virtual void ForEachWithContext<T>(T context) where T : ITraversingContext
        {
            context.PreChildrenVisit(this);
            context.PostChildrenVisit(this);
        }

        class SimpleForEachContext : ITraversingContext
        {
            private readonly Action<EMElement> action;
            private readonly int maxLevel;
            private readonly int minLevel;
            private int level;

            public SimpleForEachContext(Action<EMElement> action, int minLevel = 0, int maxLevel = int.MaxValue)
            {
                this.action = action;
                this.maxLevel = maxLevel;
                this.minLevel = minLevel;
            }

            public void PreChildrenVisit(ITraversable traversedElement)
            {
                ++level;
            }

            public void PostChildrenVisit(ITraversable traversedElement)
            {
                --level;
                if (level >= minLevel && level <= maxLevel && traversedElement is EMElement)
                {
                    action(traversedElement as EMElement);
                }
            }
        }

        public void ForEach(Action<EMElement> action, int minLevel = 0, int maxLevel = int.MaxValue)
        {
            ForEachWithContext(new SimpleForEachContext(action, minLevel, maxLevel));
        }

        public string Render(Action<ITraversable, StringBuilder, Stack<EMInclude>> renderingAction)
        {
            var rc =
                new RenderingContext(renderingAction);
            ForEachWithContext(rc);

            return rc.ToString();
        }

        public abstract void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data);

        public Interval GetOriginalTextBounds()
        {
            var bounds = GetPreprocessedTextBounds();

            Interval output;

            output.Start = Document.TextMap.GetOriginalPosition(bounds.Start, PositionRounding.Down);
            output.End = Document.TextMap.GetOriginalPosition(bounds.End, PositionRounding.Up);

            return output;
        }

        public virtual string GetClassificationString()
        {
            // no classification
            return null;
        }
    }
}
