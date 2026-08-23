/*
** lexer.cpp — Python-subset tokenizer for zenpy.
**
** Produces INDENT/DEDENT tokens for block structure.
** Suppresses NEWLINE/INDENT/DEDENT inside brackets.
** '#' line comments. Tab = 4 spaces.
*/

#include "lexer.h"
#include <cstdio>
#include <cctype>

namespace zen
{

    /* =========================================================
    ** Initialization
    ** ========================================================= */

    void Lexer::init(const char *source, const char *filename)
    {
        source_ = source;
        filename_ = filename;
        start_ = source;
        current_ = source;
        line_ = 1;

        /* Indent stack starts with 0 (column 0) */
        indent_stack_[0] = 0;
        indent_top_ = 0;

        bracket_level_ = 0;

        pending_count_ = 0;
        pending_read_ = 0;

        at_line_start_ = true; /* file starts at beginning of a line */
    }

    /* =========================================================
    ** Public: next_token
    ** ========================================================= */

    Token Lexer::next_token()
    {
        /* Return pending tokens first (from DEDENT emissions) */
        if (pending_read_ < pending_count_)
            return pending_[pending_read_++];

        /* Reset pending queue */
        pending_count_ = 0;
        pending_read_ = 0;

        /* Handle indentation at the start of a logical line */
        if (at_line_start_ && bracket_level_ == 0)
        {
            at_line_start_ = false;

            /* Skip blank lines and comment-only lines */
            while (!is_at_end())
            {
                /* Count spaces at start of this line */
                int spaces = count_indent();

                /* If rest of line is blank or comment, skip it */
                if (*current_ == '\n')
                {
                    current_++;
                    line_++;
                    continue;
                }
                if (*current_ == '#')
                {
                    skip_line_comment();
                    if (*current_ == '\n')
                    {
                        current_++;
                        line_++;
                        continue;
                    }
                    /* EOF after comment */
                    break;
                }
                if (is_at_end())
                    break;

                /* Real content — process indentation */
                int current_indent = indent_stack_[indent_top_];

                if (spaces > current_indent)
                {
                    /* INDENT */
                    if (indent_top_ + 1 >= MAX_INDENT)
                        return error_token("Too many indentation levels.");
                    indent_stack_[++indent_top_] = spaces;
                    Token t;
                    t.type = TOK_INDENT;
                    t.line = line_;
                    t.start = current_;
                    t.length = 0;
                    pending_[pending_count_++] = t;
                }
                else if (spaces < current_indent)
                {
                    /* One or more DEDENTs */
                    while (indent_top_ > 0 && indent_stack_[indent_top_] > spaces)
                    {
                        indent_top_--;
                        Token t;
                        t.type = TOK_DEDENT;
                        t.line = line_;
                        t.start = current_;
                        t.length = 0;
                        if (pending_count_ < MAX_PENDING)
                            pending_[pending_count_++] = t;
                    }
                    if (indent_stack_[indent_top_] != spaces)
                        return error_token("Indentation error: unindent does not match any outer level.");
                }
                /* else: same level, no token needed */
                break;
            }

            /* Return first pending if any were generated */
            if (pending_read_ < pending_count_)
                return pending_[pending_read_++];
        }

        /* Skip inline whitespace (spaces/tabs, NOT newlines) */
        while (!is_at_end() && (*current_ == ' ' || *current_ == '\t'))
            current_++;

        /* Backslash line continuation: \ + \n = skip both and continue */
        if (!is_at_end() && *current_ == '\\' && *(current_ + 1) == '\n')
        {
            current_ += 2;
            line_++;
            /* restart whitespace skip on next line */
            while (!is_at_end() && (*current_ == ' ' || *current_ == '\t'))
                current_++;
        }

        /* Skip inline comment */
        if (!is_at_end() && *current_ == '#')
            skip_line_comment();

        /* Check for newline */
        if (!is_at_end() && *current_ == '\n')
        {
            current_++;
            int nl_line = line_;
            line_++;
            at_line_start_ = true;

            /* Suppress NEWLINE inside brackets */
            if (bracket_level_ > 0)
                return next_token(); /* recurse: skip it */

            start_ = current_ - 1;
            Token t;
            t.type = TOK_NEWLINE;
            t.line = nl_line;
            t.start = start_;
            t.length = 1;
            return t;
        }

        /* Check EOF */
        if (is_at_end())
        {
            /* Emit remaining DEDENTs */
            if (indent_top_ > 0)
            {
                indent_top_--;
                Token t;
                t.type = TOK_DEDENT;
                t.line = line_;
                t.start = current_;
                t.length = 0;

                /* Queue remaining DEDENTs */
                while (indent_top_ > 0)
                {
                    indent_top_--;
                    Token dt;
                    dt.type = TOK_DEDENT;
                    dt.line = line_;
                    dt.start = current_;
                    dt.length = 0;
                    if (pending_count_ < MAX_PENDING)
                        pending_[pending_count_++] = dt;
                }

                /* Queue final EOF */
                Token eof;
                eof.type = TOK_EOF;
                eof.line = line_;
                eof.start = current_;
                eof.length = 0;
                if (pending_count_ < MAX_PENDING)
                    pending_[pending_count_++] = eof;

                return t;
            }

            Token t;
            t.type = TOK_EOF;
            t.line = line_;
            t.start = current_;
            t.length = 0;
            return t;
        }

        /* Start scanning a real token */
        start_ = current_;
        char c = advance_char();

        /* Identifiers and keywords */
        if (isalpha(c) || c == '_')
            return scan_identifier();

        /* Numbers */
        if (isdigit(c))
            return scan_number();

        /* Operators and delimiters */
        switch (c)
        {
        case '(':
            bracket_level_++;
            return make_token(TOK_LPAREN);
        case ')':
            if (bracket_level_ > 0) bracket_level_--;
            return make_token(TOK_RPAREN);
        case '[':
            bracket_level_++;
            return make_token(TOK_LBRACKET);
        case ']':
            if (bracket_level_ > 0) bracket_level_--;
            return make_token(TOK_RBRACKET);
        case '{':
            bracket_level_++;
            return make_token(TOK_LBRACE);
        case '}':
            if (bracket_level_ > 0) bracket_level_--;
            return make_token(TOK_RBRACE);
        case ',':
            return make_token(TOK_COMMA);
        case '.':
            if (peek() == '.' && peek_next() == '.')
            {
                advance_char();
                advance_char();
                return make_token(TOK_ELLIPSIS);
            }
            return make_token(TOK_DOT);
        case ':':
            return make_token(TOK_COLON);
        case ';':
            return make_token(TOK_SEMICOLON);
        case '@':
            return make_token(TOK_AT);
        case '~':
            return make_token(TOK_TILDE);

        case '+':
            if (match_char('=')) return make_token(TOK_PLUS_EQ);
            return make_token(TOK_PLUS);
        case '-':
            if (match_char('>')) return make_token(TOK_ARROW);
            if (match_char('=')) return make_token(TOK_MINUS_EQ);
            return make_token(TOK_MINUS);
        case '*':
            if (match_char('*'))
            {
                if (match_char('=')) return make_token(TOK_DSTAR_EQ);
                return make_token(TOK_DSTAR);
            }
            if (match_char('=')) return make_token(TOK_STAR_EQ);
            return make_token(TOK_STAR);
        case '/':
            if (match_char('/'))
            {
                if (match_char('=')) return make_token(TOK_DSLASH_EQ);
                return make_token(TOK_DSLASH);
            }
            if (match_char('=')) return make_token(TOK_SLASH_EQ);
            return make_token(TOK_SLASH);
        case '%':
            if (match_char('=')) return make_token(TOK_PERCENT_EQ);
            return make_token(TOK_PERCENT);
        case '&':
            if (match_char('=')) return make_token(TOK_AMP_EQ);
            return make_token(TOK_AMP);
        case '|':
            if (match_char('=')) return make_token(TOK_PIPE_EQ);
            return make_token(TOK_PIPE);
        case '^':
            if (match_char('=')) return make_token(TOK_CARET_EQ);
            return make_token(TOK_CARET);

        case '<':
            if (match_char('<'))
            {
                if (match_char('=')) return make_token(TOK_LSHIFT_EQ);
                return make_token(TOK_LSHIFT);
            }
            if (match_char('=')) return make_token(TOK_LTEQ);
            return make_token(TOK_LT);
        case '>':
            if (match_char('>'))
            {
                if (match_char('=')) return make_token(TOK_RSHIFT_EQ);
                return make_token(TOK_RSHIFT);
            }
            if (match_char('=')) return make_token(TOK_GTEQ);
            return make_token(TOK_GT);

        case '=':
            if (match_char('=')) return make_token(TOK_EQEQ);
            return make_token(TOK_EQ);
        case '!':
            if (match_char('=')) return make_token(TOK_BANGEQ);
            return error_token("Unexpected character '!'.");
        case '?':
            if (match_char('.')) return make_token(TOK_QDOT);
            if (match_char('?')) return make_token(TOK_DQMARK);
            return error_token("Unexpected character '?'.");

        case '"':
        case '\'':
            return scan_string(c);

        default:
            return error_token("Unexpected character.");
        }
    }

