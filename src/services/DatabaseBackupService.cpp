// SPDX-License-Identifier: MIT
#include "services/DatabaseBackupService.hpp"

#include "db/Database.hpp"
#include "util/Date.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace {

std::string timestampForFileName(const std::string& timestamp)
{
    std::string value = timestamp;
    std::replace(value.begin(), value.end(), ' ', '_');
    value.erase(std::remove(value.begin(), value.end(), ':'), value.end());
    return value;
}

std::filesystem::path uniqueBackupPath(const std::filesystem::path& folder, const std::string& timestamp)
{
    const std::string stem = "investor_command_center_backup_" + timestampForFileName(timestamp);
    std::filesystem::path path = folder / (stem + ".db");
    int suffix = 2;
    while (std::filesystem::exists(path)) {
        path = folder / (stem + "_" + std::to_string(suffix) + ".db");
        ++suffix;
    }
    return path;
}

int daysFromCivil(int year, int month, int day)
{
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned monthPrime = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153 * monthPrime + 2) / 5 + static_cast<unsigned>(day) - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

Date::DateParts civilFromDays(int days)
{
    days += 719468;
    const int era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    int year = static_cast<int>(yearOfEra) + era * 400;
    const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned monthPrime = (5 * dayOfYear + 2) / 153;
    const unsigned day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
    const int month = static_cast<int>(monthPrime) + (monthPrime < 10 ? 3 : -9);
    year += month <= 2 ? 1 : 0;
    return Date::DateParts { year, month, static_cast<int>(day) };
}

bool parseDatePrefix(const std::string& value, Date::DateParts& parts)
{
    if (value.size() < 10) {
        return false;
    }
    return Date::parseIsoDate(value.substr(0, 10), parts);
}

Date::DateParts nextMonthlyDate(const Date::DateParts& date)
{
    int year = date.year;
    int month = date.month + 1;
    if (month > 12) {
        month = 1;
        ++year;
    }
    return Date::clampDate(year, month, date.day);
}

std::string nextReminderDate(const DatabaseBackupSettings& settings)
{
    Date::DateParts lastBackup;
    if (!parseDatePrefix(settings.lastBackupAt, lastBackup)) {
        return {};
    }

    const std::string frequency = DatabaseBackupService::normalizeFrequency(settings.reminderFrequency);
    if (frequency == "Monthly") {
        const Date::DateParts next = nextMonthlyDate(lastBackup);
        return Date::formatIsoDate(next.year, next.month, next.day);
    }

    const int intervalDays = frequency == "Weekly" ? 7 : 1;
    const int nextDays = daysFromCivil(lastBackup.year, lastBackup.month, lastBackup.day) + intervalDays;
    const Date::DateParts next = civilFromDays(nextDays);
    return Date::formatIsoDate(next.year, next.month, next.day);
}

}

DatabaseBackupResult DatabaseBackupService::createBackup(Database& database, const std::string& backupFolder)
{
    DatabaseBackupResult result;
    result.backedUpAt = Date::nowIso8601();

    if (database.handle() == nullptr) {
        result.error = "SQLite database is not open.";
        return result;
    }

    if (backupFolder.empty()) {
        result.error = "Choose a backup folder in Settings before creating a database backup.";
        return result;
    }

    std::error_code filesystemError;
    const std::filesystem::path folder = std::filesystem::absolute(std::filesystem::path(backupFolder), filesystemError);
    if (filesystemError) {
        result.error = "Could not resolve backup folder: " + filesystemError.message();
        return result;
    }

    std::filesystem::create_directories(folder, filesystemError);
    if (filesystemError) {
        result.error = "Could not create backup folder: " + filesystemError.message();
        return result;
    }

    const std::filesystem::path backupPath = uniqueBackupPath(folder, result.backedUpAt);

    sqlite3* destination = nullptr;
    if (sqlite3_open(backupPath.string().c_str(), &destination) != SQLITE_OK) {
        result.error = destination == nullptr || sqlite3_errmsg(destination) == nullptr
            ? "Could not create backup database file."
            : sqlite3_errmsg(destination);
        if (destination != nullptr) {
            sqlite3_close(destination);
        }
        return result;
    }

    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", database.handle(), "main");
    if (backup == nullptr) {
        result.error = sqlite3_errmsg(destination);
        sqlite3_close(destination);
        std::filesystem::remove(backupPath, filesystemError);
        return result;
    }

    const int stepResult = sqlite3_backup_step(backup, -1);
    const int finishResult = sqlite3_backup_finish(backup);
    const int destinationError = sqlite3_errcode(destination);
    sqlite3_close(destination);

    if (stepResult != SQLITE_DONE || finishResult != SQLITE_OK || destinationError != SQLITE_OK) {
        result.error = "SQLite backup failed.";
        std::filesystem::remove(backupPath, filesystemError);
        return result;
    }

    result.success = true;
    result.backupPath = backupPath.string();
    return result;
}

bool DatabaseBackupService::isReminderDue(const DatabaseBackupSettings& settings, const std::string& today)
{
    if (!settings.reminderEnabled) {
        return false;
    }

    Date::DateParts todayParts;
    if (!Date::parseIsoDate(today, todayParts)) {
        return false;
    }

    Date::DateParts lastBackup;
    if (!parseDatePrefix(settings.lastBackupAt, lastBackup)) {
        return true;
    }

    const int todayDays = daysFromCivil(todayParts.year, todayParts.month, todayParts.day);
    const std::string frequency = normalizeFrequency(settings.reminderFrequency);
    if (frequency == "Monthly") {
        const Date::DateParts next = nextMonthlyDate(lastBackup);
        return todayDays >= daysFromCivil(next.year, next.month, next.day);
    }

    const int lastBackupDays = daysFromCivil(lastBackup.year, lastBackup.month, lastBackup.day);
    const int intervalDays = frequency == "Weekly" ? 7 : 1;
    return todayDays - lastBackupDays >= intervalDays;
}

std::string DatabaseBackupService::reminderStatusText(const DatabaseBackupSettings& settings, const std::string& today)
{
    if (!settings.reminderEnabled) {
        return "Reminder off";
    }
    if (isReminderDue(settings, today)) {
        return "Backup due";
    }

    const std::string next = nextReminderDate(settings);
    return next.empty() ? "Backup due" : "Next reminder: " + next;
}

std::string DatabaseBackupService::normalizeFrequency(const std::string& frequency)
{
    std::string normalized = frequency;
    normalized.erase(normalized.begin(), std::find_if(normalized.begin(), normalized.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), normalized.end());

    if (normalized == "Daily" || normalized == "Weekly" || normalized == "Monthly") {
        return normalized;
    }
    return "Monthly";
}
