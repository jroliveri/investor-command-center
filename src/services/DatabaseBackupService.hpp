// SPDX-License-Identifier: MIT
#pragma once

#include "models/DatabaseBackup.hpp"

#include <string>

class Database;

namespace DatabaseBackupService {
DatabaseBackupResult createBackup(Database& database, const std::string& backupFolder);
bool copyDatabaseToFile(Database& database, const std::string& destinationPath, bool overwriteExisting, std::string& error);
bool verifyDatabaseFile(const std::string& databasePath, std::string& error);
bool isReminderDue(const DatabaseBackupSettings& settings, const std::string& today);
std::string reminderStatusText(const DatabaseBackupSettings& settings, const std::string& today);
std::string normalizeFrequency(const std::string& frequency);
}
