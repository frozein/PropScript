#include "propscript.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <unordered_map>

//--------------------------------------------------------------------------------------------------------------------------------//

enum class PSparseError
{
	EXPECTED_CLOSING_PAREN,
	UNEXPECTED_OPERATOR,
	EXPECTED_OPERATOR,
	INVALID_TOKEN,
	EXPECTED_OPENING_CURLY
};

//parses a single statement
static PSnodeHandle _ps_parse_statement(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);

//parses any token that is not an operator or control flow (statement in parens, variable, function, literal)
static PSnodeHandle _ps_parse_non_op(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);
//parses a statement that is in parenthesis
static PSnodeHandle _ps_parse_statement_in_parens(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);
//parses an identifier (variable, function, literal)
static PSnodeHandle _ps_parse_id(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);
//returns the node for a given operator
static PSnode _ps_get_op_node(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);

//adds a node to the ast
static inline PSnodeHandle _ps_add_node(PSast* ast, PSnode node);
//removes a newline from the token list if there are unclosed parenthesis
static inline void _ps_continue_statement(const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens);
//removes a newline from the token list
static inline void _ps_remove_newline(const std::vector<PStoken>& tokens, uint32_t& curTokenIdx);
//returns the operator precedence of a given operator
static inline int _ps_precedence(PSnode::OP::Type op);
//throws an exception if the given token is not an identifier
static inline void _ps_force_id(PStoken token);
//throws an exception and sets the global error variables
static void _ps_error(PSparseError error, PStoken errorToken);

//--------------------------------------------------------------------------------------------------------------------------------//

static PSparseError g_psError;
static PStoken g_psErrorToken;

const std::unordered_map<std::string, PSnode::OP::Type> PS_STRING_TO_OP_TYPE = {
	{PS_KEYWORD_IN         , PSnode::OP::Type::IN},
	{PS_OP_MULT            , PSnode::OP::Type::MULT},
	{PS_OP_DIV             , PSnode::OP::Type::DIV},
	{PS_OP_MOD             , PSnode::OP::Type::MOD},
	{PS_OP_ADD             , PSnode::OP::Type::ADD},
	{PS_OP_SUB             , PSnode::OP::Type::SUB},
	{PS_OP_EQUAL           , PSnode::OP::Type::EQUAL},
	{PS_OP_MULTEQUAL       , PSnode::OP::Type::MULTEQUAL},
	{PS_OP_DIVEQUAL        , PSnode::OP::Type::DIVEQUAL},
	{PS_OP_MODEQUAL        , PSnode::OP::Type::MODEQUAL},
	{PS_OP_ADDEQUAL        , PSnode::OP::Type::ADDEQUAL},
	{PS_OP_SUBEQUAL        , PSnode::OP::Type::SUBEQUAL},
	{PS_OP_LESSTHAN        , PSnode::OP::Type::LESSTHAN},
	{PS_OP_GREATERTHAN     , PSnode::OP::Type::GREATERTHAN},
	{PS_OP_LESSTHANEQUAL   , PSnode::OP::Type::LESSTHANEQUAL},
	{PS_OP_GREATERTHANEQUAL, PSnode::OP::Type::GREATERTHANEQUAL},
	{PS_OP_EQUALITY        , PSnode::OP::Type::EQUALITY},
	{PS_OP_NONEQUALITY     , PSnode::OP::Type::NONEQUALITY},
};

//--------------------------------------------------------------------------------------------------------------------------------//

