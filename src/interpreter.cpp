#include "propscript.hpp"

#include <unordered_map>
#include <iostream>

#define _USE_MATH_DEFINES
#include <math.h>

//--------------------------------------------------------------------------------------------------------------------------------//

enum class PSruntimeError
{
	INVALID_ASSIGNMENT,
	INVALID_OP,
	UNSUPPORTED_NODE_TYPE,
	UNDEFINED_VARIABLE,
	UNDEFINED_FUNCTION,
	INVALID_PARAMS,
	INVALID_INDEX,
	INVALID_CONDITION,
	INVALID_BREAK_CONTINUE,
	FUNCTION_REDEFINITION,
	ARGUMENT_NAME_REDEFINITION
};

//executes a set of statements with their own scope
static void _ps_execute_statements(PSast* ast, const std::vector<PSnodeHandle>& nodes); 
//evaluates a single statement
static PSdata _ps_evaluate_statement(PSast* ast, const PSnode& node, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars); 
//executes a programmer-defined function
static inline PSdata _ps_execute_function(PSast* ast, const PSnode& node, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars);

//gets the scalar value from a PSdata struct, or throws an error if the data type is not a scalar
static inline float _ps_get_scalar(PSdata data, PSruntimeError potentialError, const PSnode& node);
//throws an exception and sets the global error flags
static inline void _ps_error(PSruntimeError error, PSnode errorNode);

