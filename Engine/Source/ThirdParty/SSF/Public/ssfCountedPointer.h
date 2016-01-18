#ifndef _ssfCountedPointer_h_
#define _ssfCountedPointer_h_

namespace ssf
	{
	/** 
	* ssfCountedPointer is a template class for pointers to ssf objects. All ssf interface classes have typedefs for counted pointers that points at the specific class. 
	* All such typedefs are prefixed with 'pssf'. An example of this is 'pssfObject', which is a typedef for ssfCountedPointer<ssfObject>. The ssfCountedPointer can be used
	* for any object that supports reference counting, and exports the AddReference and ReleaseReference methods. See the ssfObject class for an example.
	*/ 
	template <class T> class ssfCountedPointer
		{
		public:
			/**
			* Constructs a ssfCountedPointer from a standard pointer. If the source pointer points at an API object, a reference to the API object is added.
			* @param p is a pointer to an API object, or NULL to make the ssfCountedPointer point at nothing.
			*/
			ssfCountedPointer( T *p=NULL ) : ptr(p)
				{
				addref_ptr();
				}
				
			/**
			* Constructs a ssfCountedPointer from another ssfCountedPointer. If the source pointer points at an API object, a reference to the API object is added.
			* @param p is a ssfCountedPointer that points at an API object.
			*/
			ssfCountedPointer( const ssfCountedPointer<T> &p ) : ptr(p.ptr)
				{
				addref_ptr();
				}
				
			/**
			* Destructs the ssfCountedPointer, and releases one reference to the API object, if the ssfCountedPointer does currently point at an object.
			*/
			~ssfCountedPointer()
				{
				release_ptr();
				}
		
			/** 
			* Tells whether the ssfCountedPointer points at an object, or nothing.
			* @return true if the pointer points at nothing, of false if the pointer points at an object
			*/
			bool IsNull() const
				{
				return (ptr == NULL);
				}
				
			/**
			* Sets the pointer to point at the same object as another ssfCountedPointer
			* @param p is the source ssfCountedPointer object
			* @return a reference to this object
			*/
			ssfCountedPointer<T>& operator=( const ssfCountedPointer<T> &p ) 
				{
				if( this != &p ) 
					{
					release_ptr();
					ptr = p.ptr;
					addref_ptr();
					}
				return *this;
				}

			/**
			* Operator that returns the reference of the object the pointer is pointing at. Note! Do not use this operator if the pointer is not pointing at an object. 
			* @return a reference to the object the pointer is pointing at
			*/
			T& operator*() const 
				{
				return *ptr;
				}
				
			/**
			* Operator that returns a standard pointer to the object the pointer is pointing at. 
			* @return a pointer to the object the pointer is pointing at
			*/
			T* operator->() const 
				{
				return ptr;
				}
								
			/**
			* Operator that returns a standard pointer to the object the pointer is pointing at. 
			* @return a pointer to the object the pointer is pointing at
			*/
			operator T* () const
				{
				return ptr;
				}
				
			/**
			* Method that returns a standard pointer to the object the pointer is pointing at. 
			* @return a pointer to the object the pointer is pointing at
			*/
			T* GetPointer() const
				{
				return ptr;
				}
				
		private:
			void addref_ptr()
				{
				if( ptr!=NULL )
					{
					ptr->AddReference();
					}
				}
		
			void release_ptr()
				{
				if( ptr!=NULL )
					{
					ptr->ReleaseReference();
					ptr = NULL;
					}
				}
		
			T *ptr;	
		};
	}
			
	
#endif//_ssfCountedPointer_h_ 