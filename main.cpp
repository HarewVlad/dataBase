#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <cstdlib>
#include <vector>
#include <map>
#include <cstdarg>

// Main TODO: Should i clean code after every exec?
// Main TODO: Should i all the time open the dbf?

// Block allocator
#define MIN_BLOCK_SIZE 1024
#define MEMORY_ALIGN 64
#define MAX(a, b) (a) > (b) ? (a) : (b)
#define ALIGN(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define ALIGN_PTR(a, b) (char *)(((uintptr_t)(a) + (b) - 1) & ~((b) - 1))
struct Memory
{
	void *begin;
	void *end;
};

Memory *arena = nullptr;
std::vector<Memory *> memoryBlocks;

void *allocMemory(size_t size)
{
	static bool isInited = false;
	if (!isInited)
	{
		// Init
		arena = (Memory *)malloc(sizeof(Memory));
		arena->begin = nullptr;
		arena->end = nullptr;

		isInited = true;
	}
	
	if (size > (size_t)((size_t)arena->begin - (size_t)arena->end))
	{
		// Grow
		size_t newSize = ALIGN(MAX(size, MIN_BLOCK_SIZE), MEMORY_ALIGN);
		arena->begin = malloc(newSize);
		arena->end = (void *)((size_t)arena->begin + newSize);

		memoryBlocks.push_back(arena);
	}
	Memory memoryBlock = {};
	memoryBlock.begin = arena->begin;
	memoryBlock.end = (void *)((size_t)arena->begin + size);

	arena->begin = ALIGN_PTR(memoryBlock.end, MEMORY_ALIGN);

	return memoryBlock.begin;
}

void freeMemory()
{
	for (auto b : memoryBlocks)
	{
		free(b);
	}
}

// Utils
char *copyString(const char *src, size_t len)
{
	char *result = (char *)malloc(len);
	char *beginResult = result;
	while (*src != '\0' && len--)
	{
		*result++ = *src++;
	}
	*result = '\0';
	return beginResult;
}

void fatal(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("fatal: ");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	exit(1);
}

// Hashing
uint64_t hashString(const char *string, size_t length)
{
	static const uint64_t fnvPrime = 0x100000001b3ull;
	static const uint64_t fnvInitializer = 0xcbf29ce484222325ull;
	uint64_t hash = fnvInitializer;
	for (size_t i = 0; i < length; i++)
	{
		hash ^= string[i];
		hash *= fnvPrime;
	}
	return hash;
}

std::map<uint64_t, const char *> keywords;
void putKeyword(const char *keyword, size_t length)
{
	uint64_t hash = hashString(keyword, length);
	keywords.insert(std::pair<uint64_t, const char *>(hash, keyword));
}

// Lexer
enum TokenKind
{
	TOKEN_NONE = 0,
	// Types
	TOKEN_KEYWORD = 1,
	TOKEN_INT,
	TOKEN_NAME,
	TOKEN_STRING,
	// LBRACES, RBRACE ...
	TOKEN_RBRACE,
	TOKEN_LBRACE,
	TOKEN_RPARENT,
	TOKEN_LPARENT,
	TOKEN_SEMICOLON,
	TOKEN_COMMA,
	TOKEN_STAR,
};

char *stream = nullptr;

struct Token
{
	TokenKind kind;

	union
	{
		char *name;
		int value;
	};

	Token()
	{

	}

	~Token()
	{

	}
};

Token token;

void initKeywords()
{
	putKeyword("insert", strlen("insert"));
	putKeyword("values", strlen("values"));
	putKeyword("table", strlen("table"));
	putKeyword("create", strlen("create"));
	putKeyword("into", strlen("into"));
	putKeyword("select", strlen("select"));
	putKeyword("from", strlen("from"));
}

