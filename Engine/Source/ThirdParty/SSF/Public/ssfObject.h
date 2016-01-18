#ifndef _ssfObject_h_
#define _ssfObject_h_

namespace ssf
	{
	// base class for all ssf objects
	class ssfObject
		{
		public:
			ssfObject();

			void AddReference();
			void ReleaseReference();

		protected:
			unsigned int ReferenceCount;

			// not implemented methods and operators
			ssfObject( const ssfObject & );
			const ssfObject & operator = ( const ssfObject &other );
			virtual ~ssfObject();

		};
	}

#endif//_ssfObject_h_ 