#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <utility>
#include <vector>

namespace pcn {

// --- Frontend: lexing -------------------------------------------------------
struct SourceLocation {
	std::size_t line = 1;
	std::size_t column = 1;
};

struct SourceSpan {
	SourceLocation start;
	SourceLocation end;
};

enum class TokenKind {
	Identifier,
	Number,
	String,
	Keyword,
	Symbol,
	Newline,
	Indent,
	Dedent,
	EndOfFile
};

struct Token {
	TokenKind kind = TokenKind::EndOfFile;
	std::string text;
	SourceLocation location;
};

static const std::unordered_set<std::string> kKeywords = {
	"routine", "let", "var", "if", "else", "while", "each", "in",
	"give", "fail", "try", "choose", "when", "otherwise", "case",
	"record", "enum", "variant", "union", "module", "use", "public",
	"compile", "foreign", "unsafe", "defer", "const", "where", "as",
	"layout", "c", "packed", "align", "true", "false", "null",
	"gives", "and", "or", "not", "return"
};

static bool isIdentifierStart(char ch) {
	return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

static bool isIdentifierPart(char ch) {
	return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

class Lexer {
public:
	explicit Lexer(std::string source)
		: source_(std::move(source)) {}

	std::vector<Token> tokenize() {
		std::vector<Token> tokens;
		bool atLineStart = true;
		bool emittedAnyToken = false;
		while (!isAtEnd()) {
			SourceLocation loc{line_, column_};
			const char ch = peek();

			if (atLineStart) {
				if (ch == '\n' || ch == '\r') {
					advanceLineBreak(tokens);
					continue;
				}

				std::size_t indent = 0;
				std::size_t startIndex = index_;
				std::size_t startColumn = column_;
				while (!isAtEnd()) {
					char space = peek();
					if (space == ' ') {
						advance();
						++indent;
						continue;
					}
					if (space == '\t') {
						advance();
						indent += kTabWidth;
						continue;
					}
					break;
				}

				if (peek() == '\n' || peek() == '\r') {
					advanceLineBreak(tokens);
					continue;
				}

				if (!emittedAnyToken) {
					indentStack_.push_back(indent);
				} else {
					std::size_t currentIndent = indentStack_.back();
					if (indent > currentIndent) {
						indentStack_.push_back(indent);
						tokens.push_back({TokenKind::Indent, "<indent>", {line_, startColumn}});
					} else if (indent < currentIndent) {
						while (indentStack_.size() > 1 && indent < indentStack_.back()) {
							indentStack_.pop_back();
							tokens.push_back({TokenKind::Dedent, "<dedent>", {line_, startColumn}});
						}
						if (indent != indentStack_.back()) {
							throw std::runtime_error("inconsistent indentation at line " + std::to_string(line_));
						}
					}
				}

				atLineStart = false;
			}

			if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
				if (ch == '\n') {
					advanceLineBreak(tokens);
					atLineStart = true;
					continue;
				}
				if (ch == '\r') {
					advanceLineBreak(tokens);
					atLineStart = true;
					continue;
				}
				advance();
				++column_;
				continue;
			}

			if (ch == '#') {
				while (!isAtEnd() && peek() != '\n') {
					advance();
				}
				continue;
			}

			if (ch == '"') {
				advance();
				std::string value;
				while (!isAtEnd() && peek() != '"') {
					char c = advance();
					if (c == '\\') {
						if (isAtEnd()) {
							throw std::runtime_error("unterminated string literal");
						}
						value.push_back(advance());
					} else {
						value.push_back(c);
					}
				}
				if (isAtEnd() || peek() != '"') {
					throw std::runtime_error("unterminated string literal");
				}
				advance();
				tokens.push_back({TokenKind::String, value, loc});
				emittedAnyToken = true;
				continue;
			}

			if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
				std::string number;
				while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
					number.push_back(advance());
				}
				if (!isAtEnd() && peek() == '.') {
					number.push_back(advance());
					while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
						number.push_back(advance());
					}
				}
				tokens.push_back({TokenKind::Number, number, loc});
				emittedAnyToken = true;
				continue;
			}

			if (isIdentifierStart(ch)) {
				std::string ident;
				while (!isAtEnd() && isIdentifierPart(peek())) {
					ident.push_back(advance());
				}
				if (kKeywords.count(ident) != 0) {
					tokens.push_back({TokenKind::Keyword, ident, loc});
				} else {
					tokens.push_back({TokenKind::Identifier, ident, loc});
				}
				emittedAnyToken = true;
				continue;
			}

			if (isSingleCharSymbol(ch)) {
				std::string symbol(1, ch);
				SourceLocation symbolLoc = loc;
				advance();

				if (isMultiCharOperator(ch, peek())) {
					symbol.push_back(advance());
					++column_;
				}

				tokens.push_back({TokenKind::Symbol, symbol, symbolLoc});
				emittedAnyToken = true;
				continue;
			}

			throw std::runtime_error("unexpected character '" + std::string(1, ch) + "' at line " + std::to_string(loc.line) + ", column " + std::to_string(loc.column));
		}

		while (indentStack_.size() > 1) {
			indentStack_.pop_back();
			tokens.push_back({TokenKind::Dedent, "<dedent>", {line_, column_}});
		}
		tokens.push_back({TokenKind::EndOfFile, "", {line_, column_}});
		return tokens;
	}

private:
	bool isAtEnd() const { return index_ >= source_.size(); }
	char peek() const { return isAtEnd() ? '\0' : source_[index_]; }
	char peekNext() const {
		return index_ + 1 < source_.size() ? source_[index_ + 1] : '\0';
	}
	char advance() {
		char value = source_[index_++];
		++column_;
		return value;
	}
	void advanceLineBreak(std::vector<Token>& tokens) {
		SourceLocation loc{line_, column_};
		if (peek() == '\r') {
			advance();
			if (peek() == '\n') {
				advance();
			}
		} else if (peek() == '\n') {
			advance();
		}
		tokens.push_back({TokenKind::Newline, "\n", loc});
		++line_;
		column_ = 1;
	}

	static bool isSingleCharSymbol(char ch) {
		static const std::unordered_set<char> symbols = {'{', '}', '(', ')', '[', ']', ',', ':', ';', '.', '+', '-', '*', '/', '%', '=', '<', '>', '!', '&', '|', '^'};
		return symbols.count(ch) != 0;
	}

	static bool isMultiCharOperator(char ch, char next) {
		if (ch == '=' && next == '=') return true;
		if (ch == '!' && next == '=') return true;
		if (ch == '<' && next == '=') return true;
		if (ch == '>' && next == '=') return true;
		if (ch == '&' && next == '&') return true;
		if (ch == '|' && next == '|') return true;
		if (ch == '+' && next == '=') return true;
		if (ch == '-' && next == '=') return true;
		if (ch == '*' && next == '=') return true;
		if (ch == '/' && next == '=') return true;
		if (ch == '%' && next == '=') return true;
		if (ch == '-' && next == '>') return true;
		if (ch == ':' && next == ':') return true;
		if (ch == '<' && next == '<') return true;
		if (ch == '>' && next == '>') return true;
		return false;
	}

	std::string source_;
	std::size_t index_ = 0;
	std::size_t line_ = 1;
	std::size_t column_ = 1;
	std::vector<std::size_t> indentStack_ {0};
	static constexpr std::size_t kTabWidth = 4;
};

struct Parameter {
	std::string name;
	std::string typeName;
	SourceLocation location;
};

struct Expr;
struct Stmt;

using ExprPtr = std::shared_ptr<const Expr>;
using StmtPtr = std::shared_ptr<const Stmt>;

struct NumberExpr {
	std::string value;
	SourceSpan span;
	SourceLocation location;
};


struct StringExpr {
	std::string value;
	SourceSpan span;
	SourceLocation location;
};

struct BoolExpr {
	bool value = false;
	SourceSpan span;
	SourceLocation location;
};

struct NullExpr {
	SourceSpan span;
	SourceLocation location;
};

struct VariableExpr {
	std::string name;
	SourceSpan span;
	SourceLocation location;
};

struct UnaryExpr {
	std::string op;
	ExprPtr operand;
	SourceSpan span;
	SourceLocation location;
};

struct BinaryExpr {
	std::string op;
	ExprPtr left;
	ExprPtr right;
	SourceSpan span;
	SourceLocation location;
};

struct PipelineExpr {
	ExprPtr input;
	std::vector<ExprPtr> stages;
	SourceSpan span;
	SourceLocation location;
};

struct RangeExpr {
	ExprPtr start;
	ExprPtr end;
	ExprPtr step;
	bool inclusive = false;
	SourceSpan span;
	SourceLocation location;
};

struct ChooseCase {
	ExprPtr condition;
	ExprPtr value;
	std::string variantName;
	std::vector<std::string> bindings;
	SourceSpan span;
	SourceLocation location;
};

struct ChooseExpr {
	ExprPtr subject;
	std::vector<ChooseCase> cases;
	ExprPtr otherwiseValue;
	bool hasOtherwise = false;
	SourceSpan span;
	SourceLocation location;
};

struct CallExpr {
	ExprPtr callee;
	std::vector<ExprPtr> arguments;
	SourceSpan span;
	SourceLocation location;
};

struct GroupExpr {
	ExprPtr expression;
	SourceSpan span;
	SourceLocation location;
};

struct Expr {
	using Node = std::variant<NumberExpr, StringExpr, BoolExpr, NullExpr, VariableExpr, UnaryExpr, BinaryExpr, PipelineExpr, RangeExpr, ChooseExpr, CallExpr, GroupExpr>;
	Node node;
	SourceSpan span;
	SourceLocation location;
};

struct LetStmt {
	std::string name;
	std::string typeName;
	ExprPtr initializer;
	SourceSpan span;
	SourceLocation location;
};

struct VarStmt {
	std::string name;
	std::string typeName;
	ExprPtr initializer;
	SourceSpan span;
	SourceLocation location;
};

struct AssignStmt {
	std::string name;
	ExprPtr value;
	SourceSpan span;
	SourceLocation location;
};

struct ExprStmt {
	ExprPtr expression;
	SourceSpan span;
	SourceLocation location;
};

struct ReturnStmt {
	ExprPtr value;
	bool hasValue = false;
	SourceSpan span;
	SourceLocation location;
};

struct GiveStmt {
	ExprPtr value;
	SourceSpan span;
	SourceLocation location;
};

struct IfStmt {
	ExprPtr condition;
	std::vector<StmtPtr> thenBranch;
	std::vector<StmtPtr> elseBranch;
	bool hasElse = false;
	SourceSpan span;
	SourceLocation location;
};

struct EachStmt {
	std::vector<std::string> bindings;
	ExprPtr range;
	std::vector<StmtPtr> body;
	SourceSpan span;
	SourceLocation location;
};

struct UnsafeStmt {
	std::vector<StmtPtr> body;
	SourceSpan span;
	SourceLocation location;
};

struct DeferStmt {
	ExprPtr expression;
	SourceSpan span;
	SourceLocation location;
};

struct FailStmt {
	ExprPtr expression;
	SourceSpan span;
	SourceLocation location;
};

struct WhileStmt {
	ExprPtr condition;
	std::vector<StmtPtr> body;
	SourceSpan span;
	SourceLocation location;
};

struct BlockStmt {
	std::vector<StmtPtr> statements;
	SourceSpan span;
	SourceLocation location;
};

struct Stmt {
	using Node = std::variant<LetStmt, VarStmt, AssignStmt, ExprStmt, ReturnStmt, GiveStmt, IfStmt, EachStmt, UnsafeStmt, DeferStmt, FailStmt, WhileStmt, BlockStmt>;
	Node node;
	SourceSpan span;
	SourceLocation location;
};

struct AstFactory {
	static SourceSpan span(SourceLocation start, SourceLocation end) {
		return {start, end};
	}

	static ExprPtr makeExpr(Expr node, SourceSpan nodeSpan) {
		return std::make_shared<const Expr>(std::move(node));
	}

	static StmtPtr makeStmt(Stmt node, SourceSpan nodeSpan) {
		return std::make_shared<const Stmt>(std::move(node));
	}

	template <typename T>
	static ExprPtr makeExpr(T node, SourceSpan nodeSpan) {
		return std::make_shared<const Expr>(Expr{Expr::Node{std::move(node)}, nodeSpan, nodeSpan.start});
	}

	template <typename T>
	static StmtPtr makeStmt(T node, SourceSpan nodeSpan) {
		return std::make_shared<const Stmt>(Stmt{Stmt::Node{std::move(node)}, nodeSpan, nodeSpan.start});
	}
};

struct NominalTypeInfo {
	std::string name;
	std::string kind;
	std::vector<std::pair<std::string, std::string>> fields;
	std::vector<std::pair<std::string, std::string>> members;
	std::vector<std::string> tags;
	bool isPublic = false;
	std::string layout;
};

struct Routine {
	std::string name;
	bool isPublic = false;
	bool isCompileTime = false;
	bool isForeign = false;
	std::string foreignAbi;
	std::vector<Parameter> parameters;
	std::string returnType;
	std::vector<StmtPtr> body;
	SourceSpan span;
	SourceLocation location;
};

struct FieldDecl {
	std::string name;
	std::string typeName;
	SourceLocation location;
};

struct RecordDecl {
	std::string name;
	bool isPublic = false;
	bool isCompileTime = false;
	std::string layout;
	std::vector<FieldDecl> fields;
	SourceSpan span;
	SourceLocation location;
};

struct EnumDecl {
	std::string name;
	bool isPublic = false;
	std::string underlyingType;
	std::vector<std::pair<std::string, std::optional<std::string>>> members;
	SourceSpan span;
	SourceLocation location;
};

struct UnionDecl {
	std::string name;
	bool isPublic = false;
	std::string layout;
	std::vector<FieldDecl> fields;
	SourceSpan span;
	SourceLocation location;
};

struct VariantCase {
	std::string name;
	std::vector<Parameter> fields;
	SourceLocation location;
};

struct VariantDecl {
	std::string name;
	bool isPublic = false;
	std::vector<VariantCase> cases;
	SourceSpan span;
	SourceLocation location;
};

struct ModuleDecl {
	std::string name;
	bool isPublic = false;
	std::vector<struct Declaration> declarations;
	SourceSpan span;
	SourceLocation location;
};

struct UseDecl {
	std::vector<std::string> path;
	std::string alias;
	SourceSpan span;
	SourceLocation location;
};

struct ConstDecl {
	std::string name;
	bool isPublic = false;
	bool isCompileTime = false;
	std::string typeName;
	ExprPtr value;
	SourceSpan span;
	SourceLocation location;
};

struct ForeignDecl {
	std::string abi;
	Routine routine;
	SourceSpan span;
	SourceLocation location;
};

struct Declaration {
	using Node = std::variant<Routine, RecordDecl, EnumDecl, UnionDecl, VariantDecl, ModuleDecl, UseDecl, ConstDecl, ForeignDecl>;
	Node node;
	SourceSpan span;
	SourceLocation location;
};

struct Program {
	std::vector<Routine> routines;
	std::vector<Declaration> declarations;
};

enum class SemanticTypeKind {
	Unknown,
	Void,
	Bool,
	Number,
	String,
	Null,
	Custom
};

struct SemanticType {
	SemanticTypeKind kind = SemanticTypeKind::Unknown;
	std::string name;

	bool operator==(const SemanticType& other) const {
		return kind == other.kind && name == other.name;
	}

	bool operator!=(const SemanticType& other) const {
		return !(*this == other);
	}
};

struct SemanticDiagnostic {
	SourceLocation location;
	std::string message;
};

struct SymbolInfo {
	SemanticType type;
	bool isMutable = false;
	bool isParameter = false;
};

struct DeclarationInfo {
	std::string name;
	std::string kind;
	bool isPublic = false;
	bool isCompileTime = false;
	std::string metadata;
};

struct RoutineSignature {
	std::string name;
	SemanticType returnType;
	std::vector<SemanticType> parameterTypes;
	std::vector<std::string> parameterNames;
};

struct SemanticScope {
	std::unordered_map<std::string, SymbolInfo> symbols;
};

struct SemanticAnalysisResult {
	std::vector<SemanticDiagnostic> diagnostics;
	std::vector<DeclarationInfo> declarations;
	std::unordered_map<std::string, NominalTypeInfo> nominalTypes;

	bool succeeded() const { return diagnostics.empty(); }
};

enum class VReg : std::uint32_t {
	None = 0
};

enum class BlockID : std::uint32_t {
	None = 0
};

enum class IRValueKind {
	Unknown,
	Void,
	Bool,
	Int64,
	String,
	Null,
	Function
};

struct IRValueType {
	IRValueKind kind = IRValueKind::Unknown;
	std::string name;
};

inline IRValueType makeIRUnknownType() {
	return {IRValueKind::Unknown, {}};
}

inline IRValueType makeIRVoidType() {
	return {IRValueKind::Void, "void"};
}

inline IRValueType makeIRBoolType() {
	return {IRValueKind::Bool, "bool"};
}

inline IRValueType makeIRIntType() {
	return {IRValueKind::Int64, "int64"};
}

inline IRValueType makeIRStringType() {
	return {IRValueKind::String, "string"};
}

inline IRValueType makeIRNullType() {
	return {IRValueKind::Null, "null"};
}

inline std::string irValueTypeName(const IRValueType& type) {
	if (!type.name.empty()) {
		return type.name;
	}
	switch (type.kind) {
	case IRValueKind::Unknown: return "unknown";
	case IRValueKind::Void: return "void";
	case IRValueKind::Bool: return "bool";
	case IRValueKind::Int64: return "int64";
	case IRValueKind::String: return "string";
	case IRValueKind::Null: return "null";
	case IRValueKind::Function: return "function";
	}
	return "unknown";
}

