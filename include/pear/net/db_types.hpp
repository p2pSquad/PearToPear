#ifndef PEAR_NET_DB_TYPES_HPP_
#define PEAR_NET_DB_TYPES_HPP_

#include <cstdint>
#include <string>

namespace pear::net {

enum WalOpTypeInfo { kFileUpdate = 0, kDeviceUpdate = 1, kFileDelete = 2 };

// path - логический файл в общем репозитории (relative path от корня workspace)
// object_hash - конкретная версия содержимого, лежит в .peer/obj/<hash>
// version - монотонный номер версии для данного path
struct FileUpdateInfo {
    std::string path;
    std::string object_hash;
    uint64_t version;
    uint64_t owner_device_id;
};

struct FileDeleteInfo {
    std::string path;
    uint64_t version;
    uint64_t owner_device_id;
};

struct DeviceUpdateInfo {
    uint64_t device_id;
    std::string address;
};

struct WalEntryInfo {
    uint64_t seq_id;
    uint64_t timestamp;
    WalOpTypeInfo op_type;

    FileUpdateInfo file;
    FileDeleteInfo file_delete;
    DeviceUpdateInfo device;
};

}  // namespace pear::net

#endif  // PEAR_NET_DB_TYPES_HPP_
