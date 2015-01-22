// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

template <class ContainerType, class ElementType>
class TLinkedListIterator
{
public:
	explicit TLinkedListIterator(ContainerType* FirstLink)
		: CurrentLink(FirstLink)
	{
	}

	/**
		* Advances the iterator to the next element.
		*/
	void Next()
	{
		checkSlow(CurrentLink);
		CurrentLink = CurrentLink->GetNextLink();
	}

	TLinkedListIterator& operator++()
	{
		Next();
		return *this;
	}

	TLinkedListIterator operator++(int)
	{
		auto Tmp = *this;
		Next();
		return Tmp;
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return CurrentLink != nullptr;
	}

	// Accessors.
	ElementType& operator->() const
	{
		checkSlow(CurrentLink);
		return **CurrentLink;
	}

	ElementType& operator*() const
	{
		checkSlow(CurrentLink);
		return **CurrentLink;
	}

private:
	ContainerType* CurrentLink;

	friend bool operator==(const TLinkedListIterator& Lhs, const TLinkedListIterator& Rhs) { return Lhs.CurrentLink == Rhs.CurrentLink; }
	friend bool operator!=(const TLinkedListIterator& Lhs, const TLinkedListIterator& Rhs) { return Lhs.CurrentLink != Rhs.CurrentLink; }
};

/**
 * Encapsulates a link in a single linked list with constant access time.
 */
template <class ElementType>
class TLinkedList
{
public:
	/**
	 * Used to iterate over the elements of a linked list.
	 */
	typedef TLinkedListIterator<TLinkedList,       ElementType> TIterator;
	typedef TLinkedListIterator<TLinkedList, const ElementType> TConstIterator;

	// Constructors.
	TLinkedList()
		: NextLink(nullptr)
		, PrevLink(nullptr)
	{
	}

	explicit TLinkedList(const ElementType& InElement)
		: Element (InElement)
		, NextLink(nullptr)
		, PrevLink(nullptr)
	{
	}

	/**
	 * Removes this element from the list in constant time.
	 *
	 * This function is safe to call even if the element is not linked.
	 */
	void Unlink()
	{
		if( NextLink )
		{
			NextLink->PrevLink = PrevLink;
		}
		if( PrevLink )
		{
			*PrevLink = NextLink;
		}
		// Make it safe to call Unlink again.
		NextLink = NULL;
		PrevLink = NULL;
	}

	/**
	 * Adds this element to a list, before the given element.
	 *
	 * @param Before The link to insert this element before.
	 */
	void Link(TLinkedList*& Before)
	{
		if(Before)
		{
			Before->PrevLink = &NextLink;
		}
		NextLink = Before;
		PrevLink = &Before;
		Before = this;
	}

	/**
	 * Returns whether element is currently linked.
	 *
	 * @return true if currently linked, false otherwise
	 */
	bool IsLinked()
	{
		return PrevLink != NULL;
	}

	TLinkedList* GetPrevLink() const
	{
		return PrevLink;
	}

	TLinkedList* GetNextLink() const
	{
		return NextLink;
	}

	// Accessors.
	ElementType* operator->()
	{
		return &Element;
	}
	const ElementType* operator->() const
	{
		return &Element;
	}
	ElementType& operator*()
	{
		return Element;
	}
	const ElementType& operator*() const
	{
		return Element;
	}
	TLinkedList* Next()
	{
		return NextLink;
	}

private:
	ElementType   Element;
	TLinkedList*  NextLink;
	TLinkedList** PrevLink;

	friend TIterator      begin(      TLinkedList& List) { return TIterator     (&List); }
	friend TConstIterator begin(const TLinkedList& List) { return TConstIterator(const_cast<TLinkedList*>(&List)); }
	friend TIterator      end  (      TLinkedList& List) { return TIterator     (nullptr); }
	friend TConstIterator end  (const TLinkedList& List) { return TConstIterator(nullptr); }
};


template <class NodeType, class ElementType>
class TDoubleLinkedListIterator
{
public:
	explicit TDoubleLinkedListIterator(NodeType* StartingNode)
		: CurrentNode(StartingNode)
	{
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return CurrentNode != nullptr; 
	}

	TDoubleLinkedListIterator& operator++()
	{
		checkSlow(CurrentNode);
		CurrentNode = CurrentNode->GetNextNode();
		return *this;
	}

	TDoubleLinkedListIterator operator++(int)
	{
		auto Tmp = *this;
		++(*this);
		return Tmp;
	}

