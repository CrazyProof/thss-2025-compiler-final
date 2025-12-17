lexer grammar SysYLexer;

// Keywords
INT: 'int';
VOID: 'void';
CONST: 'const';
IF: 'if';
ELSE: 'else';
WHILE: 'while';
BREAK: 'break';
CONTINUE: 'continue';
RETURN: 'return';

// Operators
PLUS: '+';
MINUS: '-';
MUL: '*';
DIV: '/';
MOD: '%';
ASSIGN: '=';
EQ: '==';
NEQ: '!=';
LT: '<';
GT: '>';
LE: '<=';
GE: '>=';
NOT: '!';
AND: '&&';
OR: '||';

// Delimiters
LPAREN: '(';
RPAREN: ')';
LBRACKET: '[';
RBRACKET: ']';
LBRACE: '{';
RBRACE: '}';
COMMA: ',';
SEMICOLON: ';';

// Literals
DECIMAL_CONST: [1-9][0-9]*;
OCTAL_CONST: '0' [0-7]*;
HEX_CONST: ('0x' | '0X') [0-9a-fA-F]+;

// Identifier
IDENT: [a-zA-Z_][a-zA-Z0-9_]*;

// String literal (for putf)
STRING: '"' (~["\\\r\n] | '\\' .)* '"';

// Whitespace and comments
WS: [ \t\r\n]+ -> skip;
LINE_COMMENT: '//' ~[\r\n]* -> skip;
BLOCK_COMMENT: '/*' .*? '*/' -> skip;