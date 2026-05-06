#ifndef PEAR_CLI_JSON_OUTPUT_HPP
#define PEAR_CLI_JSON_OUTPUT_HPP

#include "status.hpp"

#include <pear/db/sqlite_database.hpp>
#include <pear/net/db_types.hpp>

#include <vector>

namespace pear::cli {

void print_status_json(const StatusInfo& status);

void print_ls_json(const std::vector<pear::net::FileUpdateInfo>& files, pear::db::SqliteDatabase& database);

} // namespace pear::cli

#endif // PEAR_CLI_JSON_OUTPUT_HPP
