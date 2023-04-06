#ifndef PROPSCRIPT_HPP
#define PROPSCRIPT_HPP

#include "definitions.hpp"
#include "vector"
#include "string"

#include <fstream>
#include "quickmath.hpp"

//--------------------------------------------------------------------------------------------------------------------------------//
//LEXER AND PARSER STRUCTS:

//a lexical token
struct PStoken
{
	enum Type
	{
		ID,
		OP,
		NEWLINE
	} type;

	std::string str;
	uint32_t lineNum;
};

//a handle to an abstract syntax tree node
typedef uint32_t PSnodeHandle;

//an abstract syntax tree node
struct PSnode
{
	enum Type : uint32_t
	{
		OP,
		KEYWORD,
		ID,
		NUMBER
	} type;
	uint32_t lineNum;

	//----------------------//
	//OPERATOR:

	struct OP
	{
		enum Type : uint32_t
		{
			IN               = 0,  //operator precedence

			MULT             = 10,
			DIV,
			MOD,

			ADD              = 20,
			SUB,

			EQUAL            = 30,
			MULTEQUAL,
			DIVEQUAL,
			MODEQUAL,
			ADDEQUAL,
			SUBEQUAL,
						
			LESSTHAN         = 40,
			GREATERTHAN,
			LESSTHANEQUAL,
			GREATERTHANEQUAL,
			EQUALITY,
			NONEQUALITY,

			AND              = 50,
			OR
		} type;

		PSnodeHandle left;  //the left side of the operator
		PSnodeHandle right; //the right side of the operator
		bool inParens;      //whether the operator is in parenthesis
	} op;

	//----------------------//
	//KEYWORD:

	struct Keyword
	{
		enum Type
		{
			IF,
			FOR,
			FUNC,
			RETURN,
			BREAK,
			CONTINUE
		} type;

		//code for body of control flow statement:
		std::vector<PSnodeHandle> code;

		//if statement condition / for stataement iteration rule:
		PSnodeHandle condition; 

		//else statement stuff:
		bool hasElse;
		std::vector<PSnodeHandle> elseCode;

		//function names:
		std::string name;
		std::vector<std::string> paramNames;

		//return value:
		PSnodeHandle returnVal;
	} keyword;

	//----------------------//
	//IDENTIFIER:

	struct ID
	{
		enum Type : uint32_t
		{
			FUNC,
			VAR
		} type;

		std::string name;
		std::vector<PSnodeHandle> params; //if type is a variable, also represents the index into that variable
	} id;

	//----------------------//
	//LITERAL:

	struct Literal
	{
		enum Type : uint32_t
		{
			INT,
			FLOAT
		} type;

		int32_t intNum;
		float floatNum;
	} literal;

	PSnode () {};
	~PSnode() {};
};

//an abstract syntax tree
struct PSast
{
	std::vector<PSnodeHandle> parentNodes;
	std::vector<PSnode> nodePool;
};

//--------------------------------------------------------------------------------------------------------------------------------//
//INTERPRETER STRUCTS:

//a generic struct representing any possible data type
struct PSdata
{
	enum Type
	{
		VOID,

		INT,
		FLOAT,
		VEC2,
		VEC3,
		VEC4,
		QUATERNION
	} type;

	union
	{
		int32_t intVal;
		float floatVal;
		qm::vec2 vec2Val;
		qm::vec3 vec3Val;
		qm::vec4 vec4Val;
		qm::quaternion quatVal;
	};

	PSdata() {};
	PSdata(Type t, int32_t        val) { type = t, intVal   = val; };	
	PSdata(Type t, float          val) { type = t, floatVal = val; };	
	PSdata(Type t, qm::vec2       val) { type = t, vec2Val  = val; };	
	PSdata(Type t, qm::vec3       val) { type = t, vec3Val  = val; };	
	PSdata(Type t, qm::vec4       val) { type = t, vec4Val  = val; };
	PSdata(Type t, qm::quaternion val) { type = t, quatVal  = val; };
};

//a function signature
struct PSfunctionSignature
{
	std::string name;
	PSdata (*func)(const std::vector<PSdata>& params, const PSnode& node, void* userData);
};

//a constant value
struct PSconstant
{
	std::string name;
	PSdata val;
};

//--------------------------------------------------------------------------------------------------------------------------------//

/* Lexes and tokenizes a source file
 * @param path the path to the source file
 * @returns the vector of tokens extracted from the spurce file
 */
std::vector<PStoken> ps_lex_file(std::string path);

/* Parses a list of tokens into an abstract syntax tree
 * @param tokens the list of tokens
 * @returns the generated abstract syntax tree
 */
PSast* ps_parse_tokens(const std::vector<PStoken>& tokens);
/* Frees an abstract syntax tree
 * @param ast the abstract syntax tree to free
 */
void ps_free_ast(PSast* ast);
/* Serializes an abstract syntax tree to disk
 * @param path the path to the file to save to
 * @param ast the abstact syntax tree to save
 */
void ps_save_ast(std::string path, PSast* ast);
/* Serializes an abstract syntax tree to disk
 * @param file the file pointer to save to
 * @param ast the abstact syntax tree to save
 */
void ps_save_ast(std::ofstream& file, PSast* ast);
/* Loads an abstract syntax tree from disk
 * @param path the path to the file to load from
 * @returns the loaded abstract syntax tree
 */
PSast* ps_load_ast(std::string path);
/* Loads an abstract syntax tree from disk
 * @param file the file pointer to load from
 * @returns the loaded abstract syntax tree
 */
PSast* ps_load_ast(std::ifstream& file);

/* Sets a list of user defined functions to include in execution
 * @param functions the list of user defined functions to include 
 */
void ps_set_functions(const std::vector<PSfunctionSignature>& functions);
/* Sets a list of user defined constants to include in execution
 * @param constnats the list of user defined constants to include
 */
void ps_set_constants(const std::vector<PSconstant>& constants);
/* Sets the pointer to the user data that gets passed to each function call
 * @param userData the pointer to be passed to each function call
 */
void ps_set_function_user_data(void* userData);
/* Throws an invalid parameter error, call inside a user-defined function if the parameter list is invalid
 * @param node the node passed to the function
 */
void ps_throw_invalid_param_error(PSnode node);
/* Executes the code in an abstract syntax tree
 * @param ast the abstract syntax tree to execute
 */
void ps_execute(PSast* ast);

#endif