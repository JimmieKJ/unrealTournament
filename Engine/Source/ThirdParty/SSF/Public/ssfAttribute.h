#ifndef _ssfAttribute_h_
#define _ssfAttribute_h_

#if _MSC_VER < 1600 //MSVC version <9
	#ifndef nullptr
	#define nullptr NULL
	#endif
#endif

namespace ssf
	{
	template <class T> class ssfAttribute
		{

		public:
			/**
			* Method that creates an attribute and returns the reference to it. If the attribute exists, it is returned
			* @return a reference to the object
			*/
			T& Create() 
				{
				T* p = ptr.get();
				if( p==nullptr )
					{
					ptr.reset( new T() );
					p = ptr.get();
					}
				return *p;
				}

			/**
			* Method that creates an attribute, sets it to a value and returns the reference to it. If the attribute exists, it is returned
			* @return a reference to the object
			*/
			T& Set( const T & other ) 
				{
				ptr.reset( new T(other) );
				return *(ptr.get());
				}

			/** 
			* Tells whether the ssfAttribute is set and points at an object, or nothing.
			* @return true if the pointer points at nothing, of false if the pointer points at an object
			*/
			bool IsEmpty() const
				{
				return (ptr.get() == nullptr);
				}
	
			/**
			* Method that returns a reference to the attribute object. Note that this methods throws an exception if attribute is not set
			* @return a reference to the object
			*/
			T& Get() 
				{
				T* p = ptr.get();
				if( p==nullptr )
					{
					throw std::exception("T& ssfAttribute::Get() trying to get a reference to an empty attribute");
					}
				return *p;
				}

			/**
			* Method that returns a reference to the attribute object. Note that this methods throws an exception if attribute is not set
			* @return a reference to the object
			*/
			const T& Get() const
				{
				T* p = ptr.get();
				if( p==nullptr )
					{
					throw std::exception("const T& ssfAttribute::Get() const: trying to get a reference to an empty attribute");
					}
				return *p;
				}

			/**
			* Operator that returns a reference to the attribute object. Note that this methods throws an exception if attribute is not set
			* @return a reference to the object the pointer is pointing at
			*/
			T& operator*()  
				{
				return this->Get();
				}
				
			/**
			* Operator that returns a reference to the attribute object. Note that this methods throws an exception if attribute is not set
			* @return a reference to the object the pointer is pointing at
			*/
			const T& operator*() const 
				{
				return this->Get();
				}
	
			/**
			* Method that returns a standard pointer to the object the pointer is pointing at. 
			* @return a reference to the object the pointer is pointing at
			*/
			T* operator->()  
				{
				return this->GetPointer();
				}

			/**
			* Method that returns a standard pointer to the object the pointer is pointing at. 
			* @return a reference to the object the pointer is pointing at
			*/
			const T* operator->() const
				{
				return this->GetPointer();
				}

			/**
			* Method that returns a standard pointer to the object the pointer is pointing at. 
			* @return a pointer to the object the pointer is pointing at
			*/
			T* GetPointer() const
				{
				return ptr.get();
				}
			
			//// auto_ptr methods exposed outside of the attribute
			//void reset( T* p )
			//	{
			//	ptr.reset(p);
			//	}

			//T *get() const
			//	{
			//	return ptr.get();
			//	}

		private:
			auto_ptr<T> ptr;

		};
	}
			
	
#endif//_ssfAttribute_h_ 