/* Minimal endian header for tree-sitter portability layer */
#ifndef PORTABLE_ENDIAN_H
#define PORTABLE_ENDIAN_H
#ifdef _WIN32
#  include <winsock2.h>
#  define htobe32(x) htonl(x)
#  define be32toh(x) ntohl(x)
#elif defined(__APPLE__)
#  include <machine/endian.h>
#  include <libkern/OSByteOrder.h>
#  define htobe32(x) OSSwapHostToBigInt32(x)
#  define be32toh(x) OSSwapBigToHostInt32(x)
#else
#  include <endian.h>
#endif
#endif /* PORTABLE_ENDIAN_H */

/* le16toh / be16toh stubs for platforms that lack them */
#ifndef le16toh
#  if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define le16toh(x) (x)
#    define be16toh(x) __builtin_bswap16(x)
#  else
#    define le16toh(x) __builtin_bswap16(x)
#    define be16toh(x) (x)
#  endif
#endif
