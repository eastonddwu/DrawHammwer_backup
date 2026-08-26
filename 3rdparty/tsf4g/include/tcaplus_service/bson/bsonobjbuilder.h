/*    Copyright 2009 10gen Inc.
*
*    Licensed under the Apache License, Version 2.0 (the "License");
*    you may not use this file except in compliance with the License.
*    You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
*    Unless required by applicable law or agreed to in writing, software
*    distributed under the License is distributed on an "AS IS" BASIS,
*    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*    See the License for the specific language governing permissions and
*    limitations under the License.
*/

#pragma once

#include <map>
#include <cmath>
#include <limits>
#include "bsontypes.h"
#include "parse_number.h"
#include "bsonelement.h"
#include "shared_buffer.h"
#include "bsonobj.h"
#include "builder.h"
#include "oid.h"
#include "time_support.h"
#include "bson_field.h"

namespace tcaplus {
namespace doc {

#if defined(_WIN32)
    // warning: 'this' : used in base member initializer list
#pragma warning( disable : 4355 )
#endif

    class BSONObjIterator;

    /** Utility for creating a BSONObj.
    See also the BSON() and BSON_ARRAY() macros.
    */
    class BSONObjBuilder 
	{
        BufBuilder &_b;
        BufBuilder _buf;
        int _offset;
        int _simpleChildNum; // count of the children if they are of a simple type, i.e., !e.isObject()
        bool _doneCalled;

		bool _isSubElementStarted;
		StringData _tmpFieldName;

		bool _externBufferUsed;

    public:
        char* _done() 
		{
            if (_doneCalled)
                return _b.buf() + _offset;

            _doneCalled = true;
            _b.appendNum((char)EOO);
            char *data = _b.buf() + _offset;
            int size = _b.len() - _offset;
            *(reinterpret_cast<int*>(data)) = endian_int(size);
            *(reinterpret_cast<int*>(data + BO_HEADER_OBJ_SIZE)) = endian_int(_simpleChildNum);
            return data;
        }

        /** @param initsize this is just a hint as to the final size of the object */
        BSONObjBuilder(int initsize = 512) 
                : _b(_buf), _buf(initsize + sizeof(unsigned))
                , _offset(0), _simpleChildNum(0), _doneCalled(false)
        {
            _b.skip(BO_HEADER_TOTAL); /*leave room for the header*/
			_isSubElementStarted = false;
			_externBufferUsed = false;
        }

        /** @param buffer  extern buffer
                *  @bufferSize      extern buffer size
                *
                */
        BSONObjBuilder(char* buffer, int bufferSize)
                : _b(_buf), _buf(buffer,bufferSize)
                , _offset(0), _simpleChildNum(0), _doneCalled(false)
        {
            _b.skip(BO_HEADER_TOTAL); /*leave room for the header*/
			_isSubElementStarted = false;
			_externBufferUsed = true;
        }

        /** @param baseBuilder construct a BSONObjBuilder using an existing BufBuilder
        *  This is for more efficient adding of subobjects/arrays. See docs for subobjStart for example.
        */
        BSONObjBuilder(BufBuilder &baseBuilder)
                : _b(baseBuilder), _buf(0)
                , _offset(baseBuilder.len()), _simpleChildNum(0), _doneCalled(false) 
        {
            _b.skip(BO_HEADER_TOTAL); /*leave room for the header*/
			_isSubElementStarted = false;
			_externBufferUsed = false;
        }

        ~BSONObjBuilder() {
            if (!_doneCalled && _b.buf() && _buf.getSize() == 0) {
                _done();
            }
        }

        /** add all the fields from the object specified to this object */
        BSONObjBuilder& appendElements(BSONObj x);

        /** add all the fields from the object specified to this object if they don't exist already */
        BSONObjBuilder& appendElementsUnique(BSONObj x);

        /** when a simple node is appended or two objects are merged*/
        void addChildNum(const int n)
        {
            _simpleChildNum += n;
        }

        /** check if this element is a simple node or a complex node*/
        void addChildNumIfSimpleType(const BSONElement& e)
        {
            // e does not have to be a leaf node
            // TODO: if (e.isSimpleType()) ?
            if (!e.isObject()) 
            {
                ++_simpleChildNum;
            }
        }

