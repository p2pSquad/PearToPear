#include <pear/net/remote_client.hpp>

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <fstream>
#include <stdexcept>
#include <utility>

#include "p2p.grpc.pb.h"

namespace pear::net {

namespace {

void fillProtoWalEntry(WalEntry* proto_entry, const WalEntryInfo& entry) {
    proto_entry->set_seq_id(entry.seq_id);
    proto_entry->set_timestamp(entry.timestamp);
    proto_entry->set_op_type(static_cast<WalOpType>(entry.op_type));

    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        auto* file_update = proto_entry->mutable_file_update();
        file_update->set_path(entry.file.path);
        file_update->set_object_hash(entry.file.object_hash);
        file_update->set_version(entry.file.version);
        file_update->set_owner_device_id(entry.file.owner_device_id);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        auto* file_delete = proto_entry->mutable_file_delete();
        file_delete->set_path(entry.file_delete.path);
        file_delete->set_version(entry.file_delete.version);
        file_delete->set_owner_device_id(entry.file_delete.owner_device_id);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
        auto* device_update = proto_entry->mutable_device_update();
        device_update->set_device_id(entry.device.device_id);
        device_update->set_address(entry.device.address);
    }
}

WalEntryInfo parseProtoWalEntry(const WalEntry& proto_entry) {
    WalEntryInfo entry;

    entry.seq_id = proto_entry.seq_id();
    entry.timestamp = proto_entry.timestamp();
    entry.op_type = static_cast<WalOpTypeInfo>(proto_entry.op_type());

    if (proto_entry.has_file_update()) {
        entry.file.path = proto_entry.file_update().path();
        entry.file.object_hash = proto_entry.file_update().object_hash();
        entry.file.version = proto_entry.file_update().version();
        entry.file.owner_device_id = proto_entry.file_update().owner_device_id();
        return entry;
    }

    if (proto_entry.has_file_delete()) {
        entry.file_delete.path = proto_entry.file_delete().path();
        entry.file_delete.version = proto_entry.file_delete().version();
        entry.file_delete.owner_device_id = proto_entry.file_delete().owner_device_id();
        return entry;
    }

    if (proto_entry.has_device_update()) {
        entry.device.device_id = proto_entry.device_update().device_id();
        entry.device.address = proto_entry.device_update().address();
    }

    return entry;
}

} // namespace

uint64_t RemoteClient::RegisterDevice(const std::string& gu_address, const std::string& my_address) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    RegisterRequest req;
    req.set_address(my_address);
    RegisterResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->RegisterDevice(&ctx, req, &resp);
    if (!status.ok() || !resp.success()) {
        throw std::runtime_error("RegisterDevice failed: " + resp.error_message());
    }
    return resp.assigned_device_id();
}

std::vector<WalEntryInfo> RemoteClient::UpdateDB(
    const std::string& gu_address,
    uint64_t last_seq_id,
    uint64_t device_id
) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    UpdateDBRequest req;
    req.set_last_seq_id(last_seq_id);
    req.set_device_id(device_id);
    UpdateDBResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->UpdateDB(&ctx, req, &resp);
    if (!status.ok() || !resp.success()) {
        throw std::runtime_error("UpdateDB failed: " + resp.error_message());
    }

    std::vector<WalEntryInfo> entries;

    for (const auto& proto_entry : resp.entries()) {
        entries.push_back(parseProtoWalEntry(proto_entry));
    }
    return entries;
}

bool RemoteClient::PushWAL(
    const std::string& gu_address,
    uint64_t device_id,
    const std::vector<WalEntryInfo>& entries,
    std::vector<uint64_t>& out_assigned_seq_ids
) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    PushWALRequest req;
    req.set_device_id(device_id);

    for (const auto& entry : entries) {
        auto* proto_entry = req.add_entries();
        fillProtoWalEntry(proto_entry, entry);
        proto_entry->set_seq_id(0);
    }

    PushWALResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->PushWAL(&ctx, req, &resp);
    if (status.ok() && resp.success()) {
        out_assigned_seq_ids.clear();
        for (auto id : resp.assigned_seq_ids()) {
            out_assigned_seq_ids.push_back(id);
        }
        return true;
    }
    return false;
}

void RemoteClient::DownloadFile(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path) {
    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);
    DownloadRequest req;
    req.set_object_hash(object_hash);
    req.set_requester_device_id(requester_device_id);
    grpc::ClientContext ctx;
    auto reader = stub->DownloadFile(&ctx, req);
    FileChunk chunk;
    std::ofstream out(destination_path, std::ios::binary);
    while (reader->Read(&chunk)) {
        out.write(chunk.data().data(), static_cast<std::streamsize>(chunk.data().size()));
    }
    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        throw std::runtime_error("DownloadFile failed: " + status.error_message());
    }
}

} // namespace pear::net