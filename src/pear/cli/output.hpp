#ifndef PEAR_CLI_OUTPUT_HPP
#define PEAR_CLI_OUTPUT_HPP

#include "status.hpp"

#include <pear/db/sqlite_database.hpp>
#include <pear/net/db_types.hpp>

#include <string>
#include <vector>

namespace pear::cli {

inline constexpr const char* Grusha = "🍐 ";

void print_status_info(const StatusInfo& status);
void print_file_tree(const std::vector<pear::net::FileUpdateInfo>& files, pear::db::SqliteDatabase& database);

} // namespace pear::cli

#endif
