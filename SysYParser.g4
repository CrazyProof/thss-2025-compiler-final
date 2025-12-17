parser grammar SysYParser;

options {
    tokenVocab = SysYLexer;
}

// Program structure
compUnit: (decl | funcDef)* EOF;

// Declarations
decl: constDecl | varDecl;

constDecl: CONST bType constDef (COMMA constDef)* SEMICOLON;
constDef: IDENT (LBRACKET constExp RBRACKET)* ASSIGN constInitVal;
constInitVal: constExp | LBRACE (constInitVal (COMMA constInitVal)*)? RBRACE;

varDecl: bType varDef (COMMA varDef)* SEMICOLON;
varDef: IDENT (LBRACKET constExp RBRACKET)* (ASSIGN initVal)?;
initVal: exp | LBRACE (initVal (COMMA initVal)*)? RBRACE;

// Types
bType: INT | VOID;

// Function definition
funcDef: funcType IDENT LPAREN funcFParams? RPAREN block;
funcType: VOID | INT;
funcFParams: funcFParam (COMMA funcFParam)*;
funcFParam: bType IDENT (LBRACKET RBRACKET (LBRACKET constExp RBRACKET)*)?;

// Block and statements
block: LBRACE blockItem* RBRACE;
blockItem: decl | stmt;

stmt: lVal ASSIGN exp SEMICOLON                          # assignStmt
    | exp? SEMICOLON                                     # expStmt
    | block                                              # blockStmt
    | IF LPAREN cond RPAREN stmt (ELSE stmt)?            # ifStmt
    | WHILE LPAREN cond RPAREN stmt                      # whileStmt
    | BREAK SEMICOLON                                    # breakStmt
    | CONTINUE SEMICOLON                                 # continueStmt
    | RETURN exp? SEMICOLON                              # returnStmt
    ;

// Expressions
exp: addExp;
cond: lOrExp;
lVal: IDENT (LBRACKET exp RBRACKET)*;
primaryExp: LPAREN exp RPAREN | lVal | number;
number: DECIMAL_CONST | OCTAL_CONST | HEX_CONST;
unaryExp: primaryExp 
        | IDENT LPAREN funcRParams? RPAREN 
        | unaryOp unaryExp;
unaryOp: PLUS | MINUS | NOT;
funcRParams: exp (COMMA exp)*;
mulExp: unaryExp ((MUL | DIV | MOD) unaryExp)*;
addExp: mulExp ((PLUS | MINUS) mulExp)*;
relExp: addExp ((LT | GT | LE | GE) addExp)*;
eqExp: relExp ((EQ | NEQ) relExp)*;
lAndExp: eqExp (AND eqExp)*;
lOrExp: lAndExp (OR lAndExp)*;
constExp: addExp;