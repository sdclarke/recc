#include <iomanip>
#include <sstream>

namespace BloombergLP {
namespace recc {

namespace {
std::string hashToHex(const unsigned char *hash_buffer, unsigned int hash_size)
{
    std::ostringstream ss;
    for (unsigned int i = 0; i < hash_size; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash_buffer[i]);
    }
    return ss.str();
}
}
 
}
}
