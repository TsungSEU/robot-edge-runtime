//
// Created by xucong on 25-5-30.
// Copyright (c) 2025 T3CAIC. All rights reserved.
//

#ifndef FILE_ROLLER_H
#define FILE_ROLLER_H

#include <string>
#include <vector>
#include <optional>
#include "../common/app_config.h"

namespace aurora::collector {

class FileRoller {
public:
    FileRoller();
    ~FileRoller() = default;

    int rollFiles();

private:
    std::string bagPath;

    std::vector<std::string> getSortedCompressedFiles() const;

};

}
#endif //FILE_ROLLER_H
