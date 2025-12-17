#include "IRGenerator.h"
#include "antlr4-runtime.h"
#include "SysYLexer.h"
#include "SysYParser.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace antlr4;

namespace sysy
{

    IRGenerator::IRGenerator()
        : module(std::make_unique<Module>("module")), currentDeclType(nullptr),
          isConstDecl(false), blockCounter(0)
    {
        initBuiltinFunctions();
    }

    //===----------------------------------------------------------------------===//
    // Helpers
    //===----------------------------------------------------------------------===//

    void IRGenerator::initBuiltinFunctions()
    {
        // Declare common sylib functions we may call; minimal set used by tests.
        Type *voidTy = VoidType::get();
        Type *intTy = IntType::get();

        auto declareFunc = [&](Type *ret, const std::string &name,
                               const std::vector<Type *> &params)
        {
            auto *fty = new FunctionType(ret, params);
            auto *fn = new Function(fty, name, module.get());
            fn->setDeclaration(true);
            module->addFunction(fn);
            functionTable.addFunction(name, fn, fty);
        };

        declareFunc(intTy, "getint", {});
        declareFunc(intTy, "getch", {});
        declareFunc(intTy, "getarray", {new PointerType(intTy)});
        declareFunc(voidTy, "putint", {intTy});
        declareFunc(voidTy, "putch", {intTy});
        declareFunc(voidTy, "putarray", {intTy, new PointerType(intTy)});
    }

    int IRGenerator::parseNumber(const std::string &text)
    {
        if (text.size() > 2 && (text[0] == '0') && (text[1] == 'x' || text[1] == 'X'))
        {
            return std::stoi(text, nullptr, 16);
        }
        if (text.size() > 1 && text[0] == '0')
        {
            return std::stoi(text, nullptr, 8);
        }
        return std::stoi(text, nullptr, 10);
    }

    int IRGenerator::evaluateConstPrimaryExp(SysYParser::PrimaryExpContext *ctx)
    {
        if (ctx->number())
        {
            return parseNumber(ctx->number()->getText());
        }
        if (ctx->exp())
        {
            return std::any_cast<int>(visit(ctx->exp()));
        }
        // lval in const context
        auto *lval = ctx->lVal();
        SymbolEntry *entry = symbolTable.lookup(lval->IDENT()->getText());
        if (!entry || !entry->isConst)
        {
            throw std::runtime_error("Non-const symbol in const expression");
        }
        return entry->constValue;
    }

    int IRGenerator::evaluateConstUnaryExp(SysYParser::UnaryExpContext *ctx)
    {
        if (ctx->primaryExp())
        {
            return evaluateConstPrimaryExp(ctx->primaryExp());
        }
        int val = evaluateConstUnaryExp(ctx->unaryExp());
        std::string op = ctx->unaryOp()->getText();
        if (op == "+")
            return val;
        if (op == "-")
            return -val;
        return val == 0 ? 1 : 0; // '!'
    }

