<p align="center">
  <img src="PropScriptLogo.png" />
</p>

# PropScript
PropScript is a small, interpreted programming language designed for scripting. I wrote it to facilitate the development of Terra Toy, my video game. It has a similar syntax to python and was written in ~2500 lines of code with 0 dependencies. Please note that this is merely a hobby project; It is lacking in many features, error messages are very vague, and there are likely to be a number of yet undiscovered issues.

## Features
- Basic Arithmetic (int, float, vec2, vec3, vec4, quaternion)
- If/Else Statements
- Functions (declared with "func" keyword)
- For Loops (declared similar to python: "for var in range(...)")
- Support for adding functions & constants defined in C++
- Saving and loading the abstract syntax tree to disk (similar to an object file)

Overall, the syntax is mostly identifcal to that of python, with the one notable difference being that multi-line code blocks must be enclosed in curly braces, tabs and all other whitespace is completely ignored. Additionally, individual elements of vectors are accesed as arrays, not with a "." operator (such as ".x"). The first element of a vector is accesed as "myVector[0]", for example.

## Does NOT Support
- While/Do-While Loops
- String Literals
- Structs/Classes
- Arrays
- And Much More :)

## Building
The project can be built using the included CMake file, no dependencies are required. The main function shows how to lex, parse, and execute an example script. The example script, which prints prime numbers, can be found in "examples/example.ps".
