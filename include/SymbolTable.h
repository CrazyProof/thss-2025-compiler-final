#pragma once

#include "IR.h"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace sysy {

// Symbol entry containing value and type information
struct SymbolEntry {
    Value* value;           // The IR value (alloca, global, function, etc.)
    Type* type;             // The type of the symbol
    bool isConst;           // Is it a constant?
    int constValue;         // Constant value (if applicable)
    
    SymbolEntry(Value* v = nullptr, Type* t = nullptr, bool c = false, int cv = 0)
        : value(v), type(t), isConst(c), constValue(cv) {}
};

// Scope-aware symbol table
class SymbolTable {
public:
    SymbolTable() {
        // Start with global scope
        enterScope();
    }
    
    // Enter a new scope
    void enterScope() {
        scopes.emplace_back();
    }
    
    // Exit current scope
    void exitScope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }
    
    // Add symbol to current scope
    bool addSymbol(const std::string& name, const SymbolEntry& entry) {
        if (scopes.empty()) return false;
        auto& currentScope = scopes.back();
        if (currentScope.find(name) != currentScope.end()) {
            return false; // Symbol already exists in current scope
        }
        currentScope[name] = entry;
        return true;
    }
    
    // Lookup symbol (searches from innermost to outermost scope)
    SymbolEntry* lookup(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }
    
    // Lookup symbol only in current scope
    SymbolEntry* lookupCurrent(const std::string& name) {
        if (scopes.empty()) return nullptr;
        auto& currentScope = scopes.back();
        auto found = currentScope.find(name);
        if (found != currentScope.end()) {
            return &found->second;
        }
        return nullptr;
    }
    
    // Check if we're in global scope
    bool isGlobalScope() const {
        return scopes.size() == 1;
    }
    
    // Get current scope depth
    size_t getScopeDepth() const {
        return scopes.size();
    }
    
private:
    std::vector<std::map<std::string, SymbolEntry>> scopes;
};

// Function symbol table
class FunctionTable {
public:
    // Add function
    bool addFunction(const std::string& name, Function* func, FunctionType* type) {
        if (functions.find(name) != functions.end()) {
            return false;
        }
        functions[name] = {func, type};
        return true;
    }
    
    // Lookup function
    Function* lookup(const std::string& name) {
        auto it = functions.find(name);
        if (it != functions.end()) {
            return it->second.first;
        }
        return nullptr;
    }
    
    // Get function type
    FunctionType* getType(const std::string& name) {
        auto it = functions.find(name);
        if (it != functions.end()) {
            return it->second.second;
        }
        return nullptr;
    }
    
private:
    std::map<std::string, std::pair<Function*, FunctionType*>> functions;
};

} // namespace sysy
