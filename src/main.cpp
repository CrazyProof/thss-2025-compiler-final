#include "IRGenerator.h"
#include "SysYLexer.h"
#include "SysYParser.h"
#include "antlr4-runtime.h"
#include <fstream>
#include <iostream>

int main(int argc, const char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: ./compiler <input-file> <output-file>" << std::endl;
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin.is_open())
    {
        std::cerr << "Failed to open input file: " << argv[1] << std::endl;
        return 1;
    }

    antlr4::ANTLRInputStream input(fin);
    SysYLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    SysYParser parser(&tokens);

    auto *tree = parser.compUnit();

    sysy::IRGenerator generator;
    generator.visitCompUnit(tree);
    sysy::Module *module = generator.getModule();

    std::ofstream fout(argv[2]);
    if (!fout.is_open())
    {
        std::cerr << "Failed to open output file: " << argv[2] << std::endl;
        return 1;
    }

    fout << module->toIRString();
    return 0;
}