        /** append element to the object we are building */
        BSONObjBuilder& append(const BSONElement& e) {
            // do not append eoo, that would corrupt us. the builder auto appends when done() is called.
			if(e.eoo())
			{
				msgasserted(10009, "append(const BSONElement& e) error,reason:(e.eoo())"); 
			}

            _b.appendBuf((void*)e.rawdata(), e.size());
            addChildNumIfSimpleType(e);
            return *this;
        }

        /* helper function -- see Query::where() for primary way to do this. */
        void appendWhere( const StringData& code, const BSONObj &scope ) {
            appendCodeWScope( "$where" , code , scope );
        }

        /** Append to the BSON object a field of type CodeWScope.  This is a javascript code
            fragment accompanied by some scope that goes with it.
        */
        BSONObjBuilder& appendCodeWScope( const StringData& fieldName, const StringData& code, const BSONObj &scope ) {
            _b.appendNum( (char) CodeWScope );
            _b.appendStr( fieldName );
            _b.appendNum( ( int )( 4 + 4 + code.size() + 1 + scope.objsize() ) );
            _b.appendNum( ( int ) code.size() + 1 );
            _b.appendStr( code );
            _b.appendBuf( ( void * )scope.objdata(), scope.objsize() );
            // TODO: is this a simple type?
            addChildNum(1);
            return *this;
        }

        /** append an element but with a new name */
        BSONObjBuilder& appendAs(const BSONElement& e, const StringData& fieldName) {
            // do not append eoo, that would corrupt us. the builder auto appends when done() is called.
			if(e.eoo())
			{
				msgasserted(10009, "appendAs(const BSONElement& e, const StringData& fieldName) error,reason:(e.eoo())"); 
			}

            _b.appendNum((char)e.type());
            _b.appendStr(fieldName);
            _b.appendBuf((void *)e.value(), e.valuesize());
            addChildNumIfSimpleType(e);
            return *this;
        }

        /** add a subobject as a member */
        BSONObjBuilder& append(const StringData& fieldName, BSONObj subObj) {
            _b.appendNum((char)Object);
            _b.appendStr(fieldName);
            _b.appendBuf((void *)subObj.objdata(), subObj.objsize());
            return *this;
        }

        /** add a subobject as a member */
        BSONObjBuilder& appendObject(const StringData& fieldName, const char * objdata, int size = 0) 
        {
			if(!(objdata != 0))
			{
				msgasserted(10010, "appendObject(const StringData& fieldName, const char * objdata, int size = 0) error,reason:(!(objdata != 0))"); 
			}
			
            if (size == 0) {
                size = *((int*)objdata);
            }

			if(!(size > BO_HEADER_TOTAL && size < 100000000))
			{
				msgasserted(10011, "appendObject(const StringData& fieldName, const char * objdata, int size = 0) error,reason:(!(size > BO_HEADER_TOTAL && size < 100000000))"); 
			}

            _b.appendNum((char)Object);
            _b.appendStr(fieldName);
            _b.appendBuf((void*)objdata, size);
            return *this;
        }

        /** add a subobject as a member with type Array.  Thus arr object should have "0", "1", ...
        style fields in it.
        */
        BSONObjBuilder& appendArray(const StringData& fieldName, const BSONObj &subObj) {
            _b.appendNum((char)Array);
            _b.appendStr(fieldName);
            _b.appendBuf((void *)subObj.objdata(), subObj.objsize());
            return *this;
        }

        BSONObjBuilder& append(const StringData& fieldName, BSONArray arr) {
            return appendArray(fieldName, arr);
        }

        /** add header for a new subarray and return bufbuilder for writing to
            the subarray's body 
        
            e.g.:
              BufBuilder& sub = b.subarrayStart("myArray");
              sub.append( "0", "hi" );
              sub.append( "1", "there" );
              sub.append( "2", 33 );
              sub._done();

        */
        BufBuilder &subarrayStart(const StringData& fieldName) {
            _b.appendNum((char)Array);
            _b.appendStr(fieldName);
            return _b;
        }

        /** Append a boolean element */
        BSONObjBuilder& appendBool(const StringData& fieldName, int val) {
            _b.appendNum((char)Bool);
            _b.appendStr(fieldName);
            _b.appendNum((char)(val ? 1 : 0));
            addChildNum(1);
            return *this;
        }