	TDoubleLinkedListIterator& operator--()
	{
		checkSlow(CurrentNode);
		CurrentNode = CurrentNode->GetPrevNode();
		return *this;
	}

	TDoubleLinkedListIterator operator--(int)
	{
		auto Tmp = *this;
		--(*this);
		return Tmp;
	}

	// Accessors.
	ElementType& operator->() const
	{
		checkSlow(CurrentNode);
		return CurrentNode->GetValue();
	}

	ElementType& operator*() const
	{
		checkSlow(CurrentNode);
		return CurrentNode->GetValue();
	}

	NodeType* GetNode() const
	{
		checkSlow(CurrentNode);
		return CurrentNode;
	}

private:
	NodeType* CurrentNode;

	friend bool operator==(const TDoubleLinkedListIterator& Lhs, const TDoubleLinkedListIterator& Rhs) { return Lhs.CurrentNode == Rhs.CurrentNode; }
	friend bool operator!=(const TDoubleLinkedListIterator& Lhs, const TDoubleLinkedListIterator& Rhs) { return Lhs.CurrentNode != Rhs.CurrentNode; }
};

/**
 * Double linked list.
 */
template <class ElementType>
class TDoubleLinkedList
{
public:
	class TDoubleLinkedListNode
	{
	public:
		friend class TDoubleLinkedList;

		/** Constructor */
		TDoubleLinkedListNode( const ElementType& InValue )
		: Value(InValue), NextNode(NULL), PrevNode(NULL)
		{
		}

		const ElementType& GetValue() const
		{
			return Value;
		}

		ElementType& GetValue()
		{
			return Value;
		}

		TDoubleLinkedListNode* GetNextNode()
		{
			return NextNode;
		}

		TDoubleLinkedListNode* GetPrevNode()
		{
			return PrevNode;
		}

	protected:
		ElementType            Value;
		TDoubleLinkedListNode* NextNode;
		TDoubleLinkedListNode* PrevNode;
	};

	/**
	 * Used to iterate over the elements of a linked list.
	 */
	typedef TDoubleLinkedListIterator<TDoubleLinkedListNode,       ElementType> TIterator;
	typedef TDoubleLinkedListIterator<TDoubleLinkedListNode, const ElementType> TConstIterator;

	/** Constructors. */
	TDoubleLinkedList()
		: HeadNode(nullptr)
		, TailNode(nullptr)
		, ListSize(0)
	{
	}

	/** Destructor */
	virtual ~TDoubleLinkedList()
	{
		Empty();
	}

	// Adding/Removing methods
	/**
	 * Add the specified value to the beginning of the list, making that value the new head of the list.
	 *
	 * @param	InElement	the value to add to the list.
	 *
	 * @return	whether the node was successfully added into the list
	 */
	bool AddHead( const ElementType& InElement )
	{
		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return false;
		}

		// have an existing head node - change the head node to point to this one
		if ( HeadNode != NULL )
		{
			NewNode->NextNode = HeadNode;
			HeadNode->PrevNode = NewNode;
			HeadNode = NewNode;
		}
		else
		{
			HeadNode = TailNode = NewNode;
		}

