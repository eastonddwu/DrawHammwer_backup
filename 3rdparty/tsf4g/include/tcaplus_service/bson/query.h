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
#include "bson_field.h"

namespace tcaplus {
namespace doc {
namespace bson {

class Query {
 public:
//	 static const BSONField<BSONObj> ReadPrefField;
//	 static const BSONField<std::string> ReadPrefModeField;
//	 static const BSONField<BSONArray> ReadPrefTagsField;

	 BSONObj obj;
	 Query() : obj(BSONObj()) { }
	 Query(const BSONObj& b) : obj(b) { }
//	 Query(const std::string &json);
//	 Query(const char * json);

	 /** Add a sort (ORDER BY) criteria to the query expression.
		 @param sortPattern the sort order template.  For example to order by name ascending, time descending:
		   { name : 1, ts : -1 }
		 i.e.
		   BSON( "name" << 1 << "ts" << -1 )
	 */
	 Query& sort(const BSONObj& sortPattern)
 	 {
	     appendComplex( "orderby", sortPattern );
	     return *this;
 	 }

 	 BSONObj getSort() const
	 {
        if ( ! isComplex() )
            return BSONObj();
        BSONObj ret = obj.getObjectField( "orderby" );
        if (ret.isEmpty())
            ret = obj.getObjectField( "$orderby" );
        return ret;
     }

	 /** Add a sort (ORDER BY) criteria to the query expression.
		 This version of sort() assumes you want to sort on a single field.
		 @param asc = 1 for ascending order
		 asc = -1 for descending order
	 */
	 Query& sort(const std::string &field, int asc = 1) 
	 { 
		 sort( BSON( field << asc ) ); 
		 return *this; 
	 }

	 /** Queries to the Mongo database support a $where parameter option which contains
		 a javascript function that is evaluated to see whether objects being queried match
		 its criteria.	Use this helper to append such a function to a query object.
		 Your query may also contain other traditional Mongo query terms.

		 @param jscode The javascript function to evaluate against each potential object
				match.	The function must return true for matched objects.	Use the this
				variable to inspect the current object.
		 @param scope SavedContext for the javascript object.  List in a BSON object any
				variables you would like defined when the jscode executes.	One can think
				of these as "bind variables".

		 Examples:
		   conn.findOne("test.coll", Query("{a:3}").where("this.b == 2 || this.c == 3"));
		   Query badBalance = Query().where("this.debits - this.credits < 0");
	 */
	 Query& where(const std::string &jscode, BSONObj scope)
	 {
        /* use where() before sort() and hint() and explain(), else this will assert. */
		if(isComplex())
		{
			msgasserted(10018, "where(const std::string &jscode, BSONObj scope)) error,reason:(isComplex())"); 
	 	}
	
        BSONObjBuilder b;
        b.appendElements(obj);
        b.appendWhere(jscode, scope);
        obj = b.obj();
        return *this;
     }
	 
	 Query& where(const std::string &jscode) 
 	 {
	 	return where(jscode, BSONObj()); 
	 }

	 /**
	  * @return true if this query has an orderby, hint, or some other field
	  */
	 bool isComplex( bool * hasDollar = 0 ) const
 	 {
	   	 return isComplex(obj, hasDollar);
	 }

	 static bool isComplex(const BSONObj& obj, bool* hasDollar = 0)
	 {
		 if (obj.hasElement("query")) {
			 if (hasDollar) *hasDollar = false;
			 return true;
		 }
 
		 if (obj.hasElement("$query")) {
			 if (hasDollar) *hasDollar = true;
			 return true;
		 }
 
		 return false;
	 }

	 BSONObj getFilter() const
 	 {
	 	 return obj;
 	 }
	 
	 std::string toString() const
   	 {
	 	return obj.toString();
 	 }
	 
	 operator std::string() const 
	 { 
	    return toString(); 
	 }

#if 0

 BSONObj getModifiers() const;
 BSONElement getHint() const;
 BSONObj getReadPref() const;
 int getMaxTimeMs() const;
 bool isExplain() const;
 
 /**
  * @return true if the query object contains a read preference specification object.
  */
 static bool  hasReadPreference(const BSONObj& queryObj);
 bool hasReadPreference() const;
 bool hasHint() const;
 bool hasMaxTimeMs() const;


 /**
  * Sets the read preference for this query.
  *
  * @param pref the read preference mode for this query.
  * @param tags the set of tags to use for this query.
  */
 Query& readPref(ReadPreference pref, const BSONArray& tags);

 
 /** Provide a hint to the query.
	 @param keyPattern Key pattern for the index to use.
	 Example:
	   hint("{ts:1}")
 */
 Query& hint(BSONObj keyPattern);
 Query& hint(const std::string& indexName);
 
 /**
  * Specifies a cumulative time limit in milliseconds for processing an operation.
  * MongoDB will interrupt the operation at the earliest following interrupt point.
  */
 Query& maxTimeMs(int millis);
 
 /** Provide min and/or max index limits for the query.
	 min <= x < max
  */
 Query& minKey(const BSONObj &val);
 /**
	max is exclusive
  */
 Query& maxKey(const BSONObj &val);
 
 /** Return explain information about execution of this query instead of the actual query results.
	 Normally it is easier to use the mongo shell to run db.find(...).explain().
 */
 Query& explain();
 
 /** Use snapshot mode for the query.  Snapshot mode assures no duplicates are returned, or objects missed, which were
	 present at both the start and end of the query's execution (if an object is new during the query, or deleted during
	 the query, it may or may not be returned, even with snapshot mode).
 
	 Note that short query responses (less than 1MB) are always effectively snapshotted.
 
	 Currently, snapshot mode may not be used with sorting or explicit hints.
 */
 Query& snapshot();

#endif
	 
 private:
	 void makeComplex()
 	 { 
        if ( isComplex() )
            return;
        BSONObjBuilder b;
        b.append( "query", obj );
        obj = b.obj();
     }
	 
	 template< class T >
	 void appendComplex( const char *fieldName, const T& val ) 
	 {
		 makeComplex();
		 BSONObjBuilder b;
		 b.appendElements(obj);
		 b.append(fieldName, val);
		 obj = b.obj();
	 }
 };


} //namespace bson
} // namespace doc
} // namespace tcaplus
