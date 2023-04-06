#ifndef DEFINITIONS_HPP
#define DEFINITIONS_HPP

#include <vector>
#include <string>

#define PS_KEYWORD_FUNC     "func"
#define PS_KEYWORD_RETURN   "ret"
#define PS_KEYWORD_IF       "if"
#define PS_KEYWORD_ELSE     "else"
#define PS_KEYWORD_FOR      "for"
#define PS_KEYWORD_BREAK    "break"
#define PS_KEYWORD_CONTINUE "continue"

//basically operators, but treated as keyword since we dont want lexer to treat them as ops:
#define PS_KEYWORD_IN     "in"
#define PS_KEYWORD_AND    "and"
#define PS_KEYWORD_OR     "or"

const std::vector<std::string> PS_KEYWORDS = {
	PS_KEYWORD_FUNC,
	PS_KEYWORD_RETURN,
	PS_KEYWORD_IF,
	PS_KEYWORD_ELSE,
	PS_KEYWORD_FOR,
	PS_KEYWORD_IN
};

//--------------------------------------------------------------------------------------------------------------------------------//

#define PS_OP_MULT             "*"
#define PS_OP_DIV              "/"
#define PS_OP_MOD              "%"
#define PS_OP_ADD              "+"
#define PS_OP_SUB              "-"
#define PS_OP_EQUAL            "="
#define PS_OP_MULTEQUAL        "*="
#define PS_OP_DIVEQUAL         "/="
#define PS_OP_MODEQUAL         "%="
#define PS_OP_ADDEQUAL         "+="
#define PS_OP_SUBEQUAL         "-="
#define PS_OP_LESSTHAN         "<"
#define PS_OP_GREATERTHAN      ">"
#define PS_OP_LESSTHANEQUAL    "<="
#define PS_OP_GREATERTHANEQUAL ">="
#define PS_OP_EQUALITY         "=="
#define PS_OP_NONEQUALITY      "!="

#define PS_SEPERATOR_PAREN_OPEN   "("
#define PS_SEPERATOR_PAREN_CLOSE  ")"
#define PS_SEPERATOR_CURLY_OPEN   "{"
#define PS_SEPERATOR_CURLY_CLOSE  "}"
#define PS_SEPERATOR_SQUARE_OPEN  "["
#define PS_SEPERATOR_SQUARE_CLOSE "]"
#define PS_SEPERATOR_COMMA        ","

#define PS_COMMENT "#"

//operators as seen by the parser
const std::vector<std::string> PS_OPERATORS = {
	PS_OP_ADD,
	PS_OP_SUB,
	PS_OP_MULT,
	PS_OP_DIV,
	PS_OP_MOD,
	PS_OP_EQUAL,
	PS_OP_ADDEQUAL,
	PS_OP_SUBEQUAL,
	PS_OP_MULTEQUAL,
	PS_OP_DIVEQUAL,
	PS_OP_LESSTHAN,
	PS_OP_GREATERTHAN,
	PS_OP_LESSTHANEQUAL,
	PS_OP_GREATERTHANEQUAL,
	PS_OP_EQUALITY,
	PS_OP_NONEQUALITY,
	PS_KEYWORD_AND,
	PS_KEYWORD_OR
};

//operators as seen by the lexer
const std::vector<std::string> PS_LEXER_OPERATORS = {
	PS_OP_ADD,
	PS_OP_SUB,
	PS_OP_MULT,
	PS_OP_DIV,
	PS_OP_MOD,
	PS_OP_EQUAL,
	PS_OP_ADDEQUAL,
	PS_OP_SUBEQUAL,
	PS_OP_MULTEQUAL,
	PS_OP_DIVEQUAL,
	PS_OP_LESSTHAN,
	PS_OP_GREATERTHAN,
	PS_OP_LESSTHANEQUAL,
	PS_OP_GREATERTHANEQUAL,
	PS_OP_EQUALITY,
	PS_OP_NONEQUALITY,

	PS_SEPERATOR_PAREN_OPEN,
	PS_SEPERATOR_PAREN_CLOSE,
	PS_SEPERATOR_CURLY_OPEN,
	PS_SEPERATOR_CURLY_CLOSE,
	PS_SEPERATOR_SQUARE_OPEN,
	PS_SEPERATOR_SQUARE_CLOSE,
	PS_SEPERATOR_COMMA,

	PS_COMMENT //added so lexer can easily identify comments
};

const std::vector<std::string> PS_CLOSED_SEPERATORS = {
	PS_SEPERATOR_PAREN_CLOSE,
	PS_SEPERATOR_CURLY_CLOSE,
	PS_SEPERATOR_SQUARE_CLOSE,
	PS_SEPERATOR_COMMA
};

#endif