    /* =========================================================
    ** Character-level helpers
    ** ========================================================= */

    char Lexer::advance_char()
    {
        return *current_++;
    }

    char Lexer::peek() const
    {
        return *current_;
    }

    char Lexer::peek_next() const
    {
        if (is_at_end()) return '\0';
        return current_[1];
    }

    bool Lexer::match_char(char expected)
    {
        if (is_at_end()) return false;
        if (*current_ != expected) return false;
        current_++;
        return true;
    }

    bool Lexer::is_at_end() const
    {
        return *current_ == '\0';
    }

    void Lexer::skip_line_comment()
    {
        while (!is_at_end() && *current_ != '\n')
            current_++;
    }

    /* =========================================================
    ** Token construction
    ** ========================================================= */

    Token Lexer::make_token(TokenType type)
    {
        Token t;
        t.type = type;
        t.line = line_;
        t.start = start_;
        t.length = (int32_t)(current_ - start_);
        return t;
    }

    Token Lexer::error_token(const char *msg)
    {
        Token t;
        t.type = TOK_ERROR;
        t.line = line_;
        t.start = msg;
        t.length = (int32_t)strlen(msg);
        return t;
    }

    /* =========================================================
    ** Indentation counting
    ** ========================================================= */

    int Lexer::count_indent()
    {
        int spaces = 0;
        while (!is_at_end())
        {
            if (*current_ == ' ')
            {
                spaces++;
                current_++;
            }
            else if (*current_ == '\t')
            {
                spaces += 4; /* tab = 4 spaces */
                current_++;
            }
            else
            {
                break;
            }
        }
        return spaces;
    }

