// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

using Microsoft.VisualStudio.Language.Intellisense;

namespace MarkdownMode.AutoCompletion
{
    using Microsoft.VisualStudio.Text;

    public interface IAutoCompletion
    {
        void AugmentCompletionSession(
            SnapshotSpan span, IList<CompletionSet> completionSets);
    }
}
