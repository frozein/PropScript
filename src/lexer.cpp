#include "propscript.hpp"

#include <fstream>
#include <iostream>

//--------------------------------------------------------------------------------------------------------------------------------//

//returns whether or not the given character exists in an operator at the given index
bool is_op_char(char ch, size_t idx);
//returns whether a given string is an operator
bool is_op_str(std::string op);
//adds an id token to the token list if idLen is not 0
void try_add_id_token(std::string idString, size_t& idLen, uint32_t lineNum, std::vector<PStoken>& tokens);

//--------------------------------------------------------------------------------------------------------------------------------//

size_t g_maxOpLen;

//--------------------------------------------------------------------------------------------------------------------------------//

std::vector<PStoken> ps_lex_file(std::string path)
{
	//GET MAX OPERATOR LEN:
	for(int i = 0; i < PS_LEXER_OPERATORS.size(); i++)
		if(PS_LEXER_OPERATORS[i].length() > g_maxOpLen)
			g_maxOpLen = PS_LEXER_OPERATORS[i].length();

	//LEX:
	std::vector<PStoken> tokens;
	std::ifstream file(path);
	if(!file.is_open())
	{
		std::cout << "PROPSCRIPT LEX ERROR: FAILED TO OPEN \"" << path << "\" FOR READING" << std::endl;
		return {};
	}

	bool inComment = false;

	uint32_t curLine = 1;

	std::string idString(64, ' ');
	size_t idLen = 0;

	while(!file.eof())
	{
		char curCh = file.get();
		if(file.eof())
			break;

		if(curCh == '\n')
		{
			try_add_id_token(idString, idLen, curLine, tokens);

			//remove duplicate newlines:
			if(tokens.size() > 1 && tokens[tokens.size() - 1].type != PStoken::NEWLINE)
			{
				PStoken newlineToken;
				newlineToken.type = PStoken::NEWLINE;
				newlineToken.lineNum = curLine;
				tokens.push_back(newlineToken);
			}

			inComment = false;
			curLine++;
		}
		else if(inComment)
		{
			continue;
		}
		else if(std::isspace(curCh))
		{
			try_add_id_token(idString, idLen, curLine, tokens);
		} 
		else if(is_op_char(curCh, 0))
		{
			try_add_id_token(idString, idLen, curLine, tokens);

			std::string opString(g_maxOpLen, curCh);
			size_t opLen = 1;

			while(is_op_char(file.peek(), opLen))
			{
				curCh = file.get();
				opString[opLen++] = curCh;
			}

			while(!is_op_str(opString.substr(0, opLen)))
			{
				opLen--;
				file.unget();
			}

			opString = opString.substr(0, opLen);

			if(opString == PS_COMMENT)
				inComment = true;
			else if(opLen > 0)
			{
				PStoken opToken;
				opToken.type = PStoken::OP;
				opToken.str = opString;
				opToken.lineNum = curLine;
				tokens.push_back(opToken);
			}
		}
		else
		{
			idString[idLen++] = curCh;
			if(idLen >= idString.length())
				idString.resize(idString.length() * 2);
		}
	}

	try_add_id_token(idString, idLen, curLine, tokens);

	//make sure tokens end with newline:
	if(tokens[tokens.size() - 1].type != PStoken::NEWLINE)
	{
		PStoken newlineToken;
		newlineToken.type = PStoken::NEWLINE;
		newlineToken.lineNum = curLine;
		tokens.push_back(newlineToken);
	}

	file.close();
	return tokens;
}

//--------------------------------------------------------------------------------------------------------------------------------//

bool is_op_char(char ch, size_t idx)
{
	for(int i = 0; i < PS_LEXER_OPERATORS.size(); i++)
		if(PS_LEXER_OPERATORS[i][idx] == ch)
			return true;

	return false;
}

bool is_op_str(std::string str)
{
	for(int i = 0; i < PS_LEXER_OPERATORS.size(); i++)
		if(PS_LEXER_OPERATORS[i] == str)
			return true;

	return false;
}

void try_add_id_token(std::string idString, size_t& idLen, uint32_t lineNum, std::vector<PStoken>& tokens)
{
	if(idLen <= 0)
		return;

	PStoken idToken;
	idToken.type = PStoken::ID;

	idString = idString.substr(0, idLen);
	if(idString == PS_KEYWORD_AND || idString == PS_KEYWORD_OR || idString == PS_KEYWORD_IN)
		idToken.type = PStoken::OP;

	idToken.str = idString;
	idToken.lineNum = lineNum;
	tokens.push_back(idToken);
	idLen = 0;
}