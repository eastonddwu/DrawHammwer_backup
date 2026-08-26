#pragma once

#include <cassert>

#define NOINLINE_DECL

namespace tcaplus {
namespace doc {

    class MsgAssertionException : public std::exception {
    public:
        const unsigned errorCode;
		const std::string msg;
		
        MsgAssertionException(unsigned code, const std::string &_s) : errorCode(code),msg(_s)
		{
		}

		~MsgAssertionException() throw() 
		{ 
		}

        virtual const char * what() const throw() 
		{ 
			return msg.c_str();  
		}

        virtual const char * getMsg() const throw() 
		{ 
			return msg.c_str();  
		}
		
		unsigned getCode()
		{
			return errorCode;
		}

    };

    inline void msgasserted(unsigned x, const std::string &s) 
    { 
	    throw MsgAssertionException(x, s); 
	}

    inline void massert(unsigned a, const char *b, bool x) 
	{
        if(!x)
		{
			msgasserted(a, std::string(b));
    	}
    }
	
    inline void massert(unsigned a, const std::string &b, bool x) 
	{
        if(!x) 
		{
			msgasserted(a, b);
    	}
    }

} // namespace doc
} // namespace tcaplus
