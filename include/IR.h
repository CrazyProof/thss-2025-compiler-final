#pragma once

#include "Type.h"
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <sstream>
#include <map>

namespace sysy
{

    // Forward declarations
    class Module;
    class Function;
    class BasicBlock;
    class Instruction;
    class Value;

    //===----------------------------------------------------------------------===//
    // Value - Base class for all values in LLVM IR
    //===----------------------------------------------------------------------===//
    class Value
    {
    public:
        Value(Type *type, const std::string &name = "")
            : type(type), name(name) {}
        virtual ~Value() = default;

        Type *getType() const { return type; }
        const std::string &getName() const { return name; }
        void setName(const std::string &n) { name = n; }

        virtual std::string toString() const { return "%" + name; }
        virtual bool isConstant() const { return false; }

    protected:
        Type *type;
        std::string name;
    };

    //===----------------------------------------------------------------------===//
    // Constant - Constant values
    //===----------------------------------------------------------------------===//
    class Constant : public Value
    {
    public:
        Constant(Type *type) : Value(type) {}
        bool isConstant() const override { return true; }
    };

    class ConstantInt : public Constant
    {
    public:
        ConstantInt(int64_t val, unsigned bits = 32)
            : Constant(IntType::get(bits)), value(val) {}

        int64_t getValue() const { return value; }

        std::string toString() const override
        {
            return std::to_string(value);
        }

        static ConstantInt *get(int64_t val, unsigned bits = 32)
        {
            return new ConstantInt(val, bits);
        }

    private:
        int64_t value;
    };

    //===----------------------------------------------------------------------===//
    // GlobalVariable - Global variables
    //===----------------------------------------------------------------------===//
    class GlobalVariable : public Value
    {
    public:
        GlobalVariable(Type *type, const std::string &name, bool isConst = false)
            : Value(new PointerType(type), name), valueType(type), isConstant_(isConst),
              initializer(nullptr) {}

        Type *getValueType() const { return valueType; }
        bool isConst() const { return isConstant_; }

        void setInitializer(Value *init) { initializer = init; }
        Value *getInitializer() const { return initializer; }

        std::string toString() const override
        {
            return "@" + name;
        }

        std::string toIRString() const;

    private:
        Type *valueType;
        bool isConstant_;
        Value *initializer;
    };

    //===----------------------------------------------------------------------===//
    // Argument - Function argument
    //===----------------------------------------------------------------------===//
    class Argument : public Value
    {
    public:
        Argument(Type *type, const std::string &name, Function *parent, unsigned argNo)
            : Value(type, name), parent(parent), argNo(argNo) {}

        Function *getParent() const { return parent; }
        unsigned getArgNo() const { return argNo; }

    private:
        Function *parent;
        unsigned argNo;
    };

    //===----------------------------------------------------------------------===//
    // BasicBlock - A sequence of instructions
    //===----------------------------------------------------------------------===//
    class BasicBlock : public Value
    {
    public:
        BasicBlock(const std::string &name, Function *parent = nullptr)
            : Value(LabelType::get(), name), parent(parent) {}

        Function *getParent() const { return parent; }
        void setParent(Function *p) { parent = p; }

        std::list<std::unique_ptr<Instruction>> &getInstructions() { return instructions; }
        const std::list<std::unique_ptr<Instruction>> &getInstructions() const { return instructions; }

        void addInstruction(Instruction *inst);

        bool hasTerminator() const;

        std::string toIRString() const;

    private:
        Function *parent;
        std::list<std::unique_ptr<Instruction>> instructions;
    };

    //===----------------------------------------------------------------------===//
    // Function - A function definition
    //===----------------------------------------------------------------------===//
    class Function : public Value
    {
    public:
        Function(FunctionType *type, const std::string &name, Module *parent = nullptr)
            : Value(type, name), funcType(type), parent(parent), isDeclare(false) {}

        FunctionType *getFunctionType() const { return funcType; }
        Type *getReturnType() const { return funcType->getReturnType(); }
        Module *getParent() const { return parent; }
        void setParent(Module *p) { parent = p; }

        bool isDeclaration() const { return isDeclare; }
        void setDeclaration(bool d) { isDeclare = d; }

        std::vector<std::unique_ptr<Argument>> &getArguments() { return arguments; }
        const std::vector<std::unique_ptr<Argument>> &getArguments() const { return arguments; }

