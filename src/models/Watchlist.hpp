// SPDX-License-Identifier: MIT
#pragma once

#include <string>

struct Watchlist {
    int id = 0;
    std::string name;
    std::string description;
    int sortOrder = 0;
    bool isActive = true;
    bool showInSidebar = false;
    int sidebarSlot = 0;
    std::string createdAt;
    std::string updatedAt;
};