    /* =========================================================
    ** Number scanning
    ** Supports: 123, 3.14, 1e5, 1.5e-3, 0xFF, 0b1010, 0o17
    ** ========================================================= */

    Token Lexer::scan_number()
    {
        /* Check for hex, binary, octal prefixes */
        if (start_[0] == '0' && !is_at_end())
        {
            if (*current_ == 'x' || *current_ == 'X')
            {
                advance_char();
                while (!is_at_end() && (isxdigit(*current_) || *current_ == '_'))
                    advance_char();
                return make_token(TOK_INT);
            }
            if (*current_ == 'b' || *current_ == 'B')
            {
                advance_char();
                while (!is_at_end() && (*current_ == '0' || *current_ == '1' || *current_ == '_'))
                    advance_char();
                return make_token(TOK_INT);
            }
            if (*current_ == 'o' || *current_ == 'O')
            {
                advance_char();
                while (!is_at_end() && (*current_ >= '0' && *current_ <= '7'))
                    advance_char();
                return make_token(TOK_INT);
            }
        }

        /* Decimal digits */
        while (!is_at_end() && (isdigit(*current_) || *current_ == '_'))
            advance_char();

        bool is_float = false;

        /* Fractional part */
        if (!is_at_end() && *current_ == '.' && isdigit(peek_next()))
        {
            is_float = true;
            advance_char(); /* consume '.' */
            while (!is_at_end() && (isdigit(*current_) || *current_ == '_'))
                advance_char();
        }

        /* Exponent */
        if (!is_at_end() && (*current_ == 'e' || *current_ == 'E'))
        {
            is_float = true;
            advance_char();
            if (!is_at_end() && (*current_ == '+' || *current_ == '-'))
                advance_char();
            while (!is_at_end() && isdigit(*current_))
                advance_char();
        }

        return make_token(is_float ? TOK_FLOAT : TOK_INT);
    }

    /* =========================================================
    ** String scanning
    ** Supports: "...", '...', """...""", '''...'''
    ** Escape sequences: \n, \t, \\, \", \', \0, \xHH, \uHHHH
    ** ========================================================= */

