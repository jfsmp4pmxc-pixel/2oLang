// 2oLang compiler - lexer
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "common.hpp"

static std::vector<std::string> g_files;

int registerFile(const std::string& name) {
    g_files.push_back(name);
    return (int)g_files.size() - 1;
}

std::string posStr(const Pos& p) {
    std::string f = (p.file >= 0 && p.file < (int)g_files.size()) ? g_files[p.file] : "?";
    return f + ":" + std::to_string(p.line) + ":" + std::to_string(p.col);
}

namespace {
struct Lexer {
    const std::string& s;
    int file;
    size_t i = 0;
    int line = 1, col = 1;
    Lexer(const std::string& src, int f) : s(src), file(f) {}

    char cur() const { return i < s.size() ? s[i] : '\0'; }
    char at(size_t k) const { return i + k < s.size() ? s[i + k] : '\0'; }
    void adv() {
        if (i >= s.size()) return;
        if (s[i] == '\n') {
            line++;
            col = 1;
        } else if (((unsigned char)s[i] & 0xC0) != 0x80) {
            col++;  // đếm theo ký tự UTF-8, không đếm byte tiếp theo
        }
        i++;
    }
    Pos here() const { return Pos{file, line, col}; }

    static bool identStart(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 0x80; }
    static bool identChar(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

    void readEscape(std::string& out) {
        Pos p = here();
        adv();  // bỏ dấu '\'
        char c = cur();
        switch (c) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '0': out += '\0'; break;
            case 'e': out += '\x1b'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            case 'x': {
                adv();
                int v = 0, n = 0;
                while (n < 2 && std::isxdigit((unsigned char)cur())) {
                    char h = cur();
                    v = v * 16 + (std::isdigit((unsigned char)h) ? h - '0' : (std::tolower(h) - 'a' + 10));
                    adv();
                    n++;
                }
                if (n == 0) fail(p, "escape \\x cần ít nhất một chữ số hex");
                out += (char)v;
                return;
            }
            default:
                fail(p, std::string("escape không hợp lệ: \\") + c);
        }
        adv();
    }

    std::vector<Token> run() {
        std::vector<Token> out;
        bool nl = true;
        while (true) {
            // bỏ khoảng trắng và comment
            while (i < s.size()) {
                char c = cur();
                if (c == '\n') {
                    nl = true;
                    adv();
                } else if (std::isspace((unsigned char)c)) {
                    adv();
                } else if (c == '/' && at(1) == '/') {
                    while (i < s.size() && cur() != '\n') adv();
                } else if (c == '/' && at(1) == '*') {
                    Pos p = here();
                    adv();
                    adv();
                    while (i < s.size() && !(cur() == '*' && at(1) == '/')) {
                        if (cur() == '\n') nl = true;
                        adv();
                    }
                    if (i >= s.size()) fail(p, "comment /* ... */ chưa đóng");
                    adv();
                    adv();
                } else {
                    break;
                }
            }
            Token t;
            t.pos = here();
            t.nl = nl;
            nl = false;
            if (i >= s.size()) {
                t.kind = T::Eof;
                out.push_back(t);
                return out;
            }
            unsigned char c = (unsigned char)cur();
            if (identStart(c)) {
                size_t b = i;
                while (identChar((unsigned char)cur())) adv();
                t.kind = T::Ident;
                t.text = s.substr(b, i - b);
            } else if (std::isdigit(c)) {
                size_t b = i;
                while (std::isdigit((unsigned char)cur())) adv();
                bool isF = false;
                if (cur() == '.' && std::isdigit((unsigned char)at(1))) {
                    isF = true;
                    adv();
                    while (std::isdigit((unsigned char)cur())) adv();
                }
                std::string txt = s.substr(b, i - b);
                if (!isF) {
                    // số nguyên quá lớn thì chuyển thành số thực
                    if (txt.size() > 18) {
                        isF = true;
                    } else {
                        t.kind = T::Int;
                        t.ival = std::strtoll(txt.c_str(), nullptr, 10);
                    }
                }
                if (isF) {
                    t.kind = T::Float;
                    t.fval = std::strtod(txt.c_str(), nullptr);
                }
                t.text = txt;
            } else if (c == '"') {
                Pos p = here();
                adv();
                std::string v;
                while (true) {
                    if (i >= s.size() || cur() == '\n') fail(p, "chuỗi chưa đóng dấu \"");
                    if (cur() == '"') {
                        adv();
                        break;
                    }
                    if (cur() == '\\') {
                        readEscape(v);
                    } else {
                        v += cur();
                        adv();
                    }
                }
                t.kind = T::Str;
                t.text = v;
            } else {
                t.kind = T::Op;
                std::string two;
                two += cur();
                two += at(1);
                if (two == "==" || two == "!=" || two == ">=" || two == "<=") {
                    t.text = two;
                    adv();
                    adv();
                } else if (std::string("+-*/%^<>=(){}[],;.#!").find((char)c) != std::string::npos) {
                    t.text = std::string(1, (char)c);
                    adv();
                } else {
                    fail(t.pos, std::string("ký tự không hợp lệ: '") + (char)c + "'");
                }
            }
            out.push_back(std::move(t));
        }
    }
};
}  // namespace

std::vector<Token> lex(const std::string& src, int fileId) { return Lexer(src, fileId).run(); }
