//
// Created by xucong on 25-5-8.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#include "trigger_checker.h"
#include <boost/regex.hpp>
#include <stdexcept>
#include <algorithm>

namespace aurora::collector {

using namespace std;

TriggerChecker::TriggerChecker()
    : parser_(std::make_unique<ExpressionParser>()) {}

TriggerChecker::~TriggerChecker() = default;

bool TriggerChecker::parse(const string& condition) {
    last_error_.clear();
    elements_.clear();

    if (!parser_->compile(condition)) {
        last_error_ = parser_->last_error();
        return false;
    }

    try {
        extractElements(condition);
        return true;
    } catch (const exception& e) {
        last_error_ = e.what();
        elements_.clear();
        return false;
    }
}

bool TriggerChecker::executeCheck(const unordered_map<string, Value>& variables) {
    last_error_.clear();

    // 设置变量值
    for (const auto& [name, value] : variables) {
        bool success = false;
        try {
            if (value.index() == 0) {
                success = parser_->set_variable(name, get<double>(value));
            } else {
                double bool_value = get<bool>(value) ? 1.0 : 0.0;
                success = parser_->set_variable(name, bool_value);
            }
        } catch (const std::bad_variant_access&) {
            last_error_ = "Failed to set variable: " + name + " - type mismatch";
            return false;
        }

        if (!success) {
            last_error_ = "Failed to set variable: " + name;
            return false;
        }
    }

    // 评估表达式
    bool result;
    if (!parser_->evaluate(result)) {
        last_error_ = "Expression evaluation failed: " + parser_->last_error();
        return false;
    }

    return result;
}


string TriggerChecker::lastError() const {
    return last_error_;
}

vector<TriggerChecker::ConditionElement>
TriggerChecker::getElements() const {
    return elements_;
}

void TriggerChecker::extractElements(const string& condition) {
    static const boost::regex compare_re(R"((\w+)\s*(>=|<=|>|<|=|==|!=)\s*([\d\.]+))", boost::regex::optimize);
    static const boost::regex bool_re(R"((\w+)\s*(==|=|!=)\s*(true|false))", boost::regex::icase | boost::regex::optimize);
    static const boost::regex var_re(R"(^\s*\w+\s*$)", boost::regex::optimize);

    auto sub_exprs = splitLogicalOps(condition);

    for (const auto& [expr, op] : sub_exprs) {
        string clean_expr = trim(expr);
        if (clean_expr.empty()) continue;

        ConditionElement conditionElement;
        conditionElement.logical_op = op;

        // 处理 NOT 操作符
        if (clean_expr.find("not ") == 0 || clean_expr.find("NOT ") == 0) {
            conditionElement.logical_op = "not";
            clean_expr = trim(clean_expr.substr(clean_expr.find(' ') + 1));
        } else if (clean_expr.find('!') == 0) {
            conditionElement.logical_op = "not";
            clean_expr = trim(clean_expr.substr(1));
        }

        boost::smatch matches;

        // 优先处理布尔值比较表达式
        if (boost::regex_search(clean_expr, matches, bool_re)) {
            conditionElement.variable = matches[1].str();
            conditionElement.comparison_op = matches[2].str();
            string val = matches[3].str();
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            conditionElement.threshold = (val == "true");
            elements_.push_back(std::move(conditionElement));
            continue;
        }

        // 处理数值比较表达式
        if (boost::regex_search(clean_expr, matches, compare_re)) {
            conditionElement.variable = matches[1].str();
            conditionElement.comparison_op = matches[2].str();
            try {
                conditionElement.threshold = stod(matches[3].str());
            } catch (const std::invalid_argument& e) {
                last_error_ = "Invalid number format in: " + clean_expr;
                throw std::runtime_error(last_error_); // 抛出异常中断处理
            } catch (const std::out_of_range& e) {
                last_error_ = "Number out of range in: " + clean_expr;
                throw std::runtime_error(last_error_); // 抛出异常中断处理
            }
            elements_.push_back(std::move(conditionElement));
            continue;
        }

        // 处理单变量表达式（默认为 true）
        if (boost::regex_match(clean_expr, var_re)) {
            conditionElement.variable = clean_expr;
            conditionElement.comparison_op = "==";
            conditionElement.threshold = true;
            elements_.push_back(std::move(conditionElement));
            continue;
        }

        last_error_ = "Unrecognized condition format: " + clean_expr;
    }
}


vector<pair<string, string>> TriggerChecker::splitLogicalOps(const string& expr) {
    vector<pair<string, string>> result;
    size_t start = 0;
    int paren_level = 0;
    string last_op;

    const string AND_OP = " and ";
    const string OR_OP = " or ";
    const size_t AND_LEN = AND_OP.size();
    const size_t OR_LEN = OR_OP.size();

    for (size_t i = 0; i < expr.size(); ++i) {
        char c = expr[i];

        if (c == '(') {
            paren_level++;
        } else if (c == ')') {
            paren_level--;
        }

        if (paren_level == 0) {
            if (i + AND_LEN <= expr.size() && expr.compare(i, AND_LEN, AND_OP) == 0) {
                result.emplace_back(expr.substr(start, i - start), last_op);
                last_op = "and";
                start = i + AND_LEN;
                i += AND_LEN - 1;
            }
            else if (i + OR_LEN <= expr.size() && expr.compare(i, OR_LEN, OR_OP) == 0) {
                result.emplace_back(expr.substr(start, i - start), last_op);
                last_op = "or";
                start = i + OR_LEN;
                i += OR_LEN - 1;
            }
        }
    }

    if (start < expr.size()) {
        result.emplace_back(expr.substr(start), last_op);
    }

    return result;
}


string TriggerChecker::trim(const string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto end = std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(start), ::isspace).base();
    return string(start, end);
}

}
