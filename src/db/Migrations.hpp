// SPDX-License-Identifier: MIT
#pragma once

#include <string>

class Database;

namespace Migrations {
bool run(Database& database, std::string& error);
}
