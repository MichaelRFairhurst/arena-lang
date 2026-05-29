#include "resolve/symbols.hpp"
#include "signatures/functions.hpp"
#include "signatures/types.hpp"
#include <iostream>

using namespace arena::sema;

FunctionSymbolSet::FunctionSymbolSet(const FunctionTable *ftable) {
    auto functions = ftable->get_functions();
    for (const auto &function : ftable->get_functions()) {
        symbol_to_id[function->get_symbol()] = function->get_id();
        sorted_ids.push_back(function->get_id());
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const FunctionId &a, const FunctionId &b) {
        return a.f_id < b.f_id;
    });
}

void FunctionSymbolSet::import(const FunctionSymbolSet &other) {
    for (const auto &entry : other.symbol_to_id) {
        symbol_to_id[entry.first] = entry.second;
        sorted_ids.push_back(entry.second);
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const FunctionId &a, const FunctionId &b) {
        return a.f_id < b.f_id;
    });
}

std::optional<FunctionId> FunctionSymbolSet::get_id(FunctionSymbol symbol) const {
    auto it = symbol_to_id.find(symbol);
    if (it != symbol_to_id.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool FunctionSymbolSet::operator==(const FunctionSymbolSet &other) const {
    if (symbol_to_id.size() != other.symbol_to_id.size()) {
        return false;
    }

    auto [our_end, their_end] = std::mismatch(sorted_ids.begin(), sorted_ids.end(), other.sorted_ids.begin());
    if (our_end != sorted_ids.end() || their_end != other.sorted_ids.end()) {
        return false;
    }

    return true;
}

TypeSymbolSet::TypeSymbolSet(const TypeTable *ttable) {
    for (const auto &type : ttable->get_types()) {
        symbol_to_id[type->get_symbol()] = type->get_id();
        sorted_ids.push_back(type->get_id());
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const TypeId &a, const TypeId &b) {
        return a.t_id < b.t_id;
    });
}

void TypeSymbolSet::import(const TypeSymbolSet &other) {
    for (const auto &entry : other.symbol_to_id) {
        symbol_to_id[entry.first] = entry.second;
        sorted_ids.push_back(entry.second);
    }

    std::sort(sorted_ids.begin(), sorted_ids.end(), [](const TypeId &a, const TypeId &b) {
        return a.t_id < b.t_id;
    });
}

std::optional<TypeId> TypeSymbolSet::get_id(TypeSymbol symbol) const {
    auto it = symbol_to_id.find(symbol);
    if (it != symbol_to_id.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool TypeSymbolSet::operator==(const TypeSymbolSet &other) const {
    if (symbol_to_id.size() != other.symbol_to_id.size()) {
        return false;
    }

    auto [our_end, their_end] = std::mismatch(sorted_ids.begin(), sorted_ids.end(), other.sorted_ids.begin());
    if (our_end != sorted_ids.end() || their_end != other.sorted_ids.end()) {
        return false;
    }

    return true;
}