        /** Append a boolean element */
        BSONObjBuilder& append(const StringData& fieldName, bool val) {
            _b.appendNum((char)Bool);
            _b.appendStr(fieldName);
            _b.appendNum((char)(val ? 1 : 0));
            addChildNum(1);
            return *this;
        }

        /** Append a 32 bit integer element */
        BSONObjBuilder& append(const StringData& fieldName, int n) {
            _b.appendNum((char)NumberInt);
            _b.appendStr(fieldName);
            _b.appendNum(n);
            addChildNum(1);
            return *this;
        }

        /** Append a 32 bit unsigned element - cast to a signed int. */
        BSONObjBuilder& append(const StringData& fieldName, unsigned n) {
            return append(fieldName, (int)n);
        }

        /** Append a NumberLong */
        BSONObjBuilder& append(const StringData& fieldName, long long n) {
            _b.appendNum((char)NumberLong);
            _b.appendStr(fieldName);
            _b.appendNum(n);
            addChildNum(1);
            return *this;
        }

        /** appends a number.  if n < max(int)/2 then uses int, otherwise long long */
        BSONObjBuilder& appendIntOrLL(const StringData& fieldName, long long n) {
            // extra () to avoid max macro on windows
            static const long long maxInt = (std::numeric_limits<int>::max)() / 2;
            static const long long minInt = -maxInt;
            if (minInt < n && n < maxInt) {
                append(fieldName, static_cast<int>(n));
            }
            else {
                append(fieldName, n);
            }
            return *this;
        }

        /**
        * appendNumber is a series of method for appending the smallest sensible type
        * mostly for JS
        */
        BSONObjBuilder& appendNumber(const StringData& fieldName, int n) {
            return append(fieldName, n);
        }

        BSONObjBuilder& appendNumber(const StringData& fieldName, double d) {
            return append(fieldName, d);
        }

        BSONObjBuilder& appendNumber(const StringData& fieldName, size_t n) {
            static const size_t maxInt = (1 << 30);

            if (n < maxInt)
                append(fieldName, static_cast<int>(n));
            else
                append(fieldName, static_cast<long long>(n));
            return *this;
        }

        BSONObjBuilder& appendNumber(const StringData& fieldName, long long llNumber) {
            static const long long maxInt = (1LL << 30);
            static const long long minInt = -maxInt;
            static const long long maxDouble = (1LL << 40);
            static const long long minDouble = -maxDouble;

            if (minInt < llNumber && llNumber < maxInt) {
                append(fieldName, static_cast<int>(llNumber));
            }
            else if (minDouble < llNumber && llNumber < maxDouble) {
                append(fieldName, static_cast<double>(llNumber));
            }
            else {
                append(fieldName, llNumber);
            }

            return *this;
        }

        /** Append a double element */
        BSONObjBuilder& append(const StringData& fieldName, double n) {
            _b.appendNum((char)NumberDouble);
            _b.appendStr(fieldName);
            _b.appendNum(n);
            addChildNum(1);
            return *this;
        }

        /** tries to append the data as a number
        * @return true if the data was able to be converted to a number
        */
        bool appendAsNumber(const StringData& fieldName, const std::string& data);

        /**
        Append a BSON Object ID.
        @param fieldName Field name, e.g., "_id".
        @returns the builder object
        */
        BSONObjBuilder& append(const StringData& fieldName, OID oid) {
            _b.appendNum((char)jstOID);
            _b.appendStr(fieldName);
            _b.appendBuf((void *)&oid, 12);
            addChildNum(1);
            return *this;
        }