PSast* ps_parse_tokens(const std::vector<PStoken>& tokens)
{
	PSast* result = new PSast;

	try
	{
		uint32_t curTokenIdx = 0;
		uint32_t numOpenParens = 0;

		while(curTokenIdx < tokens.size())
		{
			result->parentNodes.push_back(_ps_parse_statement(result, tokens, curTokenIdx, numOpenParens));
			_ps_remove_newline(tokens, curTokenIdx);
		}
	}
	catch(std::exception e)
	{
		std::string errMsg;
		switch(g_psError)
		{
		case PSparseError::EXPECTED_CLOSING_PAREN:
			errMsg = "EXPECTED CLOSING PARENTHESIS";
			break;
		case PSparseError::UNEXPECTED_OPERATOR:
			errMsg = "UNEXPECTED OPERATOR \"" + g_psErrorToken.str + "\"";
			break;
		case PSparseError::EXPECTED_OPERATOR:
			errMsg = "EXPECTED OPERATOR";
			break;
		case PSparseError::INVALID_TOKEN:
			errMsg = "INVALID TOKEN \"" + g_psErrorToken.str + "\"";
			break;
		case PSparseError::EXPECTED_OPENING_CURLY:
			errMsg = "EXPECTED OPENING CURLY BRACE";
			break;
		}

		std::cout << "PROPSCRIPT PARSE ERROR: " << errMsg << " ON LINE " << g_psErrorToken.lineNum << std::endl;
		delete result;
		return nullptr;
	}

	return result;
}

void ps_free_ast(PSast* ast)
{
	delete ast;
}

void ps_save_ast(std::string path, PSast* ast)
{
	std::ofstream file(path, std::ios_base::binary);
	if(!file.is_open())
	{
		std::cout << "ERROR OPENING FILE \"" << path << "\" FOR WRITING" << std::endl;
		return;
	}

	ps_save_ast(file, ast);
	
	file.close();
}

void ps_save_ast(std::ofstream& file, PSast* ast)
{
	size_t parentNodeSize = ast->parentNodes.size();
	size_t nodePoolSize = ast->nodePool.size();

	file.write((const char*)&parentNodeSize, sizeof(size_t));
	file.write((const char*)ast->parentNodes.data(), sizeof(PSnodeHandle) * parentNodeSize);

	file.write((const char*)&nodePoolSize, sizeof(size_t));
	for(int i = 0; i < nodePoolSize; i++)
	{
		PSnode& node = ast->nodePool[i];
		file.write((const char*)&node.type, sizeof(PSnode::Type));
		file.write((const char*)&node.lineNum, sizeof(uint32_t));

		switch(node.type)
		{
		case PSnode::Type::OP:
		{
			file.write((const char*)&node.op, sizeof(PSnode::OP));
			file.write((const char*)&node.op.left, sizeof(PSnodeHandle));
			file.write((const char*)&node.op.right, sizeof(PSnodeHandle));
			break;
		}
		case PSnode::Type::KEYWORD:
		{
			file.write((const char*)&node.keyword.type, sizeof(PSnode::Keyword::Type));

			size_t codeSize = node.keyword.code.size();
			size_t elseCodeSize = node.keyword.elseCode.size();
			size_t paramNameSize = node.keyword.paramNames.size();

			file.write((const char*)&codeSize, sizeof(size_t));
			file.write((const char*)node.keyword.code.data(), sizeof(PSnodeHandle) * codeSize);

			file.write((const char*)&node.keyword.condition, sizeof(PSnodeHandle));

			file.write((const char*)&node.keyword.hasElse, sizeof(bool));
			file.write((const char*)&elseCodeSize, sizeof(size_t));
			file.write((const char*)node.keyword.elseCode.data(), sizeof(PSnodeHandle) * elseCodeSize);

			size_t nameLen = node.keyword.name.length();
			file.write((const char*)&nameLen, sizeof(size_t));
			file.write((const char*)node.keyword.name.data(), sizeof(char) * nameLen);
			file.write((const char*)&paramNameSize, sizeof(size_t));
			for(int j = 0; j < paramNameSize; j++)
			{
				size_t paramNameLen = node.keyword.paramNames[j].length();
				file.write((const char*)&paramNameLen, sizeof(size_t));
				file.write((const char*)node.keyword.paramNames[j].data(), sizeof(char) * paramNameLen);
			}

			file.write((const char*)&node.keyword.returnVal, sizeof(PSnodeHandle));

			break;
		}
		case PSnode::ID:
		{
			file.write((const char*)&node.id.type, sizeof(PSnode::ID::Type));

			size_t nameLen = node.id.name.length();
			file.write((const char*)&nameLen, sizeof(size_t));
			file.write((const char*)node.id.name.data(), sizeof(char) * nameLen);

			size_t paramSize = node.id.params.size();
			file.write((const char*)&paramSize, sizeof(size_t));
			file.write((const char*)node.id.params.data(), sizeof(PSnodeHandle) * paramSize);

			break;
		}
		case PSnode::NUMBER:
		{
			file.write((const char*)&node.literal.type, sizeof(PSnode::Literal::Type));
			file.write((const char*)&node.literal.intNum, sizeof(int32_t));
			file.write((const char*)&node.literal.floatNum, sizeof(float));

			break;
		}
		}
	}
}

