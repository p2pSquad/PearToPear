#include "schema.hpp"

namespace pear::db {

namespace {

// Схема БД на весь MVP.
//
// - devices: зарегистрированные устройства сети
//   (address уникален, device_id auto-assign на стороне ГУ)
// - files: полная история версий метаданных файлов
//   (ключ — пара (path, version))
//   path - логический файл (relative path от корня workspace),
//   object_hash - указатель на содержимое в .peer/obj/<hash>
//   is_deleted хранит материализованное текущее состояние версии
// - seq_id назначается ГУ,
//   ВУ получает записи через UpdateDB и вставляет as-is
// - staging_files: локальный staging этого узла
//   Это не сетевое состояние, а очередь файлов для будущего push
//   operation: 'add' | 'update' | 'delete'
// - local_config: key/value конфиг этого узла
//   (master_address, свой device_id)
constexpr const char* kSchemaSql = R"sql(
CREATE TABLE IF NOT EXISTS devices(
    device_id INTEGER PRIMARY KEY AUTOINCREMENT,
    address TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS files(
    path TEXT NOT NULL,
    version INTEGER NOT NULL,
    object_hash TEXT,
    owner_device_id INTEGER NOT NULL,
    is_deleted INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(path, version)
);

CREATE INDEX IF NOT EXISTS files_by_path ON files(path);

CREATE TABLE IF NOT EXISTS wal(
    seq_id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    op_type INTEGER NOT NULL, -- 0=FILE_UPDATE, 1=DEVICE_UPDATE, 2=FILE_DELETE

    file_path TEXT,
    file_object_hash TEXT,
    file_version INTEGER,
    file_owner_device_id INTEGER,

    device_id INTEGER,
    device_address TEXT
);

CREATE TABLE IF NOT EXISTS staging_files(
    path TEXT PRIMARY KEY,
    object_hash TEXT NOT NULL,
    local_path TEXT NOT NULL,
    operation TEXT NOT NULL DEFAULT 'add'
);

CREATE TABLE IF NOT EXISTS local_config(
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
)sql";

}  // namespace

void ensure_schema(Connection& c) {
    c.begin();
    try {
        c.exec(kSchemaSql);
        c.commit();
    } catch (...) {
        c.rollback();
        throw;
    }
}

}  // namespace pear::db