		SetListSize(ListSize + 1);
		return true;
	}

	/**
	 * Append the specified value to the end of the list
	 *
	 * @param	InElement	the value to add to the list.
	 *
	 * @return	whether the node was successfully added into the list
	 */
	bool AddTail( const ElementType& InElement )
	{
		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return false;
		}

		if ( TailNode != NULL )
		{
			TailNode->NextNode = NewNode;
			NewNode->PrevNode = TailNode;
			TailNode = NewNode;
		}
		else
		{
			HeadNode = TailNode = NewNode;
		}

		SetListSize(ListSize + 1);
		return true;
	}

	/**
	 * Insert the specified value into the list at an arbitrary point.
	 *
	 * @param	InElement			the value to insert into the list
	 * @param	NodeToInsertBefore	the new node will be inserted into the list at the current location of this node
	 *								if NULL, the new node will become the new head of the list
	 *
	 * @return	whether the node was successfully added into the list
	 */
	bool InsertNode( const ElementType& InElement, TDoubleLinkedListNode* NodeToInsertBefore=NULL )
	{
		if ( NodeToInsertBefore == NULL || NodeToInsertBefore == HeadNode )
		{
			return AddHead(InElement);
		}

		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return false;
		}

		NewNode->PrevNode = NodeToInsertBefore->PrevNode;
		NewNode->NextNode = NodeToInsertBefore;

		NodeToInsertBefore->PrevNode->NextNode = NewNode;
		NodeToInsertBefore->PrevNode = NewNode;

		SetListSize(ListSize + 1);
		return true;
	}

	/**
	 * Remove the node corresponding to InElement
	 *
	 * @param	InElement	the value to remove from the list
	 */
	void RemoveNode( const ElementType& InElement )
	{
		TDoubleLinkedListNode* ExistingNode = FindNode(InElement);
		RemoveNode(ExistingNode);
	}

	/**
	 * Removes the node specified.
	 */
	void RemoveNode( TDoubleLinkedListNode* NodeToRemove )
	{
		if ( NodeToRemove != NULL )
		{
			// if we only have one node, just call Clear() so that we don't have to do lots of extra checks in the code below
			if ( Num() == 1 )
			{
				checkSlow(NodeToRemove == HeadNode);
				Empty();
				return;
			}

			if ( NodeToRemove == HeadNode )
			{
				HeadNode = HeadNode->NextNode;
				HeadNode->PrevNode = NULL;
			}

			else if ( NodeToRemove == TailNode )
			{
				TailNode = TailNode->PrevNode;
				TailNode->NextNode = NULL;
			}
			else
			{
				NodeToRemove->NextNode->PrevNode = NodeToRemove->PrevNode;
				NodeToRemove->PrevNode->NextNode = NodeToRemove->NextNode;
			}

			delete NodeToRemove;
			NodeToRemove = NULL;
			SetListSize(ListSize - 1);
		}
	}

	/**
	 * Removes all nodes from the list.
	 */
	void Empty()
	{
		TDoubleLinkedListNode* pNode;
		while ( HeadNode != NULL )
		{
			pNode = HeadNode->NextNode;
			delete HeadNode;
			HeadNode = pNode;
		}

		HeadNode = TailNode = NULL;
		SetListSize(0);
	}

	// Accessors.

	/**
	 * Returns the node at the head of the list
	 */
	TDoubleLinkedListNode* GetHead() const
	{
		return HeadNode;
	}

	/**
	 * Returns the node at the end of the list
	 */
	TDoubleLinkedListNode* GetTail() const
	{
		return TailNode;
	}

	/**
	 * Finds the node corresponding to the value specified
	 *
	 * @param	InElement	the value to find
	 *
	 * @return	a pointer to the node that contains the value specified, or NULL of the value couldn't be found
	 */
	TDoubleLinkedListNode* FindNode( const ElementType& InElement )
	{
		TDoubleLinkedListNode* pNode = HeadNode;
		while ( pNode != NULL )
		{
			if ( pNode->GetValue() == InElement )
			{
				break;
			}

			pNode = pNode->NextNode;
		}

		return pNode;
	}

	/**
	 * Returns the size of the list.
	 */
	int32 Num() const
	{
		return ListSize;
	}

protected:

	/**
	 * Updates the size reported by Num().  Child classes can use this function to conveniently
	 * hook into list additions/removals.
	 *
	 * @param	NewListSize		the new size for this list
	 */
	virtual void SetListSize( int32 NewListSize )
	{
		ListSize = NewListSize;
	}

private:
	TDoubleLinkedListNode* HeadNode;
	TDoubleLinkedListNode* TailNode;
	int32 ListSize;

	TDoubleLinkedList(const TDoubleLinkedList&);
	TDoubleLinkedList& operator=(const TDoubleLinkedList&);

	friend TIterator      begin(      TDoubleLinkedList& List) { return TIterator     (List.GetHead()); }
	friend TConstIterator begin(const TDoubleLinkedList& List) { return TConstIterator(List.GetHead()); }
	friend TIterator      end  (      TDoubleLinkedList& List) { return TIterator     (nullptr); }
	friend TConstIterator end  (const TDoubleLinkedList& List) { return TConstIterator(nullptr); }
};


/*----------------------------------------------------------------------------
	TList.
----------------------------------------------------------------------------*/

//
// Simple single-linked list template.
//
template <class ElementType> class TList
{
public:

	ElementType			Element;
	TList<ElementType>*	Next;

	// Constructor.

	TList(const ElementType &InElement, TList<ElementType>* InNext = NULL)
	{
		Element = InElement;
		Next = InNext;
	}
};