        /** Append a time_t date.
        @param dt a C-style 32 bit date value, that is
        the number of seconds since January 1, 1970, 00:00:00 GMT
        */
        BSONObjBuilder& appendTimeT(const StringData& fieldName, time_t dt) {
            _b.appendNum((char)Date);
            _b.appendStr(fieldName);
            _b.appendNum(static_cast<unsigned long long>(dt)* 1000);
            addChildNum(1);
            return *this;
        }
        /** Append a date.
        @param dt a Java-style 64 bit date value, that is
        the number of milliseconds since January 1, 1970, 00:00:00 GMT
        */
        BSONObjBuilder& appendDate(const StringData& fieldName, Date_t dt) {
            /* easy to pass a time_t to this and get a bad result.  thus this warning. */
#if defined(_DEBUG) && defined(MONGO_EXPOSE_MACROS)
            if (dt > 0 && dt <= 0xffffffff) {
                static int n;
                if (n++ == 0)
                    log() << "DEV WARNING appendDate() called with a tiny (but nonzero) date" << std::endl;
            }
#endif
            _b.appendNum((char)Date);
            _b.appendStr(fieldName);
            _b.appendNum(dt);
            addChildNum(1);
            return *this;
        }
        BSONObjBuilder& append(const StringData& fieldName, Date_t dt) {
            return appendDate(fieldName, dt);
        }


        /** Append a regular expression value
            @param regex the regular expression pattern
            @param regex options such as "i" or "g"
        */
        BSONObjBuilder& appendRegex(const StringData& fieldName, const StringData& regex, const StringData& options = "") {
            _b.appendNum((char) RegEx);
            _b.appendStr(fieldName);
            _b.appendStr(regex);
            _b.appendStr(options);
            addChildNum(1);
            return *this;
        }

        BSONObjBuilder& appendCode(const StringData& fieldName, const StringData& code) {
            _b.appendNum((char) Code);
            _b.appendStr(fieldName);
            _b.appendNum((int) code.size()+1);
            _b.appendStr(code);
            addChildNum(1);
            return *this;
        }
        /** Append a string element.
            @param sz size includes terminating null character */
        BSONObjBuilder& append(const StringData& fieldName, const char *str, int sz) {
            _b.appendNum((char) String);
            _b.appendStr(fieldName);
            _b.appendNum((int)sz);
            _b.appendBuf(str, sz);
            addChildNum(1);
            return *this;
        }
        /** Append a string element */
        BSONObjBuilder& append(const StringData& fieldName, const char *str) {
            return append(fieldName, str, (int) strlen(str)+1);
        }
        /** Append a string element */
        BSONObjBuilder& append(const StringData& fieldName, const std::string& str) {
            return append(fieldName, str.c_str(), (int) str.size()+1);
        }
        /** Append a string element */
        BSONObjBuilder& append(const StringData& fieldName, const StringData& str) {
            _b.appendNum((char) String);
            _b.appendStr(fieldName);
            _b.appendNum((int)str.size()+1);
            _b.appendStr(str, true);
            addChildNum(1);
            return *this;
        }
        BSONObjBuilder& appendSymbol(const StringData& fieldName, const StringData& symbol) {
            _b.appendNum((char) Symbol);
            _b.appendStr(fieldName);
            _b.appendNum((int) symbol.size()+1);
            _b.appendStr(symbol);
            addChildNum(1);
            return *this;
        }

        /** Implements builder interface but no-op in ObjBuilder */
        void appendNull() {
            msgasserted(16234, "Invalid call to appendNull in BSONObj Builder.");
        }

        /** Append a Null element to the object */
        BSONObjBuilder& appendNull( const StringData& fieldName ) {
            _b.appendNum( (char) jstNULL );
            _b.appendStr( fieldName );
            addChildNum(1);
            return *this;
        }

        // Append an element that is less than all other keys.
        BSONObjBuilder& appendMinKey( const StringData& fieldName ) {
            _b.appendNum( (char) MinKey );
            _b.appendStr( fieldName );
            addChildNum(1);
            return *this;
        }
        // Append an element that is greater than all other keys.
        BSONObjBuilder& appendMaxKey( const StringData& fieldName ) {
            _b.appendNum( (char) MaxKey );
            _b.appendStr( fieldName );
            addChildNum(1);
            return *this;
        }
        // Append a Timestamp field -- will be updated to next OpTime on db insert.
        BSONObjBuilder& appendTimestamp( const StringData& fieldName ) {
            _b.appendNum( (char) Timestamp );
            _b.appendStr( fieldName );
            _b.appendNum( (unsigned long long) 0 );
            addChildNum(1);
            return *this;
        }

        /** add header for a new subobject and return bufbuilder for writing to
         *  the subobject's body
         *
         *  example:
         *
         *  BSONObjBuilder b;
         *  BSONObjBuilder sub (b.subobjStart("fieldName"));
         *  // use sub
         *  sub.done()
         *  // use b and convert to object
         */
        BufBuilder &subobjStart(const StringData& fieldName) {
            _b.appendNum((char) Object);
            _b.appendStr(fieldName);
            return _b;
        }
        