PSast* ps_load_ast(std::string path)
{
	std::ifstream file(path, std::ios_base::binary);
	if(!file.is_open())
	{
		std::cout << "ERROR OPENING FILE \"" << path << "\" FOR READING" << std::endl;
		return nullptr;
	}

	PSast* result = ps_load_ast(file);
	
	file.close();
	return result;
}

PSast* ps_load_ast(std::ifstream& file)
{
	PSast* result = new PSast;

	size_t parentNodeSize;
	size_t nodePoolSize;

	file.read((char*)&parentNodeSize, sizeof(size_t));
	result->parentNodes.resize(parentNodeSize);
	file.read((char*)result->parentNodes.data(), sizeof(PSnodeHandle) * parentNodeSize);

	file.read((char*)&nodePoolSize, sizeof(size_t));
	for(int i = 0; i < nodePoolSize; i++)
	{
		PSnode node;
		file.read((char*)&node.type, sizeof(PSnode::Type));
		file.read((char*)&node.lineNum, sizeof(uint32_t));

		switch(node.type)
		{
		case PSnode::Type::OP:
		{
			file.read((char*)&node.op, sizeof(PSnode::OP));
			file.read((char*)&node.op.left, sizeof(PSnodeHandle));
			file.read((char*)&node.op.right, sizeof(PSnodeHandle));
			break;
		}
		case PSnode::Type::KEYWORD:
		{
			file.read((char*)&node.keyword.type, sizeof(PSnode::Keyword::Type));

			size_t codeSize;
			size_t elseCodeSize;
			size_t paramNameSize;

			file.read((char*)&codeSize, sizeof(size_t));
			node.keyword.code.resize(codeSize);
			file.read((char*)node.keyword.code.data(), sizeof(PSnodeHandle) * codeSize);

			file.read((char*)&node.keyword.condition, sizeof(PSnodeHandle));

			file.read((char*)&node.keyword.hasElse, sizeof(bool));
			file.read((char*)&elseCodeSize, sizeof(size_t));
			node.keyword.elseCode.resize(elseCodeSize);
			file.read((char*)node.keyword.elseCode.data(), sizeof(PSnodeHandle) * elseCodeSize);

			size_t nameLen;
			file.read((char*)&nameLen, sizeof(size_t));
			node.keyword.name.resize(nameLen);
			file.read(node.keyword.name.data(), sizeof(char) * nameLen);
			file.read((char*)&paramNameSize, sizeof(size_t));
			node.keyword.paramNames.resize(paramNameSize);
			for(int j = 0; j < paramNameSize; j++)
			{
				size_t paramNameLen;
				file.read((char*)&paramNameLen, sizeof(size_t));
				node.keyword.paramNames[j].resize(paramNameLen);
				file.read(node.keyword.paramNames[j].data(), sizeof(char) * paramNameLen);
			}

			file.read((char*)&node.keyword.returnVal, sizeof(PSnodeHandle));

			break;
		}
		case PSnode::ID:
		{
			file.read((char*)&node.id.type, sizeof(PSnode::ID::Type));

			size_t nameLen;
			file.read((char*)&nameLen, sizeof(size_t));
			node.id.name.resize(nameLen);
			file.read(node.id.name.data(), sizeof(char) * nameLen);

			size_t paramSize;
			file.read((char*)&paramSize, sizeof(size_t));
			node.id.params.resize(paramSize);
			file.read((char*)node.id.params.data(), sizeof(PSnodeHandle) * paramSize);

			break;
		}
		case PSnode::NUMBER:
		{
			file.read((char*)&node.literal.type, sizeof(PSnode::Literal::Type));
			file.read((char*)&node.literal.intNum, sizeof(int32_t));
			file.read((char*)&node.literal.floatNum, sizeof(float));

			break;
		}
		}

		result->nodePool.push_back(node);
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------//

static PSnodeHandle _ps_parse_statement(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	//CHECK IF CONTROL FLOW STATEMENT:
	if(tokens[curTokenIdx].str == PS_KEYWORD_IF || tokens[curTokenIdx].str == PS_KEYWORD_FOR)
	{
		bool isFor = tokens[curTokenIdx].str == PS_KEYWORD_FOR;

		if(numOpenParens > 0)
			_ps_error(PSparseError::INVALID_TOKEN, tokens[curTokenIdx]);

		PSnode controlNode;
		controlNode.type = PSnode::KEYWORD;
		controlNode.lineNum = tokens[curTokenIdx].lineNum;
		controlNode.keyword.type = isFor ? PSnode::Keyword::FOR : PSnode::Keyword::IF;

		//GET CONDITION:
		controlNode.keyword.condition = _ps_parse_statement(ast, tokens, ++curTokenIdx, numOpenParens);
		_ps_remove_newline(tokens, curTokenIdx);

		//GET CODE:
		if(tokens[curTokenIdx].str == PS_SEPERATOR_CURLY_OPEN) //multi-line
		{
			curTokenIdx++;
			_ps_remove_newline(tokens, curTokenIdx);

			while(tokens[curTokenIdx].str != PS_SEPERATOR_CURLY_CLOSE)
			{
				controlNode.keyword.code.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));
				_ps_remove_newline(tokens, curTokenIdx);
			}
		
			curTokenIdx++;
		}
		else //single-line
			controlNode.keyword.code.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));

		//IF FOR LOOP, NO ELSE STATEMENT POSSIBLE SO JUST RETURN
		if(isFor)
			return _ps_add_node(ast, controlNode);

		//CHECK FOR ELSE:
		_ps_remove_newline(tokens, curTokenIdx);

		if(tokens[curTokenIdx].str == PS_KEYWORD_ELSE)
		{
			controlNode.keyword.hasElse = true;

			curTokenIdx++;
			_ps_remove_newline(tokens, curTokenIdx);
			if(tokens[curTokenIdx].str == PS_SEPERATOR_CURLY_OPEN) //multi-line
			{
				curTokenIdx++;
				_ps_remove_newline(tokens, curTokenIdx);

				while(tokens[curTokenIdx].str != PS_SEPERATOR_CURLY_CLOSE)
				{
					controlNode.keyword.elseCode.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));
					_ps_remove_newline(tokens, curTokenIdx);
				}
			
				curTokenIdx++;
			}
			else //single-line / else-if
				controlNode.keyword.elseCode.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));
		}
		else
			controlNode.keyword.hasElse = false;

		return _ps_add_node(ast, controlNode);
	}

	//CHECK IF FUNCTION DEFINITION:
	if(tokens[curTokenIdx].str == PS_KEYWORD_FUNC)
	{
		PSnode funcNode;
		funcNode.type = PSnode::KEYWORD;
		funcNode.lineNum = tokens[curTokenIdx].lineNum;
		funcNode.keyword.type = PSnode::Keyword::FUNC;

		curTokenIdx++;
		_ps_remove_newline(tokens, curTokenIdx);

		_ps_force_id(tokens[curTokenIdx]);

		funcNode.keyword.name = tokens[curTokenIdx].str;
		curTokenIdx++;
		_ps_remove_newline(tokens, curTokenIdx);

		if(tokens[curTokenIdx].str == PS_SEPERATOR_PAREN_OPEN) //has parameters
		{
			curTokenIdx++;
			numOpenParens++;
			_ps_continue_statement(tokens, curTokenIdx, numOpenParens);

			//argument names:
			while(true)
			{
				_ps_force_id(tokens[curTokenIdx]);
				funcNode.keyword.paramNames.push_back(tokens[curTokenIdx].str);
				curTokenIdx++;

				if(tokens[curTokenIdx].str == PS_SEPERATOR_PAREN_CLOSE)
					break;
				else if(tokens[curTokenIdx].str != PS_SEPERATOR_COMMA)
					_ps_error(PSparseError::EXPECTED_OPERATOR, tokens[curTokenIdx]);

				curTokenIdx++;
				_ps_continue_statement(tokens, curTokenIdx, numOpenParens);
			}

			curTokenIdx++;
			numOpenParens--;
		}

		_ps_remove_newline(tokens, curTokenIdx);

		//GET CODE:
		if(tokens[curTokenIdx].str != PS_SEPERATOR_CURLY_OPEN) //ensure open curly brace found
			_ps_error(PSparseError::EXPECTED_OPENING_CURLY, tokens[curTokenIdx]);

		curTokenIdx++;
		_ps_remove_newline(tokens, curTokenIdx);

		while(tokens[curTokenIdx].str != PS_SEPERATOR_CURLY_CLOSE)
		{
			funcNode.keyword.code.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));
			_ps_remove_newline(tokens, curTokenIdx);
		}
		
		curTokenIdx++;

		return _ps_add_node(ast, funcNode);
	}

	//CHECK IF RETURN STATEMENT:
	if(tokens[curTokenIdx].str == PS_KEYWORD_RETURN)
	{
		PSnode returnNode;
		returnNode.type = PSnode::KEYWORD;
		returnNode.lineNum = tokens[curTokenIdx].lineNum;
		returnNode.keyword.type = PSnode::Keyword::RETURN;

		curTokenIdx++;
		if(tokens[curTokenIdx].type != PStoken::NEWLINE &&
		   std::find(PS_CLOSED_SEPERATORS.begin(), PS_CLOSED_SEPERATORS.end(), tokens[curTokenIdx].str) == PS_CLOSED_SEPERATORS.end()) //get return value if not a void return
			returnNode.keyword.returnVal = _ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens);
		else
			returnNode.keyword.returnVal = UINT32_MAX;

		return _ps_add_node(ast, returnNode);
	}

	//CHECK IF BREAK/CONTINUE STATEMENT:
	if(tokens[curTokenIdx].str == PS_KEYWORD_BREAK || tokens[curTokenIdx].str == PS_KEYWORD_CONTINUE)
	{
		PSnode breakNode;
		breakNode.type = PSnode::KEYWORD;
		breakNode.lineNum = tokens[curTokenIdx].lineNum;
		breakNode.keyword.type = tokens[curTokenIdx].str == PS_KEYWORD_BREAK ? PSnode::Keyword::BREAK : PSnode::Keyword::CONTINUE;

		curTokenIdx++;
		if(tokens[curTokenIdx].type != PStoken::NEWLINE &&
		   std::find(PS_CLOSED_SEPERATORS.begin(), PS_CLOSED_SEPERATORS.end(), tokens[curTokenIdx].str) == PS_CLOSED_SEPERATORS.end()) //get return value if not a void return
			_ps_error(PSparseError::INVALID_TOKEN, tokens[curTokenIdx]);

		return _ps_add_node(ast, breakNode);
	}

	//MUST BE REGULAR OPERATION:
	PSnodeHandle left;
	PSnodeHandle right;

	//GET LEFT TOKEN:
	left = _ps_parse_non_op(ast, tokens, curTokenIdx, numOpenParens);

	//CHECK IF LINE ENDED:
	if(tokens[curTokenIdx].type == PStoken::NEWLINE || tokens[curTokenIdx].str == PS_SEPERATOR_CURLY_OPEN ||
	   std::find(PS_CLOSED_SEPERATORS.begin(), PS_CLOSED_SEPERATORS.end(), tokens[curTokenIdx].str) != PS_CLOSED_SEPERATORS.end())
		return left;

	//GET OP TOKEN:
	PSnode opNode = _ps_get_op_node(ast, tokens, curTokenIdx, numOpenParens);

	//GET RIGHT TOKEN:
	right = _ps_parse_non_op(ast, tokens, curTokenIdx, numOpenParens);

	//CONSTRUCT OP NODE:	
	opNode.op.inParens = false;
	opNode.op.left  = left;
	opNode.op.right = right;

	//ITERATE TO GET REST OF NODES:
	while(curTokenIdx < tokens.size() && tokens[curTokenIdx].type != PStoken::NEWLINE && tokens[curTokenIdx].str != PS_SEPERATOR_CURLY_OPEN &&
	      std::find(PS_CLOSED_SEPERATORS.begin(), PS_CLOSED_SEPERATORS.end(), tokens[curTokenIdx].str) == PS_CLOSED_SEPERATORS.end())
	{
		PSnode newOp = _ps_get_op_node(ast, tokens, curTokenIdx, numOpenParens);

		right = _ps_parse_non_op(ast, tokens, curTokenIdx, numOpenParens);

		//ADD TO EXISTING NODES WITH CORRECT ORDER OF OPERATIONS:
		if(_ps_precedence(newOp.op.type) >= _ps_precedence(opNode.op.type))
		{
			newOp.op.left = _ps_add_node(ast, opNode);
			newOp.op.right = right;
			opNode = newOp;
		}
		else
		{
			//find rightmost op with lesser order:
			PSnode* rightMost = &opNode;
			PSnode rightModeRight = ast->nodePool[rightMost->op.right];
			while(rightModeRight.type == PSnode::OP && 
			     _ps_precedence(newOp.op.type) < _ps_precedence(rightModeRight.op.type) && 
			     !rightModeRight.op.inParens)
			{
				rightMost = &ast->nodePool[rightMost->op.right];
				rightModeRight = ast->nodePool[rightMost->op.right];
			}

			newOp.op.left = rightMost->op.right;
			newOp.op.right = right;
			rightMost->op.right = _ps_add_node(ast, newOp);
		}
	}

	return _ps_add_node(ast, opNode);
}

