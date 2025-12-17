#pragma once

#include "IR.h"
#include <string>
#include <map>

namespace sysy
{

    class IRBuilder
    {
    public:
        IRBuilder() : currentFunction(nullptr), currentBlock(nullptr), tmpCounter(0) {}

        //===------------------------------------------------------------------===//
        // Position management
        //===------------------------------------------------------------------===//

        void setInsertPoint(BasicBlock *bb)
        {
            currentBlock = bb;
            if (bb)
                currentFunction = bb->getParent();
        }

        BasicBlock *getInsertBlock() const { return currentBlock; }
        Function *getCurrentFunction() const { return currentFunction; }

        //===------------------------------------------------------------------===//
        // Basic block creation
        //===------------------------------------------------------------------===//

        BasicBlock *createBasicBlock(const std::string &name, Function *parent = nullptr)
        {
            Function *func = parent ? parent : currentFunction;
            BasicBlock *bb = new BasicBlock(name, func);
            if (func)
                func->addBasicBlock(bb);
            return bb;
        }

        //===------------------------------------------------------------------===//
        // Terminator instructions
        //===------------------------------------------------------------------===//

        ReturnInst *createRet(Value *val)
        {
            ReturnInst *inst = new ReturnInst(val);
            insertInstruction(inst);
            return inst;
        }

        ReturnInst *createRetVoid()
        {
            ReturnInst *inst = new ReturnInst(nullptr);
            insertInstruction(inst);
            return inst;
        }

        BranchInst *createBr(BasicBlock *dest)
        {
            BranchInst *inst = new BranchInst(dest);
            insertInstruction(inst);
            return inst;
        }

        BranchInst *createCondBr(Value *cond, BasicBlock *ifTrue, BasicBlock *ifFalse)
        {
            // Ensure condition is i1
            if (auto intType = dynamic_cast<IntType *>(cond->getType()))
            {
                if (intType->getBitWidth() != 1)
                {
                    cond = createICmpNE(cond, ConstantInt::get(0));
                }
            }
            BranchInst *inst = new BranchInst(cond, ifTrue, ifFalse);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Binary operations
        //===------------------------------------------------------------------===//

        BinaryInst *createAdd(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            BinaryInst *inst = new BinaryInst(InstType::Add, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        BinaryInst *createSub(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            BinaryInst *inst = new BinaryInst(InstType::Sub, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        BinaryInst *createMul(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            BinaryInst *inst = new BinaryInst(InstType::Mul, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        BinaryInst *createSDiv(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            BinaryInst *inst = new BinaryInst(InstType::SDiv, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        BinaryInst *createSRem(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            BinaryInst *inst = new BinaryInst(InstType::SRem, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Comparison instructions
        //===------------------------------------------------------------------===//

        ICmpInst *createICmpEQ(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::EQ, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        ICmpInst *createICmpNE(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::NE, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        ICmpInst *createICmpSLT(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::SLT, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        ICmpInst *createICmpSLE(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::SLE, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        ICmpInst *createICmpSGT(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::SGT, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        ICmpInst *createICmpSGE(Value *lhs, Value *rhs, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ICmpInst *inst = new ICmpInst(ICmpPredicate::SGE, lhs, rhs, instName);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Memory instructions
        //===------------------------------------------------------------------===//

        AllocaInst *createAlloca(Type *type, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            AllocaInst *inst = new AllocaInst(type, instName);
            insertInstruction(inst);
            return inst;
        }

        LoadInst *createLoad(Value *ptr, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            LoadInst *inst = new LoadInst(ptr, instName);
            insertInstruction(inst);
            return inst;
        }

        StoreInst *createStore(Value *val, Value *ptr)
        {
            StoreInst *inst = new StoreInst(val, ptr);
            insertInstruction(inst);
            return inst;
        }

        GetElementPtrInst *createGEP(Type *pointeeType, Value *ptr,
                                     const std::vector<Value *> &indices,
                                     const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            GetElementPtrInst *inst = new GetElementPtrInst(pointeeType, ptr, indices, instName);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Call instruction
        //===------------------------------------------------------------------===//

        CallInst *createCall(Function *callee, const std::vector<Value *> &args,
                             const std::string &name = "")
        {
            std::string instName = name.empty() ? (callee->getReturnType()->isVoidType() ? "" : getTmpName()) : name;
            CallInst *inst = new CallInst(callee, args, instName);
            insertInstruction(inst);
            return inst;
        }

        CallInst *createCall(Type *retType, const std::string &calleeName,
                             const std::vector<Value *> &args,
                             const std::string &name = "")
        {
            std::string instName = name.empty() ? (retType->isVoidType() ? "" : getTmpName()) : name;
            CallInst *inst = new CallInst(retType, calleeName, args, instName);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Cast instructions
        //===------------------------------------------------------------------===//

        ZExtInst *createZExt(Value *val, Type *destType, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            ZExtInst *inst = new ZExtInst(val, destType, instName);
            insertInstruction(inst);
            return inst;
        }

        TruncInst *createTrunc(Value *val, Type *destType, const std::string &name = "")
        {
            std::string instName = name.empty() ? getTmpName() : name;
            TruncInst *inst = new TruncInst(val, destType, instName);
            insertInstruction(inst);
            return inst;
        }

        //===------------------------------------------------------------------===//
        // Utility
        //===------------------------------------------------------------------===//

        std::string getTmpName()
        {
            return "t" + std::to_string(tmpCounter++);
        }

        void resetTmpCounter()
        {
            tmpCounter = 0;
        }

    private:
        void insertInstruction(Instruction *inst)
        {
            if (currentBlock)
            {
                currentBlock->addInstruction(inst);
            }
        }

        Function *currentFunction;
        BasicBlock *currentBlock;
        unsigned tmpCounter;
    };

} // namespace sysy