    Token Lexer::scan_string(char quote)
    {
        /* Check for triple-quoted string */
        bool triple = false;
        if (peek() == quote && peek_next() == quote)
        {
            advance_char();
            advance_char();
            triple = true;
        }

        while (!is_at_end())
        {
            char c = *current_;

            if (c == '\n')
            {
                if (!triple)
                    return error_token("Unterminated string (use triple quotes for multi-line).");
                line_++;
                current_++;
                continue;
            }

            if (c == '\\')
            {
                current_++; /* skip backslash */
                if (!is_at_end()) current_++; /* skip escaped char */
                continue;
            }

            if (c == quote)
            {
                if (triple)
                {
                    if (current_[1] == quote && current_[2] == quote)
                    {
                        current_ += 3;
                        return make_token(TOK_STRING);
                    }
                    current_++;
                    continue;
                }
                else
                {
                    current_++; /* closing quote */
                    return make_token(TOK_STRING);
                }
            }

            current_++;
        }

        return error_token("Unterminated string.");
    }

    /* =========================================================
    ** Identifier / keyword scanning
    ** ========================================================= */

    static TokenType check_keyword(const char *start, int length)
    {
        /* Simple table — could be a trie but keywords are few */
        struct KW { const char *word; TokenType type; };
        static const KW keywords[] = {
            {"and",      TOK_AND},
            {"assert",   TOK_ASSERT},
            {"async",    TOK_ASYNC},
            {"await",    TOK_AWAIT},
            {"break",    TOK_BREAK},
            {"class",    TOK_CLASS},
            {"const",    TOK_CONST},
            {"continue", TOK_CONTINUE},
            {"def",      TOK_DEF},
            {"del",      TOK_DEL},
            {"global",   TOK_GLOBAL},
            {"nonlocal", TOK_NONLOCAL},
            {"elif",     TOK_ELIF},
            {"else",     TOK_ELSE},
            {"enum",     TOK_ENUM},
            {"False",    TOK_FALSE},
            {"for",      TOK_FOR},
            {"from",     TOK_FROM},
            {"if",       TOK_IF},
            {"import",   TOK_IMPORT},
            {"using",    TOK_USING},
            {"in",       TOK_IN},
            {"is",       TOK_IS},
            {"lambda",   TOK_LAMBDA},
            {"match",    TOK_MATCH},
            {"None",     TOK_NONE},
            {"not",      TOK_NOT},
            {"or",       TOK_OR},
            {"pass",     TOK_PASS},
            {"print",    TOK_PRINT},
            {"return",   TOK_RETURN},
            {"record",   TOK_RECORD},
            {"self",     TOK_SELF},
            {"super",    TOK_SUPER},
            {"True",     TOK_TRUE},
            {"while",    TOK_WHILE},
            {"yield",    TOK_YIELD},
        };
        static const int num_kw = sizeof(keywords) / sizeof(keywords[0]);

        for (int i = 0; i < num_kw; i++)
        {
            int kwlen = (int)strlen(keywords[i].word);
            if (kwlen == length && memcmp(start, keywords[i].word, length) == 0)
                return keywords[i].type;
        }
        return TOK_IDENTIFIER;
    }

    Token Lexer::scan_identifier()
    {
        while (!is_at_end() && (isalnum(*current_) || *current_ == '_'))
            advance_char();

        int length = (int)(current_ - start_);

        /* Check for f-string: f"..." or f'...' */
        if (length == 1 && start_[0] == 'f' && !is_at_end() &&
            (*current_ == '"' || *current_ == '\''))
        {
            char q = advance_char();
            Token t = scan_string(q);
            t.type = TOK_FSTRING; /* mark as f-string for compiler */
            return t;
        }

        /* Single underscore is TOK_UNDERSCORE */
        if (length == 1 && start_[0] == '_')
            return make_token(TOK_UNDERSCORE);

        TokenType type = check_keyword(start_, length);
        return make_token(type);
    }

    /* =========================================================
    ** State save/restore (for compiler lookahead / rollback)
    ** ========================================================= */

    LexerState Lexer::save_state() const
    {
        LexerState s;
        s.current = current_;
        s.start = start_;
        s.line = line_;
        s.indent_top = indent_top_;
        s.bracket_level = bracket_level_;
        s.at_line_start = at_line_start_;
        return s;
    }

    void Lexer::restore_state(const LexerState &s)
    {
        current_ = s.current;
        start_ = s.start;
        line_ = s.line;
        indent_top_ = s.indent_top;
        bracket_level_ = s.bracket_level;
        at_line_start_ = s.at_line_start;

        /* Clear pending queue on restore */
        pending_count_ = 0;
        pending_read_ = 0;
    }

} /* namespace zen */