        /**
         * Alternative way to store an OpTime in BSON. Pass the OpTime as a Date, as follows:
         *
         *     builder.appendTimestamp("field", optime.asDate());
         *
         * This captures both the secs and inc fields.
         */
        BSONObjBuilder& appendTimestamp( const StringData& fieldName , unsigned long long val ) {
            _b.appendNum( (char) Timestamp );
            _b.appendStr( fieldName );
            _b.appendNum( val );
            addChildNum(1);
            return *this;
        }

        /**
        Timestamps are a special BSON datatype that is used internally for replication.
        Append a timestamp element to the object being ebuilt.
        @param time - in millis (but stored in seconds)
        */
        /** Append a binary data element
            @param fieldName name of the field
            @param len length of the binary data in bytes
            @param subtype subtype information for the data. @see enum BinDataType in bsontypes.h.
                   Use BinDataGeneral if you don't care about the type.
            @param data the byte array
        */
        BSONObjBuilder& appendBinData( const StringData& fieldName, int len, BinDataType type, const void *data ) {
            _b.appendNum( (char) BinData );
            _b.appendStr( fieldName );
            _b.appendNum( len );
            _b.appendNum( (char) type );
            _b.appendBuf( data, len );
            addChildNum(1);
            return *this;
        }

        void appendUndefined(const StringData& fieldName) {
            _b.appendNum((char)Undefined);
            _b.appendStr(fieldName);
            addChildNum(1);
        }

        /**
        * Append a map of values as a sub-object.
        * Note: the keys of the map should be StringData-compatible (i.e. strings).
        */
        template < class K, class T >
        BSONObjBuilder& append(const StringData& fieldName, const std::map< K, T >& vals);

        void decouple()
        {
            _b.decouple();
        }

        void reset() 
        {
            _b.reset();
            _b.skip(BO_HEADER_TOTAL); /*leave room for the header*/
            _offset = 0;
            _doneCalled = false;
            _isSubElementStarted = false;
        }

        /**
        * destructive
        * The returned BSONObj will free the buffer when it is finished.
        * @return owned BSONObj
        */
        BSONObj obj()
        {
            if(!_externBufferUsed)
        	{
	            char* buf = _done();
	            decouple();
	            return BSONObj(SharedBuffer(buf, _b.GetDeleter() ));
        	}
			else
			{
				return done();
			}
        }

        /** Fetch the object we have built.
        BSONObjBuilder still frees the object when the builder goes out of
        scope -- very important to keep in mind.  Use obj() if you
        would like the BSONObj to last longer than the builder.
        */
        BSONObj done() {
            return BSONObj(_done());
        }

        /** Peek at what is in the builder, but leave the builder ready for more appends.
        The returned object is only valid until the next modification or destruction of the builder.
        Intended use case: append a field if not already there.
        */
        BSONObj asTempObj() {
            BSONObj temp(_done());
            _b.setlen(_b.len() - 1); //next append should overwrite the EOO
            _doneCalled = false;
            return temp;
        }

        /** Make it look as if "done" has been called, so that our destructor is a no-op. Do
        *  this if you know that you don't care about the contents of the builder you are
        *  destroying.
        *
        *  Note that it is invalid to call any method other than the destructor after invoking
        *  this method.
        */
        void abandon() {
            _doneCalled = true;
        }

        void appendKeys(const BSONObj& keyPattern, const BSONObj& values);

        static std::string  numStr(int i) {
            if (i >= 0 && i<100 && numStrsReady)
                return numStrs[i];
            StringBuilder o;
            o << i;
            return o.str();
        }

        bool isArray() const {
            return false;
        }

        /** @return true if we are using our own bufbuilder, and not an alternate that was given to us in our constructor */
        bool owned() const { return &_b == &_buf; }

        BSONObjIterator iterator() const;

        bool hasField(const StringData& name) const;

        int len() const { return _b.len(); }

        BufBuilder& bb() { return _b; }

        BSONObjBuilder& operator<<( const BSONElement& e )
		{
            append( e );
            return *this;
        }

