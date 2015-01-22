// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    public abstract class EMElementWithElements : EMElement
    {
        public EMElements Elements { get; private set; }

        protected EMElementWithElements(EMDocument doc, EMElementOrigin origin, EMElement parent, int elementsOriginOffset, int elementsOriginLength)
            : base(doc, origin, parent)
        {
            Elements = new EMElements(doc, new EMElementOrigin(elementsOriginOffset, origin.Text.Substring(elementsOriginOffset, elementsOriginLength)), this);
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            Elements.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }
    }
}