//--------------------------------------------------------------------------------------------------------------------------------//

static PSnodeHandle _ps_parse_non_op(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	PSnodeHandle node;

	if(tokens[curTokenIdx].str == PS_SEPERATOR_PAREN_OPEN)
		node = _ps_parse_statement_in_parens(ast, tokens, curTokenIdx, numOpenParens);
	else
		node = _ps_parse_id(ast, tokens, curTokenIdx, numOpenParens);

	_ps_continue_statement(tokens, curTokenIdx, numOpenParens);
	return node;
}

static PSnodeHandle _ps_parse_statement_in_parens(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	PSnodeHandle node = _ps_parse_statement(ast, tokens, ++curTokenIdx, ++numOpenParens);
	if(tokens[curTokenIdx].str != PS_SEPERATOR_PAREN_CLOSE)
		_ps_error(PSparseError::EXPECTED_CLOSING_PAREN, tokens[curTokenIdx]);

	ast->nodePool[node].op.inParens = true;

	numOpenParens--;
	curTokenIdx++;
	return node;
}

static PSnodeHandle _ps_parse_id(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	bool negative = false;
	if(tokens[curTokenIdx].str == PS_OP_SUB)
	{
		negative = true;
		curTokenIdx++;
	}

	_ps_force_id(tokens[curTokenIdx]);

	//FUNCTION:
	if(tokens[curTokenIdx + 1].str == PS_SEPERATOR_PAREN_OPEN)
	{
		PSnode funcNode;
		funcNode.type = PSnode::ID;
		funcNode.lineNum = tokens[curTokenIdx].lineNum;
		funcNode.id.type = PSnode::ID::FUNC;
		funcNode.id.name = tokens[curTokenIdx].str;

		curTokenIdx += 2;
		numOpenParens++;
		_ps_continue_statement(tokens, curTokenIdx, numOpenParens);

		//0 argument function:
		if(tokens[curTokenIdx].str == PS_SEPERATOR_PAREN_CLOSE)
		{
			curTokenIdx++;
			numOpenParens--;
			return _ps_add_node(ast, funcNode);
		}

		//arguments:
		while(true)
		{
			funcNode.id.params.push_back(_ps_parse_statement(ast, tokens, curTokenIdx, numOpenParens));

			if(tokens[curTokenIdx].str == PS_SEPERATOR_PAREN_CLOSE)
				break;
			else if(tokens[curTokenIdx].str != PS_SEPERATOR_COMMA)
				_ps_error(PSparseError::EXPECTED_OPERATOR, tokens[curTokenIdx]);

			curTokenIdx++;
			_ps_continue_statement(tokens, curTokenIdx, numOpenParens);
		}

		curTokenIdx++;
		numOpenParens--;

		if(negative)
		{
			PSnode minusNode;
			minusNode.type = PSnode::OP;
			minusNode.lineNum = tokens[curTokenIdx].lineNum;
			minusNode.op.type = PSnode::OP::SUB;
			
			PSnode negOne;
			negOne.type = PSnode::NUMBER;
			negOne.lineNum = tokens[curTokenIdx].lineNum;
			negOne.literal.type = PSnode::Literal::INT;
			negOne.literal.intNum = -1;

			minusNode.op.left = _ps_add_node(ast, negOne);
			minusNode.op.right = _ps_add_node(ast, funcNode);
			return _ps_add_node(ast, minusNode);
		}
		else
			return _ps_add_node(ast, funcNode);
	}
	
	PStoken token = tokens[curTokenIdx++];

	//NUMBER:
	if(std::isdigit(token.str[0]) || token.str[0] == '.')
	{
		bool isFloat = false;
		for(int i = 0; i < token.str.length(); i++)
		{
			if(token.str[i] == '.')
			{
				if(isFloat)
					_ps_error(PSparseError::INVALID_TOKEN, tokens[curTokenIdx]);
				else
					isFloat = true;
			}
			else if(!std::isdigit(token.str[i]))
				_ps_error(PSparseError::INVALID_TOKEN, tokens[curTokenIdx]);
		}

		PSnode numNode;
		numNode.type = PSnode::NUMBER;
		numNode.lineNum = tokens[curTokenIdx - 1].lineNum;

		if(isFloat)
		{
			numNode.literal.type = PSnode::Literal::FLOAT;
			numNode.literal.floatNum = std::stof(token.str);
			if(negative)
				numNode.literal.floatNum *= -1.0f;
		}
		else
		{
			numNode.literal.type = PSnode::Literal::INT;
			numNode.literal.intNum = std::stoi(token.str);
			if(negative)
				numNode.literal.intNum *= -1;
		}

		return _ps_add_node(ast, numNode);
	}
	
	//VARIABLE:
	PSnode varNode;
	varNode.type = PSnode::ID;
	varNode.lineNum = tokens[curTokenIdx].lineNum;
	varNode.id.type = PSnode::ID::VAR;
	varNode.id.name = token.str;

	//index into variable:
	if(tokens[curTokenIdx].str == PS_SEPERATOR_SQUARE_OPEN)
	{
		numOpenParens++;
		_ps_continue_statement(tokens, curTokenIdx, numOpenParens);

		varNode.id.params.push_back(_ps_parse_statement(ast, tokens, ++curTokenIdx, numOpenParens));
		
		_ps_continue_statement(tokens, curTokenIdx, numOpenParens);
		if(tokens[curTokenIdx].str != PS_SEPERATOR_SQUARE_CLOSE)
			_ps_error(PSparseError::EXPECTED_CLOSING_PAREN, tokens[curTokenIdx]);
		
		numOpenParens--;
		curTokenIdx++;
	}

	if(negative)
	{
		PSnode minusNode;
		minusNode.type = PSnode::OP;
		minusNode.lineNum = tokens[curTokenIdx].lineNum;
		minusNode.op.type = PSnode::OP::SUB;
			
		PSnode negOne;
		negOne.type = PSnode::NUMBER;
		negOne.lineNum = tokens[curTokenIdx].lineNum;
		negOne.literal.type = PSnode::Literal::INT;
		negOne.literal.intNum = -1;

		minusNode.op.left = _ps_add_node(ast, negOne);
		minusNode.op.right = _ps_add_node(ast, varNode);
		return _ps_add_node(ast, minusNode);
	}
	else
		return _ps_add_node(ast, varNode);
}