        BSONObjBuilder& operator<<( const StringData& e )
		{
			if(!_isSubElementStarted)
			{
	 			_isSubElementStarted = true;
				_tmpFieldName = e;
			}
			else
			{
	            append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
            return *this;
        }

        BSONObjBuilder& operator<<( const std::string& e )
		{
			if(!_isSubElementStarted)
			{
	 			_isSubElementStarted = true;
				_tmpFieldName = e;
			}
			else
			{
	            append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
            return *this;
        }

        BSONObjBuilder& operator<<( const char* e )
		{
			if(!_isSubElementStarted)
			{
	 			_isSubElementStarted = true;
				_tmpFieldName = e;
			}
			else
			{
	            append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
            return *this;
        }

        template<typename T>
        BSONObjBuilder& operator<<( const BSONField<T>& e )
        {
			if(!_isSubElementStarted)
			{
	 			_isSubElementStarted = true;
				_tmpFieldName = e.name();
			}
			else
			{
	            append(_tmpFieldName, e.name() );
				_isSubElementStarted = false;
			}
            return *this;
        }

        template<typename T>
        BSONObjBuilder& operator<<( const BSONFieldValue<T>& e )
        {
            append(e.name(),e.value());
            return *this;
        }

        BSONObjBuilder& operator<<( const BSONObj& e )
		{
			if(_isSubElementStarted)
			{
	            append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the subBSONObj. "); 
			}
			return *this;
        }

		BSONObjBuilder& operator<<( bool e )
		{
			if(_isSubElementStarted)
			{
				append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the bool value. "); 
			}
			return *this;
		}

		BSONObjBuilder& operator<<( int e )
		{
			if(_isSubElementStarted)
			{
				append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the int value. "); 
			}
			return *this;
		}

		BSONObjBuilder& operator<<( long long e )
		{
			if(_isSubElementStarted)
			{
				append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the long long value. "); 
			}
			return *this;
		}


		BSONObjBuilder& operator<<( double e )
		{
			if(_isSubElementStarted)
			{
				append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the double value. "); 
			}
			return *this;
		}


		BSONObjBuilder& operator<<( Date_t e )
		{
			if(_isSubElementStarted)
			{
				append(_tmpFieldName, e );
				_isSubElementStarted = false;
			}
			else
			{
				msgasserted(10021, "need string-type fieldName before the Date_t value. "); 
			}
			return *this;
		}
		
    private:
        static const std::string numStrs[100]; // cache of 0 to 99 inclusive
        static bool numStrsReady; // for static init safety. see comments in db/jsobj.cpp
    };


    class BSONArrayBuilder {
    public:
        BSONArrayBuilder() : _i(0), _b() {}
        BSONArrayBuilder( BufBuilder &_b ) : _i(0), _b(_b) {}
        BSONArrayBuilder( int initialSize ) : _i(0), _b(initialSize) {}

        template <typename T>
        BSONArrayBuilder& append(const T& x) {
            _b.append(num(), x);
            return *this;
        }

        BSONArrayBuilder& append(const BSONElement& e) {
            _b.appendAs(e, num());
            return *this;
        }

        BSONArrayBuilder& operator<<(const BSONElement& e) {
            return append(e);
        }

        template <typename T>
        BSONArrayBuilder& operator<<(const T& x) {
            // [tcrm] _b << num().c_str() << x;
            _b.append(num().c_str(), x);
            return *this;
        }

        void appendNull() {
            _b.appendNull(num());
        }

        void appendUndefined() {
            _b.appendUndefined(num());
        }

        /**
         * destructive - ownership moves to returned BSONArray
         * @return owned BSONArray
         */
        BSONArray arr() { return BSONArray(_b.obj()); }
        BSONObj obj() { return _b.obj(); }

        BSONObj done() { return _b.done(); }

        // [tcrm] void doneFast() { _b.doneFast(); }

        template < class T >
        BSONArrayBuilder& append( const std::list< T >& vals );

        template < class T >
        BSONArrayBuilder& append( const std::set< T >& vals );

        // These two just use next position
        BufBuilder &subobjStart() { return _b.subobjStart( num() ); }
        BufBuilder &subarrayStart() { return _b.subarrayStart( num() ); }

        BSONArrayBuilder& appendRegex(const StringData& regex, const StringData& options = "") {
            _b.appendRegex(num(), regex, options);
            return *this;
        }

        BSONArrayBuilder& appendBinData(int len, BinDataType type, const void* data) {
            _b.appendBinData(num(), len, type, data);
            return *this;
        }

        BSONArrayBuilder& appendCode(const StringData& code) {
            _b.appendCode(num(), code);
            return *this;
        }

        /* [tcrm]
        BSONArrayBuilder& appendCodeWScope(const StringData& code, const BSONObj& scope) {
            _b.appendCodeWScope(num(), code, scope);
            return *this;
        }
        */

        BSONArrayBuilder& appendTimeT(time_t dt) {
            _b.appendTimeT(num(), dt);
            return *this;
        }

        BSONArrayBuilder& appendDate(Date_t dt) {
            _b.appendDate(num(), dt);
            return *this;
        }

        BSONArrayBuilder& appendBool(bool val) {
            _b.appendBool(num(), val);
            return *this;
        }

        bool isArray() const {
            return true;
        }

        int len() const { return _b.len(); }
        int arrSize() const { return _i; }

        BufBuilder& bb() { return _b.bb(); }

    private:
        BSONArrayBuilder(const BSONArrayBuilder&);
        BSONArrayBuilder& operator=(const BSONArrayBuilder&);

    private:

        std::string num() { return _b.numStr(_i++); }
        int _i;
        BSONObjBuilder _b;
    };

    template < class L >
    inline BSONObjBuilder& _appendIt(BSONObjBuilder& _this, const StringData& fieldName, const L& vals) {
        BSONObjBuilder arrBuilder;
        int n = 0;
        for (typename L::const_iterator i = vals.begin(); i != vals.end(); i++)
            arrBuilder.append(BSONObjBuilder::numStr(n++), *i);
        _this.appendArray(fieldName, arrBuilder.done());
        return _this;
    }

    template < class K, class T >
    inline BSONObjBuilder& BSONObjBuilder::append(const StringData& fieldName, const std::map< K, T >& vals) {
        BSONObjBuilder bob;
        for (typename std::map<K, T>::const_iterator i = vals.begin(); i != vals.end(); ++i){
            bob.append(i->first, i->second);
        }
        append(fieldName, bob.obj());
        return *this;
    }

    template < class L >
    inline BSONArrayBuilder& _appendArrayIt( BSONArrayBuilder& _this, const L& vals ) {
        for( typename L::const_iterator i = vals.begin(); i != vals.end(); i++ )
            _this.append( *i );
        return _this;
    }

    template < class T >
    inline BSONArrayBuilder& BSONArrayBuilder::append( const std::list< T >& vals ) {
        return _appendArrayIt< std::list< T > >( *this, vals );
    }

    template < class T >
    inline BSONArrayBuilder& BSONArrayBuilder::append( const std::set< T >& vals ) {
        return _appendArrayIt< std::set< T > >( *this, vals );
    }

	template<typename T>
	inline BSONFieldValue<BSONObj> BSONField<T>::query( const char * q , const T& t ) const {
		BSONObjBuilder b;
		b.append( q , t );
		return BSONFieldValue<BSONObj>( _name , b.obj() );
	}

	

    /** Use BSON macro to build a BSONObj from a stream

        e.g.,
           BSON( "name" << "joe" << "age" << 33 )

        with auto-generated object id:
           BSON( GENOID << "name" << "joe" << "age" << 33 )

        The labels GT, GTE, LT, LTE, NE can be helpful for stream-oriented construction
        of a BSONObj, particularly when assembling a Query.  For example,
        BSON( "a" << GT << 23.4 << NE << 30 << "b" << 2 ) produces the object
        { a: { \$gt: 23.4, \$ne: 30 }, b: 2 }.
    */
#define BSON(x) (( BSONObjBuilder() << x ).obj())

    /** Use BSON_ARRAY macro like BSON macro, but without keys

        BSONArray arr = BSON_ARRAY( "hello" << 1 << BSON( "foo" << BSON_ARRAY( "bar" << "baz" << "qux" ) ) );

     */
#define BSON_ARRAY(x) ((BSONArrayBuilder() << x ).arr())


} // namespace doc
} // namespace tcaplus
