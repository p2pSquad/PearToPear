#ifndef PEAR_NET_REMOTE_CLIENT_HPP_
#define PEAR_NET_REMOTE_CLIENT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <pear/net/db_types.hpp>

namespace pear::net {

class RemoteClient {
public:
    static uint64_t RegisterDevice(const std::string& gu_address, const std::string& my_address);
    static std::vector<WalEntryInfo> UpdateDB(const std::string& gu_address, uint64_t last_seq_id, uint64_t device_id);
    static bool PushWAL(const std::string& gu_address, uint64_t device_id, const std::vector<WalEntryInfo>& entries, std::vector<uint64_t>& out_assigned_seq_ids);
    static void DownloadFile(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path);
};

} // namespace pear::net

#endif // PEAR_NET_REMOTE_CLIENT_HPP_