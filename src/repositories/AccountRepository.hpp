// SPDX-License-Identifier: MIT
#pragma once

#include "models/Account.hpp"

#include <string>
#include <vector>

class Database;

class AccountRepository {
public:
    explicit AccountRepository(Database& database);

    std::vector<Account> listAll(std::string& error) const;
    bool create(Account& account, std::string& error) const;
    bool update(const Account& account, std::string& error) const;
    bool remove(int id, std::string& error) const;
    bool softDelete(int id, std::string& error) const;

    static bool validate(const Account& account, std::string& error);

private:
    Database& database_;
};
