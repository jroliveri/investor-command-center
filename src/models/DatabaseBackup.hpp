// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct DatabaseBackupSettings {
    std::string backupFolder;
    bool reminderEnabled = false;
    std::string reminderFrequency = "Monthly";
    std::string lastBackupAt;
};

struct DatabaseBackupResult {
    bool success = false;
    std::string backupPath;
    std::string backedUpAt;
    std::string error;
};
