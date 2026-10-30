#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <chrono>
#include <cmath>
#include <numeric>
#include <locale>
#include <cstdlib>
#include <cerrno>
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
#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace pcn {

    // Locale-independent, complete-consumption numeric parsing. The lexer accepts
    // digit separators; neither partial conversions nor process locale may change
    // generated floating-point bits.
    template<class T> static T parseParacineFloating(std::string_view source) {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
        std::string clean;
        clean.reserve(source.size());
        for (char c : source)if (c != '_')clean.push_back(c);
        T result{};
        const auto parsed = std::from_chars(clean.data(), clean.data() + clean.size(), result, std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr != clean.data() + clean.size())
            throw std::runtime_error("E0113 invalid or out-of-range floating-point literal: " + std::string(source));
        return result;
    }

    // One checked integer decoder shared by constant resolution, optimizer,
    // reference interpreter and machine lowering. It rejects trailing text and
    // overflow instead of accepting std::stoll's partially consumed prefix.
    static std::int64_t parseParacineInteger(std::string_view source) {
        std::string digits;
        digits.reserve(source.size());
        for (char c : source)if (c != '_')digits.push_back(c);
        if (digits.empty())throw std::runtime_error("E0114 empty integer literal");
        const std::size_t sign = digits[0] == '-' ? 1u : 0u;
        int base = 10;
        if (digits.size() >= sign + 2 && digits[sign] == '0') {
            if (digits[sign + 1] == 'x' || digits[sign + 1] == 'X')base = 16;
            if (digits[sign + 1] == 'b' || digits[sign + 1] == 'B')base = 2;
        }
        if (base != 10)digits.erase(sign, 2);
        std::int64_t value{};
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
            throw std::runtime_error("E0114 invalid or out-of-range integer literal: " + std::string(source));
        return value;
    }

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
        // Edition-2026 canonical reserved vocabulary.
        "abi", "alias", "alloc", "and", "as", "break", "capability", "choose",
        "const", "continue", "defer", "else", "export", "fail", "false", "for",
        "foreign", "from", "give", "gives", "if", "import", "in", "io", "is",
        "let", "loop", "module", "move", "not", "nothing", "or", "own", "public",
        "pure", "raw", "record", "return_", "routine", "static", "test", "then",
        "thread", "trap", "true", "try", "unsafe", "var", "variant", "when",
        "where", "while", "with", "yield_",

        // Compatibility spellings accepted by this bootstrap implementation.
        "each", "otherwise", "case", "enum", "union", "use", "compile", "layout",
        "packed", "align", "null", "return"
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
            : source_(normalizeSource(std::move(source))) {}

        std::vector<Token> tokenize() {
            std::vector<Token> tokens;
            bool atLineStart = true;
            bool emittedAnyToken = false;

            while (!isAtEnd()) {
                SourceLocation loc{ line_, column_ };
                char ch = peek();

                if (atLineStart) {
                    if (ch == '\n') {
                        const bool continuation = delimiterDepth_ == 0 && nextLineBeginsPipeline();
                        advanceLineBreak(tokens, delimiterDepth_ == 0 && !continuation);
                        pipelineContinuation_ = continuation;
                        continue;
                    }

                    if (pipelineContinuation_) {
                        while (!isAtEnd() && (peek() == ' ' || peek() == '\t')) advance();
                        pipelineContinuation_ = false;
                        atLineStart = false;
                        if (isAtEnd()) break;
                        loc = { line_, column_ };
                        ch = peek();
                    }

                    std::size_t indent = 0;
                    const std::size_t startColumn = column_;
                    if (!atLineStart) {
                        // A pipeline continuation has already consumed indentation for this line.
                    }
                    else if (delimiterDepth_ == 0) {
                        while (!isAtEnd() && (peek() == ' ' || peek() == '\t')) {
                            if (peek() == '\t') {
                                throw lexicalError("E0100", "tabs are forbidden in structural indentation", { line_, column_ });
                            }
                            advance();
                            ++indent;
                        }
                        if (peek() == '#' || peek() == '\n') {
                            // Blank/comment-only lines do not alter the layout stack.
                            atLineStart = false;
                            continue;
                        }
                        if ((indent % kIndentWidth) != 0) {
                            throw lexicalError("E0100", "indentation width must be divisible by four", { line_, startColumn });
                        }
                        const std::size_t current = indentStack_.back();
                        if (!emittedAnyToken && indent != 0) {
                            throw lexicalError("E0101", "top-level source may not begin indented", { line_, startColumn });
                        }
                        if (indent > current) {
                            if (indent != current + kIndentWidth) {
                                throw lexicalError("E0101", "indentation skips a structural level", { line_, startColumn });
                            }
                            indentStack_.push_back(indent);
                            tokens.push_back({ TokenKind::Indent, "<indent>",{ line_, startColumn } });
                        }
                        else if (indent < current) {
                            while (indentStack_.size() > 1 && indent < indentStack_.back()) {
                                indentStack_.pop_back();
                                tokens.push_back({ TokenKind::Dedent, "<dedent>",{ line_, startColumn } });
                            }
                            if (indent != indentStack_.back()) {
                                throw lexicalError("E0102", "inconsistent dedent", { line_, startColumn });
                            }
                        }
                    }
                    else {
                        // Newlines inside (), [] and {} are ordinary whitespace.
                        while (!isAtEnd() && (peek() == ' ' || peek() == '\t')) {
                            advance();
                        }
                    }
                    atLineStart = false;
                    if (isAtEnd()) break;
                    loc = { line_, column_ };
                    ch = peek();
                }

                if (ch == ' ' || ch == '\t') {
                    advance();
                    continue;
                }
                if (ch == '\n') {
                    const bool continuation = delimiterDepth_ == 0 && nextLineBeginsPipeline();
                    advanceLineBreak(tokens, delimiterDepth_ == 0 && !continuation);
                    pipelineContinuation_ = continuation;
                    atLineStart = true;
                    continue;
                }

                if (ch == '#') {
                    while (!isAtEnd() && peek() != '\n') advance();
                    continue;
                }

                if (ch == '"') {
                    advance();
                    std::string value;
                    while (!isAtEnd() && peek() != '"') {
                        char c = advance();
                        if (c == '\\') {
                            if (isAtEnd()) throw lexicalError("E0110", "unterminated string literal", loc);
                            char escaped = advance();
                            switch (escaped) {
                            case 'n': value.push_back('\n'); break;
                            case 'r': value.push_back('\r'); break;
                            case 't': value.push_back('\t'); break;
                            case '\\': value.push_back('\\'); break;
                            case '"': value.push_back('"'); break;
                            default: value.push_back(escaped); break;
                            }
                        }
                        else {
                            if (c == '\n') throw lexicalError("E0110", "newline in string literal", loc);
                            value.push_back(c);
                        }
                    }
                    if (isAtEnd()) throw lexicalError("E0110", "unterminated string literal", loc);
                    advance();
                    tokens.push_back({ TokenKind::String, value, loc });
                    emittedAnyToken = true;
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
                    std::string number;
                    const bool radixPrefix = ch == '0' &&
                        (peekNext() == 'x' || peekNext() == 'X' || peekNext() == 'b' || peekNext() == 'B');
                    if (radixPrefix) {
                        number.push_back(advance()); number.push_back(advance());
                        while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) != 0 || peek() == '_'))
                            number.push_back(advance());
                    }
                    else {
                        while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0 || peek() == '_'))
                            number.push_back(advance());
                        if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext())) != 0) {
                            number.push_back(advance());
                            while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0 || peek() == '_'))
                                number.push_back(advance());
                        }
                        if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
                            number.push_back(advance());
                            if (!isAtEnd() && (peek() == '+' || peek() == '-'))number.push_back(advance());
                            if (isAtEnd() || std::isdigit(static_cast<unsigned char>(peek())) == 0)
                                throw lexicalError("E0113", "exponent requires a decimal digit", loc);
                            while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0 || peek() == '_'))
                                number.push_back(advance());
                        }
                    }
                    validateNumberToken(number, loc);
                    tokens.push_back({ TokenKind::Number, number, loc });
                    emittedAnyToken = true;
                    continue;
                }

                if (isIdentifierStart(ch)) {
                    std::string ident;
                    while (!isAtEnd() && isIdentifierPart(peek())) ident.push_back(advance());
                    tokens.push_back({ kKeywords.count(ident) ? TokenKind::Keyword : TokenKind::Identifier, ident, loc });
                    emittedAnyToken = true;
                    continue;
                }

                if (isSingleCharSymbol(ch)) {
                    const SourceLocation symbolLoc = loc;
                    std::string symbol;
                    const std::string three = lookahead(3);
                    const std::string two = lookahead(2);
                    if (kThreeCharOperators.count(three)) {
                        symbol = three;
                        advance(); advance(); advance();
                    }
                    else if (kTwoCharOperators.count(two)) {
                        symbol = two;
                        advance(); advance();
                    }
                    else {
                        symbol.push_back(advance());
                    }

                    if (symbol == "(" || symbol == "[" || symbol == "{") ++delimiterDepth_;
                    if (symbol == ")" || symbol == "]" || symbol == "}") {
                        if (delimiterDepth_ == 0) throw lexicalError("E0111", "unmatched closing delimiter", symbolLoc);
                        --delimiterDepth_;
                    }
                    tokens.push_back({ TokenKind::Symbol, symbol, symbolLoc });
                    emittedAnyToken = true;
                    continue;
                }

                throw lexicalError("E0109", "unexpected character '" + std::string(1, ch) + "'", loc);
            }

            if (delimiterDepth_ != 0) {
                throw lexicalError("E0112", "unterminated grouping delimiter", { line_, column_ });
            }
            if (!tokens.empty() && tokens.back().kind != TokenKind::Newline) {
                tokens.push_back({ TokenKind::Newline, "\\n",{ line_, column_ } });
            }
            while (indentStack_.size() > 1) {
                indentStack_.pop_back();
                tokens.push_back({ TokenKind::Dedent, "<dedent>",{ line_, column_ } });
            }
            tokens.push_back({ TokenKind::EndOfFile, "",{ line_, column_ } });
            return tokens;
        }

    private:
        static std::string normalizeSource(std::string source) {
            if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
                static_cast<unsigned char>(source[1]) == 0xBB && static_cast<unsigned char>(source[2]) == 0xBF) {
                source.erase(0, 3);
            }
            std::string out;
            out.reserve(source.size() + 1);
            for (std::size_t i = 0; i < source.size(); ++i) {
                if (source[i] == '\r') {
                    if (i + 1 < source.size() && source[i + 1] == '\n') ++i;
                    out.push_back('\n');
                }
                else {
                    out.push_back(source[i]);
                }
            }
            if (out.empty() || out.back() != '\n') out.push_back('\n');
            validateUTF8(out);
            return out;
        }

        // Strict UTF-8 decoding: reject overlong forms, surrogates and non-scalars.
        // This validates input encoding; full NFC normalization remains a separate requirement.
        static void validateUTF8(const std::string& source) {
            const auto fail = [](std::size_t offset) -> void {
                throw std::runtime_error("E0113 malformed UTF-8 source at byte " +
                    std::to_string(offset + 1));
                };
            for (std::size_t i = 0; i < source.size();) {
                const auto lead = static_cast<unsigned char>(source[i]);
                if (lead <= 0x7F) { ++i; continue; }
                std::size_t extra = 0;
                if (lead >= 0xC2 && lead <= 0xDF) extra = 1;
                else if (lead >= 0xE0 && lead <= 0xEF) extra = 2;
                else if (lead >= 0xF0 && lead <= 0xF4) extra = 3;
                else fail(i);
                if (extra > source.size() - i - 1) fail(i);
                for (std::size_t j = 1; j <= extra; ++j) {
                    const auto c = static_cast<unsigned char>(source[i + j]);
                    if ((c & 0xC0) != 0x80) fail(i + j);
                }
                const auto next = static_cast<unsigned char>(source[i + 1]);
                if ((lead == 0xE0 && next < 0xA0) ||
                    (lead == 0xED && next > 0x9F) ||
                    (lead == 0xF0 && next < 0x90) ||
                    (lead == 0xF4 && next > 0x8F)) fail(i);
                i += extra + 1;
            }
        }

        std::runtime_error lexicalError(const std::string& code, const std::string& message, SourceLocation loc) const {
            return std::runtime_error(code + " at " + std::to_string(loc.line) + ":" + std::to_string(loc.column) + ": " + message);
        }

        bool isAtEnd() const { return index_ >= source_.size(); }
        char peek() const { return isAtEnd() ? '\0' : source_[index_]; }
        char peekNext() const { return index_ + 1 < source_.size() ? source_[index_ + 1] : '\0'; }
        std::string lookahead(std::size_t n) const {
            if (index_ + n > source_.size()) return {};
            return source_.substr(index_, n);
        }
        bool nextLineBeginsPipeline() const {
            std::size_t i = index_;
            if (i < source_.size() && source_[i] == '\n') ++i;
            while (i < source_.size() && (source_[i] == ' ' || source_[i] == '\t')) ++i;
            return i + 1 < source_.size() && source_[i] == '-' && source_[i + 1] == '>';
        }
        char advance() {
            char value = source_[index_++];
            ++column_;
            return value;
        }
        void advanceLineBreak(std::vector<Token>& tokens, bool emitToken) {
            const SourceLocation loc{ line_, column_ };
            if (!isAtEnd() && peek() == '\n') advance();
            if (emitToken && (tokens.empty() || tokens.back().kind != TokenKind::Newline)) {
                tokens.push_back({ TokenKind::Newline, "\\n", loc });
            }
            ++line_;
            column_ = 1;
        }

        // Syntactic validation belongs in the frontend: `check` must reject
        // malformed numeric tokens even when the backend is not invoked.
        void validateNumberToken(const std::string& literal, SourceLocation loc) const {
            const std::string_view s(literal);
            const bool radix = s.size() >= 2 && s[0] == '0' &&
                (s[1] == 'x' || s[1] == 'X' || s[1] == 'b' || s[1] == 'B');
            const int base = radix ? ((s[1] == 'x' || s[1] == 'X') ? 16 : 2) : 10;
            std::size_t at = radix ? 2 : 0;
            const auto validDigit = [&](char c)->bool {
                if (c >= '0' && c <= '9')return c - '0' < base;
                if (base == 16 && c >= 'a' && c <= 'f')return true;
                if (base == 16 && c >= 'A' && c <= 'F')return true;
                return false;
                };
            auto digitGroup = [&] {
                const std::size_t first = at;
                bool lastDigit = false;
                while (at < s.size()) {
                    if (validDigit(s[at])) { lastDigit = true; ++at; }
                    else if (s[at] == '_') {
                        if (!lastDigit || at + 1 == s.size() || !validDigit(s[at + 1]))
                            throw lexicalError("E0114", "invalid numeric separator", loc);
                        lastDigit = false; ++at;
                    }
                    else break;
                }
                if (at == first || !lastDigit)throw lexicalError("E0114", "expected numeric digits", loc);
                };
            digitGroup();
            if (!radix && at < s.size() && s[at] == '.') { ++at; digitGroup(); }
            if (!radix && at < s.size() && (s[at] == 'e' || s[at] == 'E')) {
                ++at; if (at < s.size() && (s[at] == '+' || s[at] == '-'))++at;
                digitGroup();
            }
            if (at != s.size())throw lexicalError("E0114", "invalid digit or suffix in numeric literal", loc);
            if (!radix && s.find_first_of(".eE") != std::string::npos)
                (void)parseParacineFloating<double>(s);
        }

        static bool isSingleCharSymbol(char ch) {
            static const std::unordered_set<char> symbols = {
                '{', '}', '(', ')', '[', ']', ',', ':', ';', '.', '?', '@',
                '+', '-', '*', '/', '%', '=', '<', '>', '!', '&', '|', '^', '~'
            };
            return symbols.count(ch) != 0;
        }

        inline static const std::unordered_set<std::string> kTwoCharOperators = {
            "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "%=",
            "&=", "|=", "^=", "->", "=>", "::", "<<", ">>", ".."
        };
        inline static const std::unordered_set<std::string> kThreeCharOperators = {
            "<<=", ">>=", "...", "..=" , "..<"
        };

        std::string source_;
        std::size_t index_ = 0;
        std::size_t line_ = 1;
        std::size_t column_ = 1;
        std::size_t delimiterDepth_ = 0;
        bool pipelineContinuation_ = false;
        std::vector<std::size_t> indentStack_{ 0 };
        static constexpr std::size_t kIndentWidth = 4;
    };

    struct Parameter {
        std::string name;
        std::string typeName;
        SourceLocation location;
        std::string mode = "shared";
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
        std::vector<std::string> genericArguments;
        SourceSpan span;
        SourceLocation location;
    };

    struct TryExpr {
        ExprPtr operand;
        SourceSpan span;
        SourceLocation location;
    };

    struct ClosureExpr {
        std::vector<Parameter> parameters;
        std::string returnType;
        std::vector<std::string> effects;
        ExprPtr expressionBody;
        std::vector<StmtPtr> blockBody;
        bool hasExpressionBody = false;
        bool moveCapture = false;
        std::size_t closureId = 0;
        SourceSpan span;
        SourceLocation location;
    };

    struct GroupExpr {
        ExprPtr expression;
        SourceSpan span;
        SourceLocation location;
    };

    struct MemberExpr {
        ExprPtr object;
        std::string member;
        SourceSpan span;
        SourceLocation location;
    };

    struct ArrayExpr {
        std::vector<ExprPtr> elements;
        SourceSpan span;
        SourceLocation location;
    };

    struct IndexExpr {
        ExprPtr object;
        ExprPtr index;
        SourceSpan span;
        SourceLocation location;
    };

    struct ConstructField {
        std::string name;
        ExprPtr value;
        SourceLocation location;
    };

    struct ConstructExpr {
        std::string typeName;
        std::vector<ConstructField> fields;
        SourceSpan span;
        SourceLocation location;
    };

    struct Expr {
        using Node = std::variant<NumberExpr, StringExpr, BoolExpr, NullExpr, VariableExpr, UnaryExpr, BinaryExpr, PipelineExpr, RangeExpr, ChooseExpr, CallExpr, TryExpr, ClosureExpr, GroupExpr, MemberExpr, ConstructExpr, ArrayExpr, IndexExpr>;
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

    // Mutation of one fixed-array element; the named base is an addressable place.
    struct RecordSetStmt {
        std::string name;
        std::string field;
        ExprPtr value;
        SourceSpan span;
        SourceLocation location;
    };

    struct ArraySetStmt {
        std::string name;
        ExprPtr index;
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
        bool bindingsMutable = false;
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

    struct BreakStmt {
        std::string label;
        SourceSpan span;
        SourceLocation location;
    };

    struct ContinueStmt {
        std::string label;
        SourceSpan span;
        SourceLocation location;
    };

    struct BlockStmt {
        std::vector<StmtPtr> statements;
        SourceSpan span;
        SourceLocation location;
    };

    struct Stmt {
        using Node = std::variant<LetStmt, VarStmt, AssignStmt, ArraySetStmt, RecordSetStmt, ExprStmt, ReturnStmt, GiveStmt, IfStmt, EachStmt, UnsafeStmt, DeferStmt, FailStmt, WhileStmt, BreakStmt, ContinueStmt, BlockStmt>;
        Node node;
        SourceSpan span;
        SourceLocation location;
    };

    struct AstFactory {
        static SourceSpan span(SourceLocation start, SourceLocation end) {
            return { start, end };
        }

        static ExprPtr makeExpr(Expr node, SourceSpan /*nodeSpan*/) {
            return std::make_shared<const Expr>(std::move(node));
        }

        static StmtPtr makeStmt(Stmt node, SourceSpan /*nodeSpan*/) {
            return std::make_shared<const Stmt>(std::move(node));
        }

        template <typename T>
        static ExprPtr makeExpr(T node, SourceSpan nodeSpan) {
            return std::make_shared<const Expr>(Expr{ Expr::Node{ std::move(node) }, nodeSpan, nodeSpan.start });
        }

        template <typename T>
        static StmtPtr makeStmt(T node, SourceSpan nodeSpan) {
            return std::make_shared<const Stmt>(Stmt{ Stmt::Node{ std::move(node) }, nodeSpan, nodeSpan.start });
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

    struct GenericParameter {
        std::string name;
        bool isConst = false;
        std::string constType;
        SourceLocation location;
    };

    struct CapabilityConstraint {
        std::string subject;
        std::string capability;
        std::vector<std::string> arguments;
        SourceLocation location;
    };

    struct Routine {
        std::string name;
        bool isPublic = false;
        bool isCompileTime = false;
        bool isForeign = false;
        std::string foreignAbi;
        std::vector<GenericParameter> genericParameters;
        std::vector<Parameter> parameters;
        std::string returnType;
        std::vector<CapabilityConstraint> constraints;
        std::vector<std::string> effects;
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

    struct CapabilityMemberDecl {
        std::string name;
        std::vector<Parameter> parameters;
        std::string returnType;
        std::vector<std::string> effects;
        SourceLocation location;
    };

    struct CapabilityDecl {
        std::string name;
        bool isPublic = false;
        std::vector<GenericParameter> genericParameters;
        std::vector<CapabilityMemberDecl> members;
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
        using Node = std::variant<Routine, RecordDecl, EnumDecl, UnionDecl, VariantDecl, ModuleDecl, UseDecl, ConstDecl, CapabilityDecl, ForeignDecl>;
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
        Text,
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

    enum class OwnershipState { Available, Moved };

    struct SymbolInfo {
        SemanticType type;
        bool isMutable = false;
        bool isParameter = false;
        std::string parameterMode = "shared";
        OwnershipState ownership = OwnershipState::Available;
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
        std::vector<std::string> parameterModes;
        std::vector<std::string> genericParameters;
        std::vector<CapabilityConstraint> constraints;
        std::vector<std::string> effects;
        bool isForeign = false;
    };

    struct SemanticScope {
        std::unordered_map<std::string, SymbolInfo> symbols;
    };


    struct SKMSymbolFact {
        std::uint64_t id = 0;
        std::string routine;
        std::string name;
        std::string typeName;
        std::string parameterMode = "shared";
        bool isMutable = false;
        bool isParameter = false;
        bool initialized = false;
        SourceLocation declaration;
        std::size_t firstPoint = 0;
        std::size_t lastPoint = 0;
    };

    enum class BorrowKind { Shared, Exclusive, Owned };

    struct SKMBorrowRegionFact {
        std::uint64_t id = 0;
        std::uint64_t sourceSymbol = 0;
        std::string sourceName;
        BorrowKind kind = BorrowKind::Shared;
        std::size_t beginPoint = 0;
        std::size_t endPoint = 0;
        std::size_t closureId = 0;
        bool escaping = false;
        SourceLocation location;
    };

    struct SKMEffectEdgeFact {
        std::string caller;
        std::string callee;
        std::set<std::string> effects;
    };

    struct SKMClosureCaptureFact {
        std::size_t closureId = 0;
        std::uint64_t sourceSymbol = 0;
        std::string name;
        std::string typeName;
        BorrowKind kind = BorrowKind::Shared;
        bool escaping = false;
        SourceLocation location;
    };

    struct SKMClosureFact {
        std::size_t closureId = 0;
        std::vector<std::string> parameterTypes;
        std::string returnType;
        std::set<std::string> effects;
        bool moveCapture = false;
        SourceLocation location;
    };

    struct SKMGenericSpecializationFact {
        std::string genericRoutine;
        std::string specializationName;
        std::map<std::string, std::string> substitutions;
        SourceLocation location;
    };

    struct SKMCapabilityResolutionFact {
        std::string subjectType;
        std::string capability;
        std::vector<std::string> arguments;
        bool satisfied = false;
        std::vector<std::string> evidence;
        SourceLocation location;
    };

    struct SKMCallEdgeFact {
        std::string caller;
        std::string callee;
        SourceLocation location;
    };

    struct SKMLayoutFact {
        std::string typeName;
        std::size_t size = 0;
        std::size_t alignment = 1;
        bool nicheOptimized = false;
        std::vector<std::pair<std::string, std::size_t>> fieldOffsets;
    };

    struct SKMAliasFact {
        std::string routine;
        std::string left;
        std::string right;
        bool mayAlias = true;
        SourceLocation location;
    };

    struct SemanticKnowledgeModel {
        std::vector<SKMSymbolFact> symbols;
        std::vector<SKMBorrowRegionFact> borrowRegions;
        std::vector<SKMEffectEdgeFact> effectEdges;
        std::vector<SKMClosureCaptureFact> closureCaptures;
        std::vector<SKMClosureFact> closures;
        std::vector<SKMGenericSpecializationFact> specializations;
        std::vector<SKMCapabilityResolutionFact> capabilityResolutions;
        std::vector<SKMCallEdgeFact> calls;
        std::vector<SKMLayoutFact> layouts;
        std::vector<SKMAliasFact> aliases;
        std::vector<std::string> structuralFacts;
    };

    struct SemanticAnalysisResult {
        std::vector<SemanticDiagnostic> diagnostics;
        std::vector<DeclarationInfo> declarations;
        std::unordered_map<std::string, NominalTypeInfo> nominalTypes;
        std::unordered_map<std::string, std::set<std::string>> resolvedEffects;
        SemanticKnowledgeModel skm;

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
        Float32,
        Float64,
        Text,
        String,
        Null,
        Function,
        Aggregate,
        Result,
        Option,
        Closure
    };

    struct IRValueType {
        IRValueKind kind = IRValueKind::Unknown;
        std::string name;
    };

    inline IRValueType makeIRUnknownType() {
        return { IRValueKind::Unknown,{} };
    }

    inline IRValueType makeIRVoidType() {
        return { IRValueKind::Void, "void" };
    }

    inline IRValueType makeIRBoolType() {
        return { IRValueKind::Bool, "bool" };
    }

    inline IRValueType makeIRIntType() {
        return { IRValueKind::Int64, "int64" };
    }

    inline IRValueType makeIRTextType() {
        return { IRValueKind::Text, "text" };
    }

    inline IRValueType makeIRFloatType(const std::string& type) {
        if (type == "float") return { IRValueKind::Float32, "float" };
        if (type == "double") return { IRValueKind::Float64, "double" };
        throw std::runtime_error("expected IEEE-754 float/double type");
    }

    inline IRValueType makeIRStringType() {
        return { IRValueKind::String, "string" };
    }

    inline IRValueType makeIRNullType() {
        return { IRValueKind::Null, "null" };
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
        case IRValueKind::Float32: return "float";
        case IRValueKind::Float64: return "double";
        case IRValueKind::Text: return "text";
        case IRValueKind::String: return "string";
        case IRValueKind::Null: return "null";
        case IRValueKind::Function: return "function";
        case IRValueKind::Aggregate: return "aggregate";
        case IRValueKind::Result: return "result";
        case IRValueKind::Option: return "option";
        case IRValueKind::Closure: return "closure";
        }
        return "unknown";
    }

    // --- IR types ---------------------------------------------------------------
    enum class IROpcode {
        ConstInt,
        ConstFloat,
        ConstText,
        ConstBool,
        ConstNull,
        Copy,
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        BitAnd,
        BitOr,
        BitXor,
        Shl,
        Shr,
        Neg,
        BitNot,
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
        CallClosure,
        TextToString,
        StringToText,
        StringClear,
        StringDispose,
        StringMove,
        ByteCount,
        TextEquals,
        MakeAggregate,
        ExtractField,
        ArrayIndex,
        ArrayStore,
        StoreField,
        ResultOk,
        ResultErr,
        ResultIsErr,
        ResultTry,
        OptionSome,
        OptionNone,
        MakeClosure,
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
        std::vector<std::string> parameterModes;
        std::vector<IRValueType> parameterValueTypes;
        std::vector<VReg> parameterVregs;
        std::vector<IRValueType> localValueTypes;
        std::vector<IRBasicBlock> blocks;
        std::uint32_t totalVregs = 0;
    };

    struct IRProgram {
        std::vector<IRFunction> functions;
        std::vector<DeclarationInfo> declarationSummaries;
        // Target legalization consumes the same SAGE/SKM layout truth that produced PCIR.
        // Keeping these facts attached to PCIR prevents PMIR from re-inventing source semantics.
        SemanticKnowledgeModel skm;
        std::unordered_map<std::string, NominalTypeInfo> nominalTypes;
    };

    inline SemanticType makeUnknownType() {
        return { SemanticTypeKind::Unknown,{} };
    }

    inline SemanticType makeVoidType() {
        return { SemanticTypeKind::Void, "void" };
    }

    inline SemanticType makeBoolType() {
        return { SemanticTypeKind::Bool, "bool" };
    }

    inline SemanticType makeNumberType() {
        return { SemanticTypeKind::Number, "number" };
    }

    inline SemanticType makeTextType() {
        return { SemanticTypeKind::Text, "text" };
    }

    inline SemanticType makeStringType() {
        return { SemanticTypeKind::String, "string" };
    }

    inline SemanticType makeNullType() {
        return { SemanticTypeKind::Null, "null" };
    }

    inline SemanticType makeCustomType(std::string name) {
        return { SemanticTypeKind::Custom, std::move(name) };
    }

    inline bool isNumericTypeName(const std::string& name) {
        static const std::unordered_set<std::string> names = {
            "byte", "short", "int", "long", "ubyte", "ushort", "uint", "ulong",
            "size", "ssize", "float", "double", "number"
        };
        return names.count(name) != 0;
    }

    inline bool isBuiltinTypeName(const std::string& name) {
        return name == "void" || name == "never" || name == "bool" || name == "char" ||
            name == "text" || name == "string" || name == "null" || isNumericTypeName(name);
    }

    inline bool isRawAuthorityTypeName(const std::string& name) {
        return name == "raw" || name.rfind("raw<", 0) == 0;
    }

    inline SemanticType semanticTypeFromName(const std::string& name) {
        if (name.empty()) return makeUnknownType();
        if (name == "void") return makeVoidType();
        if (name == "bool") return makeBoolType();
        if (isNumericTypeName(name)) return { SemanticTypeKind::Number, name };
        if (name == "text") return makeTextType();
        if (name == "string") return makeStringType();
        if (name == "null" || name == "nothing") return makeNullType();
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
        case SemanticTypeKind::Text: return "text";
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
        if (target.kind == SemanticTypeKind::Number && value.kind == SemanticTypeKind::Number) {
            // `number` is the contextual numeric-literal type; typed numerics do not implicitly convert.
            return target.name == "number" || value.name == "number" || target.name == value.name;
        }
        if (target.kind == SemanticTypeKind::Custom && value.kind == SemanticTypeKind::Custom) {
            if (target.name == value.name) return true;
            if (target.name.rfind("result<", 0) == 0 && (value.name.rfind("__ok<", 0) == 0 || value.name.rfind("__err<", 0) == 0)) return true;
            if (target.name.rfind("option<", 0) == 0 && (value.name.rfind("__some<", 0) == 0 || value.name == "__none")) return true;
            return false;
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
        return type.kind == SemanticTypeKind::Bool || type.kind == SemanticTypeKind::Number || type.kind == SemanticTypeKind::Text || type.kind == SemanticTypeKind::String || type.kind == SemanticTypeKind::Null || type.kind == SemanticTypeKind::Custom || type.kind == SemanticTypeKind::Unknown;
    }


    struct CanonicalTypeExpr {
        std::string head;
        std::vector<CanonicalTypeExpr> args;
        bool tuple = false;
    };

    class CanonicalTypeParser {
    public:
        explicit CanonicalTypeParser(std::string_view text) : text_(text) {}
        CanonicalTypeExpr parse() { auto t = parseType(); skip(); if (pos_ != text_.size()) throw std::runtime_error("invalid canonical type: " + std::string(text_)); return t; }
    private:
        void skip() { while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_; }
        bool eat(char c) { skip(); if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; } return false; }
        CanonicalTypeExpr parseType() {
            skip();
            CanonicalTypeExpr out;
            if (eat('(')) {
                out.tuple = true; out.head = "tuple";
                if (!eat(')')) {
                    for (;;) { out.args.push_back(parseType()); if (eat(')')) break; if (!eat(',')) throw std::runtime_error("expected ',' in tuple type"); }
                }
                return out;
            }
            std::size_t start = pos_;
            while (pos_ < text_.size()) {
                char c = text_[pos_];
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':') ++pos_; else break;
            }
            if (start == pos_) throw std::runtime_error("expected type atom");
            out.head = std::string(text_.substr(start, pos_ - start));
            if (eat('<')) {
                if (!eat('>')) {
                    for (;;) { out.args.push_back(parseType()); if (eat('>')) break; if (!eat(',')) throw std::runtime_error("expected ',' in generic type"); }
                }
            }
            return out;
        }
        std::string_view text_; std::size_t pos_ = 0;
    };

    static std::string canonicalTypeText(const CanonicalTypeExpr& t) {
        std::ostringstream oss;
        if (t.tuple) {
            oss << '(';
            for (std::size_t i = 0; i < t.args.size(); ++i) { if (i)oss << ','; oss << canonicalTypeText(t.args[i]); }
            oss << ')'; return oss.str();
        }
        oss << t.head;
        if (!t.args.empty()) { oss << '<'; for (std::size_t i = 0; i < t.args.size(); ++i) { if (i)oss << ','; oss << canonicalTypeText(t.args[i]); } oss << '>'; }
        return oss.str();
    }

    static bool matchGenericTypePattern(const CanonicalTypeExpr& pattern, const CanonicalTypeExpr& actual,
        const std::unordered_set<std::string>& genericNames,
        std::map<std::string, std::string>& substitutions) {
        if (!pattern.tuple && genericNames.contains(pattern.head) && pattern.args.empty()) {
            const std::string actualText = canonicalTypeText(actual);
            auto [it, inserted] = substitutions.emplace(pattern.head, actualText);
            return inserted || it->second == actualText;
        }
        if (pattern.tuple != actual.tuple || pattern.head != actual.head || pattern.args.size() != actual.args.size()) return false;
        for (std::size_t i = 0; i < pattern.args.size(); ++i)
            if (!matchGenericTypePattern(pattern.args[i], actual.args[i], genericNames, substitutions)) return false;
        return true;
    }

    static std::string substituteGenericType(const std::string& typeName, const std::map<std::string, std::string>& substitutions) {
        if (typeName.empty()) return typeName;
        try {
            auto root = CanonicalTypeParser(typeName).parse();
            std::function<void(CanonicalTypeExpr&)> apply = [&](CanonicalTypeExpr& t) {
                if (!t.tuple && t.args.empty()) {
                    auto it = substitutions.find(t.head);
                    if (it != substitutions.end()) { t = CanonicalTypeParser(it->second).parse(); return; }
                }
                for (auto& a : t.args) apply(a);
                };
            apply(root); return canonicalTypeText(root);
        }
        catch (...) { auto it = substitutions.find(typeName); return it == substitutions.end() ? typeName : it->second; }
    }

    static bool typeIsResult(const std::string& typeName, std::string* okType = nullptr, std::string* errType = nullptr) {
        try { auto t = CanonicalTypeParser(typeName).parse(); if (t.head != "result" || t.args.size() != 2)return false; if (okType)*okType = canonicalTypeText(t.args[0]); if (errType)*errType = canonicalTypeText(t.args[1]); return true; }
        catch (...) { return false; }
    }
    static bool typeIsOption(const std::string& typeName, std::string* valueType = nullptr) {
        try { auto t = CanonicalTypeParser(typeName).parse(); if (t.head != "option" || t.args.size() != 1)return false; if (valueType)*valueType = canonicalTypeText(t.args[0]); return true; }
        catch (...) { return false; }
    }

    static bool typeIsArray(const std::string& name, std::string* element = nullptr,
        std::size_t* count = nullptr) {
        try {
            const auto t = CanonicalTypeParser(name).parse();
            if (t.head != "array" || t.args.size() != 2 || !t.args[1].args.empty()) return false;
            const auto& digits = t.args[1].head;
            if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](unsigned char c) {return std::isdigit(c) != 0; })) return false;
            const auto n = std::stoull(digits);
            if (n > (1ull << 20)) return false; // compiler resource bound; not a source-language maximum
            if (element) *element = canonicalTypeText(t.args[0]);
            if (count) *count = static_cast<std::size_t>(n);
            return true;
        }
        catch (...) { return false; }
    }

    class Parser {
    public:
        explicit Parser(std::vector<Token> tokens)
            : tokens_(std::move(tokens)) {}

        Program parse() {
            Program program;
            skipNewlines();
            while (!isAtEnd()) {
                if (checkKeyword("public") || checkKeyword("compile") || checkKeyword("record") || checkKeyword("enum") || checkKeyword("variant") || checkKeyword("union") || checkKeyword("module") || checkKeyword("use") || checkKeyword("import") || checkKeyword("const") || checkKeyword("capability") || checkKeyword("foreign")) {
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
            if (matchKeyword("use") || matchKeyword("import")) {
                return parseUseDeclaration();
            }
            if (matchKeyword("const")) {
                return parseConstDeclaration(isPublic, isCompile);
            }
            if (matchKeyword("capability")) {
                return parseCapabilityDeclaration(isPublic);
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
        Token expectSymbol(const std::string& value) {
            if (!matchSymbol(value)) {
                throw std::runtime_error("expected symbol '" + value + "'");
            }
            return previous();
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
            return Declaration{ routine, routine.span, routine.location };
        }

        Routine parseRoutineCore() {
            Routine routine;
            routine.location = previous().location;
            routine.span = { routine.location, routine.location };
            routine.name = expectIdentifier().text;
            routine.genericParameters = parseGenericParameterList();
            routine.parameters = parseParameterList();
            if (matchKeyword("gives")) {
                routine.returnType = parseTypeName();
            }
            else {
                routine.returnType = "void";
            }
            routine.constraints = parseWhereClause();
            routine.effects = parseEffectClause();
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
            return Declaration{ ForeignDecl{ abi, routine, routine.span, routine.location }, routine.span, routine.location };
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
            record.span = { record.location, record.location };
            record.isPublic = isPublic;
            record.isCompileTime = isCompile;
            record.name = expectIdentifier().text;
            record.fields = parseFieldsBlock();
            return Declaration{ record, record.span, record.location };
        }

        Declaration parseUnionDeclaration(bool isPublic, bool isCompile) {
            UnionDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
            decl.isPublic = isPublic;
            decl.layout = isCompile ? "packed" : "";
            decl.name = expectIdentifier().text;
            decl.fields = parseFieldsBlock();
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseEnumDeclaration(bool isPublic, bool isCompile) {
            EnumDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
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
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseVariantDeclaration(bool isPublic, bool isCompile) {
            VariantDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
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
                            if (checkKeyword("var") || checkKeyword("own") || checkKeyword("raw")) p.mode = consume().text;
                            p.name = expectIdentifier().text;
                            p.location = previous().location;
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
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseCapabilityDeclaration(bool isPublic) {
            CapabilityDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
            decl.isPublic = isPublic;
            decl.name = expectIdentifier().text;
            decl.genericParameters = parseGenericParameterList();
            consumeLineBreak();
            skipNewlines();
            expectIndent();
            while (!checkToken(TokenKind::Dedent) && !checkToken(TokenKind::EndOfFile)) {
                expectKeyword("routine");
                CapabilityMemberDecl member;
                member.location = previous().location;
                member.name = expectIdentifier().text;
                member.parameters = parseParameterList();
                member.returnType = matchKeyword("gives") ? parseTypeName() : "void";
                member.effects = parseEffectClause();
                decl.members.push_back(std::move(member));
                consumeLineBreak();
                skipNewlines();
            }
            expectDedent();
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseModuleDeclaration(bool isPublic, bool isCompile) {
            ModuleDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
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
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseUseDeclaration() {
            UseDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
            decl.path.push_back(parseQualifiedName());
            if (matchKeyword("as")) {
                decl.alias = expectIdentifier().text;
            }
            consumeLineBreak();
            return Declaration{ decl, decl.span, decl.location };
        }

        Declaration parseConstDeclaration(bool isPublic, bool isCompile) {
            ConstDecl decl;
            decl.location = previous().location;
            decl.span = { decl.location, decl.location };
            decl.isPublic = isPublic;
            decl.isCompileTime = isCompile;
            decl.name = expectIdentifier().text;
            if (matchSymbol(":")) {
                decl.typeName = parseTypeName();
            }
            expectSymbol("=");
            decl.value = parseExpression();
            consumeLineBreak();
            return Declaration{ decl, decl.span, decl.location };
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

        std::vector<GenericParameter> parseGenericParameterList() {
            std::vector<GenericParameter> params;
            if (!matchSymbol("<")) return params;
            while (!typeCloseAhead()) {
                GenericParameter p;
                p.isConst = matchKeyword("const");
                auto name = expectIdentifier();
                p.name = name.text; p.location = name.location;
                if (p.isConst) { expectSymbol(":"); p.constType = parseTypeName(); }
                params.push_back(std::move(p));
                if (!matchSymbol(",")) break;
            }
            consumeTypeClose();
            return params;
        }

        std::vector<CapabilityConstraint> parseWhereClause() {
            std::vector<CapabilityConstraint> constraints;
            if (!matchKeyword("where")) return constraints;
            for (;;) {
                CapabilityConstraint c;
                auto subject = expectIdentifier(); c.subject = subject.text; c.location = subject.location;
                expectKeyword("is");
                if (peek().kind != TokenKind::Identifier && peek().kind != TokenKind::Keyword) throw std::runtime_error("expected capability name");
                c.capability = consume().text;
                if (matchSymbol("<")) {
                    while (!typeCloseAhead()) {
                        c.arguments.push_back(parseTypeName());
                        if (!matchSymbol(",")) break;
                    }
                    consumeTypeClose();
                }
                constraints.push_back(std::move(c));
                if (!matchSymbol(",")) break;
            }
            return constraints;
        }

        std::vector<Parameter> parseParameterList() {
            std::vector<Parameter> params;
            auto listLocation = peek().location;
            expectSymbol("(");
            if (!checkSymbol(")")) {
                for (;;) {
                    Parameter parameter;
                    if (checkKeyword("var") || checkKeyword("own") || checkKeyword("raw")) {
                        parameter.mode = consume().text;
                    }
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
            if (matchSymbol("(")) {
                std::string tuple = "(";
                bool first = true;
                while (!checkSymbol(")")) {
                    if (!first) { expectSymbol(","); tuple += ","; }
                    tuple += parseTypeName();
                    first = false;
                }
                expectSymbol(")");
                return tuple + ")";
            }
            if (peek().kind != TokenKind::Identifier && peek().kind != TokenKind::Keyword && peek().kind != TokenKind::Number) {
                throw std::runtime_error("expected type name");
            }
            std::string name = consume().text;
            while (matchSymbol("::")) {
                name += "::" + expectIdentifier().text;
            }
            if (matchSymbol("<")) {
                name += "<";
                bool first = true;
                while (!typeCloseAhead()) {
                    if (!first) { expectSymbol(","); name += ","; }
                    name += parseTypeName();
                    first = false;
                }
                consumeTypeClose();
                name += ">";
            }
            return name;
        }

        bool typeCloseAhead() const {
            return pendingTypeClosers_ != 0 || checkSymbol(">") || checkSymbol(">>");
        }

        void consumeTypeClose() {
            if (pendingTypeClosers_ != 0) { --pendingTypeClosers_; return; }
            if (matchSymbol(">")) return;
            if (matchSymbol(">>")) { pendingTypeClosers_ = 1; return; }
            throw std::runtime_error("expected '>' to close generic type");
        }

        std::vector<std::string> parseEffectClause() {
            std::vector<std::string> effects;
            if (!matchKeyword("with")) return effects;
            for (;;) {
                if (peek().kind != TokenKind::Keyword && peek().kind != TokenKind::Identifier)
                    throw std::runtime_error("expected effect name after 'with'");
                std::string effect = consume().text;
                static const std::unordered_set<std::string> allowed = { "alloc", "io", "thread", "trap", "unsafe", "pure" };
                if (!allowed.count(effect)) throw std::runtime_error("unknown effect: " + effect);
                effects.push_back(effect);
                if (!matchSymbol(",")) break;
            }
            if (std::find(effects.begin(), effects.end(), "pure") != effects.end() && effects.size() != 1)
                throw std::runtime_error("'pure' cannot be combined with another effect");
            return effects;
        }

        Routine parseRoutine() {
            expectKeyword("routine");
            Routine routine;
            routine.location = previous().location;
            routine.span = { routine.location, routine.location };
            routine.name = expectIdentifier().text;
            routine.genericParameters = parseGenericParameterList();
            routine.parameters = parseParameterList();
            if (matchKeyword("gives")) {
                routine.returnType = parseTypeName();
            }
            else {
                routine.returnType = "void";
            }
            routine.constraints = parseWhereClause();
            routine.effects = parseEffectClause();
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
                    return AstFactory::makeStmt(Stmt{ LetStmt{ value.name, value.typeName, value.initializer, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, VarStmt>) {
                    return AstFactory::makeStmt(Stmt{ VarStmt{ value.name, value.typeName, value.initializer, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, AssignStmt>) {
                    return AstFactory::makeStmt(Stmt{ AssignStmt{ value.name, value.value, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                    return AstFactory::makeStmt(Stmt{ ArraySetStmt{ value.name, value.index, value.value, span, location },span,location }, span);
                }
                else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                    return AstFactory::makeStmt(Stmt{ RecordSetStmt{ value.name, value.field, value.value, span, location },span,location }, span);
                }
                else if constexpr (std::is_same_v<T, ExprStmt>) {
                    return AstFactory::makeStmt(Stmt{ ExprStmt{ value.expression, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ReturnStmt>) {
                    return AstFactory::makeStmt(Stmt{ ReturnStmt{ value.value, value.hasValue, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, GiveStmt>) {
                    return AstFactory::makeStmt(Stmt{ GiveStmt{ value.value, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, IfStmt>) {
                    return AstFactory::makeStmt(Stmt{ IfStmt{ value.condition, value.thenBranch, value.elseBranch, value.hasElse, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, EachStmt>) {
                    return AstFactory::makeStmt(Stmt{ EachStmt{ value.bindings, value.range, value.body, span, location, value.bindingsMutable }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                    return AstFactory::makeStmt(Stmt{ UnsafeStmt{ value.body, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, DeferStmt>) {
                    return AstFactory::makeStmt(Stmt{ DeferStmt{ value.expression, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, FailStmt>) {
                    return AstFactory::makeStmt(Stmt{ FailStmt{ value.expression, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, WhileStmt>) {
                    return AstFactory::makeStmt(Stmt{ WhileStmt{ value.condition, value.body, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, BreakStmt>) {
                    return AstFactory::makeStmt(Stmt{ BreakStmt{ value.label, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ContinueStmt>) {
                    return AstFactory::makeStmt(Stmt{ ContinueStmt{ value.label, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, BlockStmt>) {
                    return AstFactory::makeStmt(Stmt{ BlockStmt{ value.statements, span, location }, span, location }, span);
                }
                else {
                    return AstFactory::makeStmt(Stmt{ BlockStmt{ {}, span, location }, span, location }, span);
                }
                }, node);
        }

        ExprPtr makeExpr(Expr::Node node, SourceSpan span, SourceLocation location) const {
            return std::visit([&](const auto& value) -> ExprPtr {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, NumberExpr>) {
                    return AstFactory::makeExpr(Expr{ NumberExpr{ value.value, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, StringExpr>) {
                    return AstFactory::makeExpr(Expr{ StringExpr{ value.value, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, BoolExpr>) {
                    return AstFactory::makeExpr(Expr{ BoolExpr{ value.value, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, NullExpr>) {
                    return AstFactory::makeExpr(Expr{ NullExpr{ span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, VariableExpr>) {
                    return AstFactory::makeExpr(Expr{ VariableExpr{ value.name, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    return AstFactory::makeExpr(Expr{ UnaryExpr{ value.op, value.operand, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    return AstFactory::makeExpr(Expr{ BinaryExpr{ value.op, value.left, value.right, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    return AstFactory::makeExpr(Expr{ PipelineExpr{ value.input, value.stages, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    return AstFactory::makeExpr(Expr{ RangeExpr{ value.start, value.end, value.step, value.inclusive, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    return AstFactory::makeExpr(Expr{ ChooseExpr{ value.subject, value.cases, value.otherwiseValue, value.hasOtherwise, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    return AstFactory::makeExpr(Expr{ CallExpr{ value.callee, value.arguments, value.genericArguments, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, TryExpr>) {
                    return AstFactory::makeExpr(Expr{ TryExpr{ value.operand, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ClosureExpr>) {
                    return AstFactory::makeExpr(Expr{ ClosureExpr{ value.parameters, value.returnType, value.effects, value.expressionBody, value.blockBody, value.hasExpressionBody, value.moveCapture, value.closureId, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    return AstFactory::makeExpr(Expr{ GroupExpr{ value.expression, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    return AstFactory::makeExpr(Expr{ MemberExpr{ value.object, value.member, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    return AstFactory::makeExpr(Expr{ ConstructExpr{ value.typeName, value.fields, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    return AstFactory::makeExpr(Expr{ ArrayExpr{ value.elements, span, location }, span, location }, span);
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    return AstFactory::makeExpr(Expr{ IndexExpr{ value.object, value.index, span, location }, span, location }, span);
                }
                else {
                    return AstFactory::makeExpr(Expr{ GroupExpr{ value.expression, span, location }, span, location }, span);
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
                return makeStmt(LetStmt{ name, typeName, initializer,{ start, location }, location }, { start, location }, location);
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
                return makeStmt(VarStmt{ name, typeName, initializer,{ start, location }, location }, { start, location }, location);
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
                return makeStmt(IfStmt{ condition, std::move(thenBranch), std::move(elseBranch), hasElse,{ start, location }, location }, { start, location }, location);
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
                return makeStmt(WhileStmt{ condition, std::move(body),{ start, location }, location }, { start, location }, location);
            }


            if (matchKeyword("loop")) {
                auto location = previous().location;
                auto start = location;
                consumeLineBreak();
                skipNewlines();
                expectIndent();
                auto body = parseBlockStatements();
                expectDedent();
                auto condition = makeExpr(BoolExpr{ true,{ location, location }, location }, { location, location }, location);
                return makeStmt(WhileStmt{ condition, std::move(body),{ start, location }, location }, { start, location }, location);
            }

            if (matchKeyword("break") || matchKeyword("continue")) {
                const bool isBreak = previous().text == "break";
                auto location = previous().location;
                std::string label;
                if (matchSymbol("@")) label = expectIdentifier().text;
                consumeLineBreak();
                if (isBreak) return makeStmt(BreakStmt{ label,{ location, location }, location }, { location, location }, location);
                return makeStmt(ContinueStmt{ label,{ location, location }, location }, { location, location }, location);
            }

            if (matchKeyword("each") || matchKeyword("for")) {
                auto location = previous().location;
                auto start = location;
                bool bindingsMutable = matchKeyword("var");
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
                return makeStmt(EachStmt{ std::move(bindings), range, std::move(body),{ start, location }, location, bindingsMutable }, { start, location }, location);
            }

            if (matchKeyword("unsafe")) {
                auto location = previous().location;
                auto start = location;
                consumeLineBreak();
                skipNewlines();
                expectIndent();
                auto body = parseBlockStatements();
                expectDedent();
                return makeStmt(UnsafeStmt{ std::move(body),{ start, location }, location }, { start, location }, location);
            }

            if (matchKeyword("defer")) {
                auto location = previous().location;
                auto start = location;
                auto expression = parseExpression();
                consumeLineBreak();
                return makeStmt(DeferStmt{ expression,{ start, location }, location }, { start, location }, location);
            }

            if (matchKeyword("fail")) {
                auto location = previous().location;
                auto start = location;
                auto expression = parseExpression();
                consumeLineBreak();
                return makeStmt(FailStmt{ expression,{ start, location }, location }, { start, location }, location);
            }

            if (matchKeyword("give")) {
                auto location = previous().location;
                auto start = location;
                auto value = parseExpression();
                consumeLineBreak();
                return makeStmt(GiveStmt{ value,{ start, location }, location }, { start, location }, location);
            }

            if (matchKeyword("return")) {
                auto location = previous().location;
                auto start = location;
                if (peek().kind == TokenKind::Newline || peek().kind == TokenKind::Dedent || peek().kind == TokenKind::EndOfFile) {
                    consumeLineBreak();
                    return makeStmt(ReturnStmt{ nullptr, false,{ start, location }, location }, { start, location }, location);
                }
                auto value = parseExpression();
                consumeLineBreak();
                return makeStmt(ReturnStmt{ value, true,{ start, location }, location }, { start, location }, location);
            }

            if (peek().kind == TokenKind::Identifier && isAssignmentOperator(peek(1).text)) {
                auto location = peek().location;
                auto start = location;
                std::string name = expectIdentifier().text;
                std::string assignOp = consume().text;
                auto value = parseExpression();
                if (assignOp != "=") {
                    std::string binaryOp = assignOp.substr(0, assignOp.size() - 1);
                    auto lhs = makeExpr(VariableExpr{ name,{ start, location }, location }, { start, location }, location);
                    value = makeExpr(BinaryExpr{ binaryOp, lhs, value,{ start, value ? value->span.end : location }, location }, { start, value ? value->span.end : location }, location);
                }
                consumeLineBreak();
                return makeStmt(AssignStmt{ name, value,{ start, location }, location }, { start, location }, location);
            }

            auto location = peek().location;
            auto start = location;
            auto expression = parseExpression();
            if (checkSymbol("=") || isAssignmentOperator(peek().text)) {
                auto op = consume().text;
                const auto* index = expression ? std::get_if<IndexExpr>(&expression->node) : nullptr;
                const auto* base = index && index->object ? std::get_if<VariableExpr>(&index->object->node) : nullptr;
                const auto* member = expression ? std::get_if<MemberExpr>(&expression->node) : nullptr;
                const auto* memberBase = member && member->object ? std::get_if<VariableExpr>(&member->object->node) : nullptr;
                if (op != "=")throw std::runtime_error("E0230 compound place assignment requires distinct read-modify-write operation");
                auto value = parseExpression(); consumeLineBreak();
                if (base)return makeStmt(ArraySetStmt{ base->name,index->index,value,{ start,location },location }, { start,location }, location);
                if (memberBase)return makeStmt(RecordSetStmt{ memberBase->name,member->member,value,{ start,location },location }, { start,location }, location);
                throw std::runtime_error("E0229 field/index assignment requires an addressable binding");
            }
            consumeLineBreak();
            return makeStmt(ExprStmt{ expression,{ start, location }, location }, { start, location }, location);
        }

        static bool isAssignmentOperator(const std::string& op) {
            static const std::unordered_set<std::string> ops = {
                "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="
            };
            return ops.count(op) != 0;
        }

        void consumeLineBreak() {
            if (peek().kind == TokenKind::Newline) {
                ++index_;
                return;
            }
            if (peek().kind == TokenKind::Dedent || peek().kind == TokenKind::EndOfFile) {
                return;
            }
            // A compound choose expression may have already consumed the newline
            // and final dedent that terminate its enclosing statement.
            if (index_ > 0 && previous().kind == TokenKind::Dedent) return;
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
                        cases.push_back(ChooseCase{ condition, value,{},{},{ clauseStart, clauseLoc }, clauseLoc });
                        continue;
                    }
                    if (matchKeyword("otherwise")) {
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
                return makeExpr(ChooseExpr{ subject, std::move(cases), otherwiseValue, hasOtherwise,{ start, location }, location }, { start, location }, location);
            }
            return parsePipeline();
        }

        ExprPtr parsePipeline() {
            // Edition-2026 pipeline stages are normal calls with the left-hand value
            // supplied as their first argument. Desugar *before* SAGE so effects,
            // overload selection, ownership and generic specialization see exactly
            // the call that will run; never evaluate a discarded pipeline stage.
            auto expr = parseLogicalOr();
            while (matchSymbol("->")) {
                auto location = previous().location;
                auto stage = parseCall();
                ExprPtr callee;
                std::vector<ExprPtr> arguments;
                std::vector<std::string> genericArguments;
                if (const auto* call = std::get_if<CallExpr>(&stage->node)) {
                    callee = call->callee;
                    arguments = call->arguments;
                    genericArguments = call->genericArguments;
                }
                else if (std::holds_alternative<VariableExpr>(stage->node)) {
                    callee = stage;
                }
                else {
                    throw std::runtime_error("E0214 pipeline stage must be a routine name or a routine call");
                }
                arguments.insert(arguments.begin(), expr);
                SourceSpan span{ expr->span.start, stage->span.end };
                expr = makeExpr(CallExpr{ callee, std::move(arguments),
                                         std::move(genericArguments), span, location }, span, location);
            }
            return expr;
        }

        ExprPtr parseLogicalOr() {
            auto expr = parseLogicalAnd();
            while (matchKeyword("or") || matchSymbol("||")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseLogicalAnd();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ location, right ? right->span.end : location }, location }, { location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseLogicalAnd() {
            auto expr = parseBitwiseOr();
            while (matchKeyword("and") || matchSymbol("&&")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseBitwiseOr();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }


        ExprPtr parseBitwiseOr() {
            auto expr = parseBitwiseXor();
            while (matchSymbol("|")) {
                auto location = previous().location; auto right = parseBitwiseXor();
                expr = makeExpr(BinaryExpr{ "|", expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseBitwiseXor() {
            auto expr = parseBitwiseAnd();
            while (matchSymbol("^")) {
                auto location = previous().location; auto right = parseBitwiseAnd();
                expr = makeExpr(BinaryExpr{ "^", expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseBitwiseAnd() {
            auto expr = parseEquality();
            while (matchSymbol("&")) {
                auto location = previous().location; auto right = parseEquality();
                expr = makeExpr(BinaryExpr{ "&", expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseEquality() {
            auto expr = parseComparison();
            while (matchSymbol("==") || matchSymbol("!=")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseComparison();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseComparison() {
            auto expr = parseRange();
            while (matchSymbol("<") || matchSymbol("<=") || matchSymbol(">") || matchSymbol(">=")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseTerm();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseRange() {
            auto expr = parseShift();
            while (matchSymbol("..") || matchSymbol("..=") || matchSymbol("..<")) {
                auto location = previous().location;
                std::string rangeOp = previous().text;
                auto start = expr ? expr->span.start : location;
                auto right = parseShift();
                bool inclusive = rangeOp == "..=";
                expr = makeExpr(RangeExpr{ expr, right, nullptr, inclusive,{ start, right ? right->span.end : location }, location }, { start, right ? right->span.end : location }, location);
            }
            return expr;
        }


        ExprPtr parseShift() {
            auto expr = parseTerm();
            while (matchSymbol("<<") || matchSymbol(">>")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseTerm();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseTerm() {
            auto expr = parseFactor();
            while (matchSymbol("+") || matchSymbol("-")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseFactor();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseFactor() {
            auto expr = parseUnary();
            while (matchSymbol("*") || matchSymbol("/") || matchSymbol("%")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto right = parseUnary();
                expr = makeExpr(BinaryExpr{ op, expr, right,{ expr ? expr->span.start : location, right ? right->span.end : location }, location }, { expr ? expr->span.start : location, right ? right->span.end : location }, location);
            }
            return expr;
        }

        ExprPtr parseUnary() {
            if (matchKeyword("try")) {
                auto location = previous().location;
                auto operand = parseUnary();
                return makeExpr(TryExpr{ operand,{ location, operand ? operand->span.end : location }, location }, { location, operand ? operand->span.end : location }, location);
            }
            if (matchSymbol("-") || matchSymbol("!") || matchSymbol("~") || matchKeyword("not")) {
                std::string op = previous().text;
                auto location = previous().location;
                auto operand = parseUnary();
                return makeExpr(UnaryExpr{ op, operand,{ location, operand ? operand->span.end : location }, location }, { location, operand ? operand->span.end : location }, location);
            }
            return parseCall();
        }

        bool genericCallAhead() const {
            if (!checkSymbol("<")) return false;
            std::size_t depth = 0;
            for (std::size_t i = index_; i < tokens_.size(); ++i) {
                const auto& t = tokens_[i];
                if (t.kind == TokenKind::Newline || t.kind == TokenKind::EndOfFile) return false;
                if (t.kind == TokenKind::Symbol && t.text == "<") ++depth;
                else if (t.kind == TokenKind::Symbol && t.text == ">") { if (--depth == 0) return i + 1 < tokens_.size() && tokens_[i + 1].kind == TokenKind::Symbol && tokens_[i + 1].text == "("; }
                else if (t.kind == TokenKind::Symbol && t.text == ">>") { if (depth <= 2) return i + 1 < tokens_.size() && tokens_[i + 1].kind == TokenKind::Symbol && tokens_[i + 1].text == "("; depth -= 2; }
            }
            return false;
        }

        std::vector<std::string> parseCallGenericArguments() {
            std::vector<std::string> args;
            expectSymbol("<");
            while (!typeCloseAhead()) {
                args.push_back(parseTypeName());
                if (!matchSymbol(",")) break;
            }
            consumeTypeClose();
            return args;
        }

        ExprPtr parseCall() {
            auto expr = parsePrimary();
            for (;;) {
                if (matchSymbol("[")) {
                    const auto start = expr->span.start;
                    auto idx = parseExpression();
                    auto close = expectSymbol("]");
                    const SourceSpan span{ start,close.location };
                    expr = makeExpr(IndexExpr{ expr,idx,span,start }, span, start);
                    continue;
                }
                if (matchSymbol(".")) {
                    auto memberToken = expectIdentifier();
                    auto span = SourceSpan{ expr ? expr->span.start : memberToken.location, memberToken.location };
                    expr = makeExpr(MemberExpr{ expr, memberToken.text, span, memberToken.location }, span, memberToken.location);
                    continue;
                }
                std::vector<std::string> genericArguments;
                if (std::holds_alternative<VariableExpr>(expr->node) && genericCallAhead()) genericArguments = parseCallGenericArguments();
                if (!matchSymbol("(")) {
                    if (!genericArguments.empty()) throw std::runtime_error("generic arguments must be followed by a call");
                    break;
                }
                auto location = previous().location;
                auto start = expr ? expr->span.start : location;
                std::vector<ExprPtr> arguments;
                if (!checkSymbol(")")) {
                    for (;;) { arguments.push_back(parseExpression()); if (!matchSymbol(",")) break; }
                }
                auto close = expectSymbol(")");
                expr = makeExpr(CallExpr{ expr, std::move(arguments), std::move(genericArguments),{ start, close.location }, location }, { start, close.location }, location);
            }
            return expr;
        }

        ExprPtr parseClosureExpression(bool moveCapture, SourceLocation start) {
            ClosureExpr closure; closure.moveCapture = moveCapture; closure.location = start; closure.closureId = ++nextClosureId_;
            expectSymbol("(");
            if (!checkSymbol(")")) {
                for (;;) {
                    Parameter p;
                    if (checkKeyword("var") || checkKeyword("own") || checkKeyword("raw")) p.mode = consume().text;
                    auto n = expectIdentifier(); p.name = n.text; p.location = n.location;
                    if (matchSymbol(":")) p.typeName = parseTypeName();
                    closure.parameters.push_back(std::move(p));
                    if (!matchSymbol(",")) break;
                }
            }
            expectSymbol(")");
            closure.returnType = matchKeyword("gives") ? parseTypeName() : std::string{};
            closure.effects = parseEffectClause();
            if (matchSymbol("=>")) {
                closure.expressionBody = parseExpression(); closure.hasExpressionBody = true;
                auto end = closure.expressionBody ? closure.expressionBody->span.end : start; closure.span = { start,end };
            }
            else {
                consumeLineBreak(); skipNewlines(); expectIndent(); closure.blockBody = parseBlockStatements(); expectDedent(); closure.span = { start,start };
            }
            return makeExpr(std::move(closure), closure.span, start);
        }

        ExprPtr parsePrimary() {
            if (matchSymbol("[")) {
                auto start = previous().location;
                std::vector<ExprPtr> elements;
                if (!checkSymbol("]")) {
                    do { elements.push_back(parseExpression()); } while (matchSymbol(",") && !checkSymbol("]"));
                }
                const auto close = expectSymbol("]");
                SourceSpan span{ start,close.location };
                return makeExpr(ArrayExpr{ std::move(elements),span,start }, span, start);
            }
            if (matchKeyword("move")) { auto start = previous().location; expectKeyword("routine"); return parseClosureExpression(true, start); }
            if (matchKeyword("routine")) { auto start = previous().location; return parseClosureExpression(false, start); }
            if (matchSymbol("(")) {
                auto location = previous().location;
                auto start = location;
                auto expr = parseExpression();
                expectSymbol(")");
                return makeExpr(GroupExpr{ expr,{ start, location }, location }, { start, location }, location);
            }
            if (peek().kind == TokenKind::Number) {
                auto token = consume();
                return makeExpr(NumberExpr{ token.text,{ token.location, token.location }, token.location }, { token.location, token.location }, token.location);
            }
            if (peek().kind == TokenKind::String) {
                auto token = consume();
                return makeExpr(StringExpr{ token.text,{ token.location, token.location }, token.location }, { token.location, token.location }, token.location);
            }
            if (matchKeyword("true")) {
                auto token = previous();
                return makeExpr(BoolExpr{ true,{ token.location, token.location }, token.location }, { token.location, token.location }, token.location);
            }
            if (matchKeyword("false")) {
                auto token = previous();
                return makeExpr(BoolExpr{ false,{ token.location, token.location }, token.location }, { token.location, token.location }, token.location);
            }
            if (matchKeyword("null") || matchKeyword("nothing")) {
                auto token = previous();
                return makeExpr(NullExpr{ { token.location, token.location }, token.location }, { token.location, token.location }, token.location);
            }
            if (peek().kind == TokenKind::Identifier) {
                auto token = consume();
                auto expr = makeExpr(VariableExpr{ token.text,{ token.location, token.location }, token.location }, { token.location, token.location }, token.location);
                while (matchSymbol(".")) {
                    auto memberToken = expectIdentifier();
                    auto span = SourceSpan{ expr ? expr->span.start : token.location, memberToken.location };
                    expr = makeExpr(MemberExpr{ expr, memberToken.text, span, memberToken.location }, span, memberToken.location);
                }
                if (matchSymbol("{")) {
                    std::vector<ConstructField> fields;
                    if (!checkSymbol("}")) {
                        for (;;) {
                            auto fieldName = expectIdentifier();
                            expectSymbol(":");
                            auto value = parseExpression();
                            fields.push_back(ConstructField{ fieldName.text, value, fieldName.location });
                            if (!matchSymbol(",")) {
                                break;
                            }
                        }
                    }
                    auto close = expectSymbol("}");
                    std::string typeName = token.text;
                    auto span = SourceSpan{ token.location, close.location };
                    expr = makeExpr(ConstructExpr{ typeName, std::move(fields), span, close.location }, span, close.location);
                }
                return expr;
            }
            throw std::runtime_error("expected expression");
        }

        std::vector<Token> tokens_;
        std::size_t index_ = 0;
        std::size_t pendingTypeClosers_ = 0;
        std::size_t nextClosureId_ = 0;
    };


    // Canonical, side-effect-free top-level compile-time constants.  This pass
    // resolves dependencies and cycles before the semantic checker or backend runs.
    // It intentionally rejects constructs for which constant semantics are not yet
    // implemented instead of generating a runtime expression disguised as `const`.
    class TopLevelConstantResolver {
        // No implicit conversion between concrete numeric types. A numeric literal
        // is contextually rounded to float/double; a reference retains its declared
        // type. Every float32 operation rounds at its language-defined precision.
        using Value = std::variant<std::int64_t, bool, std::string, float, double>;
        std::unordered_map<std::string, const ConstDecl*> declarations_;
        std::unordered_map<std::string, Value> values_;
        std::unordered_set<std::string> active_;

        static bool isFloatType(const std::string& name) { return name == "float" || name == "double"; }
        static bool isFP(const Value& v) { return std::holds_alternative<float>(v) || std::holds_alternative<double>(v); }
        static std::int64_t modular(std::uint64_t v) { return std::bit_cast<std::int64_t>(v); }
        static std::int64_t integer(const Value& v) {
            if (auto p = std::get_if<std::int64_t>(&v))return *p;
            throw std::runtime_error("E0231 constant expression requires an integer operand");
        }
        static bool boolean(const Value& v) {
            if (auto p = std::get_if<bool>(&v))return *p;
            throw std::runtime_error("E0231 constant expression requires a boolean operand");
        }
        static bool floatLiteral(const std::string& src) {
            if (src.find('.') != std::string::npos)return true;
            if (src.size() >= 2 && src[0] == '0' && (src[1] == 'x' || src[1] == 'X' || src[1] == 'b' || src[1] == 'B'))return false;
            return src.find_first_of("eE") != std::string::npos;
        }
        static std::string normalizedDigits(std::string s) {
            s.erase(std::remove(s.begin(), s.end(), '_'), s.end()); return s;
        }
        static Value numericLiteral(const std::string& src, const std::string& expected) {
            const std::string digits = normalizedDigits(src);
            const bool fp = floatLiteral(src);
            if (fp || isFloatType(expected)) {
                try {
                    const double d = parseParacineFloating<double>(digits);
                    return expected == "float" ? Value{ static_cast<float>(d) } : Value{ d };
                }
                catch (...) { throw std::runtime_error("E0231 invalid floating constant: " + src); }
            }
            try { return parseParacineInteger(digits); }
            catch (...) { throw std::runtime_error("E0231 invalid or out-of-range integer constant: " + src); }
        }
        static std::string inferredType(const Value& v) {
            if (std::holds_alternative<bool>(v))return "bool";
            if (std::holds_alternative<std::string>(v))return "text";
            if (std::holds_alternative<float>(v))return "float";
            if (std::holds_alternative<double>(v))return "double";
            return "int";
        }
        template<typename F> static Value floatBinary(const Value& a, const Value& b, F fn) {
            if (a.index() != b.index())throw std::runtime_error("E0231 implicit float/double conversion is forbidden in constant expression");
            if (auto x = std::get_if<float>(&a))return static_cast<float>(fn(*x, std::get<float>(b)));
            return static_cast<double>(fn(std::get<double>(a), std::get<double>(b)));
        }
        Value eval(const ExprPtr& e, const std::string& expected = {}) {
            if (!e)throw std::runtime_error("E0231 missing constant initializer");
            return std::visit([&](const auto& n)->Value {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, NumberExpr>)return numericLiteral(n.value, expected);
                else if constexpr (std::is_same_v<T, BoolExpr>)return n.value;
                else if constexpr (std::is_same_v<T, StringExpr>)return n.value;
                else if constexpr (std::is_same_v<T, VariableExpr>)return resolve(n.name);
                else if constexpr (std::is_same_v<T, GroupExpr>)return eval(n.expression, expected);
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    auto v = eval(n.operand, expected);
                    if (n.op == "-") {
                        if (auto f = std::get_if<float>(&v))return -*f;
                        if (auto d = std::get_if<double>(&v))return -*d;
                        return modular(0ull - std::bit_cast<std::uint64_t>(integer(v)));
                    }
                    if (n.op == "~")return modular(~std::bit_cast<std::uint64_t>(integer(v)));
                    if (n.op == "not" || n.op == "!")return !boolean(v);
                    throw std::runtime_error("E0231 unsupported unary constant operator: " + n.op);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    auto a = eval(n.left, expected);
                    if (n.op == "and" || n.op == "&&") { if (!boolean(a))return false; return boolean(eval(n.right, expected)); }
                    if (n.op == "or" || n.op == "||") { if (boolean(a))return true; return boolean(eval(n.right, expected)); }
                    auto b = eval(n.right, isFP(a) ? inferredType(a) : expected);
                    if (isFP(b) && a.index() != b.index() && n.left &&
                        (std::holds_alternative<NumberExpr>(n.left->node) ||
                            std::holds_alternative<GroupExpr>(n.left->node)))
                        a = eval(n.left, inferredType(b));
                    if (isFP(a) || isFP(b)) {
                        if (a.index() != b.index())throw std::runtime_error("E0231 implicit conversion between concrete numeric types in constant expression");
                        if (n.op == "+")return floatBinary(a, b, [](auto x, auto y) {return x + y; });
                        if (n.op == "-")return floatBinary(a, b, [](auto x, auto y) {return x - y; });
                        if (n.op == "*")return floatBinary(a, b, [](auto x, auto y) {return x * y; });
                        if (n.op == "/")return floatBinary(a, b, [](auto x, auto y) {return x / y; });
                        if (n.op == "==")return std::holds_alternative<float>(a) ? std::get<float>(a) == std::get<float>(b) : std::get<double>(a) == std::get<double>(b);
                        if (n.op == "!=")return std::holds_alternative<float>(a) ? std::get<float>(a) != std::get<float>(b) : std::get<double>(a) != std::get<double>(b);
                        if (n.op == "<")return std::holds_alternative<float>(a) ? std::get<float>(a) < std::get<float>(b) : std::get<double>(a) < std::get<double>(b);
                        if (n.op == "<=")return std::holds_alternative<float>(a) ? std::get<float>(a) <= std::get<float>(b) : std::get<double>(a) <= std::get<double>(b);
                        if (n.op == ">")return std::holds_alternative<float>(a) ? std::get<float>(a) > std::get<float>(b) : std::get<double>(a) > std::get<double>(b);
                        if (n.op == ">=")return std::holds_alternative<float>(a) ? std::get<float>(a) >= std::get<float>(b) : std::get<double>(a) >= std::get<double>(b);
                        throw std::runtime_error("E0231 unsupported floating constant operator: " + n.op);
                    }
                    if (n.op == "==" || n.op == "!=") {
                        if (a.index() != b.index())throw std::runtime_error("E0231 incompatible types in constant equality");
                        const bool same = a == b; return n.op == "==" ? same : !same;
                    }
                    auto x = integer(a), y = integer(b);
                    const auto ux = std::bit_cast<std::uint64_t>(x), uy = std::bit_cast<std::uint64_t>(y);
                    if (n.op == "+")return modular(ux + uy);
                    if (n.op == "-")return modular(ux - uy);
                    if (n.op == "*")return modular(ux * uy);
                    if (n.op == "&")return modular(ux & uy);
                    if (n.op == "|")return modular(ux | uy);
                    if (n.op == "^")return modular(ux ^ uy);
                    if (n.op == "<<")return modular(ux << (uy & 63u));
                    if (n.op == ">>")return x >> (uy & 63u);
                    if (n.op == "/") { if (!y)throw std::runtime_error("E0232 division by zero in constant"); if (x == INT64_MIN && y == -1)return x; return x / y; }
                    if (n.op == "%") { if (!y)throw std::runtime_error("E0232 remainder by zero in constant"); if (x == INT64_MIN && y == -1)return std::int64_t{ 0 }; return x % y; }
                    if (n.op == "<")return x < y;
                    if (n.op == "<=")return x <= y;
                    if (n.op == ">")return x > y;
                    if (n.op == ">=")return x >= y;
                    throw std::runtime_error("E0231 unsupported binary constant operator: " + n.op);
                }
                else throw std::runtime_error("E0231 constant initializer must be a pure compile-time expression");
                }, e->node);
        }
        Value resolve(const std::string& name) {
            if (auto i = values_.find(name); i != values_.end())return i->second;
            auto d = declarations_.find(name);
            if (d == declarations_.end())throw std::runtime_error("E0231 unknown constant: " + name);
            if (!active_.insert(name).second)throw std::runtime_error("E0233 circular constant definition: " + name);
            auto result = eval(d->second->value, d->second->typeName);
            active_.erase(name);
            auto type = d->second->typeName;
            if (type.empty())type = inferredType(result);
            const bool good = (type == "bool" && std::holds_alternative<bool>(result)) ||
                (type == "text" && std::holds_alternative<std::string>(result)) ||
                ((type == "int" || type == "long") && std::holds_alternative<std::int64_t>(result)) ||
                (type == "float" && std::holds_alternative<float>(result)) ||
                (type == "double" && std::holds_alternative<double>(result));
            if (!good)throw std::runtime_error("E0234 constant '" + name + "' initializer does not match declared type '" + type + "'");
            if (type == "int") { auto x = integer(result); if (x<INT32_MIN || x>INT32_MAX)throw std::runtime_error("E0234 32-bit int constant out of range: " + name); }
            values_[name] = result;
            return result;
        }
        template<typename T> static std::string exactFloat(T value) {
            // max_digits10 guarantees lossless decimal -> binary conversion, including -0.
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
            std::string text = out.str();
            if (std::isfinite(value) && text.find_first_of(".eE") == std::string::npos)text += ".0";
            return text;
        }
    public:
        void resolveProgram(Program& p) {
            declarations_.clear(); values_.clear(); active_.clear();
            for (const auto& d : p.declarations) if (const auto* c = std::get_if<ConstDecl>(&d.node))
                if (!declarations_.emplace(c->name, c).second)throw std::runtime_error("E0233 duplicate constant: " + c->name);
            for (auto& d : p.declarations) if (auto* c = std::get_if<ConstDecl>(&d.node)) {
                auto v = resolve(c->name);
                if (c->typeName.empty())c->typeName = inferredType(v);
                const SourceSpan span = c->value->span;
                if (auto n = std::get_if<std::int64_t>(&v))c->value = AstFactory::makeExpr(NumberExpr{ std::to_string(*n),span,span.start }, span);
                else if (auto f = std::get_if<float>(&v))c->value = AstFactory::makeExpr(NumberExpr{ exactFloat(*f),span,span.start }, span);
                else if (auto f = std::get_if<double>(&v))c->value = AstFactory::makeExpr(NumberExpr{ exactFloat(*f),span,span.start }, span);
                else if (auto b = std::get_if<bool>(&v))c->value = AstFactory::makeExpr(BoolExpr{ *b,span,span.start }, span);
                else c->value = AstFactory::makeExpr(StringExpr{ std::get<std::string>(v),span,span.start }, span);
            }
        }
    };

    // --- Frontend: parsing and semantic analysis --------------------------------
    class SemanticAnalyzer {
    public:
        SemanticAnalysisResult analyze(const Program& program) {
            result_ = {};
            routines_.clear();
            nominalTypes_.clear();
            closureSignatures_.clear();
            globalConstTypes_.clear();
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
        struct NominalRecord {
            NominalTypeInfo info;
        };

        void registerNominal(const std::string& name, const std::string& kind, bool isPublic, bool isCompileTime, SourceLocation location) {
            DeclarationInfo info{ name, kind, isPublic, isCompileTime,{} };
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
            auto& record = nominalTypes_[name];
            record.info.name = name;
            record.info.kind = kind;
            record.info.isPublic = isPublic;
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
            for (const auto& gp : routine.genericParameters) signature.genericParameters.push_back(gp.name);
            signature.constraints = routine.constraints;
            signature.effects = routine.effects;
            signature.isForeign = routine.isForeign;
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
                }
                else if constexpr (std::is_same_v<T, EnumDecl>) {
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
                }
                else if constexpr (std::is_same_v<T, UnionDecl>) {
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
                }
                else if constexpr (std::is_same_v<T, VariantDecl>) {
                    registerNominal(node.name, "variant", node.isPublic, false, node.location);
                    NominalTypeInfo info;
                    info.name = node.name;
                    info.kind = "variant";
                    info.isPublic = node.isPublic;
                    for (const auto& c : node.cases) {
                        info.tags.push_back(c.name);
                    }
                    result_.nominalTypes[node.name] = std::move(info);
                }
                else if constexpr (std::is_same_v<T, ModuleDecl>) {
                    registerNominal(node.name, "module", node.isPublic, false, node.location);
                    DeclarationInfo info{ node.name, "module", node.isPublic, false, "module" };
                    result_.declarations.push_back(std::move(info));
                }
                else if constexpr (std::is_same_v<T, UseDecl>) {
                    DeclarationInfo info{ node.alias.empty() ? node.path.back() : node.alias, "use", false, false, joinPath(node.path) };
                    result_.declarations.push_back(std::move(info));
                }
                else if constexpr (std::is_same_v<T, ConstDecl>) {
                    DeclarationInfo info{ node.name, "const", node.isPublic, node.isCompileTime, node.typeName };
                    result_.declarations.push_back(std::move(info));
                    if (!globalConstTypes_.emplace(node.name, SymbolInfo{ semanticTypeFromName(node.typeName), false, false }).second)
                        report(node.location, "duplicate global constant: " + node.name);
                }
                else if constexpr (std::is_same_v<T, CapabilityDecl>) {
                    registerNominal(node.name, "capability", node.isPublic, false, node.location);
                    DeclarationInfo info{ node.name, "capability", node.isPublic, false, std::to_string(node.members.size()) + " member(s)" };
                    result_.declarations.push_back(std::move(info));
                }
                else if constexpr (std::is_same_v<T, ForeignDecl>) {
                    registerRoutine(node.routine);
                    DeclarationInfo info{ node.routine.name, "foreign", node.routine.isPublic, node.routine.isCompileTime, node.abi };
                    result_.declarations.push_back(std::move(info));
                }
                else if constexpr (std::is_same_v<T, Routine>) {
                    DeclarationInfo info{ node.name, "routine", node.isPublic, node.isCompileTime, node.isForeign ? node.foreignAbi : std::string{} };
                    result_.declarations.push_back(std::move(info));
                }
                }, declaration.node);
        }

        void analyzeDeclaration(const Declaration& declaration) {
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, ConstDecl>) {
                    if (node.value) analyzeExpr(node.value);
                }
                else if constexpr (std::is_same_v<T, ModuleDecl>) {
                    pushDeclarationScope();
                    for (const auto& nested : node.declarations) {
                        registerDeclaration(nested);
                    }
                    for (const auto& nested : node.declarations) {
                        analyzeDeclaration(nested);
                    }
                    popDeclarationScope();
                }
                else if constexpr (std::is_same_v<T, CapabilityDecl>) {
                    for (const auto& member : node.members) {
                        if (member.name.empty()) report(node.location, "capability member requires a name");
                    }
                }
                else if constexpr (std::is_same_v<T, ForeignDecl>) {
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
                const auto& parameter = routine.parameters[i];
                if (parameter.mode == "raw" && !isRawAuthorityTypeName(parameter.typeName)) {
                    report(parameter.location, "E0408 meaningless raw parameter mode: this bootstrap accepts raw authority for the raw pointer type; @raw_authority aggregates are not implemented yet");
                }
                declareSymbol(parameter.location, parameter.name, { parameterType, parameter.mode == "var" || parameter.mode == "own", true, parameter.mode, OwnershipState::Available });
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
                }
                else if constexpr (std::is_same_v<T, VarStmt>) {
                    analyzeUnreachableVar(node);
                }
                else if constexpr (std::is_same_v<T, AssignStmt>) {
                    analyzeUnreachableAssign(node);
                }
                else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                    (void)analyzeArraySet(node);
                }
                else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                    (void)analyzeRecordSet(node);
                }
                else if constexpr (std::is_same_v<T, ExprStmt>) {
                    analyzeExpr(node.expression);
                }
                else if constexpr (std::is_same_v<T, ReturnStmt>) {
                    analyzeUnreachableReturn(node);
                }
                else if constexpr (std::is_same_v<T, GiveStmt>) {
                    analyzeUnreachableGive(node);
                }
                else if constexpr (std::is_same_v<T, IfStmt>) {
                    analyzeUnreachableIf(node);
                }
                else if constexpr (std::is_same_v<T, EachStmt>) {
                    analyzeUnreachableEach(node);
                }
                else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                    analyzeUnreachableUnsafe(node);
                }
                else if constexpr (std::is_same_v<T, DeferStmt>) {
                    analyzeUnreachableDefer(node);
                }
                else if constexpr (std::is_same_v<T, FailStmt>) {
                    analyzeUnreachableFail(node);
                }
                else if constexpr (std::is_same_v<T, WhileStmt>) {
                    analyzeUnreachableWhile(node);
                }
                else if constexpr (std::is_same_v<T, BreakStmt> || std::is_same_v<T, ContinueStmt>) {
                    // No expression payload to analyze in unreachable jump statements.
                }
                else if constexpr (std::is_same_v<T, BlockStmt>) {
                    analyzeUnreachableBlock(node);
                }
                }, stmt->node);
        }

        bool analyzeReachableStatement(const Stmt::Node& node) {
            return std::visit([&](const auto& statement) -> bool {
                using T = std::decay_t<decltype(statement)>;
                if constexpr (std::is_same_v<T, LetStmt>) {
                    return analyzeLet(statement);
                }
                else if constexpr (std::is_same_v<T, VarStmt>) {
                    return analyzeVar(statement);
                }
                else if constexpr (std::is_same_v<T, AssignStmt>) {
                    return analyzeAssign(statement);
                }
                else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                    return analyzeArraySet(statement);
                }
                else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                    return analyzeRecordSet(statement);
                }
                else if constexpr (std::is_same_v<T, ExprStmt>) {
                    analyzeExpr(statement.expression);
                    return true;
                }
                else if constexpr (std::is_same_v<T, ReturnStmt>) {
                    return analyzeReturn(statement);
                }
                else if constexpr (std::is_same_v<T, GiveStmt>) {
                    return analyzeGive(statement);
                }
                else if constexpr (std::is_same_v<T, IfStmt>) {
                    return analyzeIf(statement);
                }
                else if constexpr (std::is_same_v<T, EachStmt>) {
                    return analyzeEach(statement);
                }
                else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                    return analyzeUnsafe(statement);
                }
                else if constexpr (std::is_same_v<T, DeferStmt>) {
                    return analyzeDefer(statement);
                }
                else if constexpr (std::is_same_v<T, FailStmt>) {
                    return analyzeFail(statement);
                }
                else if constexpr (std::is_same_v<T, WhileStmt>) {
                    return analyzeWhile(statement);
                }
                else if constexpr (std::is_same_v<T, BreakStmt>) {
                    return analyzeBreak(statement);
                }
                else if constexpr (std::is_same_v<T, ContinueStmt>) {
                    return analyzeContinue(statement);
                }
                else if constexpr (std::is_same_v<T, BlockStmt>) {
                    return analyzeBlock(statement);
                }
                else {
                    return true;
                }
                }, node);
        }

        bool analyzeLet(const LetStmt& node) {
            if (!node.initializer) {
                report(node.location, "let binding requires an initializer");
                declareSymbol(node.location, node.name, { makeUnknownType(), false, false });
                return true;
            }
            auto valueType = analyzeExpr(node.initializer);
            SemanticType declaredType = semanticTypeFromName(node.typeName);
            if (declaredType.kind != SemanticTypeKind::Unknown) {
                if (!isAssignableTo(declaredType, valueType)) {
                    report(node.location, "let '" + node.name + "' initializer type mismatch: expected " + semanticTypeName(declaredType) + ", got " + semanticTypeName(valueType));
                }
                declareSymbol(node.location, node.name, { declaredType, false, false });
            }
            else {
                if (valueType.kind == SemanticTypeKind::Unknown) {
                    report(node.location, "cannot infer type of let binding '" + node.name + "'");
                }
                declareSymbol(node.location, node.name, { valueType, false, false });
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
                    declareSymbol(node.location, node.name, { declaredType, true, false });
                }
                else {
                    if (valueType.kind == SemanticTypeKind::Unknown) {
                        report(node.location, "cannot infer type of var binding '" + node.name + "'");
                    }
                    declareSymbol(node.location, node.name, { valueType, true, false });
                }
            }
            else {
                if (declaredType.kind == SemanticTypeKind::Unknown) {
                    report(node.location, "var '" + node.name + "' requires a type or initializer");
                    declareSymbol(node.location, node.name, { makeUnknownType(), true, false });
                }
                else {
                    declareSymbol(node.location, node.name, { declaredType, true, false });
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

        bool analyzeRecordSet(const RecordSetStmt& node) {
            auto* symbol = lookupSymbol(node.name);
            if (!symbol) { report(node.location, "E0410 field assignment to undeclared name: " + node.name); analyzeExpr(node.value); return true; }
            if (!symbol->isMutable)report(node.location, "E0415 field write requires a mutable record place: " + node.name);
            const auto* record = findNominalType(symbol->type.name);
            if (!record || record->kind != "record") {
                report(node.location, "E0313 field assignment requires a record place"); analyzeExpr(node.value); return true;
            }
            const auto field = std::find_if(record->fields.begin(), record->fields.end(), [&](const auto& f) {return f.first == node.field; });
            if (field == record->fields.end()) {
                report(node.location, "E0314 unknown record field: " + node.field); analyzeExpr(node.value); return true;
            }
            auto value = analyzeExpr(node.value);
            if (!isAssignableTo(semanticTypeFromName(field->second), value))
                report(node.location, "E0315 field assignment type mismatch: expected " + field->second + ", got " + semanticTypeName(value));
            if (field->second == "string")report(node.location, "E0426 owned string field writes require recursive drop glue");
            return true;
        }

        bool analyzeArraySet(const ArraySetStmt& node) {
            auto* symbol = lookupSymbol(node.name);
            if (!symbol) { report(node.location, "E0410 indexed assignment to unknown array: " + node.name); analyzeExpr(node.index); analyzeExpr(node.value); return true; }
            if (!symbol->isMutable)report(node.location, "E0415 indexed write requires a mutable array place: " + node.name);
            std::string element; std::size_t count = 0;
            if (!typeIsArray(symbol->type.name, &element, &count))report(node.location, "indexed assignment requires array<T,N>");
            const auto indexType = analyzeExpr(node.index);
            if (indexType.kind != SemanticTypeKind::Number || indexType.name == "float" || indexType.name == "double")
                report(node.location, "E0310 array write index must be integer");
            if (node.index)if (const auto* lit = std::get_if<NumberExpr>(&node.index->node)) {
                try {
                    if (lit->value.find('.') != std::string::npos || std::stoull(lit->value) >= count)
                        report(node.location, "E0311 array write index out of bounds");
                }
                catch (...) { report(node.location, "E0311 invalid array write index"); }
            }
            const auto value = analyzeExpr(node.value);
            if (!element.empty() && !isAssignableTo(semanticTypeFromName(element), value))
                report(node.location, "E0312 array element assignment type mismatch: expected " + element + ", got " + semanticTypeName(value));
            // Owning aggregate elements require recursive drop/move; refuse until proven safe.
            if (element == "string")report(node.location, "E0426 indexed assignment of owning strings requires recursive drop glue");
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
                }
                else if (!isAssignableTo(currentReturnType_, valueType)) {
                    report(node.location, "return type mismatch in routine '" + currentRoutine_->name + "': expected " + semanticTypeName(currentReturnType_) + ", got " + semanticTypeName(valueType));
                }
            }
            else if (currentReturnType_.kind != SemanticTypeKind::Void) {
                report(node.location, "routine '" + currentRoutine_->name + "' must return " + semanticTypeName(currentReturnType_));
            }
            return false;
        }

        bool analyzeGive(const GiveStmt& node) {
            if (!currentRoutine_) { report(node.location, "give outside of routine"); return true; }
            auto valueType = analyzeExpr(node.value);
            if (currentReturnType_.kind == SemanticTypeKind::Void) {
                report(node.location, "routine '" + currentRoutine_->name + "' returns void, but give provides a value");
            }
            else {
                std::string okType, errType, optionType;
                if (typeIsResult(currentRoutine_->returnType, &okType, &errType)) {
                    if (valueType.name.rfind("__err<", 0) == 0) report(node.location, "give cannot produce err; use fail for result failure");
                    else if (valueType.name.rfind("__ok<", 0) == 0) { /* explicit ok accepted; detailed wrapper checked by result lowering */ }
                    else if (!isAssignableTo(semanticTypeFromName(okType), valueType)) report(node.location, "give value cannot be wrapped as ok<" + okType + ">");
                }
                else if (typeIsOption(currentRoutine_->returnType, &optionType)) {
                    if (valueType.name != "__none" && valueType.name.rfind("__some<", 0) != 0 && !isAssignableTo(semanticTypeFromName(optionType), valueType)) report(node.location, "give value cannot be wrapped as some<" + optionType + ">");
                }
                else if (!isAssignableTo(currentReturnType_, valueType)) {
                    report(node.location, "give type mismatch in routine '" + currentRoutine_->name + "': expected " + semanticTypeName(currentReturnType_) + ", got " + semanticTypeName(valueType));
                }
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
                declareSymbol(node.location, binding, { makeUnknownType(), node.bindingsMutable, false });
            }
            ++currentLoopDepth_;
            auto fallsThrough = analyzeStatementList(node.body, true);
            --currentLoopDepth_;
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
            auto errorType = analyzeExpr(node.expression);
            if (!currentRoutine_) { report(node.location, "fail outside of routine"); return false; }
            std::string okType, expectedError;
            if (!typeIsResult(currentRoutine_->returnType, &okType, &expectedError)) report(node.location, "E0502 fail requires a result<T,E> return type");
            else if (!isAssignableTo(semanticTypeFromName(expectedError), errorType)) report(node.location, "fail error type mismatch: expected " + expectedError + ", got " + semanticTypeName(errorType));
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
            ++currentLoopDepth_;
            analyzeStatementList(node.body, true);
            --currentLoopDepth_;
            popScope();
            return true;
        }

        bool analyzeBreak(const BreakStmt& node) {
            if (currentLoopDepth_ == 0) report(node.location, "break outside of loop");
            if (!node.label.empty()) report(node.location, "labeled break is parsed but label resolution is not yet implemented in this bootstrap compiler");
            return false;
        }

        bool analyzeContinue(const ContinueStmt& node) {
            if (currentLoopDepth_ == 0) report(node.location, "continue outside of loop");
            if (!node.label.empty()) report(node.location, "labeled continue is parsed but label resolution is not yet implemented in this bootstrap compiler");
            return false;
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
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    return evaluateConstantBool(node.expression);
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    if (node.op == "!" || node.op == "not") {
                        auto value = evaluateConstantBool(node.operand);
                        if (value.has_value()) {
                            return !*value;
                        }
                    }
                    return std::nullopt;
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
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
                }
                else {
                    return std::nullopt;
                }
                }, expr->node);
        }

        IRValueType binaryValueType(const std::string& op) const {
            if (op == "and" || op == "or" || op == "&&" || op == "||" || op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
                return makeIRBoolType();
            }
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" ||
                op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
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
            if (node.op == "-" || node.op == "~") {
                if (node.op == "~" && (operandType.name == "float" || operandType.name == "double"))
                    report(node.location, "E0346 bitwise complement requires integer type");
                if (!isNumericType(operandType) && operandType.kind != SemanticTypeKind::Unknown) {
                    report(node.location, "unary '" + node.op + "' requires a numeric operand, got " + semanticTypeName(operandType));
                }
                return isNumericType(operandType) ? operandType : (operandType.kind == SemanticTypeKind::Unknown ? makeNumberType() : makeUnknownType());
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
            if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/" || node.op == "%" ||
                node.op == "&" || node.op == "|" || node.op == "^" || node.op == "<<" || node.op == ">>") {
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

        SemanticType analyzeTextStringMemberCall(const MemberExpr& member, const CallExpr& call) {
            const auto objectType = analyzeExpr(member.object);
            for (const auto& arg : call.arguments) (void)analyzeExpr(arg);

            if (objectType.kind == SemanticTypeKind::Text && member.member == "to_string") {
                if (!call.arguments.empty()) {
                    report(call.location, "text.to_string() takes no arguments");
                }
                return makeStringType();
            }
            if (objectType.kind == SemanticTypeKind::String && member.member == "to_text") {
                if (!call.arguments.empty()) {
                    report(call.location, "string.to_text() takes no arguments");
                }
                return makeTextType();
            }
            if ((objectType.kind == SemanticTypeKind::Text || objectType.kind == SemanticTypeKind::String) && member.member == "count") {
                if (!call.arguments.empty())report(call.location, "text/string.count() takes no arguments");
                return { SemanticTypeKind::Number,"size" };
            }
            if (objectType.kind == SemanticTypeKind::Text && member.member == "equals") {
                if (call.arguments.size() != 1 || analyzeExpr(call.arguments[0]).kind != SemanticTypeKind::Text)
                    report(call.location, "text.equals requires one text argument");
                return makeBoolType();
            }
            if (objectType.kind == SemanticTypeKind::String && (member.member == "clear" || member.member == "dispose")) {
                if (!call.arguments.empty()) {
                    report(call.location, "string mutation method takes no arguments");
                }
                // Borrow/mutation authority is enforced by SAGE, not duplicated here.
                return makeVoidType();
            }

            report(call.location, "unknown " + semanticTypeName(objectType) + " member routine '" + member.member + "'");
            return makeUnknownType();
        }

        SemanticType analyzeCallExpr(const CallExpr& node) {
            if (const auto* member = node.callee ? std::get_if<MemberExpr>(&node.callee->node) : nullptr) {
                auto objectType = member->object ? analyzeExpr(member->object) : makeUnknownType();
                if (objectType.kind == SemanticTypeKind::Text || objectType.kind == SemanticTypeKind::String)
                    return analyzeTextStringMemberCall(*member, node);
            }
            if (const auto* callee = std::get_if<VariableExpr>(&node.callee->node)) {
                if (callee->name == "ok" || callee->name == "err" || callee->name == "some") {
                    if (node.arguments.size() != 1) { report(node.location, callee->name + " requires exactly one argument"); return makeUnknownType(); }
                    auto t = analyzeExpr(node.arguments.front());
                    return makeCustomType("__" + callee->name + "<" + semanticTypeName(t) + ">");
                }
                if (callee->name == "none") { if (!node.arguments.empty())report(node.location, "none requires no arguments"); return makeCustomType("__none"); }
                if (callee->name == "print") {
                    for (const auto& argument : node.arguments) {
                        analyzeExpr(argument);
                    }
                    return makeVoidType();
                }
                if (auto* symbol = lookupSymbol(callee->name); symbol && symbol->type.kind == SemanticTypeKind::Custom) {
                    auto cs = closureSignatures_.find(symbol->type.name);
                    if (cs != closureSignatures_.end()) {
                        auto argumentTypes = collectArgumentTypes(node.arguments);
                        if (argumentTypes.size() != cs->second.parameterTypes.size()) report(node.location, "closure call arity mismatch");
                        else for (std::size_t i = 0; i < argumentTypes.size(); ++i) if (!isAssignableTo(cs->second.parameterTypes[i], argumentTypes[i])) report(node.location, "closure argument type mismatch");
                        return cs->second.returnType;
                    }
                    if (currentRoutine_) {
                        for (const auto& constraint : currentRoutine_->constraints) {
                            if (constraint.subject == symbol->type.name && constraint.capability == "callable" && constraint.arguments.size() >= 2) {
                                for (const auto& argument : node.arguments) analyzeExpr(argument);
                                return semanticTypeFromName(constraint.arguments.back());
                            }
                        }
                    }
                }
                return resolveRoutineCall(callee->name, node.arguments, node.genericArguments, node.location);
            }
            analyzeExpr(node.callee);
            for (const auto& argument : node.arguments) {
                analyzeExpr(argument);
            }
            report(node.location, "call target must be a named routine");
            return makeUnknownType();
        }

        void checkNumericCompatibility(const BinaryExpr& node, const SemanticType& left, const SemanticType& right) {
            if (left.kind == SemanticTypeKind::Number && right.kind == SemanticTypeKind::Number &&
                left.name != "number" && right.name != "number" && left.name != right.name)
                report(node.location, "E0345 no implicit conversion between numeric types '" + left.name + "' and '" + right.name + "'");
        }

        SemanticType analyzeArithmeticBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
            if ((!isNumericType(leftType) || !isNumericType(rightType)) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
                report(node.location, "arithmetic operator '" + node.op + "' requires numbers");
            }
            checkNumericCompatibility(node, leftType, rightType);
            if ((leftType.name == "float" || leftType.name == "double" || rightType.name == "float" || rightType.name == "double") &&
                (node.op == "%" || node.op == "&" || node.op == "|" || node.op == "^" || node.op == "<<" || node.op == ">>"))
                report(node.location, "E0346 integer-only operator applied to floating-point operand");
            if (isNumericType(leftType) && leftType.name != "number") return leftType;
            if (isNumericType(rightType) && rightType.name != "number") return rightType;
            return (isNumericType(leftType) || isNumericType(rightType) || leftType.kind == SemanticTypeKind::Unknown || rightType.kind == SemanticTypeKind::Unknown)
                ? makeNumberType() : makeUnknownType();
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
            checkNumericCompatibility(node, leftType, rightType);
            return makeBoolType();
        }

        SemanticType analyzeChooseExpr(const ChooseExpr& node) {
            auto subjectType = node.subject ? analyzeExpr(node.subject) : makeUnknownType();
            if (!node.hasOtherwise) report(node.location, "non-exhaustive choose expression requires otherwise");
            SemanticType resultType = makeUnknownType();
            for (const auto& choice : node.cases) {
                const auto conditionType = analyzeExpr(choice.condition);
                if (!node.subject && conditionType.kind != SemanticTypeKind::Bool && conditionType.kind != SemanticTypeKind::Unknown)
                    report(choice.location, "subjectless choose when-clause requires bool");
                if (node.subject && !isAssignableTo(subjectType, conditionType) && !isAssignableTo(conditionType, subjectType))
                    report(choice.location, "choose subject and when-clause must have compatible types");
                auto branchType = analyzeExpr(choice.value);
                if (resultType.kind == SemanticTypeKind::Unknown) {
                    resultType = branchType;
                }
                else if (!isAssignableTo(resultType, branchType) && !isAssignableTo(branchType, resultType)) {
                    report(choice.location, "choose branches must produce compatible types");
                }
            }
            if (node.hasOtherwise) {
                auto otherwiseType = analyzeExpr(node.otherwiseValue);
                if (resultType.kind == SemanticTypeKind::Unknown) {
                    resultType = otherwiseType;
                }
                else if (!isAssignableTo(resultType, otherwiseType) && !isAssignableTo(otherwiseType, resultType)) {
                    report(node.location, "otherwise branch must match choose branch types");
                }
            }
            return resultType;
        }

        SemanticType analyzeEqualityBinary(const BinaryExpr& node, const SemanticType& leftType, const SemanticType& rightType) {
            if (!isAssignableTo(leftType, rightType) && !isAssignableTo(rightType, leftType) && leftType.kind != SemanticTypeKind::Unknown && rightType.kind != SemanticTypeKind::Unknown) {
                report(node.location, "equality comparison requires compatible types");
            }
            if (isNumericType(leftType) && isNumericType(rightType))checkNumericCompatibility(node, leftType, rightType);
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
                }
                else if constexpr (std::is_same_v<T, StringExpr>) {
                    return makeTextType();
                }
                else if constexpr (std::is_same_v<T, BoolExpr>) {
                    return makeBoolType();
                }
                else if constexpr (std::is_same_v<T, NullExpr>) {
                    return makeNullType();
                }
                else if constexpr (std::is_same_v<T, VariableExpr>) {
                    auto* symbol = lookupSymbol(node.name);
                    if (symbol) {
                        return symbol->type;
                    }
                    if (routines_.contains(node.name)) {
                        report(node.location, "routine name used without call: " + node.name);
                    }
                    else {
                        report(node.location, "unknown name: " + node.name);
                    }
                    return makeUnknownType();
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    return analyzeUnaryExpr(node);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    return analyzeBinaryExpr(node);
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    analyzeExpr(node.input);
                    for (const auto& stage : node.stages) analyzeExpr(stage);
                    return makeUnknownType();
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    if (node.start) analyzeExpr(node.start);
                    if (node.end) analyzeExpr(node.end);
                    if (node.step) analyzeExpr(node.step);
                    return makeCustomType("range");
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    return analyzeChooseExpr(node);
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    return analyzeCallExpr(node);
                }
                else if constexpr (std::is_same_v<T, TryExpr>) {
                    auto operandType = analyzeExpr(node.operand);
                    std::string okType, errType;
                    if (!typeIsResult(semanticTypeName(operandType), &okType, &errType)) { report(node.location, "E0500 try requires result<T,E>"); return makeUnknownType(); }
                    if (!currentRoutine_) return semanticTypeFromName(okType);
                    std::string currentOk, currentErr;
                    if (!typeIsResult(currentRoutine_->returnType, &currentOk, &currentErr) || currentErr != errType)
                        report(node.location, "E0501 try cannot propagate error '" + errType + "' from routine returning " + currentRoutine_->returnType);
                    return semanticTypeFromName(okType);
                }
                else if constexpr (std::is_same_v<T, ClosureExpr>) {
                    std::string closureType = "closure$" + std::to_string(node.closureId);
                    RoutineSignature sig; sig.name = closureType;
                    pushScope();
                    for (const auto& p : node.parameters) {
                        if (p.typeName.empty()) report(p.location, "closure parameter '" + p.name + "' requires a type without an expected callable context");
                        auto pt = semanticTypeFromName(p.typeName); sig.parameterNames.push_back(p.name); sig.parameterTypes.push_back(pt); sig.parameterModes.push_back(p.mode); declareSymbol(p.location, p.name, { pt,p.mode == "var",true });
                    }
                    SemanticType rt = node.returnType.empty() ? makeUnknownType() : semanticTypeFromName(node.returnType);
                    if (node.hasExpressionBody) { auto bodyType = analyzeExpr(node.expressionBody); if (rt.kind == SemanticTypeKind::Unknown)rt = bodyType; else if (!isAssignableTo(rt, bodyType))report(node.location, "closure return type mismatch"); }
                    else { auto saved = currentReturnType_; auto savedRoutine = currentRoutine_; currentReturnType_ = rt.kind == SemanticTypeKind::Unknown ? makeVoidType() : rt; analyzeStatementList(node.blockBody); currentReturnType_ = saved; currentRoutine_ = savedRoutine; if (rt.kind == SemanticTypeKind::Unknown)rt = makeVoidType(); }
                    popScope(); sig.returnType = rt; closureSignatures_[closureType] = sig; return makeCustomType(closureType);
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    return analyzeExpr(node.expression);
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    return analyzeMemberExpr(node);
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    return analyzeConstructExpr(node);
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    if (node.elements.empty()) { report(node.location, "empty array literal requires explicit constructor support"); return makeUnknownType(); }
                    auto first = analyzeExpr(node.elements.front());
                    std::string element = first.kind == SemanticTypeKind::Number && first.name == "number" ?
                        (std::holds_alternative<NumberExpr>(node.elements.front()->node) &&
                            std::get<NumberExpr>(node.elements.front()->node).value.find('.') != std::string::npos ? "double" : "int")
                        : semanticTypeName(first);
                    if (element == "unknown" || element == "void" || element == "string") report(node.location, "unsupported or owning element in fixed-array literal");
                    for (std::size_t n = 1; n < node.elements.size(); ++n) {
                        auto next = analyzeExpr(node.elements[n]);
                        if (!isAssignableTo(semanticTypeFromName(element), next)) report(node.location, "array literal elements must have a common type");
                    }
                    return makeCustomType("array<" + element + "," + std::to_string(node.elements.size()) + ">");
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    const auto base = analyzeExpr(node.object);
                    auto idx = analyzeExpr(node.index);
                    std::string element; std::size_t count = 0;
                    if (!typeIsArray(base.name, &element, &count)) { report(node.location, "indexing requires array<T,N>"); return makeUnknownType(); }
                    if (idx.kind != SemanticTypeKind::Number)report(node.location, "array index must be integer");
                    if (auto* lit = std::get_if<NumberExpr>(&node.index->node)) {
                        if (lit->value.find('.') != std::string::npos)report(node.location, "array index must be integer");
                        else try { if (std::stoull(lit->value) >= count)report(node.location, "array index out of bounds"); }
                        catch (...) { report(node.location, "invalid array index"); }
                    }
                    return semanticTypeFromName(element);
                }
                else {
                    return makeUnknownType();
                }
                }, expr->node);
        }

        const NominalTypeInfo* findNominalType(const std::string& name) const {
            auto it = result_.nominalTypes.find(name);
            return it != result_.nominalTypes.end() ? &it->second : nullptr;
        }

        SemanticType nominalSemanticType(const std::string& name) const {
            if (const auto* nominal = findNominalType(name)) {
                return makeCustomType(nominal->name);
            }
            return semanticTypeFromName(name);
        }

        SemanticType analyzeMemberExpr(const MemberExpr& node) {
            auto objectType = analyzeExpr(node.object);
            if (objectType.kind == SemanticTypeKind::Custom) {
                if (const auto* nominal = findNominalType(objectType.name)) {
                    for (const auto& field : nominal->fields) {
                        if (field.first == node.member) {
                            return semanticTypeFromName(field.second);
                        }
                    }
                }
            }
            report(node.location, "unknown member access: " + semanticTypeName(objectType) + "." + node.member);
            return makeUnknownType();
        }

        SemanticType analyzeConstructExpr(const ConstructExpr& node) {
            const auto* nominal = findNominalType(node.typeName);
            for (const auto& field : node.fields) {
                if (field.value) {
                    analyzeExpr(field.value);
                }
            }
            if (!nominal) {
                report(node.location, "unknown nominal type: " + node.typeName);
                return makeUnknownType();
            }
            if (nominal->kind == "record" || nominal->kind == "union") {
                std::unordered_set<std::string> supplied;
                for (const auto& field : node.fields) {
                    if (!supplied.insert(field.name).second) report(field.location, "duplicate field '" + field.name + "' in construction of " + node.typeName);
                    auto it = std::find_if(nominal->fields.begin(), nominal->fields.end(), [&](const auto& candidate) { return candidate.first == field.name; });
                    if (it == nominal->fields.end()) {
                        report(field.location, "unknown field '" + field.name + "' for type " + node.typeName);
                        continue;
                    }
                    if (field.value) {
                        auto valueType = analyzeExpr(field.value);
                        if (!isAssignableTo(semanticTypeFromName(it->second), valueType)) {
                            report(field.location, "field '" + field.name + "' type mismatch");
                        }
                    }
                }
                if (nominal->kind == "record") for (const auto& required : nominal->fields) if (!supplied.contains(required.first)) report(node.location, "missing field '" + required.first + "' in construction of " + node.typeName);
            }
            else if (nominal->kind == "enum") {
                if (node.fields.size() > 1) {
                    report(node.location, "enum construction accepts at most one value");
                }
            }
            else if (nominal->kind == "variant") {
                if (node.fields.size() > 1) {
                    report(node.location, "variant construction accepts at most one payload field");
                }
            }
            return makeCustomType(node.typeName);
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
            auto global = globalConstTypes_.find(name);
            return global == globalConstTypes_.end() ? nullptr : &global->second;
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

        SemanticType resolveRoutineCall(const std::string& name, const std::vector<ExprPtr>& arguments, const std::vector<std::string>& explicitGenericArguments, SourceLocation location) {
            auto it = routines_.find(name);
            if (it == routines_.end()) {
                report(location, "unknown routine: " + name);
                for (const auto& argument : arguments) {
                    analyzeExpr(argument);
                }
                return makeUnknownType();
            }

            std::vector<SemanticType> argumentTypes = collectArgumentTypes(arguments);

            std::vector<SemanticAnalyzer::Candidate> candidates = buildCandidates(it->second, argumentTypes, explicitGenericArguments);

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

            const auto& best = candidates.front();
            return semanticTypeFromName(substituteGenericType(semanticTypeName(best.signature->returnType), best.substitutions));
        }

        struct Candidate {
            const RoutineSignature* signature = nullptr;
            std::size_t score = 0;
            std::map<std::string, std::string> substitutions;
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

        std::vector<SemanticAnalyzer::Candidate> buildCandidates(const std::vector<RoutineSignature>& overloads, const std::vector<SemanticType>& argumentTypes, const std::vector<std::string>& explicitGenericArguments) {
            std::vector<SemanticAnalyzer::Candidate> candidates;
            for (const auto& signature : overloads) {
                if (signature.parameterTypes.size() != argumentTypes.size()) continue;
                if (!explicitGenericArguments.empty() && explicitGenericArguments.size() != signature.genericParameters.size()) continue;
                std::unordered_set<std::string> genericNames(signature.genericParameters.begin(), signature.genericParameters.end());
                std::map<std::string, std::string> substitutions;
                for (std::size_t i = 0; i < explicitGenericArguments.size(); ++i) substitutions[signature.genericParameters[i]] = explicitGenericArguments[i];
                bool compatible = true; std::size_t score = 0;
                for (std::size_t i = 0; i < argumentTypes.size(); ++i) {
                    std::string patternText = semanticTypeName(signature.parameterTypes[i]);
                    std::string actualText = semanticTypeName(argumentTypes[i]);
                    if (!genericNames.empty() && actualText == "number") actualText = "int";
                    if (!genericNames.empty()) {
                        try {
                            auto p = CanonicalTypeParser(patternText).parse();
                            if (!p.tuple && p.args.empty() && genericNames.contains(p.head)) {
                                auto existing = substitutions.find(p.head);
                                if (existing == substitutions.end()) { substitutions[p.head] = actualText; ++score; continue; }
                                if (existing->second == actualText || (isNumericTypeName(existing->second) && isNumericTypeName(actualText))) { ++score; continue; }
                                compatible = false; break;
                            }
                            auto a = CanonicalTypeParser(actualText).parse();
                            if (!matchGenericTypePattern(p, a, genericNames, substitutions)) { compatible = false; break; }
                            ++score; continue;
                        }
                        catch (...) {}
                    }
                    if (!isAssignableTo(signature.parameterTypes[i], argumentTypes[i])) { compatible = false; break; }
                    if (argumentTypes[i] == signature.parameterTypes[i]) ++score;
                }
                if (compatible) candidates.push_back(SemanticAnalyzer::Candidate{ &signature, score, std::move(substitutions) });
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
            result_.diagnostics.push_back({ location, std::move(message) });
        }

        void report(std::string message) {
            report({}, std::move(message));
        }

        SemanticAnalysisResult result_;
        std::unordered_map<std::string, SymbolInfo> globalConstTypes_;
        std::unordered_map<std::string, std::vector<RoutineSignature>> routines_;
        std::unordered_map<std::string, NominalRecord> nominalTypes_;
        std::unordered_map<std::string, RoutineSignature> closureSignatures_;
        std::vector<SemanticScope> scopes_;
        std::vector<std::unordered_set<std::string>> declarationScopes_;
        const Routine* currentRoutine_ = nullptr;
        SemanticType currentReturnType_ = makeVoidType();
        std::size_t currentLoopDepth_ = 0;
    };


    // --- Semantic Knowledge Model materialization -------------------------------
    class SemanticKnowledgeBuilder {
    public:
        void build(const Program& program, SemanticAnalysisResult& result) {
            program_ = &program; result_ = &result; nextSymbolId_ = 1; nextBorrowId_ = 1;
            routines_.clear(); capabilities_.clear(); specializationKeys_.clear(); closureSignatures_.clear();
            for (const auto& r : program.routines) routines_[r.name].push_back(&r);
            indexDeclarations(program.declarations);
            buildLayouts();
            for (const auto& r : program.routines) buildRoutine(r);
            validateBorrowGraph();
            result.skm.structuralFacts.push_back("semantic knowledge model materialized deterministically");
            program_ = nullptr; result_ = nullptr;
        }

    private:
        struct LiveSymbol { std::size_t factIndex = 0; std::string typeName; bool mutableBinding = false; std::string mode = "shared"; };
        using Scope = std::unordered_map<std::string, LiveSymbol>;
        struct CaptureUse { BorrowKind kind = BorrowKind::Shared; std::uint64_t symbolId = 0; std::string typeName; SourceLocation location; };
        struct ClosureSignatureFact { std::vector<std::string> parameters; std::string returnType; };

        static std::string keyFor(const Routine& r) { return r.name + "#" + std::to_string(r.parameters.size()); }
        static BorrowKind stronger(BorrowKind a, BorrowKind b) {
            auto rank = [](BorrowKind k) {return k == BorrowKind::Owned ? 2 : k == BorrowKind::Exclusive ? 1 : 0; }; return rank(a) >= rank(b) ? a : b;
        }

        void indexDeclarations(const std::vector<Declaration>& declarations) {
            for (const auto& d : declarations) std::visit([&](const auto& node) { using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, CapabilityDecl>) capabilities_[node.name] = &node;
            else if constexpr (std::is_same_v<T, ForeignDecl>) routines_[node.routine.name].push_back(&node.routine);
            else if constexpr (std::is_same_v<T, Routine>) routines_[node.name].push_back(&node);
            else if constexpr (std::is_same_v<T, ModuleDecl>) indexDeclarations(node.declarations);
                }, d.node);
        }

        std::pair<std::size_t, std::size_t> layoutOf(const std::string& type, std::unordered_set<std::string>* stack = nullptr) const {
            static const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> primitive = {
                { "byte",{ 1,1 } },{ "ubyte",{ 1,1 } },{ "bool",{ 1,1 } },{ "short",{ 2,2 } },{ "ushort",{ 2,2 } },
                { "int",{ 4,4 } },{ "uint",{ 4,4 } },{ "float",{ 4,4 } },{ "char",{ 4,4 } },{ "long",{ 8,8 } },{ "ulong",{ 8,8 } },
                { "size",{ 8,8 } },{ "ssize",{ 8,8 } },{ "double",{ 8,8 } },{ "raw",{ 8,8 } },{ "text",{ 16,8 } },{ "string",{ 24,8 } }
            };
            if (auto it = primitive.find(type); it != primitive.end())return it->second;
            try {
                auto t = CanonicalTypeParser(type).parse();
                if (t.head == "raw")return { 8,8 };
                if (t.head == "view" || t.head == "span" || t.head == "ref" || t.head == "mut")return { 16,8 };
                if (t.head == "buffer")return { 24,8 };
                if (t.head == "array" && t.args.size() == 2) { auto e = layoutOf(canonicalTypeText(t.args[0]), stack); return { e.first,e.second }; }
                if (t.head == "option" && t.args.size() == 1) { auto v = canonicalTypeText(t.args[0]); auto l = layoutOf(v, stack); bool niche = v == "bool" || v == "char" || v == "text" || v.rfind("view<", 0) == 0 || v.rfind("span<", 0) == 0 || v.rfind("buffer<", 0) == 0; return niche ? l : std::pair<std::size_t, std::size_t>{ alignUpSize(l.first,l.second) + l.second,l.second }; }
                if (t.head == "result" && t.args.size() == 2) { auto a = layoutOf(canonicalTypeText(t.args[0]), stack), b = layoutOf(canonicalTypeText(t.args[1]), stack); auto al = std::max<std::size_t>({ a.second,b.second,1 }); auto payload = std::max(a.first, b.first); return { alignUpSize(1,al) + alignUpSize(payload,al),al }; }
                if (t.tuple) { std::size_t off = 0, al = 1; for (const auto& x : t.args) { auto l = layoutOf(canonicalTypeText(x), stack); al = std::max(al, l.second); off = alignUpSize(off, l.second) + l.first; }return { alignUpSize(off,al),al }; }
            }
            catch (...) {}
            auto it = result_->nominalTypes.find(type); if (it == result_->nominalTypes.end())return { 8,8 };
            std::unordered_set<std::string> localStack; if (!stack)stack = &localStack; if (stack->contains(type))return { 0,1 }; stack->insert(type);
            const auto& n = it->second; std::size_t off = 0, al = 1;
            if (n.kind == "record" || n.kind == "union") {
                if (n.kind == "union") { std::size_t mx = 0; for (const auto& f : n.fields) { auto l = layoutOf(f.second, stack); mx = std::max(mx, l.first); al = std::max(al, l.second); }off = alignUpSize(mx, al); }
                else {
                    for (const auto& f : n.fields) { auto l = layoutOf(f.second, stack); al = std::max(al, l.second); off = alignUpSize(off, l.second) + l.first; }off = alignUpSize(off, al);
                }
            }
            else if (n.kind == "enum") { off = 4; al = 4; }
            else if (n.kind == "variant") { off = 16; al = 8; }
            stack->erase(type); return { off,al };
        }
        static std::size_t alignUpSize(std::size_t v, std::size_t a) { return a ? ((v + a - 1) / a) * a : v; }

        void buildLayouts() {
            for (const auto& [name, n] : result_->nominalTypes) {
                auto [size, align] = layoutOf(name); SKMLayoutFact f; f.typeName = name; f.size = size; f.alignment = align;
                if (n.kind == "record") { std::size_t off = 0; for (const auto& field : n.fields) { auto l = layoutOf(field.second); off = alignUpSize(off, l.second); f.fieldOffsets.push_back({ field.first,off }); off += l.first; } }
                result_->skm.layouts.push_back(std::move(f));
            }
            std::unordered_set<std::string> seen; for (const auto& f : result_->skm.layouts)seen.insert(f.typeName);
            auto add = [&](const std::string& t) {if (t.empty() || t == "void" || t == "unknown" || seen.contains(t))return; if (t.size() == 1 && std::isupper(static_cast<unsigned char>(t[0])))return; auto [sz, al] = layoutOf(t); result_->skm.layouts.push_back(SKMLayoutFact{ t,sz,al,t.rfind("option<",0) == 0,{} }); seen.insert(t); };
            for (const std::string t : {"text", "string", "option<bool>", "option<char>"})add(t);
            for (const auto& [_, rs] : routines_)for (const auto* r : rs) { add(r->returnType); for (const auto& p : r->parameters)add(p.typeName); }
        }

        LiveSymbol* lookup(const std::string& name) {
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) { auto f = it->find(name); if (f != it->end())return &f->second; }return nullptr;
        }
        const LiveSymbol* lookup(const std::string& name) const {
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) { auto f = it->find(name); if (f != it->end())return &f->second; }return nullptr;
        }
        void touch(const std::string& name, std::size_t point) { if (auto* s = lookup(name)) { auto& f = result_->skm.symbols[s->factIndex]; if (!f.firstPoint)f.firstPoint = point; f.lastPoint = std::max(f.lastPoint, point); } }
        LiveSymbol declare(const std::string& name, const std::string& type, bool mut, bool param, const std::string& mode, SourceLocation loc, std::size_t point) {
            SKMSymbolFact f; f.id = nextSymbolId_++; f.routine = currentRoutine_; f.name = name; f.typeName = type; f.isMutable = mut; f.isParameter = param; f.parameterMode = mode; f.initialized = param; f.declaration = loc; f.firstPoint = point; f.lastPoint = point;
            result_->skm.symbols.push_back(f); LiveSymbol l{ result_->skm.symbols.size() - 1,type,mut,mode }; scopes_.back()[name] = l; return l;
        }

        bool textViewSource(const ExprPtr& expr, std::string& sourceName) const {
            if (!expr)return false;
            const auto* call = std::get_if<CallExpr>(&expr->node); if (!call || !call->callee)return false;
            const auto* member = std::get_if<MemberExpr>(&call->callee->node); if (!member || member->member != "to_text" || !call->arguments.empty())return false;
            const auto* source = member->object ? std::get_if<VariableExpr>(&member->object->node) : nullptr; if (!source)return false;
            sourceName = source->name; return true;
        }

        void bindTextViewBorrow(const LiveSymbol& view, const ExprPtr& initializer, SourceLocation loc) {
            std::string sourceName; if (!textViewSource(initializer, sourceName))return;
            auto* source = lookup(sourceName); if (!source || source->typeName != "string")return;
            const auto idx = addBorrow(*source, sourceName, BorrowKind::Shared, point_, point_, 0, false, loc);
            const auto viewId = result_->skm.symbols[view.factIndex].id; textViewBorrowBySymbol_[viewId] = idx;
            result_->skm.structuralFacts.push_back(currentRoutine_ + ": string.to_text borrow " + sourceName + " -> " + result_->skm.symbols[view.factIndex].name);
        }

        std::string inferExprType(const ExprPtr& e) {
            if (!e)return "unknown";
            return std::visit([&](const auto& n)->std::string {using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, NumberExpr>)return "int";
            else if constexpr (std::is_same_v<T, StringExpr>)return "text";
            else if constexpr (std::is_same_v<T, BoolExpr>)return "bool";
            else if constexpr (std::is_same_v<T, NullExpr>)return "null";
            else if constexpr (std::is_same_v<T, VariableExpr>) { if (auto* s = lookup(n.name))return s->typeName; return "unknown"; }
            else if constexpr (std::is_same_v<T, UnaryExpr>)return inferExprType(n.operand);
            else if constexpr (std::is_same_v<T, BinaryExpr>) { if (n.op == "==" || n.op == "!=" || n.op == "<" || n.op == "<=" || n.op == ">" || n.op == ">=" || n.op == "and" || n.op == "or" || n.op == "&&" || n.op == "||")return "bool"; return inferExprType(n.left); }
            else if constexpr (std::is_same_v<T, GroupExpr>)return inferExprType(n.expression);
            else if constexpr (std::is_same_v<T, ConstructExpr>)return n.typeName;
            else if constexpr (std::is_same_v<T, ArrayExpr>)return "array<" + (n.elements.empty() ? std::string("unknown") :
                std::holds_alternative<NumberExpr>(n.elements.front()->node) ?
                (std::get<NumberExpr>(n.elements.front()->node).value.find('.') != std::string::npos ? std::string("double") : std::string("int")) :
                inferExprType(n.elements.front())) + "," + std::to_string(n.elements.size()) + ">";
            else if constexpr (std::is_same_v<T, IndexExpr>) { std::string el; return typeIsArray(inferExprType(n.object), &el) ? el : "unknown"; }
            else if constexpr (std::is_same_v<T, ClosureExpr>)return "closure$" + std::to_string(n.closureId);
            else if constexpr (std::is_same_v<T, TryExpr>) { std::string ok, err; auto x = inferExprType(n.operand); return typeIsResult(x, &ok, &err) ? ok : "unknown"; }
            else if constexpr (std::is_same_v<T, MemberExpr>) { auto o = inferExprType(n.object); auto ni = result_->nominalTypes.find(o); if (ni != result_->nominalTypes.end())for (const auto& f : ni->second.fields)if (f.first == n.member)return f.second; return "unknown"; }
            else if constexpr (std::is_same_v<T, CallExpr>)return inferCallType(n);
            else if constexpr (std::is_same_v<T, ChooseExpr>) { if (!n.cases.empty())return inferExprType(n.cases.front().value); if (n.hasOtherwise)return inferExprType(n.otherwiseValue); return "unknown"; }
            else return "unknown";
                }, e->node);
        }

        const Routine* resolveCall(const CallExpr& call, std::map<std::string, std::string>* substitutions = nullptr) {
            auto* v = call.callee ? std::get_if<VariableExpr>(&call.callee->node) : nullptr; if (!v)return nullptr;
            auto it = routines_.find(v->name); if (it == routines_.end())return nullptr;
            for (const auto* r : it->second) {
                if (r->parameters.size() != call.arguments.size())continue; std::unordered_set<std::string> gens; for (const auto& g : r->genericParameters)gens.insert(g.name); std::map<std::string, std::string> sub;
                if (!call.genericArguments.empty()) { if (call.genericArguments.size() != r->genericParameters.size())continue; for (std::size_t i = 0; i < call.genericArguments.size(); ++i)sub[r->genericParameters[i].name] = call.genericArguments[i]; }
                bool ok = true; for (std::size_t i = 0; i < call.arguments.size(); ++i) { auto actual = inferExprType(call.arguments[i]); if (actual == "number") actual = "int"; auto pattern = r->parameters[i].typeName; if (gens.empty()) { if (pattern != actual && actual != "unknown" && !(actual == "int" && isNumericTypeName(pattern)))ok = false; } else try { auto pt = CanonicalTypeParser(pattern).parse(); if (!pt.tuple && pt.args.empty() && gens.contains(pt.head)) { auto ex = sub.find(pt.head); if (ex == sub.end()) { sub[pt.head] = actual; continue; }if (ex->second == actual || (isNumericTypeName(ex->second) && isNumericTypeName(actual)))continue; ok = false; } else if (!matchGenericTypePattern(pt, CanonicalTypeParser(actual).parse(), gens, sub))ok = false; } catch (...) { if (pattern != actual)ok = false; }if (!ok)break; }
                if (ok) { if (substitutions)*substitutions = std::move(sub); return r; }
            }return nullptr;
        }
        std::string inferCallType(const CallExpr& c) {
            if (auto* m = c.callee ? std::get_if<MemberExpr>(&c.callee->node) : nullptr) { auto o = inferExprType(m->object); if (o == "text" && m->member == "to_string")return "string"; if (o == "string" && m->member == "to_text")return "text"; if ((o == "text" || o == "string") && m->member == "count")return "size"; if (o == "text" && m->member == "equals")return "bool"; if (o == "string" && (m->member == "clear" || m->member == "dispose"))return "void"; }
            auto* v = c.callee ? std::get_if<VariableExpr>(&c.callee->node) : nullptr; if (v && (v->name == "ok" || v->name == "err" || v->name == "some") && c.arguments.size() == 1)return "__" + v->name + "<" + inferExprType(c.arguments[0]) + ">"; if (v && v->name == "none")return "__none"; std::map<std::string, std::string>s; if (auto* r = resolveCall(c, &s))return substituteGenericType(r->returnType, s); return "unknown";
        }

        void buildRoutine(const Routine& routine) {
            currentRoutine_ = keyFor(routine); point_ = 0; scopes_.clear(); scopes_.push_back({}); closureBindings_.clear(); closureLastUse_.clear(); closureBorrowIndexes_.clear(); textViewBorrowBySymbol_.clear();
            for (const auto& p : routine.parameters)declare(p.name, p.typeName, p.mode == "var", true, p.mode, p.location, ++point_);
            walkStatements(routine.body, false);
            const std::size_t endPoint = ++point_;
            for (auto& [cid, indexes] : closureBorrowIndexes_) { std::size_t end = closureLastUse_.contains(cid) ? closureLastUse_[cid] : endPoint; for (auto idx : indexes)result_->skm.borrowRegions[idx].endPoint = end; }
            currentRoutine_.clear(); scopes_.clear();
        }

        void walkStatements(const std::vector<StmtPtr>& stmts, bool inClosure) {
            for (const auto& s : stmts) {
                if (!s)continue; ++point_; std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>) { walkExpr(n.initializer, inClosure); auto type = n.typeName.empty() ? inferExprType(n.initializer) : n.typeName; auto declared = declare(n.name, type, std::is_same_v<T, VarStmt>, false, "shared", n.location, point_); bindTextViewBorrow(declared, n.initializer, n.location); if (n.initializer)if (auto* c = std::get_if<ClosureExpr>(&n.initializer->node))closureBindings_[n.name] = c->closureId; }
                else if constexpr (std::is_same_v<T, AssignStmt>) { touch(n.name, point_); walkExpr(n.value, inClosure); }
                else if constexpr (std::is_same_v<T, ArraySetStmt>) { touch(n.name, point_); walkExpr(n.index, inClosure); walkExpr(n.value, inClosure); result_->skm.structuralFacts.push_back(currentRoutine_ + ": checked array mutation"); }
                else if constexpr (std::is_same_v<T, RecordSetStmt>) { touch(n.name, point_); walkExpr(n.value, inClosure); result_->skm.structuralFacts.push_back(currentRoutine_ + ": record field mutation"); }
                else if constexpr (std::is_same_v<T, ExprStmt>)walkExpr(n.expression, inClosure);
                else if constexpr (std::is_same_v<T, ReturnStmt>) { if (n.hasValue) { markEscapingClosure(n.value); walkExpr(n.value, inClosure); } }
                else if constexpr (std::is_same_v<T, GiveStmt>) { markEscapingClosure(n.value); walkExpr(n.value, inClosure); }
                else if constexpr (std::is_same_v<T, FailStmt>)walkExpr(n.expression, inClosure);
                else if constexpr (std::is_same_v<T, DeferStmt>)walkExpr(n.expression, inClosure);
                else if constexpr (std::is_same_v<T, IfStmt>) { walkExpr(n.condition, inClosure); scopes_.push_back({}); walkStatements(n.thenBranch, inClosure); scopes_.pop_back(); scopes_.push_back({}); walkStatements(n.elseBranch, inClosure); scopes_.pop_back(); }
                else if constexpr (std::is_same_v<T, WhileStmt>) { result_->skm.structuralFacts.push_back(currentRoutine_ + ": loop"); walkExpr(n.condition, inClosure); scopes_.push_back({}); walkStatements(n.body, inClosure); scopes_.pop_back(); }
                else if constexpr (std::is_same_v<T, EachStmt>) { result_->skm.structuralFacts.push_back(currentRoutine_ + ": iterator/range loop"); walkExpr(n.range, inClosure); scopes_.push_back({}); for (const auto& b : n.bindings)declare(b, "int", n.bindingsMutable, false, "shared", n.location, point_); walkStatements(n.body, inClosure); scopes_.pop_back(); }
                else if constexpr (std::is_same_v<T, UnsafeStmt>) { result_->skm.structuralFacts.push_back(currentRoutine_ + ": unsafe boundary"); scopes_.push_back({}); walkStatements(n.body, inClosure); scopes_.pop_back(); }
                else if constexpr (std::is_same_v<T, BlockStmt>) { scopes_.push_back({}); walkStatements(n.statements, inClosure); scopes_.pop_back(); }
                    }, s->node);
            }
        }

        void walkExpr(const ExprPtr& e, bool inClosure) {
            if (!e)return; ++point_; std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, VariableExpr>) { touch(n.name, point_); auto cb = closureBindings_.find(n.name); if (cb != closureBindings_.end())closureLastUse_[cb->second] = point_; if (auto* ls = lookup(n.name)) { auto sid = result_->skm.symbols[ls->factIndex].id; auto tv = textViewBorrowBySymbol_.find(sid); if (tv != textViewBorrowBySymbol_.end())result_->skm.borrowRegions[tv->second].endPoint = point_; } }
            else if constexpr (std::is_same_v<T, UnaryExpr>)walkExpr(n.operand, inClosure);
            else if constexpr (std::is_same_v<T, BinaryExpr>) { walkExpr(n.left, inClosure); walkExpr(n.right, inClosure); }
            else if constexpr (std::is_same_v<T, PipelineExpr>) { walkExpr(n.input, inClosure); for (auto& x : n.stages)walkExpr(x, inClosure); }
            else if constexpr (std::is_same_v<T, RangeExpr>) { walkExpr(n.start, inClosure); walkExpr(n.end, inClosure); walkExpr(n.step, inClosure); }
            else if constexpr (std::is_same_v<T, ChooseExpr>) { walkExpr(n.subject, inClosure); for (auto& c : n.cases) { walkExpr(c.condition, inClosure); walkExpr(c.value, inClosure); }if (n.hasOtherwise)walkExpr(n.otherwiseValue, inClosure); }
            else if constexpr (std::is_same_v<T, GroupExpr>)walkExpr(n.expression, inClosure);
            else if constexpr (std::is_same_v<T, TryExpr>)walkExpr(n.operand, inClosure);
            else if constexpr (std::is_same_v<T, MemberExpr>)walkExpr(n.object, inClosure);
            else if constexpr (std::is_same_v<T, ConstructExpr>)for (auto& f : n.fields)walkExpr(f.value, inClosure);
            else if constexpr (std::is_same_v<T, ArrayExpr>)for (auto& item : n.elements)walkExpr(item, inClosure);
            else if constexpr (std::is_same_v<T, IndexExpr>) { walkExpr(n.object, inClosure); walkExpr(n.index, inClosure); }
            else if constexpr (std::is_same_v<T, CallExpr>)walkCall(n, inClosure);
            else if constexpr (std::is_same_v<T, ClosureExpr>)analyzeClosure(n);
                }, e->node);
        }

        void walkCall(const CallExpr& call, bool inClosure) {
            if (auto* m = call.callee ? std::get_if<MemberExpr>(&call.callee->node) : nullptr) {
                const std::string objectType = inferExprType(m->object);
                walkExpr(m->object, inClosure);
                if (auto* v = m->object ? std::get_if<VariableExpr>(&m->object->node) : nullptr) { if (auto* ls = lookup(v->name)) { static const std::unordered_set<std::string> mutators = { "clear", "dispose" }; if (objectType == "string" && mutators.contains(m->member))addBorrow(*ls, v->name, BorrowKind::Exclusive, point_, point_, 0, false, call.location); else if (objectType == "string" && m->member == "to_text")addBorrow(*ls, v->name, BorrowKind::Shared, point_, point_, 0, false, call.location); } }
                result_->skm.calls.push_back({ currentRoutine_,objectType + "." + m->member,call.location });
                for (const auto& a : call.arguments)walkExpr(a, inClosure);
                return;
            }
            auto* v = call.callee ? std::get_if<VariableExpr>(&call.callee->node) : nullptr; std::map<std::string, std::string>sub; const Routine* r = resolveCall(call, &sub);
            if (v) { touch(v->name, point_); auto cb = closureBindings_.find(v->name); if (cb != closureBindings_.end())closureLastUse_[cb->second] = point_; result_->skm.calls.push_back({ currentRoutine_,v->name,call.location }); if (r) { SKMEffectEdgeFact edge; edge.caller = currentRoutine_; edge.callee = keyFor(*r); auto it = result_->resolvedEffects.find(edge.callee); if (it != result_->resolvedEffects.end())edge.effects = it->second; result_->skm.effectEdges.push_back(std::move(edge)); } }
            if (r && !r->genericParameters.empty()) { std::ostringstream spec; spec << r->name << '$'; for (const auto& g : r->genericParameters) { if (g.isConst)continue; spec << g.name << '_' << sub[g.name] << '$'; }auto key = spec.str(); if (specializationKeys_.insert(key).second) { result_->skm.specializations.push_back({ r->name,key,sub,call.location }); resolveConstraints(*r, sub, call.location); } }
            std::unordered_map<std::string, std::vector<std::pair<std::size_t, std::string>>> same;
            for (std::size_t i = 0; i < call.arguments.size(); ++i) { std::string mode = (r && i < r->parameters.size()) ? r->parameters[i].mode : "shared"; if (auto* arg = std::get_if<VariableExpr>(&call.arguments[i]->node)) { touch(arg->name, point_); if (auto* ls = lookup(arg->name)) { BorrowKind kind = mode == "own" ? BorrowKind::Owned : mode == "var" ? BorrowKind::Exclusive : BorrowKind::Shared; addBorrow(*ls, arg->name, kind, point_, point_, 0, false, call.location); same[arg->name].push_back({ i,mode }); } }walkExpr(call.arguments[i], inClosure); }
            for (const auto& [name, uses] : same) { bool ex = false; for (auto& u : uses)if (u.second == "var")ex = true; if (ex && uses.size() > 1) { result_->diagnostics.push_back({ call.location,"E0415 exclusive borrow of '" + name + "' aliases another argument in the same call" }); result_->skm.aliases.push_back({ currentRoutine_,name,name,true,call.location }); } }
        }

        void validateBorrowGraph() {
            auto overlaps = [](const SKMBorrowRegionFact& a, const SKMBorrowRegionFact& b) {return a.beginPoint <= b.endPoint && b.beginPoint <= a.endPoint; };
            for (std::size_t i = 0; i < result_->skm.borrowRegions.size(); ++i)for (std::size_t j = i + 1; j < result_->skm.borrowRegions.size(); ++j) {
                const auto& a = result_->skm.borrowRegions[i]; const auto& b = result_->skm.borrowRegions[j];
                if (a.sourceSymbol != b.sourceSymbol || !overlaps(a, b))continue;
                if (a.kind == BorrowKind::Owned || b.kind == BorrowKind::Owned)continue;
                if (a.kind == BorrowKind::Exclusive || b.kind == BorrowKind::Exclusive) {
                    result_->diagnostics.push_back({ b.location,"E0416 overlapping nonlexical borrows of '" + b.sourceName + "': an exclusive region intersects another live borrow" });
                    result_->skm.aliases.push_back({ "borrow-graph",a.sourceName,b.sourceName,true,b.location });
                }
            }
        }

        std::size_t addBorrow(const LiveSymbol& ls, const std::string& name, BorrowKind kind, std::size_t begin, std::size_t end, std::size_t cid, bool escaping, SourceLocation loc) { auto& sf = result_->skm.symbols[ls.factIndex]; SKMBorrowRegionFact b; b.id = nextBorrowId_++; b.sourceSymbol = sf.id; b.sourceName = name; b.kind = kind; b.beginPoint = begin; b.endPoint = end; b.closureId = cid; b.escaping = escaping; b.location = loc; result_->skm.borrowRegions.push_back(b); const auto idx = result_->skm.borrowRegions.size() - 1; if (cid && kind != BorrowKind::Owned)closureBorrowIndexes_[cid].push_back(idx); return idx; }

        void inferClosureExprEffects(const ExprPtr& e, std::set<std::string>& out) {
            if (!e)return; std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, UnaryExpr>)inferClosureExprEffects(n.operand, out);
            else if constexpr (std::is_same_v<T, BinaryExpr>) { inferClosureExprEffects(n.left, out); inferClosureExprEffects(n.right, out); if (n.op == "/" || n.op == "%")out.insert("trap"); }
            else if constexpr (std::is_same_v<T, CallExpr>) { for (auto& a : n.arguments)inferClosureExprEffects(a, out); if (auto* m = n.callee ? std::get_if<MemberExpr>(&n.callee->node) : nullptr) { inferClosureExprEffects(m->object, out); if (m->member == "to_string")out.insert("alloc"); } else if (auto* v = n.callee ? std::get_if<VariableExpr>(&n.callee->node) : nullptr) { if (v->name == "print" || v->name == "println")out.insert("io"); auto ri = routines_.find(v->name); if (ri != routines_.end())for (auto* r : ri->second)if (r->parameters.size() == n.arguments.size()) { auto ef = result_->resolvedEffects.find(keyFor(*r)); if (ef != result_->resolvedEffects.end())out.insert(ef->second.begin(), ef->second.end()); break; } } }
            else if constexpr (std::is_same_v<T, PipelineExpr>) { inferClosureExprEffects(n.input, out); for (auto& x : n.stages)inferClosureExprEffects(x, out); }
            else if constexpr (std::is_same_v<T, RangeExpr>) { inferClosureExprEffects(n.start, out); inferClosureExprEffects(n.end, out); inferClosureExprEffects(n.step, out); }
            else if constexpr (std::is_same_v<T, ChooseExpr>) { inferClosureExprEffects(n.subject, out); for (auto& x : n.cases) { inferClosureExprEffects(x.condition, out); inferClosureExprEffects(x.value, out); }inferClosureExprEffects(n.otherwiseValue, out); }
            else if constexpr (std::is_same_v<T, TryExpr>)inferClosureExprEffects(n.operand, out);
            else if constexpr (std::is_same_v<T, GroupExpr>)inferClosureExprEffects(n.expression, out);
            else if constexpr (std::is_same_v<T, MemberExpr>)inferClosureExprEffects(n.object, out);
            else if constexpr (std::is_same_v<T, ConstructExpr>)for (auto& f : n.fields)inferClosureExprEffects(f.value, out);
            else if constexpr (std::is_same_v<T, ArrayExpr>)for (auto& item : n.elements)inferClosureExprEffects(item, out);
            else if constexpr (std::is_same_v<T, IndexExpr>) { inferClosureExprEffects(n.object, out); inferClosureExprEffects(n.index, out); out.insert("trap"); }
                }, e->node);
        }
        void inferClosureStmtEffects(const std::vector<StmtPtr>& ss, std::set<std::string>& out) {
            for (auto& s : ss)if (s)std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>)inferClosureExprEffects(n.initializer, out); else if constexpr (std::is_same_v<T, AssignStmt>)inferClosureExprEffects(n.value, out);
            else if constexpr (std::is_same_v<T, ArraySetStmt>) { inferClosureExprEffects(n.index, out); inferClosureExprEffects(n.value, out); out.insert("trap"); }
            else if constexpr (std::is_same_v<T, RecordSetStmt>)inferClosureExprEffects(n.value, out); else if constexpr (std::is_same_v<T, ExprStmt>)inferClosureExprEffects(n.expression, out); else if constexpr (std::is_same_v<T, ReturnStmt>) { if (n.hasValue)inferClosureExprEffects(n.value, out); }
            else if constexpr (std::is_same_v<T, GiveStmt>)inferClosureExprEffects(n.value, out); else if constexpr (std::is_same_v<T, FailStmt>)inferClosureExprEffects(n.expression, out); else if constexpr (std::is_same_v<T, IfStmt>) { inferClosureExprEffects(n.condition, out); inferClosureStmtEffects(n.thenBranch, out); inferClosureStmtEffects(n.elseBranch, out); }
            else if constexpr (std::is_same_v<T, WhileStmt>) { inferClosureExprEffects(n.condition, out); inferClosureStmtEffects(n.body, out); }
            else if constexpr (std::is_same_v<T, EachStmt>) { inferClosureExprEffects(n.range, out); inferClosureStmtEffects(n.body, out); }
            else if constexpr (std::is_same_v<T, UnsafeStmt>) { out.insert("unsafe"); inferClosureStmtEffects(n.body, out); }
            else if constexpr (std::is_same_v<T, DeferStmt>)inferClosureExprEffects(n.expression, out); else if constexpr (std::is_same_v<T, BlockStmt>)inferClosureStmtEffects(n.statements, out);
                }, s->node);
        }

        void analyzeClosure(const ClosureExpr& c) {
            ClosureSignatureFact sig; for (const auto& p : c.parameters) sig.parameters.push_back(p.typeName); sig.returnType = c.returnType.empty() ? "void" : c.returnType; closureSignatures_[c.closureId] = sig;
            std::set<std::string> inferredEffects; if (c.hasExpressionBody)inferClosureExprEffects(c.expressionBody, inferredEffects); else inferClosureStmtEffects(c.blockBody, inferredEffects);
            std::set<std::string> declared(c.effects.begin(), c.effects.end()); if (!declared.empty())for (const auto& e : inferredEffects)if (!declared.contains(e))result_->diagnostics.push_back({ c.location,"E0360 closure effect '" + e + "' exceeds declared effect set" });
            result_->skm.closures.push_back({ c.closureId,sig.parameters,sig.returnType,inferredEffects,c.moveCapture,c.location });
            std::unordered_set<std::string>locals; for (const auto& p : c.parameters)locals.insert(p.name); std::unordered_map<std::string, CaptureUse>captures;
            auto note = [&](const std::string& name, BorrowKind kind, SourceLocation loc) {if (locals.contains(name))return; if (auto* ls = lookup(name)) { auto& sf = result_->skm.symbols[ls->factIndex]; auto& cu = captures[name]; cu.kind = cu.symbolId ? stronger(cu.kind, kind) : kind; cu.symbolId = sf.id; cu.typeName = ls->typeName; cu.location = loc; touch(name, point_); }};
            std::function<void(const ExprPtr&)> scanExpr; std::function<void(const std::vector<StmtPtr>&)> scanStmts;
            scanExpr = [&](const ExprPtr& e) {if (!e)return; std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>; if constexpr (std::is_same_v<T, VariableExpr>)note(n.name, c.moveCapture ? BorrowKind::Owned : BorrowKind::Shared, n.location); else if constexpr (std::is_same_v<T, UnaryExpr>)scanExpr(n.operand); else if constexpr (std::is_same_v<T, BinaryExpr>) { scanExpr(n.left); scanExpr(n.right); } else if constexpr (std::is_same_v<T, CallExpr>) { auto* r = resolveCall(n); for (std::size_t i = 0; i < n.arguments.size(); ++i) { if (auto* v = std::get_if<VariableExpr>(&n.arguments[i]->node)) { BorrowKind k = BorrowKind::Shared; if (c.moveCapture)k = BorrowKind::Owned; else if (r && i < r->parameters.size()) { auto m = r->parameters[i].mode; k = m == "own" ? BorrowKind::Owned : m == "var" ? BorrowKind::Exclusive : BorrowKind::Shared; }note(v->name, k, v->location); }scanExpr(n.arguments[i]); } } else if constexpr (std::is_same_v<T, GroupExpr>)scanExpr(n.expression); else if constexpr (std::is_same_v<T, TryExpr>)scanExpr(n.operand); else if constexpr (std::is_same_v<T, MemberExpr>)scanExpr(n.object); else if constexpr (std::is_same_v<T, ConstructExpr>)for (auto& f : n.fields)scanExpr(f.value); else if constexpr (std::is_same_v<T, ArrayExpr>)for (auto& e : n.elements)scanExpr(e); else if constexpr (std::is_same_v<T, IndexExpr>) { scanExpr(n.object); scanExpr(n.index); } else if constexpr (std::is_same_v<T, PipelineExpr>) { scanExpr(n.input); for (auto& s : n.stages)scanExpr(s); } else if constexpr (std::is_same_v<T, RangeExpr>) { scanExpr(n.start); scanExpr(n.end); scanExpr(n.step); } else if constexpr (std::is_same_v<T, ChooseExpr>) { scanExpr(n.subject); for (auto& x : n.cases) { scanExpr(x.condition); scanExpr(x.value); }if (n.hasOtherwise)scanExpr(n.otherwiseValue); }}, e->node); };
            scanStmts = [&](const std::vector<StmtPtr>& ss) {for (auto& s : ss)std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>; if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>) { scanExpr(n.initializer); locals.insert(n.name); }
            else if constexpr (std::is_same_v<T, AssignStmt>) { if (!locals.contains(n.name))note(n.name, c.moveCapture ? BorrowKind::Owned : BorrowKind::Exclusive, n.location); scanExpr(n.value); }
            else if constexpr (std::is_same_v<T, ArraySetStmt>) { if (!locals.contains(n.name))note(n.name, BorrowKind::Exclusive, n.location); scanExpr(n.index); scanExpr(n.value); }
            else if constexpr (std::is_same_v<T, RecordSetStmt>) { if (!locals.contains(n.name))note(n.name, BorrowKind::Exclusive, n.location); scanExpr(n.value); }
            else if constexpr (std::is_same_v<T, ExprStmt>)scanExpr(n.expression); else if constexpr (std::is_same_v<T, ReturnStmt>) { if (n.hasValue)scanExpr(n.value); }
            else if constexpr (std::is_same_v<T, GiveStmt>)scanExpr(n.value); else if constexpr (std::is_same_v<T, FailStmt>)scanExpr(n.expression); else if constexpr (std::is_same_v<T, IfStmt>) { scanExpr(n.condition); scanStmts(n.thenBranch); scanStmts(n.elseBranch); }
            else if constexpr (std::is_same_v<T, WhileStmt>) { scanExpr(n.condition); scanStmts(n.body); }
            else if constexpr (std::is_same_v<T, EachStmt>) { scanExpr(n.range); for (auto& x : n.bindings)locals.insert(x); scanStmts(n.body); }
            else if constexpr (std::is_same_v<T, UnsafeStmt>)scanStmts(n.body); else if constexpr (std::is_same_v<T, DeferStmt>)scanExpr(n.expression); else if constexpr (std::is_same_v<T, BlockStmt>)scanStmts(n.statements); }, s->node); };
            if (c.hasExpressionBody)scanExpr(c.expressionBody); else scanStmts(c.blockBody);
            for (const auto& [name, cu] : captures) { result_->skm.closureCaptures.push_back({ c.closureId,cu.symbolId,name,cu.typeName,cu.kind,false,cu.location }); if (auto* ls = lookup(name))addBorrow(*ls, name, cu.kind, point_, point_, c.closureId, false, cu.location); }
            result_->skm.structuralFacts.push_back(currentRoutine_ + ": closure$" + std::to_string(c.closureId) + " captures=" + std::to_string(captures.size()));
        }

        void markEscapingClosure(const ExprPtr& e) { if (!e)return; std::size_t cid = 0; if (auto* c = std::get_if<ClosureExpr>(&e->node))cid = c->closureId; else if (auto* v = std::get_if<VariableExpr>(&e->node)) { auto it = closureBindings_.find(v->name); if (it != closureBindings_.end())cid = it->second; }if (!cid)return; for (auto& c : result_->skm.closureCaptures)if (c.closureId == cid) { c.escaping = true; if (c.kind != BorrowKind::Owned)result_->diagnostics.push_back({ c.location,"E0432 closure$" + std::to_string(cid) + " escapes while borrowing '" + c.name + "'; use move capture or keep the closure within the borrow region" }); }for (auto idx : closureBorrowIndexes_[cid])result_->skm.borrowRegions[idx].escaping = true; }

        bool primitiveCapability(const std::string& subject, const std::string& cap)const {
            bool scalar = isNumericTypeName(subject) || subject == "bool" || subject == "char" || subject == "text" || subject == "string";
            if (cap == "printable" || cap == "equatable")return scalar;
            if (cap == "ordered" || cap == "hashable")return scalar;
            if (cap == "defaultable")return scalar || result_->nominalTypes.contains(subject);
            if (subject == "string" && (cap == "appendable" || cap == "clearable" || cap == "resizable" || cap == "mutable_text"))return true;
            if (subject == "text" && (cap == "readable_text" || cap == "utf8_text"))return true;
            return false;
        }
        bool satisfyCapability(const std::string& subject, const std::string& cap, const std::vector<std::string>& args, std::vector<std::string>& evidence) {
            if (cap == "callable") {
                if (subject.rfind("closure$", 0) == 0) {
                    std::size_t id = 0; try { id = static_cast<std::size_t>(std::stoull(subject.substr(8))); }
                    catch (...) {}
                    auto it = closureSignatures_.find(id); if (it == closureSignatures_.end()) return false;
                    if (args.size() >= 2) {
                        std::vector<std::string> expected; try { auto tuple = CanonicalTypeParser(args.front()).parse(); if (tuple.tuple)for (auto& a : tuple.args)expected.push_back(canonicalTypeText(a)); else expected.push_back(args.front()); }
                        catch (...) { expected.push_back(args.front()); }
                        if (expected != it->second.parameters || args.back() != it->second.returnType) return false;
                    }
                    evidence.push_back("closure signature satisfies intrinsic callable relationship"); return true;
                }
                if (subject.rfind("routine", 0) == 0) { evidence.push_back("routine satisfies intrinsic callable relationship"); return true; }return false;
            }
            if (primitiveCapability(subject, cap)) { evidence.push_back("predeclared structural capability"); return true; }
            auto ci = capabilities_.find(cap); if (ci == capabilities_.end())return false; const auto* c = ci->second; for (const auto& m : c->members) { auto ri = routines_.find(m.name); bool found = false; if (ri != routines_.end())for (const auto* r : ri->second) { if (r->parameters.size() != m.parameters.size())continue; if (!r->parameters.empty() && r->parameters.front().typeName != subject)continue; found = true; evidence.push_back(r->name + "(" + subject + ")"); break; }if (!found)return false; }return true;
        }
        void resolveConstraints(const Routine& r, const std::map<std::string, std::string>& sub, SourceLocation loc) { for (const auto& c : r.constraints) { auto it = sub.find(c.subject); std::string subject = it == sub.end() ? c.subject : it->second; std::vector<std::string>args; for (const auto& a : c.arguments)args.push_back(substituteGenericType(a, sub)); std::vector<std::string>evidence; bool ok = satisfyCapability(subject, c.capability, args, evidence); result_->skm.capabilityResolutions.push_back({ subject,c.capability,args,ok,evidence,loc }); if (!ok)result_->diagnostics.push_back({ loc,"E0342 type '" + subject + "' does not satisfy capability '" + c.capability + "'" }); } }

        const Program* program_ = nullptr; SemanticAnalysisResult* result_ = nullptr;
        std::unordered_map<std::string, std::vector<const Routine*>> routines_; std::unordered_map<std::string, const CapabilityDecl*> capabilities_;
        std::vector<Scope> scopes_; std::string currentRoutine_; std::size_t point_ = 0; std::uint64_t nextSymbolId_ = 1, nextBorrowId_ = 1;
        std::unordered_map<std::string, std::size_t> closureBindings_; std::unordered_map<std::size_t, std::size_t> closureLastUse_; std::unordered_map<std::size_t, std::vector<std::size_t>> closureBorrowIndexes_;
        std::unordered_map<std::uint64_t, std::size_t> textViewBorrowBySymbol_;
        std::unordered_set<std::string> specializationKeys_;
        std::unordered_map<std::size_t, ClosureSignatureFact> closureSignatures_;
    };

    // --- SAGE semantic core: effects + ownership/borrow discipline --------------
    class SAGEEngine {
    public:
        void analyze(const Program& program, SemanticAnalysisResult& result) {
            result_ = &result;
            buildRoutineIndex(program);
            analyzeEffects(program);
            analyzeOwnership(program);
            SemanticKnowledgeBuilder skmBuilder;
            skmBuilder.build(program, result);
            result_ = nullptr;
        }

    private:
        struct BorrowSymbol {
            std::string typeName;
            bool isMutable = false;
            bool isParameter = false;
            std::string mode = "shared";
            OwnershipState ownership = OwnershipState::Available;
        };

        struct BorrowState {
            std::vector<std::unordered_map<std::string, BorrowSymbol>> scopes;
        };

        struct EffectFacts {
            std::set<std::string> direct;
            std::set<std::string> calls;
        };

        static std::string routineKey(const std::string& name, std::size_t arity) {
            return name + "#" + std::to_string(arity);
        }

        void buildRoutineIndex(const Program& program) {
            routines_.clear();
            for (const auto& routine : program.routines) {
                routines_[routineKey(routine.name, routine.parameters.size())] = &routine;
            }
            collectForeign(program.declarations);
        }

        void collectForeign(const std::vector<Declaration>& declarations) {
            for (const auto& declaration : declarations) {
                std::visit([&](const auto& node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, ForeignDecl>) {
                        routines_[routineKey(node.routine.name, node.routine.parameters.size())] = &node.routine;
                        foreign_.insert(routineKey(node.routine.name, node.routine.parameters.size()));
                    }
                    else if constexpr (std::is_same_v<T, Routine>) {
                        routines_[routineKey(node.name, node.parameters.size())] = &node;
                    }
                    else if constexpr (std::is_same_v<T, ModuleDecl>) {
                        collectForeign(node.declarations);
                    }
                    }, declaration.node);
            }
        }

        void analyzeEffects(const Program& program) {
            effectFacts_.clear();
            for (const auto& routine : program.routines) {
                auto key = routineKey(routine.name, routine.parameters.size());
                auto& facts = effectFacts_[key];
                collectStatementEffects(routine.body, facts);
            }
            for (const auto& [key, routine] : routines_) {
                if (foreign_.contains(key)) {
                    auto& facts = effectFacts_[key];
                    if (routine->effects.empty() || (routine->effects.size() == 1 && routine->effects.front() == "pure")) {
                        facts.direct.insert("io");
                        facts.direct.insert("trap");
                    }
                    else {
                        for (const auto& effect : routine->effects) if (effect != "pure") facts.direct.insert(effect);
                    }
                }
            }

            std::unordered_map<std::string, std::set<std::string>> resolved;
            for (const auto& [key, facts] : effectFacts_) resolved[key] = facts.direct;
            bool changed = true;
            for (std::size_t iteration = 0; changed && iteration < routines_.size() + 8; ++iteration) {
                changed = false;
                for (const auto& [key, facts] : effectFacts_) {
                    auto next = resolved[key];
                    for (const auto& callee : facts.calls) {
                        auto it = resolved.find(callee);
                        if (it == resolved.end()) continue;
                        next.insert(it->second.begin(), it->second.end());
                    }
                    if (next != resolved[key]) { resolved[key] = std::move(next); changed = true; }
                }
            }

            for (const auto& routine : program.routines) {
                auto key = routineKey(routine.name, routine.parameters.size());
                const auto& inferred = resolved[key];
                result_->resolvedEffects[key] = inferred;
                if (!routine.effects.empty()) {
                    std::set<std::string> allowed;
                    if (!(routine.effects.size() == 1 && routine.effects.front() == "pure")) {
                        for (const auto& e : routine.effects) allowed.insert(e);
                    }
                    for (const auto& e : inferred) {
                        if (!allowed.contains(e)) {
                            report(routine.location, "E0360 undeclared effect '" + e + "' in routine '" + routine.name + "'");
                        }
                    }
                }
            }
        }

        void collectStatementEffects(const std::vector<StmtPtr>& statements, EffectFacts& facts) {
            for (const auto& stmt : statements) {
                if (!stmt) continue;
                std::visit([&](const auto& node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>) {
                        if (node.initializer) collectExprEffects(node.initializer, facts);
                    }
                    else if constexpr (std::is_same_v<T, AssignStmt>) {
                        collectExprEffects(node.value, facts);
                    }
                    else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                        collectExprEffects(node.index, facts); collectExprEffects(node.value, facts); facts.direct.insert("trap");
                    }
                    else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                        collectExprEffects(node.value, facts);
                    }
                    else if constexpr (std::is_same_v<T, ExprStmt>) {
                        collectExprEffects(node.expression, facts);
                    }
                    else if constexpr (std::is_same_v<T, ReturnStmt>) {
                        if (node.hasValue) collectExprEffects(node.value, facts);
                    }
                    else if constexpr (std::is_same_v<T, GiveStmt>) {
                        collectExprEffects(node.value, facts);
                    }
                    else if constexpr (std::is_same_v<T, FailStmt>) {
                        collectExprEffects(node.expression, facts);
                    }
                    else if constexpr (std::is_same_v<T, IfStmt>) {
                        collectExprEffects(node.condition, facts);
                        collectStatementEffects(node.thenBranch, facts);
                        collectStatementEffects(node.elseBranch, facts);
                    }
                    else if constexpr (std::is_same_v<T, WhileStmt>) {
                        collectExprEffects(node.condition, facts);
                        collectStatementEffects(node.body, facts);
                    }
                    else if constexpr (std::is_same_v<T, EachStmt>) {
                        collectExprEffects(node.range, facts);
                        collectStatementEffects(node.body, facts);
                    }
                    else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                        // A locally encapsulated unsafe block is not automatically part of the caller contract.
                        collectStatementEffects(node.body, facts);
                    }
                    else if constexpr (std::is_same_v<T, DeferStmt>) {
                        collectExprEffects(node.expression, facts);
                    }
                    else if constexpr (std::is_same_v<T, BlockStmt>) {
                        collectStatementEffects(node.statements, facts);
                    }
                    }, stmt->node);
            }
        }

        void collectExprEffects(const ExprPtr& expr, EffectFacts& facts) {
            if (!expr) return;
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, UnaryExpr>) {
                    collectExprEffects(node.operand, facts);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    collectExprEffects(node.left, facts); collectExprEffects(node.right, facts);
                    if (node.op == "/" || node.op == "%") facts.direct.insert("trap");
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    for (const auto& a : node.arguments) collectExprEffects(a, facts);
                    if (const auto* member = node.callee ? std::get_if<MemberExpr>(&node.callee->node) : nullptr) {
                        collectExprEffects(member->object, facts);
                        if (member->member == "to_string") {
                            facts.direct.insert("alloc");
                        }
                    }
                    else if (const auto* variable = node.callee ? std::get_if<VariableExpr>(&node.callee->node) : nullptr) {
                        if (variable->name == "print" || variable->name == "println") facts.direct.insert("io");
                        auto key = routineKey(variable->name, node.arguments.size());
                        if (routines_.contains(key)) facts.calls.insert(key);
                    }
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    collectExprEffects(node.input, facts); for (const auto& s : node.stages) collectExprEffects(s, facts);
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    collectExprEffects(node.start, facts); collectExprEffects(node.end, facts); collectExprEffects(node.step, facts);
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    collectExprEffects(node.subject, facts);
                    for (const auto& c : node.cases) { collectExprEffects(c.condition, facts); collectExprEffects(c.value, facts); }
                    if (node.hasOtherwise) collectExprEffects(node.otherwiseValue, facts);
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    collectExprEffects(node.expression, facts);
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    collectExprEffects(node.object, facts);
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    for (const auto& f : node.fields) collectExprEffects(f.value, facts);
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    for (const auto& e : node.elements) collectExprEffects(e, facts);
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    collectExprEffects(node.object, facts); collectExprEffects(node.index, facts);
                    facts.direct.insert("trap");
                }
                }, expr->node);
        }

        void analyzeOwnership(const Program& program) {
            for (const auto& routine : program.routines) {
                BorrowState state;
                state.scopes.emplace_back();
                for (const auto& p : routine.parameters) {
                    state.scopes.back()[p.name] = BorrowSymbol{ p.typeName, p.mode == "var" || p.mode == "own", true, p.mode, OwnershipState::Available };
                }
                analyzeBorrowStatements(routine.body, state, false);
            }
        }

        BorrowSymbol* lookup(BorrowState& state, const std::string& name) {
            for (auto it = state.scopes.rbegin(); it != state.scopes.rend(); ++it) {
                auto found = it->find(name); if (found != it->end()) return &found->second;
            }
            return nullptr;
        }

        static bool isCopyType(const std::string& t) {
            static const std::unordered_set<std::string> copy = {
                "byte","ubyte","short","ushort","int","uint","long","ulong","size","ssize",
                "float","double","bool","char","raw","text","view","span","ref","mut"
            };
            return copy.contains(t);
        }

        std::string inferBorrowType(const ExprPtr& e, BorrowState& state) {
            if (!e) return {};
            if (const auto* v = std::get_if<VariableExpr>(&e->node)) {
                if (auto* s = lookup(state, v->name)) return s->typeName;
            }
            if (std::holds_alternative<NumberExpr>(e->node)) return "int";
            if (std::holds_alternative<BoolExpr>(e->node)) return "bool";
            if (std::holds_alternative<StringExpr>(e->node)) return "text";
            if (const auto* call = std::get_if<CallExpr>(&e->node)) {
                if (const auto* member = call->callee ? std::get_if<MemberExpr>(&call->callee->node) : nullptr) {
                    const auto sourceType = inferBorrowType(member->object, state);
                    if (sourceType == "text" && member->member == "to_string")return "string";
                    if (sourceType == "string" && member->member == "to_text")return "text";
                }
            }
            return {};
        }

        void readVariable(const VariableExpr& v, BorrowState& state) {
            if (auto* s = lookup(state, v.name)) {
                if (s->ownership == OwnershipState::Moved) report(v.location, "E0412 use after move: '" + v.name + "'");
            }
        }

        const Routine* resolveRoutine(const std::string& name, std::size_t arity) const {
            auto it = routines_.find(routineKey(name, arity));
            return it == routines_.end() ? nullptr : it->second;
        }

        void analyzeBorrowExpr(const ExprPtr& expr, BorrowState& state) {
            if (!expr) return;
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, VariableExpr>) {
                    readVariable(node, state);
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    analyzeBorrowExpr(node.operand, state);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    analyzeBorrowExpr(node.left, state); analyzeBorrowExpr(node.right, state);
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    if (const auto* member = node.callee ? std::get_if<MemberExpr>(&node.callee->node) : nullptr) {
                        analyzeBorrowExpr(member->object, state);
                        const std::string objectType = inferBorrowType(member->object, state);
                        static const std::unordered_set<std::string> mutators = { "clear", "dispose" };
                        if (objectType == "string" && mutators.contains(member->member)) {
                            const auto* variable = member->object ? std::get_if<VariableExpr>(&member->object->node) : nullptr;
                            auto* sym = variable ? lookup(state, variable->name) : nullptr;
                            if (member->member == "dispose") {
                                // Destruction consumes ownership. A `var` parameter is
                                // only an exclusive borrow, never an ownership transfer.
                                if (!sym || (sym->isParameter && sym->mode != "own"))
                                    report(node.location, "E0422 string disposal requires an owned local or own parameter");
                                else sym->ownership = OwnershipState::Moved;
                            }
                            else if (!sym || !(sym->isMutable || sym->mode == "var" || sym->mode == "own")) {
                                report(node.location, "E0417 cannot mutate string through a shared or immutable place; use var or own authority");
                            }
                        }
                        for (const auto& argument : node.arguments) analyzeBorrowExpr(argument, state);
                        return;
                    }
                    const Routine* callee = nullptr;
                    if (const auto* v = node.callee ? std::get_if<VariableExpr>(&node.callee->node) : nullptr) callee = resolveRoutine(v->name, node.arguments.size());
                    for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                        const auto& arg = node.arguments[i];
                        std::string mode = callee && i < callee->parameters.size() ? callee->parameters[i].mode : "shared";
                        if (const auto* v = arg ? std::get_if<VariableExpr>(&arg->node) : nullptr) {
                            auto* sym = lookup(state, v->name);
                            if (sym && sym->ownership == OwnershipState::Moved) report(v->location, "E0412 use after move: '" + v->name + "'");
                            if (sym && mode == "var" && !sym->isMutable) report(v->location, "E0415 exclusive borrow requires a mutable place: '" + v->name + "'");
                            if (sym && mode == "own" && !isCopyType(sym->typeName)) sym->ownership = OwnershipState::Moved;
                            if (sym && mode == "raw" && !isRawAuthorityTypeName(sym->typeName)) report(v->location, "E0408 raw parameter requires raw authority");
                        }
                        else {
                            if (mode == "var") report(arg ? arg->location : SourceLocation{}, "E0416 exclusive borrow requires a place expression");
                            analyzeBorrowExpr(arg, state);
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    analyzeBorrowExpr(node.input, state); for (const auto& x : node.stages) analyzeBorrowExpr(x, state);
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    analyzeBorrowExpr(node.start, state); analyzeBorrowExpr(node.end, state); analyzeBorrowExpr(node.step, state);
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    analyzeBorrowExpr(node.subject, state);
                    for (const auto& c : node.cases) { analyzeBorrowExpr(c.condition, state); analyzeBorrowExpr(c.value, state); }
                    if (node.hasOtherwise) analyzeBorrowExpr(node.otherwiseValue, state);
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    analyzeBorrowExpr(node.expression, state);
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    analyzeBorrowExpr(node.object, state);
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    for (const auto& f : node.fields) analyzeBorrowExpr(f.value, state);
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    for (const auto& item : node.elements)analyzeBorrowExpr(item, state);
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    analyzeBorrowExpr(node.object, state);
                    analyzeBorrowExpr(node.index, state);
                }
                }, expr->node);
        }

        static BorrowState mergeStates(const BorrowState& a, const BorrowState& b) {
            BorrowState out = a;
            const std::size_t n = std::min(out.scopes.size(), b.scopes.size());
            for (std::size_t i = 0; i < n; ++i) {
                for (auto& [name, symbol] : out.scopes[i]) {
                    auto it = b.scopes[i].find(name);
                    if (it != b.scopes[i].end() && (symbol.ownership == OwnershipState::Moved || it->second.ownership == OwnershipState::Moved)) {
                        symbol.ownership = OwnershipState::Moved;
                    }
                }
            }
            return out;
        }

        void analyzeBorrowStatements(const std::vector<StmtPtr>& statements, BorrowState& state, bool repeating) {
            for (const auto& stmt : statements) {
                if (!stmt) continue;
                std::visit([&](const auto& node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, LetStmt>) {
                        analyzeBorrowExpr(node.initializer, state);
                        if (node.initializer) if (const auto* source = std::get_if<VariableExpr>(&node.initializer->node)) {
                            if (auto* original = lookup(state, source->name))
                                if (original->typeName == "string") original->ownership = OwnershipState::Moved;
                        }
                        state.scopes.back()[node.name] = BorrowSymbol{ node.typeName.empty() ? inferBorrowType(node.initializer, state) : node.typeName, false, false, "shared", OwnershipState::Available };
                    }
                    else if constexpr (std::is_same_v<T, VarStmt>) {
                        analyzeBorrowExpr(node.initializer, state);
                        if (node.initializer) if (const auto* source = std::get_if<VariableExpr>(&node.initializer->node)) {
                            if (auto* original = lookup(state, source->name))
                                if (original->typeName == "string") original->ownership = OwnershipState::Moved;
                        }
                        state.scopes.back()[node.name] = BorrowSymbol{ node.typeName.empty() ? inferBorrowType(node.initializer, state) : node.typeName, true, false, "shared", OwnershipState::Available };
                    }
                    else if constexpr (std::is_same_v<T, AssignStmt>) {
                        analyzeBorrowExpr(node.value, state);
                        if (node.value) if (const auto* v = std::get_if<VariableExpr>(&node.value->node)) {
                            if (v->name != node.name) if (auto* prior = lookup(state, v->name))
                                if (prior->typeName == "string") prior->ownership = OwnershipState::Moved;
                        }
                        if (auto* dst = lookup(state, node.name)) dst->ownership = OwnershipState::Available;
                    }
                    else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                        if (auto* dst = lookup(state, node.name)) {
                            if (dst->ownership == OwnershipState::Moved)report(node.location, "E0412 field write after move: " + node.name);
                            if (dst->isParameter && dst->mode == "shared")report(node.location, "E0415 record field write requires var/own authority: " + node.name);
                        }
                        analyzeBorrowExpr(node.value, state);
                    }
                    else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                        if (auto* dst = lookup(state, node.name)) {
                            if (dst->ownership == OwnershipState::Moved)report(node.location, "E0412 indexed write after move: " + node.name);
                            if (dst->isParameter && dst->mode == "shared")report(node.location, "E0415 indexed write requires var/own authority: " + node.name);
                        }
                        analyzeBorrowExpr(node.index, state); analyzeBorrowExpr(node.value, state);
                    }
                    else if constexpr (std::is_same_v<T, ExprStmt>) analyzeBorrowExpr(node.expression, state);
                    else if constexpr (std::is_same_v<T, ReturnStmt>) { if (node.hasValue) analyzeBorrowExpr(node.value, state); }
                    else if constexpr (std::is_same_v<T, GiveStmt>) analyzeBorrowExpr(node.value, state);
                    else if constexpr (std::is_same_v<T, FailStmt>) analyzeBorrowExpr(node.expression, state);
                    else if constexpr (std::is_same_v<T, DeferStmt>) analyzeBorrowExpr(node.expression, state);
                    else if constexpr (std::is_same_v<T, IfStmt>) {
                        analyzeBorrowExpr(node.condition, state);
                        BorrowState thenState = state, elseState = state;
                        thenState.scopes.emplace_back(); analyzeBorrowStatements(node.thenBranch, thenState, repeating); thenState.scopes.pop_back();
                        if (node.hasElse) { elseState.scopes.emplace_back(); analyzeBorrowStatements(node.elseBranch, elseState, repeating); elseState.scopes.pop_back(); }
                        state = mergeStates(thenState, elseState);
                    }
                    else if constexpr (std::is_same_v<T, WhileStmt>) {
                        analyzeBorrowExpr(node.condition, state);
                        BorrowState body = state; body.scopes.emplace_back(); analyzeBorrowStatements(node.body, body, true); body.scopes.pop_back();
                        for (std::size_t i = 0; i < std::min(state.scopes.size(), body.scopes.size()); ++i) {
                            for (const auto& [name, after] : body.scopes[i]) {
                                auto before = state.scopes[i].find(name);
                                if (before != state.scopes[i].end() && before->second.ownership == OwnershipState::Available && after.ownership == OwnershipState::Moved) {
                                    report(node.location, "E0421 value '" + name + "' is moved inside a potentially repeating loop");
                                }
                            }
                        }
                        state = mergeStates(state, body);
                    }
                    else if constexpr (std::is_same_v<T, EachStmt>) {
                        analyzeBorrowExpr(node.range, state);
                        BorrowState body = state; body.scopes.emplace_back();
                        for (const auto& name : node.bindings) body.scopes.back()[name] = BorrowSymbol{ "int", node.bindingsMutable, false, "shared", OwnershipState::Available };
                        analyzeBorrowStatements(node.body, body, true); body.scopes.pop_back();
                        state = mergeStates(state, body);
                    }
                    else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                        state.scopes.emplace_back(); analyzeBorrowStatements(node.body, state, repeating); state.scopes.pop_back();
                    }
                    else if constexpr (std::is_same_v<T, BlockStmt>) {
                        state.scopes.emplace_back(); analyzeBorrowStatements(node.statements, state, repeating); state.scopes.pop_back();
                    }
                    }, stmt->node);
            }
        }

        void report(SourceLocation loc, std::string message) {
            result_->diagnostics.push_back({ loc, std::move(message) });
        }

        SemanticAnalysisResult* result_ = nullptr;
        std::unordered_map<std::string, const Routine*> routines_;
        std::unordered_set<std::string> foreign_;
        std::unordered_map<std::string, EffectFacts> effectFacts_;
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
            }
            else if constexpr (std::is_same_v<T, StringExpr>) {
                return '"' + node.value + '"';
            }
            else if constexpr (std::is_same_v<T, BoolExpr>) {
                return node.value ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, NullExpr>) {
                return "null";
            }
            else if constexpr (std::is_same_v<T, VariableExpr>) {
                return node.name;
            }
            else if constexpr (std::is_same_v<T, UnaryExpr>) {
                return "(" + node.op + " " + exprToString(node.operand) + ")";
            }
            else if constexpr (std::is_same_v<T, BinaryExpr>) {
                return "(" + exprToString(node.left) + " " + node.op + " " + exprToString(node.right) + ")";
            }
            else if constexpr (std::is_same_v<T, PipelineExpr>) {
                std::ostringstream oss;
                oss << exprToString(node.input);
                for (const auto& stage : node.stages) {
                    oss << " -> " << exprToString(stage);
                }
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, RangeExpr>) {
                return exprToString(node.start) + (node.inclusive ? "..=" : "..") + exprToString(node.end);
            }
            else if constexpr (std::is_same_v<T, ChooseExpr>) {
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
            }
            else if constexpr (std::is_same_v<T, CallExpr>) {
                std::ostringstream oss;
                oss << exprToString(node.callee);
                if (!node.genericArguments.empty()) { oss << '<'; for (std::size_t i = 0; i < node.genericArguments.size(); ++i) { if (i)oss << ", "; oss << node.genericArguments[i]; } oss << '>'; }
                oss << '(';
                for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                    if (i != 0) oss << ", ";
                    oss << exprToString(node.arguments[i]);
                }
                oss << ')';
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, TryExpr>) {
                return "try " + exprToString(node.operand);
            }
            else if constexpr (std::is_same_v<T, ClosureExpr>) {
                std::ostringstream oss; if (node.moveCapture)oss << "move "; oss << "routine(";
                for (std::size_t i = 0; i < node.parameters.size(); ++i) { if (i)oss << ", "; if (node.parameters[i].mode != "shared")oss << node.parameters[i].mode << ' '; oss << node.parameters[i].name; if (!node.parameters[i].typeName.empty())oss << ": " << node.parameters[i].typeName; }oss << ')';
                if (!node.returnType.empty())oss << " gives " << node.returnType;
                if (node.hasExpressionBody)oss << " => " << exprToString(node.expressionBody); else oss << " <block closure>"; return oss.str();
            }
            else if constexpr (std::is_same_v<T, GroupExpr>) {
                return "(" + exprToString(node.expression) + ")";
            }
            else if constexpr (std::is_same_v<T, PipelineExpr>) {
                return exprToString(node.input) + " -> <pipeline>";
            }
            else if constexpr (std::is_same_v<T, RangeExpr>) {
                return exprToString(node.start) + (node.inclusive ? "..=" : "..") + exprToString(node.end);
            }
            else if constexpr (std::is_same_v<T, ChooseExpr>) {
                return "choose <expr>";
            }
            else {
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
            }
            else if constexpr (std::is_same_v<T, VarStmt>) {
                oss << indentText(depth) << "var " << node.name;
                if (!node.typeName.empty()) oss << ": " << node.typeName;
                if (node.initializer) oss << " = " << exprToString(node.initializer);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, AssignStmt>) {
                oss << indentText(depth) << node.name << " = " << exprToString(node.value);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                oss << indentText(depth) << node.name << "[" << exprToString(node.index) << "] = " << exprToString(node.value);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                oss << indentText(depth) << node.name << "." << node.field << " = " << exprToString(node.value);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, ExprStmt>) {
                oss << indentText(depth) << exprToString(node.expression);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, ReturnStmt>) {
                oss << indentText(depth) << "return";
                if (node.hasValue) oss << " " << exprToString(node.value);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, GiveStmt>) {
                oss << indentText(depth) << "give " << exprToString(node.value);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, IfStmt>) {
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
            }
            else if constexpr (std::is_same_v<T, WhileStmt>) {
                oss << indentText(depth) << "while " << exprToString(node.condition) << "\n";
                for (const auto& child : node.body) {
                    oss << stmtToString(child, depth + 1) << "\n";
                }
                std::string result = oss.str();
                if (!result.empty() && result.back() == '\n') result.pop_back();
                return result;
            }
            else if constexpr (std::is_same_v<T, BreakStmt>) {
                return indentText(depth) + "break" + (node.label.empty() ? "" : " @" + node.label);
            }
            else if constexpr (std::is_same_v<T, ContinueStmt>) {
                return indentText(depth) + "continue" + (node.label.empty() ? "" : " @" + node.label);
            }
            else if constexpr (std::is_same_v<T, EachStmt>) {
                oss << indentText(depth) << "for ";
                if (node.bindingsMutable) oss << "var ";
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
            }
            else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                oss << indentText(depth) << "unsafe\n";
                for (const auto& child : node.body) {
                    oss << stmtToString(child, depth + 1) << "\n";
                }
                std::string result = oss.str();
                if (!result.empty() && result.back() == '\n') result.pop_back();
                return result;
            }
            else if constexpr (std::is_same_v<T, DeferStmt>) {
                oss << indentText(depth) << "defer " << exprToString(node.expression);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, FailStmt>) {
                oss << indentText(depth) << "fail " << exprToString(node.expression);
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, BlockStmt>) {
                for (const auto& child : node.statements) {
                    oss << stmtToString(child, depth) << "\n";
                }
                std::string result = oss.str();
                if (!result.empty() && result.back() == '\n') result.pop_back();
                return result;
            }
            else {
                return indentText(depth) + "<stmt>";
            }
            }, stmt->node);
    }

    std::string routineToString(const Routine& routine) {
        std::ostringstream oss;
        oss << "routine " << routine.name << '(';
        for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
            if (i != 0) oss << ", ";
            if (routine.parameters[i].mode != "shared") oss << routine.parameters[i].mode << ' ';
            oss << routine.parameters[i].name << ": " << routine.parameters[i].typeName;
        }
        oss << ")";
        if (!routine.returnType.empty() && routine.returnType != "void") {
            oss << " gives " << routine.returnType;
        }
        if (!routine.effects.empty()) {
            oss << " with ";
            for (std::size_t i = 0; i < routine.effects.size(); ++i) { if (i) oss << ", "; oss << routine.effects[i]; }
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

    // --- SAGE specialization + closure conversion before PCIR -------------------
    class SemanticSpecializer {
    public:
        Program specialize(const Program& source, const SemanticAnalysisResult& analysis) {
            analysis_ = &analysis; callSpecializations_.clear();
            for (const auto& f : analysis.skm.specializations) callSpecializations_[callKey(f.location, f.genericRoutine)] = f.specializationName;
            std::vector<const Routine*> all; for (const auto& r : source.routines)all.push_back(&r); collectDeclaredRoutines(source.declarations, all);
            std::unordered_map<std::string, const Routine*> genericByName; for (auto* r : all)if (!r->genericParameters.empty())genericByName[r->name] = r;
            Program out; out.declarations = source.declarations;
            for (auto* r : all) if (r->genericParameters.empty() && !r->isForeign) out.routines.push_back(rewriteRoutine(*r, {}, r->name));
            for (const auto& f : analysis.skm.specializations) { auto it = genericByName.find(f.genericRoutine); if (it == genericByName.end())continue; out.routines.push_back(rewriteRoutine(*it->second, f.substitutions, f.specializationName)); }
            synthesizeClosures(out, analysis);
            analysis_ = nullptr; return out;
        }
    private:
        static std::string callKey(SourceLocation l, const std::string& n) { return std::to_string(l.line) + ":" + std::to_string(l.column) + ":" + n; }
        void collectDeclaredRoutines(const std::vector<Declaration>& ds, std::vector<const Routine*>& out) { for (const auto& d : ds)std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>; if constexpr (std::is_same_v<T, Routine>)out.push_back(&n); else if constexpr (std::is_same_v<T, ModuleDecl>)collectDeclaredRoutines(n.declarations, out); }, d.node); }
        std::string subst(const std::string& t, const std::map<std::string, std::string>& m)const { return substituteGenericType(t, m); }
        Parameter rewriteParameter(Parameter p, const std::map<std::string, std::string>& m) { p.typeName = subst(p.typeName, m); return p; }
        ExprPtr rewriteExpr(const ExprPtr& e, const std::map<std::string, std::string>& m) {
            if (!e)return{}; auto span = e->span; return std::visit([&](const auto& n)->ExprPtr {using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, NumberExpr> || std::is_same_v<T, StringExpr> || std::is_same_v<T, BoolExpr> || std::is_same_v<T, NullExpr> || std::is_same_v<T, VariableExpr>)return std::make_shared<const Expr>(*e);
            else if constexpr (std::is_same_v<T, UnaryExpr>) { auto x = n; x.operand = rewriteExpr(n.operand, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, BinaryExpr>) { auto x = n; x.left = rewriteExpr(n.left, m); x.right = rewriteExpr(n.right, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, PipelineExpr>) { auto x = n; x.input = rewriteExpr(n.input, m); for (auto& z : x.stages)z = rewriteExpr(z, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, RangeExpr>) { auto x = n; x.start = rewriteExpr(n.start, m); x.end = rewriteExpr(n.end, m); x.step = rewriteExpr(n.step, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, ChooseExpr>) { auto x = n; x.subject = rewriteExpr(n.subject, m); for (auto& c : x.cases) { c.condition = rewriteExpr(c.condition, m); c.value = rewriteExpr(c.value, m); }x.otherwiseValue = rewriteExpr(n.otherwiseValue, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, CallExpr>) { auto x = n; x.callee = rewriteExpr(n.callee, m); for (auto& a : x.arguments)a = rewriteExpr(a, m); for (auto& a : x.genericArguments)a = subst(a, m); if (auto* v = std::get_if<VariableExpr>(&x.callee->node)) { auto it = callSpecializations_.find(callKey(n.location, v->name)); if (it != callSpecializations_.end()) { auto nv = *v; nv.name = it->second; x.callee = AstFactory::makeExpr(std::move(nv), x.callee->span); x.genericArguments.clear(); } }return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, TryExpr>) { auto x = n; x.operand = rewriteExpr(n.operand, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, ClosureExpr>) { auto x = n; for (auto& p : x.parameters)p = rewriteParameter(p, m); x.returnType = subst(x.returnType, m); x.expressionBody = rewriteExpr(n.expressionBody, m); x.blockBody = rewriteStatements(n.blockBody, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, GroupExpr>) { auto x = n; x.expression = rewriteExpr(n.expression, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, MemberExpr>) { auto x = n; x.object = rewriteExpr(n.object, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, ConstructExpr>) { auto x = n; x.typeName = subst(n.typeName, m); for (auto& f : x.fields)f.value = rewriteExpr(f.value, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, ArrayExpr>) { auto x = n; for (auto& v : x.elements)v = rewriteExpr(v, m); return AstFactory::makeExpr(std::move(x), span); }
            else if constexpr (std::is_same_v<T, IndexExpr>) { auto x = n; x.object = rewriteExpr(x.object, m); x.index = rewriteExpr(x.index, m); return AstFactory::makeExpr(std::move(x), span); }
            else return std::make_shared<const Expr>(*e);
                }, e->node);
        }
        StmtPtr rewriteStmt(const StmtPtr& s, const std::map<std::string, std::string>& m) {
            if (!s)return{}; auto span = s->span; return std::visit([&](const auto& n)->StmtPtr {using T = std::decay_t<decltype(n)>; auto x = n;
            if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>) { x.typeName = subst(x.typeName, m); x.initializer = rewriteExpr(x.initializer, m); }
            else if constexpr (std::is_same_v<T, AssignStmt>)x.value = rewriteExpr(x.value, m);
            else if constexpr (std::is_same_v<T, ArraySetStmt>) { x.index = rewriteExpr(x.index, m); x.value = rewriteExpr(x.value, m); }
            else if constexpr (std::is_same_v<T, RecordSetStmt>)x.value = rewriteExpr(x.value, m);
            else if constexpr (std::is_same_v<T, ExprStmt>)x.expression = rewriteExpr(x.expression, m);
            else if constexpr (std::is_same_v<T, ReturnStmt>) { if (x.hasValue)x.value = rewriteExpr(x.value, m); }
            else if constexpr (std::is_same_v<T, GiveStmt>)x.value = rewriteExpr(x.value, m);
            else if constexpr (std::is_same_v<T, IfStmt>) { x.condition = rewriteExpr(x.condition, m); x.thenBranch = rewriteStatements(x.thenBranch, m); x.elseBranch = rewriteStatements(x.elseBranch, m); }
            else if constexpr (std::is_same_v<T, EachStmt>) { x.range = rewriteExpr(x.range, m); x.body = rewriteStatements(x.body, m); }
            else if constexpr (std::is_same_v<T, UnsafeStmt>)x.body = rewriteStatements(x.body, m);
            else if constexpr (std::is_same_v<T, DeferStmt>)x.expression = rewriteExpr(x.expression, m);
            else if constexpr (std::is_same_v<T, FailStmt>)x.expression = rewriteExpr(x.expression, m);
            else if constexpr (std::is_same_v<T, WhileStmt>) { x.condition = rewriteExpr(x.condition, m); x.body = rewriteStatements(x.body, m); }
            else if constexpr (std::is_same_v<T, BlockStmt>)x.statements = rewriteStatements(x.statements, m);
            return AstFactory::makeStmt(std::move(x), span);
                }, s->node);
        }
        std::vector<StmtPtr> rewriteStatements(const std::vector<StmtPtr>& ss, const std::map<std::string, std::string>& m) { std::vector<StmtPtr>out; out.reserve(ss.size()); for (auto& s : ss)out.push_back(rewriteStmt(s, m)); return out; }
        Routine rewriteRoutine(const Routine& r, const std::map<std::string, std::string>& m, const std::string& name) { Routine x = r; x.name = name; x.genericParameters.clear(); x.constraints.clear(); for (auto& p : x.parameters)p = rewriteParameter(p, m); x.returnType = subst(x.returnType, m); x.body = rewriteStatements(x.body, m); return x; }
        void collectClosuresExpr(const ExprPtr& e, std::vector<const ClosureExpr*>& out) { if (!e)return; std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>; if constexpr (std::is_same_v<T, ClosureExpr>) { out.push_back(&n); collectClosuresExpr(n.expressionBody, out); collectClosuresStmt(n.blockBody, out); } else if constexpr (std::is_same_v<T, UnaryExpr>)collectClosuresExpr(n.operand, out); else if constexpr (std::is_same_v<T, BinaryExpr>) { collectClosuresExpr(n.left, out); collectClosuresExpr(n.right, out); } else if constexpr (std::is_same_v<T, CallExpr>) { collectClosuresExpr(n.callee, out); for (auto& a : n.arguments)collectClosuresExpr(a, out); } else if constexpr (std::is_same_v<T, PipelineExpr>) { collectClosuresExpr(n.input, out); for (auto& a : n.stages)collectClosuresExpr(a, out); } else if constexpr (std::is_same_v<T, RangeExpr>) { collectClosuresExpr(n.start, out); collectClosuresExpr(n.end, out); collectClosuresExpr(n.step, out); } else if constexpr (std::is_same_v<T, ChooseExpr>) { collectClosuresExpr(n.subject, out); for (auto& c : n.cases) { collectClosuresExpr(c.condition, out); collectClosuresExpr(c.value, out); }collectClosuresExpr(n.otherwiseValue, out); } else if constexpr (std::is_same_v<T, TryExpr>)collectClosuresExpr(n.operand, out); else if constexpr (std::is_same_v<T, GroupExpr>)collectClosuresExpr(n.expression, out); else if constexpr (std::is_same_v<T, MemberExpr>)collectClosuresExpr(n.object, out); else if constexpr (std::is_same_v<T, ConstructExpr>)for (auto& f : n.fields)collectClosuresExpr(f.value, out); else if constexpr (std::is_same_v<T, ArrayExpr>)for (auto& e : n.elements)collectClosuresExpr(e, out); else if constexpr (std::is_same_v<T, IndexExpr>) { collectClosuresExpr(n.object, out); collectClosuresExpr(n.index, out); }}, e->node); }
        void collectClosuresStmt(const std::vector<StmtPtr>& ss, std::vector<const ClosureExpr*>& out) {
            for (auto& s : ss)if (s)std::visit([&](const auto& n) {using T = std::decay_t<decltype(n)>; if constexpr (std::is_same_v<T, LetStmt> || std::is_same_v<T, VarStmt>)collectClosuresExpr(n.initializer, out); else if constexpr (std::is_same_v<T, AssignStmt>)collectClosuresExpr(n.value, out);
            else if constexpr (std::is_same_v<T, ArraySetStmt>) { collectClosuresExpr(n.index, out); collectClosuresExpr(n.value, out); }
            else if constexpr (std::is_same_v<T, RecordSetStmt>)collectClosuresExpr(n.value, out); else if constexpr (std::is_same_v<T, ExprStmt>)collectClosuresExpr(n.expression, out); else if constexpr (std::is_same_v<T, ReturnStmt>) { if (n.hasValue)collectClosuresExpr(n.value, out); }
            else if constexpr (std::is_same_v<T, GiveStmt>)collectClosuresExpr(n.value, out); else if constexpr (std::is_same_v<T, IfStmt>) { collectClosuresExpr(n.condition, out); collectClosuresStmt(n.thenBranch, out); collectClosuresStmt(n.elseBranch, out); }
            else if constexpr (std::is_same_v<T, EachStmt>) { collectClosuresExpr(n.range, out); collectClosuresStmt(n.body, out); }
            else if constexpr (std::is_same_v<T, UnsafeStmt>)collectClosuresStmt(n.body, out); else if constexpr (std::is_same_v<T, DeferStmt>)collectClosuresExpr(n.expression, out); else if constexpr (std::is_same_v<T, FailStmt>)collectClosuresExpr(n.expression, out); else if constexpr (std::is_same_v<T, WhileStmt>) { collectClosuresExpr(n.condition, out); collectClosuresStmt(n.body, out); }
            else if constexpr (std::is_same_v<T, BlockStmt>)collectClosuresStmt(n.statements, out); }, s->node);
        }
        void synthesizeClosures(Program& out, const SemanticAnalysisResult& analysis) { std::vector<const ClosureExpr*> closures; for (const auto& r : out.routines)collectClosuresStmt(r.body, closures); std::unordered_set<std::size_t>seen; for (auto* c : closures) { if (!seen.insert(c->closureId).second)continue; Routine fn; fn.name = "closure$" + std::to_string(c->closureId) + "$call"; fn.returnType = c->returnType.empty() ? "void" : c->returnType; fn.location = c->location; fn.span = c->span; for (const auto& cap : analysis.skm.closureCaptures)if (cap.closureId == c->closureId) { Parameter p; p.name = cap.name; p.typeName = cap.typeName; p.location = cap.location; p.mode = cap.kind == BorrowKind::Owned ? "own" : cap.kind == BorrowKind::Exclusive ? "var" : "shared"; fn.parameters.push_back(p); }for (auto p : c->parameters)fn.parameters.push_back(p); if (c->hasExpressionBody) { auto st = GiveStmt{ c->expressionBody,c->span,c->location }; fn.body.push_back(AstFactory::makeStmt(std::move(st), c->span)); } else fn.body = c->blockBody; out.routines.push_back(std::move(fn)); } }
        const SemanticAnalysisResult* analysis_ = nullptr; std::unordered_map<std::string, std::string>callSpecializations_;
    };

    // --- IR lowerer -------------------------------------------------------------
    class IRLowerer {
    public:
        IRProgram lower(const Program& program) {
            IRProgram lowered;
            routineReturnTypes_.clear();
            routineParameters_.clear();
            nominalTypeNames_.clear();
            globalConstants_.clear();
            for (const auto& d : program.declarations) {
                if (const auto* c = std::get_if<ConstDecl>(&d.node)) {
                    globalConstants_.emplace(c->name, std::make_pair(c->typeName, c->value));
                }
            }
            nominalFieldTypes_.clear();
            collectNominalNames(program.declarations);
            for (const auto& routine : program.routines) {
                routineReturnTypes_[routine.name] = routine.returnType.empty() ? "void" : routine.returnType;
                routineParameters_[routine.name] = routine.parameters;
            }
            for (const auto& routine : program.routines) {
                lowered.functions.push_back(lowerRoutine(routine));
            }
            lowerDeclarationList(program.declarations, lowered.declarationSummaries);
            return lowered;
        }

    private:
        void collectNominalNames(const std::vector<Declaration>& declarations) {
            for (const auto& d : declarations) std::visit([&](const auto& n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, RecordDecl> || std::is_same_v<T, UnionDecl> || std::is_same_v<T, VariantDecl> || std::is_same_v<T, EnumDecl>) {
                    nominalTypeNames_.insert(n.name);
                    if constexpr (std::is_same_v<T, RecordDecl> || std::is_same_v<T, UnionDecl>) {
                        for (const auto& f : n.fields) nominalFieldTypes_[n.name][f.name] = f.typeName;
                    }
                }
                else if constexpr (std::is_same_v<T, ModuleDecl>) collectNominalNames(n.declarations);
                }, d.node);
        }

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
                    output.push_back({ qualify(node.name), "record", node.isPublic, node.isCompileTime, node.layout });
                }
                else if constexpr (std::is_same_v<T, EnumDecl>) {
                    output.push_back({ qualify(node.name), "enum", node.isPublic, false, node.underlyingType.empty() ? "unknown" : node.underlyingType });
                }
                else if constexpr (std::is_same_v<T, UnionDecl>) {
                    output.push_back({ qualify(node.name), "union", node.isPublic, false, node.layout });
                }
                else if constexpr (std::is_same_v<T, VariantDecl>) {
                    output.push_back({ qualify(node.name), "variant", node.isPublic, false, "cases=" + std::to_string(node.cases.size()) });
                }
                else if constexpr (std::is_same_v<T, ModuleDecl>) {
                    output.push_back({ qualify(node.name), "module", node.isPublic, false, node.name });
                    lowerDeclarationList(node.declarations, output, qualify(node.name));
                }
                else if constexpr (std::is_same_v<T, UseDecl>) {
                    std::string alias = node.alias.empty() ? (node.path.empty() ? std::string{} : node.path.back()) : node.alias;
                    output.push_back({ qualify(alias), "use", false, false, joinPath(node.path) });
                }
                else if constexpr (std::is_same_v<T, ConstDecl>) {
                    output.push_back({ qualify(node.name), "const", node.isPublic, node.isCompileTime, node.typeName });
                }
                else if constexpr (std::is_same_v<T, ForeignDecl>) {
                    output.push_back({ qualify(node.routine.name), "foreign", node.routine.isPublic, node.routine.isCompileTime, node.abi });
                }
                else if constexpr (std::is_same_v<T, Routine>) {
                    output.push_back({ qualify(node.name), "routine", node.isPublic, node.isCompileTime, node.returnType });
                }
                }, declaration.node);
        }

        IRFunction lowerRoutine(const Routine& routine) {
            IRFunction function;
            function.name = routine.name;
            function.returnType = routine.returnType.empty() ? "void" : routine.returnType;
            currentReturnType_ = function.returnType;
            function.valueType = irValueTypeFromName(function.returnType);
            function.signature = buildSignature(routine);

            blocks_.clear();
            locals_.clear();
            localValueTypes_.clear();
            closureBindings_.clear();
            lastValue_ = VReg::None;
            lastValueType_ = makeIRUnknownType();
            nextVreg_ = 1;
            nextBlock_ = 1;

            for (const auto& parameter : routine.parameters) {
                function.parameterNames.push_back(parameter.name);
                function.parameterTypes.push_back(parameter.typeName);
                function.parameterModes.push_back(parameter.mode);
                function.parameterValueTypes.push_back(irValueTypeFromName(parameter.typeName));
                function.parameterSignature.push_back(parameter.name + ": " + parameter.typeName);
                function.parameterVregs.push_back(allocateVReg());
            }
            currentBlock_ = createBlock("entry");
            currentTerminated_ = false;
            loopBreakTargets_.clear();
            loopContinueTargets_.clear();
            loopScopeDepths_.clear();
            deferScopes_.clear();
            textViewOwners_.clear();
            ownedScopes_.clear();
            rootOwnedParams_.clear();
            for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
                if (routine.parameters[i].mode == "own" && routine.parameters[i].typeName == "string") {
                    rootOwnedParams_.push_back(function.parameterVregs[i]);
                }
            }

            for (std::size_t i = 0; i < routine.parameters.size(); ++i) {
                locals_[routine.parameters[i].name] = function.parameterVregs[i];
                localValueTypes_[routine.parameters[i].name] = function.parameterValueTypes[i];
            }

            lowerStatementList(routine.body);
            if (!currentTerminated_ && function.returnType == "void") {
                IRInstruction ret; ret.op = IROpcode::Return;
                ret.valueType = makeIRVoidType(); emit(std::move(ret));
                currentTerminated_ = true;
            }

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
                if (routine.parameters[i].mode != "shared") oss << routine.parameters[i].mode << ' ';
                oss << routine.parameters[i].name << ": " << routine.parameters[i].typeName;
            }
            oss << ") -> " << (routine.returnType.empty() ? "void" : routine.returnType);
            if (!routine.effects.empty()) {
                oss << " with ";
                for (std::size_t i = 0; i < routine.effects.size(); ++i) { if (i) oss << ','; oss << routine.effects[i]; }
            }
            return oss.str();
        }

        IRValueType binaryValueType(const std::string& op) const {
            if (op == "and" || op == "or" || op == "&&" || op == "||" || op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
                return makeIRBoolType();
            }
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" ||
                op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
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
            if (name == "float" || name == "double") return makeIRFloatType(name);
            if (isNumericTypeName(name)) return makeIRIntType();
            if (name == "text") return makeIRTextType();
            if (name == "string") return makeIRStringType();
            if (name == "null") return makeIRNullType();
            if (name.rfind("result<", 0) == 0) return { IRValueKind::Result, name };
            if (name.rfind("option<", 0) == 0) return { IRValueKind::Option, name };
            if (name.rfind("closure$", 0) == 0) return { IRValueKind::Closure, name };
            if (!name.empty() && (name.front() == '(' || nominalTypeNames_.count(name) || typeIsArray(name))) return { IRValueKind::Aggregate, name };
            if (name.empty()) return makeIRUnknownType();
            return { IRValueKind::Function, name };
        }

        // Deferred expressions run in reverse registration order at every scope exit.
        // The IR is emitted at each concrete exit, never at the defer declaration.
        void verifyBorrowDoesNotEscape(VReg value, const IRValueType& type) const {
            if (type.kind != IRValueKind::Text) return;
            const auto it = textViewOwners_.find(static_cast<std::uint32_t>(value));
            if (it == textViewOwners_.end()) return;
            for (const auto& scope : ownedScopes_)
                if (std::find(scope.begin(), scope.end(), it->second) != scope.end())
                    throw std::runtime_error("E0425 cannot return text borrowing local or owned string: backing storage is destroyed at scope exit");
        }
        void emitOwnedDrop(VReg source) {
            IRInstruction drop;
            drop.op = IROpcode::StringDispose;
            drop.args = { source };
            drop.typeName = "string";
            drop.valueType = makeIRVoidType();
            emit(std::move(drop));
        }
        void emitOwnedMove(VReg source, VReg destination, bool trackLifetime = false) {
            if (source == destination) return;
            if (trackLifetime && !ownedScopes_.empty()) ownedScopes_.back().push_back(destination);
            IRInstruction move;
            move.op = IROpcode::StringMove;
            move.args = { source };
            move.dest = destination;
            move.valueType = makeIRStringType();
            move.typeName = "string";
            emit(std::move(move));
        }
        void emitPendingDefers(std::size_t outerDepth, VReg escaping = VReg::None) {
            for (std::size_t scope = deferScopes_.size(); scope > outerDepth; --scope) {
                const auto& expressions = deferScopes_[scope - 1];
                for (auto it = expressions.rbegin(); it != expressions.rend(); ++it)
                    (void)lowerExpr(*it);
                // Each owning allocation has exactly one live owner after a move.
                // Explicit disposal zeroes the pointer; a second scoped drop is harmless.
                // Drop in reverse construction order, after user-registered defers.
                const auto owned = ownedScopes_[scope - 1];
                for (auto it = owned.rbegin(); it != owned.rend(); ++it) if (*it != escaping) emitOwnedDrop(*it);
            }
        }

        void lowerStatementList(const std::vector<StmtPtr>& statements) {
            deferScopes_.emplace_back();
            ownedScopes_.emplace_back();
            if (ownedScopes_.size() == 1) ownedScopes_.back().insert(ownedScopes_.back().end(), rootOwnedParams_.begin(), rootOwnedParams_.end());
            for (const auto& statement : statements) {
                if (currentTerminated_) break;
                lowerStatement(statement);
            }
            if (!currentTerminated_) emitPendingDefers(deferScopes_.size() - 1);
            deferScopes_.pop_back();
            ownedScopes_.pop_back();
        }

        void lowerStatement(const StmtPtr& statement) {
            if (!statement) {
                return;
            }
            std::visit([&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, LetStmt>) {
                    lowerLet(node);
                }
                else if constexpr (std::is_same_v<T, VarStmt>) {
                    lowerVar(node);
                }
                else if constexpr (std::is_same_v<T, AssignStmt>) {
                    lowerAssign(node);
                }
                else if constexpr (std::is_same_v<T, ArraySetStmt>) {
                    lowerArraySet(node);
                }
                else if constexpr (std::is_same_v<T, RecordSetStmt>) {
                    lowerRecordSet(node);
                }
                else if constexpr (std::is_same_v<T, ExprStmt>) {
                    (void)lowerExpr(node.expression);
                }
                else if constexpr (std::is_same_v<T, ReturnStmt>) {
                    lowerReturn(node);
                }
                else if constexpr (std::is_same_v<T, GiveStmt>) {
                    lowerGive(node);
                }
                else if constexpr (std::is_same_v<T, IfStmt>) {
                    lowerIf(node);
                }
                else if constexpr (std::is_same_v<T, EachStmt>) {
                    lowerEach(node);
                }
                else if constexpr (std::is_same_v<T, UnsafeStmt>) {
                    lowerUnsafe(node);
                }
                else if constexpr (std::is_same_v<T, DeferStmt>) {
                    lowerDefer(node);
                }
                else if constexpr (std::is_same_v<T, FailStmt>) {
                    lowerFail(node);
                }
                else if constexpr (std::is_same_v<T, WhileStmt>) {
                    lowerWhile(node);
                }
                else if constexpr (std::is_same_v<T, BreakStmt>) {
                    lowerBreak(node);
                }
                else if constexpr (std::is_same_v<T, ContinueStmt>) {
                    lowerContinue(node);
                }
                else if constexpr (std::is_same_v<T, BlockStmt>) {
                    lowerStatementList(node.statements);
                }
                }, statement->node);
        }

        VReg lowerExprWithExpected(const ExprPtr& expr, const IRValueType& expected) {
            if (expr && (expected.kind == IRValueKind::Float32 || expected.kind == IRValueKind::Float64)) {
                if (const auto* number = std::get_if<NumberExpr>(&expr->node)) {
                    // Contextual fitting of a numeric literal, not an implicit numeric conversion.
                    IRInstruction i; i.op = IROpcode::ConstFloat;
                    i.dest = allocateVReg(); i.valueType = expected; i.literalText = number->value;
                    lastValueType_ = expected; const auto dest = i.dest; emit(std::move(i));
                    return dest;
                }
            }
            if (expr && (expected.kind == IRValueKind::Float32 || expected.kind == IRValueKind::Float64)) {
                if (const auto* unary = std::get_if<UnaryExpr>(&expr->node)) {
                    if (unary->op == "-") {
                        auto v = lowerExprWithExpected(unary->operand, expected);
                        IRInstruction i; i.op = IROpcode::Neg; i.dest = allocateVReg();
                        i.args = { v }; i.valueType = expected; const auto dest = i.dest;
                        emit(std::move(i)); lastValueType_ = expected; return dest;
                    }
                }
            }
            return lowerExpr(expr);
        }

        void lowerLet(const LetStmt& node) {
            if (node.initializer) {
                if (const auto* closure = std::get_if<ClosureExpr>(&node.initializer->node)) {
                    ClosureBinding binding;
                    binding.functionName = "closure$" + std::to_string(closure->closureId) + "$call";
                    auto pit = routineParameters_.find(binding.functionName);
                    if (pit != routineParameters_.end() && pit->second.size() >= closure->parameters.size()) {
                        const auto captureCount = pit->second.size() - closure->parameters.size();
                        for (std::size_t i = 0; i < captureCount; ++i) binding.captures.push_back(pit->second[i].name);
                    }
                    closureBindings_[node.name] = binding;
                    IRInstruction make; make.op = IROpcode::MakeClosure; make.dest = allocateVReg(); make.callee = binding.functionName; make.valueType = { IRValueKind::Closure, "closure$" + std::to_string(closure->closureId) }; make.typeName = make.valueType.name;
                    for (const auto& capture : binding.captures) { auto it = locals_.find(capture); if (it != locals_.end() && it->second != VReg::None) { make.args.push_back(it->second); make.argumentTypes.push_back(irValueTypeName(localValueTypes_[capture])); make.argumentValueTypes.push_back(localValueTypes_[capture]); } }
                    emit(make);
                    locals_[node.name] = make.dest;
                    localValueTypes_[node.name] = make.valueType;
                    lastValue_ = make.dest;
                    lastValueType_ = make.valueType;
                    return;
                }
            }
            auto value = node.initializer ? lowerExprWithExpected(node.initializer, irValueTypeFromName(node.typeName)) : makeNullValue();
            if (lastValueType_.kind == IRValueKind::String && node.initializer &&
                std::holds_alternative<VariableExpr>(node.initializer->node)) {
                const VReg dest = allocateVReg();
                emitOwnedMove(value, dest, true);
                value = dest;
            }
            if (lastValueType_.kind == IRValueKind::Aggregate && node.initializer &&
                std::holds_alternative<VariableExpr>(node.initializer->node)) {
                IRInstruction cp; cp.op = IROpcode::Copy; cp.dest = allocateVReg(); cp.args = { value }; cp.valueType = lastValueType_;
                value = cp.dest; emit(std::move(cp));
            }
            locals_[node.name] = value;
            localValueTypes_[node.name] = lastValueType_;
            lastValue_ = value;
            lastValueType_ = localValueTypes_[node.name];
        }

        void lowerVar(const VarStmt& node) {
            auto value = node.initializer ? lowerExprWithExpected(node.initializer, irValueTypeFromName(node.typeName)) : makeNullValue();
            if (lastValueType_.kind == IRValueKind::String && node.initializer &&
                std::holds_alternative<VariableExpr>(node.initializer->node)) {
                const VReg dest = allocateVReg();
                emitOwnedMove(value, dest, true);
                value = dest;
            }
            if (lastValueType_.kind == IRValueKind::Aggregate && node.initializer &&
                std::holds_alternative<VariableExpr>(node.initializer->node)) {
                IRInstruction cp; cp.op = IROpcode::Copy; cp.dest = allocateVReg(); cp.args = { value }; cp.valueType = lastValueType_;
                value = cp.dest; emit(std::move(cp));
            }
            locals_[node.name] = value;
            localValueTypes_[node.name] = lastValueType_;
            lastValue_ = value;
            lastValueType_ = localValueTypes_[node.name];
        }

        void lowerAssign(const AssignStmt& node) {
            auto value = lowerExpr(node.value);
            auto it = locals_.find(node.name);
            if (it == locals_.end()) {
                locals_[node.name] = value;
                localValueTypes_[node.name] = lastValueType_;
                lastValue_ = value;
                return;
            }
            if (lastValueType_.kind == IRValueKind::String &&
                localValueTypes_[node.name].kind == IRValueKind::String) {
                if (value != it->second) {
                    emitOwnedDrop(it->second); // release replaced owner first
                    emitOwnedMove(value, it->second); // existing binding retains its scope
                }
                lastValue_ = it->second;
                return;
            }
            IRInstruction copy;
            copy.op = IROpcode::Copy;
            copy.dest = it->second;
            copy.args.push_back(value);
            copy.valueType = lastValueType_;
            if (lastValueType_.kind == IRValueKind::Text) {
                const auto owner = textViewOwners_.find(static_cast<std::uint32_t>(value));
                if (owner != textViewOwners_.end()) textViewOwners_[static_cast<std::uint32_t>(it->second)] = owner->second;
                else textViewOwners_.erase(static_cast<std::uint32_t>(it->second));
            }
            emit(std::move(copy));
            localValueTypes_[node.name] = lastValueType_;
            lastValue_ = it->second;
        }

        void lowerRecordSet(const RecordSetStmt& node) {
            auto it = locals_.find(node.name);
            if (it == locals_.end())throw std::runtime_error("PCIR unknown record for field mutation: " + node.name);
            const auto type = localValueTypes_.at(node.name).name;
            auto fields = nominalFieldTypes_.find(type);
            if (fields == nominalFieldTypes_.end() || !fields->second.contains(node.field))
                throw std::runtime_error("PCIR unknown record field in " + type + ": " + node.field);
            const auto fieldType = irValueTypeFromName(fields->second.at(node.field));
            const auto value = lowerExprWithExpected(node.value, fieldType);
            IRInstruction inst; inst.op = IROpcode::StoreField; inst.args = { it->second,value };
            inst.literalText = node.field; inst.typeName = type; inst.valueType = fieldType;
            emit(std::move(inst));
        }

        void lowerArraySet(const ArraySetStmt& node) {
            auto it = locals_.find(node.name);
            if (it == locals_.end())throw std::runtime_error("PCIR unknown indexed array target " + node.name);
            const auto arrType = localValueTypes_.at(node.name).name;
            std::string element; std::size_t count = 0;
            if (!typeIsArray(arrType, &element, &count))throw std::runtime_error("PCIR indexed store requires fixed array");
            const auto index = lowerExpr(node.index);
            const auto value = lowerExprWithExpected(node.value, irValueTypeFromName(element));
            IRInstruction inst; inst.op = IROpcode::ArrayStore;
            inst.args = { it->second,index,value }; inst.typeName = arrType; inst.valueType = irValueTypeFromName(element);
            emit(std::move(inst));
        }

        void lowerReturn(const ReturnStmt& node) {
            IRInstruction inst;
            inst.op = IROpcode::Return;
            if (node.hasValue) {
                auto value = lowerExpr(node.value);
                inst.valueType = lastValueType_;
                verifyBorrowDoesNotEscape(value, inst.valueType);
                // Snapshot before running cleanup, so later mutations cannot change the result.
                if (lastValueType_.kind == IRValueKind::String) {
                    const VReg dest = allocateVReg();
                    emitOwnedMove(value, dest);
                    value = dest;
                }
                else {
                    IRInstruction save; save.op = IROpcode::Copy;
                    save.dest = allocateVReg(); save.args = { value }; save.valueType = lastValueType_;
                    value = save.dest; emit(std::move(save));
                }
                inst.args.push_back(value);
            }
            else {
                inst.valueType = makeIRVoidType();
            }
            emitPendingDefers(0, inst.valueType.kind == IRValueKind::String ? inst.args.front() : VReg::None);
            emit(std::move(inst));
            currentTerminated_ = true;
        }

        void lowerGive(const GiveStmt& node) {
            auto value = lowerExprWithExpected(node.value, irValueTypeFromName(currentReturnType_));
            verifyBorrowDoesNotEscape(value, lastValueType_);
            if (currentReturnType_.rfind("result<", 0) == 0 && lastValueType_.kind != IRValueKind::Result) value = emitTagged(IROpcode::ResultOk, value, { IRValueKind::Result, currentReturnType_ });
            else if (currentReturnType_.rfind("option<", 0) == 0 && lastValueType_.kind != IRValueKind::Option) value = emitTagged(IROpcode::OptionSome, value, { IRValueKind::Option, currentReturnType_ });
            // Preserve the returned object/value before deferred cleanup runs.
            if (currentReturnType_ == "string") {
                const VReg dest = allocateVReg();
                emitOwnedMove(value, dest);
                value = dest;
            }
            else {
                IRInstruction save; save.op = IROpcode::Copy;
                save.dest = allocateVReg(); save.args = { value }; save.valueType = irValueTypeFromName(currentReturnType_);
                value = save.dest; emit(std::move(save));
            }
            IRInstruction inst; inst.op = IROpcode::Return; inst.args.push_back(value); inst.valueType = irValueTypeFromName(currentReturnType_);
            emitPendingDefers(0, inst.valueType.kind == IRValueKind::String ? inst.args.front() : VReg::None);
            emit(std::move(inst)); currentTerminated_ = true;
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

            if (node.hasElse) {
                currentBlock_ = elseBlock;
                currentTerminated_ = false;
                lastValue_ = VReg::None;
                lowerStatementList(node.elseBranch);
                if (!currentTerminated_) {
                    emitBranch(mergeBlock);
                }
            }

            currentBlock_ = mergeBlock;
            currentTerminated_ = false;
        }

        void lowerWhile(const WhileStmt& node) {
            BlockID condBlock = createBlock("while.cond");
            BlockID bodyBlock = createBlock("while.body");
            BlockID exitBlock = createBlock("while.exit");
            emitBranch(condBlock);

            currentBlock_ = condBlock;
            currentTerminated_ = false;
            auto conditionValue = lowerExpr(node.condition);
            IRInstruction branch;
            branch.op = IROpcode::BranchIf;
            branch.args.push_back(conditionValue);
            branch.targetBlocks.push_back(bodyBlock);
            branch.targetBlocks.push_back(exitBlock);
            emit(std::move(branch));
            currentTerminated_ = true;

            loopContinueTargets_.push_back(condBlock);
            loopBreakTargets_.push_back(exitBlock);
            loopScopeDepths_.push_back(deferScopes_.size());
            currentBlock_ = bodyBlock;
            currentTerminated_ = false;
            lastValue_ = VReg::None;
            lowerStatementList(node.body);
            if (!currentTerminated_) emitBranch(condBlock);
            loopContinueTargets_.pop_back();
            loopBreakTargets_.pop_back();
            loopScopeDepths_.pop_back();

            currentBlock_ = exitBlock;
            currentTerminated_ = false;
        }

        void lowerEach(const EachStmt& node) {
            if (node.bindings.size() != 1) {
                throw std::runtime_error("bootstrap PCIR lowering currently requires one binding in for-range loops");
            }
            const auto* range = node.range ? std::get_if<RangeExpr>(&node.range->node) : nullptr;
            if (!range || !range->start || !range->end) {
                throw std::runtime_error("bootstrap PCIR lowering currently supports for over numeric ranges only");
            }

            const std::string& binding = node.bindings.front();
            VReg startValue = lowerExpr(range->start);
            VReg endValue = lowerExpr(range->end);
            VReg induction = allocateVReg();
            IRInstruction init;
            init.op = IROpcode::Copy;
            init.dest = induction;
            init.args.push_back(startValue);
            init.valueType = makeIRIntType();
            emit(std::move(init));
            locals_[binding] = induction;
            localValueTypes_[binding] = makeIRIntType();

            BlockID condBlock = createBlock("for.cond");
            BlockID bodyBlock = createBlock("for.body");
            BlockID stepBlock = createBlock("for.step");
            BlockID exitBlock = createBlock("for.exit");
            emitBranch(condBlock);

            currentBlock_ = condBlock;
            currentTerminated_ = false;
            VReg condition = allocateVReg();
            IRInstruction compare;
            compare.op = range->inclusive ? IROpcode::Le : IROpcode::Lt;
            compare.dest = condition;
            compare.args = { induction, endValue };
            compare.valueType = makeIRBoolType();
            emit(std::move(compare));
            IRInstruction branch;
            branch.op = IROpcode::BranchIf;
            branch.args.push_back(condition);
            branch.targetBlocks = { bodyBlock, exitBlock };
            branch.valueType = makeIRVoidType();
            emit(std::move(branch));
            currentTerminated_ = true;

            loopContinueTargets_.push_back(stepBlock);
            loopBreakTargets_.push_back(exitBlock);
            loopScopeDepths_.push_back(deferScopes_.size());
            currentBlock_ = bodyBlock;
            currentTerminated_ = false;
            lowerStatementList(node.body);
            if (!currentTerminated_) emitBranch(stepBlock);

            currentBlock_ = stepBlock;
            currentTerminated_ = false;
            VReg one = emitConst(IROpcode::ConstInt, "1");
            VReg next = allocateVReg();
            IRInstruction add;
            add.op = IROpcode::Add;
            add.dest = next;
            add.args = { induction, one };
            add.valueType = makeIRIntType();
            emit(std::move(add));
            IRInstruction update;
            update.op = IROpcode::Copy;
            update.dest = induction;
            update.args.push_back(next);
            update.valueType = makeIRIntType();
            emit(std::move(update));
            emitBranch(condBlock);

            loopContinueTargets_.pop_back();
            loopBreakTargets_.pop_back();
            loopScopeDepths_.pop_back();
            currentBlock_ = exitBlock;
            currentTerminated_ = false;
        }

        void lowerBreak(const BreakStmt&) {
            if (loopBreakTargets_.empty()) throw std::runtime_error("internal error: break without loop target");
            emitPendingDefers(loopScopeDepths_.back());
            emitBranch(loopBreakTargets_.back());
        }

        void lowerContinue(const ContinueStmt&) {
            if (loopContinueTargets_.empty()) throw std::runtime_error("internal error: continue without loop target");
            emitPendingDefers(loopScopeDepths_.back());
            emitBranch(loopContinueTargets_.back());
        }

        void lowerUnsafe(const UnsafeStmt& node) {
            lowerStatementList(node.body);
        }

        void lowerDefer(const DeferStmt& node) {
            if (deferScopes_.empty()) throw std::runtime_error("defer outside any active lexical scope");
            deferScopes_.back().push_back(node.expression);
        }

        void lowerFail(const FailStmt& node) {
            auto value = node.expression ? lowerExpr(node.expression) : makeNullValue();
            value = emitTagged(IROpcode::ResultErr, value, { IRValueKind::Result, currentReturnType_ });
            IRInstruction save; save.op = IROpcode::Copy;
            save.dest = allocateVReg(); save.args = { value }; save.valueType = { IRValueKind::Result, currentReturnType_ };
            value = save.dest; emit(std::move(save));
            IRInstruction inst; inst.op = IROpcode::Return; inst.args.push_back(value); inst.valueType = { IRValueKind::Result, currentReturnType_ };
            emitPendingDefers(0);
            emit(std::move(inst)); currentTerminated_ = true;
        }

        VReg lowerChoose(const ChooseExpr& node) {
            // Conditional evaluation is a genuine CFG diamond/chain. Values for the
            // untaken arms are never evaluated, unlike the old unconditional phi stub.
            const IRValueType resultType = inferChooseValueType(node);
            const VReg result = allocateVReg();
            const BlockID merge = createBlock("choose.merge");
            VReg subject = VReg::None;
            if (node.subject) subject = lowerExpr(node.subject);
            for (std::size_t i = 0; i < node.cases.size(); ++i) {
                const auto& arm = node.cases[i];
                VReg condition = lowerExpr(arm.condition);
                if (subject != VReg::None) {
                    IRInstruction eq; eq.op = IROpcode::Eq; eq.dest = allocateVReg();
                    eq.args = { subject, condition }; eq.valueType = makeIRBoolType();
                    condition = eq.dest; emit(std::move(eq));
                }
                const BlockID take = createBlock("choose.arm" + std::to_string(i));
                const BlockID skip = createBlock("choose.next" + std::to_string(i));
                IRInstruction branch; branch.op = IROpcode::BranchIf;
                branch.args = { condition }; branch.targetBlocks = { take, skip };
                emit(std::move(branch)); currentTerminated_ = true;
                currentBlock_ = take; currentTerminated_ = false;
                const VReg branchValue = lowerExpr(arm.value);
                IRInstruction set; set.op = IROpcode::Copy; set.dest = result;
                set.args = { branchValue }; set.valueType = resultType; emit(std::move(set));
                emitBranch(merge);
                currentBlock_ = skip; currentTerminated_ = false;
            }
            if (!node.hasOtherwise) {
                throw std::runtime_error("non-exhaustive choose expression requires otherwise");
            }
            const VReg fallback = lowerExpr(node.otherwiseValue);
            IRInstruction set; set.op = IROpcode::Copy; set.dest = result;
            set.args = { fallback }; set.valueType = resultType; emit(std::move(set));
            emitBranch(merge);
            currentBlock_ = merge; currentTerminated_ = false;
            lastValueType_ = resultType;
            return result;
        }

        IRValueType inferChooseValueType(const ChooseExpr& node) const {
            if (!node.cases.empty()) return lowerExprType(node.cases.front().value);
            if (node.hasOtherwise) return lowerExprType(node.otherwiseValue);
            return makeIRUnknownType();
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
                    if (node.value.find('.') != std::string::npos) {
                        lastValueType_ = makeIRFloatType("double");
                        return emitConst(IROpcode::ConstFloat, node.value);
                    }
                    lastValueType_ = makeIRIntType();
                    return emitConst(IROpcode::ConstInt, node.value);
                }
                else if constexpr (std::is_same_v<T, StringExpr>) {
                    lastValueType_ = makeIRTextType();
                    return emitConst(IROpcode::ConstText, node.value);
                }
                else if constexpr (std::is_same_v<T, BoolExpr>) {
                    lastValueType_ = makeIRBoolType();
                    return emitBool(node.value);
                }
                else if constexpr (std::is_same_v<T, NullExpr>) {
                    lastValueType_ = makeIRNullType();
                    return makeNullValue();
                }
                else if constexpr (std::is_same_v<T, VariableExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerVariable(node.name);
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerUnary(node);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerBinary(node);
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerCall(node);
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    VReg current = lowerExpr(node.input);
                    for (const auto& stage : node.stages) {
                        if (stage) {
                            current = lowerExpr(stage);
                        }
                    }
                    return current;
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    lastValueType_ = makeIRUnknownType();
                    return makeNullValue();
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerChoose(node);
                }
                else if constexpr (std::is_same_v<T, TryExpr>) {
                    auto sourceType = lowerExprType(node.operand);
                    auto source = lowerExpr(node.operand);
                    std::string okType, errType;
                    IRValueType resultType = makeIRUnknownType();
                    if (typeIsResult(sourceType.name, &okType, &errType)) resultType = irValueTypeFromName(okType);
                    // `try` propagates an error as a return.  Deferred scope cleanup must
                    // execute on that edge, but never on the successful edge.
                    const bool hasDefers = std::any_of(deferScopes_.begin(), deferScopes_.end(),
                        [](const auto& scope) { return !scope.empty(); }) ||
                        std::any_of(ownedScopes_.begin(), ownedScopes_.end(),
                            [](const auto& scope) { return !scope.empty(); });
                    if (hasDefers) {
                        IRInstruction flag; flag.op = IROpcode::ResultIsErr;
                        flag.dest = allocateVReg(); flag.args = { source };
                        flag.valueType = makeIRBoolType(); flag.typeName = sourceType.name;
                        const VReg isErr = flag.dest; emit(std::move(flag));
                        const BlockID errorBlock = createBlock("try.error.cleanup");
                        const BlockID successBlock = createBlock("try.success");
                        IRInstruction branch; branch.op = IROpcode::BranchIf;
                        branch.args = { isErr }; branch.targetBlocks = { errorBlock, successBlock };
                        emit(std::move(branch)); currentTerminated_ = true;
                        currentBlock_ = errorBlock; currentTerminated_ = false;
                        emitPendingDefers(0);
                        // This intrinsic copies the Err payload to the enclosing result's
                        // ABI and returns.  The terminal instruction below also makes
                        // the CFG explicitly total for independent IR verification.
                        IRInstruction propagate; propagate.op = IROpcode::ResultTry;
                        propagate.dest = allocateVReg(); propagate.args = { source };
                        propagate.valueType = resultType; propagate.typeName = sourceType.name;
                        propagate.returnType = currentReturnType_; emit(std::move(propagate));
                        IRInstruction terminal; terminal.op = IROpcode::Return;
                        terminal.args = { source }; terminal.valueType = irValueTypeFromName(currentReturnType_);
                        emit(std::move(terminal)); currentTerminated_ = true;
                        currentBlock_ = successBlock; currentTerminated_ = false;
                    }
                    IRInstruction inst; inst.op = IROpcode::ResultTry; inst.dest = allocateVReg(); inst.args.push_back(source); inst.valueType = resultType; inst.typeName = sourceType.name; inst.returnType = currentReturnType_; emit(inst); lastValueType_ = inst.valueType; return inst.dest;
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    if (node.elements.empty())throw std::runtime_error("cannot lower empty array literal");
                    std::string elemType;
                    const auto firstType = lowerExprType(node.elements.front());
                    elemType = firstType.kind == IRValueKind::Int64 ? "int" : firstType.name;
                    const auto arrayType = "array<" + elemType + "," + std::to_string(node.elements.size()) + ">";
                    IRInstruction inst; inst.op = IROpcode::MakeAggregate; inst.dest = allocateVReg();
                    inst.typeName = arrayType; inst.valueType = { IRValueKind::Aggregate,arrayType };
                    for (std::size_t n = 0; n < node.elements.size(); ++n) {
                        inst.args.push_back(lowerExpr(node.elements[n]));
                        inst.argumentTypes.push_back(std::to_string(n));
                        inst.argumentValueTypes.push_back(lastValueType_);
                    }
                    const auto dest = inst.dest; emit(std::move(inst)); lastValueType_ = { IRValueKind::Aggregate,arrayType }; return dest;
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    const auto arrType = lowerExprType(node.object);
                    std::string elem; std::size_t count = 0;
                    if (!typeIsArray(arrType.name, &elem, &count))throw std::runtime_error("PCIR array index requires array<T,N>");
                    auto a = lowerExpr(node.object), idx = lowerExpr(node.index);
                    IRInstruction inst; inst.op = IROpcode::ArrayIndex; inst.dest = allocateVReg();
                    inst.typeName = arrType.name; inst.valueType = irValueTypeFromName(elem);
                    inst.args = { a,idx }; const auto dest = inst.dest; emit(std::move(inst));
                    lastValueType_ = irValueTypeFromName(elem); return dest;
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    IRInstruction inst; inst.op = IROpcode::MakeAggregate; inst.dest = allocateVReg(); inst.typeName = node.typeName; inst.valueType = { IRValueKind::Aggregate, node.typeName };
                    for (const auto& f : node.fields) { auto v = lowerExpr(f.value); inst.args.push_back(v); inst.argumentTypes.push_back(f.name); inst.argumentValueTypes.push_back(lastValueType_); }
                    emit(inst); lastValueType_ = inst.valueType; return inst.dest;
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    auto objectType = lowerExprType(node.object);
                    IRValueType fieldType = makeIRUnknownType();
                    if (auto nt = nominalFieldTypes_.find(objectType.name); nt != nominalFieldTypes_.end()) {
                        if (auto ft = nt->second.find(node.member); ft != nt->second.end()) fieldType = irValueTypeFromName(ft->second);
                    }
                    IRInstruction inst; inst.op = IROpcode::ExtractField; inst.dest = allocateVReg(); inst.args.push_back(lowerExpr(node.object)); inst.literalText = node.member; inst.typeName = objectType.name; inst.valueType = fieldType; emit(inst); lastValueType_ = inst.valueType; return inst.dest;
                }
                else if constexpr (std::is_same_v<T, ClosureExpr>) {
                    IRInstruction inst; inst.op = IROpcode::MakeClosure; inst.dest = allocateVReg(); inst.callee = "closure$" + std::to_string(node.closureId) + "$call"; inst.valueType = { IRValueKind::Closure, "closure$" + std::to_string(node.closureId) }; inst.typeName = inst.valueType.name;
                    if (auto pit = routineParameters_.find(inst.callee); pit != routineParameters_.end() && pit->second.size() >= node.parameters.size()) {
                        const std::size_t captureCount = pit->second.size() - node.parameters.size();
                        for (std::size_t i = 0; i < captureCount; ++i) { const auto& p = pit->second[i]; auto it = locals_.find(p.name); if (it != locals_.end() && it->second != VReg::None) { inst.args.push_back(it->second); inst.argumentTypes.push_back(p.typeName); inst.argumentValueTypes.push_back(localValueTypes_[p.name]); } }
                    }
                    emit(inst); lastValueType_ = inst.valueType; return inst.dest;
                }
                else if constexpr (std::is_same_v<T, GroupExpr>) {
                    lastValueType_ = lowerExprType(expr);
                    return lowerExpr(node.expression);
                }
                else {
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
                }
                else if constexpr (std::is_same_v<T, NumberExpr>) {
                    return node.value.find('.') != std::string::npos ? makeIRFloatType("double") : makeIRIntType();
                }
                else if constexpr (std::is_same_v<T, StringExpr>) {
                    return makeIRTextType();
                }
                else if constexpr (std::is_same_v<T, NullExpr>) {
                    return makeIRNullType();
                }
                else if constexpr (std::is_same_v<T, VariableExpr>) {
                    auto it = localValueTypes_.find(node.name);
                    if (it != localValueTypes_.end())return it->second;
                    if (auto g = globalConstants_.find(node.name); g != globalConstants_.end())return irValueTypeFromName(g->second.first);
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, UnaryExpr>) {
                    return node.op == "!" || node.op == "not" ? makeIRBoolType() : lowerExprType(node.operand);
                }
                else if constexpr (std::is_same_v<T, BinaryExpr>) {
                    auto a = lowerExprType(node.left), b = lowerExprType(node.right);
                    if (a.kind == IRValueKind::Float32 || a.kind == IRValueKind::Float64) {
                        auto result = binaryValueType(node.op);
                        return result.kind == IRValueKind::Bool ? result : a;
                    }
                    if (b.kind == IRValueKind::Float32 || b.kind == IRValueKind::Float64) {
                        auto result = binaryValueType(node.op);
                        return result.kind == IRValueKind::Bool ? result : b;
                    }
                    return binaryValueType(node.op);
                }
                else if constexpr (std::is_same_v<T, CallExpr>) {
                    if (const auto* m = node.callee ? std::get_if<MemberExpr>(&node.callee->node) : nullptr) {
                        auto objectType = lowerExprType(m->object);
                        if (objectType.kind == IRValueKind::Text && m->member == "to_string")return makeIRStringType();
                        if (objectType.kind == IRValueKind::String && m->member == "to_text")return makeIRTextType();
                        if ((objectType.kind == IRValueKind::Text || objectType.kind == IRValueKind::String) && m->member == "count")return makeIRIntType();
                        if (objectType.kind == IRValueKind::Text && m->member == "equals")return makeIRBoolType();
                        if (objectType.kind == IRValueKind::String && m->member == "clear")return makeIRVoidType();
                    }
                    if (const auto* v = node.callee ? std::get_if<VariableExpr>(&node.callee->node) : nullptr) {
                        if (v->name == "ok" || v->name == "err") return currentReturnType_.rfind("result<", 0) == 0 ? irValueTypeFromName(currentReturnType_) : IRValueType{ IRValueKind::Result,"result" };
                        if (v->name == "some" || v->name == "none") return currentReturnType_.rfind("option<", 0) == 0 ? irValueTypeFromName(currentReturnType_) : IRValueType{ IRValueKind::Option,"option" };
                        if (auto it = routineReturnTypes_.find(v->name); it != routineReturnTypes_.end()) return irValueTypeFromName(it->second);
                        auto lt = localValueTypes_.find(v->name);
                        if (lt != localValueTypes_.end() && lt->second.kind == IRValueKind::Closure) { auto it = routineReturnTypes_.find(lt->second.name + "$call"); if (it != routineReturnTypes_.end()) return irValueTypeFromName(it->second); }
                    }
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, PipelineExpr>) {
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, RangeExpr>) {
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, ChooseExpr>) {
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, TryExpr>) {
                    auto inner = lowerExprType(node.operand); std::string okType, errType;
                    return typeIsResult(inner.name, &okType, &errType) ? irValueTypeFromName(okType) : makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, ArrayExpr>) {
                    if (node.elements.empty())return makeIRUnknownType();
                    auto first = lowerExprType(node.elements.front());
                    const std::string elem = first.kind == IRValueKind::Int64 ? "int" : first.name;
                    return { IRValueKind::Aggregate,"array<" + elem + "," + std::to_string(node.elements.size()) + ">" };
                }
                else if constexpr (std::is_same_v<T, IndexExpr>) {
                    const auto arr = lowerExprType(node.object);
                    std::string elem; return typeIsArray(arr.name, &elem) ? irValueTypeFromName(elem) : makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, ConstructExpr>) {
                    return { IRValueKind::Aggregate, node.typeName };
                }
                else if constexpr (std::is_same_v<T, MemberExpr>) {
                    auto objectType = lowerExprType(node.object);
                    if (auto nt = nominalFieldTypes_.find(objectType.name); nt != nominalFieldTypes_.end()) if (auto ft = nt->second.find(node.member); ft != nt->second.end()) return irValueTypeFromName(ft->second);
                    return makeIRUnknownType();
                }
                else if constexpr (std::is_same_v<T, ClosureExpr>) {
                    return { IRValueKind::Closure, "closure$" + std::to_string(node.closureId) };
                }
                else {
                    return makeIRUnknownType();
                }
                }, expr->node);
        }

        VReg emitTagged(IROpcode op, VReg payload, IRValueType type) {
            IRInstruction inst; inst.op = op; inst.dest = allocateVReg(); if (payload != VReg::None) inst.args.push_back(payload); inst.valueType = type; emit(inst); lastValueType_ = type; return inst.dest;
        }

        VReg lowerVariable(const std::string& name) {
            auto it = locals_.find(name);
            if (it != locals_.end()) return it->second;
            if (auto g = globalConstants_.find(name); g != globalConstants_.end())
                return lowerExprWithExpected(g->second.second, irValueTypeFromName(g->second.first));
            VReg value = allocateVReg();
            locals_[name] = value;
            return value;
        }

        VReg lowerUnary(const UnaryExpr& node) {
            IRInstruction inst;
            auto dest = allocateVReg();
            inst.dest = dest;
            inst.valueType = node.op == "!" || node.op == "not" ? makeIRBoolType() : lowerExprType(node.operand);
            inst.args.push_back(lowerExpr(node.operand));
            if (node.op == "-") {
                inst.op = IROpcode::Neg;
            }
            else if (node.op == "~") {
                inst.op = IROpcode::BitNot;
            }
            else if (node.op == "!" || node.op == "not") {
                inst.op = IROpcode::Not;
            }
            else {
                inst.op = IROpcode::Nop;
            }
            emit(std::move(inst));
            lastValueType_ = inst.valueType;
            return dest;
        }

        VReg lowerBinary(const BinaryExpr& node) {
            // Source-level logical operators short-circuit.  Use an explicit CFG
            // and a shared destination slot rather than eagerly evaluating RHS.
            // The same branch semantics are used by PCIR, PMIR and native x64.
            if (node.op == "and" || node.op == "&&" || node.op == "or" || node.op == "||") {
                const bool isAnd = node.op == "and" || node.op == "&&";
                const VReg left = lowerExpr(node.left);
                const VReg destination = allocateVReg();
                const BlockID evaluateRight = createBlock("logic.rhs");
                const BlockID skipRight = createBlock("logic.short");
                const BlockID join = createBlock("logic.join");
                IRInstruction branch; branch.op = IROpcode::BranchIf;
                branch.args = { left };
                branch.targetBlocks = isAnd ? std::vector<BlockID>{evaluateRight, skipRight}
                : std::vector<BlockID>{ skipRight, evaluateRight };
                emit(std::move(branch)); currentTerminated_ = true;
                currentBlock_ = skipRight; currentTerminated_ = false;
                IRInstruction constant; constant.op = IROpcode::ConstBool;
                constant.dest = allocateVReg(); constant.valueType = makeIRBoolType();
                constant.immediate = isAnd ? 0 : 1; constant.hasImmediate = true;
                const VReg shortValue = constant.dest; emit(std::move(constant));
                IRInstruction fromShort; fromShort.op = IROpcode::Copy;
                fromShort.dest = destination; fromShort.args = { shortValue };
                fromShort.valueType = makeIRBoolType(); emit(std::move(fromShort));
                emitBranch(join);
                currentBlock_ = evaluateRight; currentTerminated_ = false;
                const VReg rhs = lowerExpr(node.right);
                IRInstruction fromRight; fromRight.op = IROpcode::Copy;
                fromRight.dest = destination; fromRight.args = { rhs };
                fromRight.valueType = makeIRBoolType(); emit(std::move(fromRight));
                emitBranch(join);
                currentBlock_ = join; currentTerminated_ = false;
                lastValueType_ = makeIRBoolType();
                return destination;
            }
            IRInstruction inst;
            auto dest = allocateVReg();
            inst.dest = dest;
            const auto lt = lowerExprType(node.left);
            const auto rt = lowerExprType(node.right);
            const bool leftFP = lt.kind == IRValueKind::Float32 || lt.kind == IRValueKind::Float64;
            const bool rightFP = rt.kind == IRValueKind::Float32 || rt.kind == IRValueKind::Float64;
            const bool leftLiteral = node.left && std::holds_alternative<NumberExpr>(node.left->node);
            const bool rightLiteral = node.right && std::holds_alternative<NumberExpr>(node.right->node);
            const IRValueType valueType = (leftLiteral && rightFP) ? rt :
                (rightLiteral && leftFP) ? lt :
                leftFP ? lt : (rightFP ? rt : makeIRIntType());
            if (leftFP && rightFP && lt.kind != rt.kind &&
                !(node.left && std::holds_alternative<NumberExpr>(node.left->node)) &&
                !(node.right && std::holds_alternative<NumberExpr>(node.right->node)))
                throw std::runtime_error("IEEE-754 operands have different widths; conversion must be explicit");
            if ((leftFP || rightFP) && (node.op == "%" || node.op == "&" || node.op == "|" ||
                node.op == "^" || node.op == "<<" || node.op == ">>"))
                throw std::runtime_error("floating-point operands do not support integer bitwise/remainder operations");
            inst.valueType = binaryValueType(node.op);
            if (leftFP || rightFP) {
                if (inst.valueType.kind != IRValueKind::Bool) inst.valueType = valueType;
                inst.argumentValueTypes = { valueType, valueType };
            }
            inst.args.push_back(lowerExprWithExpected(node.left, valueType));
            inst.args.push_back(lowerExprWithExpected(node.right, valueType));
            if (node.op == "+") inst.op = IROpcode::Add;
            else if (node.op == "-") inst.op = IROpcode::Sub;
            else if (node.op == "*") inst.op = IROpcode::Mul;
            else if (node.op == "/") inst.op = IROpcode::Div;
            else if (node.op == "%") inst.op = IROpcode::Mod;
            else if (node.op == "&") inst.op = IROpcode::BitAnd;
            else if (node.op == "|") inst.op = IROpcode::BitOr;
            else if (node.op == "^") inst.op = IROpcode::BitXor;
            else if (node.op == "<<") inst.op = IROpcode::Shl;
            else if (node.op == ">>") inst.op = IROpcode::Shr;
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
            if (const auto* member = node.callee ? std::get_if<MemberExpr>(&node.callee->node) : nullptr) {
                auto objectType = lowerExprType(member->object);
                if (objectType.kind == IRValueKind::Text && member->member == "to_string") {
                    if (!node.arguments.empty()) throw std::runtime_error("PCIR text.to_string takes no arguments");
                    auto dest = allocateVReg(); inst.op = IROpcode::TextToString; inst.dest = dest; inst.args.push_back(lowerExpr(member->object)); inst.valueType = makeIRStringType(); inst.typeName = "text"; emit(inst); lastValueType_ = inst.valueType; return dest;
                }
                if (objectType.kind == IRValueKind::String && member->member == "to_text") {
                    if (!node.arguments.empty()) throw std::runtime_error("PCIR string.to_text takes no arguments");
                    auto dest = allocateVReg(); inst.op = IROpcode::StringToText; inst.dest = dest; inst.args.push_back(lowerExpr(member->object)); inst.valueType = makeIRTextType(); inst.typeName = "string"; textViewOwners_[static_cast<std::uint32_t>(dest)] = inst.args[0]; emit(inst); lastValueType_ = inst.valueType; return dest;
                }
                if ((objectType.kind == IRValueKind::Text || objectType.kind == IRValueKind::String) && member->member == "count") {
                    if (!node.arguments.empty())throw std::runtime_error("text/string.count takes no arguments");
                    auto dest = allocateVReg(); inst.op = IROpcode::ByteCount; inst.dest = dest; inst.args.push_back(lowerExpr(member->object)); inst.valueType = makeIRIntType(); emit(inst); lastValueType_ = inst.valueType; return dest;
                }
                if (objectType.kind == IRValueKind::Text && member->member == "equals") {
                    if (node.arguments.size() != 1)throw std::runtime_error("text.equals requires one argument");
                    auto dest = allocateVReg(); inst.op = IROpcode::TextEquals; inst.dest = dest;
                    inst.args.push_back(lowerExpr(member->object)); inst.args.push_back(lowerExpr(node.arguments[0]));
                    inst.valueType = makeIRBoolType(); emit(inst); lastValueType_ = inst.valueType; return dest;
                }
                if (objectType.kind == IRValueKind::String && (member->member == "clear" || member->member == "dispose")) {
                    if (!node.arguments.empty()) throw std::runtime_error("PCIR string.clear takes no arguments");
                    auto dest = allocateVReg(); inst.op = (member->member == "dispose" ? IROpcode::StringDispose : IROpcode::StringClear); inst.dest = dest; inst.args.push_back(lowerExpr(member->object)); inst.valueType = makeIRVoidType(); inst.typeName = "string"; emit(inst); lastValueType_ = inst.valueType; return dest;
                }
                throw std::runtime_error("PCIR lowering for member routine '" + objectType.name + "." + member->member + "' is not implemented in this bootstrap; semantic authority/effects were still checked by SAGE");
            }
            if (const auto* intrinsic = node.callee ? std::get_if<VariableExpr>(&node.callee->node) : nullptr) {
                if (intrinsic->name == "ok" || intrinsic->name == "err" || intrinsic->name == "some" || intrinsic->name == "none") {
                    VReg payload = VReg::None; if (!node.arguments.empty()) payload = lowerExpr(node.arguments.front());
                    IRValueType tagged = (intrinsic->name == "ok" || intrinsic->name == "err")
                        ? (currentReturnType_.rfind("result<", 0) == 0 ? irValueTypeFromName(currentReturnType_) : IRValueType{ IRValueKind::Result,"result" })
                        : (currentReturnType_.rfind("option<", 0) == 0 ? irValueTypeFromName(currentReturnType_) : IRValueType{ IRValueKind::Option,"option" });
                    if (intrinsic->name == "ok") return emitTagged(IROpcode::ResultOk, payload, tagged);
                    if (intrinsic->name == "err") return emitTagged(IROpcode::ResultErr, payload, tagged);
                    if (intrinsic->name == "some") return emitTagged(IROpcode::OptionSome, payload, tagged);
                    return emitTagged(IROpcode::OptionNone, VReg::None, tagged);
                }
            }
            if (const auto* variable = node.callee ? std::get_if<VariableExpr>(&node.callee->node) : nullptr) {
                auto cb = closureBindings_.find(variable->name);
                if (cb != closureBindings_.end()) {
                    auto dest = allocateVReg();
                    inst.dest = dest; inst.op = IROpcode::Call; inst.callee = cb->second.functionName;
                    inst.returnType = routineReturnTypes_.count(inst.callee) ? routineReturnTypes_[inst.callee] : "value";
                    inst.typeName = inst.returnType; inst.valueType = irValueTypeFromName(inst.returnType);
                    for (const auto& capture : cb->second.captures) {
                        auto it = locals_.find(capture);
                        if (it == locals_.end() || it->second == VReg::None) throw std::runtime_error("PCIR closure conversion: missing capture '" + capture + "'");
                        inst.args.push_back(it->second); inst.argumentTypes.push_back(irValueTypeName(localValueTypes_[capture])); inst.argumentValueTypes.push_back(localValueTypes_[capture]);
                    }
                    for (const auto& argument : node.arguments) { auto v = lowerExpr(argument); inst.args.push_back(v); inst.argumentTypes.push_back(argumentTypeName(argument)); inst.argumentValueTypes.push_back(lastValueType_); }
                    inst.callSignature = inst.callee; emit(std::move(inst)); lastValueType_ = irValueTypeFromName(routineReturnTypes_[cb->second.functionName]); return dest;
                }
                auto lt = localValueTypes_.find(variable->name);
                if (lt != localValueTypes_.end() && lt->second.kind == IRValueKind::Closure) {
                    auto dest = allocateVReg(); inst.dest = dest; inst.op = IROpcode::CallClosure; inst.args.push_back(lowerVariable(variable->name)); inst.typeName = lt->second.name;
                    for (const auto& argument : node.arguments) { auto v = lowerExpr(argument); inst.args.push_back(v); inst.argumentTypes.push_back(argumentTypeName(argument)); inst.argumentValueTypes.push_back(lastValueType_); }
                    auto rt = routineReturnTypes_.find(lt->second.name + "$call"); inst.returnType = rt == routineReturnTypes_.end() ? "unknown" : rt->second; inst.valueType = irValueTypeFromName(inst.returnType); emit(inst); lastValueType_ = inst.valueType; return dest;
                }
            }
            auto dest = allocateVReg();
            inst.dest = dest;
            inst.op = IROpcode::Call;
            inst.callee = calleeName(node.callee);
            inst.returnType = callReturnType(node.callee);
            inst.typeName = inst.returnType;
            inst.callSignature = calleeSignature(node.callee);
            inst.valueType = irValueTypeFromName(inst.returnType);
            std::size_t argIndex = 0;
            for (const auto& argument : node.arguments) {
                IRValueType expected = makeIRUnknownType();
                if (auto it = routineParameters_.find(inst.callee); it != routineParameters_.end() && argIndex < it->second.size())
                    expected = irValueTypeFromName(it->second[argIndex].typeName);
                auto argumentValue = lowerExprWithExpected(argument, expected);
                ++argIndex;
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
                auto signature = routineReturnTypes_.find(variable->name);
                if (signature != routineReturnTypes_.end()) {
                    return signature->second;
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
                }
                else if constexpr (std::is_same_v<T, NumberExpr>) {
                    return "number";
                }
                else if constexpr (std::is_same_v<T, StringExpr>) {
                    return "text";
                }
                else if constexpr (std::is_same_v<T, NullExpr>) {
                    return "null";
                }
                else {
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
            inst.valueType = op == IROpcode::ConstText ? makeIRTextType() :
                op == IROpcode::ConstFloat ? makeIRFloatType("double") : makeIRIntType();
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
            if (inst.dest != VReg::None && inst.valueType.kind == IRValueKind::String &&
                (inst.op == IROpcode::TextToString || inst.op == IROpcode::Call) &&
                !ownedScopes_.empty()) ownedScopes_.back().push_back(inst.dest);
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
            blocks_.push_back({ id, blockLabel(id) + "_" + label, makeIRVoidType(),{},{},{} });
            return id;
        }

        VReg allocateVReg() {
            return static_cast<VReg>(nextVreg_++);
        }

        struct ClosureBinding { std::string functionName; std::vector<std::string> captures; };
        std::unordered_map<std::string, std::string> routineReturnTypes_;
        std::unordered_map<std::string, std::vector<Parameter>> routineParameters_;
        std::unordered_map<std::string, std::pair<std::string, ExprPtr>> globalConstants_;
        std::unordered_map<std::string, ClosureBinding> closureBindings_;
        std::unordered_set<std::string> nominalTypeNames_;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> nominalFieldTypes_;
        std::string currentReturnType_;
        std::vector<IRBasicBlock> blocks_;
        std::unordered_map<std::string, VReg> locals_;
        std::unordered_map<std::string, IRValueType> localValueTypes_;
        VReg lastValue_ = VReg::None;
        IRValueType lastValueType_ = makeIRUnknownType();
        std::uint32_t nextVreg_ = 1;
        std::uint32_t nextBlock_ = 1;
        BlockID currentBlock_ = BlockID::None;
        bool currentTerminated_ = false;
        std::vector<BlockID> loopBreakTargets_;
        std::vector<BlockID> loopContinueTargets_;
        std::vector<std::size_t> loopScopeDepths_;
        std::vector<std::vector<ExprPtr>> deferScopes_;
        std::vector<std::vector<VReg>> ownedScopes_;
        std::vector<VReg> rootOwnedParams_;
        std::unordered_map<std::uint32_t, VReg> textViewOwners_;
    };

    IRProgram lowerProgramToIR(const Program& program, const SemanticAnalysisResult* analysis = nullptr) {
        IRLowerer lowerer;
        auto ir = lowerer.lower(program);
        if (analysis) { ir.skm = analysis->skm; ir.nominalTypes = analysis->nominalTypes; }
        return ir;
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

    // --- PCIR verification, optimization and semantic oracle --------------------
    class PCIRVerifier {
    public:
        std::vector<std::string> verify(const IRProgram& program) const {
            std::vector<std::string> errors;
            std::unordered_set<std::string> functions;
            for (const auto& fn : program.functions) {
                if (!functions.insert(fn.name).second) errors.push_back("duplicate PCIR function: " + fn.name);
                verifyFunction(fn, errors);
            }
            return errors;
        }

    private:
        static bool isTerminator(IROpcode op) {
            return op == IROpcode::Branch || op == IROpcode::BranchIf || op == IROpcode::Return;
        }

        void verifyFunction(const IRFunction& fn, std::vector<std::string>& errors) const {
            if (fn.blocks.empty()) { errors.push_back(fn.name + ": has no basic blocks"); return; }
            std::unordered_set<std::uint32_t> blockIds;
            for (const auto& b : fn.blocks) blockIds.insert(static_cast<std::uint32_t>(b.id));
            auto validReg = [&](VReg r) {
                auto v = static_cast<std::uint32_t>(r);
                return r != VReg::None && v <= fn.totalVregs;
                };
            for (const auto& p : fn.parameterVregs) if (!validReg(p)) errors.push_back(fn.name + ": invalid parameter vreg");
            // A valid vreg number does not establish that any instruction produced
            // its contents. Check the complete function def-use universe; branch-
            // specific initialization is handled separately by semantic dataflow.
            std::unordered_set<std::uint32_t> defined;
            std::vector<std::uint32_t> used;
            for (auto r : fn.parameterVregs) defined.insert(static_cast<std::uint32_t>(r));
            for (const auto& block : fn.blocks) {
                for (const auto& inst : block.phiNodes) {
                    if (inst.dest != VReg::None) defined.insert(static_cast<std::uint32_t>(inst.dest));
                    for (auto [pred, v] : inst.phiInputs) { (void)pred; used.push_back(static_cast<std::uint32_t>(v)); }
                }
                for (const auto& inst : block.instructions) {
                    if (inst.dest != VReg::None) defined.insert(static_cast<std::uint32_t>(inst.dest));
                    for (auto v : inst.args) used.push_back(static_cast<std::uint32_t>(v));
                }
            }
            for (auto v : used) if (!defined.contains(v))
                errors.push_back(fn.name + ": operand v" + std::to_string(v) + " has no definition in PCIR");
            for (const auto& block : fn.blocks) {
                if (block.id == BlockID::None) errors.push_back(fn.name + ": block has invalid id 0");
                bool terminated = false;
                auto checkInst = [&](const IRInstruction& inst, bool phi) {
                    if (inst.dest != VReg::None && !validReg(inst.dest)) errors.push_back(fn.name + ": instruction destination outside vreg file");
                    for (auto arg : inst.args) if (!validReg(arg)) errors.push_back(fn.name + ": instruction uses invalid vreg");
                    for (auto [pred, value] : inst.phiInputs) {
                        if (pred != BlockID::None && !blockIds.contains(static_cast<std::uint32_t>(pred))) errors.push_back(fn.name + ": phi references invalid predecessor");
                        if (!validReg(value)) errors.push_back(fn.name + ": phi uses invalid vreg");
                    }
                    for (auto target : inst.targetBlocks) if (!blockIds.contains(static_cast<std::uint32_t>(target))) errors.push_back(fn.name + ": branch references invalid block");
                    if (phi && inst.op != IROpcode::Phi) errors.push_back(fn.name + ": non-phi instruction in phi list");
                    };
                for (const auto& phi : block.phiNodes) checkInst(phi, true);
                for (std::size_t i = 0; i < block.instructions.size(); ++i) {
                    const auto& inst = block.instructions[i];
                    checkInst(inst, false);
                    if (terminated) errors.push_back(fn.name + ": instruction appears after terminator in " + block.labelName);
                    if (isTerminator(inst.op)) terminated = true;
                    if (isTerminator(inst.op) && i + 1 != block.instructions.size()) errors.push_back(fn.name + ": terminator is not final instruction in " + block.labelName);
                    if (inst.op == IROpcode::StringMove &&
                        (inst.args.size() != 1 || inst.dest == VReg::None || inst.valueType.kind != IRValueKind::String ||
                            inst.args.front() == inst.dest))
                        errors.push_back(fn.name + ": consuming string move has invalid source/destination");
                    if (inst.op == IROpcode::StoreField && (inst.args.size() != 2 || inst.dest != VReg::None))
                        errors.push_back(fn.name + ": malformed record field write");
                    if (inst.op == IROpcode::ArrayStore && (inst.args.size() != 3 || inst.dest != VReg::None))
                        errors.push_back(fn.name + ": malformed checked indexed write");
                    if (inst.op == IROpcode::StringDispose && inst.args.size() != 1)
                        errors.push_back(fn.name + ": owned-string drop requires one operand");
                    if (inst.op == IROpcode::Branch && inst.targetBlocks.size() != 1) errors.push_back(fn.name + ": branch requires one target");
                    if (inst.op == IROpcode::BranchIf && (inst.targetBlocks.size() != 2 || inst.args.size() != 1)) errors.push_back(fn.name + ": conditional branch shape invalid");
                }
            }
            std::unordered_map<std::uint32_t, const IRBasicBlock*> byId;
            for (const auto& b : fn.blocks) byId.emplace(static_cast<std::uint32_t>(b.id), &b);
            std::vector<std::uint32_t> work{ static_cast<std::uint32_t>(fn.blocks.front().id) };
            std::unordered_set<std::uint32_t> visited;
            while (!work.empty()) {
                auto id = work.back(); work.pop_back();
                if (!visited.insert(id).second) continue;
                auto found = byId.find(id); if (found == byId.end())continue;
                const auto& b = *found->second;
                if (b.instructions.empty() || !isTerminator(b.instructions.back().op)) {
                    if (fn.returnType != "void")errors.push_back(fn.name + ": reachable " + b.labelName + " falls through without returning " + fn.returnType);
                    continue;
                }
                for (auto target : b.instructions.back().targetBlocks)work.push_back(static_cast<std::uint32_t>(target));
            }
            // Forward MUST-defined dataflow. A register existing somewhere in a
            // function is not proof it is initialized on the path where it is read.
            // The lattice is the set of definitely initialized virtual registers;
            // join is intersection. This handles branches and loop backedges.
            std::unordered_map<std::uint32_t, std::set<std::uint32_t>> preds;
            for (const auto& b : fn.blocks) {
                if (b.instructions.empty())continue;
                const auto& tail = b.instructions.back();
                if (!isTerminator(tail.op))continue;
                for (auto t : tail.targetBlocks)
                    preds[static_cast<std::uint32_t>(t)].insert(static_cast<std::uint32_t>(b.id));
            }
            const auto entry = static_cast<std::uint32_t>(fn.blocks.front().id);
            std::set<std::uint32_t> universe;
            for (auto d : defined)universe.insert(d);
            std::set<std::uint32_t> params;
            for (auto p : fn.parameterVregs)params.insert(static_cast<std::uint32_t>(p));
            std::unordered_map<std::uint32_t, std::set<std::uint32_t>> inDefs, outDefs;
            for (auto id : visited) { inDefs[id] = (id == entry ? params : universe); outDefs[id] = universe; }
            bool changed = true;
            for (std::size_t iteration = 0; changed && iteration <= fn.blocks.size() * 2 + 4; ++iteration) {
                changed = false;
                for (const auto& b : fn.blocks) {
                    const auto id = static_cast<std::uint32_t>(b.id);
                    if (!visited.contains(id))continue;
                    std::set<std::uint32_t> incoming = (id == entry ? params : universe);
                    if (id != entry) {
                        bool first = true;
                        for (auto pred : preds[id])if (visited.contains(pred)) {
                            if (first) { incoming = outDefs[pred]; first = false; }
                            else {
                                std::set<std::uint32_t> intersection;
                                std::set_intersection(incoming.begin(), incoming.end(), outDefs[pred].begin(), outDefs[pred].end(),
                                    std::inserter(intersection, intersection.end())); incoming = std::move(intersection);
                            }
                        }
                        if (first)incoming.clear();
                    }
                    auto outgoing = incoming;
                    for (const auto& phi : b.phiNodes)if (phi.dest != VReg::None)outgoing.insert(static_cast<std::uint32_t>(phi.dest));
                    for (const auto& i : b.instructions)if (i.dest != VReg::None)outgoing.insert(static_cast<std::uint32_t>(i.dest));
                    if (inDefs[id] != incoming || outDefs[id] != outgoing) { inDefs[id] = std::move(incoming); outDefs[id] = std::move(outgoing); changed = true; }
                }
            }
            for (const auto& b : fn.blocks) {
                const auto id = static_cast<std::uint32_t>(b.id);
                if (!visited.contains(id))continue;
                auto live = inDefs[id];
                for (const auto& phi : b.phiNodes) {
                    for (const auto& [pred, value] : phi.phiInputs) {
                        const auto predId = static_cast<std::uint32_t>(pred);
                        if (!preds[id].contains(predId))errors.push_back(fn.name + ": PHI incoming from non-predecessor into " + b.labelName);
                        if (visited.contains(predId) && !outDefs[predId].contains(static_cast<std::uint32_t>(value)))
                            errors.push_back(fn.name + ": PHI reads v" + std::to_string(static_cast<std::uint32_t>(value)) + " without definite definition on incoming edge");
                    }
                    if (phi.dest != VReg::None)live.insert(static_cast<std::uint32_t>(phi.dest));
                }
                for (const auto& i : b.instructions) {
                    for (auto arg : i.args)if (!live.contains(static_cast<std::uint32_t>(arg)))
                        errors.push_back(fn.name + ": v" + std::to_string(static_cast<std::uint32_t>(arg)) + " may be uninitialized in " + b.labelName);
                    if (i.dest != VReg::None)live.insert(static_cast<std::uint32_t>(i.dest));
                }
            }
        }

    };

    class PCIROptimizer {
    public:
        explicit PCIROptimizer(int level = 2) : level_(std::clamp(level, 0, 3)) {}

        void optimize(IRProgram& program) const {
            if (level_ == 0) return;
            for (auto& fn : program.functions) {
                for (auto& block : fn.blocks) foldBlock(block);
                if (level_ >= 2) removeDeadPureInstructions(fn);
                if (level_ >= 3) removeUnreachableBlocks(fn);
            }
        }

    private:
        struct Constant { bool known = false; std::int64_t value = 0; bool boolean = false; };

        static bool pure(IROpcode op) {
            switch (op) {
            case IROpcode::ConstInt: case IROpcode::ConstFloat: case IROpcode::ConstBool: case IROpcode::ConstNull: case IROpcode::Copy:
            case IROpcode::Add: case IROpcode::Sub: case IROpcode::Mul: case IROpcode::BitAnd: case IROpcode::BitOr:
            case IROpcode::BitXor: case IROpcode::Shl: case IROpcode::Shr: case IROpcode::Neg: case IROpcode::BitNot:
            case IROpcode::Not: case IROpcode::Eq: case IROpcode::Ne: case IROpcode::Lt: case IROpcode::Le:
            case IROpcode::Gt: case IROpcode::Ge: case IROpcode::And: case IROpcode::Or: case IROpcode::StringToText:case IROpcode::ByteCount:case IROpcode::TextEquals:
            case IROpcode::MakeAggregate: case IROpcode::ExtractField: case IROpcode::ResultOk: case IROpcode::ResultErr:
            case IROpcode::OptionSome: case IROpcode::OptionNone: case IROpcode::MakeClosure: return true;
            default: return false;
            }
        }

        static std::optional<std::int64_t> eval(IROpcode op, std::int64_t a, std::int64_t b = 0) {
            auto u = [](std::int64_t x) { return static_cast<std::uint64_t>(x); };
            switch (op) {
            case IROpcode::Add: return static_cast<std::int64_t>(u(a) + u(b));
            case IROpcode::Sub: return static_cast<std::int64_t>(u(a) - u(b));
            case IROpcode::Mul: return static_cast<std::int64_t>(u(a) * u(b));
            case IROpcode::Div:
                if (b == 0) return std::nullopt;
                if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return a;
                return a / b;
            case IROpcode::Mod:
                if (b == 0) return std::nullopt;
                if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return 0;
                return a % b;
            case IROpcode::BitAnd: return a & b;
            case IROpcode::BitOr: return a | b;
            case IROpcode::BitXor: return a ^ b;
            case IROpcode::Shl: return static_cast<std::int64_t>(u(a) << (u(b) & 63u));
            case IROpcode::Shr: return a >> (u(b) & 63u);
            case IROpcode::Neg: return static_cast<std::int64_t>(0u - u(a));
            case IROpcode::BitNot: return ~a;
            case IROpcode::Not: return a == 0 ? 1 : 0;
            case IROpcode::Eq: return a == b; case IROpcode::Ne: return a != b;
            case IROpcode::Lt: return a < b; case IROpcode::Le: return a <= b;
            case IROpcode::Gt: return a > b; case IROpcode::Ge: return a >= b;
            case IROpcode::And: return (a != 0) && (b != 0); case IROpcode::Or: return (a != 0) || (b != 0);
            default: return std::nullopt;
            }
        }

        void foldBlock(IRBasicBlock& block) const {
            std::unordered_map<std::uint32_t, Constant> constants;
            auto key = [](VReg v) { return static_cast<std::uint32_t>(v); };
            for (auto& inst : block.instructions) {
                if (inst.op == IROpcode::ConstInt) {
                    try { constants[key(inst.dest)] = { true, parseParacineInteger(inst.literalText), false }; }
                    catch (...) { constants.erase(key(inst.dest)); }
                    continue;
                }
                if (inst.op == IROpcode::ConstBool) { constants[key(inst.dest)] = { true, inst.immediate ? 1 : 0, true }; continue; }
                if (inst.op == IROpcode::ConstNull) { constants[key(inst.dest)] = { true, 0, false }; continue; }
                if (inst.op == IROpcode::Copy && inst.args.size() == 1) {
                    auto it = constants.find(key(inst.args[0]));
                    if (it != constants.end()) constants[key(inst.dest)] = it->second; else constants.erase(key(inst.dest));
                    continue;
                }
                if (inst.op == IROpcode::BranchIf && inst.args.size() == 1 && inst.targetBlocks.size() == 2) {
                    auto it = constants.find(key(inst.args[0]));
                    if (it != constants.end() && it->second.known) {
                        BlockID target = it->second.value ? inst.targetBlocks[0] : inst.targetBlocks[1];
                        inst.op = IROpcode::Branch; inst.args.clear(); inst.targetBlocks = { target };
                    }
                    continue;
                }
                if (pure(inst.op) && inst.dest != VReg::None && (inst.args.size() == 1 || inst.args.size() == 2) &&
                    std::none_of(inst.argumentValueTypes.begin(), inst.argumentValueTypes.end(), [](const IRValueType& t) {return t.kind == IRValueKind::Float32 || t.kind == IRValueKind::Float64; })) {
                    auto a = constants.find(key(inst.args[0]));
                    auto b = inst.args.size() == 2 ? constants.find(key(inst.args[1])) : constants.end();
                    if (a != constants.end() && (inst.args.size() == 1 || b != constants.end())) {
                        auto value = eval(inst.op, a->second.value, inst.args.size() == 2 ? b->second.value : 0);
                        if (value) {
                            bool isBool = inst.valueType.kind == IRValueKind::Bool;
                            inst.op = isBool ? IROpcode::ConstBool : IROpcode::ConstInt;
                            inst.args.clear(); inst.targetBlocks.clear(); inst.phiInputs.clear();
                            inst.hasImmediate = isBool;
                            inst.immediate = *value;
                            inst.literalText = isBool ? std::string{} : std::to_string(*value);
                            constants[key(inst.dest)] = { true, *value, isBool };
                            continue;
                        }
                    }
                    constants.erase(key(inst.dest));
                }
                else if (inst.dest != VReg::None) constants.erase(key(inst.dest));
            }
        }

        void removeDeadPureInstructions(IRFunction& fn) const {
            std::unordered_map<std::uint32_t, std::size_t> uses;
            for (const auto& b : fn.blocks) {
                for (const auto& phi : b.phiNodes) for (auto [pred, v] : phi.phiInputs) { (void)pred; ++uses[static_cast<std::uint32_t>(v)]; }
                for (const auto& i : b.instructions) for (auto v : i.args) ++uses[static_cast<std::uint32_t>(v)];
            }
            for (auto& b : fn.blocks) {
                b.instructions.erase(std::remove_if(b.instructions.begin(), b.instructions.end(), [&](const IRInstruction& i) {
                    return i.dest != VReg::None && pure(i.op) && uses[static_cast<std::uint32_t>(i.dest)] == 0;
                    }), b.instructions.end());
            }
        }

        void removeUnreachableBlocks(IRFunction& fn) const {
            if (fn.blocks.empty()) return;
            std::unordered_map<std::uint32_t, const IRBasicBlock*> byId;
            for (const auto& b : fn.blocks) byId[static_cast<std::uint32_t>(b.id)] = &b;
            std::unordered_set<std::uint32_t> live;
            std::vector<std::uint32_t> work{ static_cast<std::uint32_t>(fn.blocks.front().id) };
            while (!work.empty()) {
                auto id = work.back(); work.pop_back(); if (!live.insert(id).second) continue;
                auto it = byId.find(id); if (it == byId.end()) continue;
                for (const auto& inst : it->second->instructions) for (auto t : inst.targetBlocks) work.push_back(static_cast<std::uint32_t>(t));
            }
            fn.blocks.erase(std::remove_if(fn.blocks.begin(), fn.blocks.end(), [&](const IRBasicBlock& b) { return !live.contains(static_cast<std::uint32_t>(b.id)); }), fn.blocks.end());
        }

        int level_ = 2;
    };

    enum class ABIRepresentationKind {
        Scalar,
        Text,
        String,
        Aggregate,
        Result,
        OptionBoolNiche,
        OptionCharNiche,
        OptionPointerNiche,
        OptionTagged,
        Closure,
        Unknown
    };

    struct ABIFieldLayout {
        std::string name;
        std::string typeName;
        std::size_t offset = 0;
        bool byReference = false;
    };

    struct ABITypeLayout {
        ABIRepresentationKind kind = ABIRepresentationKind::Unknown;
        std::string typeName;
        std::size_t size = 8;
        std::size_t alignment = 8;
        bool rich = false;
        bool signedScalar = true;
        std::size_t tagOffset = 0;
        std::size_t payloadOffset = 0;
        std::string valueType;
        std::string okType;
        std::string errType;
        std::string closureCallee;
        std::vector<ABIFieldLayout> fields;
    };

    class RichABIResolver {
    public:
        RichABIResolver(const SemanticKnowledgeModel& skm, const std::unordered_map<std::string, NominalTypeInfo>& nominals)
            : skm_(skm), nominals_(nominals) {}
        explicit RichABIResolver(const IRProgram& program) : RichABIResolver(program.skm, program.nominalTypes) {}

        ABITypeLayout layout(const IRValueType& type) const {
            if (type.kind == IRValueKind::Void) return scalarLayout("void", 0, 1, false);
            if (type.kind == IRValueKind::Bool) return scalarLayout("bool", 1, 1, false);
            if (type.kind == IRValueKind::Int64) return scalarLayout(type.name.empty() ? "long" : type.name, 8, 8, true);
            if (type.kind == IRValueKind::Null || type.kind == IRValueKind::Function) return scalarLayout(type.name.empty() ? "raw" : type.name, 8, 8, false);
            if (type.kind == IRValueKind::Text) return layout("text");
            if (type.kind == IRValueKind::String) return layout("string");
            return layout(type.name);
        }

        ABITypeLayout layout(const std::string& typeName) const {
            if (auto it = cache_.find(typeName); it != cache_.end()) return it->second;
            ABITypeLayout out = compute(typeName);
            cache_[typeName] = out;
            return out;
        }

        bool isRich(const IRValueType& t) const { return layout(t).rich; }
        bool isRich(const std::string& t) const { return layout(t).rich; }

    private:
        static std::size_t alignUp(std::size_t v, std::size_t a) { return a ? ((v + a - 1) / a) * a : v; }
        static ABITypeLayout scalarLayout(std::string name, std::size_t size, std::size_t align, bool sign) {
            ABITypeLayout x; x.kind = ABIRepresentationKind::Scalar; x.typeName = std::move(name); x.size = size; x.alignment = align; x.rich = false; x.signedScalar = sign; return x;
        }
        const SKMLayoutFact* skmLayout(const std::string& name) const {
            for (const auto& l : skm_.layouts) if (l.typeName == name) return &l;
            return nullptr;
        }
        std::size_t closureId(const std::string& name) const {
            if (name.rfind("closure$", 0) != 0) return 0;
            std::size_t p = 8, id = 0; while (p < name.size() && std::isdigit(static_cast<unsigned char>(name[p]))) { id = id * 10 + static_cast<std::size_t>(name[p] - '0'); ++p; } return id;
        }
        ABITypeLayout compute(const std::string& name) const {
            if (name.empty() || name == "unknown" || name == "number" || name == "value") return scalarLayout(name.empty() ? "unknown" : name, 8, 8, true);
            if (name == "void" || name == "never") return scalarLayout(name, 0, 1, false);
            if (name == "bool") return scalarLayout(name, 1, 1, false);
            if (name == "byte") return scalarLayout(name, 1, 1, true);
            if (name == "ubyte") return scalarLayout(name, 1, 1, false);
            if (name == "short") return scalarLayout(name, 2, 2, true);
            if (name == "ushort") return scalarLayout(name, 2, 2, false);
            if (name == "int") return scalarLayout(name, 4, 4, true);
            if (name == "uint" || name == "char") return scalarLayout(name, 4, 4, false);
            if (name == "long" || name == "ssize") return scalarLayout(name, 8, 8, true);
            if (name == "ulong" || name == "size" || name == "raw" || name.rfind("raw<", 0) == 0) return scalarLayout(name, 8, 8, false);
            if (name == "float") return scalarLayout(name, 4, 4, true);
            if (name == "double") return scalarLayout(name, 8, 8, true);
            if (name == "text") { ABITypeLayout x; x.kind = ABIRepresentationKind::Text; x.typeName = name; x.size = 16; x.alignment = 8; x.rich = true; x.fields = { { "ptr","raw<ubyte>",0,false },{ "count","size",8,false } }; return x; }
            if (name == "string") { ABITypeLayout x; x.kind = ABIRepresentationKind::String; x.typeName = name; x.size = 24; x.alignment = 8; x.rich = true; x.fields = { { "ptr","raw<ubyte>",0,false },{ "count","size",8,false },{ "capacity","size",16,false } }; return x; }
            if (name.rfind("closure$", 0) == 0) {
                ABITypeLayout x; x.kind = ABIRepresentationKind::Closure; x.typeName = name; x.rich = true; x.closureCallee = name + "$call";
                const auto id = closureId(name); std::size_t off = 0, al = 1;
                for (const auto& c : skm_.closureCaptures) if (c.closureId == id) {
                    auto cl = layout(c.typeName); off = alignUp(off, cl.alignment); x.fields.push_back({ c.name,c.typeName,off,false }); off += cl.size; al = std::max(al, cl.alignment);
                }
                x.alignment = std::max<std::size_t>(al, 1); x.size = alignUp(off, x.alignment); if (x.size == 0)x.size = 1; return x;
            }
            try {
                auto t = CanonicalTypeParser(name).parse();
                if (t.head == "result" && t.args.size() == 2) {
                    ABITypeLayout x; x.kind = ABIRepresentationKind::Result; x.typeName = name; x.rich = true; x.okType = canonicalTypeText(t.args[0]); x.errType = canonicalTypeText(t.args[1]);
                    auto a = layout(x.okType), b = layout(x.errType); x.alignment = std::max<std::size_t>({ a.alignment,b.alignment,1 }); x.payloadOffset = alignUp(1, x.alignment); x.size = alignUp(x.payloadOffset + std::max(a.size, b.size), x.alignment); return x;
                }
                if (t.head == "option" && t.args.size() == 1) {
                    const auto value = canonicalTypeText(t.args[0]); auto v = layout(value); ABITypeLayout x; x.typeName = name; x.valueType = value;
                    if (value == "bool") { x.kind = ABIRepresentationKind::OptionBoolNiche; x.size = 1; x.alignment = 1; x.rich = false; x.signedScalar = false; return x; }
                    if (value == "char") { x.kind = ABIRepresentationKind::OptionCharNiche; x.size = 4; x.alignment = 4; x.rich = false; x.signedScalar = false; return x; }
                    if (value == "text" || value.rfind("view<", 0) == 0 || value.rfind("span<", 0) == 0 || value.rfind("buffer<", 0) == 0) { x.kind = ABIRepresentationKind::OptionPointerNiche; x.size = v.size; x.alignment = v.alignment; x.rich = true; return x; }
                    x.kind = ABIRepresentationKind::OptionTagged; x.rich = true; x.alignment = std::max<std::size_t>(v.alignment, 1); x.payloadOffset = alignUp(1, x.alignment); x.size = alignUp(x.payloadOffset + v.size, x.alignment); return x;
                }
                if (t.head == "view" || t.head == "span" || t.head == "ref" || t.head == "mut") { ABITypeLayout x; x.kind = ABIRepresentationKind::Aggregate; x.typeName = name; x.size = 16; x.alignment = 8; x.rich = true; x.fields = { { "ptr","raw",0,false },{ "count","size",8,false } }; return x; }
                if (t.head == "buffer") { ABITypeLayout x; x.kind = ABIRepresentationKind::Aggregate; x.typeName = name; x.size = 24; x.alignment = 8; x.rich = true; x.fields = { { "ptr","raw",0,false },{ "count","size",8,false },{ "capacity","size",16,false } }; return x; }
                if (t.head == "array" && t.args.size() == 2) {
                    std::string element; std::size_t count = 0;
                    if (!typeIsArray(name, &element, &count))throw std::runtime_error("invalid array type: " + name);
                    auto el = layout(element); if (el.size == 0)throw std::runtime_error("array of zero-sized element unsupported");
                    if (count > std::numeric_limits<std::size_t>::max() / el.size)throw std::runtime_error("array byte size overflow");
                    ABITypeLayout x; x.kind = ABIRepresentationKind::Aggregate; x.typeName = name; x.rich = true;
                    x.alignment = el.alignment; x.size = el.size * count;
                    for (std::size_t k = 0; k < count; ++k)x.fields.push_back({ std::to_string(k),element,k * el.size,false });
                    return x;
                }
                if (t.tuple) {
                    ABITypeLayout x; x.kind = ABIRepresentationKind::Aggregate; x.typeName = name; x.rich = true; std::size_t off = 0, al = 1;
                    for (std::size_t i = 0; i < t.args.size(); ++i) { auto tn = canonicalTypeText(t.args[i]); auto fl = layout(tn); off = alignUp(off, fl.alignment); x.fields.push_back({ std::to_string(i),tn,off,false }); off += fl.size; al = std::max(al, fl.alignment); }x.alignment = al; x.size = alignUp(off, al); return x;
                }
            }
            catch (...) {}
            if (auto ni = nominals_.find(name); ni != nominals_.end()) {
                const auto& n = ni->second; ABITypeLayout x; x.kind = ABIRepresentationKind::Aggregate; x.typeName = name; x.rich = true;
                if (const auto* sl = skmLayout(name)) { x.size = sl->size; x.alignment = sl->alignment; for (const auto& f : n.fields) { std::size_t off = 0; for (const auto& sf : sl->fieldOffsets)if (sf.first == f.first) { off = sf.second; break; }x.fields.push_back({ f.first,f.second,off,false }); } }
                else { std::size_t off = 0, al = 1; for (const auto& f : n.fields) { auto fl = layout(f.second); off = alignUp(off, fl.alignment); x.fields.push_back({ f.first,f.second,off,false }); off += fl.size; al = std::max(al, fl.alignment); }x.alignment = al; x.size = alignUp(off, al); }
                if (x.size == 0)x.size = 1; return x;
            }
            return scalarLayout(name, 8, 8, false);
        }

        const SemanticKnowledgeModel& skm_;
        const std::unordered_map<std::string, NominalTypeInfo>& nominals_;
        mutable std::unordered_map<std::string, ABITypeLayout> cache_;
    };

    static const char* abiKindName(ABIRepresentationKind kind) {
        switch (kind) {
        case ABIRepresentationKind::Scalar: return "scalar";
        case ABIRepresentationKind::Text: return "text-slice";
        case ABIRepresentationKind::String: return "owned-string";
        case ABIRepresentationKind::Aggregate: return "aggregate";
        case ABIRepresentationKind::Result: return "tagged-result";
        case ABIRepresentationKind::OptionBoolNiche: return "option-bool-niche";
        case ABIRepresentationKind::OptionCharNiche: return "option-char-niche";
        case ABIRepresentationKind::OptionPointerNiche: return "option-pointer-niche";
        case ABIRepresentationKind::OptionTagged: return "tagged-option";
        case ABIRepresentationKind::Closure: return "closure-environment";
        case ABIRepresentationKind::Unknown: return "unknown";
        }
        return "unknown";
    }

    static std::set<std::string> collectABITypeNames(const IRProgram& program) {
        std::set<std::string> types = { "text", "option<bool>", "option<char>" };
        for (const auto& [name, _] : program.nominalTypes) types.insert(name);
        for (const auto& c : program.skm.closures) types.insert("closure$" + std::to_string(c.closureId));
        for (const auto& fn : program.functions) {
            if (!fn.returnType.empty()) types.insert(fn.returnType);
            for (const auto& t : fn.parameterTypes) if (!t.empty()) types.insert(t);
            for (const auto& t : fn.localValueTypes) if (!t.name.empty()) types.insert(t.name);
            for (const auto& block : fn.blocks) for (const auto& inst : block.instructions) {
                if (!inst.valueType.name.empty()) types.insert(inst.valueType.name);
                if (!inst.typeName.empty()) types.insert(inst.typeName);
                if (!inst.returnType.empty()) types.insert(inst.returnType);
                for (const auto& t : inst.argumentValueTypes) if (!t.name.empty()) types.insert(t.name);
            }
        }
        for (const char* n : { "unknown", "void", "number", "value", "function", "null" }) types.erase(n);
        return types;
    }

    class RichABIVerifier {
    public:
        std::vector<std::string> verify(const IRProgram& program) const {
            RichABIResolver abi(program); std::vector<std::string> errors;
            for (const auto& name : collectABITypeNames(program)) {
                const auto l = abi.layout(name);
                if (l.alignment == 0 || (l.alignment & (l.alignment - 1)) != 0) errors.push_back(name + ": ABI alignment is not a power of two");
                if (l.rich && l.size == 0) errors.push_back(name + ": rich ABI object has zero size");
                if (l.size && l.alignment && (l.size % l.alignment) != 0 && l.rich) errors.push_back(name + ": rich ABI size is not rounded to alignment");
                for (const auto& f : l.fields) {
                    const auto fl = abi.layout(f.typeName);
                    if (fl.alignment && (f.offset % fl.alignment) != 0) errors.push_back(name + "." + f.name + ": field is misaligned");
                    if (f.offset + fl.size > l.size) errors.push_back(name + "." + f.name + ": field exceeds ABI object size");
                }
                if (l.kind == ABIRepresentationKind::Text) {
                    if (l.size != 16 || l.alignment != 8 || l.fields.size() != 2 || l.fields[0].offset != 0 || l.fields[1].offset != 8)
                        errors.push_back("text: canonical borrowed-slice ABI must be {ptr@0,count@8}, size 16 align 8");
                }
                else if (l.kind == ABIRepresentationKind::OptionBoolNiche && (l.size != 1 || l.rich)) {
                    errors.push_back(name + ": option<bool> niche ABI must be one direct byte");
                }
                else if (l.kind == ABIRepresentationKind::OptionCharNiche && (l.size != 4 || l.rich)) {
                    errors.push_back(name + ": option<char> niche ABI must be one direct 32-bit scalar");
                }
                else if (l.kind == ABIRepresentationKind::OptionPointerNiche) {
                    if (!l.rich || l.size < 8) errors.push_back(name + ": pointer-niche option must preserve a rich pointer-leading representation");
                }
                else if (l.kind == ABIRepresentationKind::Result || l.kind == ABIRepresentationKind::OptionTagged) {
                    if (!l.rich || l.payloadOffset == 0 || l.payloadOffset >= l.size) errors.push_back(name + ": tagged rich ABI has invalid payload placement");
                }
                else if (l.kind == ABIRepresentationKind::Closure && l.closureCallee.empty()) {
                    errors.push_back(name + ": closure environment has no statically known call target");
                }
            }
            return errors;
        }
    };

    std::string richABIReport(const IRProgram& program) {
        RichABIResolver abi(program);
        std::ostringstream oss;
        oss << "Rich-value target ABI (Windows x86-64 / Microsoft x64)\n";
        oss << "  scalar values: direct integer register/stack lanes\n";
        oss << "  rich parameters: indirect pointer in ordinary ABI lane\n";
        oss << "  rich returns: hidden caller-owned sret pointer in first ABI lane\n";
        for (const auto& name : collectABITypeNames(program)) {
            const auto l = abi.layout(name);
            oss << "  " << name << ": " << abiKindName(l.kind) << " size=" << l.size << " align=" << l.alignment
                << (l.rich ? " indirect/sret" : " direct") << "\n";
            if (l.kind == ABIRepresentationKind::OptionBoolNiche) oss << "    none=2, false=0, true=1\n";
            if (l.kind == ABIRepresentationKind::OptionCharNiche) oss << "    none=0x00110000\n";
            if (l.kind == ABIRepresentationKind::OptionPointerNiche) oss << "    none=null first pointer; Some preserves underlying rich layout\n";
            if (l.kind == ABIRepresentationKind::Result || l.kind == ABIRepresentationKind::OptionTagged)
                oss << "    tag@" << l.tagOffset << " payload@" << l.payloadOffset << "\n";
            if (l.kind == ABIRepresentationKind::Closure) oss << "    static target=" << l.closureCallee << " (environment stores captures only)\n";
            for (const auto& f : l.fields) oss << "    +" << f.offset << " " << f.name << ": " << f.typeName << "\n";
        }
        return oss.str();
    }

    class PCIRInterpreter {
    public:
        explicit PCIRInterpreter(const IRProgram& program) : program_(program) { for (const auto& fn : program.functions) functions_[fn.name] = &fn; }

        std::int64_t run(const std::string& entry = {}) {
            const IRFunction* fn = selectEntry(entry); if (!fn) throw std::runtime_error("no zero-argument entry routine (expected main, answer, or first zero-argument routine)");
            auto value = execute(*fn, {}); if (value.object) throw std::runtime_error("PCIR oracle entry routine returned a rich value; process entry must reduce to an integer status"); return value.scalar;
        }

    private:
        struct Object;
        struct Value { std::int64_t scalar = 0; std::shared_ptr<Object> object; };
        struct Object { std::string typeName; std::string text; int tag = 0; std::vector<std::string> names; std::vector<Value> fields; std::string closureCallee; };
        static Value scalar(std::int64_t v) { Value x; x.scalar = v; return x; }
        static Value object(std::string type) { Value x; x.object = std::make_shared<Object>(); x.object->typeName = std::move(type); return x; }
        // Aggregate copies have value semantics; the semantic oracle must not alias
        // the same mutable Object merely because Values use shared_ptr internally.
        static Value copyAggregateValue(const Value& src) {
            if (!src.object) return src;
            if (src.object->typeName.rfind("array<", 0) != 0 && src.object->typeName.rfind("closure$", 0) == 0)
                return src; // closure environment borrows retain identity
            Value dst = object(src.object->typeName);
            *dst.object = *src.object;
            for (auto& field : dst.object->fields)field = copyAggregateValue(field);
            return dst;
        }
        static std::int64_t asInt(const Value& v) { if (v.object)throw std::runtime_error("PCIR oracle expected scalar value"); return v.scalar; }

        const IRFunction* selectEntry(const std::string& requested) const {
            if (!requested.empty()) { auto it = functions_.find(requested); return it == functions_.end() ? nullptr : it->second; }
            for (const char* name : { "main","answer","start" }) { auto it = functions_.find(name); if (it != functions_.end() && it->second->parameterVregs.empty())return it->second; }for (const auto& fn : program_.functions)if (fn.parameterVregs.empty())return &fn; return nullptr;
        }
        static std::int64_t binary(IROpcode op, std::int64_t a, std::int64_t b) {
            auto u = [](std::int64_t x) {return static_cast<std::uint64_t>(x); }; switch (op) {
            case IROpcode::Add:return static_cast<std::int64_t>(u(a) + u(b)); case IROpcode::Sub:return static_cast<std::int64_t>(u(a) - u(b)); case IROpcode::Mul:return static_cast<std::int64_t>(u(a) * u(b));
            case IROpcode::Div:if (!b)throw std::runtime_error("Paracine trap: division by zero"); if (a == std::numeric_limits<std::int64_t>::min() && b == -1)return a; return a / b;
            case IROpcode::Mod:if (!b)throw std::runtime_error("Paracine trap: remainder by zero"); if (a == std::numeric_limits<std::int64_t>::min() && b == -1)return 0; return a % b;
            case IROpcode::BitAnd:return a & b; case IROpcode::BitOr:return a | b; case IROpcode::BitXor:return a ^ b; case IROpcode::Shl:return static_cast<std::int64_t>(u(a) << (u(b) & 63u)); case IROpcode::Shr:return a >> (u(b) & 63u);
            case IROpcode::Eq:return a == b; case IROpcode::Ne:return a != b; case IROpcode::Lt:return a < b; case IROpcode::Le:return a <= b; case IROpcode::Gt:return a > b; case IROpcode::Ge:return a >= b; case IROpcode::And:return(a != 0) && (b != 0); case IROpcode::Or:return(a != 0) || (b != 0); default:throw std::runtime_error("PCIR interpreter: unsupported binary opcode");
            }
        }

        static std::int64_t floatBinary(IROpcode op, std::int64_t a, std::int64_t b, bool single) {
            auto compute = [&](auto lhs, auto rhs) -> std::int64_t {
                using T = decltype(lhs);
                switch (op) {
                case IROpcode::Eq:return lhs == rhs;
                case IROpcode::Ne:return lhs != rhs;
                case IROpcode::Lt:return lhs < rhs;
                case IROpcode::Le:return lhs <= rhs;
                case IROpcode::Gt:return lhs > rhs;
                case IROpcode::Ge:return lhs >= rhs;
                default:break;
                }
                T result{};
                switch (op) {
                case IROpcode::Add: result = lhs + rhs; break;
                case IROpcode::Sub: result = lhs - rhs; break;
                case IROpcode::Mul: result = lhs * rhs; break;
                case IROpcode::Div: result = lhs / rhs; break;
                default:throw std::runtime_error("unsupported IEEE-754 oracle instruction");
                }
                if constexpr (sizeof(T) == 4) { std::uint32_t bits; std::memcpy(&bits, &result, 4); return static_cast<std::int64_t>(bits); }
                else { std::uint64_t bits; std::memcpy(&bits, &result, 8); return static_cast<std::int64_t>(bits); }
                };
            if (single) { std::uint32_t x = static_cast<std::uint32_t>(a), y = static_cast<std::uint32_t>(b); float f, g; std::memcpy(&f, &x, 4); std::memcpy(&g, &y, 4); return compute(f, g); }
            std::uint64_t x = static_cast<std::uint64_t>(a), y = static_cast<std::uint64_t>(b); double f, g; std::memcpy(&f, &x, 8); std::memcpy(&g, &y, 8); return compute(f, g);
        }

        Value execute(const IRFunction& fn, const std::vector<Value>& args) {
            if (callDepth_++ > 1024)throw std::runtime_error("Paracine trap: recursion limit exceeded in PCIR oracle"); struct Guard { std::size_t& d; ~Guard() { --d; } }guard{ callDepth_ }; std::vector<Value>regs(fn.totalVregs + 1); for (std::size_t i = 0; i < std::min(args.size(), fn.parameterVregs.size()); ++i)regs[static_cast<std::uint32_t>(fn.parameterVregs[i])] = args[i]; std::unordered_map<std::uint32_t, std::size_t>bi; for (std::size_t i = 0; i < fn.blocks.size(); ++i)bi[static_cast<std::uint32_t>(fn.blocks[i].id)] = i; std::size_t current = 0; BlockID pred = BlockID::None; std::size_t steps = 0;
            while (steps++ < 10000000) {
                const auto& block = fn.blocks.at(current); for (const auto& phi : block.phiNodes) { if (phi.dest == VReg::None || phi.phiInputs.empty())continue; VReg sel = VReg::None; for (auto [p, v] : phi.phiInputs)if (p == pred) { sel = v; break; }if (sel == VReg::None)throw std::runtime_error("PCIR phi missing predecessor input"); regs[static_cast<std::uint32_t>(phi.dest)] = regs[static_cast<std::uint32_t>(sel)]; }bool jumped = false; for (const auto& inst : block.instructions) {
                    auto get = [&](std::size_t n)->Value {return regs.at(static_cast<std::uint32_t>(inst.args.at(n))); }; auto set = [&](Value v) {if (inst.dest != VReg::None)regs.at(static_cast<std::uint32_t>(inst.dest)) = std::move(v); }; switch (inst.op) {
                    case IROpcode::ConstInt:set(scalar(parseParacineInteger(inst.literalText))); break;
                    case IROpcode::ConstFloat: {
                        if (inst.valueType.kind == IRValueKind::Float32) { float x = parseParacineFloating<float>(inst.literalText); std::uint32_t bits; std::memcpy(&bits, &x, 4); set(scalar(bits)); }
                        else { double x = parseParacineFloating<double>(inst.literalText); std::uint64_t bits; std::memcpy(&bits, &x, 8); set(scalar(static_cast<std::int64_t>(bits))); }break;
                    }case IROpcode::ConstBool:set(scalar(inst.immediate ? 1 : 0)); break; case IROpcode::ConstNull:set(scalar(0)); break; case IROpcode::ConstText: { auto v = object("text"); v.object->text = inst.literalText; set(v); break; }case IROpcode::Copy:set(inst.valueType.kind == IRValueKind::Aggregate ? copyAggregateValue(get(0)) : get(0)); break;
                    case IROpcode::Neg: {
                        auto v = static_cast<std::uint64_t>(asInt(get(0)));
                        if (inst.valueType.kind == IRValueKind::Float32) v ^= 0x80000000u;
                        else if (inst.valueType.kind == IRValueKind::Float64) v ^= 0x8000000000000000ull;
                        else v = 0u - v;
                        set(scalar(static_cast<std::int64_t>(v))); break;
                    }case IROpcode::BitNot:set(scalar(~asInt(get(0)))); break; case IROpcode::Not:set(scalar(asInt(get(0)) == 0)); break;
                    case IROpcode::Add:case IROpcode::Sub:case IROpcode::Mul:case IROpcode::Div:case IROpcode::Mod:case IROpcode::BitAnd:case IROpcode::BitOr:case IROpcode::BitXor:case IROpcode::Shl:case IROpcode::Shr:case IROpcode::Eq:case IROpcode::Ne:case IROpcode::Lt:case IROpcode::Le:case IROpcode::Gt:case IROpcode::Ge:case IROpcode::And:case IROpcode::Or: {
                        if (!inst.argumentValueTypes.empty() &&
                            (inst.argumentValueTypes[0].kind == IRValueKind::Float32 || inst.argumentValueTypes[0].kind == IRValueKind::Float64))
                            set(scalar(floatBinary(inst.op, asInt(get(0)), asInt(get(1)), inst.argumentValueTypes[0].kind == IRValueKind::Float32)));
                        else set(scalar(binary(inst.op, asInt(get(0)), asInt(get(1))))); break;
                    }
                    case IROpcode::Branch:pred = block.id; current = bi.at(static_cast<std::uint32_t>(inst.targetBlocks.at(0))); jumped = true; break; case IROpcode::BranchIf:pred = block.id; current = bi.at(static_cast<std::uint32_t>(asInt(get(0)) ? inst.targetBlocks.at(0) : inst.targetBlocks.at(1))); jumped = true; break;
                    case IROpcode::Call: { std::vector<Value>ca; for (auto v : inst.args)ca.push_back(regs.at(static_cast<std::uint32_t>(v))); if (inst.callee == "print" || inst.callee == "println") { if (!ca.empty())std::cout << asInt(ca.front()); if (inst.callee == "println")std::cout << '\n'; set(scalar(0)); } else { auto it = functions_.find(inst.callee); if (it == functions_.end())throw std::runtime_error("PCIR oracle unresolved call: " + inst.callee); set(execute(*it->second, ca)); }break; }
                    case IROpcode::TextToString: { auto src = get(0); if (!src.object || src.object->typeName != "text")throw std::runtime_error("PCIR oracle text.to_string on non-text"); auto v = object("string"); v.object->text = src.object->text; set(v); break; }
                    case IROpcode::StringToText: { auto src = get(0); if (!src.object || src.object->typeName != "string")throw std::runtime_error("PCIR oracle string.to_text on non-string"); auto v = object("text"); v.object->text = src.object->text; set(v); break; }
                    case IROpcode::TextEquals: { auto x = get(0), y = get(1); if (!x.object || !y.object || x.object->typeName != "text" || y.object->typeName != "text")throw std::runtime_error("PCIR text.equals requires two text values"); set(scalar(x.object->text == y.object->text)); break; }
                    case IROpcode::ByteCount: { auto src = get(0); if (!src.object)throw std::runtime_error("PCIR byte_count requires text/string"); set(scalar(static_cast<std::int64_t>(src.object->text.size()))); break; }
                    case IROpcode::StringMove: { auto src = get(0); if (!src.object || src.object->typeName != "string")throw std::runtime_error("PCIR oracle string.move on non-string"); auto v = object("string"); v.object->text = std::move(src.object->text); set(v); break; }
                    case IROpcode::StringDispose: { auto src = get(0); if (!src.object || src.object->typeName != "string")throw std::runtime_error("PCIR oracle string.dispose on non-string"); src.object->text.clear(); set(scalar(0)); break; }
                    case IROpcode::StringClear: { auto src = get(0); if (!src.object || src.object->typeName != "string")throw std::runtime_error("PCIR oracle string.clear on non-string"); src.object->text.clear(); set(scalar(0)); break; }
                    case IROpcode::MakeAggregate: { auto v = object(inst.typeName); v.object->names = inst.argumentTypes; for (std::size_t i = 0; i < inst.args.size(); ++i)v.object->fields.push_back(get(i)); set(v); break; }
                    case IROpcode::ExtractField: { auto o = get(0); if (!o.object)throw std::runtime_error("PCIR oracle member access on scalar"); auto it = std::find(o.object->names.begin(), o.object->names.end(), inst.literalText); if (it == o.object->names.end())throw std::runtime_error("PCIR oracle unknown aggregate field: " + inst.literalText); set(o.object->fields[static_cast<std::size_t>(it - o.object->names.begin())]); break; }
                    case IROpcode::ArrayIndex: { auto a = get(0); auto k = asInt(get(1)); if (!a.object || k < 0 || static_cast<std::size_t>(k) >= a.object->fields.size())throw std::runtime_error("Paracine trap: array index out of bounds"); set(a.object->fields[static_cast<std::size_t>(k)]); break; }
                    case IROpcode::ArrayStore: { auto a = get(0); auto k = asInt(get(1)); if (!a.object || k < 0 || static_cast<std::size_t>(k) >= a.object->fields.size())throw std::runtime_error("Paracine trap: array store out of bounds"); a.object->fields[static_cast<std::size_t>(k)] = copyAggregateValue(get(2)); break; }
                    case IROpcode::StoreField: { auto a = get(0); if (!a.object)throw std::runtime_error("PCIR field write requires a record"); auto it = std::find(a.object->names.begin(), a.object->names.end(), inst.literalText); if (it == a.object->names.end())throw std::runtime_error("PCIR record has no field " + inst.literalText); a.object->fields[static_cast<std::size_t>(it - a.object->names.begin())] = copyAggregateValue(get(1)); break; }
                    case IROpcode::ResultOk:case IROpcode::ResultErr: { auto v = object(inst.valueType.name); v.object->tag = inst.op == IROpcode::ResultErr ? 1 : 0; if (!inst.args.empty())v.object->fields.push_back(get(0)); set(v); break; }
                    case IROpcode::ResultIsErr: { auto v = get(0); if (!v.object)throw std::runtime_error("PCIR oracle result.is_err on non-result"); set(scalar(v.object->tag != 0)); break; }
                    case IROpcode::ResultTry: { auto v = get(0); if (!v.object)throw std::runtime_error("PCIR oracle try on non-result"); if (v.object->tag != 0)return v; if (v.object->fields.empty())throw std::runtime_error("PCIR oracle result missing payload"); set(v.object->fields.front()); break; }
                    case IROpcode::OptionSome:case IROpcode::OptionNone: { auto v = object(inst.valueType.name); v.object->tag = inst.op == IROpcode::OptionSome ? 1 : 0; if (inst.op == IROpcode::OptionSome && !inst.args.empty())v.object->fields.push_back(get(0)); set(v); break; }
                    case IROpcode::MakeClosure: { auto v = object(inst.valueType.name); v.object->closureCallee = inst.callee; for (std::size_t i = 0; i < inst.args.size(); ++i)v.object->fields.push_back(get(i)); set(v); break; }
                    case IROpcode::CallClosure: { auto c = get(0); if (!c.object || c.object->closureCallee.empty())throw std::runtime_error("PCIR oracle call on non-closure"); auto it = functions_.find(c.object->closureCallee); if (it == functions_.end())throw std::runtime_error("PCIR oracle unresolved closure target"); std::vector<Value>ca = c.object->fields; for (std::size_t i = 1; i < inst.args.size(); ++i)ca.push_back(get(i)); set(execute(*it->second, ca)); break; }
                    case IROpcode::Return:return inst.args.empty() ? scalar(0) : get(0); case IROpcode::Phi:case IROpcode::Nop:break;
                    }
                    if (jumped)break;
                }if (!jumped)return scalar(0);
            }throw std::runtime_error("Paracine trap: PCIR execution step limit exceeded");
        }

        const IRProgram& program_; std::unordered_map<std::string, const IRFunction*>functions_; std::size_t callDepth_ = 0;
    };

    // --- PMIR: target-aware Windows x86-64 rich-value machine representation ---
    enum class PMIROpcode {
        ConstInt, ConstFloat, ConstBool, ConstNull, ConstText, Copy, Add, Sub, Mul, Div, Mod, BitAnd, BitOr, BitXor,
        Shl, Shr, Neg, BitNot, Not, Eq, Ne, Lt, Le, Gt, Ge, And, Or, Branch, BranchIf, Call, CallClosure,
        TextToString, StringToText, StringClear, StringDispose, StringMove, ByteCount, TextEquals, MakeAggregate, ExtractField, StoreField, ArrayIndex, ArrayStore, ResultOk, ResultErr, ResultIsErr, ResultTry, OptionSome, OptionNone, MakeClosure, Return, Nop
    };

    struct PMIRInstruction {
        PMIROpcode op = PMIROpcode::Nop;
        std::uint32_t destSlot = 0;
        std::vector<std::uint32_t> srcSlots;
        std::vector<std::uint32_t> targets;
        std::string callee;
        std::string typeName;
        std::string literalText;
        std::string returnType;
        IRValueType valueType = makeIRUnknownType();
        std::vector<std::string> argumentTypes;
        std::vector<IRValueType> argumentValueTypes;
        std::int64_t immediate = 0;
        bool hasImmediate = false;
    };

    struct PMIRSlotInfo {
        IRValueType type = makeIRUnknownType();
        std::uint32_t cellOffset = 0;     // positive distance below RBP for the 8-byte value cell
        std::uint32_t objectOffset = 0;   // positive distance below RBP for rich object base
        std::uint32_t objectSize = 0;
        std::uint32_t objectAlignment = 1;
        bool rich = false;
    };

    struct PMIRBlock { std::uint32_t id = 0; std::string label; std::vector<PMIRInstruction> instructions; };
    struct PMIRFunction {
        std::string name;
        std::string returnType;
        IRValueType returnValueType = makeIRVoidType();
        bool richReturn = false;
        std::vector<std::uint32_t> parameterSlots;
        std::vector<std::string> parameterTypes;
        std::vector<std::string> parameterModes;
        std::vector<IRValueType> parameterValueTypes;
        std::unordered_map<std::uint32_t, PMIRSlotInfo> slots;
        std::vector<PMIRBlock> blocks;
        std::uint32_t frameSize = 0;
        std::uint32_t outgoingArea = 32;
        std::uint32_t totalSlots = 0;
        std::uint32_t sretCellOffset = 0;
    };
    struct PMIRProgram {
        std::vector<PMIRFunction> functions;
        SemanticKnowledgeModel skm;
        std::unordered_map<std::string, NominalTypeInfo> nominalTypes;
    };

    class PMIRLowerer {
    public:
        // Lower every PCIR phi on its incoming edge, not at the destination.
        // The first pass snapshots all sources to fresh virtual registers so even
        // mutually dependent phi assignments retain simultaneous SSA semantics.
        static IRFunction lowerPhiEdges(const IRFunction& source) {
            IRFunction out = source;
            std::unordered_map<std::uint32_t, const IRBasicBlock*> targets;
            std::uint32_t nextBlock = 1;
            for (const auto& block : source.blocks) {
                targets.emplace(static_cast<std::uint32_t>(block.id), &block);
                nextBlock = std::max(nextBlock, static_cast<std::uint32_t>(block.id) + 1);
            }
            std::vector<IRBasicBlock> synthetic;
            for (auto& block : out.blocks) {
                if (block.instructions.empty()) continue;
                auto& terminal = block.instructions.back();
                if (terminal.op != IROpcode::Branch && terminal.op != IROpcode::BranchIf) continue;
                std::map<std::uint32_t, BlockID> redirects;
                for (auto& target : terminal.targetBlocks) {
                    const auto original = target;
                    const auto tid = static_cast<std::uint32_t>(original);
                    const auto found = targets.find(tid);
                    if (found == targets.end() || found->second->phiNodes.empty()) continue;
                    if (auto it = redirects.find(tid); it != redirects.end()) { target = it->second; continue; }
                    IRBasicBlock edge;
                    edge.id = static_cast<BlockID>(nextBlock++);
                    edge.labelName = "bb_phi_edge_" + std::to_string(static_cast<std::uint32_t>(block.id)) + "_" + std::to_string(tid);
                    std::vector<std::pair<VReg, VReg>> snapshots;
                    for (const auto& phi : found->second->phiNodes) {
                        if (phi.op != IROpcode::Phi || phi.dest == VReg::None)
                            throw std::runtime_error("PMIR invalid phi node on edge: " + edge.labelName);
                        VReg incoming = VReg::None;
                        for (const auto& [pred, value] : phi.phiInputs) {
                            if (pred == block.id) {
                                if (incoming != VReg::None) throw std::runtime_error("PMIR duplicate phi predecessor");
                                incoming = value;
                            }
                        }
                        if (incoming == VReg::None) throw std::runtime_error("PMIR missing phi incoming value on " + edge.labelName);
                        IRInstruction capture; capture.op = IROpcode::Copy;
                        capture.dest = static_cast<VReg>(++out.totalVregs);
                        capture.valueType = phi.valueType; capture.args = { incoming };
                        edge.instructions.push_back(capture);
                        snapshots.emplace_back(phi.dest, capture.dest);
                    }
                    std::size_t index = 0;
                    for (const auto& phi : found->second->phiNodes) {
                        IRInstruction assign; assign.op = IROpcode::Copy;
                        assign.dest = snapshots[index].first;
                        assign.args = { snapshots[index].second }; assign.valueType = phi.valueType;
                        edge.instructions.push_back(std::move(assign)); ++index;
                    }
                    IRInstruction jump; jump.op = IROpcode::Branch; jump.targetBlocks = { original };
                    jump.valueType = makeIRVoidType(); edge.instructions.push_back(std::move(jump));
                    redirects.emplace(tid, edge.id); target = edge.id;
                    synthetic.push_back(std::move(edge));
                }
            }
            for (auto& block : out.blocks) block.phiNodes.clear();
            out.blocks.insert(out.blocks.end(), std::make_move_iterator(synthetic.begin()), std::make_move_iterator(synthetic.end()));
            return out;
        }
        PMIRProgram lower(const IRProgram& ir) const {
            PMIRProgram out; out.skm = ir.skm; out.nominalTypes = ir.nominalTypes;
            RichABIResolver abi(ir);
            for (const auto& fn : ir.functions) out.functions.push_back(lowerFunction(fn, abi));
            return out;
        }
    private:
        static std::uint32_t alignUp32(std::uint32_t v, std::uint32_t a) { return a ? ((v + a - 1u) / a) * a : v; }
        static std::uint32_t align16(std::uint32_t v) { return alignUp32(v, 16); }
        static PMIROpcode map(IROpcode op) {
            switch (op) {
            case IROpcode::ConstInt:return PMIROpcode::ConstInt; case IROpcode::ConstFloat:return PMIROpcode::ConstFloat; case IROpcode::ConstText:return PMIROpcode::ConstText; case IROpcode::ConstBool:return PMIROpcode::ConstBool; case IROpcode::ConstNull:return PMIROpcode::ConstNull;
            case IROpcode::Copy:return PMIROpcode::Copy; case IROpcode::Add:return PMIROpcode::Add; case IROpcode::Sub:return PMIROpcode::Sub; case IROpcode::Mul:return PMIROpcode::Mul;
            case IROpcode::Div:return PMIROpcode::Div; case IROpcode::Mod:return PMIROpcode::Mod; case IROpcode::BitAnd:return PMIROpcode::BitAnd; case IROpcode::BitOr:return PMIROpcode::BitOr;
            case IROpcode::BitXor:return PMIROpcode::BitXor; case IROpcode::Shl:return PMIROpcode::Shl; case IROpcode::Shr:return PMIROpcode::Shr; case IROpcode::Neg:return PMIROpcode::Neg;
            case IROpcode::BitNot:return PMIROpcode::BitNot; case IROpcode::Not:return PMIROpcode::Not; case IROpcode::Eq:return PMIROpcode::Eq; case IROpcode::Ne:return PMIROpcode::Ne;
            case IROpcode::Lt:return PMIROpcode::Lt; case IROpcode::Le:return PMIROpcode::Le; case IROpcode::Gt:return PMIROpcode::Gt; case IROpcode::Ge:return PMIROpcode::Ge;
            case IROpcode::And:return PMIROpcode::And; case IROpcode::Or:return PMIROpcode::Or; case IROpcode::Branch:return PMIROpcode::Branch; case IROpcode::BranchIf:return PMIROpcode::BranchIf;
            case IROpcode::Call:return PMIROpcode::Call; case IROpcode::CallClosure:return PMIROpcode::CallClosure; case IROpcode::TextToString:return PMIROpcode::TextToString; case IROpcode::StringToText:return PMIROpcode::StringToText; case IROpcode::StringClear:return PMIROpcode::StringClear; case IROpcode::StringDispose:return PMIROpcode::StringDispose; case IROpcode::StringMove:return PMIROpcode::StringMove; case IROpcode::ByteCount:return PMIROpcode::ByteCount; case IROpcode::TextEquals:return PMIROpcode::TextEquals; case IROpcode::MakeAggregate:return PMIROpcode::MakeAggregate;
            case IROpcode::ExtractField:return PMIROpcode::ExtractField; case IROpcode::StoreField:return PMIROpcode::StoreField; case IROpcode::ArrayIndex:return PMIROpcode::ArrayIndex; case IROpcode::ArrayStore:return PMIROpcode::ArrayStore; case IROpcode::ResultOk:return PMIROpcode::ResultOk; case IROpcode::ResultErr:return PMIROpcode::ResultErr;
            case IROpcode::ResultIsErr:return PMIROpcode::ResultIsErr; case IROpcode::ResultTry:return PMIROpcode::ResultTry; case IROpcode::OptionSome:return PMIROpcode::OptionSome; case IROpcode::OptionNone:return PMIROpcode::OptionNone;
            case IROpcode::MakeClosure:return PMIROpcode::MakeClosure; case IROpcode::Return:return PMIROpcode::Return;
            case IROpcode::Nop:return PMIROpcode::Nop;
            default:throw std::runtime_error("PMIR cannot legalize PCIR opcode " + std::to_string(static_cast<int>(op)));
            }
        }
        static std::vector<IRValueType> collectVregTypes(const IRFunction& fn) {
            std::vector<IRValueType> types(fn.totalVregs + 1, makeIRUnknownType());
            for (std::size_t i = 0; i < fn.parameterVregs.size() && i < fn.parameterValueTypes.size(); ++i) types[static_cast<std::uint32_t>(fn.parameterVregs[i])] = fn.parameterValueTypes[i];
            for (const auto& b : fn.blocks) { for (const auto& p : b.phiNodes)if (p.dest != VReg::None)types[static_cast<std::uint32_t>(p.dest)] = p.valueType; for (const auto& i : b.instructions)if (i.dest != VReg::None)types[static_cast<std::uint32_t>(i.dest)] = i.valueType; }
            return types;
        }
        PMIRFunction lowerFunction(const IRFunction& original, const RichABIResolver& abi) const {
            const IRFunction normalized = lowerPhiEdges(original);
            const IRFunction& fn = normalized;
            PMIRFunction out; out.name = fn.name; out.returnType = fn.returnType; out.returnValueType = fn.valueType; out.richReturn = abi.isRich(fn.valueType); out.totalSlots = fn.totalVregs;
            out.parameterTypes = fn.parameterTypes; out.parameterModes = fn.parameterModes; out.parameterValueTypes = fn.parameterValueTypes;
            for (auto v : fn.parameterVregs)out.parameterSlots.push_back(static_cast<std::uint32_t>(v));
            const auto vregTypes = collectVregTypes(fn);
            // Current automatic drop glue handles direct string owners. A nested owner
            // requires recursive, tag-aware drop flags. Refuse unsafe shallow copies.
            std::function<bool(const std::string&, std::set<std::string>&)> containsOwner =
                [&](const std::string& type, std::set<std::string>& seen)->bool {
                if (type == "string") return true;
                if (type.empty() || type == "text" || !seen.insert(type).second) return false;
                const auto lay = abi.layout(type);
                if (lay.kind == ABIRepresentationKind::Result) return containsOwner(lay.okType, seen) || containsOwner(lay.errType, seen);
                if (lay.kind == ABIRepresentationKind::OptionTagged || lay.kind == ABIRepresentationKind::OptionPointerNiche)
                    return containsOwner(lay.valueType, seen);
                for (const auto& field : lay.fields) {
                    if (field.typeName == "string")return true;
                    if (field.typeName != type && containsOwner(field.typeName, seen))return true;
                }
                return false;
                };
            for (const auto& ty : vregTypes) {
                if (ty.kind != IRValueKind::Aggregate && ty.kind != IRValueKind::Result &&
                    ty.kind != IRValueKind::Option && ty.kind != IRValueKind::Closure)continue;
                std::set<std::string> visited;
                if (containsOwner(ty.name, visited))
                    throw std::runtime_error("E0426 nested owned string in '" + ty.name + "' requires recursive aggregate drop/move legalization; refusing unsafe shallow copy");
            }
            std::size_t maxAbiArgs = 0;
            for (const auto& b : fn.blocks)for (const auto& i : b.instructions) {
                if (i.op == IROpcode::Call) { std::size_t n = i.args.size() + (abi.isRich(i.valueType) ? 1 : 0); maxAbiArgs = std::max(maxAbiArgs, n); }
                else if (i.op == IROpcode::CallClosure && !i.args.empty()) { auto cl = abi.layout(vregTypes[static_cast<std::uint32_t>(i.args[0])]); std::size_t n = cl.fields.size() + (i.args.size() - 1) + (abi.isRich(i.valueType) ? 1 : 0); maxAbiArgs = std::max(maxAbiArgs, n); }
            }
            out.outgoingArea = static_cast<std::uint32_t>(32 + (maxAbiArgs > 4 ? (maxAbiArgs - 4) * 8 : 0));
            std::uint32_t cursor = out.totalSlots * 8u;
            if (out.richReturn) { out.sretCellOffset = cursor + 8u; cursor = out.sretCellOffset; }
            for (std::uint32_t slot = 1; slot <= out.totalSlots; ++slot) { PMIRSlotInfo info; info.type = vregTypes[slot]; info.cellOffset = slot * 8u; auto lay = abi.layout(info.type); info.rich = lay.rich; info.objectSize = static_cast<std::uint32_t>(lay.size); info.objectAlignment = static_cast<std::uint32_t>(std::max<std::size_t>(lay.alignment, 1)); if (info.rich) { cursor = alignUp32(cursor + std::max<std::uint32_t>(info.objectSize, 1), info.objectAlignment); info.objectOffset = cursor; }out.slots[slot] = info; }
            out.frameSize = align16(cursor + out.outgoingArea);
            for (const auto& b : fn.blocks) {
                PMIRBlock pb; pb.id = static_cast<std::uint32_t>(b.id); pb.label = b.labelName;
                if (!b.phiNodes.empty()) throw std::runtime_error("PMIR internal error: unlowered SSA phi");
                for (const auto& i : b.instructions) {
                    PMIRInstruction p; p.op = map(i.op); p.destSlot = static_cast<std::uint32_t>(i.dest); p.callee = i.callee; p.typeName = i.typeName; p.literalText = i.literalText; p.returnType = i.returnType; p.valueType = i.valueType; p.argumentTypes = i.argumentTypes; p.argumentValueTypes = i.argumentValueTypes; p.immediate = i.immediate; p.hasImmediate = i.hasImmediate;
                    if (i.op == IROpcode::ConstInt) { p.immediate = parseParacineInteger(i.literalText); p.hasImmediate = true; }
                    if (i.op == IROpcode::ConstFloat) {
                        if (i.valueType.kind == IRValueKind::Float32) { const float x = parseParacineFloating<float>(i.literalText); std::uint32_t bits; std::memcpy(&bits, &x, 4); p.immediate = static_cast<std::int64_t>(bits); }
                        else if (i.valueType.kind == IRValueKind::Float64) { const double x = parseParacineFloating<double>(i.literalText); std::uint64_t bits; std::memcpy(&bits, &x, 8); p.immediate = static_cast<std::int64_t>(bits); }
                        else throw std::runtime_error("floating literal needs concrete float/double type");
                        p.hasImmediate = true;
                    }if (i.op == IROpcode::ConstBool) { p.immediate = i.immediate ? 1 : 0; p.hasImmediate = true; }if (i.op == IROpcode::ConstNull) { p.immediate = 0; p.hasImmediate = true; }
                    for (auto v : i.args)p.srcSlots.push_back(static_cast<std::uint32_t>(v)); for (auto t : i.targetBlocks)p.targets.push_back(static_cast<std::uint32_t>(t)); pb.instructions.push_back(std::move(p));
                }
                out.blocks.push_back(std::move(pb));
            }
            return out;
        }
    };

    class PMIRVerifier {
    public:
        std::vector<std::string> verify(const PMIRProgram& program) const {
            std::vector<std::string> errors; std::unordered_set<std::string> names; RichABIResolver abi(program.skm, program.nominalTypes);
            for (const auto& fn : program.functions) {
                if (!names.insert(fn.name).second)errors.push_back("duplicate PMIR function: " + fn.name); if ((fn.frameSize & 15u) != 0)errors.push_back(fn.name + ": frame is not 16-byte aligned"); if (fn.outgoingArea < 32)errors.push_back(fn.name + ": Microsoft x64 shadow space is missing"); if (fn.richReturn && !fn.sretCellOffset)errors.push_back(fn.name + ": rich return missing hidden sret cell");
                std::unordered_set<std::uint32_t>blocks; for (const auto& b : fn.blocks)blocks.insert(b.id); auto slotOK = [&](std::uint32_t s) {return s == 0 || s <= fn.totalSlots; }; for (auto p : fn.parameterSlots)if (!slotOK(p) || p == 0)errors.push_back(fn.name + ": invalid parameter stack slot");
                const auto usable = fn.frameSize - fn.outgoingArea; for (const auto& [slot, si] : fn.slots) { if (si.cellOffset > usable)errors.push_back(fn.name + ": slot cell overlaps outgoing call area"); if (si.rich && (si.objectOffset > usable || si.objectOffset < si.objectSize))errors.push_back(fn.name + ": rich object storage outside local frame"); (void)slot; }
                for (const auto& b : fn.blocks)for (const auto& i : b.instructions) { if (!slotOK(i.destSlot))errors.push_back(fn.name + ": invalid PMIR destination slot"); for (auto x : i.srcSlots)if (!slotOK(x) || x == 0)errors.push_back(fn.name + ": invalid PMIR source slot"); for (auto t : i.targets)if (!blocks.contains(t))errors.push_back(fn.name + ": invalid PMIR branch target"); if (i.op == PMIROpcode::Branch && i.targets.size() != 1)errors.push_back(fn.name + ": PMIR branch requires one target"); if (i.op == PMIROpcode::BranchIf && (i.targets.size() != 2 || i.srcSlots.size() != 1))errors.push_back(fn.name + ": PMIR conditional branch shape invalid"); }
            }return errors;
        }
    };

    static std::string pmirOpcodeName(PMIROpcode op) {
        switch (op) {
        case PMIROpcode::ConstInt:return"mov.imm"; case PMIROpcode::ConstFloat:return"mov.fp.bits"; case PMIROpcode::ConstBool:return"mov.bool"; case PMIROpcode::ConstNull:return"mov.null"; case PMIROpcode::ConstText:return"text.literal"; case PMIROpcode::Copy:return"copy";
        case PMIROpcode::Add:return"add"; case PMIROpcode::Sub:return"sub"; case PMIROpcode::Mul:return"imul"; case PMIROpcode::Div:return"idiv.defined"; case PMIROpcode::Mod:return"imod.defined"; case PMIROpcode::BitAnd:return"and"; case PMIROpcode::BitOr:return"or"; case PMIROpcode::BitXor:return"xor"; case PMIROpcode::Shl:return"shl"; case PMIROpcode::Shr:return"sar"; case PMIROpcode::Neg:return"neg"; case PMIROpcode::BitNot:return"not"; case PMIROpcode::Not:return"logical.not"; case PMIROpcode::Eq:return"cmp.eq"; case PMIROpcode::Ne:return"cmp.ne"; case PMIROpcode::Lt:return"cmp.lt"; case PMIROpcode::Le:return"cmp.le"; case PMIROpcode::Gt:return"cmp.gt"; case PMIROpcode::Ge:return"cmp.ge"; case PMIROpcode::And:return"logical.and"; case PMIROpcode::Or:return"logical.or"; case PMIROpcode::Branch:return"jmp"; case PMIROpcode::BranchIf:return"br.if"; case PMIROpcode::Call:return"call"; case PMIROpcode::CallClosure:return"call.closure"; case PMIROpcode::TextToString:return"text.to_string"; case PMIROpcode::StringToText:return"string.to_text"; case PMIROpcode::StringClear:return"string.clear"; case PMIROpcode::StringDispose:return"string.dispose"; case PMIROpcode::StringMove:return"string.move"; case PMIROpcode::ByteCount:return"text.count"; case PMIROpcode::TextEquals:return"text.equals"; case PMIROpcode::MakeAggregate:return"aggregate.make"; case PMIROpcode::ExtractField:return"aggregate.field"; case PMIROpcode::StoreField:return"aggregate.store.field"; case PMIROpcode::ArrayIndex:return"array.index.checked"; case PMIROpcode::ArrayStore:return"array.store.checked"; case PMIROpcode::ResultOk:return"result.ok"; case PMIROpcode::ResultErr:return"result.err"; case PMIROpcode::ResultIsErr:return"result.is_err"; case PMIROpcode::ResultTry:return"result.try"; case PMIROpcode::OptionSome:return"option.some"; case PMIROpcode::OptionNone:return"option.none"; case PMIROpcode::MakeClosure:return"closure.make"; case PMIROpcode::Return:return"ret"; case PMIROpcode::Nop:return"nop";
        }return"?";
    }

    std::string pmirToString(const PMIRProgram& program) { std::ostringstream oss; for (const auto& fn : program.functions) { oss << "pmir routine " << fn.name << " frame=" << fn.frameSize << " shadow/out=" << fn.outgoingArea; if (fn.richReturn)oss << " sret=" << fn.returnType; oss << "\n"; for (const auto& [s, si] : fn.slots)if (si.rich)oss << "  rich s" << s << " " << irValueTypeName(si.type) << " object@rbp-" << si.objectOffset << " size=" << si.objectSize << "\n"; for (const auto& b : fn.blocks) { oss << "  .L" << b.id << " " << b.label << ":\n"; for (const auto& i : b.instructions) { oss << "    " << pmirOpcodeName(i.op); if (i.destSlot)oss << " s" << i.destSlot; for (auto s : i.srcSlots)oss << " s" << s; if (i.hasImmediate)oss << " " << i.immediate; for (auto t : i.targets)oss << " .L" << t; if (!i.callee.empty())oss << " " << i.callee; if (!i.valueType.name.empty())oss << " :" << i.valueType.name; oss << "\n"; } }oss << "\n"; }return oss.str(); }

    // --- Integrated x86-64 encoder with rich-value legalization ----------------
    struct MachineListingLine { std::size_t offset = 0; std::vector<std::uint8_t>bytes; std::string text; };
    struct MachineProgram { struct ImportCall { std::size_t dispPos; std::string symbol; }; std::vector<ImportCall> imports; std::vector<std::uint8_t>text; std::unordered_map<std::string, std::uint32_t>functionOffsets; std::unordered_map<std::string, std::uint32_t>functionSizes; std::vector<MachineListingLine>listing; std::uint32_t entryOffset = 0; std::string entryName; };

class MachineVerifier { public:std::vector<std::string>verify(const MachineProgram& machine)const { std::vector<std::string>errors; if (machine.text.empty())errors.push_back("machine text section is empty"); if (!machine.entryName.empty()) { auto it = machine.functionOffsets.find(machine.entryName); if (it == machine.functionOffsets.end() || it->second != machine.entryOffset)errors.push_back("entry point does not match selected function"); }std::vector<std::pair<std::uint32_t, std::uint32_t>>ranges; for (const auto& [name, off] : machine.functionOffsets) { auto sz = machine.functionSizes.find(name); if (sz == machine.functionSizes.end()) { errors.push_back("missing machine function size: " + name); continue; }if (static_cast<std::uint64_t>(off) + sz->second > machine.text.size())errors.push_back("machine function outside .text: " + name); ranges.emplace_back(off, off + sz->second); }std::sort(ranges.begin(), ranges.end()); for (std::size_t i = 1; i < ranges.size(); ++i)if (ranges[i].first < ranges[i - 1].second)errors.push_back("machine function ranges overlap"); for (const auto& line : machine.listing) { if (line.offset + line.bytes.size() > machine.text.size()) { errors.push_back("machine listing outside .text"); continue; }if (!std::equal(line.bytes.begin(), line.bytes.end(), machine.text.begin() + static_cast<std::ptrdiff_t>(line.offset)))errors.push_back("decoded listing does not match final encoded bytes"); }return errors; } };

                              class X64Encoder {
                              public:
                                  MachineProgram encode(const PMIRProgram& program, const std::string& requestedEntry = {})const {
                                      MachineProgram out; RichABIResolver abi(program.skm, program.nominalTypes); struct CallFix { std::size_t dispPos; std::string callee; }; struct LitFix { std::size_t dispPos; std::string literal; }; std::vector<CallFix>calls; std::vector<LitFix>lits;
                                      for (const auto& fn : program.functions) { while (out.text.size() % 16)out.text.push_back(0x90); const std::size_t base = out.text.size(); out.functionOffsets[fn.name] = static_cast<std::uint32_t>(base); auto enc = encodeFunction(fn, abi); for (auto& line : enc.listing) { line.offset += base; out.listing.push_back(std::move(line)); }for (const auto& c : enc.calls)calls.push_back({ base + c.dispPos,c.callee }); for (const auto& l : enc.literals)lits.push_back({ base + l.dispPos,l.literal }); for (const auto& imp : enc.imports)out.imports.push_back({ base + imp.dispPos,imp.symbol }); out.text.insert(out.text.end(), enc.bytes.begin(), enc.bytes.end()); out.functionSizes[fn.name] = static_cast<std::uint32_t>(enc.bytes.size()); }
                                      for (const auto& fix : calls) { auto it = out.functionOffsets.find(fix.callee); if (it == out.functionOffsets.end())throw std::runtime_error("native linker unresolved routine: " + fix.callee); std::int64_t rel = static_cast<std::int64_t>(it->second) - static_cast<std::int64_t>(fix.dispPos + 4); if (rel<std::numeric_limits<std::int32_t>::min() || rel>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("native call relocation out of rel32 range"); patch32(out.text, fix.dispPos, static_cast<std::int32_t>(rel)); }
                                      std::unordered_map<std::string, std::uint32_t>pool; for (const auto& fix : lits) { auto it = pool.find(fix.literal); std::uint32_t off = 0; if (it == pool.end()) { off = static_cast<std::uint32_t>(out.text.size()); pool.emplace(fix.literal, off); out.text.insert(out.text.end(), fix.literal.begin(), fix.literal.end()); out.text.push_back(0); } else off = it->second; std::int64_t rel = static_cast<std::int64_t>(off) - static_cast<std::int64_t>(fix.dispPos + 4); patch32(out.text, fix.dispPos, static_cast<std::int32_t>(rel)); }
                                      selectEntry(program, requestedEntry, out); for (auto& line : out.listing) { const std::size_t n = line.bytes.size(); if (line.offset + n <= out.text.size())line.bytes.assign(out.text.begin() + static_cast<std::ptrdiff_t>(line.offset), out.text.begin() + static_cast<std::ptrdiff_t>(line.offset + n)); }return out;
                                  }
                              private:
                                  struct EncodedFunction { std::vector<std::uint8_t>bytes; std::vector<MachineListingLine>listing; struct Call { std::size_t dispPos; std::string callee; }; struct Literal { std::size_t dispPos; std::string literal; }; struct Import { std::size_t dispPos; std::string symbol; }; std::vector<Call>calls; std::vector<Literal>literals; std::vector<Import>imports; };
                                  struct BranchFix { std::size_t dispPos; std::uint32_t target; };
                                  static void put32(std::vector<std::uint8_t>& b, std::int32_t v) { for (int i = 0; i < 4; ++i)b.push_back(static_cast<std::uint8_t>((static_cast<std::uint32_t>(v) >> (i * 8)) & 0xff)); }
                                  static void put64(std::vector<std::uint8_t>& b, std::uint64_t v) { for (int i = 0; i < 8; ++i)b.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xff)); }
                                  static void patch32(std::vector<std::uint8_t>& b, std::size_t p, std::int32_t v) { for (int i = 0; i < 4; ++i)b[p + i] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(v) >> (i * 8)) & 0xff); }
                                  static std::int32_t negDisp(std::uint32_t off) { return -static_cast<std::int32_t>(off); }
                                  static void rex(std::vector<std::uint8_t>& b, bool w, int r, int base) { std::uint8_t x = 0x40; if (w)x |= 8; if (r >= 8)x |= 4; if (base >= 8)x |= 1; if (x != 0x40)b.push_back(x); }
                                  static void movImm(std::vector<std::uint8_t>& b, int reg, std::uint64_t imm) { b.push_back(static_cast<std::uint8_t>(0x48 | (reg >= 8 ? 1 : 0))); b.push_back(static_cast<std::uint8_t>(0xB8 + (reg & 7))); put64(b, imm); }
                                  static void movReg(std::vector<std::uint8_t>& b, int dst, int src) { std::uint8_t r = 0x48; if (src >= 8)r |= 4; if (dst >= 8)r |= 1; b.push_back(r); b.push_back(0x89); b.push_back(static_cast<std::uint8_t>(0xC0 | ((src & 7) << 3) | (dst & 7))); }
                                  static void loadRbp(std::vector<std::uint8_t>& b, int reg, std::int32_t disp) { b.push_back(static_cast<std::uint8_t>(0x48 | (reg >= 8 ? 4 : 0))); b.push_back(0x8B); b.push_back(static_cast<std::uint8_t>(0x85 | ((reg & 7) << 3))); put32(b, disp); }
                                  static void storeRbp(std::vector<std::uint8_t>& b, int reg, std::int32_t disp) { b.push_back(static_cast<std::uint8_t>(0x48 | (reg >= 8 ? 4 : 0))); b.push_back(0x89); b.push_back(static_cast<std::uint8_t>(0x85 | ((reg & 7) << 3))); put32(b, disp); }
                                  static void leaRbp(std::vector<std::uint8_t>& b, int reg, std::int32_t disp) { b.push_back(static_cast<std::uint8_t>(0x48 | (reg >= 8 ? 4 : 0))); b.push_back(0x8D); b.push_back(static_cast<std::uint8_t>(0x85 | ((reg & 7) << 3))); put32(b, disp); }
                                  static void storeRspRax(std::vector<std::uint8_t>& b, std::int32_t disp) { b.insert(b.end(), { 0x48,0x89,0x84,0x24 }); put32(b, disp); }
                                  static void loadMem(std::vector<std::uint8_t>& b, int dst, int base, std::int32_t disp, std::size_t width, bool sign) {
                                      if (width >= 8) { rex(b, true, dst, base); b.push_back(0x8B); b.push_back(static_cast<std::uint8_t>(0x80 | ((dst & 7) << 3) | (base & 7))); put32(b, disp); return; }
                                      if (width == 4 && sign) { rex(b, true, dst, base); b.push_back(0x63); b.push_back(static_cast<std::uint8_t>(0x80 | ((dst & 7) << 3) | (base & 7))); put32(b, disp); return; }
                                      if (width == 4) { rex(b, false, dst, base); b.push_back(0x8B); b.push_back(static_cast<std::uint8_t>(0x80 | ((dst & 7) << 3) | (base & 7))); put32(b, disp); return; }
                                      rex(b, true, dst, base); b.push_back(0x0F); b.push_back(static_cast<std::uint8_t>(width == 2 ? (sign ? 0xBF : 0xB7) : (sign ? 0xBE : 0xB6))); b.push_back(static_cast<std::uint8_t>(0x80 | ((dst & 7) << 3) | (base & 7))); put32(b, disp);
                                  }
                                  static void storeMem(std::vector<std::uint8_t>& b, int base, std::int32_t disp, int src, std::size_t width) {
                                      if (width >= 8) { rex(b, true, src, base); b.push_back(0x89); }
                                      else if (width == 4) { rex(b, false, src, base); b.push_back(0x89); }
                                      else if (width == 2) { b.push_back(0x66); rex(b, false, src, base); b.push_back(0x89); }
                                      else { rex(b, false, src, base); b.push_back(0x88); }b.push_back(static_cast<std::uint8_t>(0x80 | ((src & 7) << 3) | (base & 7))); put32(b, disp);
                                  }
                                  static void storeSlot(std::vector<std::uint8_t>& b, int reg, const PMIRFunction& fn, std::uint32_t slot) { storeRbp(b, reg, negDisp(fn.slots.at(slot).cellOffset)); }
                                  static void loadSlot(std::vector<std::uint8_t>& b, int reg, const PMIRFunction& fn, std::uint32_t slot) { loadRbp(b, reg, negDisp(fn.slots.at(slot).cellOffset)); }
                                  static void leaObject(std::vector<std::uint8_t>& b, int reg, const PMIRFunction& fn, std::uint32_t slot) { const auto& si = fn.slots.at(slot); if (!si.rich || !si.objectOffset)throw std::runtime_error("x64 rich destination has no object storage"); leaRbp(b, reg, negDisp(si.objectOffset)); }
                                  static void epilogue(std::vector<std::uint8_t>& b) { b.insert(b.end(), { 0x48,0x89,0xEC,0x5D,0xC3 }); }
                                  static void normalizeBool(std::vector<std::uint8_t>& b, std::uint8_t setcc) { b.insert(b.end(), { 0x48,0x83,0xF8,0x00,0x0F,setcc,0xC0,0x48,0x0F,0xB6,0xC0 }); }
                                  static std::vector<std::uint8_t>slice(const std::vector<std::uint8_t>& b, std::size_t a, std::size_t z) { return std::vector<std::uint8_t>(b.begin() + static_cast<std::ptrdiff_t>(a), b.begin() + static_cast<std::ptrdiff_t>(z)); }
                                  static void copyFixed(std::vector<std::uint8_t>& b, int src, int dst, std::size_t size) { std::size_t off = 0; for (; off + 8 <= size; off += 8) { loadMem(b, 0, src, static_cast<std::int32_t>(off), 8, false); storeMem(b, dst, static_cast<std::int32_t>(off), 0, 8); }if (off + 4 <= size) { loadMem(b, 0, src, static_cast<std::int32_t>(off), 4, false); storeMem(b, dst, static_cast<std::int32_t>(off), 0, 4); off += 4; }if (off + 2 <= size) { loadMem(b, 0, src, static_cast<std::int32_t>(off), 2, false); storeMem(b, dst, static_cast<std::int32_t>(off), 0, 2); off += 2; }if (off < size) { loadMem(b, 0, src, static_cast<std::int32_t>(off), 1, false); storeMem(b, dst, static_cast<std::int32_t>(off), 0, 1); } }
                                  static void zeroFixed(std::vector<std::uint8_t>& b, int dst, std::size_t size) { movImm(b, 0, 0); std::size_t off = 0; for (; off + 8 <= size; off += 8)storeMem(b, dst, static_cast<std::int32_t>(off), 0, 8); if (off + 4 <= size) { storeMem(b, dst, static_cast<std::int32_t>(off), 0, 4); off += 4; }if (off + 2 <= size) { storeMem(b, dst, static_cast<std::int32_t>(off), 0, 2); off += 2; }if (off < size)storeMem(b, dst, static_cast<std::int32_t>(off), 0, 1); }
                                  static std::size_t emitLocalJcc(std::vector<std::uint8_t>& b, std::uint8_t cc) { b.push_back(0x0F); b.push_back(cc); auto p = b.size(); put32(b, 0); return p; }static std::size_t emitLocalJmp(std::vector<std::uint8_t>& b) { b.push_back(0xE9); auto p = b.size(); put32(b, 0); return p; }static void patchLocal(std::vector<std::uint8_t>& b, std::size_t p, std::size_t target) { patch32(b, p, static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(p + 4))); }
                                  static void emitJmp(std::vector<std::uint8_t>& b, std::uint32_t target, std::vector<BranchFix>& fs) { b.push_back(0xE9); std::size_t p = b.size(); put32(b, 0); fs.push_back({ p,target }); }static void emitJcc(std::vector<std::uint8_t>& b, std::uint8_t cc, std::uint32_t target, std::vector<BranchFix>& fs) { b.push_back(0x0F); b.push_back(cc); std::size_t p = b.size(); put32(b, 0); fs.push_back({ p,target }); }

                                  static bool fpType(IRValueKind kind) { return kind == IRValueKind::Float32 || kind == IRValueKind::Float64; }
                                  // Microsoft x64 scalar IEEE-754 lanes: XMM0..XMM3 correspond to the
                                  // first four positional ABI argument slots, XMM0 is the return lane.
                                  // Every virtual slot is eight bytes; float32 occupies the low four.
                                  static void xmmRbp(std::vector<std::uint8_t>& b, int xmm, std::int32_t disp, bool store, bool single) {
                                      b.push_back(single ? 0xF3 : 0xF2);
                                      b.insert(b.end(), { 0x0F,static_cast<std::uint8_t>(store ? 0x11 : 0x10),static_cast<std::uint8_t>(0x85 | ((xmm & 7) << 3)) });
                                      put32(b, disp);
                                  }
                                  static void xmmRbpOp(std::vector<std::uint8_t>& b, int xmm, std::int32_t disp, std::uint8_t op, bool single) {
                                      b.push_back(single ? 0xF3 : 0xF2); b.insert(b.end(), { 0x0F,op,static_cast<std::uint8_t>(0x85 | ((xmm & 7) << 3)) }); put32(b, disp);
                                  }
                                  static void xmmSlotLoad(std::vector<std::uint8_t>& b, const PMIRFunction& fn, std::uint32_t slot, int xmm = 0) {
                                      auto& info = fn.slots.at(slot); xmmRbp(b, xmm, negDisp(info.cellOffset), false, info.type.kind == IRValueKind::Float32);
                                  }
                                  static void xmmSlotStore(std::vector<std::uint8_t>& b, const PMIRFunction& fn, std::uint32_t slot, int xmm = 0) {
                                      auto& info = fn.slots.at(slot); xmmRbp(b, xmm, negDisp(info.cellOffset), true, info.type.kind == IRValueKind::Float32);
                                  }
                                  static void xmmCompare(std::vector<std::uint8_t>& b, bool single) {
                                      if (!single)b.push_back(0x66); b.insert(b.end(), { 0x0F,0x2E,0xC1 });
                                  }
                                  static void floatOp(const PMIRFunction& fn, const PMIRInstruction& i, std::vector<std::uint8_t>& b) {
                                      const bool single = fn.slots.at(i.srcSlots.at(0)).type.kind == IRValueKind::Float32;
                                      xmmSlotLoad(b, fn, i.srcSlots[0]);
                                      if (i.op == PMIROpcode::Eq || i.op == PMIROpcode::Ne || i.op == PMIROpcode::Lt ||
                                          i.op == PMIROpcode::Le || i.op == PMIROpcode::Gt || i.op == PMIROpcode::Ge) {
                                          xmmSlotLoad(b, fn, i.srcSlots[1], 1); xmmCompare(b, single);
                                          // UCOMIS[D] returns PF=1 on unordered comparisons: explicit NaN logic.
                                          const std::uint8_t cc = i.op == PMIROpcode::Eq ? 0x94 : i.op == PMIROpcode::Ne ? 0x95 :
                                              i.op == PMIROpcode::Lt ? 0x92 : i.op == PMIROpcode::Le ? 0x96 :
                                              i.op == PMIROpcode::Gt ? 0x97 : 0x93;
                                          b.insert(b.end(), { 0x0F,cc,0xC0 }); // setcc al
                                          if (i.op == PMIROpcode::Eq || i.op == PMIROpcode::Lt || i.op == PMIROpcode::Le) {
                                              b.insert(b.end(), { 0x0F,0x9B,0xC1,0x20,0xC8 }); // setnp cl; and al,cl
                                          }
                                          else if (i.op == PMIROpcode::Ne) {
                                              b.insert(b.end(), { 0x0F,0x9A,0xC1,0x08,0xC8 }); // setp cl; or al,cl
                                          }
                                          b.insert(b.end(), { 0x0F,0xB6,0xC0 });
                                          storeSlot(b, 0, fn, i.destSlot); return;
                                      }
                                      const std::uint8_t op = i.op == PMIROpcode::Add ? 0x58 : i.op == PMIROpcode::Sub ? 0x5C :
                                          i.op == PMIROpcode::Mul ? 0x59 : i.op == PMIROpcode::Div ? 0x5E : 0;
                                      if (!op)throw std::runtime_error("unsupported x64 floating-point operation");
                                      xmmRbpOp(b, 0, negDisp(fn.slots.at(i.srcSlots[1]).cellOffset), op, single);
                                      xmmSlotStore(b, fn, i.destSlot);
                                  }

                                  static void loadIncoming(std::vector<std::uint8_t>& b, std::size_t abiIndex) { const int regs[4] = { 1,2,8,9 }; if (abiIndex < 4)movReg(b, 0, regs[abiIndex]); else loadRbp(b, 0, static_cast<std::int32_t>(48 + (abiIndex - 4) * 8)); }
                                  static void placeArgFromRax(std::vector<std::uint8_t>& b, std::size_t abiIndex) { const int regs[4] = { 1,2,8,9 }; if (abiIndex < 4)movReg(b, regs[abiIndex], 0); else storeRspRax(b, static_cast<std::int32_t>(32 + (abiIndex - 4) * 8)); }
                                  static void initRichDest(std::vector<std::uint8_t>& b, const PMIRFunction& fn, std::uint32_t slot) { leaObject(b, 0, fn, slot); storeSlot(b, 0, fn, slot); }
                                  static ABIFieldLayout findField(const ABITypeLayout& lay, const std::string& name) { for (const auto& f : lay.fields)if (f.name == name)return f; throw std::runtime_error("native legalization unknown field '" + name + "' in " + lay.typeName); }
                                  static void storeValueInto(std::vector<std::uint8_t>& b, const PMIRFunction& fn, std::uint32_t srcSlot, int dstBase, std::size_t dstOffset, const ABITypeLayout& valueLayout) { if (valueLayout.rich) { movReg(b, 11, dstBase); loadSlot(b, 10, fn, srcSlot); if (dstOffset) { movImm(b, 0, dstOffset);/* add r11,rax */b.insert(b.end(), { 0x49,0x01,0xC3 }); }copyFixed(b, 10, 11, valueLayout.size); } else { loadSlot(b, 0, fn, srcSlot); storeMem(b, dstBase, static_cast<std::int32_t>(dstOffset), 0, std::max<std::size_t>(valueLayout.size, 1)); } }
                                  static void loadValueFrom(std::vector<std::uint8_t>& b, const PMIRFunction& fn, std::uint32_t destSlot, int srcBase, std::size_t srcOffset, const ABITypeLayout& valueLayout) { if (valueLayout.rich) { initRichDest(b, fn, destSlot); movReg(b, 11, 0); movReg(b, 10, srcBase); if (srcOffset) { movImm(b, 0, srcOffset); b.insert(b.end(), { 0x49,0x01,0xC2 }); }copyFixed(b, 10, 11, valueLayout.size); } else { loadMem(b, 0, srcBase, static_cast<std::int32_t>(srcOffset), std::max<std::size_t>(valueLayout.size, 1), valueLayout.signedScalar); storeSlot(b, 0, fn, destSlot); } }

                                  EncodedFunction encodeFunction(const PMIRFunction& fn, const RichABIResolver& abi)const {
                                      EncodedFunction out; auto& b = out.bytes; std::unordered_map<std::uint32_t, std::size_t>blockOffsets; std::vector<BranchFix>branches; auto record = [&](std::size_t st, const std::string& t) {out.listing.push_back({ st,slice(b,st,b.size()),t }); }; std::size_t s = b.size(); b.push_back(0x55); b.insert(b.end(), { 0x48,0x89,0xE5 }); b.insert(b.end(), { 0x48,0x81,0xEC }); put32(b, static_cast<std::int32_t>(fn.frameSize)); record(s, "push rbp; mov rbp,rsp; sub rsp," + std::to_string(fn.frameSize));
                                      std::size_t abiBase = 0; if (fn.richReturn) { s = b.size(); storeRbp(b, 1, negDisp(fn.sretCellOffset)); record(s, "spill hidden sret"); abiBase = 1; }
                                      for (std::size_t i = 0; i < fn.parameterSlots.size(); ++i) {
                                          s = b.size(); const auto slot = fn.parameterSlots[i]; loadIncoming(b, abiBase + i); const auto& si = fn.slots.at(slot); if (si.rich) {/* Rich parameters remain indirect. */storeSlot(b, 0, fn, slot); }
                                          else if (fpType(si.type.kind) && abiBase + i < 4) { xmmSlotStore(b, fn, slot, static_cast<int>(abiBase + i)); }
                                          else storeSlot(b, 0, fn, slot); record(s, "legalize parameter -> s" + std::to_string(slot) + (si.rich ? " (indirect rich)" : ""));
                                      }
                                      for (const auto& block : fn.blocks) { blockOffsets[block.id] = b.size(); for (const auto& ins : block.instructions) { s = b.size(); encodeInstruction(fn, ins, abi, b, branches, out.calls, out.literals, out.imports); record(s, pmirOpcodeName(ins.op)); } }
                                      if (fn.blocks.empty() || b.empty() || b.back() != 0xC3) { s = b.size(); b.insert(b.end(), { 0x31,0xC0 }); epilogue(b); record(s, "implicit return 0"); }
                                      for (const auto& f : branches) { auto it = blockOffsets.find(f.target); if (it == blockOffsets.end())throw std::runtime_error("x64 encoder invalid branch target"); std::int64_t rel = static_cast<std::int64_t>(it->second) - static_cast<std::int64_t>(f.dispPos + 4); patch32(b, f.dispPos, static_cast<std::int32_t>(rel)); }return out;
                                  }

                                  static void emitDirectCall(std::vector<std::uint8_t>& b, const std::string& callee, std::vector<EncodedFunction::Call>& calls) { b.push_back(0xE8); auto p = b.size(); put32(b, 0); calls.push_back({ p,callee }); }
                                  static void emitImportedCall(std::vector<std::uint8_t>& b, const std::string& symbol, std::vector<EncodedFunction::Import>& imports) { b.push_back(0xFF); b.push_back(0x15); auto p = b.size(); put32(b, 0); imports.push_back({ p,symbol }); }
                                  void encodeCall(const PMIRFunction& fn, const PMIRInstruction& i, const RichABIResolver& abi, std::vector<std::uint8_t>& b, std::vector<EncodedFunction::Call>& calls)const {
                                      std::size_t ai = 0; const bool richRet = abi.isRich(i.valueType); if (richRet) { initRichDest(b, fn, i.destSlot); loadSlot(b, 0, fn, i.destSlot); placeArgFromRax(b, ai++); }for (std::size_t n = 0; n < i.srcSlots.size(); ++n) {
                                          const auto kind = fn.slots.at(i.srcSlots[n]).type.kind;
                                          if (fpType(kind) && ai < 4) { xmmSlotLoad(b, fn, i.srcSlots[n], static_cast<int>(ai)); ++ai; }
                                          else { loadSlot(b, 0, fn, i.srcSlots[n]); placeArgFromRax(b, ai++); }
                                      }
                                      emitDirectCall(b, i.callee, calls);
                                      if (i.destSlot && !richRet) { if (fpType(i.valueType.kind))xmmSlotStore(b, fn, i.destSlot); else storeSlot(b, 0, fn, i.destSlot); }
                                  }
                                  void encodeClosureCall(const PMIRFunction& fn, const PMIRInstruction& i, const RichABIResolver& abi, std::vector<std::uint8_t>& b, std::vector<EncodedFunction::Call>& calls)const {
                                      if (i.srcSlots.empty())throw std::runtime_error("closure call missing closure value"); const auto closureType = fn.slots.at(i.srcSlots[0]).type; auto cl = abi.layout(closureType); if (cl.kind != ABIRepresentationKind::Closure)throw std::runtime_error("call.closure source is not a concrete closure type"); loadSlot(b, 10, fn, i.srcSlots[0]); std::size_t ai = 0; const bool richRet = abi.isRich(i.valueType); if (richRet) { initRichDest(b, fn, i.destSlot); loadSlot(b, 0, fn, i.destSlot); placeArgFromRax(b, ai++); }for (const auto& f : cl.fields) {
                                          auto fl = abi.layout(f.typeName);
                                          if ((f.typeName == "float" || f.typeName == "double") && ai < 4) {
                                              b.push_back(f.typeName == "float" ? 0xF3 : 0xF2);
                                              rex(b, false, static_cast<int>(ai), 10); b.insert(b.end(), { 0x0F,0x10,
                                                 static_cast<std::uint8_t>(0x80 | ((ai & 7) << 3) | (10 & 7)) }); put32(b, static_cast<std::int32_t>(f.offset));
                                              ++ai; continue;
                                          }
                                          if (fl.rich) { movReg(b, 0, 10); if (f.offset) { movImm(b, 11, f.offset); b.insert(b.end(), { 0x4C,0x01,0xD8 }); } }
                                          else loadMem(b, 0, 10, static_cast<std::int32_t>(f.offset), std::max<std::size_t>(fl.size, 1), fl.signedScalar);
                                          placeArgFromRax(b, ai++);
                                      }for (std::size_t n = 1; n < i.srcSlots.size(); ++n) {
                                          const auto kind = fn.slots.at(i.srcSlots[n]).type.kind;
                                          if (fpType(kind) && ai < 4) { xmmSlotLoad(b, fn, i.srcSlots[n], static_cast<int>(ai)); ++ai; }
                                          else { loadSlot(b, 0, fn, i.srcSlots[n]); placeArgFromRax(b, ai++); }
                                      }
                                      emitDirectCall(b, cl.closureCallee, calls);
                                      if (i.destSlot && !richRet) { if (fpType(i.valueType.kind))xmmSlotStore(b, fn, i.destSlot); else storeSlot(b, 0, fn, i.destSlot); }
                                  }

                                  void encodeInstruction(const PMIRFunction& fn, const PMIRInstruction& i, const RichABIResolver& abi, std::vector<std::uint8_t>& b, std::vector<BranchFix>& branches, std::vector<EncodedFunction::Call>& calls, std::vector<EncodedFunction::Literal>& literals, std::vector<EncodedFunction::Import>& imports)const {
                                      auto unaryLoad = [&]() {if (i.srcSlots.empty())throw std::runtime_error("PMIR unary missing source"); loadSlot(b, 0, fn, i.srcSlots[0]); }; auto binaryLoad = [&]() {if (i.srcSlots.size() < 2)throw std::runtime_error("PMIR binary missing source"); loadSlot(b, 0, fn, i.srcSlots[0]); loadSlot(b, 10, fn, i.srcSlots[1]); }; auto save = [&]() {if (i.destSlot)storeSlot(b, 0, fn, i.destSlot); };
                                      if (i.srcSlots.size() >= 2 && fpType(fn.slots.at(i.srcSlots[0]).type.kind)) {
                                          switch (i.op) {
                                          case PMIROpcode::Add:case PMIROpcode::Sub:case PMIROpcode::Mul:case PMIROpcode::Div:
                                          case PMIROpcode::Eq:case PMIROpcode::Ne:case PMIROpcode::Lt:case PMIROpcode::Le:
                                          case PMIROpcode::Gt:case PMIROpcode::Ge:
                                              floatOp(fn, i, b); return;
                                          default:break;
                                          }
                                      }
                                      switch (i.op) {
                                      case PMIROpcode::ConstInt:case PMIROpcode::ConstFloat:case PMIROpcode::ConstBool:case PMIROpcode::ConstNull:movImm(b, 0, static_cast<std::uint64_t>(i.immediate)); save(); break;
                                      case PMIROpcode::ConstText: { initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); b.insert(b.end(), { 0x48,0x8D,0x05 }); auto p = b.size(); put32(b, 0); literals.push_back({ p,i.literalText }); storeMem(b, 11, 0, 0, 8); movImm(b, 0, static_cast<std::uint64_t>(i.literalText.size())); storeMem(b, 11, 8, 0, 8); break; }
                                      case PMIROpcode::Copy: { const auto& dst = fn.slots.at(i.destSlot); if (dst.rich) { initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); loadSlot(b, 10, fn, i.srcSlots.at(0)); copyFixed(b, 10, 11, dst.objectSize); } else { unaryLoad(); save(); }break; }
                                      case PMIROpcode::Add:binaryLoad(); b.insert(b.end(), { 0x4C,0x01,0xD0 }); save(); break; case PMIROpcode::Sub:binaryLoad(); b.insert(b.end(), { 0x4C,0x29,0xD0 }); save(); break; case PMIROpcode::Mul:binaryLoad(); b.insert(b.end(), { 0x49,0x0F,0xAF,0xC2 }); save(); break; case PMIROpcode::BitAnd:binaryLoad(); b.insert(b.end(), { 0x4C,0x21,0xD0 }); save(); break; case PMIROpcode::BitOr:binaryLoad(); b.insert(b.end(), { 0x4C,0x09,0xD0 }); save(); break; case PMIROpcode::BitXor:binaryLoad(); b.insert(b.end(), { 0x4C,0x31,0xD0 }); save(); break; case PMIROpcode::And:binaryLoad(); b.insert(b.end(), { 0x4C,0x21,0xD0 }); normalizeBool(b, 0x95); save(); break; case PMIROpcode::Or:binaryLoad(); b.insert(b.end(), { 0x4C,0x09,0xD0 }); normalizeBool(b, 0x95); save(); break; case PMIROpcode::Shl:loadSlot(b, 0, fn, i.srcSlots.at(0)); loadSlot(b, 1, fn, i.srcSlots.at(1)); b.insert(b.end(), { 0x48,0xD3,0xE0 }); save(); break; case PMIROpcode::Shr:loadSlot(b, 0, fn, i.srcSlots.at(0)); loadSlot(b, 1, fn, i.srcSlots.at(1)); b.insert(b.end(), { 0x48,0xD3,0xF8 }); save(); break; case PMIROpcode::Neg:unaryLoad(); if (fpType(i.valueType.kind)) {
                                          movImm(b, 10, i.valueType.kind == IRValueKind::Float32 ? 0x80000000ull : 0x8000000000000000ull);
                                          b.insert(b.end(), { 0x4C,0x31,0xD0 });
                                      }
                                                          else b.insert(b.end(), { 0x48,0xF7,0xD8 }); save(); break; case PMIROpcode::BitNot:unaryLoad(); b.insert(b.end(), { 0x48,0xF7,0xD0 }); save(); break; case PMIROpcode::Not:unaryLoad(); normalizeBool(b, 0x94); save(); break;
                                                          case PMIROpcode::Eq:case PMIROpcode::Ne:case PMIROpcode::Lt:case PMIROpcode::Le:case PMIROpcode::Gt:case PMIROpcode::Ge: { binaryLoad(); b.insert(b.end(), { 0x4C,0x39,0xD0 }); std::uint8_t cc = 0x94; if (i.op == PMIROpcode::Ne)cc = 0x95; else if (i.op == PMIROpcode::Lt)cc = 0x9C; else if (i.op == PMIROpcode::Le)cc = 0x9E; else if (i.op == PMIROpcode::Gt)cc = 0x9F; else if (i.op == PMIROpcode::Ge)cc = 0x9D; b.insert(b.end(), { 0x0F,cc,0xC0,0x48,0x0F,0xB6,0xC0 }); save(); break; }
                                                          case PMIROpcode::Div:case PMIROpcode::Mod: { binaryLoad(); b.insert(b.end(), { 0x49,0x83,0xFA,0x00 }); auto jz = emitLocalJcc(b, 0x84); b.insert(b.end(), { 0x49,0x83,0xFA,0xFF }); auto jne = emitLocalJcc(b, 0x85); movImm(b, 11, 0x8000000000000000ull); b.insert(b.end(), { 0x4C,0x39,0xD8 }); auto jne2 = emitLocalJcc(b, 0x85); if (i.op == PMIROpcode::Mod)b.insert(b.end(), { 0x31,0xC0 }); auto done = emitLocalJmp(b); auto normal = b.size(); patchLocal(b, jne, normal); patchLocal(b, jne2, normal); b.insert(b.end(), { 0x48,0x99,0x49,0xF7,0xFA }); if (i.op == PMIROpcode::Mod)b.insert(b.end(), { 0x48,0x89,0xD0 }); auto after = emitLocalJmp(b); auto trap = b.size(); patchLocal(b, jz, trap); b.insert(b.end(), { 0x0F,0x0B }); auto finish = b.size(); patchLocal(b, done, finish); patchLocal(b, after, finish); save(); break; }
                                                          case PMIROpcode::Branch:emitJmp(b, i.targets.at(0), branches); break; case PMIROpcode::BranchIf:loadSlot(b, 0, fn, i.srcSlots.at(0)); b.insert(b.end(), { 0x48,0x85,0xC0 }); emitJcc(b, 0x85, i.targets.at(0), branches); emitJmp(b, i.targets.at(1), branches); break;
                                                          case PMIROpcode::Call:encodeCall(fn, i, abi, b, calls); break; case PMIROpcode::CallClosure:encodeClosureCall(fn, i, abi, b, calls); break;
                                                          case PMIROpcode::TextEquals: {
                                                              if (i.srcSlots.size() != 2)throw std::runtime_error("text.equals requires two operands");
                                                              loadSlot(b, 10, fn, i.srcSlots[0]); loadMem(b, 8, 10, 8, 8, false);
                                                              loadSlot(b, 11, fn, i.srcSlots[1]); loadMem(b, 9, 11, 8, 8, false);
                                                              b.insert(b.end(), { 0x4D,0x39,0xC8 }); // cmp r8,r9
                                                              auto unequal = emitLocalJcc(b, 0x85);
                                                              loadMem(b, 10, 10, 0, 8, false); loadMem(b, 11, 11, 0, 8, false);
                                                              b.insert(b.end(), { 0x4D,0x85,0xC0 }); // test r8,r8
                                                              auto equalEmpty = emitLocalJcc(b, 0x84);
                                                              const auto loop = b.size();
                                                              loadMem(b, 0, 10, 0, 1, false); loadMem(b, 1, 11, 0, 1, false);
                                                              b.insert(b.end(), { 0x38,0xC8 }); // cmp al,cl
                                                              auto unequalByte = emitLocalJcc(b, 0x85);
                                                              b.insert(b.end(), { 0x49,0xFF,0xC2,0x49,0xFF,0xC3,0x49,0xFF,0xC8 });
                                                              auto repeat = emitLocalJcc(b, 0x85); patchLocal(b, repeat, loop);
                                                              const auto equal = b.size(); patchLocal(b, equalEmpty, equal);
                                                              movImm(b, 0, 1); auto end = emitLocalJmp(b);
                                                              const auto notEqual = b.size(); patchLocal(b, unequal, notEqual); patchLocal(b, unequalByte, notEqual);
                                                              movImm(b, 0, 0); patchLocal(b, end, b.size()); storeSlot(b, 0, fn, i.destSlot); break;
                                                          }
                                                          case PMIROpcode::ByteCount: { if (i.srcSlots.size() != 1)throw std::runtime_error("byte count missing source"); loadSlot(b, 10, fn, i.srcSlots[0]); loadMem(b, 0, 10, 8, 8, false); storeSlot(b, 0, fn, i.destSlot); break; }
                                                          case PMIROpcode::StringToText: { if (i.srcSlots.empty())throw std::runtime_error("string.to_text missing source"); initRichDest(b, fn, i.destSlot); loadSlot(b, 10, fn, i.srcSlots[0]); loadSlot(b, 11, fn, i.destSlot); copyFixed(b, 10, 11, 16); break; }
                                                          case PMIROpcode::StringClear: { if (i.srcSlots.empty())throw std::runtime_error("string.clear missing source"); loadSlot(b, 11, fn, i.srcSlots[0]); movImm(b, 0, 0); storeMem(b, 11, 8, 0, 8); break; }
                                                          case PMIROpcode::StringMove: {
                                                              if (i.srcSlots.size() != 1 || !i.destSlot || i.srcSlots[0] == i.destSlot)
                                                                  throw std::runtime_error("invalid consuming string move");
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              initRichDest(b, fn, i.destSlot);
                                                              loadSlot(b, 11, fn, i.destSlot);
                                                              copyFixed(b, 10, 11, 24);
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              zeroFixed(b, 10, 24);
                                                              break;
                                                          }
                                                          case PMIROpcode::StringDispose: {
                                                              if (i.srcSlots.size() != 1)throw std::runtime_error("string.dispose missing ownership source");
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              loadMem(b, 0, 10, 0, 8, false); b.insert(b.end(), { 0x48,0x85,0xC0 });
                                                              auto noPointer = emitLocalJcc(b, 0x84);
                                                              emitImportedCall(b, "GetProcessHeap", imports);
                                                              movReg(b, 1, 0); // heap
                                                              movImm(b, 2, 0); // flags
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              loadMem(b, 8, 10, 0, 8, false); // buffer address
                                                              emitImportedCall(b, "HeapFree", imports);
                                                              b.insert(b.end(), { 0x48,0x85,0xC0 });
                                                              auto freed = emitLocalJcc(b, 0x85); b.insert(b.end(), { 0x0F,0x0B });
                                                              patchLocal(b, freed, b.size()); patchLocal(b, noPointer, b.size());
                                                              loadSlot(b, 11, fn, i.srcSlots[0]); zeroFixed(b, 11, 24);
                                                              break;
                                                          }
                                                          case PMIROpcode::TextToString: {
                                                              if (i.srcSlots.size() != 1)throw std::runtime_error("text.to_string requires one text input");
                                                              // Owned string ABI {ptr,len,capacity}. The actual storage is obtained
                                                              // from the Windows process heap and never aliases the source literal.
                                                              initRichDest(b, fn, i.destSlot);
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              loadMem(b, 0, 10, 8, 8, false); // count
                                                              // Distinct overflow guard for len+1; 0xffff... is unrepresentable.
                                                              b.insert(b.end(), { 0x48,0x83,0xF8,0xFF });
                                                              auto notOverflow = emitLocalJcc(b, 0x85);
                                                              b.insert(b.end(), { 0x0F,0x0B });
                                                              patchLocal(b, notOverflow, b.size());
                                                              // GetProcessHeap() -> RAX
                                                              emitImportedCall(b, "GetProcessHeap", imports);
                                                              movReg(b, 1, 0); // RCX=heap
                                                              movImm(b, 2, 8); // RDX=HEAP_ZERO_MEMORY
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              loadMem(b, 8, 10, 8, 8, false); // R8=byte count
                                                              b.insert(b.end(), { 0x49,0xFF,0xC0 }); // inc r8; includes terminator
                                                              emitImportedCall(b, "HeapAlloc", imports);
                                                              b.insert(b.end(), { 0x48,0x85,0xC0 });
                                                              auto allocated = emitLocalJcc(b, 0x85);
                                                              b.insert(b.end(), { 0x0F,0x0B }); // terminal OOM trap
                                                              patchLocal(b, allocated, b.size());
                                                              movReg(b, 8, 0); // retain the allocated buffer pointer in volatile R8
                                                              loadSlot(b, 11, fn, i.destSlot); // object header address
                                                              storeMem(b, 11, 0, 0, 8); // ptr
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              loadMem(b, 9, 10, 8, 8, false); // r9 length (loop counter)
                                                              storeMem(b, 11, 8, 9, 8); // count
                                                              storeMem(b, 11, 16, 9, 8); // capacity: count bytes (+1 allocated for NUL)
                                                              loadMem(b, 10, 10, 0, 8, false); // source pointer
                                                              movReg(b, 11, 8); // destination byte buffer, not the object header
                                                              b.insert(b.end(), { 0x4D,0x85,0xC9 }); // test r9,r9
                                                              auto done = emitLocalJcc(b, 0x84);
                                                              auto loop = b.size();
                                                              loadMem(b, 0, 10, 0, 1, false);
                                                              storeMem(b, 11, 0, 0, 1);
                                                              b.insert(b.end(), { 0x49,0xFF,0xC2,0x49,0xFF,0xC3,0x49,0xFF,0xC9 }); // inc r10; inc r11; dec r9
                                                              auto again = emitLocalJcc(b, 0x85);
                                                              patchLocal(b, again, loop);
                                                              patchLocal(b, done, b.size());
                                                              break;
                                                          }
                                                          case PMIROpcode::MakeAggregate: { auto lay = abi.layout(i.valueType); initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); for (std::size_t n = 0; n < i.srcSlots.size(); ++n) { const std::string field = n < i.argumentTypes.size() ? i.argumentTypes[n] : std::to_string(n); auto f = findField(lay, field); auto fl = abi.layout(f.typeName); storeValueInto(b, fn, i.srcSlots[n], 11, f.offset, fl); }break; }
                                                          case PMIROpcode::ExtractField: { if (i.srcSlots.empty())throw std::runtime_error("aggregate.field missing source"); auto owner = abi.layout(i.typeName.empty() ? fn.slots.at(i.srcSlots[0]).type.name : i.typeName); auto f = findField(owner, i.literalText); auto fl = abi.layout(f.typeName); loadSlot(b, 10, fn, i.srcSlots[0]); loadValueFrom(b, fn, i.destSlot, 10, f.offset, fl); break; }
                                                          case PMIROpcode::ArrayIndex: {
                                                              if (i.srcSlots.size() != 2)throw std::runtime_error("PMIR array index requires array and index");
                                                              std::string elem; std::size_t count = 0;
                                                              if (!typeIsArray(i.typeName, &elem, &count))throw std::runtime_error("PMIR malformed array type");
                                                              const auto layout = abi.layout(elem);
                                                              if (layout.size == 0 || layout.size > std::numeric_limits<std::int32_t>::max())throw std::runtime_error("PMIR unsupported array element stride");
                                                              loadSlot(b, 9, fn, i.srcSlots[1]); // r9 = index
                                                              movImm(b, 8, static_cast<std::uint64_t>(count)); // r8 = bound
                                                              b.insert(b.end(), { 0x4D,0x39,0xC1 }); // cmp r9,r8 (unsigned)
                                                              auto valid = emitLocalJcc(b, 0x82); // JB: index < bound
                                                              b.insert(b.end(), { 0x0F,0x0B }); // UD2 on out-of-bounds
                                                              patchLocal(b, valid, b.size());
                                                              if (layout.size != 1) { b.insert(b.end(), { 0x4D,0x69,0xC9 }); put32(b, static_cast<std::int32_t>(layout.size)); } // imul r9,r9,stride
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              b.insert(b.end(), { 0x4D,0x01,0xCA }); // add r10,r9
                                                              loadValueFrom(b, fn, i.destSlot, 10, 0, layout);
                                                              break;
                                                          }
                                                          case PMIROpcode::StoreField: {
                                                              if (i.srcSlots.size() != 2)throw std::runtime_error("PMIR record field store requires owner and value");
                                                              auto owner = abi.layout(i.typeName);
                                                              if (owner.kind != ABIRepresentationKind::Aggregate)throw std::runtime_error("PMIR record field store requires aggregate type");
                                                              const auto field = findField(owner, i.literalText); const auto fld = abi.layout(field.typeName);
                                                              loadSlot(b, 10, fn, i.srcSlots[0]); storeValueInto(b, fn, i.srcSlots[1], 10, field.offset, fld);
                                                              break;
                                                          }
                                                          case PMIROpcode::ArrayStore: {
                                                              if (i.srcSlots.size() != 3)throw std::runtime_error("PMIR array.store requires array,index,value");
                                                              std::string elem; std::size_t count = 0;
                                                              if (!typeIsArray(i.typeName, &elem, &count))throw std::runtime_error("PMIR malformed array.store type");
                                                              const auto elementLayout = abi.layout(elem);
                                                              if (elementLayout.size == 0 || elementLayout.size > std::numeric_limits<std::int32_t>::max())throw std::runtime_error("PMIR array.store stride overflow");
                                                              loadSlot(b, 9, fn, i.srcSlots[1]);
                                                              movImm(b, 8, static_cast<std::uint64_t>(count));
                                                              b.insert(b.end(), { 0x4D,0x39,0xC1 }); // cmp r9,r8 unsigned
                                                              auto valid = emitLocalJcc(b, 0x82);
                                                              b.insert(b.end(), { 0x0F,0x0B }); // trap on OOB
                                                              patchLocal(b, valid, b.size());
                                                              if (elementLayout.size != 1) { b.insert(b.end(), { 0x4D,0x69,0xC9 }); put32(b, static_cast<std::int32_t>(elementLayout.size)); }
                                                              loadSlot(b, 10, fn, i.srcSlots[0]);
                                                              b.insert(b.end(), { 0x4D,0x01,0xCA }); // add r10,r9
                                                              storeValueInto(b, fn, i.srcSlots[2], 10, 0, elementLayout);
                                                              break;
                                                          }
                                                          case PMIROpcode::ResultOk:case PMIROpcode::ResultErr: { auto lay = abi.layout(i.valueType); initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); movImm(b, 0, i.op == PMIROpcode::ResultErr ? 1 : 0); storeMem(b, 11, static_cast<std::int32_t>(lay.tagOffset), 0, 1); if (!i.srcSlots.empty()) { const auto pt = abi.layout(i.op == PMIROpcode::ResultErr ? lay.errType : lay.okType); storeValueInto(b, fn, i.srcSlots[0], 11, lay.payloadOffset, pt); }break; }
                                                          case PMIROpcode::ResultIsErr: { if (i.srcSlots.empty())throw std::runtime_error("result.is_err missing source"); auto lay = abi.layout(i.typeName.empty() ? fn.slots.at(i.srcSlots[0]).type.name : i.typeName); loadSlot(b, 10, fn, i.srcSlots[0]); loadMem(b, 0, 10, static_cast<std::int32_t>(lay.tagOffset), 1, false); storeSlot(b, 0, fn, i.destSlot); break; }
                                                          case PMIROpcode::ResultTry: { if (i.srcSlots.empty())throw std::runtime_error("result.try missing source"); auto srcLay = abi.layout(i.typeName.empty() ? fn.slots.at(i.srcSlots[0]).type.name : i.typeName); loadSlot(b, 10, fn, i.srcSlots[0]); loadMem(b, 0, 10, static_cast<std::int32_t>(srcLay.tagOffset), 1, false); b.insert(b.end(), { 0x48,0x85,0xC0 }); auto ok = emitLocalJcc(b, 0x84); if (!fn.richReturn)throw std::runtime_error("result.try error propagation requires rich result return ABI"); auto dstLay = abi.layout(fn.returnValueType); loadRbp(b, 11, negDisp(fn.sretCellOffset)); movImm(b, 0, 1); storeMem(b, 11, static_cast<std::int32_t>(dstLay.tagOffset), 0, 1); auto errLay = abi.layout(srcLay.errType); movReg(b, 0, 10); if (srcLay.payloadOffset) { movImm(b, 9, srcLay.payloadOffset); b.insert(b.end(), { 0x4C,0x01,0xC8 }); }movReg(b, 10, 0); movReg(b, 0, 11); if (dstLay.payloadOffset) { movImm(b, 9, dstLay.payloadOffset); b.insert(b.end(), { 0x4C,0x01,0xC8 }); }movReg(b, 11, 0); copyFixed(b, 10, 11, errLay.size); loadRbp(b, 0, negDisp(fn.sretCellOffset)); epilogue(b); auto okAt = b.size(); patchLocal(b, ok, okAt); loadSlot(b, 10, fn, i.srcSlots[0]); auto okLay = abi.layout(srcLay.okType); loadValueFrom(b, fn, i.destSlot, 10, srcLay.payloadOffset, okLay); break; }
                                                          case PMIROpcode::OptionSome:case PMIROpcode::OptionNone: { auto lay = abi.layout(i.valueType); if (lay.kind == ABIRepresentationKind::OptionBoolNiche) { movImm(b, 0, i.op == PMIROpcode::OptionNone ? 2 : 0); if (i.op == PMIROpcode::OptionSome && !i.srcSlots.empty())loadSlot(b, 0, fn, i.srcSlots[0]); storeSlot(b, 0, fn, i.destSlot); break; }if (lay.kind == ABIRepresentationKind::OptionCharNiche) { if (i.op == PMIROpcode::OptionNone)movImm(b, 0, 0x00110000u); else loadSlot(b, 0, fn, i.srcSlots.at(0)); storeSlot(b, 0, fn, i.destSlot); break; }initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); if (lay.kind == ABIRepresentationKind::OptionPointerNiche) { if (i.op == PMIROpcode::OptionNone)zeroFixed(b, 11, lay.size); else { auto vl = abi.layout(lay.valueType); storeValueInto(b, fn, i.srcSlots.at(0), 11, 0, vl); }break; }movImm(b, 0, i.op == PMIROpcode::OptionSome ? 1 : 0); storeMem(b, 11, 0, 0, 1); if (i.op == PMIROpcode::OptionSome) { auto vl = abi.layout(lay.valueType); storeValueInto(b, fn, i.srcSlots.at(0), 11, lay.payloadOffset, vl); }break; }
                                                          case PMIROpcode::MakeClosure: { auto lay = abi.layout(i.valueType); initRichDest(b, fn, i.destSlot); loadSlot(b, 11, fn, i.destSlot); for (std::size_t n = 0; n < i.srcSlots.size() && n < lay.fields.size(); ++n) { auto fl = abi.layout(lay.fields[n].typeName); storeValueInto(b, fn, i.srcSlots[n], 11, lay.fields[n].offset, fl); }break; }
                                                          case PMIROpcode::Return: {
                                                              if (fn.richReturn) { if (i.srcSlots.empty())throw std::runtime_error("rich return missing source"); auto lay = abi.layout(fn.returnValueType); loadSlot(b, 10, fn, i.srcSlots[0]); loadRbp(b, 11, negDisp(fn.sretCellOffset)); copyFixed(b, 10, 11, lay.size); movReg(b, 0, 11); epilogue(b); }
                                                              else {
                                                                  if (i.srcSlots.empty())b.insert(b.end(), { 0x31,0xC0 }); else if (fpType(fn.returnValueType.kind))xmmSlotLoad(b, fn, i.srcSlots[0]);
                                                                  else loadSlot(b, 0, fn, i.srcSlots[0]); epilogue(b);
                                                              }break;
                                                          }
                                                          case PMIROpcode::Nop:b.push_back(0x90); break;
                                      }
                                  }

                                  void selectEntry(const PMIRProgram& program, const std::string& requested, MachineProgram& out)const {
                                      auto pick = [&](const std::string& name)->const PMIRFunction* {for (const auto& f : program.functions)if (f.name == name && f.parameterSlots.empty() && !f.richReturn)return&f; return nullptr; }; const PMIRFunction* fn = nullptr; if (!requested.empty())fn = pick(requested); else { for (const char* n : { "main","answer","start" })if ((fn = pick(n)))break; if (!fn)for (const auto& f : program.functions)if (f.parameterSlots.empty() && !f.richReturn) { fn = &f; break; } }if (!fn)throw std::runtime_error("native linker: no zero-argument scalar entry routine");
                                      if (fn->returnValueType.kind == IRValueKind::Float32 || fn->returnValueType.kind == IRValueKind::Float64)
                                          throw std::runtime_error("native executable entry must return an integer process status, not an IEEE-754 value");
                                      out.entryName = fn->name;
                                      out.entryOffset = out.functionOffsets.at(fn->name);
                                  }
                              };

                              std::string machineListingToString(const MachineProgram& program) { std::ostringstream oss; oss << "entry " << program.entryName << " +0x" << std::hex << program.entryOffset << std::dec << "\n"; for (const auto& line : program.listing) { oss << std::hex << std::setw(8) << std::setfill('0') << line.offset << "  "; for (auto x : line.bytes)oss << std::setw(2) << static_cast<unsigned>(x) << ' '; oss << std::dec << std::setfill(' ') << "  " << line.text << "\n"; }return oss.str(); }

                              // --- IR emitter -------------------------------------------------------------
                              class CppIREmitter {
                              public:
                                  std::string emit(const IRProgram& program) const {
                                      // Never emit plausible but semantically incorrect C++ for rich values.
                                      for (const auto& fn : program.functions) {
                                          auto scalar = [](const IRValueType& t) {
                                              return t.kind == IRValueKind::Void || t.kind == IRValueKind::Bool ||
                                                  t.kind == IRValueKind::Int64 || t.kind == IRValueKind::Null || t.kind == IRValueKind::Unknown;
                                              };
                                          if (!scalar(fn.valueType)) throw std::runtime_error("C++20 backend cannot lower return type " + fn.returnType);
                                          for (const auto& t : fn.parameterValueTypes) if (!scalar(t))throw std::runtime_error("C++20 backend cannot lower parameter type " + t.name);
                                          for (const auto& b : fn.blocks) {
                                              if (!b.phiNodes.empty())throw std::runtime_error("C++20 backend requires edge-accurate phi lowering");
                                              for (const auto& i : b.instructions) {
                                                  switch (i.op) {
                                                  case IROpcode::ConstFloat:case IROpcode::ConstText:case IROpcode::CallClosure:
                                                  case IROpcode::TextToString:case IROpcode::StringToText:case IROpcode::StringClear:case IROpcode::StringDispose:case IROpcode::StringMove:case IROpcode::ByteCount:case IROpcode::TextEquals:
                                                  case IROpcode::ResultOk:case IROpcode::ResultErr:case IROpcode::ResultIsErr:case IROpcode::ResultTry:
                                                  case IROpcode::OptionSome:case IROpcode::OptionNone:
                                                  case IROpcode::MakeAggregate:case IROpcode::ExtractField:case IROpcode::StoreField:case IROpcode::ArrayIndex:case IROpcode::ArrayStore:case IROpcode::MakeClosure:
                                                      throw std::runtime_error("C++20 backend cannot lower rich opcode " + std::to_string(static_cast<int>(i.op)));
                                                  case IROpcode::Call:
                                                      if (i.callee == "print" || i.callee == "println")throw std::runtime_error("C++20 backend cannot lower I/O builtin " + i.callee);
                                                      break;
                                                  default:break;
                                                  }
                                              }
                                          }
                                      }
                                      std::ostringstream oss;
                                      oss << "#include <cstdint>\n#include <cstdlib>\n#include <limits>\n\n";
                                      oss << "namespace paracine_runtime_detail_2026 {\n";
                                      oss << "static std::int64_t add(std::int64_t a, std::int64_t b) { return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)); }\n";
                                      oss << "static std::int64_t sub(std::int64_t a, std::int64_t b) { return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)); }\n";
                                      oss << "static std::int64_t mul(std::int64_t a, std::int64_t b) { return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); }\n";
                                      oss << "static std::int64_t neg(std::int64_t a) { return static_cast<std::int64_t>(0ull - static_cast<std::uint64_t>(a)); }\n";
                                      oss << "static std::int64_t div(std::int64_t a, std::int64_t b) { if (b == 0) std::abort(); if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return a; return a / b; }\n";
                                      oss << "static std::int64_t mod(std::int64_t a, std::int64_t b) { if (b == 0) std::abort(); if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return 0; return a % b; }\n";
                                      oss << "static std::int64_t shl(std::int64_t a, std::int64_t b) { return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << (static_cast<std::uint64_t>(b) & 63u)); }\n";
                                      oss << "static std::int64_t shr(std::int64_t a, std::int64_t b) { return a >> (static_cast<std::uint64_t>(b) & 63u); }\n";
                                      oss << "} // namespace paracine_runtime_detail_2026\n\n";

                                      for (const auto& decl : program.declarationSummaries) emitDeclarationSummary(oss, decl);
                                      if (!program.declarationSummaries.empty()) oss << "\n";
                                      for (const auto& function : program.functions) emitPrototype(oss, function);
                                      if (!program.functions.empty()) oss << "\n";
                                      for (const auto& function : program.functions) emitFunction(oss, function);

                                      const IRFunction* entry = nullptr;
                                      for (const auto& function : program.functions) if (function.name == "main" && function.parameterNames.empty()) { entry = &function; break; }
                                      if (!entry) throw std::runtime_error("C++20 backend requires a zero-argument main routine");
                                      oss << "int main() {";
                                      if (entry) {
                                          if (entry->returnType == "void") oss << " " << cppFunctionName(entry->name) << "(); return 0; ";
                                          else oss << " return static_cast<int>(" << cppFunctionName(entry->name) << "()); ";
                                      }
                                      else {
                                          oss << " return 0; ";
                                      }
                                      oss << "}\n";
                                      return oss.str();
                                  }

                              private:
                                  static std::string cppFunctionName(const std::string& name) {
                                      std::string out = "pcn_";
                                      for (char ch : name) out.push_back((std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') ? ch : '_');
                                      return out;
                                  }

                                  static std::string cppReturnType(const std::string& type) {
                                      if (type == "void") return "void";
                                      if (type == "bool") return "bool";
                                      // The bootstrap IR currently represents scalar numeric values in 64-bit virtual registers.
                                      return "std::int64_t";
                                  }

                                  void emitDeclarationSummary(std::ostringstream& oss, const DeclarationInfo& decl) const {
                                      oss << "// " << decl.kind << ": " << decl.name;
                                      if (!decl.metadata.empty()) oss << " // " << decl.metadata;
                                      oss << "\n";
                                  }

                                  void emitPrototype(std::ostringstream& oss, const IRFunction& function) const {
                                      oss << "static " << cppReturnType(function.returnType) << ' ' << cppFunctionName(function.name) << '(';
                                      for (std::size_t i = 0; i < function.parameterNames.size(); ++i) {
                                          if (i) oss << ", ";
                                          oss << "std::int64_t p" << i;
                                      }
                                      oss << ");\n";
                                  }

                                  void emitFunction(std::ostringstream& oss, const IRFunction& function) const {
                                      oss << "// " << function.signature << "\n";
                                      oss << "static " << cppReturnType(function.returnType) << ' ' << cppFunctionName(function.name) << '(';
                                      for (std::size_t i = 0; i < function.parameterNames.size(); ++i) {
                                          if (i) oss << ", ";
                                          oss << "std::int64_t p" << i;
                                      }
                                      oss << ") {\n";
                                      oss << "    /* PCIR bootstrap value type: " << irValueTypeName(function.valueType) << " */\n";
                                      for (std::uint32_t i = 1; i <= function.totalVregs; ++i) oss << "    std::int64_t v" << i << " = 0;\n";
                                      for (std::size_t i = 0; i < function.parameterVregs.size(); ++i) {
                                          oss << "    v" << reg(function.parameterVregs[i]) << " = p" << i << "; /* " << function.parameterSignature[i] << " */\n";
                                      }
                                      if (!function.blocks.empty()) oss << "    goto " << blockLabel(function.blocks.front().id) << ";\n";
                                      for (const auto& block : function.blocks) emitBlock(oss, block);
                                      if (function.returnType == "void") oss << "    return;\n";
                                      else oss << "    return 0; /* unreachable fallback for conservative C++ compilers */\n";
                                      oss << "}\n\n";
                                  }

                                  void emitBlock(std::ostringstream& oss, const IRBasicBlock& block) const {
                                      oss << blockLabel(block.id) << ": /* " << block.labelName << " */\n";
                                      if (!block.predecessors.empty()) {
                                          oss << "    /* predecessors: ";
                                          for (std::size_t i = 0; i < block.predecessors.size(); ++i) { if (i) oss << ", "; oss << block.predecessors[i]; }
                                          oss << " */\n";
                                      }
                                      for (const auto& phi : block.phiNodes) emitInstruction(oss, phi);
                                      for (const auto& inst : block.instructions) emitInstruction(oss, inst);
                                      if (block.instructions.empty()) oss << "    ;\n";
                                  }

                                  void emitInstruction(std::ostringstream& oss, const IRInstruction& inst) const {
                                      switch (inst.op) {
                                      case IROpcode::ConstFloat: throw std::runtime_error("C++20 backend does not support IEEE-754 PCIR yet");
                                      case IROpcode::ArrayIndex:case IROpcode::ArrayStore:case IROpcode::StoreField:throw std::runtime_error("C++20 backend cannot lower native aggregate mutation");
                                      case IROpcode::ConstInt: oss << "    v" << reg(inst.dest) << " = " << sanitizedIntegerLiteral(inst) << ";\n"; break;
                                      case IROpcode::ConstText: oss << "    /* text literal retained in PCIR metadata: " << escapedComment(inst.literalText) << " */\n"; break;
                                      case IROpcode::ConstBool: oss << "    v" << reg(inst.dest) << " = " << (inst.hasImmediate && inst.immediate ? 1 : 0) << ";\n"; break;
                                      case IROpcode::ConstNull: oss << "    v" << reg(inst.dest) << " = 0;\n"; break;
                                      case IROpcode::Copy: emitBinaryCopy(oss, inst); break;
                                      case IROpcode::Add: emitHelper2(oss, inst, "paracine_runtime_detail_2026::add"); break;
                                      case IROpcode::Sub: emitHelper2(oss, inst, "paracine_runtime_detail_2026::sub"); break;
                                      case IROpcode::Mul: emitHelper2(oss, inst, "paracine_runtime_detail_2026::mul"); break;
                                      case IROpcode::Div: emitHelper2(oss, inst, "paracine_runtime_detail_2026::div"); break;
                                      case IROpcode::Mod: emitHelper2(oss, inst, "paracine_runtime_detail_2026::mod"); break;
                                      case IROpcode::BitAnd: emitBinary(oss, inst, "&"); break;
                                      case IROpcode::BitOr: emitBinary(oss, inst, "|"); break;
                                      case IROpcode::BitXor: emitBinary(oss, inst, "^"); break;
                                      case IROpcode::Shl: emitHelper2(oss, inst, "paracine_runtime_detail_2026::shl"); break;
                                      case IROpcode::Shr: emitHelper2(oss, inst, "paracine_runtime_detail_2026::shr"); break;
                                      case IROpcode::Neg: oss << "    v" << reg(inst.dest) << " = paracine_runtime_detail_2026::neg(v" << reg(inst.args[0]) << ");\n"; break;
                                      case IROpcode::BitNot: oss << "    v" << reg(inst.dest) << " = ~v" << reg(inst.args[0]) << ";\n"; break;
                                      case IROpcode::Not: oss << "    v" << reg(inst.dest) << " = !v" << reg(inst.args[0]) << ";\n"; break;
                                      case IROpcode::Eq: emitBinary(oss, inst, "=="); break;
                                      case IROpcode::Ne: emitBinary(oss, inst, "!="); break;
                                      case IROpcode::Lt: emitBinary(oss, inst, "<"); break;
                                      case IROpcode::Le: emitBinary(oss, inst, "<="); break;
                                      case IROpcode::Gt: emitBinary(oss, inst, ">"); break;
                                      case IROpcode::Ge: emitBinary(oss, inst, ">="); break;
                                      case IROpcode::And: emitBinary(oss, inst, "&&"); break;
                                      case IROpcode::Or: emitBinary(oss, inst, "||"); break;
                                      case IROpcode::Phi:
                                          if (!inst.phiInputs.empty()) oss << "    v" << reg(inst.dest) << " = v" << reg(inst.phiInputs.front().second) << "; /* conservative phi */\n";
                                          break;
                                      case IROpcode::Branch: if (!inst.targetBlocks.empty()) oss << "    goto " << blockLabel(inst.targetBlocks.front()) << ";\n"; break;
                                      case IROpcode::BranchIf:
                                          if (inst.args.size() && inst.targetBlocks.size() >= 2)
                                              oss << "    if (v" << reg(inst.args[0]) << ") goto " << blockLabel(inst.targetBlocks[0]) << "; else goto " << blockLabel(inst.targetBlocks[1]) << ";\n";
                                          break;
                                      case IROpcode::Call: emitCall(oss, inst); break;
                                      case IROpcode::CallClosure:
                                          oss << "    /* indirect closure call requires closure environment legalization */\n"; break;
                                      case IROpcode::TextToString:
                                          oss << "    /* text.to_string: allocates owned UTF-8 storage; rich portable runtime legalization required */\n"; break;
                                      case IROpcode::StringToText:
                                          oss << "    /* string.to_text: zero-copy immutable borrow; rich portable runtime legalization required */\n"; break;
                                      case IROpcode::TextEquals:throw std::runtime_error("C++20 exporter lacks text equality legalization");
                                      case IROpcode::ByteCount:throw std::runtime_error("C++20 exporter lacks rich byte-count legalization");
                                      case IROpcode::StringDispose:case IROpcode::StringMove:
                                          throw std::runtime_error("C++20 exporter lacks string disposal");
                                      case IROpcode::StringClear:
                                          oss << "    /* string.clear: requires exclusive/owned string authority; rich portable runtime legalization required */\n"; break;
                                      case IROpcode::ResultOk: case IROpcode::ResultErr: case IROpcode::ResultIsErr: case IROpcode::ResultTry: case IROpcode::OptionSome: case IROpcode::OptionNone:
                                      case IROpcode::MakeAggregate: case IROpcode::ExtractField: case IROpcode::MakeClosure:
                                          oss << "    /* semantic PCIR op " << static_cast<int>(inst.op) << " requires portable aggregate runtime legalization */\n"; break;
                                      case IROpcode::Return:
                                          if (!inst.args.empty()) oss << "    return v" << reg(inst.args[0]) << ";\n";
                                          else oss << "    return;\n";
                                          break;
                                      case IROpcode::Nop: oss << "    /* nop */\n"; break;
                                      }
                                  }

                                  void emitBinaryCopy(std::ostringstream& oss, const IRInstruction& inst) const {
                                      if (!inst.args.empty()) oss << "    v" << reg(inst.dest) << " = v" << reg(inst.args[0]) << ";\n";
                                  }
                                  void emitBinary(std::ostringstream& oss, const IRInstruction& inst, const char* op) const {
                                      oss << "    v" << reg(inst.dest) << " = v" << reg(inst.args[0]) << ' ' << op << " v" << reg(inst.args[1]) << ";\n";
                                  }
                                  void emitHelper2(std::ostringstream& oss, const IRInstruction& inst, const char* helper) const {
                                      oss << "    v" << reg(inst.dest) << " = " << helper << "(v" << reg(inst.args[0]) << ", v" << reg(inst.args[1]) << ");\n";
                                  }
                                  void emitCall(std::ostringstream& oss, const IRInstruction& inst) const {
                                      const bool isPrint = inst.callee == "print";
                                      if (isPrint) { oss << "    /* builtin print is runtime-bound in native Paracine; portability shim omitted */\n"; return; }
                                      if (inst.returnType != "void") oss << "    v" << reg(inst.dest) << " = "; else oss << "    ";
                                      oss << cppFunctionName(inst.callee) << '(';
                                      for (std::size_t i = 0; i < inst.args.size(); ++i) { if (i) oss << ", "; oss << "v" << reg(inst.args[i]); }
                                      oss << ");\n";
                                  }

                                  static std::uint32_t reg(VReg value) { return static_cast<std::uint32_t>(value); }
                                  static std::string blockLabel(BlockID block) { return "bb_" + std::to_string(static_cast<std::uint32_t>(block)); }
                                  static std::string sanitizedIntegerLiteral(const IRInstruction& inst) {
                                      std::string out = inst.literalText.empty() ? std::to_string(inst.immediate) : inst.literalText;
                                      out.erase(std::remove(out.begin(), out.end(), '_'), out.end());
                                      return out;
                                  }
                                  static std::string escapedComment(const std::string& text) {
                                      std::string out;
                                      for (char ch : text) out.push_back(ch == '\n' || ch == '\r' ? ' ' : ch);
                                      return out;
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
                                  std::unordered_map<std::string, std::uint32_t> functionOffsets;
                                  std::unordered_map<std::string, std::uint32_t> functionSizes;
                                  std::unordered_map<std::string, std::uint32_t> functionFrameSizes;
                                  std::unordered_map<std::string, std::uint32_t> unwindOffsets;
                                  std::vector<MachineProgram::ImportCall> importCalls;
                                  std::uint32_t entryPointOffset = 0;
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
                                  // Section-relative layout: 40-byte descriptor table, then ILT/IAT,
                                  // IMAGE_IMPORT_BY_NAME strings, and the DLL name. All RVA fixups are
                                  // resolved only after section layout, keeping PE layout deterministic.
                                  struct ImportLayout {
                                      std::vector<std::string> names;
                                      std::uint32_t ilt = 40, iat = 0, dll = 0;
                                      std::map<std::string, std::uint32_t> hintNames;
                                      std::uint32_t total = 0;
                                  };
                                  static ImportLayout describeImports(const std::vector<MachineProgram::ImportCall>& calls) {
                                      ImportLayout imp;
                                      const std::vector<std::string> canonical = { "GetProcessHeap","HeapAlloc","HeapFree" };
                                      std::set<std::string> used;
                                      for (const auto& c : calls) { if (std::find(canonical.begin(), canonical.end(), c.symbol) == canonical.end())throw std::runtime_error("unsupported Win32 import: " + c.symbol); used.insert(c.symbol); }
                                      for (const auto& c : canonical)if (used.contains(c))imp.names.push_back(c);
                                      imp.iat = imp.ilt + static_cast<std::uint32_t>((imp.names.size() + 1) * 8);
                                      std::uint32_t at = imp.iat + static_cast<std::uint32_t>((imp.names.size() + 1) * 8);
                                      for (const auto& name : imp.names) { if (at & 1)++at; imp.hintNames[name] = at; at += static_cast<std::uint32_t>(2 + name.size() + 1); }
                                      imp.dll = at;
                                      imp.total = at + static_cast<std::uint32_t>(sizeof("KERNEL32.dll"));
                                      return imp;
                                  }
                                  static std::vector<std::uint8_t> importSection(const ImportLayout& imp, std::uint32_t baseRva) {
                                      ByteBuffer out;
                                      if (imp.names.empty())return {};
                                      out.writeU32(baseRva + imp.ilt); out.writeU32(0); out.writeU32(0); out.writeU32(baseRva + imp.dll); out.writeU32(baseRva + imp.iat);
                                      for (int j = 0; j < 20; ++j)out.writeU8(0);
                                      for (const auto& name : imp.names)out.writeU64(baseRva + imp.hintNames.at(name)); out.writeU64(0);
                                      for (const auto& name : imp.names)out.writeU64(baseRva + imp.hintNames.at(name)); out.writeU64(0);
                                      for (const auto& name : imp.names) { out.align(2); out.writeU16(0); out.writeStringZ(name); }
                                      if (out.size() != imp.dll)throw std::runtime_error("internal PE import layout mismatch");
                                      out.writeStringZ("KERNEL32.dll");
                                      if (out.size() != imp.total)throw std::runtime_error("internal PE import size mismatch");
                                      return out.release();
                                  }
                              public:
                                  std::vector<std::uint8_t> emitObject(const IRProgram& program, const std::string& entry = {}) const {
                                      const auto image = buildImage(program, false, entry);
                                      return emitCOFFObject(image);
                                  }

                                  std::vector<std::uint8_t> emitExecutable(const IRProgram& program, const std::string& entry = {}) const {
                                      const auto image = buildImage(program, true, entry);
                                      return emitPEImage(image);
                                  }

                              private:
                                  PECOFFImage buildImage(const IRProgram& program, bool includeUnwind, const std::string& entry) const {
                                      PECOFFImage image;

                                      PMIRLowerer pmirLowerer;
                                      auto pmir = pmirLowerer.lower(program);
                                      X64Encoder encoder;
                                      auto machine = encoder.encode(pmir, entry);

                                      PECOFFSection text;
                                      text.name = ".text";
                                      text.characteristics = 0x60000020u;
                                      text.data = machine.text;
                                      text.virtualSize = static_cast<std::uint32_t>(text.data.size());
                                      image.importCalls = machine.imports;
                                      image.functionOffsets = machine.functionOffsets;
                                      image.functionSizes = machine.functionSizes;
                                      image.entryPointOffset = machine.entryOffset;
                                      for (const auto& f : pmir.functions) image.functionFrameSizes[f.name] = f.frameSize;

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
                                      if (includeUnwind) {
                                          PECOFFSection xdata; xdata.name = ".xdata"; xdata.characteristics = 0x40000040u;
                                          for (const auto& f : pmir.functions) {
                                              while (xdata.data.size() % 4) xdata.data.push_back(0);
                                              image.unwindOffsets[f.name] = static_cast<std::uint32_t>(xdata.data.size());
                                              auto emit8 = [&](std::uint8_t v) {xdata.data.push_back(v); };
                                              auto emit16 = [&](std::uint16_t v) {emit8(static_cast<std::uint8_t>(v)); emit8(static_cast<std::uint8_t>(v >> 8)); };
                                              const std::uint32_t frame = f.frameSize; const bool large = frame > 128;
                                              emit8(0x01); emit8(11); emit8(static_cast<std::uint8_t>(large ? 4 : 3)); emit8(0x05);
                                              emit8(11);
                                              if (!large) emit8(static_cast<std::uint8_t>((((frame - 8) / 8) & 0x0F) << 4 | 0x02));
                                              else { emit8(0x01); emit16(static_cast<std::uint16_t>(frame / 8)); }
                                              emit8(4); emit8(0x03); emit8(1); emit8(0x50);
                                              while (xdata.data.size() % 4) xdata.data.push_back(0);
                                          }
                                          xdata.virtualSize = static_cast<std::uint32_t>(xdata.data.size());
                                          PECOFFSection pdata; pdata.name = ".pdata"; pdata.characteristics = 0x40000040u; pdata.data.resize(pmir.functions.size() * 12u); pdata.virtualSize = static_cast<std::uint32_t>(pdata.data.size());
                                          image.sections.push_back(std::move(xdata)); image.sections.push_back(std::move(pdata));
                                      }
                                      image.sections.push_back(std::move(rdata));
                                      image.sections.push_back(std::move(data));
                                      if (!decl.data.empty())image.sections.push_back(std::move(decl));
                                      if (includeUnwind && !machine.imports.empty()) {
                                          PECOFFSection idata; idata.name = ".idata"; idata.characteristics = 0xC0000040u;
                                          idata.data.resize(describeImports(machine.imports).total, 0);
                                          idata.virtualSize = static_cast<std::uint32_t>(idata.data.size());
                                          image.sections.push_back(std::move(idata));
                                      }
                                      image.entrySectionIndex = 1;
                                      image.entryPointRva = image.sectionAlignment;

                                      for (const auto& function : program.functions) {
                                          image.functionNames.push_back(function.name);
                                          image.functionSignatures.push_back(function.signature);
                                          image.typeNames.push_back(function.returnType);
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
                                      buffer.writeU16(static_cast<std::uint16_t>(machine));
                                      buffer.writeU16(sectionCount);
                                      buffer.writeU32(0); // deterministic TimeDateStamp
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

                                      // COFF AMD64_REL32 relocations against undefined __imp_ symbols.
                                      // Section symbols are listed first, then local functions, then imports.
                                      if (!image.importCalls.empty()) {
                                          if (image.importCalls.size() > 65535u)throw std::runtime_error("too many COFF relocations");
                                          buffer.align(4);
                                          const std::uint32_t relocOffset = static_cast<std::uint32_t>(buffer.size());
                                          std::vector<std::string> importNames;
                                          for (const auto& c : image.importCalls) if (std::find(importNames.begin(), importNames.end(), c.symbol) == importNames.end())importNames.push_back(c.symbol);
                                          for (const auto& c : image.importCalls) {
                                              const auto it = std::find(importNames.begin(), importNames.end(), c.symbol);
                                              buffer.writeU32(static_cast<std::uint32_t>(c.dispPos));
                                              buffer.writeU32(static_cast<std::uint32_t>(image.sections.size() + image.functionNames.size() + std::distance(importNames.begin(), it)));
                                              buffer.writeU16(0x0004); // IMAGE_REL_AMD64_REL32
                                          }
                                          // IMAGE_SECTION_HEADER: PointerToRelocations + NumberOfRelocations.
                                          buffer.patchU32(sectionInfo[0].headerOffset + 24, relocOffset);
                                          buffer.patchU16(sectionInfo[0].headerOffset + 32, static_cast<std::uint16_t>(image.importCalls.size()));
                                      }
                                      const std::size_t symbolTableOffset = buffer.size();
                                      std::vector<std::pair<std::string, std::uint32_t>> stringTableEntries;
                                      std::size_t symbolCount = 0;
                                      for (std::size_t i = 0; i < image.sections.size(); ++i) {
                                          writeSymbol(buffer, image.sections[i].name, 0, static_cast<std::int16_t>(i + 1), 3, 0, stringTableEntries);
                                          ++symbolCount;
                                      }
                                      for (const auto& name : image.functionNames) {
                                          auto it = image.functionOffsets.find(name);
                                          const std::uint32_t value = it == image.functionOffsets.end() ? 0u : it->second;
                                          writeSymbol(buffer, name, value, 1, 2, 0, stringTableEntries);
                                          ++symbolCount;
                                      }

                                      std::vector<std::string> importNames;
                                      for (const auto& c : image.importCalls) if (std::find(importNames.begin(), importNames.end(), c.symbol) == importNames.end())importNames.push_back(c.symbol);
                                      for (const auto& name : importNames) {
                                          writeSymbol(buffer, "__imp_" + name, 0, 0, 2, 0, stringTableEntries);
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
                                          image.entryPointRva = image.sections.front().virtualAddress + image.entryPointOffset;
                                      }
                                      PECOFFSection* pdataSection = nullptr; PECOFFSection* xdataSection = nullptr;
                                      for (auto& sec : image.sections) { if (sec.name == ".pdata")pdataSection = &sec; else if (sec.name == ".xdata")xdataSection = &sec; }
                                      if (pdataSection && xdataSection) {
                                          ByteBuffer pd;
                                          for (const auto& name : image.functionNames) {
                                              auto off = image.functionOffsets.at(name), size = image.functionSizes.at(name), uw = image.unwindOffsets.at(name);
                                              pd.writeU32(image.sections.front().virtualAddress + off); pd.writeU32(image.sections.front().virtualAddress + off + size); pd.writeU32(xdataSection->virtualAddress + uw);
                                          }
                                          pdataSection->data = pd.release(); pdataSection->virtualSize = static_cast<std::uint32_t>(pdataSection->data.size());
                                      }

                                      if (!image.importCalls.empty()) {
                                          auto idata = std::find_if(image.sections.begin(), image.sections.end(), [](const PECOFFSection& s) {return s.name == ".idata"; });
                                          if (idata == image.sections.end())throw std::runtime_error("native imports require .idata");
                                          const ImportLayout imp = describeImports(image.importCalls);
                                          idata->data = importSection(imp, idata->virtualAddress);
                                          std::map<std::string, std::size_t> indices;
                                          for (std::size_t j = 0; j < imp.names.size(); ++j)indices[imp.names[j]] = j;
                                          auto& code = image.sections.front().data;
                                          for (const auto& fix : image.importCalls) {
                                              auto index = indices.find(fix.symbol);
                                              if (index == indices.end())throw std::runtime_error("unresolved imported routine " + fix.symbol);
                                              std::int64_t dest = static_cast<std::int64_t>(idata->virtualAddress + imp.iat + index->second * 8u);
                                              std::int64_t src = static_cast<std::int64_t>(image.sections.front().virtualAddress + fix.dispPos + 4u);
                                              auto rel = dest - src;
                                              if (rel<std::numeric_limits<std::int32_t>::min() || rel>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("import IAT displacement exceeds 32-bit range");
                                              if (fix.dispPos + 4 > code.size())throw std::runtime_error("import call relocation outside .text");
                                              for (int k = 0; k < 4; ++k)code[fix.dispPos + k] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(rel) >> (8 * k)) & 255);
                                          }
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
                                      }
                                      else {
                                          buffer.writeU32(0);
                                          std::uint32_t offset = 4;
                                          for (const auto& entry : stringTableEntries) {
                                              offset += static_cast<std::uint32_t>(entry.first.size() + 1);
                                          }
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
                                      buffer.writeU16(0x8664); // IMAGE_FILE_MACHINE_AMD64
                                      buffer.writeU16(sectionCount);
                                      buffer.writeU32(0);      // reproducible TimeDateStamp
                                      buffer.writeU32(0);      // PointerToSymbolTable
                                      buffer.writeU32(0);      // NumberOfSymbols
                                      buffer.writeU16(optionalHeaderSize);
                                      buffer.writeU16(0x0022); // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
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
                                      // Do not advertise ASLR/high-entropy VA while this linker still lacks
                                      // an image base-relocation directory. Keep NX compatibility and
                                      // terminal-server awareness; ASLR is a release-blocking linker task.
                                      buffer.writeU16(0x8100);
                                      buffer.writeU64(0x100000);
                                      buffer.writeU64(0x1000);
                                      buffer.writeU64(0x100000);
                                      buffer.writeU64(0x1000);
                                      buffer.writeU32(0);
                                      buffer.writeU32(16);
                                      std::uint32_t exceptionRva = 0, exceptionSize = 0, importRva = 0, importSize = 0, iatRva = 0, iatSize = 0;
                                      for (const auto& sec : image.sections) {
                                          if (sec.name == ".pdata") { exceptionRva = sec.virtualAddress; exceptionSize = sec.virtualSize; }
                                          if (sec.name == ".idata") {
                                              importRva = sec.virtualAddress; importSize = 40;
                                              const auto imp = describeImports(image.importCalls);
                                              iatRva = sec.virtualAddress + imp.iat; iatSize = static_cast<std::uint32_t>((imp.names.size() + 1) * 8);
                                          }
                                      }
                                      for (int i = 0; i < 16; ++i) {
                                          if (i == 1) { buffer.writeU32(importRva); buffer.writeU32(importSize); }
                                          else if (i == 3) { buffer.writeU32(exceptionRva); buffer.writeU32(exceptionSize); }
                                          else if (i == 12) { buffer.writeU32(iatRva); buffer.writeU32(iatSize); }
                                          else { buffer.writeU32(0); buffer.writeU32(0); }
                                      }
                                  }
                              };

                              std::string opcodeName(IROpcode op) {
                                  switch (op) {
                                  case IROpcode::ConstInt: return "const.int"; case IROpcode::ConstFloat:return "const.float"; case IROpcode::ConstText: return "const.text";
                                  case IROpcode::ConstBool: return "const.bool"; case IROpcode::ConstNull: return "const.null";
                                  case IROpcode::Copy: return "copy"; case IROpcode::Add: return "add"; case IROpcode::Sub: return "sub";
                                  case IROpcode::Mul: return "mul"; case IROpcode::Div: return "div"; case IROpcode::Mod: return "mod";
                                  case IROpcode::BitAnd: return "bit.and"; case IROpcode::BitOr: return "bit.or"; case IROpcode::BitXor: return "bit.xor";
                                  case IROpcode::Shl: return "shl"; case IROpcode::Shr: return "shr"; case IROpcode::Neg: return "neg";
                                  case IROpcode::BitNot: return "bit.not"; case IROpcode::Not: return "not"; case IROpcode::Eq: return "eq";
                                  case IROpcode::Ne: return "ne"; case IROpcode::Lt: return "lt"; case IROpcode::Le: return "le";
                                  case IROpcode::Gt: return "gt"; case IROpcode::Ge: return "ge"; case IROpcode::And: return "and";
                                  case IROpcode::Or: return "or"; case IROpcode::Phi: return "phi"; case IROpcode::Branch: return "br";
                                  case IROpcode::BranchIf: return "br.if"; case IROpcode::Call: return "call"; case IROpcode::CallClosure: return "call.closure";
                                  case IROpcode::TextToString: return "text.to_string"; case IROpcode::StringToText: return "string.to_text"; case IROpcode::StringClear: return "string.clear"; case IROpcode::StringDispose: return "string.dispose"; case IROpcode::StringMove:return "string.move"; case IROpcode::ByteCount: return "text.count"; case IROpcode::TextEquals: return "text.equals";
                                  case IROpcode::MakeAggregate: return "aggregate.make"; case IROpcode::ExtractField: return "aggregate.field"; case IROpcode::ArrayIndex:return "array.index"; case IROpcode::StoreField:return "aggregate.store.field"; case IROpcode::ArrayStore:return "array.store";
                                  case IROpcode::ResultOk: return "result.ok"; case IROpcode::ResultErr: return "result.err"; case IROpcode::ResultIsErr: return "result.is_err"; case IROpcode::ResultTry: return "result.try";
                                  case IROpcode::OptionSome: return "option.some"; case IROpcode::OptionNone: return "option.none"; case IROpcode::MakeClosure: return "closure.make";
                                  case IROpcode::Return: return "ret"; case IROpcode::Nop: return "nop";
                                  }
                                  return "?";
                              }

                              std::string irToString(const IRProgram& program) {
                                  std::ostringstream oss;
                                  for (const auto& fn : program.functions) {
                                      oss << "pcir routine " << fn.signature << "\n";
                                      for (const auto& block : fn.blocks) {
                                          oss << block.labelName << ":\n";
                                          auto emitInst = [&](const IRInstruction& inst) {
                                              oss << "    ";
                                              if (inst.dest != VReg::None) oss << 'v' << static_cast<std::uint32_t>(inst.dest) << " = ";
                                              oss << opcodeName(inst.op);
                                              if (!inst.callee.empty()) oss << ' ' << inst.callee;
                                              if (!inst.literalText.empty()) oss << ' ' << inst.literalText;
                                              for (auto arg : inst.args) oss << " v" << static_cast<std::uint32_t>(arg);
                                              for (auto target : inst.targetBlocks) oss << " bb_" << static_cast<std::uint32_t>(target);
                                              oss << " : " << irValueTypeName(inst.valueType) << "\n";
                                              };
                                          for (const auto& phi : block.phiNodes) emitInst(phi);
                                          for (const auto& inst : block.instructions) emitInst(inst);
                                      }
                                      oss << "\n";
                                  }
                                  return oss.str();
                              }

                              static const char* borrowKindName(BorrowKind k) { return k == BorrowKind::Owned ? "own" : k == BorrowKind::Exclusive ? "exclusive" : "shared"; }

                              std::string skmToString(const SemanticAnalysisResult& analysis) {
                                  std::ostringstream oss;
                                  oss << "SAGE Semantic Knowledge Model\n";
                                  oss << "symbols: " << analysis.skm.symbols.size() << "\n";
                                  for (const auto& s : analysis.skm.symbols) oss << "  #" << s.id << " " << s.routine << "::" << s.name << " : " << s.typeName << " [" << (s.isMutable ? "var" : "let") << (s.isParameter ? ", param " + s.parameterMode : "") << "] points " << s.firstPoint << ".." << s.lastPoint << "\n";
                                  oss << "borrow regions: " << analysis.skm.borrowRegions.size() << "\n";
                                  for (const auto& b : analysis.skm.borrowRegions) oss << "  r" << b.id << " " << borrowKindName(b.kind) << " " << b.sourceName << " (#" << b.sourceSymbol << ") " << b.beginPoint << ".." << b.endPoint << (b.closureId ? " closure$" + std::to_string(b.closureId) : "") << (b.escaping ? " ESCAPES" : "") << "\n";
                                  oss << "closures: " << analysis.skm.closures.size() << "\n";
                                  for (const auto& c : analysis.skm.closures) { oss << "  closure$" << c.closureId << " ("; for (std::size_t i = 0; i < c.parameterTypes.size(); ++i) { if (i)oss << ", "; oss << c.parameterTypes[i]; }oss << ") -> " << c.returnType << " effects ["; if (c.effects.empty())oss << "pure"; else { bool first = true; for (const auto& e : c.effects) { if (!first)oss << ','; first = false; oss << e; } }oss << "]" << (c.moveCapture ? " move" : "") << "\n"; }
                                  oss << "closure captures: " << analysis.skm.closureCaptures.size() << "\n";
                                  for (const auto& c : analysis.skm.closureCaptures) oss << "  closure$" << c.closureId << " captures " << borrowKindName(c.kind) << " " << c.name << " : " << c.typeName << (c.escaping ? " [escaping]" : "") << "\n";
                                  oss << "generic specializations: " << analysis.skm.specializations.size() << "\n";
                                  for (const auto& g : analysis.skm.specializations) { oss << "  " << g.genericRoutine << " -> " << g.specializationName << " {"; bool first = true; for (const auto& [k, v] : g.substitutions) { if (!first)oss << ", "; first = false; oss << k << '=' << v; } oss << "}\n"; }
                                  oss << "capability resolutions: " << analysis.skm.capabilityResolutions.size() << "\n";
                                  for (const auto& c : analysis.skm.capabilityResolutions) { oss << "  " << c.subjectType << " is " << c.capability << ": " << (c.satisfied ? "yes" : "no"); if (!c.evidence.empty()) { oss << " via "; for (std::size_t i = 0; i < c.evidence.size(); ++i) { if (i)oss << ", "; oss << c.evidence[i]; } } oss << "\n"; }
                                  oss << "effect edges: " << analysis.skm.effectEdges.size() << "\n";
                                  for (const auto& e : analysis.skm.effectEdges) { oss << "  " << e.caller << " -> " << e.callee << " ["; if (e.effects.empty())oss << "pure"; else { bool first = true; for (const auto& x : e.effects) { if (!first)oss << ','; first = false; oss << x; } } oss << "]\n"; }
                                  oss << "layouts: " << analysis.skm.layouts.size() << "\n";
                                  for (const auto& l : analysis.skm.layouts) { oss << "  " << l.typeName << " size=" << l.size << " align=" << l.alignment << (l.nicheOptimized ? " niche" : "") << "\n"; for (const auto& f : l.fieldOffsets)oss << "    +" << f.second << " " << f.first << "\n"; }
                                  oss << "structural facts: " << analysis.skm.structuralFacts.size() << "\n";
                                  for (const auto& f : analysis.skm.structuralFacts) oss << "  " << f << "\n";
                                  return oss.str();
                              }

                              // --- Compiler pipeline + command interface ----------------------------------
                              struct CompilerOptions {
                                  int optimizationLevel = 2;
                                  std::string entry;
                              };

                              struct CompilationArtifacts {
                                  Program program;
                                  SemanticAnalysisResult analysis;
                                  IRProgram pcir;
                                  PMIRProgram pmir;
                                  MachineProgram machine;
                              };

                              static std::string readSourceFile(const std::filesystem::path& path) {
                                  std::ifstream input(path, std::ios::binary);
                                  if (!input) throw std::runtime_error("failed to read file: " + path.string());
                                  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
                              }

                              static void writeTextFile(const std::filesystem::path& path, const std::string& text) {
                                  std::ofstream out(path, std::ios::binary);
                                  if (!out) throw std::runtime_error("failed to write file: " + path.string());
                                  out.write(text.data(), static_cast<std::streamsize>(text.size()));
                              }

                              static void writeBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
                                  std::ofstream out(path, std::ios::binary);
                                  if (!out) throw std::runtime_error("failed to write file: " + path.string());
                                  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                              }

                              static CompilationArtifacts compileAll(const std::string& source, const CompilerOptions& options, bool needMachine = true,
                                  std::vector<std::pair<std::string, double>>* phaseTimings = nullptr) {
                                  auto phaseStart = std::chrono::steady_clock::now();
                                  auto markPhase = [&](const char* name) {
                                      if (phaseTimings) {
                                          const auto now = std::chrono::steady_clock::now();
                                          phaseTimings->emplace_back(name, std::chrono::duration<double, std::milli>(now - phaseStart).count());
                                          phaseStart = now;
                                      }
                                      };
                                  CompilationArtifacts out;
                                  Lexer lexer(source);
                                  auto tokens = lexer.tokenize();
                                  markPhase("lex");
                                  Parser parser(std::move(tokens));
                                  out.program = parser.parse();
                                  markPhase("parse");
                                  TopLevelConstantResolver{}.resolveProgram(out.program);
                                  markPhase("constants");

                                  SemanticAnalyzer semantic;
                                  out.analysis = semantic.analyze(out.program);
                                  SAGEEngine sage;
                                  sage.analyze(out.program, out.analysis);
                                  markPhase("SAGE/SKM");
                                  if (!out.analysis.succeeded()) return out;

                                  SemanticSpecializer specializer;
                                  Program specializedProgram = specializer.specialize(out.program, out.analysis);
                                  markPhase("specialize");
                                  out.pcir = lowerProgramToIR(specializedProgram, &out.analysis);
                                  markPhase("PCIR lower");
                                  PCIRVerifier verifier;
                                  auto before = verifier.verify(out.pcir);
                                  if (!before.empty()) {
                                      std::ostringstream oss; oss << "internal compiler error: invalid PCIR before optimization";
                                      for (const auto& e : before) oss << "\n  " << e;
                                      throw std::runtime_error(oss.str());
                                  }
                                  markPhase("PCIR verify pre");
                                  PCIROptimizer optimizer(options.optimizationLevel);
                                  optimizer.optimize(out.pcir);
                                  markPhase("optimize");
                                  auto after = verifier.verify(out.pcir);
                                  if (!after.empty()) {
                                      std::ostringstream oss; oss << "internal compiler error: optimizer produced invalid PCIR";
                                      for (const auto& e : after) oss << "\n  " << e;
                                      throw std::runtime_error(oss.str());
                                  }

                                  markPhase("PCIR verify post");
                                  if (needMachine) {
                                      RichABIVerifier abiVerifier;
                                      auto abiErrors = abiVerifier.verify(out.pcir);
                                      if (!abiErrors.empty()) { std::ostringstream oss; oss << "internal compiler error: invalid rich-value target ABI"; for (const auto& e : abiErrors) oss << "\n  " << e; throw std::runtime_error(oss.str()); }
                                      markPhase("rich ABI");
                                      PMIRLowerer pmirLowerer;
                                      out.pmir = pmirLowerer.lower(out.pcir);
                                      markPhase("PMIR lower");
                                      PMIRVerifier pmirVerifier;
                                      auto pmirErrors = pmirVerifier.verify(out.pmir);
                                      if (!pmirErrors.empty()) { std::ostringstream oss; oss << "internal compiler error: invalid PMIR"; for (const auto& e : pmirErrors) oss << "\n  " << e; throw std::runtime_error(oss.str()); }
                                      markPhase("PMIR verify");
                                      X64Encoder encoder;
                                      out.machine = encoder.encode(out.pmir, options.entry);
                                      markPhase("x64 encode");
                                      MachineVerifier machineVerifier;
                                      auto machineErrors = machineVerifier.verify(out.machine);
                                      if (!machineErrors.empty()) { std::ostringstream oss; oss << "internal compiler error: machine verification failed"; for (const auto& e : machineErrors) oss << "\n  " << e; throw std::runtime_error(oss.str()); }
                                      markPhase("machine verify");
                                  }
                                  return out;
                              }

                              static std::string effectSetText(const std::set<std::string>& effects) {
                                  if (effects.empty()) return "pure";
                                  std::ostringstream oss; bool first = true;
                                  for (const auto& e : effects) { if (!first) oss << ','; first = false; oss << e; }
                                  return oss.str();
                              }

                              [[maybe_unused]] static int runNativeExecutable(const std::filesystem::path& exe) {
#ifdef _WIN32
                                  std::string command = "\"" + std::filesystem::absolute(exe).string() + "\"";
                                  std::vector<char> mutableCommand(command.begin(), command.end()); mutableCommand.push_back('\0');
                                  STARTUPINFOA si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
                                  if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                                      throw std::runtime_error("failed to launch native executable (Win32 error " + std::to_string(GetLastError()) + ")");
                                  }
                                  const DWORD waitState = WaitForSingleObject(pi.hProcess, 30000);
                                  if (waitState != WAIT_OBJECT_0) {
                                      if (waitState == WAIT_TIMEOUT) (void)TerminateProcess(pi.hProcess, 124);
                                      CloseHandle(pi.hThread);
                                      CloseHandle(pi.hProcess);
                                      throw std::runtime_error(waitState == WAIT_TIMEOUT
                                          ? "native executable exceeded 30-second execution deadline"
                                          : "failed while waiting for native executable");
                                  }
                                  DWORD exitCode = 0;
                                  const BOOL codeOK = GetExitCodeProcess(pi.hProcess, &exitCode);
                                  CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
                                  if (!codeOK) throw std::runtime_error("GetExitCodeProcess failed");
                                  return static_cast<int>(exitCode);
#else
                                  (void)exe;
                                  return -1;
#endif
                              }

                              // Linux/x86-64 differential execution of Windows-x64 machine code.
                              // The import slots are resolved to ABI-compatible host shims: this tests the
                              // generated Windows x64 calls but is not an emulation of the Windows loader.
#if defined(__linux__) && defined(__x86_64__)
                              static std::unordered_set<void*> pcn_test_live_heap;
                              static std::size_t pcn_test_heap_allocs = 0, pcn_test_heap_frees = 0;
                              extern "C" __attribute__((ms_abi)) void* pcn_test_GetProcessHeap() { return reinterpret_cast<void*>(1); }
                              extern "C" __attribute__((ms_abi)) void* pcn_test_HeapAlloc(void*, std::uint32_t flags, std::size_t count) {
                                  void* ptr = (flags & 8u) ? std::calloc(1, std::max<std::size_t>(count, 1)) : std::malloc(std::max<std::size_t>(count, 1));
                                  if (ptr) { pcn_test_live_heap.insert(ptr); ++pcn_test_heap_allocs; }
                                  return ptr;
                              }
                              extern "C" __attribute__((ms_abi)) int pcn_test_HeapFree(void*, std::uint32_t, void* pointer) {
                                  if (!pointer || pcn_test_live_heap.erase(pointer) != 1)return 0;
                                  std::free(pointer); ++pcn_test_heap_frees; return 1;
                              }
                              static void* hostImport(const std::string& name) {
                                  if (name == "GetProcessHeap")return reinterpret_cast<void*>(&pcn_test_GetProcessHeap);
                                  if (name == "HeapAlloc")return reinterpret_cast<void*>(&pcn_test_HeapAlloc);
                                  if (name == "HeapFree")return reinterpret_cast<void*>(&pcn_test_HeapFree);
                                  throw std::runtime_error("no host-side Win64 ABI shim for " + name);
                              }
                              static std::int64_t executeEmittedMachineText(const MachineProgram& machine) {
                                  if (machine.text.empty() || machine.entryOffset >= machine.text.size())
                                      throw std::runtime_error("cannot execute empty or invalid machine entry");
                                  const long rawPage = sysconf(_SC_PAGESIZE);
                                  if (rawPage <= 0) throw std::runtime_error("cannot determine memory page size");
                                  const std::size_t page = static_cast<std::size_t>(rawPage);
                                  const std::size_t importBase = (machine.text.size() + 7u) & ~std::size_t{ 7 };
                                  const std::size_t used = importBase + machine.imports.size() * sizeof(void*);
                                  const std::size_t length = ((used + page - 1) / page) * page;
                                  void* region = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                                  if (region == MAP_FAILED) throw std::runtime_error("machine-text mapping failed");
                                  struct MappingGuard {
                                      void* base; std::size_t size;
                                      ~MappingGuard() { if (base != MAP_FAILED) munmap(base, size); }
                                  } guard{ region, length };
                                  std::memcpy(region, machine.text.data(), machine.text.size());
                                  for (std::size_t n = 0; n < machine.imports.size(); ++n) {
                                      const auto& fix = machine.imports[n];
                                      if (fix.dispPos + 4 > machine.text.size())throw std::runtime_error("invalid host imported call relocation");
                                      void* target = hostImport(fix.symbol);
                                      std::memcpy(static_cast<unsigned char*>(region) + importBase + n * 8, &target, sizeof(target));
                                      const std::int64_t rel = static_cast<std::int64_t>(importBase + n * 8) - static_cast<std::int64_t>(fix.dispPos + 4);
                                      const auto disp = static_cast<std::int32_t>(rel);
                                      for (int k = 0; k < 4; ++k)static_cast<unsigned char*>(region)[fix.dispPos + k] = static_cast<unsigned char>((static_cast<std::uint32_t>(disp) >> (8 * k)) & 255);
                                  }
                                  if (mprotect(region, length, PROT_READ | PROT_EXEC) != 0)
                                      throw std::runtime_error("machine-text executable protection failed");
                                  using WinX64Entry = std::int64_t(__attribute__((ms_abi))*)();
                                  auto* address = static_cast<unsigned char*>(region) + machine.entryOffset;
                                  auto entry = reinterpret_cast<WinX64Entry>(address);
                                  const auto result = entry();
                                  if (!pcn_test_live_heap.empty() || pcn_test_heap_allocs != pcn_test_heap_frees)
                                      throw std::runtime_error("native differential execution leaked owned Windows heap storage");
                                  return result;
                              }
                              // Never execute untrusted generated machine code in the compiler process.
                              // A signal, recursion overflow, or infinite loop must not kill the compiler.
                              static std::int64_t executeEmittedMachineTextIsolated(const MachineProgram& machine,
                                  std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
                                  int channel[2];
                                  if (pipe(channel) != 0) throw std::runtime_error("native test: pipe failed");
                                  pid_t pid = fork();
                                  if (pid < 0) { close(channel[0]); close(channel[1]); throw std::runtime_error("native test: fork failed"); }
                                  if (pid == 0) {
                                      close(channel[0]);
                                      struct rlimit core = { 0, 0 };
                                      (void)setrlimit(RLIMIT_CORE, &core);
                                      struct rlimit mem = { 512ull * 1024 * 1024, 512ull * 1024 * 1024 };
                                      (void)setrlimit(RLIMIT_AS, &mem);
                                      try {
                                          auto result = executeEmittedMachineText(machine);
                                          const auto* data = reinterpret_cast<const char*>(&result);
                                          std::size_t sent = 0;
                                          while (sent < sizeof(result)) {
                                              const ssize_t n = write(channel[1], data + sent, sizeof(result) - sent);
                                              if (n <= 0) _exit(123);
                                              sent += static_cast<std::size_t>(n);
                                          }
                                          _exit(0);
                                      }
                                      catch (...) { _exit(124); }
                                  }
                                  close(channel[1]);
                                  const auto deadline = std::chrono::steady_clock::now() + timeout;
                                  int status = 0;
                                  for (;;) {
                                      const auto done = waitpid(pid, &status, WNOHANG);
                                      if (done == pid) break;
                                      if (done < 0) { close(channel[0]); throw std::runtime_error("native test: waitpid failed"); }
                                      if (std::chrono::steady_clock::now() >= deadline) {
                                          kill(pid, SIGKILL);
                                          (void)waitpid(pid, &status, 0);
                                          close(channel[0]);
                                          throw std::runtime_error("native test: generated program exceeded execution deadline");
                                      }
                                      usleep(10000);
                                  }
                                  if (WIFSIGNALED(status)) {
                                      close(channel[0]);
                                      throw std::runtime_error("native test: generated machine code terminated by signal " +
                                          std::to_string(WTERMSIG(status)));
                                  }
                                  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                                      close(channel[0]);
                                      throw std::runtime_error("native test: generated machine code did not return successfully");
                                  }
                                  std::int64_t value = 0;
                                  char* data = reinterpret_cast<char*>(&value);
                                  std::size_t got = 0;
                                  while (got < sizeof(value)) {
                                      ssize_t n = read(channel[0], data + got, sizeof(value) - got);
                                      if (n <= 0) break;
                                      got += static_cast<std::size_t>(n);
                                  }
                                  close(channel[0]);
                                  if (got != sizeof(value)) throw std::runtime_error("native test: incomplete result from child");
                                  return value;
                              }
#endif

                              static std::filesystem::path withExtension(const std::filesystem::path& source, const char* extension) {
                                  auto p = source; p.replace_extension(extension); return p;
                              }


                              // Optional external COFF link adapter. Native PCIR/PMIR emission remains ours;
                              // an installed lld-link performs arbitrary third-party COFF/archive resolution.
                              // This deliberately does not claim to be Paracine's future built-in linker.
                              static int executeArgv(const std::vector<std::string>& argv) {
                                  if (argv.empty())throw std::runtime_error("external linker command is empty");
#ifdef _WIN32
                                  auto wide = [](const std::string& s)->std::wstring {
                                      if (s.empty())return {};
                                      const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
                                      if (n <= 0)throw std::runtime_error("invalid UTF-8 in linker argument");
                                      std::wstring out(static_cast<std::size_t>(n), L'\0');
                                      if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), static_cast<int>(s.size()), out.data(), n) != n)
                                          throw std::runtime_error("linker argument conversion failed");
                                      return out;
                                      };
                                  auto quote = [](const std::wstring& a)->std::wstring {
                                      std::wstring out = L"\""; std::size_t slash = 0;
                                      for (wchar_t ch : a) {
                                          if (ch == L'\\') { ++slash; continue; }
                                          if (ch == L'"') { out.append(slash * 2 + 1, L'\\'); out += ch; slash = 0; continue; }
                                          out.append(slash, L'\\'); slash = 0; out += ch;
                                      }
                                      out.append(slash * 2, L'\\'); out += L'"'; return out;
                                      };
                                  std::wstring cmd;
                                  for (const auto& a : argv) { if (!cmd.empty())cmd += L' '; cmd += quote(wide(a)); }
                                  std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end()); mutableCmd.push_back(L'\0');
                                  STARTUPINFOW si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
                                  if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
                                      throw std::runtime_error("cannot start external linker (lld-link or configured path): Win32 error " + std::to_string(GetLastError()));
                                  CloseHandle(pi.hThread);
                                  DWORD exitCode = 1;
                                  const DWORD waited = WaitForSingleObject(pi.hProcess, INFINITE);
                                  if (waited == WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess, &exitCode);
                                  CloseHandle(pi.hProcess);
                                  if (waited != WAIT_OBJECT_0)throw std::runtime_error("external linker process wait failed");
                                  return static_cast<int>(exitCode);
#else
                                  pid_t pid = fork();
                                  if (pid < 0)throw std::runtime_error("failed to create external linker process");
                                  if (pid == 0) {
                                      std::vector<char*> raw; raw.reserve(argv.size() + 1);
                                      for (const auto& a : argv)raw.push_back(const_cast<char*>(a.c_str())); raw.push_back(nullptr);
                                      execvp(raw.front(), raw.data()); _exit(127);
                                  }
                                  int status = 0;
                                  while (waitpid(pid, &status, 0) < 0) { if (errno == EINTR)continue; throw std::runtime_error("failed waiting for external linker"); }
                                  if (!WIFEXITED(status))throw std::runtime_error("external linker terminated abnormally");
                                  return WEXITSTATUS(status);
#endif
                              }

                              static int runExternalCOFFLink(const std::vector<std::string>& args) {
                                  std::string source, output, entry = "main", executable = "lld-link", subsystem = "console";
                                  std::vector<std::string> otherInputs, searchPaths;
                                  bool dll = false;
                                  for (std::size_t i = 1; i < args.size(); ++i) {
                                      const auto& a = args[i];
                                      if ((a == "-o" || a == "--out") && i + 1 < args.size())output = args[++i];
                                      else if (a == "--entry" && i + 1 < args.size())entry = args[++i];
                                      else if (a == "--linker" && i + 1 < args.size())executable = args[++i];
                                      else if (a == "--libpath" && i + 1 < args.size())searchPaths.push_back(args[++i]);
                                      else if (a == "--subsystem" && i + 1 < args.size())subsystem = args[++i];
                                      else if (a == "--dll")dll = true;
                                      else if (a.size() && a[0] == '-')throw std::runtime_error("unsupported link argument: " + a);
                                      else if (std::filesystem::path(a).extension() == ".pcn") {
                                          if (!source.empty())throw std::runtime_error("link currently accepts one .pcn compilation unit; additional input must be COFF .obj/.lib");
                                          source = a;
                                      }
                                      else {
                                          auto ext = std::filesystem::path(a).extension().string();
                                          if (ext != ".obj" && ext != ".lib" && ext != ".res")throw std::runtime_error("external link input must be .pcn, .obj, .lib, or .res: " + a);
                                          if (!std::filesystem::exists(a))throw std::runtime_error("external link input does not exist: " + a);
                                          otherInputs.push_back(a);
                                      }
                                  }
                                  if (source.empty() && otherInputs.empty())throw std::runtime_error("link requires .pcn or COFF objects/libraries");
                                  if (subsystem != "console" && subsystem != "windows")throw std::runtime_error("link subsystem must be console or windows");
                                  if (output.empty()) {
                                      std::filesystem::path sample = source.empty() ? std::filesystem::path(otherInputs.front()) : std::filesystem::path(source);
                                      sample.replace_extension(dll ? ".dll" : ".exe"); output = sample.string();
                                  }
                                  struct Scratch { std::filesystem::path filename; ~Scratch() { std::error_code ec; if (!filename.empty())std::filesystem::remove(filename, ec); } } scratch;
                                  if (!source.empty()) {
                                      auto options = CompilerOptions{};
                                      const auto artifacts = compileAll(readSourceFile(source), options, false);
                                      if (!artifacts.analysis.succeeded()) { printSemanticDiagnostics(artifacts.analysis); return 1; }
                                      const auto obj = PECOFFGenerator{}.emitObject(artifacts.pcir, entry);
                                      scratch.filename = std::filesystem::path(output);
                                      scratch.filename += ".paracine-link-temp.obj";
                                      writeBinaryFile(scratch.filename, obj);
                                  }
                                  std::vector<std::string> invocation = { executable,"/nologo","/machine:x64","/nodefaultlib","/out:" + output };
                                  invocation.push_back("/subsystem:" + subsystem);
                                  if (dll) { invocation.push_back("/dll"); invocation.push_back("/noentry"); }
                                  else invocation.push_back("/entry:" + entry);
                                  for (const auto& dir : searchPaths)invocation.push_back("/libpath:" + dir);
                                  if (!scratch.filename.empty())invocation.push_back(scratch.filename.string());
                                  for (const auto& arg : otherInputs)invocation.push_back(arg);
                                  const int result = executeArgv(invocation);
                                  if (result)throw std::runtime_error("external COFF link failed with exit code " + std::to_string(result));
                                  if (!std::filesystem::exists(output))throw std::runtime_error("external linker succeeded without producing output");
                                  std::cout << "external COFF link succeeded: " << output << "\n";
                                  return 0;
                              }

                              // --- Developer workflows: conservative formatting, deterministic packaging,
                              //     compiler-phase profiling, project creation and environment diagnostics. ---
                              // These tools don't change the language's supported semantic surface.
                              static constexpr const char* kCompilerBuild = "Paracine 2026 development toolchain (non-1.0)";

                              static std::string canonicalFormat(const std::string& source) {
                                  // Whitespace-only normalization: line-ending normalization, trailing-space
                                  // removal and a final newline. Do not rewrite operator spacing, identifier
                                  // spelling, string contents, indentation, or comments.
                                  std::string formatted;
                                  formatted.reserve(source.size() + 1);
                                  std::size_t lineStart = 0;
                                  for (std::size_t i = 0; i < source.size(); ++i) {
                                      if (source[i] == '\r' || source[i] == '\n') {
                                          std::size_t end = i;
                                          while (end > lineStart && (source[end - 1] == ' ' || source[end - 1] == '\t')) --end;
                                          formatted.append(source, lineStart, end - lineStart);
                                          formatted.push_back('\n');
                                          if (source[i] == '\r' && i + 1 < source.size() && source[i + 1] == '\n') ++i;
                                          lineStart = i + 1;
                                      }
                                  }
                                  if (lineStart < source.size()) {
                                      std::size_t end = source.size();
                                      while (end > lineStart && (source[end - 1] == ' ' || source[end - 1] == '\t')) --end;
                                      formatted.append(source, lineStart, end - lineStart);
                                      formatted.push_back('\n');
                                  }
                                  if (formatted.empty()) formatted = "\n";
                                  // Validate both streams with the compiler's own lexer before writing.
                                  // Parsing isn't required for formatting an in-progress source file.
                                  auto before = Lexer(source).tokenize(), after = Lexer(formatted).tokenize();
                                  if (before.size() != after.size())throw std::runtime_error("formatter changed token count; refusing to write");
                                  for (std::size_t i = 0; i < before.size(); ++i) {
                                      if (before[i].kind != after[i].kind || before[i].text != after[i].text)
                                          throw std::runtime_error("formatter changed lexical content; refusing to write");
                                  }
                                  return formatted;
                              }

                              static std::uint32_t archiveCRC32(const std::vector<std::uint8_t>& input) {
                                  std::uint32_t crc = 0xffffffffu;
                                  for (const auto byte : input) {
                                      crc ^= byte;
                                      for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
                                  }
                                  return ~crc;
                              }
                              static std::vector<std::uint8_t> byteVector(const std::string& text) {
                                  return std::vector<std::uint8_t>(text.begin(), text.end());
                              }
                              static std::string jsonEscape(const std::string& input) {
                                  std::ostringstream out;
                                  for (unsigned char ch : input) {
                                      if (ch == '"')out << "\\\"";
                                      else if (ch == '\\')out << "\\\\";
                                      else if (ch == '\n')out << "\\n";
                                      else if (ch == '\r')out << "\\r";
                                      else if (ch == '\t')out << "\\t";
                                      else if (ch < 32) {
                                          const char* digits = "0123456789abcdef";
                                          out << "\\u00" << digits[ch >> 4] << digits[ch & 15];
                                      }
                                      else out << static_cast<char>(ch);
                                  }
                                  return out.str();
                              }
                              struct ArchiveFile { std::string name; std::vector<std::uint8_t> data; };
                              static std::vector<std::uint8_t> makeDeterministicZip(const std::vector<ArchiveFile>& files) {
                                  if (files.size() > 65535)throw std::runtime_error("ZIP supports at most 65535 entries");
                                  std::vector<std::uint8_t> out, central;
                                  const auto u16 = [](std::vector<std::uint8_t>& dst, std::uint16_t n) {dst.push_back(n & 255); dst.push_back((n >> 8) & 255); };
                                  const auto u32 = [](std::vector<std::uint8_t>& dst, std::uint32_t n) {for (int i = 0; i < 4; ++i)dst.push_back((n >> (8 * i)) & 255); };
                                  const auto length32 = [](std::size_t n)->std::uint32_t {
                                      if (n > 0xffffffffull)throw std::runtime_error("ZIP64 is not supported: file too large");
                                      return static_cast<std::uint32_t>(n);
                                      };
                                  std::set<std::string> names;
                                  for (const auto& file : files) {
                                      if (file.name.empty() || file.name.front() == '/' || file.name.find("..") != std::string::npos ||
                                          file.name.find('\\') != std::string::npos || file.name.find(':') != std::string::npos ||
                                          file.name.size() > 65535 || !names.insert(file.name).second)
                                          throw std::runtime_error("unsafe or duplicate archive entry name");
                                      const auto pos = length32(out.size()), len = length32(file.data.size());
                                      const auto crc = archiveCRC32(file.data);
                                      // ZIP32 stored entries, fixed DOS date 1980-01-01, UTF-8 flag; reproducible.
                                      u32(out, 0x04034b50u); u16(out, 20); u16(out, 0x0800); u16(out, 0); u16(out, 0);
                                      u16(out, 0x0021); u32(out, crc); u32(out, len); u32(out, len);
                                      u16(out, static_cast<std::uint16_t>(file.name.size())); u16(out, 0);
                                      out.insert(out.end(), file.name.begin(), file.name.end());
                                      out.insert(out.end(), file.data.begin(), file.data.end());
                                      u32(central, 0x02014b50u); u16(central, 20); u16(central, 20); u16(central, 0x0800);
                                      u16(central, 0); u16(central, 0); u16(central, 0x0021); u32(central, crc);
                                      u32(central, len); u32(central, len); u16(central, static_cast<std::uint16_t>(file.name.size()));
                                      u16(central, 0); u16(central, 0); u16(central, 0); u16(central, 0); u32(central, 0); u32(central, pos);
                                      central.insert(central.end(), file.name.begin(), file.name.end());
                                  }
                                  const auto centralAt = length32(out.size()), centralSize = length32(central.size());
                                  out.insert(out.end(), central.begin(), central.end());
                                  u32(out, 0x06054b50u); u16(out, 0); u16(out, 0);
                                  u16(out, static_cast<std::uint16_t>(files.size())); u16(out, static_cast<std::uint16_t>(files.size()));
                                  u32(out, centralSize); u32(out, centralAt); u16(out, 0);
                                  return out;
                              }
                              static std::vector<std::uint8_t> buildDistribution(const std::string& source,
                                  const std::filesystem::path& sourcePath,
                                  const IRProgram& pcir,
                                  const std::string& entry) {
                                  PECOFFGenerator generator;
                                  auto executable = generator.emitExecutable(pcir, entry);
                                  auto object = generator.emitObject(pcir, entry);
                                  const auto original = sourcePath.filename().string();
                                  std::ostringstream manifest;
                                  manifest << "{\n  \"format\": \"paracine-development-bundle-v1\",\n"
                                      << "  \"compiler\": \"" << jsonEscape(kCompilerBuild) << "\",\n"
                                      << "  \"target\": \"windows-x86_64-pe32plus\",\n"
                                      << "  \"original_source\": \"" << jsonEscape(original) << "\",\n"
                                      << "  \"entry\": \"" << jsonEscape(entry.empty() ? "main" : entry) << "\",\n"
                                      << "  \"files\": [\"src/main.pcn\", \"bin/program.exe\", \"obj/program.obj\"]\n}\n";
                                  const std::string readme = "Paracine native Windows x64 development bundle\n"
                                      "This archive contains a Paracine source file and the emitted COFF/PE32+ binaries.\n"
                                      "This is a development-build artifact, not a certified Paracine 1.0 distribution.\n"
                                      "Run bin/program.exe on a compatible Windows x86-64 host after review.\n"
                                      "The file manifest.json describes the target and compiler edition.\n";
                                  return makeDeterministicZip({ { "manifest.json",byteVector(manifest.str()) },
                                       { "README.txt",byteVector(readme) },{ "src/main.pcn",byteVector(source) },
                                       { "obj/program.obj",std::move(object) },{ "bin/program.exe",std::move(executable) } });
                              }
                              static std::string formatPhaseTimings(const std::vector<std::pair<std::string, double>>& timings) {
                                  std::ostringstream out;
                                  out << "Paracine compiler phase profile (milliseconds, wall-clock)\n";
                                  double total = 0;
                                  for (const auto& item : timings) {
                                      out << "  " << std::left << std::setw(24) << item.first << " "
                                          << std::fixed << std::setprecision(3) << item.second << " ms\n"; total += item.second;
                                  }
                                  out << "  " << std::left << std::setw(24) << "TOTAL" << " " << std::fixed << std::setprecision(3) << total << " ms\n";
                                  return out.str();
                              }
                              static void initializeProject(const std::filesystem::path& root) {
                                  if (root.empty())throw std::runtime_error("init requires a directory path");
                                  std::error_code ec;
                                  if (std::filesystem::exists(root, ec)) {
                                      if (!std::filesystem::is_directory(root, ec) || !std::filesystem::is_empty(root, ec))
                                          throw std::runtime_error("init refuses to overwrite an existing nonempty path: " + root.string());
                                  }
                                  std::filesystem::create_directories(root / "src");
                                  writeTextFile(root / "src/main.pcn", "routine main() gives int\n    give 0\n");
                                  writeTextFile(root / "README.md", "# Paracine starter\n\n"
                                      "Build from this directory: `paracine build src/main.pcn -o main.exe`\n"
                                      "Run on Windows x86-64: `main.exe`\n"
                                      "The compiler is a development build; check supported features before deployment.\n");
                                  writeTextFile(root / ".gitignore", "*.exe\n*.obj\n*.pcir\n*.pmir\n*.asm\n*.zip\n");
                              }
                              static bool binaryAvailableOnPath(const std::string& binary) {
                                  std::string pathText;
#ifdef _WIN32
                                  // MSVC C4996: use the bounds-aware Windows CRT API rather than getenv.
                                  // _dupenv_s allocates a copy; std::free must release it on every path.
                                  char* raw = nullptr;
                                  std::size_t rawSize = 0;
                                  if (_dupenv_s(&raw, &rawSize, "PATH") != 0) {
                                      std::free(raw);
                                      return false;
                                  }
                                  std::unique_ptr<char, decltype(&std::free)> ownedPath(raw, &std::free);
                                  if (!ownedPath) return false;
                                  pathText.assign(ownedPath.get());
                                  constexpr char sep = ';';
                                  const std::string extension = ".exe";
#else
                                  const char* raw = std::getenv("PATH");
                                  if (!raw) return false;
                                  pathText.assign(raw);
                                  constexpr char sep = ':';
                                  const std::string extension = "";
#endif
                                  std::stringstream paths(pathText); std::string part;
                                  while (std::getline(paths, part, sep)) {
                                      auto path = std::filesystem::path(part) / binary;
                                      std::error_code ec;
                                      if (std::filesystem::is_regular_file(path, ec) || std::filesystem::is_regular_file(path.string() + extension, ec))return true;
                                  }
                                  return false;
                              }
                              static void printDoctor(std::ostream& out) {
                                  out << kCompilerBuild << "\nHost: ";
#if defined(_WIN32)
                                  out << "Windows";
#elif defined(__linux__)
                                  out << "Linux";
#else
                                  out << "other";
#endif
#if defined(_M_X64)||defined(__x86_64__)
                                  out << " x86-64\n";
#else
                                  out << " non-x86-64\n";
#endif
                                  out << "Compilation target: Windows x86-64 (COFF/PE32+)\n";
                                  for (const char* tool : { "cmake","clang++","g++","lld-link" })
                                      out << "External " << tool << ": " << (binaryAvailableOnPath(tool) ? "found" : "not found") << "\n";
                                  out << "Internal native linker: limited (external COFF linking uses lld-link adapter)\n"
                                      << "Language conformance: incomplete; this is NOT a 1.0 release.\n";
                              }

                              static void printHelp(std::ostream& os) {
                                  os <<
                                      "PARACINE 2026 native compiler\n\n"
                                      "Usage:\n"
                                      "  paracine build <file.pcn> [-o file.exe] [-O0|-O1|-O2|-O3] [--entry name]\n"
                                      "  paracine link [file.pcn] [third-party.obj/.lib/.res ...] [-o file.exe] [--entry main] [--dll] [--libpath dir] [--linker lld-link]\n"
                                      "  paracine run <file.pcn> [-O0|-O1|-O2|-O3] [--entry name]\n"
                                      "  paracine check <file.pcn> [--json]\n"
                                      "  paracine sage <file.pcn> [-o file.skm]\n"
                                      "  paracine test <file.pcn>\n"
                                      "  paracine emit cpp|obj|exe <file.pcn> [-o file]\n"
                                      "  paracine ir <file.pcn> [-o file.pcir]\n"
                                      "  paracine mir <file.pcn> [-o file.pmir]\n"
                                      "  paracine asm <file.pcn> [-o file.asm]\n"
                                      "  paracine obj <file.pcn> [-o file.obj]\n"
                                      "  paracine pe <file.pcn> [-o file.exe]\n"
                                      "  paracine explain <file.pcn>\n"
                                      "  paracine inspect <file.pcn>\n"
                                      "  paracine fmt <file.pcn> [--check|--write|-o formatted.pcn]\n"
                                      "  paracine package <file.pcn> [-o release.zip] [-O0..-O3]\n"
                                      "  paracine profile <file.pcn> [-o report.txt] [-O0..-O3]\n"
                                      "  paracine bench <file.pcn> [--iterations N] [-O0..-O3]\n"
                                      "  paracine init <empty-directory>\n"
                                      "  paracine doctor | version\n"
                                      "  paracine clean <file.pcn>\n\n"
                                      "Legacy compatibility:\n"
                                      "  paracine <file.pcn> [--check|--ast|--ir|--mir|--asm|--pe|--coff|--cpp] [--out file]\n";
                              }

}  // namespace pcn

int main(int argc, char** argv) {
    if (argc < 2) { pcn::printHelp(std::cout); return 0; }
    bool diagnosticJSON = false;
    try {
        std::vector<std::string> args; for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        if (args[0] == "link")return pcn::runExternalCOFFLink(args);
        if (args[0] == "version" || args[0] == "--version") {
            std::cout << pcn::kCompilerBuild << "\n"; return 0;
        }
        if (args[0] == "doctor") {
            if (args.size() != 1)throw std::runtime_error("doctor does not accept arguments");
            pcn::printDoctor(std::cout); return 0;
        }
        if (args[0] == "init") {
            if (args.size() != 2)throw std::runtime_error("usage: paracine init <empty-directory>");
            pcn::initializeProject(args[1]); std::cout << "initialized Paracine project: " << args[1] << "\n"; return 0;
        }
        auto isSource = [](const std::string& s) { return std::filesystem::path(s).extension() == ".pcn"; };
        std::string command, emitKind, sourceArg, outputArg, legacyMode;
        bool formatCheck = false, formatWrite = false;
        int iterations = 5;
        pcn::CompilerOptions options;

        if (isSource(args[0])) {
            sourceArg = args[0]; legacyMode = "pe";
            for (std::size_t i = 1; i < args.size(); ++i) {
                const auto& a = args[i];
                if (a == "--check") legacyMode = "check"; else if (a == "--ast") legacyMode = "ast"; else if (a == "--ir") legacyMode = "ir";
                else if (a == "--mir") legacyMode = "mir"; else if (a == "--asm") legacyMode = "asm"; else if (a == "--coff") legacyMode = "obj";
                else if (a == "--pe") legacyMode = "pe"; else if (a == "--cpp") legacyMode = "cpp";
                else if ((a == "--out" || a == "-o") && i + 1 < args.size()) outputArg = args[++i];
                else if (a.rfind("-O", 0) == 0 && a.size() == 3 && a[2] >= '0' && a[2] <= '3') options.optimizationLevel = a[2] - '0';
                else if (a == "--entry" && i + 1 < args.size()) options.entry = args[++i];
                else throw std::runtime_error("unknown option: " + a);
            }
            command = legacyMode;
        }
        else {
            command = args[0];
            std::size_t i = 1;
            if (command == "help" || command == "--help" || command == "-h") { pcn::printHelp(std::cout); return 0; }
            if (command == "emit") { if (i >= args.size()) throw std::runtime_error("emit requires cpp, obj, or exe"); emitKind = args[i++]; }
            if (i < args.size() && isSource(args[i])) sourceArg = args[i++];
            for (; i < args.size(); ++i) {
                const auto& a = args[i]; if ((a == "-o" || a == "--out") && i + 1 < args.size())outputArg = args[++i];
                else if (a.rfind("-O", 0) == 0 && a.size() == 3 && a[2] >= '0' && a[2] <= '3')options.optimizationLevel = a[2] - '0';
                else if (a == "--entry" && i + 1 < args.size())options.entry = args[++i];
                else if (a == "--json" && command == "check")diagnosticJSON = true;
                else if (a == "--check" && command == "fmt")formatCheck = true;
                else if (a == "--write" && command == "fmt")formatWrite = true;
                else if (a == "--iterations" && command == "bench" && i + 1 < args.size()) {
                    const std::string value = args[++i];
                    if (value.empty() || value.size() > 2 || !std::all_of(value.begin(), value.end(), [](unsigned char c) {return c >= '0' && c <= '9'; }))
                        throw std::runtime_error("--iterations must be an integer 1..30");
                    iterations = std::stoi(value);
                    if (iterations < 1 || iterations>30)throw std::runtime_error("--iterations must be 1..30");
                }
                else throw std::runtime_error("unknown option: " + a);
            }
            if (command == "emit") { if (emitKind == "cpp")command = "cpp"; else if (emitKind == "obj")command = "obj"; else if (emitKind == "exe")command = "pe"; else throw std::runtime_error("unknown emit kind: " + emitKind); }
        }

        if (command == "clean") {
            if (sourceArg.empty()) throw std::runtime_error("clean requires a .pcn source path");
            std::filesystem::path src = sourceArg; std::size_t removed = 0;
            for (const char* ext : { ".exe",".obj",".cpp",".pcir",".pmir",".asm" }) { auto f = pcn::withExtension(src, ext); std::error_code ec; if (std::filesystem::remove(f, ec))++removed; }
            std::cout << "Paracine clean: removed " << removed << " generated artifact(s).\n"; return 0;
        }
        if (sourceArg.empty()) throw std::runtime_error("command requires a .pcn source file");
        std::filesystem::path sourcePath = sourceArg;
        std::string source = pcn::readSourceFile(sourcePath);
        if (command == "fmt") {
            if (formatCheck && (formatWrite || !outputArg.empty()))
                throw std::runtime_error("fmt --check cannot be combined with --write or -o");
            if (formatWrite && !outputArg.empty())
                throw std::runtime_error("fmt --write cannot be combined with -o");
            const auto formatted = pcn::canonicalFormat(source);
            if (formatCheck) {
                if (formatted != source) { std::cerr << "format check failed: " << sourcePath.string() << "\n"; return 2; }
                std::cout << "format check passed: " << sourcePath.string() << "\n"; return 0;
            }
            if (formatWrite) {
                if (formatted != source)pcn::writeTextFile(sourcePath, formatted);
                std::cout << "formatted " << sourcePath.string() << "\n";
            }
            else if (!outputArg.empty()) {
                pcn::writeTextFile(outputArg, formatted);
                std::cout << "wrote formatted source " << outputArg << "\n";
            }
            else std::cout << formatted;
            return 0;
        }
        if (command == "bench") {
            std::vector<double> times;
            times.reserve(static_cast<std::size_t>(iterations));
            for (int n = 0; n < iterations; ++n) {
                const auto begin = std::chrono::steady_clock::now();
                auto trial = pcn::compileAll(source, options, true);
                if (!trial.analysis.succeeded()) {
                    pcn::printSemanticDiagnostics(trial.analysis); return 1;
                }
                times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            }
            std::sort(times.begin(), times.end());
            std::ostringstream report;
            report << "Paracine compile benchmark (not generated-program runtime), iterations=" << iterations
                << ", -O" << options.optimizationLevel << "\n" << std::fixed << std::setprecision(3)
                << "minimum=" << times.front() << " ms\nmedian=" << times[times.size() / 2]
                << " ms\nmaximum=" << times.back() << " ms\n";
            if (outputArg.empty())std::cout << report.str(); else pcn::writeTextFile(outputArg, report.str());
            return 0;
        }
        std::vector<std::pair<std::string, double>> phaseTimings;
        const bool needMachine = command != "check" && command != "ast" && command != "cpp" && command != "ir" && command != "explain" && command != "sage";
        auto artifacts = pcn::compileAll(source, options, needMachine, command == "profile" ? &phaseTimings : nullptr);
        if (!artifacts.analysis.succeeded()) {
            if (diagnosticJSON) {
                std::cout << "{\"ok\":false,\"diagnostics\":[";
                for (std::size_t i = 0; i < artifacts.analysis.diagnostics.size(); ++i) {
                    const auto& d = artifacts.analysis.diagnostics[i];
                    if (i)std::cout << ",";
                    std::cout << "{\"line\":" << d.location.line << ",\"column\":" << d.location.column
                        << ",\"message\":\"" << pcn::jsonEscape(d.message) << "\"}";
                }
                std::cout << "]}\n";
            }
            else pcn::printSemanticDiagnostics(artifacts.analysis);
            return 1;
        }

        auto writeOrPrint = [&](const std::string& text, const std::string& defaultExt) {if (outputArg.empty() && defaultExt.empty())std::cout << text; else { std::filesystem::path p = outputArg.empty() ? pcn::withExtension(sourcePath, defaultExt.c_str()) : std::filesystem::path(outputArg); pcn::writeTextFile(p, text); std::cout << "wrote " << p.string() << "\n"; }};

        if (command == "profile") {
            auto report = pcn::formatPhaseTimings(phaseTimings);
            report += "PCIR functions=" + std::to_string(artifacts.pcir.functions.size()) +
                "\nPMIR functions=" + std::to_string(artifacts.pmir.functions.size()) +
                "\nMachine code bytes=" + std::to_string(artifacts.machine.text.size()) + "\n";
            if (outputArg.empty())std::cout << report;
            else { pcn::writeTextFile(outputArg, report); std::cout << "wrote profile " << outputArg << "\n"; }
            return 0;
        }
        if (command == "package") {
            auto archive = pcn::buildDistribution(source, sourcePath, artifacts.pcir, options.entry);
            auto path = outputArg.empty() ? pcn::withExtension(sourcePath, ".zip") : std::filesystem::path(outputArg);
            pcn::writeBinaryFile(path, archive);
            std::cout << "wrote deterministic development bundle " << path.string()
                << " (" << archive.size() << " bytes)\n";
            return 0;
        }
        if (command == "check") {
            if (diagnosticJSON) {
                std::cout << "{\"ok\":true,\"routines\":" << artifacts.program.routines.size()
                    << ",\"declarations\":" << artifacts.program.declarations.size() << ",\"effects\":{";
                std::vector<std::string> keys;
                for (const auto& kv : artifacts.analysis.resolvedEffects)keys.push_back(kv.first);
                std::sort(keys.begin(), keys.end());
                for (std::size_t j = 0; j < keys.size(); ++j) {
                    if (j)std::cout << ",";
                    const auto& eff = artifacts.analysis.resolvedEffects.at(keys[j]);
                    std::cout << "\"" << pcn::jsonEscape(keys[j]) << "\":[";
                    std::size_t i = 0;
                    for (const auto& name : eff) { if (i++)std::cout << ","; std::cout << "\"" << pcn::jsonEscape(name) << "\""; }
                    std::cout << "]";
                }
                std::cout << "}}\n"; return 0;
            }
            std::cout << "Paracine check succeeded: " << artifacts.program.routines.size() << " routine(s), " << artifacts.program.declarations.size() << " declaration(s). SAGE/SKM semantic checks passed. PCIR verified.\n";
            for (const auto& [name, effects] : artifacts.analysis.resolvedEffects)std::cout << "  " << name << " with " << pcn::effectSetText(effects) << "\n";
            return 0;
        }
        if (command == "ast") { writeOrPrint(pcn::programToString(artifacts.program) + "\n", outputArg.empty() ? "" : ".ast"); return 0; }
        if (command == "sage") { writeOrPrint(pcn::skmToString(artifacts.analysis), outputArg.empty() ? "" : ".skm"); return 0; }
        if (command == "ir") { writeOrPrint(pcn::irToString(artifacts.pcir), outputArg.empty() ? "" : ".pcir"); return 0; }
        if (command == "mir") { writeOrPrint(pcn::pmirToString(artifacts.pmir), outputArg.empty() ? "" : ".pmir"); return 0; }
        if (command == "asm") { writeOrPrint(pcn::machineListingToString(artifacts.machine), outputArg.empty() ? "" : ".asm"); return 0; }
        if (command == "explain") {
            std::cout << "SAGE/SKM semantic report\n";
            for (const auto& r : artifacts.program.routines) { auto key = r.name + "#" + std::to_string(r.parameters.size()); auto it = artifacts.analysis.resolvedEffects.find(key); std::cout << "  routine " << r.name << ": effects=" << (it == artifacts.analysis.resolvedEffects.end() ? "pure" : pcn::effectSetText(it->second)) << ", parameters=" << r.parameters.size() << "\n"; }
            std::cout << "PCIR: " << artifacts.pcir.functions.size() << " function(s), optimization -O" << options.optimizationLevel << "\n"; return 0;
        }
        if (command == "cpp") { pcn::CppIREmitter e; auto text = e.emit(artifacts.pcir); if (outputArg.empty())std::cout << text; else pcn::writeTextFile(outputArg, text); return 0; }

        if (command == "run") {
            // `run` executes native output once. The independent `test` command
            // performs oracle/native differential execution.
            pcn::PECOFFGenerator generator; auto bytes = generator.emitExecutable(artifacts.pcir, options.entry);
            auto exe = pcn::withExtension(sourcePath, ".exe"); pcn::writeBinaryFile(exe, bytes);
#ifdef _WIN32
            int nativeResult = pcn::runNativeExecutable(exe);
            std::cout << "Paracine run completed with native exit code " << nativeResult << ".\n"; return nativeResult;
#elif defined(__linux__) && defined(__x86_64__)
            auto nativeResult = pcn::executeEmittedMachineTextIsolated(artifacts.machine);
            std::cout << "Paracine run: isolated emitted x86-64 execution (Microsoft x64 ABI), exit=" << nativeResult << "; PE image: " << exe.string() << "\n";
            return static_cast<int>(nativeResult);
#else
            std::cout << "PE32+ emitted to " << exe.string() << "; execution requires Windows x86-64.\n"; return 0;
#endif
        }
        if (command == "test") {
            pcn::PCIRInterpreter oracle(artifacts.pcir); auto value = oracle.run(options.entry);
            pcn::PECOFFGenerator generator; auto obj = generator.emitObject(artifacts.pcir, options.entry); auto pe = generator.emitExecutable(artifacts.pcir, options.entry);
            if (obj.empty() || pe.empty())throw std::runtime_error("native backend emitted empty artifact");
#if defined(__linux__) && defined(__x86_64__)
            const auto native = pcn::executeEmittedMachineTextIsolated(artifacts.machine);
            if (native != value) { std::cerr << "Paracine test FAILED: emitted x64=" << native << ", oracle=" << value << "\n"; return 2; }
            std::cout << "Paracine test passed: emitted x64=" << native << ", PCIR oracle=" << value << "; COFF and PE32+ generated.\n";
#elif defined(_WIN32)
            // Windows tests execute the actual PE through the OS loader.
            const auto generatedTest = pcn::withExtension(sourcePath, ".exe");
            pcn::writeBinaryFile(generatedTest, pe);
            const auto native = pcn::runNativeExecutable(generatedTest);
            if (static_cast<std::int64_t>(native) != value) { std::cerr << "Paracine test FAILED: Windows PE=" << native << ", oracle=" << value << "\n"; return 2; }
            std::cout << "Paracine test passed: Windows PE=" << native << ", PCIR oracle=" << value << ".\n";
#else
            std::cout << "Paracine structural test passed: COFF and PE32+ generated; PCIR oracle=" << value << ". Native execution was not available on this host.\n";
#endif
            return 0;
        }
        if (command == "inspect") {
            std::cout << "Paracine native pipeline\n  SAGE core: verified\n  PCIR functions: " << artifacts.pcir.functions.size() << "\n  PMIR functions: " << artifacts.pmir.functions.size() << "\n  machine .text bytes: " << artifacts.machine.text.size() << "\n  entry: " << artifacts.machine.entryName << " +0x" << std::hex << artifacts.machine.entryOffset << std::dec << "\n\n";
            std::cout << pcn::richABIReport(artifacts.pcir); return 0;
        }

        pcn::PECOFFGenerator generator;
        if (command == "obj") { auto bytes = generator.emitObject(artifacts.pcir, options.entry); auto p = outputArg.empty() ? pcn::withExtension(sourcePath, ".obj") : std::filesystem::path(outputArg); pcn::writeBinaryFile(p, bytes); std::cout << "wrote COFF object " << p.string() << " (" << bytes.size() << " bytes)\n"; return 0; }
        if (command == "pe" || command == "build") { auto bytes = generator.emitExecutable(artifacts.pcir, options.entry); auto p = outputArg.empty() ? pcn::withExtension(sourcePath, ".exe") : std::filesystem::path(outputArg); pcn::writeBinaryFile(p, bytes); std::cout << "wrote PE32+ executable " << p.string() << " (" << bytes.size() << " bytes)\n"; return 0; }

        throw std::runtime_error("unknown command: " + command);
    }
    catch (const std::exception& ex) {
        if (diagnosticJSON)
            std::cout << "{\"ok\":false,\"phase\":\"compiler\",\"error\":\""
            << pcn::jsonEscape(ex.what()) << "\"}\n";
        else std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}

