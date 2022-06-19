#pragma once

#include <endian.h>

#include <cstdint>

namespace fishnet {

namespace net {

namespace sockets {

// the inline assembler code makes type blur,
// so we disable warnings for a while.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"

inline uint64_t hostToNetwork64(uint64_t host64) {
    return htobe64(host64);
}

inline uint32_t hostToNetwork32(uint32_t host32) {
    return htobe32(host32);
}

inline uint16_t hostToNetwork16(uint32_t host16) {
    return htobe16(host16);
}

inline uint64_t networkToHost64(uint64_t net64) {
    return be64toh(net64);
}

inline uint32_t networkToHost32(uint32_t net32) {
    return be32toh(net32);
}

inline uint16_t networkToHost16(uint16_t net16) {
    return be16toh(net16);
}

#pragma GCC diagnostic pop

}  // namespace sockets
}  // namespace net
}  // namespace fishnet