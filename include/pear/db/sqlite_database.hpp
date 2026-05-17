#ifndef PEAR_DB_SQLITE_DATABASE_HPP
#define PEAR_DB_SQLITE_DATABASE_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <pear/net/db_types.hpp>
#include <string>
#include <vector>

namespace pear::db {

class Connection;

// path - relative path в workspace (логический ID файла)
// object_hash - содержимое (.peer/obj/<hash>)
// local_path - путь к подготовленному файлу на диске
// operation - 'add' | 'update' | 'delete'
struct StagedFileInfo {
    std::string path;
    std::string object_hash;
    std::string local_path;
    std::string operation;
};

class SqliteDatabase {
   public:
    explicit SqliteDatabase(const std::filesystem::path& db_path);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) noexcept = default;
    SqliteDatabase& operator=(SqliteDatabase&&) noexcept = default;

    std::vector<pear::net::WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id);
    void applyWalEntries(const std::vector<pear::net::WalEntryInfo>& entries);

    // version == 0 -> вернуть актуальную (последнюю не удалённую) версию
    std::optional<pear::net::FileUpdateInfo> getFileInfoByPath(const std::string& path,
                                                               uint64_t version);
    std::optional<std::string> getObjectHashByPath(const std::string& path);

    uint64_t addWalEntry(const pear::net::WalEntryInfo& entry);
    uint64_t getLastSeqId();
    uint64_t getNextVersion(const std::string& path);
    std::vector<pear::net::FileUpdateInfo> getAllFiles();

    void stageFile(const std::string& path, const std::string& object_hash,
                   const std::string& local_path, const std::string& operation = "add");
    void unstageFile(const std::string& path);
    std::vector<StagedFileInfo> getStagedFiles();
    void clearStaging();

    uint64_t registerDevice(const std::string& address);
    std::string getDeviceAddress(uint64_t device_id);

    void setMasterAddress(const std::string& address);
    std::string getMasterAddress();

    void setDeviceId(uint64_t id);
    uint64_t getDeviceId();

    std::vector<std::string> getAllFileStatus();

   private:
    void applyWalEntryToState(const pear::net::WalEntryInfo& entry);

    std::unique_ptr<Connection> conn_;
};

}  // namespace pear::db

#endif  // PEAR_DB_SQLITE_DATABASE_HPP
