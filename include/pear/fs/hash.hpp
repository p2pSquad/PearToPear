#ifndef PEAR_FS_HASH_HPP_
#define PEAR_FS_HASH_HPP_

#include <filesystem>
#include <iosfwd>
#include <string>

namespace pear::storage {

std::string get_stream_hash(std::istream& input);
std::string get_file_hash(const std::filesystem::path& path);

}  // namespace pear::storage

#endif  // PEAR_FS_HASH_HPP_