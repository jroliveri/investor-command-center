// SPDX-License-Identifier: MIT
#pragma once

#include "models/DatabaseBackup.hpp"

#include <string>

class Database;

namespace DatabaseBackupService {
DatabaseBackupResult createBackup(Database& database, const std::string& backupFolder);
bool isReminderDue(const DatabaseBackupSettings& settings, const std::string& today);
std::string reminderStatusText(const DatabaseBackupSettings& settings, const std::string& today);
std::string normalizeFrequency(const std::string& frequency);
}
