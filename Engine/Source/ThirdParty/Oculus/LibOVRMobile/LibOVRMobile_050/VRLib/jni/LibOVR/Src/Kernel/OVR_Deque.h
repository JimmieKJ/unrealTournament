/************************************************************************************

Filename    :   OVR_Deque.h
Content     :   Deque container
Created     :   Nov. 15, 2013
Authors     :   Dov Katz

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_Deque_h
#define OVR_Deque_h

#include "OVR_Allocator.h"

namespace OVR {

template <class Elem>
class Deque
{
public:
    enum
    {
        DefaultCapacity = 500
    };

    Deque(int capacity = DefaultCapacity);
    Deque(const Deque<Elem> &OtherDeque);
    virtual ~Deque(void);

    virtual void        PushBack  	(const Elem &Item);    // Adds Item to the end
    virtual void        PushFront 	(const Elem &Item);    // Adds Item to the beginning
    virtual Elem        PopBack   	(void);                // Removes Item from the end
    virtual Elem        PopFront  	(void);                // Removes Item from the beginning
    virtual const Elem& PeekBack  	(int count = 0) const; // Returns count-th Item from the end
    virtual const Elem& PeekFront 	(int count = 0) const; // Returns count-th Item from the beginning
    virtual inline int  GetSize   	(void) const;          // Returns Number of Elements
	virtual inline int	GetCapacity	(void)          const; // Returns the maximum possible number of elements
    virtual void        Clear     	(void);				   // Remove all elements
    virtual inline bool IsEmpty   	()     const;
    virtual inline bool IsFull    	()     const;

protected:
    Elem        *Data;          // The actual Data array
    const int   Capacity;       // Deque capacity
    int         Beginning;      // Index of the first element
    int         End;            // Index of the next after last element

    // Instead of calculating the number of elements, using this variable
    // is much more convenient.
    int         ElemCount;

private:
    Deque&      operator= (const Deque& q) { return *this; }; // forbidden
};

// Same as Deque, but allows to write more elements than maximum capacity
// Old elements are lost as they are overwritten with the new ones
template <class Elem>
class CircularBuffer : public Deque<Elem>
{
public:
    CircularBuffer(int MaxSize = Deque<Elem>::DefaultCapacity) : Deque<Elem>(MaxSize) { };
    virtual ~CircularBuffer(){};

    // The following methods are inline as a workaround for a VS bug causing erroneous C4505 warnings
    // See: http://stackoverflow.com/questions/3051992/compiler-warning-at-c-template-base-class
    inline virtual void PushBack  (const Elem &Item);    // Adds Item to the end, overwriting the oldest element at the beginning if necessary
    inline virtual void PushFront (const Elem &Item);    // Adds Item to the beginning, overwriting the oldest element at the end if necessary
};

//----------------------------------------------------------------------------------

// Deque Constructor function
template <class Elem>
Deque<Elem>::Deque(int capacity) :
Capacity( capacity )   
{
    Data = (Elem*) OVR_ALLOC(Capacity * sizeof(Elem));
    ConstructArray<Elem>(Data, Capacity);
    Clear();
}

// Deque Copy Constructor function
template <class Elem>
Deque<Elem>::Deque(const Deque &OtherDeque) :
Capacity( OtherDeque.Capacity )  // Initialize the constant
{
    Beginning = OtherDeque.Beginning;
    End       = OtherDeque.End;
    ElemCount = OtherDeque.ElemCount;

    Data      = (Elem*) OVR_ALLOC(Capacity * sizeof(Elem));
    for (int i = 0; i < Capacity; i++)
        Data[i] = OtherDeque.Data[i];
}

// Deque Destructor function
template <class Elem>
Deque<Elem>::~Deque(void)
{
    DestructArray<Elem>(Data, Capacity);
    OVR_FREE(Data);
}

template <class Elem>
void Deque<Elem>::Clear()
{
    Beginning = 0;
    End       = 0;
    ElemCount = 0;
}

// Push functions
template <class Elem>
void Deque<Elem>::PushBack(const Elem &Item)
{
    // Error Check: Make sure we aren't  
    // exceeding our maximum storage space
    assert( ElemCount < Capacity );

    Data[ End++ ] = Item;
    ++ElemCount;

    // Check for wrap-around
    if (End >= Capacity)
        End -= Capacity;
}

template <class Elem>
void Deque<Elem>::PushFront(const Elem &Item)
{
    // Error Check: Make sure we aren't  
    // exceeding our maximum storage space
    assert( ElemCount < Capacity );

    Beginning--;
    // Check for wrap-around
    if (Beginning < 0)
        Beginning += Capacity;

    Data[ Beginning ] = Item;
    ++ElemCount;
}

// Pop functions
template <class Elem>
Elem Deque<Elem>::PopFront(void)
{
    // Error Check: Make sure we aren't reading from an empty Deque
    assert( ElemCount > 0 );

    Elem ReturnValue = Data[ Beginning++ ];
    --ElemCount;

    // Check for wrap-around
    if (Beginning >= Capacity)
        Beginning -= Capacity;

    return ReturnValue;
}

template <class Elem>
Elem Deque<Elem>::PopBack(void)
{
    // Error Check: Make sure we aren't reading from an empty Deque
    assert( ElemCount > 0 );

    End--;
    // Check for wrap-around
    if (End < 0)
        End += Capacity;

    Elem ReturnValue = Data[ End ];
    --ElemCount;

    return ReturnValue;
}

// Peek functions
template <class Elem>
const Elem& Deque<Elem>::PeekFront(int count) const
{
    // Error Check: Make sure we aren't reading from an empty Deque
    assert( ElemCount > count );

    int idx = Beginning + count;
    if (idx >= Capacity)
        idx -= Capacity;
    return Data[ idx ];
}

template <class Elem>
const Elem& Deque<Elem>::PeekBack(int count) const
{
    // Error Check: Make sure we aren't reading from an empty Deque
    assert( ElemCount > count );

    int idx = End - count - 1;
    if (idx < 0)
        idx += Capacity;
    return Data[ idx ];
}

template <class Elem>
inline int Deque<Elem>::GetCapacity(void) const
{
    return Deque<Elem>::Capacity;
}

// ElemNum() function
template <class Elem>
inline int Deque<Elem>::GetSize(void) const
{
    return ElemCount;
}

template <class Elem>
inline bool Deque<Elem>::IsEmpty(void) const
{
    return ElemCount==0;
}

template <class Elem>
inline bool Deque<Elem>::IsFull(void) const
{
    return ElemCount==Capacity;
}

// ******* CircularBuffer<Elem> *******
// Push functions
template <class Elem>
void CircularBuffer<Elem>::PushBack(const Elem &Item)
{
    if (this->IsFull())
        this->PopFront();
    Deque<Elem>::PushBack(Item);
}

template <class Elem>
void CircularBuffer<Elem>::PushFront(const Elem &Item)
{
    if (this->IsFull())
        this->PopBack();
    Deque<Elem>::PushFront(Item);
}

};   

#endif