// --- IR types ---------------------------------------------------------------
enum class IROpcode {
	ConstInt,
	ConstString,
	ConstBool,
	ConstNull,
	Copy,
	Add,
	Sub,
	Mul,
	Div,
	Neg,
	Not,
	Eq,
	Ne,
	Lt,
	Le,
	Gt,
	Ge,
	And,
	Or,
	Phi,
	Branch,
	BranchIf,
	Call,
	Return,
	Nop
};

struct IRInstruction {
	IROpcode op = IROpcode::Nop;
	VReg dest = VReg::None;
	IRValueType valueType = makeIRUnknownType();
	std::vector<VReg> args;
	std::vector<BlockID> targetBlocks;
	std::vector<std::pair<BlockID, VReg>> phiInputs;
	std::vector<std::string> argumentTypes;
	std::vector<IRValueType> argumentValueTypes;
	std::string callee;
	std::string typeName;
	std::string literalText;
	std::string returnType;
	std::string callSignature;
	std::string blockLabel;
	std::int64_t immediate = 0;
	bool hasImmediate = false;
};

struct IRBasicBlock {
	BlockID id = BlockID::None;
	std::string labelName;
	IRValueType terminatorType = makeIRVoidType();
	std::vector<IRInstruction> instructions;
	std::vector<std::string> predecessors;
	std::vector<IRInstruction> phiNodes;
};

struct IRFunction {
	std::string name;
	std::string signature;
	std::vector<std::string> parameterSignature;
	std::string returnType;
	IRValueType valueType = makeIRVoidType();
	std::vector<std::string> parameterNames;
	std::vector<std::string> parameterTypes;
	std::vector<IRValueType> parameterValueTypes;
	std::vector<VReg> parameterVregs;
	std::vector<IRValueType> localValueTypes;
	std::vector<IRBasicBlock> blocks;
	std::uint32_t totalVregs = 0;
};

struct IRProgram {
	std::vector<IRFunction> functions;
	std::vector<DeclarationInfo> declarationSummaries;
};

inline SemanticType makeUnknownType() {
	return {SemanticTypeKind::Unknown, {}};
}

inline SemanticType makeVoidType() {
	return {SemanticTypeKind::Void, "void"};
}

inline SemanticType makeBoolType() {
	return {SemanticTypeKind::Bool, "bool"};
}

inline SemanticType makeNumberType() {
	return {SemanticTypeKind::Number, "number"};
}

inline SemanticType makeStringType() {
	return {SemanticTypeKind::String, "string"};
}

inline SemanticType makeNullType() {
	return {SemanticTypeKind::Null, "null"};
}

inline SemanticType makeCustomType(std::string name) {
	return {SemanticTypeKind::Custom, std::move(name)};
}

inline bool isBuiltinTypeName(const std::string& name) {
	return name == "void" || name == "bool" || name == "number" || name == "string" || name == "null";
}

inline SemanticType semanticTypeFromName(const std::string& name) {
	if (name == "void") return makeVoidType();
	if (name == "bool") return makeBoolType();
	if (name == "number") return makeNumberType();
	if (name == "string") return makeStringType();
	if (name == "null") return makeNullType();
	if (name.empty()) return makeUnknownType();
	return makeCustomType(name);
}

inline std::string semanticTypeName(const SemanticType& type) {
	if (!type.name.empty()) {
		return type.name;
	}
	switch (type.kind) {
	case SemanticTypeKind::Unknown: return "unknown";
	case SemanticTypeKind::Void: return "void";
	case SemanticTypeKind::Bool: return "bool";
	case SemanticTypeKind::Number: return "number";
	case SemanticTypeKind::String: return "string";
	case SemanticTypeKind::Null: return "null";
	case SemanticTypeKind::Custom: return "custom";
	}
	return "unknown";
}

inline bool isNumericType(const SemanticType& type) {
	return type.kind == SemanticTypeKind::Number;
}

inline bool isAssignableTo(const SemanticType& target, const SemanticType& value) {
	if (target.kind == SemanticTypeKind::Unknown || value.kind == SemanticTypeKind::Unknown) {
		return true;
	}
	if (target == value) {
		return true;
	}
	if (target.kind == SemanticTypeKind::Custom || value.kind == SemanticTypeKind::Custom) {
		return target.name == value.name;
	}
	if (target.kind == SemanticTypeKind::Void || value.kind == SemanticTypeKind::Void) {
		return target.kind == value.kind;
	}
	return false;
}

inline bool isTruthyCompatible(const SemanticType& type) {
	return type.kind == SemanticTypeKind::Bool || type.kind == SemanticTypeKind::Number || type.kind == SemanticTypeKind::String || type.kind == SemanticTypeKind::Null || type.kind == SemanticTypeKind::Custom || type.kind == SemanticTypeKind::Unknown;
}

class Parser {
public:
	explicit Parser(std::vector<Token> tokens)
		: tokens_(std::move(tokens)) {}

	Program parse() {
		Program program;
		skipNewlines();
		while (!isAtEnd()) {
			if (checkKeyword("public") || checkKeyword("compile") || checkKeyword("record") || checkKeyword("enum") || checkKeyword("variant") || checkKeyword("union") || checkKeyword("module") || checkKeyword("use") || checkKeyword("const") || checkKeyword("foreign")) {
				program.declarations.push_back(parseDeclaration());
				skipNewlines();
				continue;
			}
			if (checkKeyword("routine")) {
				program.routines.push_back(parseRoutine());
				skipNewlines();
				continue;
			}
			throw std::runtime_error("unexpected top-level token: " + peek().text);
		}
		return program;
	}

	Declaration parseDeclaration() {
		bool isPublic = matchKeyword("public");
		bool isCompile = matchKeyword("compile");
		if (matchKeyword("routine")) {
			return parseRoutineDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("foreign")) {
			return parseForeignDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("record")) {
			return parseRecordDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("enum")) {
			return parseEnumDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("variant")) {
			return parseVariantDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("union")) {
			return parseUnionDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("module")) {
			return parseModuleDeclaration(isPublic, isCompile);
		}
		if (matchKeyword("use")) {
			return parseUseDeclaration();
		}
		if (matchKeyword("const")) {
			return parseConstDeclaration(isPublic, isCompile);
		}
		throw std::runtime_error("expected top-level declaration");
	}

private:
	const Token& peek() const { return tokens_[index_]; }
	const Token& peek(std::size_t offset) const {
		std::size_t pos = index_ + offset;
		if (pos >= tokens_.size()) {
			return tokens_.back();
		}
		return tokens_[pos];
	}
	const Token& previous() const { return tokens_[index_ - 1]; }
	bool isAtEnd() const { return peek().kind == TokenKind::EndOfFile; }
	bool checkKeyword(const std::string& value) const { return peek().kind == TokenKind::Keyword && peek().text == value; }
	bool checkSymbol(const std::string& value) const { return peek().kind == TokenKind::Symbol && peek().text == value; }
	bool checkKeywordAhead(std::size_t offset, const std::string& value) const { return peek(offset).kind == TokenKind::Keyword && peek(offset).text == value; }
	bool checkSymbolAhead(std::size_t offset, const std::string& value) const { return peek(offset).kind == TokenKind::Symbol && peek(offset).text == value; }
	bool matchKeyword(const std::string& value) {
		if (checkKeyword(value)) {
			++index_;
			return true;
		}
		return false;
	}
	bool matchSymbol(const std::string& value) {
		if (checkSymbol(value)) {
			++index_;
			return true;
		}
		return false;
	}
	Token consume() {
		if (isAtEnd()) {
			throw std::runtime_error("unexpected end of file");
		}
		return tokens_[index_++];
	}
	Token expectKeyword(const std::string& value) {
		if (!matchKeyword(value)) {
			throw std::runtime_error("expected keyword '" + value + "'");
		}
		return previous();
	}
	Token expectIdentifier() {
		if (peek().kind != TokenKind::Identifier) {
			throw std::runtime_error("expected identifier");
		}
		return consume();
	}
	Token expectToken(TokenKind kind, const std::string& description) {
		if (peek().kind != kind) {
			throw std::runtime_error("expected " + description);
		}
		return consume();
	}

	std::string parseQualifiedName() {
		std::string name = expectIdentifier().text;
		while (matchSymbol("::") || matchSymbol(".")) {
			name += "::" + expectIdentifier().text;
		}
		return name;
	}

	Declaration parseRoutineDeclaration(bool isPublic, bool isCompile) {
		Routine routine = parseRoutineCore();
		routine.isPublic = isPublic;
		routine.isCompileTime = isCompile;
		return Declaration{routine, routine.span, routine.location};
	}

	Routine parseRoutineCore() {
		Routine routine;
		routine.location = previous().location;
		routine.span = {routine.location, routine.location};
		routine.name = expectIdentifier().text;
		routine.parameters = parseParameterList();
		if (matchKeyword("gives")) {
			routine.returnType = parseTypeName();
		} else {
			routine.returnType = "void";
		}
		skipNewlines();
		expectIndent();
		routine.body = parseBlockStatements();
		expectDedent();
		return routine;
	}

	Declaration parseForeignDeclaration(bool isPublic, bool isCompile) {
		std::string abi = parseTypeName();
		auto routine = parseRoutineCore();
		routine.isPublic = isPublic;
		routine.isCompileTime = isCompile;
		routine.isForeign = true;
		routine.foreignAbi = abi;
		return Declaration{ForeignDecl{abi, routine, routine.span, routine.location}, routine.span, routine.location};
	}