        std::list<std::unique_ptr<BasicBlock>> &getBasicBlocks() { return basicBlocks; }
        const std::list<std::unique_ptr<BasicBlock>> &getBasicBlocks() const { return basicBlocks; }

        void addArgument(Argument *arg) { arguments.emplace_back(arg); }
        void addBasicBlock(BasicBlock *bb)
        {
            bb->setParent(this);
            basicBlocks.emplace_back(bb);
        }

        std::string toString() const override
        {
            return "@" + name;
        }

        std::string toIRString() const;

    private:
        FunctionType *funcType;
        Module *parent;
        bool isDeclare;
        std::vector<std::unique_ptr<Argument>> arguments;
        std::list<std::unique_ptr<BasicBlock>> basicBlocks;
    };

    //===----------------------------------------------------------------------===//
    // Module - Top level container
    //===----------------------------------------------------------------------===//
    class Module
    {
    public:
        explicit Module(const std::string &name) : name(name) {}

        const std::string &getName() const { return name; }

        std::vector<std::unique_ptr<GlobalVariable>> &getGlobals() { return globals; }
        const std::vector<std::unique_ptr<GlobalVariable>> &getGlobals() const { return globals; }

        std::vector<std::unique_ptr<Function>> &getFunctions() { return functions; }
        const std::vector<std::unique_ptr<Function>> &getFunctions() const { return functions; }

        void addGlobal(GlobalVariable *global) { globals.emplace_back(global); }
        void addFunction(Function *func)
        {
            func->setParent(this);
            functions.emplace_back(func);
        }

        std::string toIRString() const;

    private:
        std::string name;
        std::vector<std::unique_ptr<GlobalVariable>> globals;
        std::vector<std::unique_ptr<Function>> functions;
    };

    //===----------------------------------------------------------------------===//
    // Instruction - Base class for all instructions
    //===----------------------------------------------------------------------===//
    enum class InstType
    {
        // Terminator instructions
        Ret,
        Br,

        // Binary operations
        Add,
        Sub,
        Mul,
        SDiv,
        SRem,

        // Comparison
        ICmp,

        // Memory operations
        Alloca,
        Load,
        Store,
        GetElementPtr,

        // Other
        Call,
        ZExt,
        Trunc,
        Bitcast
    };

    class Instruction : public Value
    {
    public:
        Instruction(InstType instType, Type *type, const std::string &name = "")
            : Value(type, name), instType(instType), parent(nullptr) {}

        InstType getInstType() const { return instType; }
        BasicBlock *getParent() const { return parent; }
        void setParent(BasicBlock *p) { parent = p; }

        std::vector<Value *> &getOperands() { return operands; }
        const std::vector<Value *> &getOperands() const { return operands; }

        void addOperand(Value *v) { operands.push_back(v); }
        Value *getOperand(size_t i) const { return operands[i]; }
        size_t getNumOperands() const { return operands.size(); }

        virtual std::string toIRString() const = 0;

    protected:
        InstType instType;
        BasicBlock *parent;
        std::vector<Value *> operands;
    };

    //===----------------------------------------------------------------------===//
    // Terminator Instructions
    //===----------------------------------------------------------------------===//

    class ReturnInst : public Instruction
    {
    public:
        ReturnInst(Value *retVal = nullptr)
            : Instruction(InstType::Ret, VoidType::get()), retVal(retVal)
        {
            if (retVal)
                addOperand(retVal);
        }

        Value *getReturnValue() const { return retVal; }
        bool hasReturnValue() const { return retVal != nullptr; }

        std::string toIRString() const override
        {
            if (retVal)
            {
                return "ret " + retVal->getType()->toString() + " " + retVal->toString();
            }
            return "ret void";
        }

    private:
        Value *retVal;
    };

    class BranchInst : public Instruction
    {
    public:
        // Unconditional branch
        BranchInst(BasicBlock *dest)
            : Instruction(InstType::Br, VoidType::get()),
              isConditional(false), condition(nullptr)
        {
            addOperand(dest);
        }

        // Conditional branch
        BranchInst(Value *cond, BasicBlock *ifTrue, BasicBlock *ifFalse)
            : Instruction(InstType::Br, VoidType::get()),
              isConditional(true), condition(cond)
        {
            addOperand(cond);
            addOperand(ifTrue);
            addOperand(ifFalse);
        }