    int IRGenerator::evaluateConstMulExp(SysYParser::MulExpContext *ctx)
    {
        int result = evaluateConstUnaryExp(ctx->unaryExp(0));
        for (size_t i = 1; i < ctx->unaryExp().size(); ++i)
        {
            int rhs = evaluateConstUnaryExp(ctx->unaryExp(i));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "*")
                result *= rhs;
            else if (op == "/")
                result /= rhs;
            else
                result %= rhs; // %
        }
        return result;
    }

    int IRGenerator::evaluateConstAddExp(SysYParser::AddExpContext *ctx)
    {
        int result = evaluateConstMulExp(ctx->mulExp(0));
        for (size_t i = 1; i < ctx->mulExp().size(); ++i)
        {
            int rhs = evaluateConstMulExp(ctx->mulExp(i));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "+")
                result += rhs;
            else
                result -= rhs;
        }
        return result;
    }

    int IRGenerator::evaluateConstExp(SysYParser::ConstExpContext *ctx)
    {
        return evaluateConstAddExp(ctx->addExp());
    }

    Type *IRGenerator::getArrayType(Type *baseType, const std::vector<int> &dims)
    {
        Type *ty = baseType;
        for (int dim : dims)
        {
            ty = allocType<ArrayType>(ty, dim);
        }
        return ty;
    }

    Value *IRGenerator::getLValPointer(SysYParser::LValContext *ctx)
    {
        std::string name = ctx->IDENT()->getText();
        SymbolEntry *entry = symbolTable.lookup(name);
        if (!entry)
            throw std::runtime_error("Undefined variable: " + name);

        Value *ptr = entry->value;
        Type *elemType = entry->type;

        // If there are indices, build GEP
        if (!ctx->exp().empty())
        {
            std::vector<Value *> indices;
            indices.push_back(ConstantInt::get(0));
            for (auto *e : ctx->exp())
            {
                Value *idx = std::any_cast<Value *>(visit(e));
                indices.push_back(idx);
            }
            ptr = builder.createGEP(elemType, ptr, indices, builder.getTmpName());
        }
        return ptr;
    }

    Value *IRGenerator::generateShortCircuitAnd(SysYParser::LAndExpContext *ctx)
    {
        // Evaluate sequentially; no phi, returns i1.
        Value *val = std::any_cast<Value *>(visit(ctx->eqExp(0)));
        if (auto *it = dynamic_cast<IntType *>(val->getType()); it && it->getBitWidth() != 1)
        {
            val = builder.createICmpNE(val, ConstantInt::get(0));
        }
        for (size_t i = 1; i < ctx->eqExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->eqExp(i)));
            if (auto *it = dynamic_cast<IntType *>(rhs->getType()); it && it->getBitWidth() != 1)
            {
                rhs = builder.createICmpNE(rhs, ConstantInt::get(0));
            }
            // and on i1 via multiplication
            val = builder.createMul(val, rhs, builder.getTmpName());
        }
        return val;
    }

    Value *IRGenerator::generateShortCircuitOr(SysYParser::LOrExpContext *ctx)
    {
        Value *val = std::any_cast<Value *>(visit(ctx->lAndExp(0)));
        if (auto *it = dynamic_cast<IntType *>(val->getType()); it && it->getBitWidth() != 1)
        {
            val = builder.createICmpNE(val, ConstantInt::get(0));
        }
        for (size_t i = 1; i < ctx->lAndExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->lAndExp(i)));
            if (auto *it = dynamic_cast<IntType *>(rhs->getType()); it && it->getBitWidth() != 1)
            {
                rhs = builder.createICmpNE(rhs, ConstantInt::get(0));
            }
            // or: a + b then compare !=0
            Value *sum = builder.createAdd(val, rhs, builder.getTmpName());
            val = builder.createICmpNE(sum, ConstantInt::get(0, 1), builder.getTmpName());
        }
        return val;
    }

    //===----------------------------------------------------------------------===//
    // Visitors
    //===----------------------------------------------------------------------===//

    std::any IRGenerator::visitCompUnit(SysYParser::CompUnitContext *ctx)
    {
        for (auto *decl : ctx->decl())
            visit(decl);
        for (auto *func : ctx->funcDef())
            visit(func);
        return nullptr;
    }

    std::any IRGenerator::visitConstDecl(SysYParser::ConstDeclContext *ctx)
    {
        currentDeclType = ctx->bType()->INT() ? (Type *)IntType::get() : (Type *)VoidType::get();
        isConstDecl = true;
        for (auto *def : ctx->constDef())
            visit(def);
        isConstDecl = false;
        return nullptr;
    }

    std::any IRGenerator::visitConstDef(SysYParser::ConstDefContext *ctx)
    {
        std::string name = ctx->IDENT()->getText();
        std::vector<int> dims;
        for (auto *ce : ctx->constExp())
        {
            dims.push_back(evaluateConstExp(ce));
        }
        Type *varType = dims.empty() ? currentDeclType : getArrayType(currentDeclType, dims);

        if (symbolTable.isGlobalScope())
        {
            auto *gv = new GlobalVariable(varType, name, true);
            // Only handle scalar initializer; arrays default zeroinitializer
            if (auto *civ = ctx->constInitVal()->constExp())
            {
                int val = evaluateConstExp(civ);
                gv->setInitializer(ConstantInt::get(val));
            }
            module->addGlobal(gv);
            symbolTable.addSymbol(name, {gv, varType, true, gv->getInitializer() ? static_cast<ConstantInt *>(gv->getInitializer())->getValue() : 0});
        }
        else
        {
            Value *alloca = builder.createAlloca(varType, name);
            int initVal = ctx->constInitVal()->constExp() ? evaluateConstExp(ctx->constInitVal()->constExp()) : 0;
            builder.createStore(ConstantInt::get(initVal), alloca);
            symbolTable.addSymbol(name, {alloca, varType, true, initVal});
        }
        return nullptr;
    }

    std::any IRGenerator::visitVarDecl(SysYParser::VarDeclContext *ctx)
    {
        currentDeclType = ctx->bType()->INT() ? (Type *)IntType::get() : (Type *)VoidType::get();
        isConstDecl = false;
        for (auto *def : ctx->varDef())
            visit(def);
        return nullptr;
    }

    std::any IRGenerator::visitVarDef(SysYParser::VarDefContext *ctx)
    {
        std::string name = ctx->IDENT()->getText();
        std::vector<int> dims;
        for (auto *ce : ctx->constExp())
            dims.push_back(evaluateConstExp(ce));
        Type *varType = dims.empty() ? currentDeclType : getArrayType(currentDeclType, dims);

        bool hasInit = ctx->initVal() != nullptr;
        if (symbolTable.isGlobalScope())
        {
            auto *gv = new GlobalVariable(varType, name, false);
            if (hasInit && ctx->initVal()->exp())
            {
                int val = std::any_cast<int>(visit(ctx->initVal()->exp()));
                gv->setInitializer(ConstantInt::get(val));
            }
            module->addGlobal(gv);
            symbolTable.addSymbol(name, {gv, varType, false, 0});
        }
        else
        {
            Value *alloca = builder.createAlloca(varType, name);
            if (hasInit && ctx->initVal()->exp())
            {
                Value *init = std::any_cast<Value *>(visit(ctx->initVal()->exp()));
                builder.createStore(init, alloca);
            }
            symbolTable.addSymbol(name, {alloca, varType, false, 0});
        }
        return nullptr;
    }

    std::any IRGenerator::visitFuncDef(SysYParser::FuncDefContext *ctx)
    {
        Type *retType = ctx->funcType()->INT() ? (Type *)IntType::get() : (Type *)VoidType::get();
        std::vector<Type *> params;
        if (auto *fps = ctx->funcFParams())
        {
            for (auto *fp : fps->funcFParam())
            {
                std::vector<int> dims;
                for (size_t i = 1; i < fp->LBRACKET().size(); ++i)
                {
                    dims.push_back(evaluateConstExp(fp->constExp(i - 1)));
                }
                Type *pTy = fp->LBRACKET().empty() ? (Type *)IntType::get()
                                                   : (Type *)new PointerType(getArrayType(IntType::get(), dims));
                params.push_back(pTy);
            }
        }

        auto *fty = new FunctionType(retType, params);
        std::string name = ctx->IDENT()->getText();
        auto *func = new Function(fty, name, module.get());
        module->addFunction(func);
        functionTable.addFunction(name, func, fty);

        symbolTable.enterScope();

        // Build arguments
        if (ctx->funcFParams())
        {
            size_t idx = 0;
            for (auto *fp : ctx->funcFParams()->funcFParam())
            {
                auto *arg = new Argument(params[idx], fp->IDENT()->getText(), func, idx);
                func->addArgument(arg);
                // Allocate and store argument to allow addressable use
                builder.setInsertPoint(nullptr);
                BasicBlock *entry = func->getBasicBlocks().empty()
                                        ? builder.createBasicBlock("entry", func)
                                        : func->getBasicBlocks().front().get();
                builder.setInsertPoint(entry);
                Value *slot = builder.createAlloca(params[idx], arg->getName());
                builder.createStore(arg, slot);
                symbolTable.addSymbol(arg->getName(), {slot, params[idx], false, 0});
                ++idx;
            }
        }

        // Ensure we have an entry block for subsequent code
        if (func->getBasicBlocks().empty())
        {
            builder.createBasicBlock("entry", func);
        }
        builder.setInsertPoint(func->getBasicBlocks().front().get());
        visit(ctx->block());

        // Add implicit return if missing
        if (!func->getBasicBlocks().empty() && !func->getBasicBlocks().back()->hasTerminator())
        {
            if (retType->isVoidType())
                builder.createRetVoid();
            else
                builder.createRet(ConstantInt::get(0));
        }

        symbolTable.exitScope();
        return nullptr;
    }

    std::any IRGenerator::visitFuncFParams(SysYParser::FuncFParamsContext *ctx)
    {
        // Parameters are handled directly in visitFuncDef; nothing to do here.
        return visitChildren(ctx);
    }

    std::any IRGenerator::visitFuncFParam(SysYParser::FuncFParamContext *ctx)
    {
        return visitChildren(ctx);
    }

    std::any IRGenerator::visitBlock(SysYParser::BlockContext *ctx)
    {
        symbolTable.enterScope();
        for (auto *item : ctx->blockItem())
            visit(item);
        symbolTable.exitScope();
        return nullptr;
    }

    std::any IRGenerator::visitAssignStmt(SysYParser::AssignStmtContext *ctx)
    {
        Value *ptr = getLValPointer(ctx->lVal());
        Value *val = std::any_cast<Value *>(visit(ctx->exp()));
        builder.createStore(val, ptr);
        return nullptr;
    }

    std::any IRGenerator::visitExpStmt(SysYParser::ExpStmtContext *ctx)
    {
        if (ctx->exp())
            visit(ctx->exp());
        return nullptr;
    }

    std::any IRGenerator::visitBlockStmt(SysYParser::BlockStmtContext *ctx)
    {
        visit(ctx->block());
        return nullptr;
    }

    std::any IRGenerator::visitIfStmt(SysYParser::IfStmtContext *ctx)
    {
        Function *func = builder.getCurrentFunction();
        BasicBlock *thenBB = builder.createBasicBlock("if.then", func);
        BasicBlock *elseBB = ctx->ELSE() ? builder.createBasicBlock("if.else", func) : nullptr;
        BasicBlock *mergeBB = builder.createBasicBlock("if.end", func);

        Value *cond = std::any_cast<Value *>(visit(ctx->cond()));
        if (auto *it = dynamic_cast<IntType *>(cond->getType()); it && it->getBitWidth() != 1)
        {
            cond = builder.createICmpNE(cond, ConstantInt::get(0));
        }
        if (elseBB)
            builder.createCondBr(cond, thenBB, elseBB);
        else
            builder.createCondBr(cond, thenBB, mergeBB);

        builder.setInsertPoint(thenBB);
        visit(ctx->stmt(0));
        if (!thenBB->hasTerminator())
            builder.createBr(mergeBB);

        if (elseBB)
        {
            builder.setInsertPoint(elseBB);
            visit(ctx->stmt(1));
            if (!elseBB->hasTerminator())
                builder.createBr(mergeBB);
        }

        builder.setInsertPoint(mergeBB);
        return nullptr;
    }

    std::any IRGenerator::visitWhileStmt(SysYParser::WhileStmtContext *ctx)
    {
        Function *func = builder.getCurrentFunction();
        BasicBlock *condBB = builder.createBasicBlock("while.cond", func);
        BasicBlock *bodyBB = builder.createBasicBlock("while.body", func);
        BasicBlock *endBB = builder.createBasicBlock("while.end", func);

        builder.createBr(condBB);

        builder.setInsertPoint(condBB);
        Value *cond = std::any_cast<Value *>(visit(ctx->cond()));
        if (auto *it = dynamic_cast<IntType *>(cond->getType()); it && it->getBitWidth() != 1)
        {
            cond = builder.createICmpNE(cond, ConstantInt::get(0));
        }
        builder.createCondBr(cond, bodyBB, endBB);

        breakTargets.push(endBB);
        continueTargets.push(condBB);

        builder.setInsertPoint(bodyBB);
        visit(ctx->stmt());
        if (!bodyBB->hasTerminator())
            builder.createBr(condBB);

        breakTargets.pop();
        continueTargets.pop();

        builder.setInsertPoint(endBB);
        return nullptr;
    }

    std::any IRGenerator::visitBreakStmt(SysYParser::BreakStmtContext *ctx)
    {
        if (!breakTargets.empty())
            builder.createBr(breakTargets.top());
        return nullptr;
    }

    std::any IRGenerator::visitContinueStmt(SysYParser::ContinueStmtContext *ctx)
    {
        if (!continueTargets.empty())
            builder.createBr(continueTargets.top());
        return nullptr;
    }

    std::any IRGenerator::visitReturnStmt(SysYParser::ReturnStmtContext *ctx)
    {
        if (ctx->exp())
        {
            Value *val = std::any_cast<Value *>(visit(ctx->exp()));
            builder.createRet(val);
        }
        else
        {
            builder.createRetVoid();
        }
        return nullptr;
    }

    std::any IRGenerator::visitCond(SysYParser::CondContext *ctx)
    {
        return visit(ctx->lOrExp());
    }

    std::any IRGenerator::visitExp(SysYParser::ExpContext *ctx)
    {
        return visit(ctx->addExp());
    }

    std::any IRGenerator::visitLVal(SysYParser::LValContext *ctx)
    {
        Value *ptr = getLValPointer(ctx);
        if (auto *pt = dynamic_cast<PointerType *>(ptr->getType()))
        {
            return (Value *)builder.createLoad(ptr, builder.getTmpName());
        }
        return ptr;
    }

    std::any IRGenerator::visitPrimaryExp(SysYParser::PrimaryExpContext *ctx)
    {
        if (ctx->exp())
            return visit(ctx->exp());
        if (ctx->number())
        {
            int val = parseNumber(ctx->number()->getText());
            return (Value *)ConstantInt::get(val);
        }
        return visit(ctx->lVal());
    }

    std::any IRGenerator::visitNumber(SysYParser::NumberContext *ctx)
    {
        int val = parseNumber(ctx->getText());
        return (Value *)ConstantInt::get(val);
    }

    std::any IRGenerator::visitUnaryExp(SysYParser::UnaryExpContext *ctx)
    {
        if (ctx->primaryExp())
            return visit(ctx->primaryExp());
        if (ctx->IDENT())
        {
            // Function call
            std::string name = ctx->IDENT()->getText();
            Function *callee = functionTable.lookup(name);
            std::vector<Value *> args;
            if (ctx->funcRParams())
            {
                for (auto *e : ctx->funcRParams()->exp())
                {
                    args.push_back(std::any_cast<Value *>(visit(e)));
                }
            }
            if (callee)
                return (Value *)builder.createCall(callee, args, builder.getTmpName());
            // external call with int return
            return (Value *)builder.createCall(IntType::get(), name, args, builder.getTmpName());
        }

        Value *operand = std::any_cast<Value *>(visit(ctx->unaryExp()));
        std::string op = ctx->unaryOp()->getText();
        if (op == "+")
            return operand;
        if (op == "-")
        {
            if (operand->isConstant())
            {
                auto *ci = dynamic_cast<ConstantInt *>(operand);
                return (Value *)ConstantInt::get(-ci->getValue());
            }
            return (Value *)builder.createSub(ConstantInt::get(0), operand, builder.getTmpName());
        }
        // '!'
        if (auto *it = dynamic_cast<IntType *>(operand->getType()); it && it->getBitWidth() != 1)
        {
            operand = builder.createICmpEQ(operand, ConstantInt::get(0));
        }
        return operand;
    }

    std::any IRGenerator::visitMulExp(SysYParser::MulExpContext *ctx)
    {
        Value *val = std::any_cast<Value *>(visit(ctx->unaryExp(0)));
        for (size_t i = 1; i < ctx->unaryExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->unaryExp(i)));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "*")
                val = builder.createMul(val, rhs, builder.getTmpName());
            else if (op == "/")
                val = builder.createSDiv(val, rhs, builder.getTmpName());
            else
                val = builder.createSRem(val, rhs, builder.getTmpName());
        }
        return val;
    }

    std::any IRGenerator::visitAddExp(SysYParser::AddExpContext *ctx)
    {
        Value *val = std::any_cast<Value *>(visit(ctx->mulExp(0)));
        for (size_t i = 1; i < ctx->mulExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->mulExp(i)));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "+")
                val = builder.createAdd(val, rhs, builder.getTmpName());
            else
                val = builder.createSub(val, rhs, builder.getTmpName());
        }
        return val;
    }

    std::any IRGenerator::visitRelExp(SysYParser::RelExpContext *ctx)
    {
        Value *val = std::any_cast<Value *>(visit(ctx->addExp(0)));
        if (ctx->addExp().size() == 1)
            return val;
        for (size_t i = 1; i < ctx->addExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->addExp(i)));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "<")
                val = builder.createICmpSLT(val, rhs, builder.getTmpName());
            else if (op == "<=")
                val = builder.createICmpSLE(val, rhs, builder.getTmpName());
            else if (op == ">")
                val = builder.createICmpSGT(val, rhs, builder.getTmpName());
            else
                val = builder.createICmpSGE(val, rhs, builder.getTmpName());
        }
        return val;
    }

    std::any IRGenerator::visitEqExp(SysYParser::EqExpContext *ctx)
    {
        Value *val = std::any_cast<Value *>(visit(ctx->relExp(0)));
        if (ctx->relExp().size() == 1)
            return val;
        for (size_t i = 1; i < ctx->relExp().size(); ++i)
        {
            Value *rhs = std::any_cast<Value *>(visit(ctx->relExp(i)));
            std::string op = ctx->children[2 * i - 1]->getText();
            if (op == "==")
                val = builder.createICmpEQ(val, rhs, builder.getTmpName());
            else
                val = builder.createICmpNE(val, rhs, builder.getTmpName());
        }
        return val;
    }

    std::any IRGenerator::visitLAndExp(SysYParser::LAndExpContext *ctx)
    {
        return generateShortCircuitAnd(ctx);
    }

    std::any IRGenerator::visitLOrExp(SysYParser::LOrExpContext *ctx)
    {
        return generateShortCircuitOr(ctx);
    }

    std::any IRGenerator::visitConstExp(SysYParser::ConstExpContext *ctx)
    {
        int v = evaluateConstExp(ctx);
        return v;
    }

} // namespace sysy
