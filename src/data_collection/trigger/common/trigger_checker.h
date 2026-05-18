//
// Created by xucong on 25-5-8.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#ifndef TRIGGERCONDITION_CHECKER_H
#define TRIGGERCONDITION_CHECKER_H

#include "expression_parser.h"
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>


namespace aurora::collector {

class TriggerChecker {
public:
    using Value = std::variant<double, int, bool>;

    struct ConditionElement {
        std::string variable;
        std::string comparison_op;
        std::variant<double, bool> threshold;
        std::string logical_op;

        std::string threshold_str() const {
            if (std::holds_alternative<double>(threshold)) {
                return std::to_string(std::get<double>(threshold));
            }
            return std::get<bool>(threshold) ? "true" : "false";
        }
    };

    TriggerChecker();
    ~TriggerChecker();

    // 禁止拷贝和赋值
    TriggerChecker(const TriggerChecker&) = delete;
    TriggerChecker& operator=(const TriggerChecker&) = delete;

    bool parse(const std::string& condition);
    bool executeCheck(const std::unordered_map<std::string, Value>& variables);
    std::string lastError() const;
    std::vector<ConditionElement> getElements() const;

private:
    std::unique_ptr<ExpressionParser> parser_;
    std::vector<ConditionElement> elements_;
    std::string last_error_;

    void extractElements(const std::string& condition);
    static std::vector<std::pair<std::string, std::string>> splitLogicalOps(const std::string& expr);
    static std::string trim(const std::string& s);
};


}

#endif //TRIGGERCONDITION_CHECKER_H
