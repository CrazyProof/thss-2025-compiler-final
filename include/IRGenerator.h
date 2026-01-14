#pragma once

#include "SysYParserBaseVisitor.h"
#include "IR.h"
#include "IRBuilder.h"
#include "SymbolTable.h"
#include "Type.h"
#include <memory>
#include <stack>
#include <vector>
#include <unordered_map>

namespace sysy
{

    class IRGenerator : public SysYParserBaseVisitor
    {
    public:
        IRGenerator();

        // Get the generated module
        Module *getModule() { return module.get(); }

        // Main entry point
        antlrcpp::Any visitCompUnit(SysYParser::CompUnitContext *ctx) override;

        // Declarations
        antlrcpp::Any visitConstDecl(SysYParser::ConstDeclContext *ctx) override;
        antlrcpp::Any visitConstDef(SysYParser::ConstDefContext *ctx) override;
        antlrcpp::Any visitVarDecl(SysYParser::VarDeclContext *ctx) override;
        antlrcpp::Any visitVarDef(SysYParser::VarDefContext *ctx) override;

        // Function
        antlrcpp::Any visitFuncDef(SysYParser::FuncDefContext *ctx) override;
        antlrcpp::Any visitFuncFParams(SysYParser::FuncFParamsContext *ctx) override;
        antlrcpp::Any visitFuncFParam(SysYParser::FuncFParamContext *ctx) override;

        // Block and statements
        antlrcpp::Any visitBlock(SysYParser::BlockContext *ctx) override;
        antlrcpp::Any visitAssignStmt(SysYParser::AssignStmtContext *ctx) override;
        antlrcpp::Any visitExpStmt(SysYParser::ExpStmtContext *ctx) override;
        antlrcpp::Any visitBlockStmt(SysYParser::BlockStmtContext *ctx) override;
        antlrcpp::Any visitIfStmt(SysYParser::IfStmtContext *ctx) override;
        antlrcpp::Any visitWhileStmt(SysYParser::WhileStmtContext *ctx) override;
        antlrcpp::Any visitBreakStmt(SysYParser::BreakStmtContext *ctx) override;
        antlrcpp::Any visitContinueStmt(SysYParser::ContinueStmtContext *ctx) override;
        antlrcpp::Any visitReturnStmt(SysYParser::ReturnStmtContext *ctx) override;

        // Expressions
        antlrcpp::Any visitExp(SysYParser::ExpContext *ctx) override;
        antlrcpp::Any visitCond(SysYParser::CondContext *ctx) override;
        antlrcpp::Any visitLVal(SysYParser::LValContext *ctx) override;
        antlrcpp::Any visitPrimaryExp(SysYParser::PrimaryExpContext *ctx) override;
        antlrcpp::Any visitNumber(SysYParser::NumberContext *ctx) override;
        antlrcpp::Any visitUnaryExp(SysYParser::UnaryExpContext *ctx) override;
        antlrcpp::Any visitMulExp(SysYParser::MulExpContext *ctx) override;
        antlrcpp::Any visitAddExp(SysYParser::AddExpContext *ctx) override;
        antlrcpp::Any visitRelExp(SysYParser::RelExpContext *ctx) override;
        antlrcpp::Any visitEqExp(SysYParser::EqExpContext *ctx) override;
        antlrcpp::Any visitLAndExp(SysYParser::LAndExpContext *ctx) override;
        antlrcpp::Any visitLOrExp(SysYParser::LOrExpContext *ctx) override;
        antlrcpp::Any visitConstExp(SysYParser::ConstExpContext *ctx) override;

    private:
        // Helper methods
        void initBuiltinFunctions();
        int parseNumber(const std::string &text);
        int evaluateConstExp(SysYParser::ConstExpContext *ctx);
        int evaluateConstAddExp(SysYParser::AddExpContext *ctx);
        int evaluateConstMulExp(SysYParser::MulExpContext *ctx);
        int evaluateConstUnaryExp(SysYParser::UnaryExpContext *ctx);
        int evaluateConstPrimaryExp(SysYParser::PrimaryExpContext *ctx);

        Value *getLValPointer(SysYParser::LValContext *ctx);
        Type *getArrayType(Type *baseType, const std::vector<int> &dims);

        // Name helpers to keep SSA names unique
        std::string uniqueValueName(const std::string &base);
        std::string uniqueBlockName(const std::string &base);

        // Generate short-circuit evaluation for logical expressions
        Value *generateShortCircuitAnd(SysYParser::LAndExpContext *ctx);
        Value *generateShortCircuitOr(SysYParser::LOrExpContext *ctx);

        // Module and builder
        std::unique_ptr<Module> module;
        IRBuilder builder;

        // Symbol tables
        SymbolTable symbolTable;
        FunctionTable functionTable;

        // Current type being declared
        Type *currentDeclType;
        bool isConstDecl;

        // Loop control
        std::stack<BasicBlock *> breakTargets;
        std::stack<BasicBlock *> continueTargets;

        // Block counter for unique naming
        unsigned blockCounter;

        // Track how many times a source name was used to uniquify SSA names
        std::unordered_map<std::string, unsigned> valueNameCounters;

        // Type management
        std::vector<std::unique_ptr<Type>> allocatedTypes;

        template <typename T, typename... Args>
        T *allocType(Args &&...args)
        {
            auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
            T *raw = ptr.get();
            allocatedTypes.push_back(std::move(ptr));
            return raw;
        }
    };

} // namespace sysy
