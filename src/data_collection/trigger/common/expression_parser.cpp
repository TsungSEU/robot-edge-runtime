//
// Created by xucong on 25-5-8.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//


#include "expression_parser.h"
#include "exprtk.hpp"
#include <cctype>
#include <set>
#include <map>
#include <stdexcept>

namespace aurora::collector {

struct ExpressionParser::Impl {
    using symbol_table_t = exprtk::symbol_table<double>;
    using expression_t = exprtk::expression<double>;
    using parser_t = exprtk::parser<double>;

    parser_t parser;
    std::map<std::string, std::unique_ptr<expression_t>> expression_cache;
    std::map<std::string, std::shared_ptr<symbol_table_t>> symbol_tables;
    std::map<std::string, std::unordered_map<std::string, double>> variables;
    expression_t* current_expr = nullptr;
    std::string current_expr_str;
    std::string last_error;

    static const std::set<std::string> reserved_words;

    std::set<std::string> extract_variable_names(const std::string& expr) const;
    bool is_reserved_word(const std::string& word) const;
    void rebuild_expression_cache();
};

// 静态成员定义
const std::set<std::string> ExpressionParser::Impl::reserved_words = {
    "and", "or", "not", "true", "false", "if", "else",
    "while", "for", "var", "in", "return", "break",
    "continue", "switch", "case"
};

ExpressionParser::ExpressionParser() : impl_(std::make_unique<Impl>()) {}

ExpressionParser::~ExpressionParser() = default;

bool ExpressionParser::compile(const std::string& expr_str, bool use_cache) {
    // check cache first before compilation.
    if (use_cache) {
        auto it = impl_->expression_cache.find(expr_str);
        if (it != impl_->expression_cache.end()) {
            impl_->current_expr = it->second.get();
            impl_->current_expr_str = expr_str;
            return true;
        }
    }

    // parse variable names and expressions.
    auto var_names = impl_->extract_variable_names(expr_str);

    // create a symbol table and expression.
    auto new_symbol_table = std::make_shared<Impl::symbol_table_t>();
    auto new_expression = std::make_unique<Impl::expression_t>();

    //  allocate memory and assign to new expression object
    auto& vars = impl_->variables[expr_str];
    for (const auto& name : var_names) {
        vars[name] = 0.0; // 默认值
        new_symbol_table->add_variable(name, vars[name]);
    }

    // register symbol table
    new_expression->register_symbol_table(*new_symbol_table);

    // parse expression
    if (!impl_->parser.compile(expr_str, *new_expression)) {
        impl_->last_error = impl_->parser.error();
        return false;
    }

    // cache
    impl_->symbol_tables[expr_str] = new_symbol_table;
    impl_->current_expr = new_expression.get();
    impl_->expression_cache[expr_str] = std::move(new_expression);
    impl_->current_expr_str = expr_str;

    return true;
}

bool ExpressionParser::set_variable(const std::string& name, double value) {
    if (!impl_->current_expr) {
        impl_->last_error = "no active expression found!";
        return false;
    }

    auto it = impl_->variables.find(impl_->current_expr_str);
    if (it == impl_->variables.end()) {
        impl_->last_error = "internal error: no symbol table for current expression found!";
        return false;
    }

    auto var_it = it->second.find(name);
    if (var_it == it->second.end()) {

        impl_->last_error = "variable '" + name + "' not found in current expression";
        return false;
    }

    var_it->second = value;
    return true;
}

void ExpressionParser::set_variables(const std::unordered_map<std::string, double>& vars) {
    if (!impl_->current_expr) return;

    auto it = impl_->variables.find(impl_->current_expr_str);
    if (it == impl_->variables.end()) return;

    for (const auto& [name, value] : vars) {
        auto var_it = it->second.find(name);
        if (var_it != it->second.end()) {
            var_it->second = value;
        }
    }
}

bool ExpressionParser::evaluate(bool& result) {
    if (!impl_->current_expr) {
        impl_->last_error = "no active expression found!";
        return false;
    }

    try {
        double eval_result = impl_->current_expr->value();
        result = (eval_result >= 0.5); // ExprTk使用1.0/0.0表示真/假
        return true;
    } catch (...) {
        impl_->last_error = "exception during expression evaluation";
        return false;
    }
}

std::vector<std::string> ExpressionParser::get_variable_names() const {
    std::vector<std::string> names;
    if (impl_->current_expr) {
        auto it = impl_->variables.find(impl_->current_expr_str);
        if (it != impl_->variables.end()) {
            names.reserve(it->second.size());
            for (const auto& [name, _] : it->second) {
                names.push_back(name);
            }
        }
    }
    return names;
}

std::string ExpressionParser::last_error() const {
    return impl_->last_error;
}

void ExpressionParser::clear_cache() {
    impl_->expression_cache.clear();
    impl_->symbol_tables.clear();
    impl_->variables.clear();
    impl_->current_expr = nullptr;
    impl_->current_expr_str.clear();
}

void ExpressionParser::add_function(const std::string& name, double (*func)(double)) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    // ensure at least one symbol table exists in the cache.
    if (impl_->symbol_tables.empty()) {
        auto default_table = std::make_shared<Impl::symbol_table_t>();
        impl_->symbol_tables["__default__"] = default_table;
    }

    // add function to default symbol table and add it into all existing tables.
    for (auto& [_, sym_table] : impl_->symbol_tables) {
        sym_table->add_function(name, func);
    }

    // compile and cache the expression.
    impl_->rebuild_expression_cache();
}

std::set<std::string> ExpressionParser::Impl::extract_variable_names(const std::string& expr) const {
    std::set<std::string> variables;
    std::string current_var;
    bool in_number = false;

    for (char c : expr) {
        if (isalpha(c) || c == '_') {
            current_var += c;
            in_number = false;
        } else if (isdigit(c) && !current_var.empty()) {
            current_var += c;
        } else {
            if (!current_var.empty() && !is_reserved_word(current_var)) {
                variables.insert(current_var);
            }
            current_var.clear();
            in_number = false;
        }
    }

    if (!current_var.empty() && !is_reserved_word(current_var)) {
        variables.insert(current_var);
    }

    return variables;
}

bool ExpressionParser::Impl::is_reserved_word(const std::string& word) const {
    return reserved_words.count(word) > 0;
}

void ExpressionParser::Impl::rebuild_expression_cache() {
    for (auto& [expr_str, expr] : expression_cache) {
        auto& sym_table = symbol_tables[expr_str];
        expr->register_symbol_table(*sym_table);
        parser.compile(expr_str, *expr);
    }
}

}
