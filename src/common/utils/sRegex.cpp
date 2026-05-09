//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 T3CAIC. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#include "sRegex.h"

namespace aurora::common{

bool IsMatch(const std::string &path, const std::string &regexPattern) {
    try
    {
        boost::regex pattern(regexPattern);
        return boost::regex_match(path, pattern);
    }
    catch (const boost::regex_error &e)
    {
        std::cerr << "Regex error: " << e.what() << std::endl;
        return false;
    }
}

}
