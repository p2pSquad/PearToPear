#include <pear/fs/hash.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/evp.h>

namespace pear::storage {

namespace {

using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

DigestContext make_digest_context() {
    DigestContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (context == nullptr) {
        throw std::runtime_error("failed to create hash context");
    }

    return context;
}

std::string bytes_to_hex(const unsigned char* bytes, unsigned int size) {
    static constexpr char hex_digits[] = "0123456789abcdef";

    std::string result;
    result.reserve(static_cast<std::size_t>(size) * 2);

    for (unsigned int index = 0; index < size; ++index) {
        unsigned char byte = bytes[index];
        result.push_back(hex_digits[byte >> 4]);
        result.push_back(hex_digits[byte & 0x0f]);
    }

    return result;
}

}  // namespace

std::string get_stream_hash(std::istream& input) {
    DigestContext context = make_digest_context();

    if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("failed to initialize sha256 hash");
    }

    std::array<char, 8192> buffer{};

    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize bytes_read = input.gcount();

        if (bytes_read > 0 && EVP_DigestUpdate(context.get(), buffer.data(), static_cast<std::size_t>(bytes_read)) != 1) {
            throw std::runtime_error("failed to update sha256 hash");
        }
    }

    if (input.bad()) {
        throw std::runtime_error("failed to read input stream");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_size = 0;

    if (EVP_DigestFinal_ex(context.get(), hash, &hash_size) != 1) {
        throw std::runtime_error("failed to finalize sha256 hash");
    }

    return bytes_to_hex(hash, hash_size);
}

std::string get_file_hash(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + path.string());
    }

    return get_stream_hash(file);
}

}  // namespace pear::storage