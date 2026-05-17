#include <pear/net/master_service.hpp>

#include <exception>
#include <utility>

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

MasterServiceImpl::MasterServiceImpl(std::shared_ptr<pear::db::SqliteDatabase> db)
    : db_(std::move(db)) {}

grpc::Status MasterServiceImpl::RegisterDevice(
    grpc::ServerContext* /*ctx*/,
    const RegisterRequest* req,
    RegisterResponse* resp
) {
    try {
        uint64_t new_id = db_->registerDevice(req->address());

        resp->set_success(true);
        resp->set_assigned_device_id(new_id);

        auto full_wal = db_->getWalEntriesSince(0);
        for (const auto& entry : full_wal) {
            auto* proto_entry = resp->add_full_wal();
            fillProtoWalEntry(proto_entry, entry);
        }
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::UpdateDB(
    grpc::ServerContext* /*ctx*/,
    const UpdateDBRequest* req,
    UpdateDBResponse* resp
) {
    try {
        auto entries = db_->getWalEntriesSince(req->last_seq_id());

        for (const auto& entry : entries) {
            auto* proto_entry = resp->add_entries();
            fillProtoWalEntry(proto_entry, entry);
        }

        resp->set_success(true);
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

grpc::Status MasterServiceImpl::PushWAL(
    grpc::ServerContext* /*ctx*/,
    const PushWALRequest* req,
    PushWALResponse* resp
) {
    try {
        for (int i = 0; i < req->entries_size(); ++i) {
            const auto& proto_entry = req->entries(i);
            WalEntryInfo entry = parseProtoWalEntry(proto_entry);

            entry.seq_id = 0;

            uint64_t new_seq_id = db_->addWalEntry(entry);
            resp->add_assigned_seq_ids(new_seq_id);
        }

        resp->set_success(true);
    } catch (const std::exception& e) {
        resp->set_success(false);
        resp->set_error_message(e.what());
    }

    return grpc::Status::OK;
}

} // namespace pear::net