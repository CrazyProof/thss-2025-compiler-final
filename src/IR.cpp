#include "IR.h"
#include <sstream>

namespace sysy
{

    //===----------------------------------------------------------------------===//
    // GlobalVariable implementation
    //===----------------------------------------------------------------------===//

    std::string GlobalVariable::toIRString() const
    {
        std::stringstream ss;
        ss << "@" << name << " = ";

        if (isConstant_)
        {
            ss << "constant ";
        }
        else
        {
            ss << "global ";
        }

        ss << valueType->toString();

        if (initializer)
        {
            ss << " " << initializer->toString();
        }
        else
        {
            // Default initialization
            if (valueType->isIntType())
            {
                ss << " 0";
            }
            else if (valueType->isArrayType())
            {
                ss << " zeroinitializer";
            }
        }

        ss << ", align 4";
        return ss.str();
    }

    //===----------------------------------------------------------------------===//
    // BasicBlock implementation
    //===----------------------------------------------------------------------===//

    void BasicBlock::addInstruction(Instruction *inst)
    {
        inst->setParent(this);
        instructions.emplace_back(inst);
    }

    bool BasicBlock::hasTerminator() const
    {
        if (instructions.empty())
            return false;
        const auto &last = instructions.back();
        return last->getInstType() == InstType::Ret ||
               last->getInstType() == InstType::Br;
    }

    std::string BasicBlock::toIRString() const
    {
        std::stringstream ss;
        ss << name << ":\n";
        for (const auto &inst : instructions)
        {
            ss << "  " << inst->toIRString() << "\n";
        }
        return ss.str();
    }

    //===----------------------------------------------------------------------===//
    // Function implementation
    //===----------------------------------------------------------------------===//

    std::string Function::toIRString() const
    {
        std::stringstream ss;

        if (isDeclare)
        {
            ss << "declare ";
        }
        else
        {
            ss << "define ";
        }

        ss << funcType->getReturnType()->toString() << " @" << name << "(";

        if (isDeclare)
        {
            const auto &paramTypes = funcType->getParamTypes();
            for (size_t i = 0; i < paramTypes.size(); ++i)
            {
                if (i > 0)
                    ss << ", ";
                ss << paramTypes[i]->toString();
            }
        }
        else
        {
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                if (i > 0)
                    ss << ", ";
                ss << arguments[i]->getType()->toString();
                ss << " %" << arguments[i]->getName();
            }
        }

        ss << ")";

        if (isDeclare)
        {
            ss << "\n";
        }
        else
        {
            ss << " {\n";
            for (const auto &bb : basicBlocks)
            {
                ss << bb->toIRString();
            }
            ss << "}\n";
        }

        return ss.str();
    }

    //===----------------------------------------------------------------------===//
    // Module implementation
    //===----------------------------------------------------------------------===//

    std::string Module::toIRString() const
    {
        std::stringstream ss;

        ss << "; ModuleID = '" << name << "'\n";
        ss << "source_filename = \"" << name << "\"\n\n";

        // Global variables
        for (const auto &global : globals)
        {
            ss << global->toIRString() << "\n";
        }

        if (!globals.empty())
        {
            ss << "\n";
        }

        // Function declarations first
        for (const auto &func : functions)
        {
            if (func->isDeclaration())
            {
                ss << func->toIRString();
            }
        }

        if (!functions.empty())
        {
            ss << "\n";
        }

        // Function definitions
        for (const auto &func : functions)
        {
            if (!func->isDeclaration())
            {
                ss << func->toIRString() << "\n";
            }
        }

        return ss.str();
    }

} // namespace sysy