//OPERATOR FUNCTIONS:
static inline PSdata _ps_mult(const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_div (const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_mod (const PSdata& left, const PSdata& right, const PSnode& node);

static inline PSdata _ps_add(const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_sub(const PSdata& left, const PSdata& right, const PSnode& node);

static inline PSdata _ps_equal(PSast* ast, const PSnode& var, const PSdata& val, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars);

static inline PSdata _ps_lessthan        (const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_greaterthan     (const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_lessthanequal   (const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_greaterthanequal(const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_equality        (const PSdata& left, const PSdata& right, const PSnode& node);

static inline PSdata _ps_and(const PSdata& left, const PSdata& right, const PSnode& node);
static inline PSdata _ps_or (const PSdata& left, const PSdata& right, const PSnode& node);

//DEFAULT LIBRARY FUNCTIONS (more will be added as i need them):
static PSdata _ps_range(const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_print(const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_rand (const std::vector<PSdata>& params, const PSnode& node, void* userData);

static PSdata _ps_int       (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_vec2      (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_vec3      (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_vec4      (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_quaternion(const std::vector<PSdata>& params, const PSnode& node, void* userData);

static PSdata _ps_sqrt(const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_pow (const std::vector<PSdata>& params, const PSnode& node, void* userData);

static PSdata _ps_sin (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_cos (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_tan (const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_asin(const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_acos(const std::vector<PSdata>& params, const PSnode& node, void* userData);
static PSdata _ps_atan(const std::vector<PSdata>& params, const PSnode& node, void* userData);

//--------------------------------------------------------------------------------------------------------------------------------//

static PSruntimeError g_psError;
static PSnode g_psErrorNode;

const std::vector<PSfunctionSignature> PS_DEFAULT_LIB_FUNCTIONS = {
	{"range"     , _ps_range},
	{"print"     , _ps_print},
	{"rand"      , _ps_rand},
	{"int"       , _ps_int},
	{"vec2"      , _ps_vec2},
	{"vec3"      , _ps_vec3},
	{"vec4"      , _ps_vec4},
	{"quaternion", _ps_quaternion},
	{"sqrt"      , _ps_sqrt},
	{"pow"       , _ps_pow},
	{"sin"       , _ps_sin},
	{"cos"       , _ps_cos},
	{"tan"       , _ps_tan},
	{"asin"      , _ps_asin},
	{"acos"      , _ps_acos},
	{"atan"      , _ps_atan},
};

const std::vector<PSconstant> PS_DEFAULT_CONSTANTS = {
	{"M_PI" , PSdata(PSdata::FLOAT,        (float)M_PI)},
	{"M_TAU", PSdata(PSdata::FLOAT, 2.0f * (float)M_PI)},
	{"M_E"  , PSdata(PSdata::FLOAT,        (float)M_E) }
};

static std::unordered_map<std::string, PSfunctionSignature> g_psLibFunctions;
static std::unordered_map<std::string, PSdata> g_psConstants;
static void* g_psLibFunctionUserData = nullptr;

static std::unordered_map<std::string, PSnode> g_psFunctions;
static std::unordered_map<std::string, PSdata> g_psVariables;

static bool g_psInLoop = false;

static bool g_psReturnFlag   = false;
static bool g_psBreakFlag    = false;
static bool g_psContinueFlag = false;
static PSdata g_psReturnVal  = {};

//--------------------------------------------------------------------------------------------------------------------------------//

void ps_set_functions(const std::vector<PSfunctionSignature>& functions)
{
	g_psLibFunctions.clear();

	for(int i = 0; i < PS_DEFAULT_LIB_FUNCTIONS.size(); i++)
		g_psLibFunctions[PS_DEFAULT_LIB_FUNCTIONS[i].name] = PS_DEFAULT_LIB_FUNCTIONS[i];

	for(int i = 0; i < functions.size(); i++)
		g_psLibFunctions[functions[i].name] = functions[i];
}

void ps_set_constants(const std::vector<PSconstant>& constants)
{
	g_psConstants.clear();

	for(int i = 0; i < PS_DEFAULT_CONSTANTS.size(); i++)
		g_psConstants[PS_DEFAULT_CONSTANTS[i].name] = PS_DEFAULT_CONSTANTS[i].val;

	for(int i = 0; i < constants.size(); i++)
		g_psConstants[constants[i].name] = constants[i].val;
}

void ps_set_function_user_data(void* userData)
{
	g_psLibFunctionUserData = userData;
}

void ps_throw_invalid_param_error(PSnode node)
{
	_ps_error(PSruntimeError::INVALID_PARAMS, node);
}

void ps_execute(PSast* ast)
{
	if(g_psLibFunctions.size() == 0)
		ps_set_functions({});

	if(g_psConstants.size() == 0)
		ps_set_constants({});

	try
	{
		_ps_execute_statements(ast, ast->parentNodes);

		if(g_psReturnFlag)
			g_psReturnFlag = false;
	}
	catch(std::exception e)
	{
		std::string errMsg;
		PSnode testNode = g_psErrorNode;
		switch(g_psError)
		{
		case PSruntimeError::INVALID_ASSIGNMENT:
			errMsg = "INVALID ASSIGNMENT";
			break;
		case PSruntimeError::INVALID_OP:
			errMsg = "INVALID OPERATION";
			break;
		case PSruntimeError::UNSUPPORTED_NODE_TYPE:
			errMsg = "UNSUPPORTED NODE TYPE (i must've forgot to implement something in the interpreter)";
			break;
		case PSruntimeError::UNDEFINED_VARIABLE:
			errMsg = "UNDEFINED VARIABLE";
			break;
		case PSruntimeError::UNDEFINED_FUNCTION:
			errMsg = "UNDEFINED FUNCTION";
			break;
		case PSruntimeError::INVALID_PARAMS:
			errMsg = "INVALID PARAMETERS";
			break;
		case PSruntimeError::INVALID_INDEX:
			errMsg = "INVALID INDEX";
			break;
		case PSruntimeError::INVALID_CONDITION:
			errMsg = "INVALID CONDITION";
			break;
		case PSruntimeError::INVALID_BREAK_CONTINUE:
			errMsg = "INVALID BREAK/CONTINUE";
			break;
		case PSruntimeError::FUNCTION_REDEFINITION:
			errMsg = "FUNCTION REDEFINITION";
			break;
		case PSruntimeError::ARGUMENT_NAME_REDEFINITION:
			errMsg = "ARGUMENT NAME REDEFINITION";
			break;
		}

		std::cout << "PROPSCRIPT RUNTIME ERROR: " << errMsg << " ON LINE " << g_psErrorNode.lineNum << std::endl;
	
		//there might be uncleared vars/funcs if we run into an error:
		g_psFunctions.clear();
		g_psVariables.clear();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------//

static void _ps_execute_statements(PSast* ast, const std::vector<PSnodeHandle>& nodes)
{
	std::vector<std::string> addedFuncs;				
	std::vector<std::string> addedVars;

	for(int i = 0; i < nodes.size(); i++)
	{
		_ps_evaluate_statement(ast, ast->nodePool[nodes[i]], addedFuncs, addedVars);

		if(g_psReturnFlag || g_psBreakFlag || g_psContinueFlag)
			break;
	}

	for(int i = 0; i < addedFuncs.size(); i++)
		g_psFunctions.erase(addedFuncs[i]);
	for(int i = 0; i < addedVars.size(); i++)
		g_psVariables.erase(addedVars[i]);
}

static PSdata _ps_evaluate_statement(PSast* ast, const PSnode& node, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars)
{
	switch(node.type)
	{
	case PSnode::OP:
	{
		if(node.op.type == PSnode::OP::EQUAL)
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_evaluate_statement(ast, ast->nodePool[node.op.right], addedFuncs, addedVars), addedFuncs, addedVars);

		PSdata left  = _ps_evaluate_statement(ast, ast->nodePool[node.op.left ], addedFuncs, addedVars);
		PSdata right = _ps_evaluate_statement(ast, ast->nodePool[node.op.right], addedFuncs, addedVars);

		switch(node.op.type)
		{
		case PSnode::OP::MULT:
			return _ps_mult(left, right, node);
		case PSnode::OP::DIV:
			return _ps_div(left, right, node);
		case PSnode::OP::MOD:
			return _ps_mod(left, right, node);
		case PSnode::OP::ADD:
			return _ps_add(left, right, node);
		case PSnode::OP::SUB:
			return _ps_sub(left, right, node);
		case PSnode::OP::MULTEQUAL:
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_mult(left, right, node), addedFuncs, addedVars);
		case PSnode::OP::DIVEQUAL:
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_div(left, right, node), addedFuncs, addedVars);
		case PSnode::OP::MODEQUAL:
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_mod(left, right, node), addedFuncs, addedVars);
		case PSnode::OP::ADDEQUAL:
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_add(left, right, node), addedFuncs, addedVars);
		case PSnode::OP::SUBEQUAL:
			return _ps_equal(ast, ast->nodePool[node.op.left], _ps_sub(left, right, node), addedFuncs, addedVars);
		case PSnode::OP::LESSTHAN:
			return _ps_lessthan(left, right, node);
		case PSnode::OP::GREATERTHAN:
			return _ps_greaterthan(left, right, node);
		case PSnode::OP::LESSTHANEQUAL:
			return _ps_lessthanequal(left, right, node);
		case PSnode::OP::GREATERTHANEQUAL:
			return _ps_greaterthanequal(left, right, node);
		case PSnode::OP::EQUALITY:
			return _ps_equality(left, right, node);
		case PSnode::OP::NONEQUALITY:
		{
			PSdata equality = _ps_equality(left, right, node);
			equality.intVal = !equality.intVal;
			return equality;
		}
		case PSnode::OP::AND:
			return _ps_and(left, right, node);
		case PSnode::OP::OR:
			return _ps_or(left, right, node);
		default:
			_ps_error(PSruntimeError::UNSUPPORTED_NODE_TYPE, node);
		}

		return {};
	}
	case PSnode::ID:
	{
		if(node.id.type == PSnode::ID::FUNC)
		{
			std::vector<PSdata> params;
			for(int i = 0; i < node.id.params.size(); i++)
				params.push_back(_ps_evaluate_statement(ast, ast->nodePool[node.id.params[i]], addedFuncs, addedVars));

			if(g_psLibFunctions.count(node.id.name) > 0)
				return g_psLibFunctions[node.id.name].func(params, node, g_psLibFunctionUserData);
			else if(g_psFunctions.count(node.id.name) > 0)
				return _ps_execute_function(ast, node, addedFuncs, addedVars);
			else
				_ps_error(PSruntimeError::UNDEFINED_FUNCTION, node);
		}
		else
		{
			if(g_psConstants.count(node.id.name) == 1)
				return g_psConstants[node.id.name];

			if(g_psVariables.count(node.id.name) == 0)
				_ps_error(PSruntimeError::UNDEFINED_VARIABLE, node);

			PSdata var = g_psVariables[node.id.name];
			if(node.id.params.size() == 0)
				return var;

			if(var.type == PSdata::INT || var.type == PSdata::FLOAT)
				_ps_error(PSruntimeError::INVALID_INDEX, node);

			PSdata index = _ps_evaluate_statement(ast, ast->nodePool[node.id.params[0]], addedFuncs, addedVars);
			if(index.type != PSdata::INT)
				_ps_error(PSruntimeError::INVALID_INDEX, node);

			PSdata result;
			result.type = PSdata::FLOAT;

			if(var.type == PSdata::VEC2 && index.intVal <= 1)
				result.floatVal = *((float*)&var.vec2Val + index.intVal);
			else if(var.type == PSdata::VEC3 && index.intVal <= 2)
				result.floatVal = *((float*)&var.vec3Val + index.intVal);
			else if(var.type == PSdata::VEC4 && index.intVal <= 3)
				result.floatVal = *((float*)&var.vec4Val + index.intVal);
			else
				_ps_error(PSruntimeError::INVALID_INDEX, node);

			return result;
		}

		return {};
	}
	case PSnode::NUMBER:
	{
		PSdata num;

		if(node.literal.type == PSnode::Literal::INT)
		{
			num.type = PSdata::INT;
			num.intVal = node.literal.intNum;
		}
		else
		{
			num.type = PSdata::FLOAT;
			num.floatVal = node.literal.floatNum;
		}

		return num;
	}
	case PSnode::KEYWORD:
	{
		switch(node.keyword.type)
		{
		case PSnode::Keyword::IF:
		{
			PSdata condition = _ps_evaluate_statement(ast, ast->nodePool[node.keyword.condition], addedVars, addedFuncs);
			if(_ps_get_scalar(condition, PSruntimeError::INVALID_CONDITION, node) != 0.0f)
				_ps_execute_statements(ast, node.keyword.code);
			else if(node.keyword.hasElse)
				_ps_execute_statements(ast, node.keyword.elseCode);

			return {};
		}
		case PSnode::Keyword::FOR:
		{
			if(ast->nodePool[node.keyword.condition].type != PSnode::OP ||
			   ast->nodePool[node.keyword.condition].op.type != PSnode::OP::IN)
			   	_ps_error(PSruntimeError::INVALID_CONDITION, node);

			PSnode var = ast->nodePool[ast->nodePool[node.keyword.condition].op.left];
			if(var.type != PSnode::ID || var.id.type != PSnode::ID::VAR || g_psVariables.count(var.id.name) > 0)
				_ps_error(PSruntimeError::INVALID_CONDITION, node);

			std::vector<std::string> forFuncs;
			std::vector<std::string> forVars;

			PSdata range = _ps_evaluate_statement(ast, ast->nodePool[ast->nodePool[node.keyword.condition].op.right], forFuncs, forVars);
			if(range.type != PSdata::VEC2)
				_ps_error(PSruntimeError::INVALID_CONDITION, node);

			bool outermostLoop = false;
			if(!g_psInLoop)
			{
				outermostLoop = true;
				g_psInLoop = true;
			}

			int32_t min = (int32_t)ceilf (range.vec2Val.x);
			int32_t max = (int32_t)floorf(range.vec2Val.y);
			for(int32_t i = min; i <= max; i++)
			{
				PSdata val;
				val.type = PSdata::INT;
				val.intVal = i;
				_ps_equal(ast, var, val, forFuncs, forVars);

				_ps_execute_statements(ast, node.keyword.code);

				if(g_psBreakFlag)
				{
					g_psBreakFlag = false;
					break;
				}

				if(g_psContinueFlag)
				{
					g_psContinueFlag = false;
					continue;
				}
			}

			for(int i = 0; i < forFuncs.size(); i++)
				g_psFunctions.erase(forFuncs[i]);
			for(int i = 0; i < forVars.size(); i++)
				g_psVariables.erase(forVars[i]);

			if(outermostLoop)
				g_psInLoop = false;

			return {};
		}
		case PSnode::Keyword::FUNC:
		{
			if(g_psFunctions.count(node.keyword.name) > 0)
				_ps_error(PSruntimeError::FUNCTION_REDEFINITION, node);

			g_psFunctions[node.keyword.name] = node;
			addedFuncs.push_back(node.keyword.name);

			return {};
		}
		case PSnode::Keyword::RETURN:
		{
			g_psReturnFlag = true;

			if(node.keyword.returnVal < UINT32_MAX)
				g_psReturnVal = _ps_evaluate_statement(ast, ast->nodePool[node.keyword.returnVal], addedFuncs, addedVars);
			else
				g_psReturnVal = {};

			return {};
		}
		case PSnode::Keyword::BREAK:
		{
			if(!g_psInLoop)
				_ps_error(PSruntimeError::INVALID_BREAK_CONTINUE, node);

			g_psBreakFlag = true;
			return {};
		}
		case PSnode::Keyword::CONTINUE:
		{
			if(!g_psInLoop)
				_ps_error(PSruntimeError::INVALID_BREAK_CONTINUE, node);

			g_psContinueFlag = true;
			return {};
		}
		default:
			_ps_error(PSruntimeError::UNSUPPORTED_NODE_TYPE, node);		
		}
		
		return {};
	}
	default:
		_ps_error(PSruntimeError::UNSUPPORTED_NODE_TYPE, node);
		return {};
	}
}

static inline PSdata _ps_execute_function(PSast* ast, const PSnode& node, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars)
{
	PSnode funcNode = g_psFunctions[node.id.name];

	if(funcNode.keyword.paramNames.size() != node.id.params.size())
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	std::unordered_map<std::string, PSdata> funcVars;

	for(int i = 0; i < funcNode.keyword.paramNames.size(); i++)
	{
		if(funcVars.count(funcNode.keyword.paramNames[i]) > 0)
			_ps_error(PSruntimeError::ARGUMENT_NAME_REDEFINITION, funcNode);

		funcVars[funcNode.keyword.paramNames[i]] = _ps_evaluate_statement(ast, ast->nodePool[node.id.params[i]], addedFuncs, addedVars);
	}

	funcVars.swap(g_psVariables);
	_ps_execute_statements(ast, funcNode.keyword.code);
	funcVars.swap(g_psVariables);

	for(int i = 0; i < funcNode.keyword.paramNames.size(); i++)
		g_psVariables.erase(funcNode.keyword.paramNames[i]);

	if(g_psReturnFlag)
	{
		g_psReturnFlag = false;
		return g_psReturnVal;
	}
	else
		return {};
}

//--------------------------------------------------------------------------------------------------------------------------------//

static inline float _ps_get_scalar(PSdata data, PSruntimeError potentialError, const PSnode& node)
{
	if(data.type == PSdata::INT)
		return (float)data.intVal;
	else if(data.type == PSdata::FLOAT)
		return data.floatVal;
	else
		_ps_error(potentialError, node);

	return 0.0f;
}

static inline void _ps_error(PSruntimeError error, PSnode errorNode)
{
	g_psError = error;
	g_psErrorNode = errorNode;
	throw std::exception();
}

//--------------------------------------------------------------------------------------------------------------------------------//

static inline PSdata _ps_mult(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;

	if(left.type == PSdata::INT && right.type == PSdata::INT)
	{
		result.type = PSdata::INT;
		result.intVal = left.intVal * right.intVal;
	}
	else if(left.type == PSdata::FLOAT && right.type == PSdata::FLOAT)
	{
		result.type = PSdata::FLOAT;
		result.floatVal = left.floatVal * right.floatVal;
	}
	else if((left.type == PSdata::INT   && right.type == PSdata::FLOAT) ||
	        (left.type == PSdata::FLOAT && right.type == PSdata::INT))
	{
		result.type = PSdata::FLOAT;
		result.floatVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) * _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC2 && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val * right.vec2Val;
	}
	else if(left.type == PSdata::VEC3 && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val * right.vec3Val;
	}
	else if(left.type == PSdata::VEC4 && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val * right.vec4Val;
	}
	else if(left.type == PSdata::QUATERNION && right.type == PSdata::QUATERNION)
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = left.quatVal * right.quatVal;
	}
	else if(left.type == PSdata::VEC2 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val * _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = right.vec2Val * _ps_get_scalar(left, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC3 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val * _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = right.vec3Val * _ps_get_scalar(left, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC4 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val * _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = right.vec4Val * _ps_get_scalar(left, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::QUATERNION && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = left.quatVal * _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::QUATERNION)
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = right.quatVal * _ps_get_scalar(left, PSruntimeError::INVALID_OP, node);
	}
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_div(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;

	if(left.type == PSdata::INT && right.type == PSdata::INT)
	{
		result.type = PSdata::INT;
		result.intVal = left.intVal / right.intVal;
	}
	else if(left.type == PSdata::FLOAT && right.type == PSdata::FLOAT)
	{
		result.type = PSdata::FLOAT;
		result.floatVal = left.floatVal / right.floatVal;
	}
	else if((left.type == PSdata::INT   && right.type == PSdata::FLOAT) ||
	        (left.type == PSdata::FLOAT && right.type == PSdata::INT))
	{
		result.type = PSdata::FLOAT;
		result.floatVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) / _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC2 && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val / right.vec2Val;
	}
	else if(left.type == PSdata::VEC3 && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val / right.vec3Val;
	}
	else if(left.type == PSdata::VEC4 && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val / right.vec4Val;
	}
	else if(left.type == PSdata::VEC2 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val / _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) / right.vec2Val;
	}
	else if(left.type == PSdata::VEC3 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val / _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) / right.vec3Val;
	}
	else if(left.type == PSdata::VEC4 && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val / _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) / right.vec4Val;
	}
	else if(left.type == PSdata::QUATERNION && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = left.quatVal / _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && right.type == PSdata::QUATERNION)
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = right.quatVal / _ps_get_scalar(left, PSruntimeError::INVALID_OP, node);
	}
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_mod(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;

	if(left.type == PSdata::INT && right.type == PSdata::INT)
	{
		result.type = PSdata::INT;
		result.intVal = left.intVal % right.intVal;
	}
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_add(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;

	if(left.type == PSdata::INT && right.type == PSdata::INT)
	{
		result.type = PSdata::INT;
		result.intVal = left.intVal + right.intVal;
	}
	else if(left.type == PSdata::FLOAT && right.type == PSdata::FLOAT)
	{
		result.type = PSdata::FLOAT;
		result.floatVal = left.floatVal + right.floatVal;
	}
	else if((left.type == PSdata::INT   && right.type == PSdata::FLOAT) ||
	        (left.type == PSdata::FLOAT && right.type == PSdata::INT))
	{
		result.type = PSdata::FLOAT;
		result.floatVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) + _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC2 && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val + right.vec2Val;
	}
	else if(left.type == PSdata::VEC3 && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val + right.vec3Val;
	}
	else if(left.type == PSdata::VEC4 && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val + right.vec4Val;
	}
	else if(left.type == PSdata::QUATERNION && right.type == PSdata::QUATERNION)
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = left.quatVal + right.quatVal;
	}
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_sub(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;

	if(left.type == PSdata::INT && right.type == PSdata::INT)
	{
		result.type = PSdata::INT;
		result.intVal = left.intVal - right.intVal;
	}
	else if(left.type == PSdata::FLOAT && right.type == PSdata::FLOAT)
	{
		result.type = PSdata::FLOAT;
		result.floatVal = left.floatVal - right.floatVal;
	}
	else if((left.type == PSdata::INT   && right.type == PSdata::FLOAT) ||
	        (left.type == PSdata::FLOAT && right.type == PSdata::INT))
	{
		result.type = PSdata::FLOAT;
		result.floatVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) - _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	}
	else if(left.type == PSdata::VEC2 && right.type == PSdata::VEC2)
	{
		result.type = PSdata::VEC2;
		result.vec2Val = left.vec2Val - right.vec2Val;
	}
	else if(left.type == PSdata::VEC3 && right.type == PSdata::VEC3)
	{
		result.type = PSdata::VEC3;
		result.vec3Val = left.vec3Val - right.vec3Val;
	}
	else if(left.type == PSdata::VEC4 && right.type == PSdata::VEC4)
	{
		result.type = PSdata::VEC4;
		result.vec4Val = left.vec4Val - right.vec4Val;
	}
	else if(left.type == PSdata::QUATERNION && right.type == PSdata::QUATERNION)
	{
		result.type = PSdata::QUATERNION;
		result.quatVal = left.quatVal - right.quatVal;
	}
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_equal(PSast* ast, const PSnode& var, const PSdata& val, std::vector<std::string>& addedFuncs, std::vector<std::string>& addedVars)
{
	if(var.type != PSnode::ID || var.id.type != PSnode::ID::VAR)
		_ps_error(PSruntimeError::INVALID_ASSIGNMENT, var);
			
	if(val.type == PSdata::VOID)
		_ps_error(PSruntimeError::INVALID_ASSIGNMENT, var);

	if(g_psVariables.count(var.id.name) > 0)
	{
		if(g_psVariables[var.id.name].type == PSdata::FLOAT && val.type == PSdata::INT)
		{
			g_psVariables[var.id.name].floatVal = (float)val.intVal;
			return g_psVariables[var.id.name];
		}
		else if(var.id.params.size() == 1)
		{
			PSdata index = _ps_evaluate_statement(ast, ast->nodePool[var.id.params[0]], addedFuncs, addedVars);
			if(index.type != PSdata::INT)
				_ps_error(PSruntimeError::INVALID_INDEX, var);

			PSdata* varRef = &g_psVariables[var.id.name];
			float floatVal = _ps_get_scalar(val, PSruntimeError::INVALID_ASSIGNMENT, var);

			if(varRef->type == PSdata::VEC2 && index.intVal <= 1)
				*((float*)&varRef->vec2Val + index.intVal) = floatVal;
			else if(varRef->type == PSdata::VEC3 && index.intVal <= 2)
				*((float*)&varRef->vec3Val + index.intVal) = floatVal;
			else if(varRef->type == PSdata::VEC4 && index.intVal <= 3)
				*((float*)&varRef->vec4Val + index.intVal) = floatVal;
			else
				_ps_error(PSruntimeError::INVALID_INDEX, var);

			PSdata result;
			result.type = PSdata::FLOAT;
			result.floatVal = floatVal;
			return result;
		}
		else if(g_psVariables[var.id.name].type != val.type)
			_ps_error(PSruntimeError::INVALID_ASSIGNMENT, var);
	}
	else if(var.id.params.size() != 0)
		_ps_error(PSruntimeError::INVALID_INDEX, var);
	else
		addedVars.push_back(var.id.name);

	g_psVariables[var.id.name] = val;
	return val;
}

static inline PSdata _ps_lessthan(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) < _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_greaterthan(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) > _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_lessthanequal(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) <= _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_greaterthanequal(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) >= _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_equality(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	if((left.type == PSdata::INT || left.type == PSdata::FLOAT) && (right.type == PSdata::INT || right.type == PSdata::FLOAT))
		result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) == _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);
	else if(left.type == PSdata::VEC2 && right.type == PSdata::VEC2)
		result.intVal = left.vec2Val == right.vec2Val;
	else if(left.type == PSdata::VEC3 && right.type == PSdata::VEC3)
		result.intVal = left.vec3Val == right.vec3Val;
	else if(left.type == PSdata::VEC4 && right.type == PSdata::VEC4)
		result.intVal = left.vec4Val == right.vec4Val;
	else
		_ps_error(PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_and(const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) && _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;
}

static inline PSdata _ps_or (const PSdata& left, const PSdata& right, const PSnode& node)
{
	PSdata result;
	result.type = PSdata::INT;

	result.intVal = _ps_get_scalar(left, PSruntimeError::INVALID_OP, node) || _ps_get_scalar(right, PSruntimeError::INVALID_OP, node);

	return result;	
}

//--------------------------------------------------------------------------------------------------------------------------------//

static PSdata _ps_range(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 2 || params[0].type != PSdata::INT || params[1].type != PSdata::INT)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	PSdata result;
	result.type = PSdata::VEC2;
	result.vec2Val = {(float)params[0].intVal, (float)params[1].intVal};
	return result;
}

static PSdata _ps_print(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	for(int i = 0; i < params.size(); i++)
	{
		switch(params[i].type)
		{
		case PSdata::INT:
			std::cout << params[i].intVal;
			break;
		case PSdata::FLOAT:
			std::cout << params[i].floatVal;
			break;
		case PSdata::VEC2:
			std::cout << "(" << params[i].vec2Val.x << ", " << params[i].vec2Val.y << ")";
			break;
		case PSdata::VEC3:
			std::cout << "(" << params[i].vec3Val.x << ", " << params[i].vec3Val.y << ", " << params[i].vec3Val.z << ")";
			break;
		case PSdata::VEC4:
			std::cout << "(" << params[i].vec4Val.x << ", " << params[i].vec4Val.y << ", " << params[i].vec4Val.z << ", " << params[i].vec4Val.w << ")";
			break;
		default:
			_ps_error(PSruntimeError::INVALID_PARAMS, node);
		}

		if(i < params.size() - 1)
			std::cout << ", ";
	}

	std::cout << std::endl;

	return {};
}

float _ps_scalar_rand(float min, float max)
{
	return (float)rand() / RAND_MAX * (max - min) + min;
}

static PSdata _ps_rand(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 2)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	PSdata result;

	if(params[0].type == PSdata::VEC2 && params[1].type == PSdata::VEC2)
	{
		qm::vec2 min = params[0].vec2Val;
		qm::vec2 max = params[1].vec2Val;

		result.type = PSdata::VEC2;
		result.vec2Val.x = _ps_scalar_rand(min.x, max.x);
		result.vec2Val.y = _ps_scalar_rand(min.y, max.y);
	}
	else if(params[0].type == PSdata::VEC3 && params[1].type == PSdata::VEC3)
	{
		qm::vec3 min = params[0].vec3Val;
		qm::vec3 max = params[1].vec3Val;
		
		result.type = PSdata::VEC3;
		result.vec3Val.x = _ps_scalar_rand(min.x, max.x);
		result.vec3Val.y = _ps_scalar_rand(min.y, max.y);
		result.vec3Val.z = _ps_scalar_rand(min.z, max.z);
	}
	else if(params[0].type == PSdata::VEC4 && params[1].type == PSdata::VEC4)
	{
		qm::vec4 min = params[0].vec4Val;
		qm::vec4 max = params[1].vec4Val;
		
		result.type = PSdata::VEC4;
		result.vec4Val.x = _ps_scalar_rand(min.x, max.x);
		result.vec4Val.y = _ps_scalar_rand(min.y, max.y);
		result.vec4Val.z = _ps_scalar_rand(min.z, max.z);
		result.vec4Val.w = _ps_scalar_rand(min.w, max.w);
	}
	else if(params[0].type == PSdata::INT && params[1].type == PSdata::INT)
	{
		int min = params[0].intVal;
		int max = params[1].intVal;

		result.type = PSdata::INT;
		result.intVal = rand() % (max - min) + min;
	}
	else
	{
		float min = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		float max = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);

		result.type = PSdata::FLOAT;
		result.floatVal = _ps_scalar_rand(min, max);
	}

	return result;
}

static PSdata _ps_int(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	PSdata result;
	result.type = PSdata::INT;
	result.intVal = (int)_ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);

	return result;
}

static PSdata _ps_vec2(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{	
	PSdata result;
	result.type = PSdata::VEC2;

	switch(params.size())
	{
	case 0:
	{
		result.vec2Val = {0.0f, 0.0f};
		break;
	}
	case 1:
	{
		float val = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		result.vec2Val = {val, val};
		break;
	}
	case 2:
	{
		float x = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		float y = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		result.vec2Val = {x, y};
		break;
	}
	default:
		_ps_error(PSruntimeError::INVALID_PARAMS, node);
	}

	return result;
}

static PSdata _ps_vec3(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	PSdata result;
	result.type = PSdata::VEC3;

	switch(params.size())
	{
	case 0:
	{
		result.vec3Val = {0.0f, 0.0f, 0.0f};
		break;
	}
	case 1:
	{
		float val = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		result.vec3Val = {val, val, val};
		break;
	}
	case 2:
	{
		if(params[0].type != PSdata::VEC2)
			_ps_error(PSruntimeError::INVALID_PARAMS, node);
		
		qm::vec2 vecVal = params[0].vec2Val;
		float val = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		result.vec3Val = {vecVal.x, vecVal.y, val};
		break;
	}
	case 3:
	{
		float x = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		float y = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		float z = _ps_get_scalar(params[2], PSruntimeError::INVALID_PARAMS, node);
		result.vec3Val = {x, y, z};
		break;
	}
	default:
		_ps_error(PSruntimeError::INVALID_PARAMS, node);
	}

	return result;
}

static PSdata _ps_vec4(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	PSdata result;
	result.type = PSdata::VEC4;

	switch(params.size())
	{
	case 0:
	{
		result.vec4Val = {0.0f, 0.0f, 0.0f, 0.0f};
		break;
	}
	case 1:
	{
		float val = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		result.vec4Val = {val, val, val, val};
		break;
	}
	case 2:
	{
		if(params[0].type != PSdata::VEC3)
			_ps_error(PSruntimeError::INVALID_PARAMS, node);
		
		qm::vec3 vecVal = params[0].vec3Val;
		float val = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		result.vec4Val = {vecVal.x, vecVal.y, vecVal.z, val};
		break;
	}
	case 3:
	{
		float x = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
		float y = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		float z = _ps_get_scalar(params[2], PSruntimeError::INVALID_PARAMS, node);
		float w = _ps_get_scalar(params[3], PSruntimeError::INVALID_PARAMS, node);
		result.vec4Val = {x, y, z, w};
		break;
	}
	default:
		_ps_error(PSruntimeError::INVALID_PARAMS, node);
	}

	return result;
}

static PSdata _ps_quaternion(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() > 0 && params[0].type != PSdata::VEC3)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::QUATERNION;

	if(params.size() == 0)
	{
		result.quatVal = qm::quaternion_identity();
	}
	else if(params.size() == 1)
	{
		qm::vec3 angles = params[0].vec3Val;
		angles.x = qm::rad_to_deg(angles.x);
		angles.y = qm::rad_to_deg(angles.y);
		angles.z = qm::rad_to_deg(angles.z);
		result.quatVal = qm::quaternion_from_euler(angles);
	}
	else if(params.size() == 2)
	{
		qm::vec3 axis = params[0].vec3Val;
		float angle = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
		angle = qm::rad_to_deg(angle);
		result.quatVal = qm::quaternion_from_axis_angle(axis, angle);
	}
	else
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	return result;
}

static PSdata _ps_sqrt(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = sqrtf(input);

	return result;
}

static PSdata _ps_pow(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 2)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float base = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	float exp  = _ps_get_scalar(params[1], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = powf(base, exp);

	return result;
}

static PSdata _ps_sin(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = sinf(input);

	return result;
}

static PSdata _ps_cos(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = cosf(input);

	return result;
}

static PSdata _ps_tan(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = tanf(input);

	return result;
}

static PSdata _ps_asin(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = asinf(input);

	return result;
}

static PSdata _ps_acos(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = acosf(input);

	return result;
}

static PSdata _ps_atan(const std::vector<PSdata>& params, const PSnode& node, void* userData)
{
	if(params.size() != 1)
		_ps_error(PSruntimeError::INVALID_PARAMS, node);

	float input = _ps_get_scalar(params[0], PSruntimeError::INVALID_PARAMS, node);
	
	PSdata result;
	result.type = PSdata::FLOAT;
	result.floatVal = atanf(input);

	return result;
}