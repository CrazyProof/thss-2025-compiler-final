#pragma once

#include <string>
#include <vector>
#include <memory>

namespace sysy {

// Type ID enumeration
enum class TypeID {
    VoidTypeID,
    IntTypeID,
    ArrayTypeID,
    PointerTypeID,
    FunctionTypeID,
    LabelTypeID
};

// Base type class
class Type {
public:
    explicit Type(TypeID id) : typeID(id) {}
    virtual ~Type() = default;
    
    TypeID getTypeID() const { return typeID; }
    virtual std::string toString() const = 0;
    
    bool isVoidType() const { return typeID == TypeID::VoidTypeID; }
    bool isIntType() const { return typeID == TypeID::IntTypeID; }
    bool isArrayType() const { return typeID == TypeID::ArrayTypeID; }
    bool isPointerType() const { return typeID == TypeID::PointerTypeID; }
    bool isFunctionType() const { return typeID == TypeID::FunctionTypeID; }
    bool isLabelType() const { return typeID == TypeID::LabelTypeID; }
    
protected:
    TypeID typeID;
};

// Void type (singleton)
class VoidType : public Type {
public:
    static VoidType* get() {
        static VoidType instance;
        return &instance;
    }
    
    std::string toString() const override { return "void"; }
    
private:
    VoidType() : Type(TypeID::VoidTypeID) {}
};

// Integer type (with bit width, singleton for each width)
class IntType : public Type {
public:
    static IntType* get(unsigned bits = 32) {
        if (bits == 1) {
            static IntType i1(1);
            return &i1;
        } else if (bits == 32) {
            static IntType i32(32);
            return &i32;
        }
        // Default to i32
        static IntType i32Default(32);
        return &i32Default;
    }
    
    unsigned getBitWidth() const { return bitWidth; }
    
    std::string toString() const override { 
        return "i" + std::to_string(bitWidth); 
    }
    
private:
    explicit IntType(unsigned bits) : Type(TypeID::IntTypeID), bitWidth(bits) {}
    unsigned bitWidth;
};

// Label type (for basic blocks)
class LabelType : public Type {
public:
    static LabelType* get() {
        static LabelType instance;
        return &instance;
    }
    
    std::string toString() const override { return "label"; }
    
private:
    LabelType() : Type(TypeID::LabelTypeID) {}
};

// Array type
class ArrayType : public Type {
public:
    ArrayType(Type* elemType, uint64_t count)
        : Type(TypeID::ArrayTypeID), elementType(elemType), elementCount(count) {}
    
    Type* getElementType() const { return elementType; }
    uint64_t getElementCount() const { return elementCount; }
    
    std::string toString() const override {
        return "[" + std::to_string(elementCount) + " x " + elementType->toString() + "]";
    }
    
private:
    Type* elementType;
    uint64_t elementCount;
};

// Pointer type
class PointerType : public Type {
public:
    explicit PointerType(Type* pointee)
        : Type(TypeID::PointerTypeID), pointeeType(pointee) {}
    
    Type* getPointeeType() const { return pointeeType; }
    
    std::string toString() const override {
        return pointeeType->toString() + "*";
    }
    
private:
    Type* pointeeType;
};

// Function type
class FunctionType : public Type {
public:
    FunctionType(Type* retType, const std::vector<Type*>& params)
        : Type(TypeID::FunctionTypeID), returnType(retType), paramTypes(params) {}
    
    Type* getReturnType() const { return returnType; }
    const std::vector<Type*>& getParamTypes() const { return paramTypes; }
    size_t getNumParams() const { return paramTypes.size(); }
    
    std::string toString() const override {
        std::string result = returnType->toString() + " (";
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (i > 0) result += ", ";
            result += paramTypes[i]->toString();
        }
        result += ")";
        return result;
    }
    
private:
    Type* returnType;
    std::vector<Type*> paramTypes;
};

} // namespace sysy