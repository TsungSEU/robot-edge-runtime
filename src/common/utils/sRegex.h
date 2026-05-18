//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#ifndef SREGEX_H
#define SREGEX_H

#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <boost/regex.hpp>

#include "nlohmann/json.hpp"

namespace aurora::common{
using json = nlohmann::json;
using namespace boost;

bool IsMatch(const std::string &path, const std::string &regexPattern);

}

#endif
