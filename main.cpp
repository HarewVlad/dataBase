#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <cstdlib>
#include <vector>
#include <map>
#include <cstdarg>

// Main TODO: offset to data 4096
// Main TODO: Should i all the time open the dbf? (Caching)
// Main TODO: replace 'string' and 'int' to some int value
// Main TODO: read code or type it in console ? (for all stages use std::vector<Read>, std::vector<Insert> ...)

// Block allocator
#define MIN_BLOCK_SIZE 1024
#define MEMORY_ALIGN 8
#define MAX(a, b) (a) > (b) ? (a) : (b)
#define ALIGN(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define ALIGN_PTR(a, b) (char *)(((uintptr_t)(a) + (b) - 1) & ~((b) - 1))

struct Memory
{
	void *begin;
	void *end;
};

Memory memory = {};
std::vector<Memory> memoryBlocks;

void *allocMemory(size_t size)
{
	if (size > (size_t)((size_t)memory.end - (size_t)memory.begin))
	{
		// Grow
		size_t newSize = ALIGN(MAX(size, MIN_BLOCK_SIZE), MEMORY_ALIGN);
		memory.begin = malloc(newSize);
		memory.end = (void *)((size_t)memory.begin + newSize);

		memoryBlocks.push_back(memory);
	}
	void *ptr = memory.begin;
	memory.begin = ALIGN_PTR((void *)((size_t)memory.begin + size), MEMORY_ALIGN);

	return ptr;
}

/*
void freeMemory()
{
	for (auto b : memoryBlocks)
	{
		free(b);
	}
}
*/

// Utils
char *copyString(const char *src, size_t len)
{
	char *result = (char *)allocMemory(len); // NOTE: custom allocation
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
	putKeyword("delete", strlen("delete"));
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
	Expr *e = (Expr *)allocMemory(sizeof(Expr)); // NOTE: custom allocation
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
	int numRows;
	std::vector<Expr *> values;
};

struct Read
{
	char *tableName;
	// TODO: add more info later
};

struct Delete
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
std::vector<Delete> deletions;
std::vector<Table> tables;

Insert insertion;
Table table;
Read read;
Delete del;

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

char *readStringFromFile(FILE **f)
{
	char *string = (char *)allocMemory(256); // NOTE: custom allocation
	int length = 0;
	int c = 0;
	while ((c = fgetc(*f)) != '\0')
	{
		if (feof(*f)) // TODO: mb do it another way
		{
			return nullptr;
		}

		*string++ = c;
		length++;
	}
	*string = c;
	string -= length;

	return string;
}

uint8_t read8(FILE **f) // TODO: free memory
{
	char *byteChar = (char *)allocMemory(2); // NOTE: custom allocation
	memset(byteChar, 0, 2);
	fread(byteChar, 1, 1, *f);

	uint8_t byte = (uint8_t)byteChar[0];

	return byte;
}

uint16_t read16(FILE **f)
{
	uint8_t second = read8(f);
	uint8_t first = read8(f);

	uint16_t byte2 = (first << 8) | second;

	return byte2;
}

