// SPDX-License-Identifier: MIT
#pragma once

#include <string>

class SettingsView {
public:
    void render(const std::string& databasePath, const char* appVersion);
};