        bool isConditionalBranch() const { return isConditional; }
        Value *getCondition() const { return condition; }

        std::string toIRString() const override
        {
            if (isConditional)
            {
                return "br i1 " + condition->toString() + ", label %" +
                       static_cast<BasicBlock *>(getOperand(1))->getName() + ", label %" +
                       static_cast<BasicBlock *>(getOperand(2))->getName();
            }
            return "br label %" + static_cast<BasicBlock *>(getOperand(0))->getName();
        }

    private:
        bool isConditional;
        Value *condition;
    };

    //===----------------------------------------------------------------------===//
    // Binary Instructions
    //===----------------------------------------------------------------------===//

    class BinaryInst : public Instruction
    {
    public:
        BinaryInst(InstType op, Value *lhs, Value *rhs, const std::string &name = "")
            : Instruction(op, lhs->getType(), name)
        {
            addOperand(lhs);
            addOperand(rhs);
        }

        Value *getLHS() const { return getOperand(0); }
        Value *getRHS() const { return getOperand(1); }

        std::string toIRString() const override
        {
            std::string opName;
            switch (instType)
            {
            case InstType::Add:
                opName = "add";
                break;
            case InstType::Sub:
                opName = "sub";
                break;
            case InstType::Mul:
                opName = "mul";
                break;
            case InstType::SDiv:
                opName = "sdiv";
                break;
            case InstType::SRem:
                opName = "srem";
                break;
            default:
                opName = "unknown";
                break;
            }
            return "%" + name + " = " + opName + " " + type->toString() + " " +
                   getLHS()->toString() + ", " + getRHS()->toString();
        }
    };

    //===----------------------------------------------------------------------===//
    // Comparison Instruction
    //===----------------------------------------------------------------------===//

    enum class ICmpPredicate
    {
        EQ,  // equal
        NE,  // not equal
        SGT, // signed greater than
        SGE, // signed greater or equal
        SLT, // signed less than
        SLE  // signed less or equal
    };

    class ICmpInst : public Instruction
    {
    public:
        ICmpInst(ICmpPredicate pred, Value *lhs, Value *rhs, const std::string &name = "")
            : Instruction(InstType::ICmp, IntType::get(1), name), predicate(pred)
        {
            addOperand(lhs);
            addOperand(rhs);
        }

        ICmpPredicate getPredicate() const { return predicate; }

        std::string toIRString() const override
        {
            std::string predName;
            switch (predicate)
            {
            case ICmpPredicate::EQ:
                predName = "eq";
                break;
            case ICmpPredicate::NE:
                predName = "ne";
                break;
            case ICmpPredicate::SGT:
                predName = "sgt";
                break;
            case ICmpPredicate::SGE:
                predName = "sge";
                break;
            case ICmpPredicate::SLT:
                predName = "slt";
                break;
            case ICmpPredicate::SLE:
                predName = "sle";
                break;
            }
            return "%" + name + " = icmp " + predName + " " +
                   getOperand(0)->getType()->toString() + " " +
                   getOperand(0)->toString() + ", " + getOperand(1)->toString();
        }

    private:
        ICmpPredicate predicate;
    };

    //===----------------------------------------------------------------------===//
    // Memory Instructions
    //===----------------------------------------------------------------------===//

    class AllocaInst : public Instruction
    {
    public:
        AllocaInst(Type *allocType, const std::string &name = "")
            : Instruction(InstType::Alloca, new PointerType(allocType), name),
              allocatedType(allocType) {}

        Type *getAllocatedType() const { return allocatedType; }

        std::string toIRString() const override
        {
            return "%" + name + " = alloca " + allocatedType->toString() + ", align 4";
        }

    private:
        Type *allocatedType;
    };

    class LoadInst : public Instruction
    {
    public:
        LoadInst(Value *ptr, const std::string &name = "")
            : Instruction(InstType::Load,
                          static_cast<PointerType *>(ptr->getType())->getPointeeType(), name)
        {
            addOperand(ptr);
        }

        Value *getPointer() const { return getOperand(0); }

        std::string toIRString() const override
        {
            return "%" + name + " = load " + type->toString() + ", " +
                   getOperand(0)->getType()->toString() + " " + getOperand(0)->toString() +
                   ", align 4";
        }
    };

