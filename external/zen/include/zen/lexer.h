#ifndef ZEN_LEXER_H
#define ZEN_LEXER_H

#include "common.h"
#include <cstdint>
#include <cstring>

namespace zen
{

    /* =========================================================
    ** Token types for Python-subset syntax.
    **
    ** Design:
    **   - NEWLINE separates statements (suppressed inside brackets)
    **   - INDENT/DEDENT delimit blocks (after ':')
    **   - No semicolons, no braces for blocks
    **   - '#' comments
    ** ========================================================= */

    enum TokenType : uint8_t
    {
        /* Literals */
        TOK_INT,            /* 42, 0xFF, 0b101 */
        TOK_FLOAT,          /* 3.14, 1e-5 */
        TOK_STRING,         /* "hello", 'world' */
        TOK_FSTRING,        /* f"hello {name}" */
        TOK_FSTRING_START,  /* f"...{ — starts interpolation */
        TOK_FSTRING_MID,    /* }...{ — between interpolations */
        TOK_FSTRING_END,    /* }..." — ends interpolation */

        /* Identifiers + Keywords */
        TOK_IDENTIFIER,
        TOK_DEF,
        TOK_CLASS,
        TOK_RECORD,
        TOK_ENUM,
        TOK_MATCH,
        TOK_RETURN,
        TOK_IF,
        TOK_ELIF,
        TOK_ELSE,
        TOK_WHILE,
        TOK_FOR,
        TOK_IN,
        TOK_BREAK,
        TOK_CONTINUE,
        TOK_PASS,
        TOK_ASSERT,
        TOK_TRUE,
        TOK_FALSE,
        TOK_NONE,       /* Python uses None, not nil */
        TOK_AND,
        TOK_OR,
        TOK_NOT,
        TOK_IMPORT,
        TOK_USING,
        TOK_FROM,
        TOK_CONST,
        TOK_YIELD,
        TOK_AWAIT,
        TOK_ASYNC,
        TOK_SELF,
        TOK_SUPER,
        TOK_PRINT,      /* built-in statement for now */
        TOK_LAMBDA,
        TOK_IS,         /* identity comparison */
        TOK_DEL,        /* del statement */
        TOK_GLOBAL,     /* global declaration */
        TOK_NONLOCAL,   /* nonlocal declaration */

        /* Operators */
        TOK_PLUS,       /* + */
        TOK_MINUS,      /* - */
        TOK_STAR,       /* * */
        TOK_SLASH,      /* / */
        TOK_PERCENT,    /* % */
        TOK_DSTAR,      /* ** (power) */
        TOK_DSLASH,     /* // (floor div) */
        TOK_AMP,        /* & */
        TOK_PIPE,       /* | */
        TOK_CARET,      /* ^ */
        TOK_TILDE,      /* ~ */
        TOK_LSHIFT,     /* << */
        TOK_RSHIFT,     /* >> */

        /* Comparison */
        TOK_EQ,         /* = (assignment) */
        TOK_EQEQ,      /* == */
        TOK_BANGEQ,     /* != */
        TOK_LT,        /* < */
        TOK_GT,        /* > */
        TOK_LTEQ,      /* <= */
        TOK_GTEQ,      /* >= */

        /* Augmented assignment */
        TOK_PLUS_EQ,    /* += */
        TOK_MINUS_EQ,   /* -= */
        TOK_STAR_EQ,    /* *= */
        TOK_SLASH_EQ,   /* /= */
        TOK_PERCENT_EQ, /* %= */
        TOK_DSTAR_EQ,   /* **= */
        TOK_DSLASH_EQ,  /* //= */
        TOK_AMP_EQ,     /* &= */
        TOK_PIPE_EQ,    /* |= */
        TOK_CARET_EQ,   /* ^= */
        TOK_LSHIFT_EQ,  /* <<= */
        TOK_RSHIFT_EQ,  /* >>= */

        /* Delimiters */
        TOK_LPAREN,     /* ( */
        TOK_RPAREN,     /* ) */
        TOK_LBRACKET,   /* [ */
        TOK_RBRACKET,   /* ] */
        TOK_LBRACE,     /* { */
        TOK_RBRACE,     /* } */
        TOK_COMMA,      /* , */
        TOK_DOT,        /* . */
        TOK_QDOT,       /* ?. (optional chaining) */
        TOK_DQMARK,     /* ?? (null coalescing) */
        TOK_COLON,      /* : */
        TOK_SEMICOLON,  /* ; (allowed but optional, for one-liners) */
        TOK_ARROW,      /* -> (return type hint) */
        TOK_AT,         /* @ (decorators — future) */
        TOK_ELLIPSIS,   /* ... */

        /* Structure */
        TOK_NEWLINE,    /* statement separator (suppressed in brackets) */
        TOK_INDENT,     /* block begin */
        TOK_DEDENT,     /* block end */

        /* Special */
        TOK_UNDERSCORE, /* _ (discard in unpacking) */
        TOK_EOF,
        TOK_ERROR,
    };

    /* =========================================================
    ** Token
    ** ========================================================= */

    struct Token
    {
        TokenType type;
        int32_t line;
        const char *start;  /* pointer into source */
        int32_t length;
    };

    /* =========================================================
    ** Lexer — Python-subset tokenizer.
    **
    ** INDENT/DEDENT algorithm (same as CPython):
    **   - Maintain stack of indentation levels, starting with [0]
    **   - At start of each logical line (after NEWLINE):
    **     - Count leading spaces (tab = 4 spaces)
    **     - If > stack top: push, emit INDENT
    **     - If < stack top: pop until match, emit DEDENT for each pop
    **     - If no match: indentation error
    **   - bracket_level > 0 suppresses NEWLINE/INDENT/DEDENT
    **
    ** The lexer produces a flat stream: the compiler never worries
    ** about whitespace. It just sees INDENT/DEDENT as block markers.
    ** ========================================================= */

    struct LexerState
    {
        const char *current;
        const char *start;
        int line;
        int indent_top;
        int bracket_level;
        bool at_line_start;
    };

    class Lexer
    {
    public:
        void init(const char *source, const char *filename = nullptr);
        Token next_token();

        /* State save/restore for compiler lookahead */
        LexerState save_state() const;
        void restore_state(const LexerState &s);

        const char *source() const { return source_; }
        const char *filename() const { return filename_; }

    private:
        /* Core scanning */
        char advance_char();
        char peek() const;
        char peek_next() const;
        bool match_char(char expected);
        void skip_line_comment();
        bool is_at_end() const;

        /* Token constructors */
        Token make_token(TokenType type);
        Token error_token(const char *msg);

        /* Scanning specific literal types */
        Token scan_number();
        Token scan_string(char quote);
        Token scan_identifier();

        /* Indentation engine */
        int count_indent();
        bool process_indentation();

        /* Source */
        const char *source_;
        const char *filename_;
        const char *start_;     /* start of current lexeme */
        const char *current_;   /* current position in source */
        int line_;

        /* Indentation stack */
        static const int MAX_INDENT = 64;
        int indent_stack_[MAX_INDENT];
        int indent_top_;        /* top index (indent_stack_[0] = 0 always) */

        /* Bracket nesting suppresses NEWLINE/INDENT/DEDENT */
        int bracket_level_;

        /* Pending tokens queue (DEDENT may produce multiple tokens) */
        static const int MAX_PENDING = 64;
        Token pending_[MAX_PENDING];
        int pending_count_;
        int pending_read_;

        /* Flags */
        bool at_line_start_;    /* we are at the beginning of a logical line */
    };

} /* namespace zen */

#endif /* ZEN_LEXER_H */