uint32_t read32(FILE **f)
{
	uint16_t second = read16(f);
	uint16_t first = read16(f);

	uint32_t byte4 = (first << 16) | second;

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

bool expectByte2(uint16_t byte2, FILE **f)
{
	if (read16(f) == byte2)
	{
		return true;
	}
	else
	{
		fatal("error in dbf");
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

bool isNextByteEOF(FILE **f)
{
	fgetc(*f);
	if (!feof(*f))
	{
		fseek(*f, -1, SEEK_CUR);
		return false;
	}
	else
	{
		return true;
	}
}

bool isTableFound(FILE **f, char *tableName) // TODO: seems like fseek to eof is not finding eof until reads byte
{
	int next = 0;
	while (!isNextByteEOF(f))
	{
		char *tableNameFromFile = readStringFromFile(f);
		if (strcmp(tableNameFromFile, tableName) != 0)
		{
			next = read32(f) + 2 + 2; // BEGIN_TABLE_DATA, END_TABLE_DATA
		}
		else
		{
			return true;
		}

		fseek(*f, next, SEEK_CUR);
	}
	return false;
}

// TODO: change search
void readData(char *tableName)
{
	FILE *f = fopen("dataBase.df", "rb");
	if (!f)
	{
		printf("Data base file not created or cant be opened\n");
		return;
	}

	if (isTableFound(&f, tableName))
	{
		int size = read32(&f);

		printf("Table name is -> '%s'\n", tableName);
		printf("Table size -> '%d' bytes\n", size);

		if (matchByte2(BEGIN_TABLE_DATA, &f))
		{
			while (!isByte2(END_TABLE_DATA, &f))
			{
				char *varType = readStringFromFile(&f);
				if (strcmp(varType, "int") == 0)
				{
					int varValue = read32(&f);
					printf("Value is int -> '%d'\n", varValue);
				}
				else if (strcmp(varType, "string") == 0)
				{
					char *varValue = readStringFromFile(&f);
					printf("Value is string -> '%s'\n", varValue);
				}
			}
		}
	}
	else
	{
		printf("Table is empty\n");
		return;
	}

	fclose(f);
}

// TODO: bug with copy size
void deleteData(char *tableName)
{
	FILE *fDb = fopen("dataBase.df", "rb+");
	if (!fDb)
	{
		printf("Data base file not created or cant be opened\n");
		return;
	}

	FILE *fTemp = fopen("tempFile.temp", "wb+");
	if (!fTemp)
	{
		printf("Unable to delete data\n");
		return;
	}

	if (isTableFound(&fDb, tableName))
	{
		int firstPartSize = ftell(fDb) - strlen(tableName) - 1; // '\0'
		int tableSize = read32(&fDb);
		int end = firstPartSize + tableSize + sizeof(int) + 2 * sizeof(short) + 2;

		// First part
		if (firstPartSize != 0)
		{
			char *firstPart = (char *)malloc(firstPartSize + 1); // '\0'
			memset(firstPart, 0, firstPartSize);
			fseek(fDb, 0, SEEK_SET);
			fread(firstPart, 1, firstPartSize, fDb);

			fwrite(firstPart, 1, firstPartSize, fTemp);
		}

		// Second part
		fseek(fDb, 0, SEEK_END);
		int endOfFile = ftell(fDb);

		int secondPartSize = endOfFile - end - 1;
		if (secondPartSize != 0)
		{
			char *secondPart = (char *)malloc(secondPartSize + 1); // '\0'
			memset(secondPart, 0, secondPartSize);
			fseek(fDb, end + 1, SEEK_SET); // end + 1 -> Next after end of table section
			fread(secondPart, 1, secondPartSize, fDb);

			fwrite(secondPart, 1, secondPartSize, fTemp);
		}

		// Clear old file
		fDb = freopen("dataBase.df", "wb", fDb);
		if (!fDb)
		{
			fatal("Error while deleting info from data base");
			return;
		}

		// Copy content of temp file
		fseek(fTemp, 0, SEEK_SET);
		int ch = 0;
		while ((ch = fgetc(fTemp)) != EOF)
		{
			fputc(ch, fDb);
		}
	}
	else
	{
		fatal("table do not exist");
	}

	fclose(fTemp);
	fclose(fDb);

	// Delete temp file
	remove("tempFile.temp");
}

enum CommandType
{
	COMMAND_CREATE,
	COMMAND_INSERT,
	COMMAND_SELECT,
	COMMAND_DELETE
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
	else if (matchKeyword("insert") && matchKeyword("into"))
	{
		Insert i;
		i.tableName = parseName();
			
		if (matchKeyword("values"))
		{
			int numRows = 0;

			expectToken(TOKEN_RPARENT);
			Expr *value = parseValue();
			i.values.push_back(value);
			while (matchToken(TOKEN_COMMA))
			{
				value = parseValue();
				i.values.push_back(value);
			}
			expectToken(TOKEN_LPARENT);

			numRows++;

			while (matchToken(TOKEN_COMMA))
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

				numRows++;
			}
			expectToken(TOKEN_SEMICOLON);

			i.numRows = numRows;

			// For executing code from file
			insertions.push_back(i);

			// Current insertion to commit
			insertion = i;

			return COMMAND_INSERT;
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

				// For executing code from file
				reads.push_back(r);

				// Current read request
				read = r;

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
	else if (matchKeyword("delete") && matchKeyword("from"))
	{
		Delete d = {};
		d.tableName = parseName();

		expectToken(TOKEN_SEMICOLON);

		deletions.push_back(d);

		// For executing code from file
		deletions.push_back(d);

		// Current delete request
		del = d;

		return COMMAND_DELETE;
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
	for (int i = 0; i < tables.size(); i++)
	{
		if (strcmp(insertion.tableName, tables[i].tableName) == 0)
		{
			isDeclared = true;

			// Check for number of insertions
			Table t = tables[i];
			if (t.tableVars.size() != insertion.values.size() / insertion.numRows)
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
}

void resolveRead()
{
	// Check if tableName exists in read requests
	bool isDeclared = false;
	for (int i = 0; i < tables.size(); i++)
	{
		if (strcmp(read.tableName, tables[i].tableName) == 0)
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

void resolveDelete()
{
	// Check if tableName exists in delete requests
	bool isDeclared = false;
	for (int i = 0; i < tables.size(); i++)
	{
		if (strcmp(del.tableName, tables[i].tableName) == 0)
		{
			isDeclared = true;
			break;
		}
	}
	if (!isDeclared)
	{
		fatal("delete request from undeclared table");
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
		break;
		case COMMAND_SELECT:
		{
			resolveRead();
		}
		break;
		case COMMAND_DELETE:
		{
			resolveDelete();
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
	Table t;
	for (int i = 0; i < tables.size(); i++)
	{
		if (strcmp(insertion.tableName, tables[i].tableName) == 0)
		{
			t = tables[i];
			break;
		}
	}

	for (int i = 0; i < t.tableVars.size(); i++)
	{
		for (int j = 0; j < insertion.values.size(); j++)
		{
			Type insertionType = insertion.values[j]->kind;
			Type tableType = t.tableVars[i].varType;

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
		case COMMAND_DELETE:
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
	readData(read.tableName);
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
	FILE *f = fopen("dataBase.tf", "ab+");
	if (!f)
	{
		fatal("can't open database file");
	}
	fwrite(code, emit_ptr - code, 1, f);
	fclose(f);

	// Clean code
	cleanCode();
}

size_t getExprSize(Expr *expr)
{
	size_t size = -1;
	switch (expr->kind)
	{
		case TYPE_INT:
		{
			size = sizeof(int);
		}
		break;
		case TYPE_STRING:
		{
			size = strlen(expr->string) + 1; // '\0'
		}
		break;
		default:
			fatal("wrong expr type");
			break;
	}

	return size;
}

size_t getInsertDataSize()
{
	size_t totalSize = 0;
	for (int i = 0; i < insertion.values.size(); i++)
	{
		Type varType = insertion.values[i]->kind;
		char *varTypeString = typeToString(varType);
		
		Expr *expr = insertion.values[i];

		totalSize += strlen(varTypeString) + 1; // '\0'
		totalSize += getExprSize(expr);
	}
	return totalSize;
}

void execInsertData()
{
	// Data section
	emitString(insertion.tableName, strlen(insertion.tableName) + 1); // '\0'
	emit32(getInsertDataSize());

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
	FILE *f = fopen("dataBase.df", "ab+");
	if (!f)
	{
		fatal("can't open database file");
	}
	fwrite(code, emit_ptr - code, 1, f);
	fclose(f);

	// Clean code
	cleanCode();
}

void execDelete()
{
	deleteData(del.tableName);
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
		case COMMAND_DELETE:
		{
			execDelete();
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
	char *command = (char *)allocMemory(256); // NOTE: custom allocation
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
		t.tableName = readStringFromFile(&f);

		while (!isByte2(END_TABLE_DESC, &f))
		{
			char *varName = readStringFromFile(&f);
			char *varTypeString = readStringFromFile(&f);
			Type varType = stringToType(varTypeString);
			t.tableVars.push_back(Var{ varName, varType });
		}

		tables.push_back(t);

		expectByte2(END_TABLE_DESC, &f);
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