    class StoreInst : public Instruction
    {
    public:
        StoreInst(Value *val, Value *ptr)
            : Instruction(InstType::Store, VoidType::get())
        {
            addOperand(val);
            addOperand(ptr);
        }

        Value *getValue() const { return getOperand(0); }
        Value *getPointer() const { return getOperand(1); }

        std::string toIRString() const override
        {
            return "store " + getOperand(0)->getType()->toString() + " " +
                   getOperand(0)->toString() + ", " +
                   getOperand(1)->getType()->toString() + " " + getOperand(1)->toString() +
                   ", align 4";
        }
    };

    class GetElementPtrInst : public Instruction
    {
    public:
        GetElementPtrInst(Type *pointeeType, Value *ptr, const std::vector<Value *> &indices,
                          const std::string &name = "")
            : Instruction(InstType::GetElementPtr, new PointerType(computeResultType(pointeeType, indices)), name),
              sourceElementType(pointeeType)
        {
            addOperand(ptr);
            for (auto idx : indices)
            {
                addOperand(idx);
            }
        }

        Type *getSourceElementType() const { return sourceElementType; }

        std::string toIRString() const override
        {
            std::string result = "%" + name + " = getelementptr " +
                                 sourceElementType->toString() + ", " +
                                 getOperand(0)->getType()->toString() + " " +
                                 getOperand(0)->toString();
            for (size_t i = 1; i < getNumOperands(); ++i)
            {
                result += ", " + getOperand(i)->getType()->toString() + " " +
                          getOperand(i)->toString();
            }
            return result;
        }

    private:
        Type *sourceElementType;

        static Type *computeResultType(Type *baseType, const std::vector<Value *> &indices)
        {
            Type *currentType = baseType;
            // Skip first index (array offset)
            for (size_t i = 1; i < indices.size(); ++i)
            {
                if (auto arrType = dynamic_cast<ArrayType *>(currentType))
                {
                    currentType = arrType->getElementType();
                }
            }
            return currentType;
        }
    };

    //===----------------------------------------------------------------------===//
    // Call Instruction
    //===----------------------------------------------------------------------===//

    class CallInst : public Instruction
    {
    public:
        CallInst(Function *callee, const std::vector<Value *> &args, const std::string &name = "")
            : Instruction(InstType::Call, callee->getReturnType(), name), callee(callee)
        {
            for (auto arg : args)
            {
                addOperand(arg);
            }
        }

        // For external function calls
        CallInst(Type *retType, const std::string &calleeName, const std::vector<Value *> &args,
                 const std::string &name = "")
            : Instruction(InstType::Call, retType, name), callee(nullptr),
              externalCalleeName(calleeName)
        {
            for (auto arg : args)
            {
                addOperand(arg);
            }
        }

        Function *getCallee() const { return callee; }
        std::string getCalleeName() const
        {
            return callee ? callee->getName() : externalCalleeName;
        }

        std::string toIRString() const override
        {
            std::string result;
            if (!type->isVoidType())
            {
                result = "%" + name + " = ";
            }
            result += "call " + type->toString() + " @" + getCalleeName() + "(";
            for (size_t i = 0; i < getNumOperands(); ++i)
            {
                if (i > 0)
                    result += ", ";
                result += getOperand(i)->getType()->toString() + " " + getOperand(i)->toString();
            }
            result += ")";
            return result;
        }

    private:
        Function *callee;
        std::string externalCalleeName;
    };

    //===----------------------------------------------------------------------===//
    // Cast Instructions
    //===----------------------------------------------------------------------===//

    class ZExtInst : public Instruction
    {
    public:
        ZExtInst(Value *val, Type *destType, const std::string &name = "")
            : Instruction(InstType::ZExt, destType, name)
        {
            addOperand(val);
        }

        std::string toIRString() const override
        {
            return "%" + name + " = zext " + getOperand(0)->getType()->toString() + " " +
                   getOperand(0)->toString() + " to " + type->toString();
        }
    };

    class TruncInst : public Instruction
    {
    public:
        TruncInst(Value *val, Type *destType, const std::string &name = "")
            : Instruction(InstType::Trunc, destType, name)
        {
            addOperand(val);
        }

        std::string toIRString() const override
        {
            return "%" + name + " = trunc " + getOperand(0)->getType()->toString() + " " +
                   getOperand(0)->toString() + " to " + type->toString();
        }
    };

} // namespace sysy