	std::vector<FieldDecl> parseFieldsBlock() {
		std::vector<FieldDecl> fields;
		skipNewlines();
		expectIndent();
		while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
			FieldDecl field;
			field.name = expectIdentifier().text;
			field.location = previous().location;
			expectSymbol(":");
			field.typeName = parseTypeName();
			fields.push_back(field);
			consumeLineBreak();
			skipNewlines();
		}
		expectDedent();
		return fields;
	}

	Declaration parseRecordDeclaration(bool isPublic, bool isCompile) {
		RecordDecl record;
		record.location = previous().location;
		record.span = {record.location, record.location};
		record.isPublic = isPublic;
		record.isCompileTime = isCompile;
		record.name = expectIdentifier().text;
		record.fields = parseFieldsBlock();
		return Declaration{record, record.span, record.location};
	}

	Declaration parseUnionDeclaration(bool isPublic, bool isCompile) {
		UnionDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.isPublic = isPublic;
		decl.layout = isCompile ? "packed" : "";
		decl.name = expectIdentifier().text;
		decl.fields = parseFieldsBlock();
		return Declaration{decl, decl.span, decl.location};
	}

	Declaration parseEnumDeclaration(bool isPublic, bool isCompile) {
		EnumDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.isPublic = isPublic;
		decl.underlyingType = isCompile ? "ubyte" : "";
		decl.name = expectIdentifier().text;
		skipNewlines();
		expectIndent();
		while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
			std::string memberName = expectIdentifier().text;
			std::optional<std::string> value;
			if (matchSymbol("=")) {
				value = parseTypeName();
			}
			decl.members.emplace_back(memberName, value);
			consumeLineBreak();
			skipNewlines();
		}
		expectDedent();
		return Declaration{decl, decl.span, decl.location};
	}

	Declaration parseVariantDeclaration(bool isPublic, bool isCompile) {
		VariantDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.isPublic = isPublic;
		decl.name = expectIdentifier().text;
		skipNewlines();
		expectIndent();
		while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
			VariantCase c;
			c.name = expectIdentifier().text;
			c.location = previous().location;
			if (matchSymbol("(")) {
				if (!checkSymbol(")")) {
					for (;;) {
						Parameter p;
						p.name = expectIdentifier().text;
						expectSymbol(":");
						p.typeName = parseTypeName();
						c.fields.push_back(p);
						if (!matchSymbol(",")) break;
					}
				}
				expectSymbol(")");
			}
			decl.cases.push_back(std::move(c));
			consumeLineBreak();
			skipNewlines();
		}
		expectDedent();
		(void)isCompile;
		return Declaration{decl, decl.span, decl.location};
	}

	Declaration parseModuleDeclaration(bool isPublic, bool isCompile) {
		ModuleDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.isPublic = isPublic;
		decl.name = expectIdentifier().text;
		consumeLineBreak();
		skipNewlines();
		expectIndent();
		while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
			decl.declarations.push_back(parseDeclaration());
			skipNewlines();
		}
		expectDedent();
		(void)isCompile;
		return Declaration{decl, decl.span, decl.location};
	}

	Declaration parseUseDeclaration() {
		UseDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.path.push_back(parseQualifiedName());
		if (matchKeyword("as")) {
			decl.alias = expectIdentifier().text;
		}
		consumeLineBreak();
		return Declaration{decl, decl.span, decl.location};
	}

	Declaration parseConstDeclaration(bool isPublic, bool isCompile) {
		ConstDecl decl;
		decl.location = previous().location;
		decl.span = {decl.location, decl.location};
		decl.isPublic = isPublic;
		decl.isCompileTime = isCompile;
		decl.name = expectIdentifier().text;
		if (matchSymbol(":")) {
			decl.typeName = parseTypeName();
		}
		expectSymbol("=");
		decl.value = parseExpression();
		consumeLineBreak();
		return Declaration{decl, decl.span, decl.location};
	}
	void skipNewlines() {
		while (peek().kind == TokenKind::Newline) {
			++index_;
		}
	}

	bool matchIndent() {
		if (peek().kind == TokenKind::Indent) {
			++index_;
			return true;
		}
		return false;
	}

	bool matchDedent() {
		if (peek().kind == TokenKind::Dedent) {
			++index_;
			return true;
		}
		return false;
	}

	void expectIndent() {
		if (!matchIndent()) {
			throw std::runtime_error("expected indentation block");
		}
	}

	void expectDedent() {
		if (!matchDedent()) {
			throw std::runtime_error("expected end of indented block");
		}
	}

	std::vector<Parameter> parseParameterList() {
		std::vector<Parameter> params;
		auto listLocation = peek().location;
		expectSymbol("(");
		if (!checkSymbol(")")) {
			for (;;) {
				Parameter parameter;
				parameter.name = expectIdentifier().text;
				parameter.location = previous().location;
				expectSymbol(":");
				parameter.typeName = parseTypeName();
				params.push_back(parameter);
				if (matchSymbol(",")) {
					continue;
				}
				break;
			}
		}
		expectSymbol(")");
		(void)listLocation;
		return params;
	}

	std::string parseTypeName() {
		if (peek().kind == TokenKind::Identifier || peek().kind == TokenKind::Keyword) {
			return consume().text;
		}
		throw std::runtime_error("expected type name");
	}

	Routine parseRoutine() {
		expectKeyword("routine");
		Routine routine;
		routine.location = previous().location;
		routine.span = {routine.location, routine.location};
		routine.name = expectIdentifier().text;
		routine.parameters = parseParameterList();
		if (matchKeyword("gives")) {
			routine.returnType = parseTypeName();
		} else {
			routine.returnType = "void";
		}
		skipNewlines();
		expectIndent();
		routine.body = parseBlockStatements();
		expectDedent();
		return routine;
	}

	std::vector<StmtPtr> parseBlockStatements() {
		std::vector<StmtPtr> statements;
		skipNewlines();
		while (!isAtEnd() && !checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
			statements.push_back(parseStatement());
			skipNewlines();
		}
		return statements;
	}

	bool checkToken(TokenKind kind) const {
		return peek().kind == kind;
	}

	StmtPtr makeStmt(Stmt::Node node, SourceSpan span, SourceLocation location) const {
		return std::visit([&](const auto& value) -> StmtPtr {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, LetStmt>) {
				return AstFactory::makeStmt(Stmt{LetStmt{value.name, value.typeName, value.initializer, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, VarStmt>) {
				return AstFactory::makeStmt(Stmt{VarStmt{value.name, value.typeName, value.initializer, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, AssignStmt>) {
				return AstFactory::makeStmt(Stmt{AssignStmt{value.name, value.value, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, ExprStmt>) {
				return AstFactory::makeStmt(Stmt{ExprStmt{value.expression, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, ReturnStmt>) {
				return AstFactory::makeStmt(Stmt{ReturnStmt{value.value, value.hasValue, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, GiveStmt>) {
				return AstFactory::makeStmt(Stmt{GiveStmt{value.value, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, IfStmt>) {
				return AstFactory::makeStmt(Stmt{IfStmt{value.condition, value.thenBranch, value.elseBranch, value.hasElse, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, EachStmt>) {
				return AstFactory::makeStmt(Stmt{EachStmt{value.bindings, value.range, value.body, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, UnsafeStmt>) {
				return AstFactory::makeStmt(Stmt{UnsafeStmt{value.body, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, DeferStmt>) {
				return AstFactory::makeStmt(Stmt{DeferStmt{value.expression, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, FailStmt>) {
				return AstFactory::makeStmt(Stmt{FailStmt{value.expression, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, WhileStmt>) {
				return AstFactory::makeStmt(Stmt{WhileStmt{value.condition, value.body, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, BlockStmt>) {
				return AstFactory::makeStmt(Stmt{BlockStmt{value.statements, span, location}, span, location}, span);
			} else {
				return AstFactory::makeStmt(Stmt{BlockStmt{{}, span, location}, span, location}, span);
			}
		}, node);
	}

	ExprPtr makeExpr(Expr::Node node, SourceSpan span, SourceLocation location) const {
		return std::visit([&](const auto& value) -> ExprPtr {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, NumberExpr>) {
				return AstFactory::makeExpr(Expr{NumberExpr{value.value, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, StringExpr>) {
				return AstFactory::makeExpr(Expr{StringExpr{value.value, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, BoolExpr>) {
				return AstFactory::makeExpr(Expr{BoolExpr{value.value, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, NullExpr>) {
				return AstFactory::makeExpr(Expr{NullExpr{span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, VariableExpr>) {
				return AstFactory::makeExpr(Expr{VariableExpr{value.name, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, UnaryExpr>) {
				return AstFactory::makeExpr(Expr{UnaryExpr{value.op, value.operand, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, BinaryExpr>) {
				return AstFactory::makeExpr(Expr{BinaryExpr{value.op, value.left, value.right, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, PipelineExpr>) {
				return AstFactory::makeExpr(Expr{PipelineExpr{value.input, value.stages, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, RangeExpr>) {
				return AstFactory::makeExpr(Expr{RangeExpr{value.start, value.end, value.step, value.inclusive, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, ChooseExpr>) {
				return AstFactory::makeExpr(Expr{ChooseExpr{value.subject, value.cases, value.otherwiseValue, value.hasOtherwise, span, location}, span, location}, span);
			} else if constexpr (std::is_same_v<T, CallExpr>) {
				return AstFactory::makeExpr(Expr{CallExpr{value.callee, value.arguments, span, location}, span, location}, span);
			} else {
				return AstFactory::makeExpr(Expr{GroupExpr{value.expression, span, location}, span, location}, span);
			}
		}, node);
	}

	std::string parseOperatorText() {
		Token token = consume();
		return token.text;
	}

	StmtPtr parseStatement() {
		if (matchKeyword("let")) {
			auto location = previous().location;
			auto start = location;
			const std::string name = expectIdentifier().text;
			std::string typeName;
			if (matchSymbol(":")) {
				typeName = parseTypeName();
			}
			expectSymbol("=");
			auto initializer = parseExpression();
			consumeLineBreak();
			return makeStmt(LetStmt{name, typeName, initializer, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("var")) {
			auto location = previous().location;
			auto start = location;
			const std::string name = expectIdentifier().text;
			std::string typeName;
			if (matchSymbol(":")) {
				typeName = parseTypeName();
			}
			auto initializer = ExprPtr{};
			if (matchSymbol("=")) {
				initializer = parseExpression();
			}
			consumeLineBreak();
			return makeStmt(VarStmt{name, typeName, initializer, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("if")) {
			auto location = previous().location;
			auto start = location;
			auto condition = parseExpression();
			consumeLineBreak();
			skipNewlines();
			expectIndent();
			auto thenBranch = parseBlockStatements();
			expectDedent();
			std::vector<StmtPtr> elseBranch;
			bool hasElse = false;
			skipNewlines();
			if (matchKeyword("else")) {
				consumeLineBreak();
				skipNewlines();
				expectIndent();
				elseBranch = parseBlockStatements();
				expectDedent();
				hasElse = true;
			}
			return makeStmt(IfStmt{condition, std::move(thenBranch), std::move(elseBranch), hasElse, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("while")) {
			auto location = previous().location;
			auto start = location;
			auto condition = parseExpression();
			consumeLineBreak();
			skipNewlines();
			expectIndent();
			auto body = parseBlockStatements();
			expectDedent();
			return makeStmt(WhileStmt{condition, std::move(body), {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("each")) {
			auto location = previous().location;
			auto start = location;
			std::vector<std::string> bindings;
			bindings.push_back(expectIdentifier().text);
			while (matchSymbol(",")) {
				bindings.push_back(expectIdentifier().text);
			}
			expectKeyword("in");
			auto range = parseExpression();
			consumeLineBreak();
			skipNewlines();
			expectIndent();
			auto body = parseBlockStatements();
			expectDedent();
			return makeStmt(EachStmt{std::move(bindings), range, std::move(body), {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("unsafe")) {
			auto location = previous().location;
			auto start = location;
			consumeLineBreak();
			skipNewlines();
			expectIndent();
			auto body = parseBlockStatements();
			expectDedent();
			return makeStmt(UnsafeStmt{std::move(body), {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("defer")) {
			auto location = previous().location;
			auto start = location;
			auto expression = parseExpression();
			consumeLineBreak();
			return makeStmt(DeferStmt{expression, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("fail")) {
			auto location = previous().location;
			auto start = location;
			auto expression = parseExpression();
			consumeLineBreak();
			return makeStmt(FailStmt{expression, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("give")) {
			auto location = previous().location;
			auto start = location;
			auto value = parseExpression();
			consumeLineBreak();
			return makeStmt(GiveStmt{value, {start, location}, location}, {start, location}, location);
		}

		if (matchKeyword("return")) {
			auto location = previous().location;
			auto start = location;
			if (peek().kind == TokenKind::Newline || peek().kind == TokenKind::Dedent || peek().kind == TokenKind::EndOfFile) {
				consumeLineBreak();
				return makeStmt(ReturnStmt{nullptr, false, {start, location}, location}, {start, location}, location);
			}
			auto value = parseExpression();
			consumeLineBreak();
			return makeStmt(ReturnStmt{value, true, {start, location}, location}, {start, location}, location);
		}

		if (peek().kind == TokenKind::Identifier && checkSymbolAhead(1, "=")) {
			auto location = peek().location;
			auto start = location;
			std::string name = expectIdentifier().text;
			expectSymbol("=");
			auto value = parseExpression();
			consumeLineBreak();
			return makeStmt(AssignStmt{name, value, {start, location}, location}, {start, location}, location);
		}

		auto location = peek().location;
		auto start = location;
		auto expression = parseExpression();
		consumeLineBreak();
		return makeStmt(ExprStmt{expression, {start, location}, location}, {start, location}, location);
	}

	void consumeLineBreak() {
		if (peek().kind == TokenKind::Newline) {
			++index_;
			return;
		}
		if (peek().kind == TokenKind::Dedent || peek().kind == TokenKind::EndOfFile) {
			return;
		}
		throw std::runtime_error("expected end of line");
	}

	ExprPtr parseExpression() { return parseChoose(); }

	ExprPtr parseChoose() {
		if (matchKeyword("choose")) {
			auto location = previous().location;
			auto start = location;
			ExprPtr subject;
			if (!(checkToken(TokenKind::Newline) || checkToken(TokenKind::Indent) || checkToken(TokenKind::Dedent) || checkToken(TokenKind::EndOfFile))) {
				subject = parsePipeline();
			}
			consumeLineBreak();
			skipNewlines();
			expectIndent();
			std::vector<ChooseCase> cases;
			ExprPtr otherwiseValue;
			bool hasOtherwise = false;
			while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
				if (matchKeyword("when") || matchKeyword("case")) {
					auto clauseLoc = previous().location;
					auto clauseStart = clauseLoc;
					auto condition = parseExpression();
					consumeLineBreak();
					skipNewlines();
					expectIndent();
					auto value = parseExpression();
					consumeLineBreak();
					expectDedent();
					cases.push_back(ChooseCase{condition, value, {}, {}, {clauseStart, clauseLoc}, clauseLoc});
					continue;
				}
				if (matchKeyword("otherwise")) {
					auto clauseLoc = previous().location;
					auto clauseStart = clauseLoc;
					consumeLineBreak();
					skipNewlines();
					expectIndent();
					otherwiseValue = parseExpression();
					consumeLineBreak();
					expectDedent();
					hasOtherwise = true;
					continue;
				}
				throw std::runtime_error("expected choose clause");
			}
			expectDedent();
			return makeExpr(ChooseExpr{subject, std::move(cases), otherwiseValue, hasOtherwise, {start, location}, location}, {start, location}, location);
		}
		return parsePipeline();
	}

	ExprPtr parsePipeline() {
		auto expr = parseLogicalOr();
		while (matchSymbol("->")) {
			auto location = previous().location;
			std::vector<ExprPtr> stages;
			stages.push_back(parseCall());
			auto start = expr ? expr->span.start : location;
			expr = makeExpr(PipelineExpr{expr, std::move(stages), {start, stages.back() ? stages.back()->span.end : location}, location}, {start, stages.back() ? stages.back()->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseLogicalOr() {
		auto expr = parseLogicalAnd();
		while (matchKeyword("or") || matchSymbol("||")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseLogicalAnd();
			expr = makeExpr(BinaryExpr{op, expr, right, {location, right ? right->span.end : location}, location}, {location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseLogicalAnd() {
		auto expr = parseEquality();
		while (matchKeyword("and") || matchSymbol("&&")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseEquality();
			expr = makeExpr(BinaryExpr{op, expr, right, {expr ? expr->span.start : location, right ? right->span.end : location}, location}, {expr ? expr->span.start : location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseEquality() {
		auto expr = parseComparison();
		while (matchSymbol("==") || matchSymbol("!=")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseComparison();
			expr = makeExpr(BinaryExpr{op, expr, right, {expr ? expr->span.start : location, right ? right->span.end : location}, location}, {expr ? expr->span.start : location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseComparison() {
		auto expr = parseRange();
		while (matchSymbol("<") || matchSymbol("<=") || matchSymbol(">") || matchSymbol(">=")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseTerm();
			expr = makeExpr(BinaryExpr{op, expr, right, {expr ? expr->span.start : location, right ? right->span.end : location}, location}, {expr ? expr->span.start : location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseRange() {
		auto expr = parseTerm();
		while (matchSymbol("..") || matchSymbol("..<")) {
			auto location = previous().location;
			auto start = expr ? expr->span.start : location;
			auto right = parseTerm();
			bool inclusive = previous().text == "..";
			expr = makeExpr(RangeExpr{expr, right, nullptr, inclusive, {start, right ? right->span.end : location}, location}, {start, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseTerm() {
		auto expr = parseFactor();
		while (matchSymbol("+") || matchSymbol("-")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseFactor();
			expr = makeExpr(BinaryExpr{op, expr, right, {expr ? expr->span.start : location, right ? right->span.end : location}, location}, {expr ? expr->span.start : location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseFactor() {
		auto expr = parseUnary();
		while (matchSymbol("*") || matchSymbol("/") || matchSymbol("%")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto right = parseUnary();
			expr = makeExpr(BinaryExpr{op, expr, right, {expr ? expr->span.start : location, right ? right->span.end : location}, location}, {expr ? expr->span.start : location, right ? right->span.end : location}, location);
		}
		return expr;
	}

	ExprPtr parseUnary() {
		if (matchSymbol("-") || matchSymbol("!") || matchKeyword("not")) {
			std::string op = previous().text;
			auto location = previous().location;
			auto operand = parseUnary();
			return makeExpr(UnaryExpr{op, operand, {location, operand ? operand->span.end : location}, location}, {location, operand ? operand->span.end : location}, location);
		}
		return parseCall();
	}

	ExprPtr parseCall() {
		auto expr = parsePrimary();
		while (matchSymbol("(")) {
			auto location = previous().location;
			auto start = expr ? expr->span.start : location;
			std::vector<ExprPtr> arguments;
			if (!checkSymbol(")")) {
				for (;;) {
					arguments.push_back(parseExpression());
					if (!matchSymbol(",")) {
						break;
					}
				}
			}
			expectSymbol(")");
			expr = makeExpr(CallExpr{expr, std::move(arguments), {start, location}, location}, {start, location}, location);
		}
		return expr;
	}

	ExprPtr parsePrimary() {
		if (matchSymbol("(")) {
			auto location = previous().location;
			auto start = location;
			auto expr = parseExpression();
			expectSymbol(")");
			return makeExpr(GroupExpr{expr, {start, location}, location}, {start, location}, location);
		}
		if (peek().kind == TokenKind::Number) {
			auto token = consume();
			return makeExpr(NumberExpr{token.text, {token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		if (peek().kind == TokenKind::String) {
			auto token = consume();
			return makeExpr(StringExpr{token.text, {token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		if (matchKeyword("true")) {
			auto token = previous();
			return makeExpr(BoolExpr{true, {token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		if (matchKeyword("false")) {
			auto token = previous();
			return makeExpr(BoolExpr{false, {token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		if (matchKeyword("null")) {
			auto token = previous();
			return makeExpr(NullExpr{{token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		if (peek().kind == TokenKind::Identifier) {
			auto token = consume();
			return makeExpr(VariableExpr{token.text, {token.location, token.location}, token.location}, {token.location, token.location}, token.location);
		}
		throw std::runtime_error("expected expression");
	}

	void expectSymbol(const std::string& value) {
		if (!matchSymbol(value)) {
			throw std::runtime_error("expected symbol '" + value + "'");
		}
	}

	std::vector<Token> tokens_;
	std::size_t index_ = 0;
};

// --- Frontend: parsing and semantic analysis --------------------------------
class SemanticAnalyzer {
public:
	SemanticAnalysisResult analyze(const Program& program) {
		result_ = {};
		routines_.clear();
		declarationScopes_.clear();
		pushDeclarationScope();
		for (const auto& declaration : program.declarations) {
			registerDeclaration(declaration);
		}
		for (const auto& routine : program.routines) {
			registerRoutine(routine);
		}
		for (const auto& declaration : program.declarations) {
			analyzeDeclaration(declaration);
		}
		for (const auto& routine : program.routines) {
			analyzeRoutine(routine);
		}
		return result_;
	}

private:
	void registerNominal(const std::string& name, const std::string& kind, bool isPublic, bool isCompileTime, SourceLocation location) {
		DeclarationInfo info{name, kind, isPublic, isCompileTime, {}};
		result_.declarations.push_back(std::move(info));
		if (declarationScopes_.empty()) {
			pushDeclarationScope();
		}
		auto& scope = declarationScopes_.back();
		if (scope.contains(name)) {
			report(location, "duplicate top-level declaration: " + name);
			return;
		}
		scope.insert(name);
	}

	void pushDeclarationScope() {
		declarationScopes_.push_back({});
	}

	void popDeclarationScope() {
		if (!declarationScopes_.empty()) {
			declarationScopes_.pop_back();
		}
	}

	std::string joinPath(const std::vector<std::string>& path) const {
		std::string text;
		for (std::size_t i = 0; i < path.size(); ++i) {
			if (i != 0) text += "::";
			text += path[i];
		}
		return text;
	}

	void registerRoutine(const Routine& routine) {
		RoutineSignature signature;
		signature.name = routine.name;
		signature.returnType = semanticTypeFromName(routine.returnType);
		for (const auto& parameter : routine.parameters) {
			if (std::find(signature.parameterNames.begin(), signature.parameterNames.end(), parameter.name) != signature.parameterNames.end()) {
				report(parameter.location, "duplicate parameter name in routine '" + routine.name + "': " + parameter.name);
				continue;
			}
			signature.parameterNames.push_back(parameter.name);
			signature.parameterTypes.push_back(semanticTypeFromName(parameter.typeName));
		}
		auto& overloads = routines_[routine.name];
		for (const auto& existing : overloads) {
			if (sameSignature(existing, signature)) {
				report(routine.location, "duplicate routine overload: " + routine.name + formatSignature(signature));
				return;
			}
		}
		overloads.push_back(std::move(signature));
	}

	void registerDeclaration(const Declaration& declaration) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, RecordDecl>) {
				registerNominal(node.name, "record", node.isPublic, node.isCompileTime, node.location);
				NominalTypeInfo info;
				info.name = node.name;
				info.kind = "record";
				info.isPublic = node.isPublic;
				info.layout = node.layout;
				for (const auto& field : node.fields) {
					info.fields.emplace_back(field.name, field.typeName);
				}
				result_.nominalTypes[node.name] = std::move(info);
			} else if constexpr (std::is_same_v<T, EnumDecl>) {
				registerNominal(node.name, "enum", node.isPublic, false, node.location);
				NominalTypeInfo info;
				info.name = node.name;
				info.kind = "enum";
				info.isPublic = node.isPublic;
				info.layout = node.underlyingType;
				for (const auto& member : node.members) {
					info.members.emplace_back(member.first, member.second.value_or("unknown"));
				}
				result_.nominalTypes[node.name] = std::move(info);
			} else if constexpr (std::is_same_v<T, UnionDecl>) {
				registerNominal(node.name, "union", node.isPublic, false, node.location);
				NominalTypeInfo info;
				info.name = node.name;
				info.kind = "union";
				info.isPublic = node.isPublic;
				info.layout = node.layout;
				for (const auto& field : node.fields) {
					info.fields.emplace_back(field.name, field.typeName);
				}
				result_.nominalTypes[node.name] = std::move(info);
			} else if constexpr (std::is_same_v<T, VariantDecl>) {
				registerNominal(node.name, "variant", node.isPublic, false, node.location);
				NominalTypeInfo info;
				info.name = node.name;
				info.kind = "variant";
				info.isPublic = node.isPublic;
				for (const auto& c : node.cases) {
					info.tags.push_back(c.name);
				}
				result_.nominalTypes[node.name] = std::move(info);
			} else if constexpr (std::is_same_v<T, ModuleDecl>) {
				registerNominal(node.name, "module", node.isPublic, false, node.location);
				DeclarationInfo info{node.name, "module", node.isPublic, false, "module"};
				result_.declarations.push_back(std::move(info));
			} else if constexpr (std::is_same_v<T, UseDecl>) {
				DeclarationInfo info{node.alias.empty() ? node.path.back() : node.alias, "use", false, false, joinPath(node.path)};
				result_.declarations.push_back(std::move(info));
			} else if constexpr (std::is_same_v<T, ConstDecl>) {
				DeclarationInfo info{node.name, "const", node.isPublic, node.isCompileTime, node.typeName};
				result_.declarations.push_back(std::move(info));
				declareSymbol(node.location, node.name, {semanticTypeFromName(node.typeName), false, false});
			} else if constexpr (std::is_same_v<T, ForeignDecl>) {
				registerRoutine(node.routine);
				DeclarationInfo info{node.routine.name, "foreign", node.routine.isPublic, node.routine.isCompileTime, node.abi};
				result_.declarations.push_back(std::move(info));
			} else if constexpr (std::is_same_v<T, Routine>) {
				DeclarationInfo info{node.name, "routine", node.isPublic, node.isCompileTime, node.isForeign ? node.foreignAbi : std::string{}};
				result_.declarations.push_back(std::move(info));
			}
		}, declaration.node);
	}

	void analyzeDeclaration(const Declaration& declaration) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, ConstDecl>) {
				if (node.value) analyzeExpr(node.value);
			} else if constexpr (std::is_same_v<T, ModuleDecl>) {
				pushDeclarationScope();
				for (const auto& nested : node.declarations) {
					registerDeclaration(nested);
				}
				for (const auto& nested : node.declarations) {
					analyzeDeclaration(nested);
				}
				popDeclarationScope();
			} else if constexpr (std::is_same_v<T, ForeignDecl>) {
				analyzeRoutine(node.routine);
			}
		}, declaration.node);
	}

	void analyzeRoutine(const Routine& routine) {
		auto routineIt = routines_.find(routine.name);
		if (routineIt == routines_.end()) {
			return;
		}

		currentRoutine_ = &routine;
		currentReturnType_ = makeUnknownType();
		const RoutineSignature* matchingSignature = findSignatureByArity(routineIt->second, routine.parameters.size());
		std::vector<SemanticType> parameterTypes = matchingSignature ? matchingSignature->parameterTypes : std::vector<SemanticType>{};
		if (matchingSignature) {
			currentReturnType_ = matchingSignature->returnType;
		}
		scopes_.clear();
		pushScope();
		for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
			SemanticType parameterType = i < parameterTypes.size() ? parameterTypes[i] : makeUnknownType();
			declareSymbol(routine.parameters[i].location, routine.parameters[i].name, {parameterType, false, true});
		}
		analyzeStatementList(routine.body);
		popScope();
		currentRoutine_ = nullptr;
	}

	bool analyzeStatementList(const std::vector<StmtPtr>& statements, bool reachable = true) {
		bool fallsThrough = true;
		for (const auto& stmt : statements) {
			if (!reachable) {
				if (stmt) {
					report(stmt->location, "unreachable code");
					analyzeUnreachableStatement(stmt);
				}
				continue;
			}
			if (stmt) {
				fallsThrough = analyzeStatementNode(stmt);
				if (!fallsThrough) {
					reachable = false;
				}
			}
		}
		return fallsThrough;
	}

	bool analyzeStatementNode(const StmtPtr& stmt) {
		if (!stmt) {
			return true;
		}

		return std::visit([&](const auto& node) -> bool {
			return analyzeReachableStatement(node);
		}, stmt->node);
	}

	void analyzeUnreachableStatement(const StmtPtr& stmt) {
		if (!stmt) {
			return;
		}
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, LetStmt>) {
				analyzeUnreachableLet(node);
			} else if constexpr (std::is_same_v<T, VarStmt>) {
				analyzeUnreachableVar(node);
			} else if constexpr (std::is_same_v<T, AssignStmt>) {
				analyzeUnreachableAssign(node);
			} else if constexpr (std::is_same_v<T, ExprStmt>) {
				analyzeExpr(node.expression);
			} else if constexpr (std::is_same_v<T, ReturnStmt>) {
				analyzeUnreachableReturn(node);
			} else if constexpr (std::is_same_v<T, GiveStmt>) {
				analyzeUnreachableGive(node);
			} else if constexpr (std::is_same_v<T, IfStmt>) {
				analyzeUnreachableIf(node);
			} else if constexpr (std::is_same_v<T, EachStmt>) {
				analyzeUnreachableEach(node);
			} else if constexpr (std::is_same_v<T, UnsafeStmt>) {
				analyzeUnreachableUnsafe(node);
			} else if constexpr (std::is_same_v<T, DeferStmt>) {
				analyzeUnreachableDefer(node);
			} else if constexpr (std::is_same_v<T, FailStmt>) {
				analyzeUnreachableFail(node);
			} else if constexpr (std::is_same_v<T, WhileStmt>) {
				analyzeUnreachableWhile(node);
			} else if constexpr (std::is_same_v<T, BlockStmt>) {
				analyzeUnreachableBlock(node);
			}
		}, stmt->node);
	}

	bool analyzeReachableStatement(const Stmt::Node& node) {
		return std::visit([&](const auto& statement) -> bool {
			using T = std::decay_t<decltype(statement)>;
			if constexpr (std::is_same_v<T, LetStmt>) {
				return analyzeLet(statement);
			} else if constexpr (std::is_same_v<T, VarStmt>) {
				return analyzeVar(statement);
			} else if constexpr (std::is_same_v<T, AssignStmt>) {
				return analyzeAssign(statement);
			} else if constexpr (std::is_same_v<T, ExprStmt>) {
				analyzeExpr(statement.expression);
				return true;
			} else if constexpr (std::is_same_v<T, ReturnStmt>) {
				return analyzeReturn(statement);
			} else if constexpr (std::is_same_v<T, GiveStmt>) {
				return analyzeGive(statement);
			} else if constexpr (std::is_same_v<T, IfStmt>) {
				return analyzeIf(statement);
			} else if constexpr (std::is_same_v<T, EachStmt>) {
				return analyzeEach(statement);
			} else if constexpr (std::is_same_v<T, UnsafeStmt>) {
				return analyzeUnsafe(statement);
			} else if constexpr (std::is_same_v<T, DeferStmt>) {
				return analyzeDefer(statement);
			} else if constexpr (std::is_same_v<T, FailStmt>) {
				return analyzeFail(statement);
			} else if constexpr (std::is_same_v<T, WhileStmt>) {
				return analyzeWhile(statement);
			} else if constexpr (std::is_same_v<T, BlockStmt>) {
				return analyzeBlock(statement);
			} else {
				return true;
			}
		}, node);
	}

	bool analyzeLet(const LetStmt& node) {
		if (!node.initializer) {
			report(node.location, "let binding requires an initializer");
			declareSymbol(node.location, node.name, {makeUnknownType(), false, false});
			return true;
		}
		auto valueType = analyzeExpr(node.initializer);
		SemanticType declaredType = semanticTypeFromName(node.typeName);
		if (declaredType.kind != SemanticTypeKind::Unknown) {
			if (!isAssignableTo(declaredType, valueType)) {
				report(node.location, "let '" + node.name + "' initializer type mismatch: expected " + semanticTypeName(declaredType) + ", got " + semanticTypeName(valueType));
			}
			declareSymbol(node.location, node.name, {declaredType, false, false});
		} else {
			if (valueType.kind == SemanticTypeKind::Unknown) {
				report(node.location, "cannot infer type of let binding '" + node.name + "'");
			}
			declareSymbol(node.location, node.name, {valueType, false, false});
		}
		return true;
	}

	bool analyzeVar(const VarStmt& node) {
		SemanticType declaredType = semanticTypeFromName(node.typeName);
		if (node.initializer) {
			auto valueType = analyzeExpr(node.initializer);
			if (declaredType.kind != SemanticTypeKind::Unknown) {
				if (!isAssignableTo(declaredType, valueType)) {
					report(node.location, "var '" + node.name + "' initializer type mismatch: expected " + semanticTypeName(declaredType) + ", got " + semanticTypeName(valueType));
				}
				declareSymbol(node.location, node.name, {declaredType, true, false});
			} else {
				if (valueType.kind == SemanticTypeKind::Unknown) {
					report(node.location, "cannot infer type of var binding '" + node.name + "'");
				}
				declareSymbol(node.location, node.name, {valueType, true, false});
			}
		} else {
			if (declaredType.kind == SemanticTypeKind::Unknown) {
				report(node.location, "var '" + node.name + "' requires a type or initializer");
				declareSymbol(node.location, node.name, {makeUnknownType(), true, false});
			} else {
				declareSymbol(node.location, node.name, {declaredType, true, false});
			}
		}
		return true;
	}

	bool analyzeAssign(const AssignStmt& node) {
		auto* symbol = lookupSymbol(node.name);
		if (!symbol) {
			report(node.location, "assignment to undeclared name: " + node.name);
			analyzeExpr(node.value);
			return true;
		}
		if (!symbol->isMutable) {
			report(node.location, "cannot assign to immutable binding: " + node.name);
		}
		auto valueType = analyzeExpr(node.value);
		if (!isAssignableTo(symbol->type, valueType)) {
			report(node.location, "assignment type mismatch for '" + node.name + "': expected " + semanticTypeName(symbol->type) + ", got " + semanticTypeName(valueType));
		}
		return true;
	}

	bool analyzeReturn(const ReturnStmt& node) {
		if (!currentRoutine_) {
			report(node.location, "return outside of routine");
			return true;
		}
		if (node.hasValue) {
			auto valueType = analyzeExpr(node.value);
			if (currentReturnType_.kind == SemanticTypeKind::Void) {
				report(node.location, "routine '" + currentRoutine_->name + "' returns void, but return provides a value");
			} else if (!isAssignableTo(currentReturnType_, valueType)) {
				report(node.location, "return type mismatch in routine '" + currentRoutine_->name + "': expected " + semanticTypeName(currentReturnType_) + ", got " + semanticTypeName(valueType));
			}
		} else if (currentReturnType_.kind != SemanticTypeKind::Void) {
			report(node.location, "routine '" + currentRoutine_->name + "' must return " + semanticTypeName(currentReturnType_));
		}
		return false;
	}

	bool analyzeGive(const GiveStmt& node) {
		if (!currentRoutine_) {
			report(node.location, "give outside of routine");
			return true;
		}
		auto valueType = analyzeExpr(node.value);
		if (currentReturnType_.kind == SemanticTypeKind::Void) {
			report(node.location, "routine '" + currentRoutine_->name + "' returns void, but give provides a value");
		} else if (!isAssignableTo(currentReturnType_, valueType)) {
			report(node.location, "give type mismatch in routine '" + currentRoutine_->name + "': expected " + semanticTypeName(currentReturnType_) + ", got " + semanticTypeName(valueType));
		}
		return false;
	}

	bool analyzeIf(const IfStmt& node) {
		auto conditionType = analyzeExpr(node.condition);
		if (conditionType.kind != SemanticTypeKind::Bool && conditionType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "if condition must be bool, got " + semanticTypeName(conditionType));
		}
		auto constant = evaluateConstantBool(node.condition);
		if (constant.has_value()) {
			if (*constant) {
				pushScope();
				auto thenFallsThrough = analyzeStatementList(node.thenBranch, true);
				popScope();
				if (node.hasElse) {
					report(node.location, "else branch is unreachable because condition is always true");
					pushScope();
					analyzeStatementList(node.elseBranch, false);
					popScope();
				}
				return thenFallsThrough;
			}
			report(node.location, "then branch is unreachable because condition is always false");
			pushScope();
			analyzeStatementList(node.thenBranch, false);
			popScope();
			if (node.hasElse) {
				pushScope();
				auto elseFallsThrough = analyzeStatementList(node.elseBranch, true);
				popScope();
				return elseFallsThrough;
			}
			return true;
		}
		pushScope();
		auto thenFallsThrough = analyzeStatementList(node.thenBranch, true);
		popScope();
		bool elseFallsThrough = true;
		if (node.hasElse) {
			pushScope();
			elseFallsThrough = analyzeStatementList(node.elseBranch, true);
			popScope();
		}
		return node.hasElse ? (thenFallsThrough || elseFallsThrough) : true;
	}

	bool analyzeEach(const EachStmt& node) {
		analyzeExpr(node.range);
		pushScope();
		for (const auto& binding : node.bindings) {
			declareSymbol(node.location, binding, {makeUnknownType(), false, false});
		}
		auto fallsThrough = analyzeStatementList(node.body, true);
		popScope();
		return fallsThrough;
	}

	bool analyzeUnsafe(const UnsafeStmt& node) {
		pushScope();
		auto fallsThrough = analyzeStatementList(node.body, true);
		popScope();
		return fallsThrough;
	}

	bool analyzeDefer(const DeferStmt& node) {
		analyzeExpr(node.expression);
		return true;
	}

	bool analyzeFail(const FailStmt& node) {
		analyzeExpr(node.expression);
		return false;
	}

	bool analyzeWhile(const WhileStmt& node) {
		auto conditionType = analyzeExpr(node.condition);
		if (conditionType.kind != SemanticTypeKind::Bool && conditionType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "while condition must be bool, got " + semanticTypeName(conditionType));
		}
		auto constant = evaluateConstantBool(node.condition);
		pushScope();
		if (constant.has_value() && !*constant) {
			report(node.location, "while body is unreachable because condition is always false");
			analyzeStatementList(node.body, false);
			popScope();
			return true;
		}
		analyzeStatementList(node.body, true);
		popScope();
		return true;
	}

	bool analyzeBlock(const BlockStmt& node) {
		pushScope();
		auto fallsThrough = analyzeStatementList(node.statements, true);
		popScope();
		return fallsThrough;
	}

	void analyzeUnreachableLet(const LetStmt& node) {
		if (node.initializer) analyzeExpr(node.initializer);
	}

	void analyzeUnreachableVar(const VarStmt& node) {
		if (node.initializer) analyzeExpr(node.initializer);
	}

	void analyzeUnreachableAssign(const AssignStmt& node) {
		analyzeExpr(node.value);
	}

	void analyzeUnreachableReturn(const ReturnStmt& node) {
		if (node.hasValue) analyzeExpr(node.value);
	}

	void analyzeUnreachableGive(const GiveStmt& node) {
		analyzeExpr(node.value);
	}

	void analyzeUnreachableIf(const IfStmt& node) {
		analyzeExpr(node.condition);
		analyzeStatementList(node.thenBranch, false);
		if (node.hasElse) {
			analyzeStatementList(node.elseBranch, false);
		}
	}

	void analyzeUnreachableEach(const EachStmt& node) {
		analyzeExpr(node.range);
		analyzeStatementList(node.body, false);
	}

	void analyzeUnreachableUnsafe(const UnsafeStmt& node) {
		analyzeStatementList(node.body, false);
	}

	void analyzeUnreachableDefer(const DeferStmt& node) {
		analyzeExpr(node.expression);
	}

	void analyzeUnreachableFail(const FailStmt& node) {
		analyzeExpr(node.expression);
	}

	void analyzeUnreachableWhile(const WhileStmt& node) {
		analyzeExpr(node.condition);
		analyzeStatementList(node.body, false);
	}

	void analyzeUnreachableBlock(const BlockStmt& node) {
		analyzeStatementList(node.statements, false);
	}

	std::optional<bool> evaluateConstantBool(const ExprPtr& expr) {
		if (!expr) {
			return std::nullopt;
		}
		return std::visit([&](const auto& node) -> std::optional<bool> {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, BoolExpr>) {
				return node.value;
			} else if constexpr (std::is_same_v<T, GroupExpr>) {
				return evaluateConstantBool(node.expression);
			} else if constexpr (std::is_same_v<T, UnaryExpr>) {
				if (node.op == "!" || node.op == "not") {
					auto value = evaluateConstantBool(node.operand);
					if (value.has_value()) {
						return !*value;
					}
				}
				return std::nullopt;
			} else if constexpr (std::is_same_v<T, BinaryExpr>) {
				auto left = evaluateConstantBool(node.left);
				auto right = evaluateConstantBool(node.right);
				if ((node.op == "and" || node.op == "&&") && left.has_value() && right.has_value()) {
					return *left && *right;
				}
				if ((node.op == "or" || node.op == "||") && left.has_value() && right.has_value()) {
					return *left || *right;
				}
				if (node.op == "==" && left.has_value() && right.has_value()) {
					return *left == *right;
				}
				if (node.op == "!=" && left.has_value() && right.has_value()) {
					return *left != *right;
				}
				return std::nullopt;
			} else {
				return std::nullopt;
			}
		}, expr->node);
	}

	IRValueType binaryValueType(const std::string& op) const {
		if (op == "and" || op == "or" || op == "&&" || op == "||" || op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
			return makeIRBoolType();
		}
		if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
			return makeIRIntType();
		}
		return makeIRUnknownType();
	}

	std::string calleeSignature(const ExprPtr& callee) const {
		if (!callee) {
			return "<unknown>()";
		}
		if (const auto* variable = std::get_if<VariableExpr>(&callee->node)) {
			return variable->name + "()";
		}
		return "<expr>()";
	}

	SemanticType analyzeUnaryExpr(const UnaryExpr& node) {
		auto operandType = analyzeExpr(node.operand);
		if (node.op == "-") {
			if (!isNumericType(operandType) && operandType.kind != SemanticTypeKind::Unknown) {
				report(node.location, "unary '-' requires a number, got " + semanticTypeName(operandType));
			}
			return isNumericType(operandType) || operandType.kind == SemanticTypeKind::Unknown ? makeNumberType() : makeUnknownType();
		}
		if (node.op == "!" || node.op == "not") {
			if (operandType.kind != SemanticTypeKind::Bool && operandType.kind != SemanticTypeKind::Unknown) {
				report(node.location, "logical negation requires bool, got " + semanticTypeName(operandType));
			}
			return operandType.kind == SemanticTypeKind::Bool || operandType.kind == SemanticTypeKind::Unknown ? makeBoolType() : makeUnknownType();
		}
		return makeUnknownType();
	}

	SemanticType analyzeBinaryExpr(const BinaryExpr& node) {
		auto leftType = analyzeExpr(node.left);
		auto rightType = analyzeExpr(node.right);
		if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/" || node.op == "%") {
			return analyzeArithmeticBinary(node, leftType, rightType);
		}
		if (node.op == "and" || node.op == "or" || node.op == "&&" || node.op == "||") {
			return analyzeLogicalBinary(node, leftType, rightType);
		}
		if (node.op == "<" || node.op == "<=" || node.op == ">" || node.op == ">=") {
			return analyzeComparisonBinary(node, leftType, rightType);
		}
		if (node.op == "==" || node.op == "!=") {
			return analyzeEqualityBinary(node, leftType, rightType);
		}
		return makeUnknownType();
	}

	SemanticType analyzeCallExpr(const CallExpr& node) {
		if (const auto* callee = std::get_if<VariableExpr>(&node.callee->node)) {
			if (callee->name == "print") {
				for (const auto& argument : node.arguments) {
					analyzeExpr(argument);
				}
				return makeVoidType();
			}
			return resolveRoutineCall(callee->name, node.arguments, node.location);
		}
		analyzeExpr(node.callee);
		for (const auto& argument : node.arguments) {
			analyzeExpr(argument);
		}
		report(node.location, "call target must be a named routine");
		return makeUnknownType();
	}

	SemanticType analyzeArithmeticBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
		if ((!isNumericType(leftType) || !isNumericType(rightType)) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "arithmetic operator '" + node.op + "' requires numbers");
		}
		return (isNumericType(leftType) || isNumericType(rightType) || leftType.kind == SemanticTypeKind::Unknown || rightType.kind == SemanticTypeKind::Unknown)
			? makeNumberType()
			: makeUnknownType();
	}

	SemanticType analyzeLogicalBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
		if ((leftType.kind != SemanticTypeKind::Bool || rightType.kind != SemanticTypeKind::Bool) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "logical operator '" + node.op + "' requires bool operands");
		}
		return (leftType.kind == SemanticTypeKind::Bool || rightType.kind == SemanticTypeKind::Bool || leftType.kind == SemanticTypeKind::Unknown || rightType.kind == SemanticTypeKind::Unknown)
			? makeBoolType()
			: makeUnknownType();
	}

	SemanticType analyzeComparisonBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
		if ((!isNumericType(leftType) || !isNumericType(rightType)) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "comparison operator '" + node.op + "' requires numbers");
		}
		return makeBoolType();
	}

	SemanticType analyzeChooseExpr(const ChooseExpr& node) {
		analyzeExpr(node.subject);
		SemanticType resultType = makeUnknownType();
		for (const auto& choice : node.cases) {
			analyzeExpr(choice.condition);
			auto branchType = analyzeExpr(choice.value);
			if (resultType.kind == SemanticTypeKind::Unknown) {
				resultType = branchType;
			} else if (!isAssignableTo(resultType, branchType) && !isAssignableTo(branchType, resultType)) {
				report(choice.location, "choose branches must produce compatible types");
			}
		}
		if (node.hasOtherwise) {
			auto otherwiseType = analyzeExpr(node.otherwiseValue);
			if (resultType.kind == SemanticTypeKind::Unknown) {
				resultType = otherwiseType;
			} else if (!isAssignableTo(resultType, otherwiseType) && !isAssignableTo(otherwiseType, resultType)) {
				report(node.location, "otherwise branch must match choose branch types");
			}
		}
		return resultType;
	}

	SemanticType analyzeEqualityBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
		if (!isAssignableTo(leftType, rightType) && !isAssignableTo(rightType, leftType) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
			report(node.location, "equality comparison requires compatible types");
		}
		return makeBoolType();
	}

	SemanticType analyzeExpr(const ExprPtr& expr) {
		if (!expr) {
			return makeUnknownType();
		}

		return std::visit([&](const auto& node) -> SemanticType {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, NumberExpr>) {
				return makeNumberType();
			} else if constexpr (std::is_same_v<T, StringExpr>) {
				return makeStringType();
			} else if constexpr (std::is_same_v<T, BoolExpr>) {
				return makeBoolType();
			} else if constexpr (std::is_same_v<T, NullExpr>) {
				return makeNullType();
			} else if constexpr (std::is_same_v<T, VariableExpr>) {
				auto* symbol = lookupSymbol(node.name);
				if (symbol) {
					return symbol->type;
				}
				if (routines_.contains(node.name)) {
					report(node.location, "routine name used without call: " + node.name);
				} else {
					report(node.location, "unknown name: " + node.name);
				}
				return makeUnknownType();
			} else if constexpr (std::is_same_v<T, UnaryExpr>) {
				return analyzeUnaryExpr(node);
			} else if constexpr (std::is_same_v<T, BinaryExpr>) {
				return analyzeBinaryExpr(node);
			} else if constexpr (std::is_same_v<T, PipelineExpr>) {
				analyzeExpr(node.input);
				for (const auto& stage : node.stages) analyzeExpr(stage);
				return makeUnknownType();
			} else if constexpr (std::is_same_v<T, RangeExpr>) {
				if (node.start) analyzeExpr(node.start);
				if (node.end) analyzeExpr(node.end);
				if (node.step) analyzeExpr(node.step);
				return makeCustomType("range");
			} else if constexpr (std::is_same_v<T, ChooseExpr>) {
				return analyzeChooseExpr(node);
			} else if constexpr (std::is_same_v<T, CallExpr>) {
				return analyzeCallExpr(node);
			} else if constexpr (std::is_same_v<T, GroupExpr>) {
				return analyzeExpr(node.expression);
		} else if constexpr (std::is_same_v<T, PipelineExpr>) {
			analyzeExpr(node.input);
			for (const auto& stage : node.stages) analyzeExpr(stage);
			return makeUnknownType();
		} else if constexpr (std::is_same_v<T, RangeExpr>) {
			if (node.start) analyzeExpr(node.start);
			if (node.end) analyzeExpr(node.end);
			if (node.step) analyzeExpr(node.step);
			return makeCustomType("range");
		} else if constexpr (std::is_same_v<T, ChooseExpr>) {
			return analyzeChooseExpr(node);
			} else {
				return makeUnknownType();
			}
		}, expr->node);
	}

	void pushScope() {
		scopes_.push_back(SemanticScope{});
	}

	void popScope() {
		if (!scopes_.empty()) {
			scopes_.pop_back();
		}
	}

	SymbolInfo* lookupSymbol(const std::string& name) {
		for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
			auto symbolIt = it->symbols.find(name);
			if (symbolIt != it->symbols.end()) {
				return &symbolIt->second;
			}
		}
		return nullptr;
	}

	void declareSymbol(SourceLocation location, const std::string& name, SymbolInfo symbol) {
		if (scopes_.empty()) {
			pushScope();
		}
		auto& scope = scopes_.back();
		if (scope.symbols.contains(name)) {
			report(location, "duplicate local name: " + name);
			return;
		}
		scope.symbols.emplace(name, std::move(symbol));
	}

	SemanticType resolveRoutineCall(const std::string& name, const std::vector<ExprPtr>& arguments, SourceLocation location) {
		auto it = routines_.find(name);
		if (it == routines_.end()) {
			report(location, "unknown routine: " + name);
			for (const auto& argument : arguments) {
				analyzeExpr(argument);
			}
			return makeUnknownType();
		}

		std::vector<SemanticType> argumentTypes = collectArgumentTypes(arguments);

		std::vector<SemanticAnalyzer::Candidate> candidates = buildCandidates(it->second, argumentTypes);

		if (candidates.empty()) {
			report(location, formatOverloadMismatch(name, it->second, argumentTypes));
			return makeUnknownType();
		}

		std::sort(candidates.begin(), candidates.end(), [](const SemanticAnalyzer::Candidate& a, const SemanticAnalyzer::Candidate& b) { return a.score > b.score; });
		if (candidates.size() > 1 && candidates[0].score == candidates[1].score) {
			std::vector<RoutineSignature> candidateSignatures;
			std::vector<std::size_t> candidateScores;
			std::size_t limit = std::min<std::size_t>(candidates.size(), 3);
			for (std::size_t i = 0; i < limit; ++i) {
				candidateSignatures.push_back(*candidates[i].signature);
				candidateScores.push_back(candidates[i].score);
			}
			report(location, formatAmbiguousOverload(name, candidateSignatures, candidateScores));
			return candidates[0].signature->returnType;
		}

		return candidates.front().signature->returnType;
	}

	struct Candidate {
		const RoutineSignature* signature = nullptr;
		std::size_t score = 0;
	};

	bool sameSignature(const RoutineSignature& left, const RoutineSignature& right) const {
		if (left.parameterTypes.size() != right.parameterTypes.size()) {
			return false;
		}
		for (std::size_t i = 0; i < left.parameterTypes.size(); ++i) {
			if (left.parameterTypes[i] != right.parameterTypes[i]) {
				return false;
			}
		}
		return true;
	}

	const RoutineSignature* findSignatureByArity(const std::vector<RoutineSignature>& overloads, std::size_t parameterCount) const {
		for (const auto& signature : overloads) {
			if (signature.parameterTypes.size() == parameterCount) {
				return &signature;
			}
		}
		return nullptr;
	}

	std::vector<SemanticType> collectArgumentTypes(const std::vector<ExprPtr>& arguments) {
		std::vector<SemanticType> argumentTypes;
		argumentTypes.reserve(arguments.size());
		for (const auto& argument : arguments) {
			argumentTypes.push_back(analyzeExpr(argument));
		}
		return argumentTypes;
	}

	std::vector<SemanticAnalyzer::Candidate> buildCandidates(const std::vector<RoutineSignature>& overloads, const std::vector<SemanticType>& argumentTypes) {
		std::vector<SemanticAnalyzer::Candidate> candidates;
		for (const auto& signature : overloads) {
			if (signature.parameterTypes.size() != argumentTypes.size()) {
				continue;
			}
			bool compatible = true;
			std::size_t score = 0;
			for (std::size_t i = 0; i < argumentTypes.size(); ++i) {
				const auto& argType = argumentTypes[i];
				if (!isAssignableTo(signature.parameterTypes[i], argType)) {
					compatible = false;
					break;
				}
				if (argType == signature.parameterTypes[i]) {
					++score;
				}
			}
			if (compatible) {
				candidates.push_back(SemanticAnalyzer::Candidate{&signature, score});
			}
		}
		return candidates;
	}

	std::string formatSignature(const RoutineSignature& signature) const {
		std::ostringstream oss;
		oss << signature.name << '(';
		for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i) {
			if (i != 0) oss << ", ";
			oss << semanticTypeName(signature.parameterTypes[i]);
		}
		oss << ") -> " << semanticTypeName(signature.returnType);
		return oss.str();
	}

	std::string formatAmbiguousOverload(const std::string& name, const std::vector<RoutineSignature>& candidates, const std::vector<std::size_t>& scores) const {
		std::ostringstream oss;
		oss << "ambiguous overload resolution for routine '" << name << "': ";
		const std::size_t limit = std::min<std::size_t>(candidates.size(), 3);
		for (std::size_t i = 0; i < limit; ++i) {
			if (i != 0) oss << "; ";
			oss << formatSignature(candidates[i]);
			if (i < scores.size()) {
				oss << " [score " << scores[i] << "]";
			}
		}
		if (candidates.size() > limit) {
			oss << "; ...";
		}
		return oss.str();
	}

	std::string formatOverloadMismatch(const std::string& name, const std::vector<RoutineSignature>& candidates, const std::vector<SemanticType>& argumentTypes) const {
		std::ostringstream oss;
		oss << "no matching overload for routine '" << name << "' with arguments (";
		for (std::size_t i = 0; i < argumentTypes.size(); ++i) {
			if (i != 0) oss << ", ";
			oss << semanticTypeName(argumentTypes[i]);
		}
		oss << "). Candidates: ";
		const std::size_t limit = std::min<std::size_t>(candidates.size(), 3);
		for (std::size_t i = 0; i < limit; ++i) {
			if (i != 0) oss << "; ";
			oss << formatSignature(candidates[i]);
		}
		if (candidates.size() > limit) {
			oss << "; ...";
		}
		return oss.str();
	}

	void report(SourceLocation location, std::string message) {
		result_.diagnostics.push_back({location, std::move(message)});
	}

	void report(std::string message) {
		report({}, std::move(message));
	}

	SemanticAnalysisResult result_;
	std::unordered_map<std::string, std::vector<RoutineSignature>> routines_;
	std::vector<SemanticScope> scopes_;
	std::vector<std::unordered_set<std::string>> declarationScopes_;
	const Routine* currentRoutine_ = nullptr;
	SemanticType currentReturnType_ = makeVoidType();
};

// --- Frontend helpers: pretty-printing --------------------------------------
std::string indentText(std::size_t depth) {
	return std::string(depth * 4, ' ');
}

std::string exprToString(const ExprPtr& expr);
std::string stmtToString(const StmtPtr& stmt, std::size_t depth);

std::string exprToString(const ExprPtr& expr) {
	if (!expr) {
		return "<null>";
	}
	return std::visit([](const auto& node) -> std::string {
		using T = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<T, NumberExpr>) {
			return node.value;
		} else if constexpr (std::is_same_v<T, StringExpr>) {
			return '"' + node.value + '"';
		} else if constexpr (std::is_same_v<T, BoolExpr>) {
			return node.value ? "true" : "false";
		} else if constexpr (std::is_same_v<T, NullExpr>) {
			return "null";
		} else if constexpr (std::is_same_v<T, VariableExpr>) {
			return node.name;
		} else if constexpr (std::is_same_v<T, UnaryExpr>) {
			return "(" + node.op + " " + exprToString(node.operand) + ")";
		} else if constexpr (std::is_same_v<T, BinaryExpr>) {
			return "(" + exprToString(node.left) + " " + node.op + " " + exprToString(node.right) + ")";
		} else if constexpr (std::is_same_v<T, PipelineExpr>) {
			std::ostringstream oss;
			oss << exprToString(node.input);
			for (const auto& stage : node.stages) {
				oss << " -> " << exprToString(stage);
			}
			return oss.str();
		} else if constexpr (std::is_same_v<T, RangeExpr>) {
			return exprToString(node.start) + (node.inclusive ? ".." : "..<") + exprToString(node.end);
		} else if constexpr (std::is_same_v<T, ChooseExpr>) {
			std::ostringstream oss;
			oss << "choose";
			if (node.subject) oss << " " << exprToString(node.subject);
			for (const auto& choice : node.cases) {
				oss << "\n" << indentText(1) << "when " << exprToString(choice.condition) << "\n" << indentText(2) << exprToString(choice.value);
			}
			if (node.hasOtherwise) {
				oss << "\n" << indentText(1) << "otherwise\n" << indentText(2) << exprToString(node.otherwiseValue);
			}
			return oss.str();
		} else if constexpr (std::is_same_v<T, CallExpr>) {
			std::ostringstream oss;
			oss << exprToString(node.callee) << '(';
			for (std::size_t i = 0; i < node.arguments.size(); ++i) {
				if (i != 0) oss << ", ";
				oss << exprToString(node.arguments[i]);
			}
			oss << ')';
			return oss.str();
		} else if constexpr (std::is_same_v<T, GroupExpr>) {
			return "(" + exprToString(node.expression) + ")";
		} else if constexpr (std::is_same_v<T, PipelineExpr>) {
			return exprToString(node.input) + " -> <pipeline>";
		} else if constexpr (std::is_same_v<T, RangeExpr>) {
			return exprToString(node.start) + (node.inclusive ? ".." : "..<") + exprToString(node.end);
		} else if constexpr (std::is_same_v<T, ChooseExpr>) {
			return "choose <expr>";
		} else {
			return "<expr>";
		}
	}, expr->node);
}

std::string stmtToString(const StmtPtr& stmt, std::size_t depth) {
	if (!stmt) {
		return indentText(depth) + "<null>";
	}
	return std::visit([&](const auto& node) -> std::string {
		using T = std::decay_t<decltype(node)>;
		std::ostringstream oss;
		if constexpr (std::is_same_v<T, LetStmt>) {
			oss << indentText(depth) << "let " << node.name;
			if (!node.typeName.empty()) oss << ": " << node.typeName;
			oss << " = " << exprToString(node.initializer);
			return oss.str();
		} else if constexpr (std::is_same_v<T, VarStmt>) {
			oss << indentText(depth) << "var " << node.name;
			if (!node.typeName.empty()) oss << ": " << node.typeName;
			if (node.initializer) oss << " = " << exprToString(node.initializer);
			return oss.str();
		} else if constexpr (std::is_same_v<T, AssignStmt>) {
			oss << indentText(depth) << node.name << " = " << exprToString(node.value);
			return oss.str();
		} else if constexpr (std::is_same_v<T, ExprStmt>) {
			oss << indentText(depth) << exprToString(node.expression);
			return oss.str();
		} else if constexpr (std::is_same_v<T, ReturnStmt>) {
			oss << indentText(depth) << "return";
			if (node.hasValue) oss << " " << exprToString(node.value);
			return oss.str();
		} else if constexpr (std::is_same_v<T, GiveStmt>) {
			oss << indentText(depth) << "give " << exprToString(node.value);
			return oss.str();
		} else if constexpr (std::is_same_v<T, IfStmt>) {
			oss << indentText(depth) << "if " << exprToString(node.condition) << "\n";
			for (const auto& child : node.thenBranch) {
				oss << stmtToString(child, depth + 1) << "\n";
			}
			if (node.hasElse) {
				oss << indentText(depth) << "else\n";
				for (const auto& child : node.elseBranch) {
					oss << stmtToString(child, depth + 1) << "\n";
				}
			}
			std::string result = oss.str();
			if (!result.empty() && result.back() == '\n') result.pop_back();
			return result;
		} else if constexpr (std::is_same_v<T, WhileStmt>) {
			oss << indentText(depth) << "while " << exprToString(node.condition) << "\n";
			for (const auto& child : node.body) {
				oss << stmtToString(child, depth + 1) << "\n";
			}
			std::string result = oss.str();
			if (!result.empty() && result.back() == '\n') result.pop_back();
			return result;
		} else if constexpr (std::is_same_v<T, EachStmt>) {
			oss << indentText(depth) << "each ";
			for (std::size_t i = 0; i < node.bindings.size(); ++i) {
				if (i != 0) oss << ", ";
				oss << node.bindings[i];
			}
			oss << " in " << exprToString(node.range) << "\n";
			for (const auto& child : node.body) {
				oss << stmtToString(child, depth + 1) << "\n";
			}
			std::string result = oss.str();
			if (!result.empty() && result.back() == '\n') result.pop_back();
			return result;
		} else if constexpr (std::is_same_v<T, UnsafeStmt>) {
			oss << indentText(depth) << "unsafe\n";
			for (const auto& child : node.body) {
				oss << stmtToString(child, depth + 1) << "\n";
			}
			std::string result = oss.str();
			if (!result.empty() && result.back() == '\n') result.pop_back();
			return result;
		} else if constexpr (std::is_same_v<T, DeferStmt>) {
			oss << indentText(depth) << "defer " << exprToString(node.expression);
			return oss.str();
		} else if constexpr (std::is_same_v<T, FailStmt>) {
			oss << indentText(depth) << "fail " << exprToString(node.expression);
			return oss.str();
		} else if constexpr (std::is_same_v<T, BlockStmt>) {
			for (const auto& child : node.statements) {
				oss << stmtToString(child, depth) << "\n";
			}
			std::string result = oss.str();
			if (!result.empty() && result.back() == '\n') result.pop_back();
			return result;
		} else {
			return indentText(depth) + "<stmt>";
		}
	}, stmt->node);
}

std::string routineToString(const Routine& routine) {
	std::ostringstream oss;
	oss << "routine " << routine.name << '(';
	for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
		if (i != 0) oss << ", ";
		oss << routine.parameters[i].name << ": " << routine.parameters[i].typeName;
	}
	oss << ")";
	if (!routine.returnType.empty() && routine.returnType != "void") {
		oss << " gives " << routine.returnType;
	}
	for (const auto& stmt : routine.body) {
		oss << "\n" << stmtToString(stmt, 1);
	}
	return oss.str();
}

std::string programToString(const Program& program) {
	std::ostringstream oss;
	oss << "Parsed " << program.routines.size() << " routine(s).";
	for (const auto& routine : program.routines) {
		oss << "\n" << routineToString(routine);
	}
	return oss.str();
}

// --- IR lowerer -------------------------------------------------------------
class IRLowerer {
public:
	IRProgram lower(const Program& program) {
		IRProgram lowered;
		for (const auto& routine : program.routines) {
			lowered.functions.push_back(lowerRoutine(routine));
		}
		lowerDeclarationList(program.declarations, lowered.declarationSummaries);
		return lowered;
	}

private:
	static std::string joinPath(const std::vector<std::string>& path) {
		std::string text;
		for (std::size_t i = 0; i < path.size(); ++i) {
			if (i != 0) {
				text += "::";
			}
			text += path[i];
		}
		return text;
	}

	void lowerDeclarationList(const std::vector<Declaration>& declarations, std::vector<DeclarationInfo>& output, const std::string& scopePrefix = {}) {
		for (const auto& declaration : declarations) {
			lowerDeclaration(declaration, output, scopePrefix);
		}
	}

	void lowerDeclaration(const Declaration& declaration, std::vector<DeclarationInfo>& output, const std::string& scopePrefix) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			auto qualify = [&](const std::string& name) {
				if (scopePrefix.empty()) {
					return name;
				}
				return scopePrefix + "::" + name;
			};
			if constexpr (std::is_same_v<T, RecordDecl>) {
				output.push_back({qualify(node.name), "record", node.isPublic, node.isCompileTime, node.layout});
			} else if constexpr (std::is_same_v<T, EnumDecl>) {
				output.push_back({qualify(node.name), "enum", node.isPublic, false, node.underlyingType.empty() ? "unknown" : node.underlyingType});
			} else if constexpr (std::is_same_v<T, UnionDecl>) {
				output.push_back({qualify(node.name), "union", node.isPublic, false, node.layout});
			} else if constexpr (std::is_same_v<T, VariantDecl>) {
				output.push_back({qualify(node.name), "variant", node.isPublic, false, "cases=" + std::to_string(node.cases.size())});
			} else if constexpr (std::is_same_v<T, ModuleDecl>) {
				output.push_back({qualify(node.name), "module", node.isPublic, false, node.name});
				lowerDeclarationList(node.declarations, output, qualify(node.name));
			} else if constexpr (std::is_same_v<T, UseDecl>) {
				std::string alias = node.alias.empty() ? (node.path.empty() ? std::string{} : node.path.back()) : node.alias;
				output.push_back({qualify(alias), "use", false, false, joinPath(node.path)});
			} else if constexpr (std::is_same_v<T, ConstDecl>) {
				output.push_back({qualify(node.name), "const", node.isPublic, node.isCompileTime, node.typeName});
			} else if constexpr (std::is_same_v<T, ForeignDecl>) {
				output.push_back({qualify(node.routine.name), "foreign", node.routine.isPublic, node.routine.isCompileTime, node.abi});
			} else if constexpr (std::is_same_v<T, Routine>) {
				output.push_back({qualify(node.name), "routine", node.isPublic, node.isCompileTime, node.returnType});
			}
		}, declaration.node);
	}

	IRFunction lowerRoutine(const Routine& routine) {
		IRFunction function;
		function.name = routine.name;
		function.returnType = routine.returnType.empty() ? "void" : routine.returnType;
		function.valueType = function.returnType == "void" ? makeIRVoidType() : makeIRIntType();
		function.signature = buildSignature(routine);
		for (const auto& parameter : routine.parameters) {
			function.parameterNames.push_back(parameter.name);
			function.parameterTypes.push_back(parameter.typeName);
			function.parameterValueTypes.push_back(irValueTypeFromName(parameter.typeName));
			function.parameterSignature.push_back(parameter.name + ": " + parameter.typeName);
			function.parameterVregs.push_back(allocateVReg());
		}

		blocks_.clear();
		locals_.clear();
		localValueTypes_.clear();
		lastValue_ = VReg::None;
		lastValueType_ = makeIRUnknownType();
		nextVreg_ = 1;
		nextBlock_ = 1;
		currentBlock_ = createBlock("entry");
		currentTerminated_ = false;

		for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
			locals_[routine.parameters[i].name] = function.parameterVregs[i];
			localValueTypes_[routine.parameters[i].name] = function.parameterValueTypes[i];
		}

		lowerStatementList(routine.body);

		function.blocks = std::move(blocks_);
		function.totalVregs = nextVreg_ ? nextVreg_ - 1 : 0;
		return function;
	}

	std::string blockLabel(BlockID id) const {
		return "bb_" + std::to_string(static_cast<std::uint32_t>(id));
	}

	std::string buildSignature(const Routine& routine) const {
		std::ostringstream oss;
		oss << routine.name << '(';
		for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
			if (i != 0) oss << ", ";
			oss << routine.parameters[i].name << ": " << routine.parameters[i].typeName;
		}
		oss << ") -> " << (routine.returnType.empty() ? "void" : routine.returnType);
		return oss.str();
	}

	IRValueType binaryValueType(const std::string& op) const {
		if (op == "and" || op == "or" || op == "&&" || op == "||" || op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
			return makeIRBoolType();
		}
		if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
			return makeIRIntType();
		}
		return makeIRUnknownType();
	}

	std::string calleeSignature(const ExprPtr& callee) const {
		if (!callee) {
			return "<unknown>()";
		}
		if (const auto* variable = std::get_if<VariableExpr>(&callee->node)) {
			return variable->name + "()";
		}
		return "<expr>()";
	}

	IRValueType irValueTypeFromName(const std::string& name) const {
		if (name == "void") return makeIRVoidType();
		if (name == "bool") return makeIRBoolType();
		if (name == "number") return makeIRIntType();
		if (name == "string") return makeIRStringType();
		if (name == "null") return makeIRNullType();
		if (name.empty()) return makeIRUnknownType();
		return {IRValueKind::Function, name};
	}

	void lowerStatementList(const std::vector<StmtPtr>& statements) {
		for (const auto& statement : statements) {
			ensureActiveBlock();
			lowerStatement(statement);
		}
	}

	void lowerStatement(const StmtPtr& statement) {
		if (!statement) {
			return;
		}
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, LetStmt>) {
				lowerLet(node);
			} else if constexpr (std::is_same_v<T, VarStmt>) {
				lowerVar(node);
			} else if constexpr (std::is_same_v<T, AssignStmt>) {
				lowerAssign(node);
			} else if constexpr (std::is_same_v<T, ExprStmt>) {
				(void)lowerExpr(node.expression);
			} else if constexpr (std::is_same_v<T, ReturnStmt>) {
				lowerReturn(node);
			} else if constexpr (std::is_same_v<T, GiveStmt>) {
				lowerGive(node);
			} else if constexpr (std::is_same_v<T, IfStmt>) {
				lowerIf(node);
			} else if constexpr (std::is_same_v<T, EachStmt>) {
				lowerEach(node);
			} else if constexpr (std::is_same_v<T, UnsafeStmt>) {
				lowerUnsafe(node);
			} else if constexpr (std::is_same_v<T, DeferStmt>) {
				lowerDefer(node);
			} else if constexpr (std::is_same_v<T, FailStmt>) {
				lowerFail(node);
			} else if constexpr (std::is_same_v<T, WhileStmt>) {
				lowerWhile(node);
			} else if constexpr (std::is_same_v<T, BlockStmt>) {
				lowerStatementList(node.statements);
			}
		}, statement->node);
	}

	void lowerLet(const LetStmt& node) {
		auto value = node.initializer ? lowerExpr(node.initializer) : makeNullValue();
		locals_[node.name] = value;
		localValueTypes_[node.name] = lastValueType_;
		lastValue_ = value;
		lastValueType_ = localValueTypes_[node.name];
	}

	void lowerVar(const VarStmt& node) {
		auto value = node.initializer ? lowerExpr(node.initializer) : makeNullValue();
		locals_[node.name] = value;
		localValueTypes_[node.name] = lastValueType_;
		lastValue_ = value;
		lastValueType_ = localValueTypes_[node.name];
	}

	void lowerAssign(const AssignStmt& node) {
		auto value = lowerExpr(node.value);
		locals_[node.name] = value;
		localValueTypes_[node.name] = lastValueType_;
		lastValue_ = value;
		lastValueType_ = localValueTypes_[node.name];
	}

	void lowerReturn(const ReturnStmt& node) {
		IRInstruction inst;
		inst.op = IROpcode::Return;
		if (node.hasValue) {
			inst.args.push_back(lowerExpr(node.value));
			inst.valueType = lastValueType_;
		} else {
			inst.valueType = makeIRVoidType();
		}
		emit(std::move(inst));
		currentTerminated_ = true;
	}

	void lowerGive(const GiveStmt& node) {
		IRInstruction inst;
		inst.op = IROpcode::Return;
		inst.args.push_back(lowerExpr(node.value));
		inst.valueType = lastValueType_;
		emit(std::move(inst));
		currentTerminated_ = true;
	}

	void lowerIf(const IfStmt& node) {
		BlockID thenBlock = createBlock("if.then");
		BlockID elseBlock = node.hasElse ? createBlock("if.else") : createBlock("if.merge");
		BlockID mergeBlock = node.hasElse ? createBlock("if.merge") : elseBlock;
		auto conditionValue = lowerExpr(node.condition);

		IRInstruction branch;
		branch.op = IROpcode::BranchIf;
		branch.args.push_back(conditionValue);
		branch.targetBlocks.push_back(thenBlock);
		branch.targetBlocks.push_back(elseBlock);
		emit(std::move(branch));
		currentTerminated_ = true;

		currentBlock_ = thenBlock;
		currentTerminated_ = false;
		lastValue_ = VReg::None;
		lowerStatementList(node.thenBranch);
		if (!currentTerminated_) {
			emitBranch(mergeBlock);
		}
		appendPhiInput(mergeBlock, thenBlock, lastValue_);

		if (node.hasElse) {
			currentBlock_ = elseBlock;
			currentTerminated_ = false;
			lastValue_ = VReg::None;
			lowerStatementList(node.elseBranch);
			if (!currentTerminated_) {
				emitBranch(mergeBlock);
			}
			appendPhiInput(mergeBlock, elseBlock, lastValue_);
		}

		currentBlock_ = mergeBlock;
		currentTerminated_ = false;
	}

	void lowerWhile(const WhileStmt& node) {
		BlockID condBlock = currentBlock_;
		BlockID bodyBlock = createBlock("while.body");
		BlockID exitBlock = createBlock("while.exit");
		auto conditionValue = lowerExpr(node.condition);

		IRInstruction branch;
		branch.op = IROpcode::BranchIf;
		branch.args.push_back(conditionValue);
		branch.targetBlocks.push_back(bodyBlock);
		branch.targetBlocks.push_back(exitBlock);
		emit(std::move(branch));
		currentTerminated_ = true;

		currentBlock_ = bodyBlock;
		currentTerminated_ = false;
		lastValue_ = VReg::None;
		lowerStatementList(node.body);
		if (!currentTerminated_) {
			appendPhiInput(condBlock, bodyBlock, lastValue_);
			emitBranch(condBlock);
		}

		currentBlock_ = exitBlock;
		currentTerminated_ = false;
	}

	void lowerEach(const EachStmt& node) {
		for (const auto& binding : node.bindings) {
			locals_[binding] = allocateVReg();
			localValueTypes_[binding] = makeIRIntType();
		}
		lowerExpr(node.range);
		BlockID loopBlock = createBlock("each.loop");
		BlockID bodyBlock = createBlock("each.body");
		BlockID exitBlock = createBlock("each.exit");
		emitBranch(loopBlock);
		currentBlock_ = loopBlock;
		currentTerminated_ = false;
		emitBranch(bodyBlock);
		currentBlock_ = bodyBlock;
		currentTerminated_ = false;
		lowerStatementList(node.body);
		if (!currentTerminated_) {
			emitBranch(loopBlock);
		}
		currentBlock_ = exitBlock;
		currentTerminated_ = false;
	}

	void lowerUnsafe(const UnsafeStmt& node) {
		lowerStatementList(node.body);
	}

	void lowerDefer(const DeferStmt& node) {
		(void)lowerExpr(node.expression);
	}

	void lowerFail(const FailStmt& node) {
		IRInstruction inst;
		inst.op = IROpcode::Return;
		if (node.expression) {
			inst.args.push_back(lowerExpr(node.expression));
			inst.valueType = lastValueType_;
		} else {
			inst.valueType = makeIRVoidType();
		}
		emit(std::move(inst));
		currentTerminated_ = true;
	}

	VReg lowerChoose(const ChooseExpr& node) {
		IRInstruction inst;
		inst.dest = allocateVReg();
		inst.op = IROpcode::Phi;
		inst.valueType = makeIRUnknownType();
		if (node.subject) {
			(void)lowerExpr(node.subject);
		}
		for (const auto& choice : node.cases) {
			(void)lowerExpr(choice.condition);
			inst.phiInputs.emplace_back(BlockID::None, lowerExpr(choice.value));
		}
		if (node.hasOtherwise) {
			inst.phiInputs.emplace_back(BlockID::None, lowerExpr(node.otherwiseValue));
		}
		emit(std::move(inst));
		return inst.dest;
	}

	void appendPhiInput(BlockID target, BlockID source, VReg value) {
		if (target == BlockID::None || source == BlockID::None || value == VReg::None) {
			return;
		}
		auto& block = blocks_[static_cast<std::size_t>(static_cast<std::uint32_t>(target) - 1)];
		block.predecessors.push_back(blockLabel(source));
		IRInstruction phi;
		phi.op = IROpcode::Phi;
		phi.dest = allocateVReg();
		phi.phiInputs.emplace_back(source, value);
		block.phiNodes.push_back(std::move(phi));
		lastValue_ = value;
	}

	VReg lowerExpr(const ExprPtr& expr) {
		if (!expr) {
			lastValueType_ = makeIRUnknownType();
			return makeNullValue();
		}
		VReg value = std::visit([&](const auto& node) -> VReg {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, NumberExpr>) {
				lastValueType_ = makeIRIntType();
				return emitConst(IROpcode::ConstInt, node.value);
			} else if constexpr (std::is_same_v<T, StringExpr>) {
				lastValueType_ = makeIRStringType();
				return emitConst(IROpcode::ConstString, node.value);
			} else if constexpr (std::is_same_v<T, BoolExpr>) {
				lastValueType_ = makeIRBoolType();
				return emitBool(node.value);
			} else if constexpr (std::is_same_v<T, NullExpr>) {
				lastValueType_ = makeIRNullType();
				return makeNullValue();
			} else if constexpr (std::is_same_v<T, VariableExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerVariable(node.name);
			} else if constexpr (std::is_same_v<T, UnaryExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerUnary(node);
			} else if constexpr (std::is_same_v<T, BinaryExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerBinary(node);
			} else if constexpr (std::is_same_v<T, CallExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerCall(node);
			} else if constexpr (std::is_same_v<T, PipelineExpr>) {
				lastValueType_ = lowerExprType(expr);
				VReg current = lowerExpr(node.input);
				for (const auto& stage : node.stages) {
					if (stage) {
						current = lowerExpr(stage);
					}
				}
				return current;
			} else if constexpr (std::is_same_v<T, RangeExpr>) {
				lastValueType_ = makeIRUnknownType();
				return makeNullValue();
			} else if constexpr (std::is_same_v<T, ChooseExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerChoose(node);
			} else if constexpr (std::is_same_v<T, GroupExpr>) {
				lastValueType_ = lowerExprType(expr);
				return lowerExpr(node.expression);
			} else {
				lastValueType_ = makeIRUnknownType();
				return makeNullValue();
			}
		}, expr->node);
		lastValue_ = value;
		return value;
	}

	IRValueType lowerExprType(const ExprPtr& expr) const {
		if (!expr) {
			return makeIRUnknownType();
		}
		return std::visit([&](const auto& node) -> IRValueType {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, BoolExpr>) {
				return makeIRBoolType();
			} else if constexpr (std::is_same_v<T, NumberExpr>) {
				return makeIRIntType();
			} else if constexpr (std::is_same_v<T, StringExpr>) {
				return makeIRStringType();
			} else if constexpr (std::is_same_v<T, NullExpr>) {
				return makeIRNullType();
			} else if constexpr (std::is_same_v<T, VariableExpr>) {
				auto it = localValueTypes_.find(node.name);
				return it != localValueTypes_.end() ? it->second : makeIRUnknownType();
			} else if constexpr (std::is_same_v<T, UnaryExpr>) {
				return node.op == "!" || node.op == "not" ? makeIRBoolType() : makeIRIntType();
			} else if constexpr (std::is_same_v<T, BinaryExpr>) {
				return binaryValueType(node.op);
			} else if constexpr (std::is_same_v<T, CallExpr>) {
				return node.callee ? makeIRIntType() : makeIRUnknownType();
			} else if constexpr (std::is_same_v<T, PipelineExpr>) {
				return makeIRUnknownType();
			} else if constexpr (std::is_same_v<T, RangeExpr>) {
				return makeIRUnknownType();
			} else if constexpr (std::is_same_v<T, ChooseExpr>) {
				return makeIRUnknownType();
			} else {
				return makeIRUnknownType();
			}
		}, expr->node);
	}

	VReg lowerVariable(const std::string& name) {
		auto it = locals_.find(name);
		if (it != locals_.end()) {
			return it->second;
		}
		VReg value = allocateVReg();
		locals_[name] = value;
		return value;
	}

	VReg lowerUnary(const UnaryExpr& node) {
		IRInstruction inst;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.valueType = node.op == "!" || node.op == "not" ? makeIRBoolType() : makeIRIntType();
		inst.args.push_back(lowerExpr(node.operand));
		if (node.op == "-") {
			inst.op = IROpcode::Neg;
		} else if (node.op == "!" || node.op == "not") {
			inst.op = IROpcode::Not;
		} else {
			inst.op = IROpcode::Nop;
		}
		emit(std::move(inst));
		lastValueType_ = inst.valueType;
		return dest;
	}

	VReg lowerBinary(const BinaryExpr& node) {
		IRInstruction inst;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.valueType = binaryValueType(node.op);
		inst.args.push_back(lowerExpr(node.left));
		inst.args.push_back(lowerExpr(node.right));
		if (node.op == "+") inst.op = IROpcode::Add;
		else if (node.op == "-") inst.op = IROpcode::Sub;
		else if (node.op == "*") inst.op = IROpcode::Mul;
		else if (node.op == "/") inst.op = IROpcode::Div;
		else if (node.op == "and" || node.op == "&&") inst.op = IROpcode::And;
		else if (node.op == "or" || node.op == "||") inst.op = IROpcode::Or;
		else if (node.op == "==") inst.op = IROpcode::Eq;
		else if (node.op == "!=") inst.op = IROpcode::Ne;
		else if (node.op == "<") inst.op = IROpcode::Lt;
		else if (node.op == "<=") inst.op = IROpcode::Le;
		else if (node.op == ">") inst.op = IROpcode::Gt;
		else if (node.op == ">=") inst.op = IROpcode::Ge;
		else inst.op = IROpcode::Nop;
		emit(std::move(inst));
		lastValueType_ = inst.valueType;
		return dest;
	}

	VReg lowerCall(const CallExpr& node) {
		IRInstruction inst;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.op = IROpcode::Call;
		inst.callee = calleeName(node.callee);
		inst.returnType = callReturnType(node.callee);
		inst.typeName = inst.returnType;
		inst.callSignature = calleeSignature(node.callee);
		inst.valueType = irValueTypeFromName(inst.returnType);
		for (const auto& argument : node.arguments) {
			auto argumentValue = lowerExpr(argument);
			inst.args.push_back(argumentValue);
			inst.argumentTypes.push_back(argumentTypeName(argument));
			inst.argumentValueTypes.push_back(lastValueType_);
		}
		emit(std::move(inst));
		lastValueType_ = inst.valueType;
		return dest;
	}

	std::string callReturnType(const ExprPtr& callee) {
		if (!callee) {
			return "unknown";
		}
		if (const auto* variable = std::get_if<VariableExpr>(&callee->node)) {
			auto it = locals_.find(variable->name);
			if (it != locals_.end()) {
				return "value";
			}
			if (variable->name == "print") {
				return "void";
			}
		}
		return "value";
	}

	std::string argumentTypeName(const ExprPtr& expr) const {
		if (!expr) {
			return "unknown";
		}
		return std::visit([&](const auto& node) -> std::string {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, BoolExpr>) {
				return "bool";
			} else if constexpr (std::is_same_v<T, NumberExpr>) {
				return "number";
			} else if constexpr (std::is_same_v<T, StringExpr>) {
				return "string";
			} else if constexpr (std::is_same_v<T, NullExpr>) {
				return "null";
			} else {
				return "value";
			}
		}, expr->node);
	}

	std::string calleeName(const ExprPtr& callee) {
		if (!callee) {
			return "<unknown>";
		}
		if (const auto* variable = std::get_if<VariableExpr>(&callee->node)) {
			return variable->name;
		}
		return "<expr>";
	}

	VReg emitConst(IROpcode op, const std::string& text) {
		IRInstruction inst;
		inst.op = op;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.literalText = text;
		inst.valueType = op == IROpcode::ConstString ? makeIRStringType() : makeIRIntType();
		emit(std::move(inst));
		return dest;
	}

	VReg emitBool(bool value) {
		IRInstruction inst;
		inst.op = IROpcode::ConstBool;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.valueType = makeIRBoolType();
		inst.hasImmediate = true;
		inst.immediate = value ? 1 : 0;
		emit(std::move(inst));
		return dest;
	}

	VReg makeNullValue() {
		IRInstruction inst;
		inst.op = IROpcode::ConstNull;
		auto dest = allocateVReg();
		inst.dest = dest;
		inst.valueType = makeIRNullType();
		emit(std::move(inst));
		return dest;
	}

	void emitBranch(BlockID target) {
		IRInstruction inst;
		inst.op = IROpcode::Branch;
		inst.targetBlocks.push_back(target);
		inst.valueType = makeIRVoidType();
		emit(std::move(inst));
		currentTerminated_ = true;
	}

	void emit(IRInstruction inst) {
		ensureActiveBlock();
		inst.blockLabel = currentBlock().labelName;
		currentBlock().instructions.push_back(std::move(inst));
	}

	IRBasicBlock& currentBlock() {
		return blocks_[static_cast<std::size_t>(static_cast<std::uint32_t>(currentBlock_) - 1)];
	}

	void ensureActiveBlock() {
		if (currentBlock_ == BlockID::None) {
			currentBlock_ = createBlock("block");
			currentTerminated_ = false;
		}
		if (currentTerminated_) {
			currentBlock_ = createBlock("dead");
			currentTerminated_ = false;
		}
	}

	BlockID createBlock(const std::string& label) {
		BlockID id = static_cast<BlockID>(nextBlock_++);
		blocks_.push_back({id, blockLabel(id) + "_" + label, makeIRVoidType(), {}, {}, {}});
		return id;
	}

	VReg allocateVReg() {
		return static_cast<VReg>(nextVreg_++);
	}

	std::vector<IRBasicBlock> blocks_;
	std::unordered_map<std::string, VReg> locals_;
	std::unordered_map<std::string, IRValueType> localValueTypes_;
	VReg lastValue_ = VReg::None;
	IRValueType lastValueType_ = makeIRUnknownType();
	std::uint32_t nextVreg_ = 1;
	std::uint32_t nextBlock_ = 1;
	BlockID currentBlock_ = BlockID::None;
	bool currentTerminated_ = false;
};

IRProgram lowerProgramToIR(const Program& program) {
	IRLowerer lowerer;
	return lowerer.lower(program);
}

void printSemanticDiagnostics(const SemanticAnalysisResult& analysis) {
	for (const auto& diagnostic : analysis.diagnostics) {
		std::cerr << "semantic error";
		if (diagnostic.location.line != 0 || diagnostic.location.column != 0) {
			std::cerr << " at " << diagnostic.location.line << ":" << diagnostic.location.column;
		}
		std::cerr << ": " << diagnostic.message << "\n";
	}
}

// --- IR emitter -------------------------------------------------------------
class CppIREmitter {
public:
	std::string emit(const IRProgram& program) const {
		std::ostringstream oss;
		oss << "#include <cstdint>\n\n";
		for (const auto& decl : program.declarationSummaries) {
			emitDeclarationSummary(oss, decl);
		}
		for (const auto& function : program.functions) {
			emitFunction(oss, function);
		}
		oss << "int main() { return 0; }\n";
		return oss.str();
	}

private:
	void emitDeclarationSummary(std::ostringstream& oss, const DeclarationInfo& decl) const {
		oss << "// " << decl.kind << ": " << decl.name;
		if (!decl.metadata.empty()) {
			oss << " // " << decl.metadata;
		}
		oss << "\n";
	}

	void emitFunction(std::ostringstream& oss, const IRFunction& function) const {
		oss << "// " << function.signature << "\n";
		oss << "static " << function.returnType << ' ' << function.name << "() {\n";
		oss << "    /* IR value type: " << irValueTypeName(function.valueType) << " */\n";
		for (std::uint32_t i = 1; i <= function.totalVregs; ++i) {
			oss << "    std::int64_t v" << i << " = 0;\n";
		}
		if (!function.parameterSignature.empty()) {
			oss << "    /* parameters: ";
			for (std::size_t i = 0; i < function.parameterSignature.size(); ++i) {
				if (i != 0) oss << ", ";
				oss << function.parameterSignature[i];
			}
			oss << " */\n";
		}
		for (const auto& block : function.blocks) {
			emitBlock(oss, block);
		}
		if (function.returnType == "void") {
			oss << "    return;\n";
		}
		oss << "}\n\n";
	}

	void emitBlock(std::ostringstream& oss, const IRBasicBlock& block) const {
		oss << block.labelName << ":\n";
		oss << "    /* terminator: " << irValueTypeName(block.terminatorType) << " */\n";
		if (!block.predecessors.empty()) {
			oss << "    /* predecessors: ";
			for (std::size_t i = 0; i < block.predecessors.size(); ++i) {
				if (i != 0) oss << ", ";
				oss << block.predecessors[i];
			}
			oss << " */\n";
		}
		for (const auto& phi : block.phiNodes) {
			emitInstruction(oss, phi);
		}
		for (const auto& inst : block.instructions) {
			emitInstruction(oss, inst);
		}
		if (block.instructions.empty() && block.terminatorType.kind == IRValueKind::Void) {
			oss << "    /* empty block */\n";
		}
	}

	void emitInstruction(std::ostringstream& oss, const IRInstruction& inst) const {
		switch (inst.op) {
		case IROpcode::ConstInt:
			oss << "    v" << reg(inst.dest) << " = " << literalText(inst) << ";\n";
			break;
		case IROpcode::ConstString:
			oss << "    /* string literal: ";
			for (char ch : inst.literalText) {
				oss << (ch == '\n' ? ' ' : ch);
			}
			oss << " */\n";
			break;
		case IROpcode::ConstBool:
			oss << "    v" << reg(inst.dest) << " = " << (inst.hasImmediate && inst.immediate ? 1 : 0) << ";\n";
			break;
		case IROpcode::ConstNull:
			oss << "    v" << reg(inst.dest) << " = 0;\n";
			break;
		case IROpcode::Copy:
			oss << "    v" << reg(inst.dest) << " = v" << reg(inst.args[0]) << ";\n";
			break;
		case IROpcode::Add:
			emitBinary(oss, inst, "+");
			break;
		case IROpcode::Sub:
			emitBinary(oss, inst, "-");
			break;
		case IROpcode::Mul:
			emitBinary(oss, inst, "*");
			break;
		case IROpcode::Div:
			emitBinary(oss, inst, "/");
			break;
		case IROpcode::Neg:
			oss << "    v" << reg(inst.dest) << " = -v" << reg(inst.args[0]) << ";\n";
			break;
		case IROpcode::Not:
			oss << "    v" << reg(inst.dest) << " = !v" << reg(inst.args[0]) << ";\n";
			break;
		case IROpcode::Eq:
			emitBinary(oss, inst, "==");
			break;
		case IROpcode::Ne:
			emitBinary(oss, inst, "!=");
			break;
		case IROpcode::Lt:
			emitBinary(oss, inst, "<");
			break;
		case IROpcode::Le:
			emitBinary(oss, inst, "<=");
			break;
		case IROpcode::Gt:
			emitBinary(oss, inst, ">");
			break;
		case IROpcode::Ge:
			emitBinary(oss, inst, ">=");
			break;
		case IROpcode::And:
			emitBinary(oss, inst, "&&");
			break;
		case IROpcode::Or:
			emitBinary(oss, inst, "||");
			break;
		case IROpcode::Phi:
			oss << "    v" << reg(inst.dest) << " = ";
			for (std::size_t i = 0; i < inst.phiInputs.size(); ++i) {
				if (i != 0) oss << " /* or */ ";
				oss << "v" << reg(inst.phiInputs[i].second);
			}
			oss << ";\n";
			break;
		case IROpcode::Branch:
			oss << "    goto " << blockLabel(inst.targetBlocks.front()) << ";\n";
			break;
		case IROpcode::BranchIf:
			oss << "    if (v" << reg(inst.args[0]) << ") goto " << blockLabel(inst.targetBlocks[0]) << "; else goto " << blockLabel(inst.targetBlocks[1]) << ";\n";
			break;
		case IROpcode::Call:
			emitCall(oss, inst);
			break;
		case IROpcode::Return:
			if (!inst.args.empty()) {
				oss << "    return v" << reg(inst.args[0]) << ";\n";
			} else {
				oss << "    return;\n";
			}
			oss << "    /* returns " << irValueTypeName(inst.valueType) << " */\n";
			break;
		case IROpcode::Nop:
			oss << "    /* nop */\n";
			break;
		}
	}

	void emitBinary(std::ostringstream& oss, const IRInstruction& inst, const char* op) const {
		oss << "    v" << reg(inst.dest) << " = v" << reg(inst.args[0]) << ' ' << op << ' ' << "v" << reg(inst.args[1]) << ";\n";
	}

	void emitCall(std::ostringstream& oss, const IRInstruction& inst) const {
		oss << "    /* call " << inst.callSignature << " -> " << irValueTypeName(inst.valueType) << " */\n";
		if (inst.returnType == "void") {
			oss << "    " << inst.callee << "(";
		} else {
			oss << "    v" << reg(inst.dest) << " = " << inst.callee << "(";
		}
		for (std::size_t i = 0; i < inst.args.size(); ++i) {
			if (i != 0) oss << ", ";
			oss << "v" << reg(inst.args[i]);
		}
		oss << ");\n";
	}

	std::uint32_t reg(VReg reg) const {
		return static_cast<std::uint32_t>(reg);
	}

	std::string blockLabel(BlockID block) const {
		return "bb_" + std::to_string(static_cast<std::uint32_t>(block));
	}

	std::string literalText(const IRInstruction& inst) const {
		return inst.literalText.empty() ? std::to_string(inst.immediate) : inst.literalText;
	}
};

// --- PE/COFF backend --------------------------------------------------------
enum class PECOFFOutputKind {
	ObjectFile,
	Executable
};

struct PECOFFSection {
	std::string name;
	std::uint32_t characteristics = 0;
	std::vector<std::uint8_t> data;
	std::uint32_t virtualAddress = 0;
	std::uint32_t virtualSize = 0;
	std::uint32_t rawPointer = 0;
	std::uint32_t rawSize = 0;
};

struct PECOFFImage {
	std::vector<PECOFFSection> sections;
	std::vector<std::string> functionNames;
	std::vector<std::string> functionSignatures;
	std::vector<std::string> typeNames;
	std::uint32_t entryPointRva = 0;
	std::uint32_t entrySectionIndex = 0;
	std::uint64_t imageBase = 0x140000000ull;
	std::uint32_t fileAlignment = 0x200;
	std::uint32_t sectionAlignment = 0x1000;
	std::uint16_t machine = 0x8664;
};

class ByteBuffer {
public:
	void writeU8(std::uint8_t value) {
		bytes_.push_back(value);
	}

	void writeU16(std::uint16_t value) {
		writeIntegral(value);
	}

	void writeU32(std::uint32_t value) {
		writeIntegral(value);
	}

	void writeU64(std::uint64_t value) {
		writeIntegral(value);
	}

	void writeBytes(const std::vector<std::uint8_t>& data) {
		bytes_.insert(bytes_.end(), data.begin(), data.end());
	}

	void writeBytes(const void* data, std::size_t size) {
		const auto* ptr = static_cast<const std::uint8_t*>(data);
		bytes_.insert(bytes_.end(), ptr, ptr + size);
	}

	void writeString(const std::string& text) {
		writeBytes(text.data(), text.size());
	}

	void writeStringZ(const std::string& text) {
		writeString(text);
		writeU8(0);
	}

	void align(std::size_t alignment, std::uint8_t fill = 0) {
		if (alignment == 0) {
			return;
		}
		const std::size_t remainder = bytes_.size() % alignment;
		if (remainder == 0) {
			return;
		}
		bytes_.insert(bytes_.end(), alignment - remainder, fill);
	}

	void patchU16(std::size_t offset, std::uint16_t value) {
		patchIntegral(offset, value);
	}

	void patchU32(std::size_t offset, std::uint32_t value) {
		patchIntegral(offset, value);
	}

	void patchU64(std::size_t offset, std::uint64_t value) {
		patchIntegral(offset, value);
	}

	std::size_t size() const {
		return bytes_.size();
	}

	const std::vector<std::uint8_t>& bytes() const {
		return bytes_;
	}

	std::vector<std::uint8_t> release() {
		return std::move(bytes_);
	}

private:
	template <typename T>
	void writeIntegral(T value) {
		static_assert(std::is_integral_v<T>);
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			bytes_.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(value) >> (i * 8)) & 0xffu));
		}
	}

	template <typename T>
	void patchIntegral(std::size_t offset, T value) {
		static_assert(std::is_integral_v<T>);
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			bytes_[offset + i] = static_cast<std::uint8_t>((static_cast<std::uint64_t>(value) >> (i * 8)) & 0xffu);
		}
	}

	std::vector<std::uint8_t> bytes_;
};

class PECOFFGenerator {
public:
	std::vector<std::uint8_t> emitObject(const IRProgram& program) const {
		const auto image = buildImage(program);
		return emitCOFFObject(image);
	}

	std::vector<std::uint8_t> emitExecutable(const IRProgram& program) const {
		const auto image = buildImage(program);
		return emitPEImage(image);
	}

private:
	PECOFFImage buildImage(const IRProgram& program) const {
		PECOFFImage image;

		PECOFFSection text;
		text.name = ".text";
		text.characteristics = 0x60000020u;
		text.data = {0x31, 0xC0, 0xC3};
		text.virtualSize = static_cast<std::uint32_t>(text.data.size());

		PECOFFSection rdata;
		rdata.name = ".rdata";
		rdata.characteristics = 0x40000040u;
		rdata.data = buildMetadataBlob(program);
		rdata.virtualSize = static_cast<std::uint32_t>(rdata.data.size());

		PECOFFSection data;
		data.name = ".data";
		data.characteristics = 0xC0000040u;
		data.data = buildSummaryBlob(program);
		data.virtualSize = static_cast<std::uint32_t>(data.data.size());

		PECOFFSection decl;
		decl.name = ".decl";
		decl.characteristics = 0x40000040u;
		decl.data = buildDeclarationBlob(program);
		decl.virtualSize = static_cast<std::uint32_t>(decl.data.size());

		image.sections.push_back(std::move(text));
		image.sections.push_back(std::move(rdata));
		image.sections.push_back(std::move(data));
		image.sections.push_back(std::move(decl));
		image.entrySectionIndex = 1;
		image.entryPointRva = image.sectionAlignment;

		for (const auto& function : program.functions) {
			image.functionNames.push_back(function.name);
			image.functionSignatures.push_back(function.signature);
			image.typeNames.push_back(function.returnType);
		}
		for (const auto& decl : program.declarationSummaries) {
			image.functionNames.push_back(decl.name);
			image.functionSignatures.push_back(decl.kind + ":" + decl.name);
			image.typeNames.push_back(decl.metadata);
		}

		return image;
	}

	std::vector<std::uint8_t> buildMetadataBlob(const IRProgram& program) const {
		ByteBuffer buffer;
		buffer.writeStringZ("paracine-pe-coff");
		buffer.writeU32(static_cast<std::uint32_t>(program.declarationSummaries.size()));
		for (const auto& decl : program.declarationSummaries) {
			buffer.writeStringZ(decl.kind);
			buffer.writeStringZ(decl.name);
			buffer.writeStringZ(decl.metadata);
			buffer.writeU8(decl.isPublic ? 1 : 0);
			buffer.writeU8(decl.isCompileTime ? 1 : 0);
		}
		for (const auto& function : program.functions) {
			buffer.writeStringZ(function.signature);
			buffer.writeStringZ(function.returnType);
			buffer.writeStringZ(function.name);
			buffer.writeU32(function.totalVregs);
			buffer.writeU32(static_cast<std::uint32_t>(function.blocks.size()));
		}
		buffer.align(8);
		return buffer.release();
	}

	std::vector<std::uint8_t> buildSummaryBlob(const IRProgram& program) const {
		ByteBuffer buffer;
		buffer.writeU32(static_cast<std::uint32_t>(program.declarationSummaries.size()));
		for (const auto& decl : program.declarationSummaries) {
			buffer.writeU32(static_cast<std::uint32_t>(decl.kind.size()));
			buffer.writeU32(static_cast<std::uint32_t>(decl.name.size()));
			buffer.writeU32(static_cast<std::uint32_t>(decl.metadata.size()));
		}
		buffer.writeU32(static_cast<std::uint32_t>(program.functions.size()));
		for (const auto& function : program.functions) {
			buffer.writeU32(static_cast<std::uint32_t>(function.parameterNames.size()));
			buffer.writeU32(static_cast<std::uint32_t>(function.blocks.size()));
			buffer.writeU32(function.totalVregs);
		}
		buffer.align(4);
		return buffer.release();
	}

	std::vector<std::uint8_t> buildDeclarationBlob(const IRProgram& program) const {
		ByteBuffer buffer;
		for (const auto& decl : program.declarationSummaries) {
			buffer.writeStringZ(decl.kind);
			buffer.writeStringZ(decl.name);
			buffer.writeStringZ(decl.metadata);
			buffer.writeU8(decl.isPublic ? 1 : 0);
			buffer.writeU8(decl.isCompileTime ? 1 : 0);
		}
		buffer.align(8);
		return buffer.release();
	}

	std::vector<std::uint8_t> emitCOFFObject(const PECOFFImage& image) const {
		ByteBuffer buffer;
		const std::uint32_t machine = image.machine;
		const std::uint16_t sectionCount = static_cast<std::uint16_t>(image.sections.size());
		const std::uint16_t optionalHeaderSize = 0;
		const std::size_t fileHeaderOffset = buffer.size();
		buffer.writeU16(machine);
		buffer.writeU16(sectionCount);
		buffer.writeU32(0);
		buffer.writeU32(0);
		const std::size_t symbolTablePointerOffset = buffer.size();
		buffer.writeU32(0);
		const std::size_t symbolCountOffset = buffer.size();
		buffer.writeU32(0);
		buffer.writeU16(optionalHeaderSize);
		buffer.writeU16(0);

		struct COFFSectionInfo {
			std::size_t headerOffset = 0;
			std::size_t rawPointerOffset = 0;
		};
		std::vector<COFFSectionInfo> sectionInfo(image.sections.size());
		for (std::size_t i = 0; i < image.sections.size(); ++i) {
			const auto& section = image.sections[i];
			sectionInfo[i].headerOffset = buffer.size();
			writeSectionHeader(buffer, section, 0, static_cast<std::uint32_t>(section.data.size()));
		}

		for (std::size_t i = 0; i < image.sections.size(); ++i) {
			buffer.align(4);
			const auto rawPointer = static_cast<std::uint32_t>(buffer.size());
			sectionInfo[i].rawPointerOffset = rawPointer;
			buffer.writeBytes(image.sections[i].data);
			buffer.align(4);
			patchSectionRawPointer(buffer, sectionInfo[i].headerOffset, rawPointer, static_cast<std::uint32_t>(image.sections[i].data.size()));
		}

		const std::size_t symbolTableOffset = buffer.size();
		std::vector<std::pair<std::string, std::uint32_t>> stringTableEntries;
		std::size_t symbolCount = 0;
		for (std::size_t i = 0; i < image.sections.size(); ++i) {
			writeSymbol(buffer, image.sections[i].name, 0, static_cast<std::int16_t>(i + 1), 3, 0, stringTableEntries);
			++symbolCount;
		}
		for (const auto& name : image.functionNames) {
			writeSymbol(buffer, name, 0, 1, 2, 0, stringTableEntries);
			++symbolCount;
		}

		ByteBuffer stringTable;
		std::uint32_t stringTableSize = 4;
		for (const auto& entry : stringTableEntries) {
			stringTableSize += static_cast<std::uint32_t>(entry.first.size() + 1);
		}
		stringTable.writeU32(stringTableSize);
		for (const auto& entry : stringTableEntries) {
			stringTable.writeStringZ(entry.first);
		}

		buffer.writeBytes(stringTable.bytes());
		buffer.patchU32(symbolTablePointerOffset, static_cast<std::uint32_t>(symbolTableOffset));
		buffer.patchU32(symbolCountOffset, static_cast<std::uint32_t>(symbolCount));
		return buffer.release();
	}

	std::vector<std::uint8_t> emitPEImage(PECOFFImage image) const {
		constexpr std::uint32_t fileAlignment = 0x200;
		constexpr std::uint32_t sectionAlignment = 0x1000;
		image.fileAlignment = fileAlignment;
		image.sectionAlignment = sectionAlignment;

		const std::size_t sectionCount = image.sections.size();
		std::uint32_t headersSize = 0x80 + 4 + 20 + 240 + static_cast<std::uint32_t>(sectionCount * 40);
		headersSize = alignUp(headersSize, fileAlignment);

		std::uint32_t virtualAddress = sectionAlignment;
		std::uint32_t rawPointer = headersSize;
		for (auto& section : image.sections) {
			section.virtualAddress = virtualAddress;
			section.rawPointer = rawPointer;
			section.rawSize = alignUp(static_cast<std::uint32_t>(section.data.size()), fileAlignment);
			section.virtualSize = static_cast<std::uint32_t>(section.data.size());
			virtualAddress = alignUp(virtualAddress + alignUp(section.virtualSize, sectionAlignment), sectionAlignment);
			rawPointer += section.rawSize;
		}

		if (!image.sections.empty()) {
			image.entryPointRva = image.sections.front().virtualAddress;
		}

		ByteBuffer buffer;
		writeDosHeader(buffer, 0x80);
		buffer.align(0x80);
		writePESignature(buffer);
		writeCOFFHeader(buffer, static_cast<std::uint16_t>(sectionCount), 240);
		writePEOptionalHeader(buffer, image, headersSize, virtualAddress);
		for (const auto& section : image.sections) {
			writeSectionHeader(buffer, section, section.rawPointer, section.rawSize);
		}
		buffer.align(fileAlignment);
		for (const auto& section : image.sections) {
			if (buffer.size() < section.rawPointer) {
				buffer.align(fileAlignment);
			}
			buffer.writeBytes(section.data);
			buffer.align(fileAlignment);
		}
		return buffer.release();
	}

	static std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment) {
		return alignment == 0 ? value : ((value + alignment - 1u) / alignment) * alignment;
	}

	void writeSectionHeader(ByteBuffer& buffer, const PECOFFSection& section, std::uint32_t rawPointer, std::uint32_t rawSize) const {
		char name[8] = {};
		std::memcpy(name, section.name.data(), std::min<std::size_t>(section.name.size(), 8));
		buffer.writeBytes(name, sizeof(name));
		buffer.writeU32(section.virtualSize);
		buffer.writeU32(section.virtualAddress);
		buffer.writeU32(rawSize);
		buffer.writeU32(rawPointer);
		buffer.writeU32(0);
		buffer.writeU32(0);
		buffer.writeU16(0);
		buffer.writeU16(0);
		buffer.writeU32(section.characteristics);
	}

	void patchSectionRawPointer(ByteBuffer& buffer, std::size_t headerOffset, std::uint32_t rawPointer, std::uint32_t rawSize) const {
		buffer.patchU32(headerOffset + 16, rawSize);
		buffer.patchU32(headerOffset + 20, rawPointer);
	}

	void writeSymbol(ByteBuffer& buffer,
		const std::string& name,
		std::uint32_t value,
		std::int16_t sectionNumber,
		std::uint8_t storageClass,
		std::uint8_t auxCount,
		std::vector<std::pair<std::string, std::uint32_t>>& stringTableEntries) const {
		if (name.size() <= 8) {
			char symbolName[8] = {};
			std::memcpy(symbolName, name.data(), name.size());
			buffer.writeBytes(symbolName, sizeof(symbolName));
		} else {
			buffer.writeU32(0);
			const std::uint32_t offset = 4 + static_cast<std::uint32_t>(stringTableEntries.size() * 4);
			buffer.writeU32(offset);
			stringTableEntries.emplace_back(name, offset);
		}
		buffer.writeU32(value);
		buffer.writeU16(static_cast<std::uint16_t>(sectionNumber));
		buffer.writeU16(0);
		buffer.writeU8(storageClass);
		buffer.writeU8(auxCount);
	}

	void writeDosHeader(ByteBuffer& buffer, std::uint32_t peOffset) const {
		buffer.writeU16(0x5A4D);
		for (int i = 0; i < 29; ++i) {
			buffer.writeU16(0);
		}
		buffer.writeU32(peOffset);
	}

	void writePESignature(ByteBuffer& buffer) const {
		buffer.writeU32(0x00004550);
	}

	void writeCOFFHeader(ByteBuffer& buffer, std::uint16_t sectionCount, std::uint16_t optionalHeaderSize) const {
		buffer.writeU16(0x8664);
		buffer.writeU16(sectionCount);
		buffer.writeU32(0);
		buffer.writeU32(0);
		buffer.writeU32(0);
		buffer.writeU32(0x0002 | 0x0020);
		buffer.writeU16(optionalHeaderSize);
		buffer.writeU16(0x2022);
	}

	void writePEOptionalHeader(ByteBuffer& buffer, const PECOFFImage& image, std::uint32_t headersSize, std::uint32_t imageSize) const {
		const std::uint32_t sizeOfCode = image.sections.empty() ? 0 : alignUp(static_cast<std::uint32_t>(image.sections[0].rawSize), image.fileAlignment);
		std::uint32_t sizeOfInitializedData = 0;
		for (std::size_t i = 1; i < image.sections.size(); ++i) {
			sizeOfInitializedData += alignUp(static_cast<std::uint32_t>(image.sections[i].rawSize), image.fileAlignment);
		}

		buffer.writeU16(0x20B);
		buffer.writeU8(14);
		buffer.writeU8(0);
		buffer.writeU32(sizeOfCode);
		buffer.writeU32(sizeOfInitializedData);
		buffer.writeU32(0);
		buffer.writeU32(image.entryPointRva);
		buffer.writeU32(image.sections.empty() ? 0 : image.sections.front().virtualAddress);
		buffer.writeU64(image.imageBase);
		buffer.writeU32(image.sectionAlignment);
		buffer.writeU32(image.fileAlignment);
		buffer.writeU16(6);
		buffer.writeU16(0);
		buffer.writeU16(0);
		buffer.writeU16(0);
		buffer.writeU16(6);
		buffer.writeU16(0);
		buffer.writeU32(0);
		buffer.writeU32(imageSize);
		buffer.writeU32(headersSize);
		buffer.writeU32(0);
		buffer.writeU16(3);
		buffer.writeU16(0x8160);
		buffer.writeU64(0x100000);
		buffer.writeU64(0x1000);
		buffer.writeU64(0x100000);
		buffer.writeU64(0x1000);
		buffer.writeU32(0);
		buffer.writeU32(16);
		for (int i = 0; i < 16; ++i) {
			buffer.writeU64(0);
			buffer.writeU64(0);
		}
	}
};

// --- Driver -----------------------------------------------------------------
}  // namespace pcn

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Usage: paracine <source.pcn> [--pe|--coff|--cpp] [--out file]\n";
		return 1;
	}

	std::string outputMode = "pe";
	std::string outputPath;
	for (int i = 2; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--coff") {
			outputMode = "coff";
		} else if (arg == "--pe") {
			outputMode = "pe";
		} else if (arg == "--cpp") {
			outputMode = "cpp";
		} else if (arg == "--out" && i + 1 < argc) {
			outputPath = argv[++i];
		}
	}

	std::ifstream input(argv[1], std::ios::binary);
	if (!input) {
		std::cerr << "failed to read file: " << argv[1] << "\n";
		return 1;
	}

	std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

	try {
		pcn::Lexer lexer(source);
		auto tokens = lexer.tokenize();

		pcn::Parser parser(std::move(tokens));
		pcn::Program program = parser.parse();

		pcn::SemanticAnalyzer analyzer;
		auto analysis = analyzer.analyze(program);
		if (!analysis.succeeded()) {
			pcn::printSemanticDiagnostics(analysis);
			return 1;
		}

		auto ir = pcn::lowerProgramToIR(program);
		if (outputMode == "cpp") {
			pcn::CppIREmitter emitter;
			std::cout << emitter.emit(ir);
			return 0;
		}

		pcn::PECOFFGenerator generator;
		std::vector<std::uint8_t> binary = outputMode == "coff" ? generator.emitObject(ir) : generator.emitExecutable(ir);
		if (!outputPath.empty()) {
			std::ofstream output(outputPath, std::ios::binary);
			if (!output) {
				std::cerr << "failed to write file: " << outputPath << "\n";
				return 1;
			}
			output.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
		} else {
			std::cout.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
		}
		return 0;
	} catch (const std::exception& ex) {
		std::cerr << "error: " << ex.what() << "\n";
		return 1;
	}
}
