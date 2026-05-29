#include "signatures/functions.hpp"

using namespace arena::sema;

void FunctionTable::add_function(FunctionSymbol symbol,
                    ResolvedFunction function,
                    const arena::ast::FunctionDeclaration *source) {
    auto id = registry->get_function_id(symbol);
    functions[id] = function;
    this->source[id] = source;
    function_list.push_back(function);
}

void FunctionTable::import(const FunctionTable &other) {
    for (const auto &entry : other.functions) {
        auto id = entry.first;
        auto function = entry.second;
        functions[id] = function;
        source[id] = other.source.at(id);
        function_list.push_back(function);
    }
}

std::vector<FunctionId> FunctionTable::get_ids() const {
    std::vector<FunctionId> result;
    for (const auto &entry : functions) {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end(), [](const FunctionId &a, const FunctionId &b) {
        return a.f_id < b.f_id;
    });
    return result;
}

std::vector<const ResolvedFunction*> FunctionTable::get_functions() const {
    std::vector<const ResolvedFunction*> result;
    for (const auto &entry : functions) {
        result.push_back(&entry.second);
    }
    return result;
}

std::optional<ResolvedFunction> FunctionTable::resolve(std::string_view name) const {
    auto id = registry->get_function_id(name);
    return get_function(id);
}

std::optional<ResolvedFunction> FunctionTable::get_function(FunctionId id) const {
    auto it = functions.find(id);
    if (it != functions.end()) {
        return it->second;
    }
    return std::nullopt;
}