#include "propscript.hpp"
#include <iostream>
#include <vector>

int main()
{
	std::vector<PStoken> tokens = ps_lex_file("examples/example.ps");
	
	PSast* ast = ps_parse_tokens(tokens);
	if(!ast)
		return -1;

	ps_execute(ast);

	ps_save_ast("examples/example.psobj", ast);
	ps_free_ast(ast);

	return 0;
}