bool isKeyword(const char *name, size_t length)
{
	uint64_t hash = hashString(name, length);
	auto it = keywords.find(hash);
	if (it != keywords.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}

void next()
{
begin:
	token.kind = TOKEN_NONE;
	switch (*stream)
	{
		case ' ': case '\n': case '\r': case '\t':
			{
				stream++;
				goto begin;
			}
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			{
				int value = 0;
				while (isdigit(*stream))
				{
					value *= 10;
					value += *stream++ - '0';
				}

				// TODO: double, float, ...
				token.kind = TOKEN_INT;
				token.value = value;
			}
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
		case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
		case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
		case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
		case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
		case '_':
			{
				char *begin = stream;
				while (isalnum(*stream) || *stream == '_')
				{
					stream++;
				}
				char *end = stream;
				token.name = copyString(begin, end - begin);
				token.kind = isKeyword(token.name, end - begin) ? TOKEN_KEYWORD : TOKEN_NAME;
			}
			break;
		case '\'':
			{
				stream++;
				char *begin = stream;
				while (*stream != '\'')
				{
					stream++;
				}
				char *end = stream;
				token.name = copyString(begin, end - begin);
				token.kind = TOKEN_STRING;
				stream++;
			}
			break;
		case '(':
			{
				token.name = stream;
				token.kind = TOKEN_RPARENT;
				stream++;
			}
			break;
		case ')':
			{
				token.name = stream;
				token.kind = TOKEN_LPARENT;
				stream++;
			}
			break;
		case ',':
			{
				token.name = stream;
				token.kind = TOKEN_COMMA;
				stream++;
			}
			break;
		case ';':
			{
				token.name = stream;
				token.kind = TOKEN_SEMICOLON;
				stream++;
			}
			break;
		case '*':
			{
				token.name = stream;
				token.kind = TOKEN_STAR;
				stream++;
			}
			break;
		default:
			{
				token.name = stream;
				stream++;
			}
			break;
	}
}

void initStream(char *commands)
{
	stream = commands;
	next();
}

// Parser

enum Type
{
	TYPE_NONE,
	TYPE_STRING,
	TYPE_INT,
};

struct Expr
{
	Type kind;
	union
	{
		char *string;
		int value;
		// ...
	};
};

Expr *newExpr(Type kind)
{
	Expr *e = (Expr *)malloc(sizeof(Expr));
	e->kind = kind;
	return e;
}

Expr *newExprInt(int value)
{
	Expr *e = newExpr(TYPE_INT);
	e->value = value;
	return e;
}

Expr *newExprString(char *string)
{
	Expr *e = newExpr(TYPE_STRING);
	e->string = string;
	return e;
}

struct Insert
{
	char *tableName;
	std::vector<Expr *> values;
};

struct Read
{
	char *tableName;
	// TODO: add more info later
};

struct Var
{
	char *varName;
	Type varType;
};

struct Table
{
	char *tableName;
	std::vector<Var> tableVars;
};

std::vector<Insert> insertions;
std::vector<Read> reads;
std::vector<Table> tables;

Insert insertion;
Table table;

bool matchToken(TokenKind tokenKind)
{
	if (token.kind == tokenKind)
	{
		next();
		return true;
	}
	else
	{
		return false;
	}
}

bool expectToken(TokenKind tokenKind)
{
	if (token.kind == tokenKind)
	{
		next();
		return true;
	}
	else
	{
		fatal("unexpected token '%c'", token.kind);
		return false;
	}
}

bool matchKeyword(const char *keyword)
{
	if (token.kind == TOKEN_KEYWORD && strcmp(token.name, keyword) == 0)
	{
		next();
		return true;
	}
	else
	{
		return false;
	}
}

char *parseName()
{
	char *name = {};
	name = token.name;
	expectToken(TOKEN_NAME);
	return name;
}

Type parseVarKind() // TODO: redo 'if' part
{
	Type kind = {};
	if (strcmp(token.name, "int") == 0)
	{
		kind = TYPE_INT;
	}
	else if (strcmp(token.name, "string") == 0)
	{
		kind = TYPE_STRING;
	}
	else
	{
		kind = TYPE_NONE;
	}
	next();
	return kind;
}

int parseInt()
{
	int value = {};
	value = token.value;
	expectToken(TOKEN_INT);
	return value;
}

char *parseString()
{
	char *string = {};
	string = token.name;
	expectToken(TOKEN_STRING);
	return string;
}

Expr *parseValue()
{
	Expr *value = newExpr(TYPE_NONE);
	if (token.kind == TOKEN_STRING)
	{
		value = newExprString(parseString());
	}
	else if (token.kind == TOKEN_INT)
	{
		value = newExprInt(parseInt());
	}

	return value;
}

char *getStringFromFile(FILE **f)
{
	char *string = (char *)malloc(256); // TODO: redo this
	int length = 0;
	int c = 0;
	while ((c = fgetc(*f)) != '\0')
	{
		*string++ = c;
		length++;
	}
	*string = '\0';
	string -= length;

	return string;
}

uint16_t read16(FILE **f) // TODO: free memory
{
	char *byte2Char = (char *)malloc(3); // 3 -> 2 for number and 1 for '\0'
	memset(byte2Char, 0, 3);
	fread(byte2Char, 2, 1, *f);

	uint16_t byte2 = ((uint8_t)byte2Char[1] << 8) | (uint8_t)byte2Char[0];

	return byte2;
}

uint32_t read32(FILE **f) // TODO: free memory
{
	char *byte4Char = (char *)malloc(5); // 5 -> 4 for number and 1 for '\0'
	memset(byte4Char, 0, 5);
	fread(byte4Char, 4, 1, *f);

	uint32_t byte4 = ((uint8_t)byte4Char[3] << 24)
		| ((uint8_t)byte4Char[2] << 16)
		| ((uint8_t)byte4Char[1] << 8)
		| (uint8_t)byte4Char[0];

	return byte4;
}

enum
{
	BEGIN_TABLE_DESC = 0x11ff,
	END_TABLE_DESC = 0xff11,

	BEGIN_TABLE_DATA = 0x22ff,
	END_TABLE_DATA = 0xff22
};

bool matchByte2(uint16_t byte2, FILE **f)
{
	if (read16(f) == byte2)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool isByte2(uint16_t byte2, FILE **f)
{
	if (read16(f) == byte2)
	{
		fseek(*f, -2, SEEK_CUR);
		return true;
	}
	else
	{
		fseek(*f, -2, SEEK_CUR);
		return false;
	}
}

void readData(char *tableName)
{
	FILE *f = fopen("dataBase.df", "rb");
	if (!f)
	{
		printf("Data base file not created or cant be opened\n");
		return;
	}

readTableName:
	char *tableNameFromFile = getStringFromFile(&f);
	if (strcmp(tableNameFromFile, tableName) == 0)
	{
		printf("Table name is -> '%s'\n", tableNameFromFile);
			
		if (matchByte2(BEGIN_TABLE_DATA, &f))
		{
			while (!isByte2(END_TABLE_DATA, &f))
			{
				char *varType = getStringFromFile(&f);
				if (strcmp(varType, "int") == 0)
				{
					int varValue = read32(&f);
					printf("Value is int -> '%d'\n", varValue);
				}
				else if (strcmp(varType, "string") == 0)
				{
					char *varValue = getStringFromFile(&f);
					printf("Value is string -> '%s'\n", varValue);
				}
			}
		}
	}
	else
	{
		while (!matchByte2(END_TABLE_DATA, &f));
		goto readTableName;
	}
}

enum CommandType
{
	COMMAND_CREATE,
	COMMAND_INSERT,
	COMMAND_SELECT,
};

CommandType parse()
{
	if (matchKeyword("create"))
	{
		if (matchKeyword("table"))
		{
			Table t;
			t.tableName = parseName();			

			expectToken(TOKEN_RPARENT);

			char *varName = parseName();
			Type varKind = parseVarKind();
			t.tableVars.push_back(Var{ varName, varKind });
			while (matchToken(TOKEN_COMMA))
			{
				varName = parseName();
				varKind = parseVarKind();
				t.tableVars.push_back(Var{ varName, varKind });
			}
			expectToken(TOKEN_LPARENT);
			expectToken(TOKEN_SEMICOLON);

			tables.push_back(t);

			// Current table to commit
			table = t;

			return COMMAND_CREATE;
		}
		else
		{
			fatal("wrong stmt");
		}
	}
	else if (matchKeyword("insert"))
	{
		if (matchKeyword("into"))
		{
			Insert i;
			i.tableName = parseName();
			
			if (matchKeyword("values"))
			{
				expectToken(TOKEN_RPARENT);
				
				Expr *value = parseValue();
				i.values.push_back(value);
				while (matchToken(TOKEN_COMMA))
				{
					value = parseValue();
					i.values.push_back(value);
				}
				expectToken(TOKEN_LPARENT);
				expectToken(TOKEN_SEMICOLON);

				insertions.push_back(i);

				// Current insertion to commit
				insertion = i;

				return COMMAND_INSERT;
			}
		}
		else
		{
			fatal("wrong stmt");
		}
	}
	else if (matchKeyword("select"))
	{
		if (matchToken(TOKEN_STAR))
		{
			if (matchKeyword("from"))
			{
				Read r = {};
				r.tableName = parseName();

				expectToken(TOKEN_SEMICOLON);

				reads.push_back(r);

				return COMMAND_SELECT;
			}
			else
			{
				fatal("wrong stmt");
			}
		}
		else
		{
			fatal("wrong stmt");
		}
	}
	else
	{
		fatal("wrong stmt");
	}
}

// Resolver
void resolveTable()
{
	// Check for table duplications
	if (tables.size() != 0)
	{
		for (int i = 0; i < tables.size() - 1; i++) // TODO: optimize mb
		{
			for (int j = i + 1; j < tables.size(); j++)
			{
				if (strcmp(tables[i].tableName, tables[j].tableName) == 0) // TODO: make it fast and not so ugly
				{
					fatal("duplicate tables '%s'", tables[i].tableName);
				}
			}
		}
	}
}

void resolveInsert()
{
	// Check for not existing tables in insert // TODO: optimize mb
	bool isDeclared = false;
	for (int i = 0; i < insertions.size(); i++)
	{
		for (int j = 0; j < tables.size(); j++)
		{
			if (strcmp(insertions[i].tableName, tables[j].tableName) == 0)
			{
				isDeclared = true;

				// Check for number of insertions
				Table t = tables[j];
				if (t.tableVars.size() != insertions[i].values.size())
				{
					fatal("number of insertions != table vars");
				}

				break;
			}
		}

		if (!isDeclared)
		{
			fatal("insertion into undeclared table");
		}
		isDeclared = false;
	}
}

void resolveRead()
{
	// Check if tableName exists in read requests
	bool isDeclared = false;
	for (int i = 0; i < reads.size(); i++)
	{
		for (int j = 0; j < tables.size(); j++)
		{
			if (strcmp(tables[j].tableName, reads[i].tableName) == 0)
			{
				isDeclared = true;
				break;
			}
		}
		if (!isDeclared)
		{
			fatal("read request from undeclared table"); // TODO: from or to?
		}
	}
}

void resolve(CommandType type)
{
	switch (type)
	{
		case COMMAND_CREATE:
		{
			resolveTable();
		}
		break;
		case COMMAND_INSERT:
		{
			resolveInsert();
		}
		case COMMAND_SELECT:
		{
			resolveRead();
		}
		break;
		default:
			fatal("wrong command to resolve");
			break;
	}
}

// Type check
void typeCheckInsert()
{
	// Check type that u insert with type that table u insert to have
	for (int i = 0; i < insertions.size(); i++)
	{
		// Search for table
		Table t;
		for (int j = 0; j < tables.size(); j++)
		{
			if (strcmp(insertions[i].tableName, tables[j].tableName) == 0)
			{
				t = tables[j];
				break;
			}
		}

		for (int j = 0; j < insertions[i].values.size(); j++)
		{
			Type insertionType = insertions[i].values[j]->kind;
			Type tableType = t.tableVars[j].varType;

			if (insertionType != tableType)
			{
				fatal("insert incompatible type");
			}
		}
	}
}

void typeCheck(CommandType type)
{
	switch (type)
	{
		case COMMAND_CREATE:
		{
			// ...
		}
		break;
		case COMMAND_INSERT:
		{
			typeCheckInsert();
		}
		break;
		case COMMAND_SELECT:
		{
			// ...
		}
		break;
		default:
			fatal("wrong command for typecheck");
			break;
	}
}

// Emit to file
enum
{
	MAX_ROW_SIZE = 4096
};

uint8_t code[MAX_ROW_SIZE];
uint8_t *emit_ptr = code;

void cleanCode()
{
	memset(code, 0, emit_ptr - code);
	emit_ptr = code;
}

void emit(uint8_t byte)
{
	*emit_ptr = byte;
	emit_ptr++;
}

void emit16(uint16_t byte2)
{
	emit(byte2 & 0xFF);
	emit((byte2 >> 8) & 0xFF);
}

void emit32(uint32_t byte4)
{
	emit(byte4 & 0xFF);
	emit((byte4 >> 8) & 0xFF);
	emit((byte4 >> 16) & 0xFF);
	emit((byte4 >> 24) & 0xFF);
}

void emit64(uint64_t byte8)
{
	emit32(byte8 & 0xFFFFFFFF);
	emit32((byte8 >> 32) & 0xFFFFFFFF);
}

void emitString(char *string, size_t length)
{
	for (int i = 0; i < length; i++)
	{
		emit(string[i]);
	}
}

void emitValue(Expr *expr)
{
	switch (expr->kind)
	{
		case TYPE_INT:
		{
			emit32(expr->value);
		}
		break;
		case TYPE_STRING:
		{
			emitString(expr->string, strlen(expr->string) + 1); // '\0'
		}
		break;
		default:
			fatal("none or wrong type");
			break;
	}
}

char *typeToString(Type type)
{
	switch (type)
	{
		case TYPE_INT:
		{
			return (char *)"int";
		}
		break;
		case TYPE_STRING:
		{
			return (char *)"string";
		}
		break;
		default:
			fatal("none or wrong type");
			break;
	}
}

Type stringToType(char *type)
{
	if (strcmp(type, "int") == 0)
	{
		return TYPE_INT;
	}
	else if (strcmp(type, "string") == 0)
	{
		return TYPE_STRING;
	}
	else
		return TYPE_NONE;
}

void execRead()
{
	for (int i = 0; i < reads.size(); i++)
	{
		readData(reads[i].tableName);
	}

	reads.clear();
}

void execInsertTable()
{
	// Table section
	emit16(BEGIN_TABLE_DESC);
	emitString(table.tableName, strlen(table.tableName) + 1); // '\0'
	for (int i = 0; i < table.tableVars.size(); i++)
	{
		char *varName = table.tableVars[i].varName;
		emitString(varName, strlen(varName) + 1); // '\0'

		Type varType = table.tableVars[i].varType;
		char *varTypeString = typeToString(varType);
		emitString(varTypeString, strlen(varTypeString) + 1); // '\0'
	}
	emit16(END_TABLE_DESC);

	// Write to file
	FILE *f = fopen("dataBase.tf", "a+");
	if (!f)
	{
		fatal("can't open database file");
	}
	fwrite(code, emit_ptr - code, 1, f);
	fclose(f);

	// Clean code
	cleanCode();
}

void execInsertData()
{
	// Data section
	emitString(insertion.tableName, strlen(insertion.tableName) + 1); // '\0'

	emit16(BEGIN_TABLE_DATA);
	for (int i = 0; i < insertion.values.size(); i++)
	{
		Type varType = insertion.values[i]->kind;
		char *varTypeString = typeToString(varType);
		emitString(varTypeString, strlen(varTypeString) + 1); // '\0'

		Expr *expr = insertion.values[i];
		emitValue(expr);
	}
	emit16(END_TABLE_DATA);

	// Write to file
	FILE *f = fopen("dataBase.df", "a+");
	if (!f)
	{
		fatal("can't open database file");
	}
	fwrite(code, emit_ptr - code, 1, f);
	fclose(f);

	// Clean code
	cleanCode();
}

void exec(CommandType type)
{
	switch (type)
	{
		case COMMAND_INSERT:
		{
			execInsertData();
		}
		break;
		case COMMAND_CREATE:
		{
			execInsertTable();
		}
		break;
		case COMMAND_SELECT:
		{
			execRead();
		}
		break;
		default:
			fatal("wrong command to execute");
			break;
	}
}

// Console to type code
char *readConsole()
{
	char *command = (char *)malloc(256);
	memset(command, 0, 256);

	int length = 0;
	for (; ;)
	{
		int c = fgetc(stdin);
		if (c == '\n')
			break;
		
		*command++ = c;
		length++;

		if (length > 256)
		{
			fatal("num char to print limit reached");
		}
	}
	*command = '\0';
	command -= length;

	return command;
}

void loadTableInfo()
{
	FILE *f = fopen("dataBase.tf", "rb");
	if (!f)
	{
		printf("Data base file not created or cant be opened\n");
		return;
	}

	while (matchByte2(BEGIN_TABLE_DESC, &f))
	{
		Table t = {};
		t.tableName = getStringFromFile(&f);

		while (!isByte2(END_TABLE_DESC, &f))
		{
			char *varName = getStringFromFile(&f);
			char *varTypeString = getStringFromFile(&f);
			Type varType = stringToType(varTypeString);
			t.tableVars.push_back(Var{ varName, varType });
		}

		tables.push_back(t);
	}

	fclose(f);
}

void execCode(char *code)
{
	initStream(code);

	CommandType type = parse();
	resolve(type);
	typeCheck(type);
	exec(type);
}

int main()
{
	initKeywords();

	loadTableInfo();
	for (; ;)
	{
		char *code = readConsole();

		if (strcmp(code, "exit") == 0)
		{
			return 0;
		}
		
		execCode(code);
	}
}