static PSnode _ps_get_op_node(PSast* ast, const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	//ENSURE ACTUALLY AN OP:
	if(tokens[curTokenIdx].type != PStoken::OP)
		_ps_error(PSparseError::EXPECTED_OPERATOR, tokens[curTokenIdx]);

	PSnode opNode;
	opNode.type = PSnode::OP;
	opNode.lineNum = tokens[curTokenIdx].lineNum;
	if(PS_STRING_TO_OP_TYPE.count(tokens[curTokenIdx].str) == 0)
		_ps_error(PSparseError::INVALID_TOKEN, tokens[curTokenIdx]);
	else
		opNode.op.type = PS_STRING_TO_OP_TYPE.at(tokens[curTokenIdx].str);

	curTokenIdx++;
	_ps_continue_statement(tokens, curTokenIdx, numOpenParens);
	return opNode;
}

//--------------------------------------------------------------------------------------------------------------------------------//

static inline PSnodeHandle _ps_add_node(PSast* ast, PSnode node)
{
	ast->nodePool.push_back(node);
	return ast->nodePool.size() - 1;
}

static inline void _ps_continue_statement(const std::vector<PStoken>& tokens, uint32_t& curTokenIdx, uint32_t& numOpenParens)
{
	if(tokens[curTokenIdx].type == PStoken::NEWLINE && numOpenParens != 0)
	{
		curTokenIdx++;
		if(curTokenIdx >= tokens.size())
			_ps_error(PSparseError::EXPECTED_CLOSING_PAREN, tokens[curTokenIdx - 1]);
	}
}

static inline void _ps_remove_newline(const std::vector<PStoken>& tokens, uint32_t& curTokenIdx)
{
	if(tokens[curTokenIdx].type == PStoken::NEWLINE)
		curTokenIdx++;
}

static inline int _ps_precedence(PSnode::OP::Type op)
{
	return (int)op / 10;
}

static inline void _ps_force_id(PStoken token)
{
	if(token.type != PStoken::ID)
		_ps_error(PSparseError::UNEXPECTED_OPERATOR, token);

	if(std::find(PS_KEYWORDS.begin(), PS_KEYWORDS.end(), token.str) != PS_KEYWORDS.end())
		_ps_error(PSparseError::INVALID_TOKEN, token);
}

static void _ps_error(PSparseError error, PStoken errorToken)
{
	g_psError = error;
	g_psErrorToken = errorToken;
	throw std::exception();
}