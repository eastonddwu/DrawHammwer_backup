/** note : endian support isn't done, but work is started / sketched out */

#pragma once

#include <stdint.h>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace tcaplus {
namespace doc {

    // no PDP or anything like that at the moment, just big/little.
    //const bool big = true;
	//#if defined(__GNUC__)
	//#pragma GCC diagnostic ignored "-Wstring-compare"
	//#endif
    //const bool big = ((unsigned short&)"a") >= 0x8000;

	static inline bool IsBig()
    {
    	return (0x1234 == htons(0x1234));
    }
	const bool big = IsBig();
	
    // todo add right intrinsics for gcc
    static inline uint16_t bswap_slow16(uint16_t v) {
        return ((v & 0x00FF) << 8) |
               ((v & 0xFF00) >> 8);
    }

    static inline uint32_t bswap_slow32(uint32_t v) {
        return ((v & 0x000000FFUL) << 24) |
               ((v & 0x0000FF00UL) <<  8) |
               ((v & 0x00FF0000UL) >>  8) |
               ((v & 0xFF000000UL) >> 24);
    }

    static inline uint64_t bswap_slow64(uint64_t v) {
        return ((v & 0x00000000000000FFULL) << 56) |
               ((v & 0x000000000000FF00ULL) << 40) |
               ((v & 0x0000000000FF0000ULL) << 24) |
               ((v & 0x00000000FF000000ULL) <<  8) |
               ((v & 0x000000FF00000000ULL) >>  8) |
               ((v & 0x0000FF0000000000ULL) >> 24) |
               ((v & 0x00FF000000000000ULL) >> 40) |
               ((v & 0xFF00000000000000ULL) >> 56);
    }

#if defined(_WIN32)
    // this will be optimized to the right thing as big is known at compile time
    inline unsigned endian(unsigned x) {
        return big ? _byteswap_ulong(x) : x;
    }
    inline short endian_short(short x) {
        return big ? _byteswap_ushort(x) : x;
    }
    inline long long endian_ll(long long x) {
        return big ? _byteswap_uint64(x) : x;
    }
#elif defined(__GNUC__) && (__GNUC__ >= 4)
  #if __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 3
    inline long long endian_ll(long long x) { return big ? __builtin_bswap64(x) : x; }
    inline unsigned endian(unsigned x) { return big ? __builtin_bswap32(x) : x; }
  #else
    inline long long endian_ll(long long x) { return big ? bswap_slow64(x) : x; }
    inline unsigned endian(unsigned x) { return big ? bswap_slow32(x) : x; }
  #endif
  #if __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 8
    inline short endian_short(short x) { return big ? __builtin_bswap16(x) : x; }
  #else
    inline short endian_short(short x) { return big ? bswap_slow16(x) : x; }
  #endif
#endif

    inline int endian_int(int x) {
        return endian(x);
    }
    inline double endian_d(double x) {
        long long z = endian_ll((long long&)x);
        return (double&)z;
    }
    inline int readInt(const char *p) {
        return (int) endian(*reinterpret_cast<const unsigned *>(p));
    }



} // namespace doc
} // namespace tcaplus
