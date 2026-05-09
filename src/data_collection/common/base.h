//
// Created by xucong on 25-8-26.
// Copyright (c) 2025 T3CAIC Group Limited. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#pragma once

#if (defined(__cplusplus) && __cplusplus >= 201703L)

#include <iostream>
#include <filesystem>
#include <optional>
#include <any>
namespace aurora::collector {

namespace fs = std::filesystem;
template <typename T>
using optional = std::optional<T>;
using any = std::any;
template <typename T>
T any_cast(const any& a) {
    return std::any_cast<T>(a);
}
const auto nullopt = std::nullopt;
const auto overwrite_if_exists = std::filesystem::copy_options::overwrite_existing;
static fs::path normalize_path(const fs::path& p) {
    return fs::absolute(fs::path(p)).lexically_normal();
}
static fs::path relative_path(const fs::path& path, const fs::path& base) {
    return fs::relative(path, base);
}

inline char* getInstallRootPath() {
    char* install_root_path = getenv("INSTALL_ROOT_PATH");
    if (NULL != install_root_path) {
        return install_root_path;
    } else {
        std::cerr
            << "the environment variable 'INSTALL_ROOT_PATH' has not been set. "
            "Please refer to the environment variable setting document on the relevant platform."
            << std::endl;
        exit(-1);
    }
}

}

#else

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/any.hpp>
namespace aurora::collector {
    namespace fs = boost::filesystem;
    template <typename T>
    using optional = boost::optional<T>;
    using any = boost::any;
    template <typename T>
    T any_cast(const any& a) {
        return boost::any_cast<T>(a);
    }
    const auto nullopt = boost::none;
    const auto overwrite_if_exists = boost::filesystem::copy_option::overwrite_if_exists;
    static fs::path normalize_path(const fs::path& p) {
        fs::path result = fs::absolute(fs::path(p));
        for (const auto& part : p) {
            if (part == "..") {
                result = result.parent_path();
            } else if (part != ".") {
                result /= part;
            }
        }
        return result;
    }
    static fs::path relative_path(const fs::path& path, const fs::path& base) {
        fs::path result;
        auto path_it = path.begin();
        auto base_it = base.begin();
        while(path_it != path.end() && base_it != base.end() && *path_it == *base_it) {
            ++path_it;
            ++base_it;
        }
        for(; base_it != base.end(); ++base_it) {
            result /= "..";
        }
        for(; path_it != path.end(); ++path_it) {
            result /= *path_it;
        }
        return result;
    }

    inline char* getInstallRootPath() {
        char* install_root_path = getenv("INSTALL_ROOT_PATH");
        if (NULL != install_root_path) {
            return install_root_path;
        } else {
            std::cerr
                << "the environment variable 'INSTALL_ROOT_PATH' has not been set. "
                "Please refer to the environment variable setting document on the relevant platform."
                << std::endl;
            exit(-1);
        }
    }
}

#endif
