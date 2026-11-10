// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Chương trình này là phần mềm tự do: bạn có thể phân phối lại và/hoặc sửa đổi nó theo các điều khoản của
// Giấy phép Công cộng GNU phiên bản 3 hoặc (tuỳ bạn chọn) bất kỳ phiên bản nào sau đó. Xem file LICENSE.
// Chương trình được phân phối với hy vọng hữu ích nhưng KHÔNG CÓ BẢO HÀNH NÀO.

// 2oLang compiler - parser (đệ quy xuống)
#include <set>

#include "common.hpp"

namespace {

const std::set<std::string> kKeywords = {
    "global", "var", "entry", "db", "int", "float", "string", "drive", "define", "obj", "public", "private",
    "msg", "if", "else", "while", "break", "continue", "return", "exit", "and", "or", "not", "to", "true",
    "false", "nil", "len", "element", "random", "input"};

const std::set<std::string> kTypes = {"int", "float", "string", "drive"};
const std::set<std::string> kColors = {"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white", "gray"};

// ---- dãy byte của phím tắt (input alt + f4, ...) ----
const std::set<std::string> kKeyNames = {"enter", "esc", "tab", "space", "backspace", "delete", "home", "end",
                                         "up", "down", "left", "right"};
bool isFnKey(const std::string& s, int* n = nullptr) {
    if (s.size() < 2 || s[0] != 'f') return false;
    for (size_t i = 1; i < s.size(); i++)
        if (!isdigit((unsigned char)s[i])) return false;
    int v = std::stoi(s.substr(1));
    if (v < 1 || v > 12) return false;
    if (n) *n = v;
    return true;
}

struct Parser {
    const std::vector<Token>& t;
    size_t p = 0;
    bool rt;
    Parser(const std::vector<Token>& toks, bool isRuntime) : t(toks), rt(isRuntime) {}

    const Token& cur() const { return t[p]; }
    const Token& peekTok(size_t k = 1) const { return t[std::min(p + k, t.size() - 1)]; }
    bool isOp(const char* s) const { return cur().kind == T::Op && cur().text == s; }
    bool isOpAt(size_t k, const char* s) const { return peekTok(k).kind == T::Op && peekTok(k).text == s; }
    bool isId(const char* s) const { return cur().kind == T::Ident && cur().text == s; }
    bool atEof() const { return cur().kind == T::Eof; }
    const Token& next() { return t[p < t.size() - 1 ? p++ : p]; }

    std::string describe(const Token& k) const {
        switch (k.kind) {
            case T::Eof: return "hết file";
            case T::Str: return "chuỗi \"" + k.text + "\"";
            default: return "'" + k.text + "'";
        }
    }
    [[noreturn]] void err(const std::string& m) const { fail(cur().pos, m); }
    void expectOp(const char* s) {
        if (!isOp(s)) err(std::string("mong đợi '") + s + "' nhưng gặp " + describe(cur()));
        next();
    }
    std::string expectIdent(const char* what) {
        if (cur().kind != T::Ident) err(std::string("mong đợi ") + what + " nhưng gặp " + describe(cur()));
        return next().text;
    }
    std::string expectName(const char* what) {
        Pos ps = cur().pos;
        std::string n = expectIdent(what);
        if (kKeywords.count(n) && !(rt && n.rfind("rt_", 0) == 0)) fail(ps, "'" + n + "' là từ khoá, không đặt làm tên được");
        return n;
    }
    void endStmt() {
        if (isOp(";")) {
            next();
            return;
        }
        if (cur().nl || isOp("}") || atEof()) return;  // cho phép bỏ ';' khi xuống dòng
        err("thiếu ';' (gặp " + describe(cur()) + " ngay sau câu lệnh)");
    }

    // ---------------- biểu thức ----------------
    ExprP mk(EK k, Pos ps) {
        auto e = std::make_unique<Expr>();
        e->k = k;
        e->pos = ps;
        return e;
    }
    ExprP bin(const std::string& op, ExprP l, ExprP r, Pos ps) {
        auto e = mk(EK::Binary, ps);
        e->s = op;
        e->a.push_back(std::move(l));
        e->a.push_back(std::move(r));
        return e;
    }

    ExprP parseExpr() { return parseOr(); }

    ExprP parseOr() {
        auto l = parseAnd();
        while (isId("or")) {
            Pos ps = next().pos;
            l = bin("or", std::move(l), parseAnd(), ps);
        }
        return l;
    }
    ExprP parseAnd() {
        auto l = parseCmp();
        while (isId("and")) {
            Pos ps = next().pos;
            l = bin("and", std::move(l), parseCmp(), ps);
        }
        return l;
    }
    ExprP parseCmp() {
        auto l = parseConcat();
        while (cur().kind == T::Op &&
               (cur().text == "==" || cur().text == "!=" || cur().text == "<" || cur().text == ">" ||
                cur().text == "<=" || cur().text == ">=")) {
            Token o = next();
            l = bin(o.text, std::move(l), parseConcat(), o.pos);
        }
        return l;
    }
    bool operandStart() const {
        const Token& k = cur();
        if (k.nl) return false;  // xuống dòng thì không nối chuỗi ngầm
        if (k.kind == T::Int || k.kind == T::Float || k.kind == T::Str) return true;
        if (k.kind == T::Ident) {
            static const std::set<std::string> noStart = {"global", "var", "entry", "db", "int", "float",
                                                          "string", "drive", "define", "obj", "public", "private",
                                                          "msg", "if", "else", "while", "break", "continue",
                                                          "return", "exit", "and", "or", "to"};
            return !noStart.count(k.text);
        }
        return false;
    }
    // nối chuỗi bằng cách đặt hai biểu thức cạnh nhau: ten ho, i"\n"
    ExprP parseConcat() {
        auto l = parseAdd();
        while (operandStart()) {
            Pos ps = cur().pos;
            l = bin("cat", std::move(l), parseAdd(), ps);
        }
        return l;
    }
    ExprP parseAdd() {
        auto l = parseMul();
        while (isOp("+") || isOp("-")) {
            Token o = next();
            l = bin(o.text, std::move(l), parseMul(), o.pos);
        }
        return l;
    }
    ExprP parseMul() {
        auto l = parseUnary();
        while (isOp("*") || isOp("/") || isOp("%")) {
            Token o = next();
            l = bin(o.text, std::move(l), parseUnary(), o.pos);
        }
        return l;
    }
    ExprP parseUnary() {
        Pos ps = cur().pos;
        std::string op;
        if (isOp("-")) op = "neg";
        else if (isOp("!") || isId("not")) op = "not";
        else if (isId("len")) op = "len";
        else if (isId("element")) op = "element";
        if (!op.empty()) {
            next();
            auto e = mk(EK::Unary, ps);
            e->s = op;
            e->a.push_back(parseUnary());
            return e;
        }
        return parsePow();
    }
    ExprP parsePow() {
        auto l = parsePostfix();
        if (isOp("^")) {
            Pos ps = next().pos;
            return bin("^", std::move(l), parseUnary(), ps);  // kết hợp phải
        }
        return l;
    }
    ExprP parsePostfix() {
        auto e = parsePrimary();
        while (true) {
            if (isOp(".") && peekTok().kind == T::Ident) {
                Pos ps = next().pos;
                std::string name = next().text;
                if (isOp("(")) {
                    auto m = mk(EK::Method, ps);
                    m->s = name;
                    m->a.push_back(std::move(e));
                    next();
                    if (!isOp(")")) {
                        while (true) {
                            m->a.push_back(parseExpr());
                            if (isOp(",")) next();
                            else break;
                        }
                    }
                    expectOp(")");
                    e = std::move(m);
                } else {
                    auto f = mk(EK::Field, ps);
                    f->s = name;
                    f->a.push_back(std::move(e));
                    e = std::move(f);
                }
            } else if (isOp("[")) {
                Pos ps = next().pos;
                auto ix = mk(EK::Index, ps);
                ix->a.push_back(std::move(e));
                ix->a.push_back(parseExpr());
                expectOp("]");
                e = std::move(ix);
            } else {
                break;
            }
        }
        return e;
    }

    ExprP parseChord(Pos ps) {
        int mask = 0;  // shift=1 alt=2 ctrl=4
        std::string key;
        while (true) {
            Pos kp = cur().pos;
            std::string w = expectIdent("tên phím");
            if (w == "shift") mask |= 1;
            else if (w == "alt") mask |= 2;
            else if (w == "ctrl") mask |= 4;
            else {
                key = w;
                if (!(w.size() == 1 || kKeyNames.count(w) || isFnKey(w)))
                    fail(kp, "phím không hỗ trợ: '" + w + "'");
                break;
            }
            expectOp("+");
        }
        std::string seq;
        const std::string ESC = "\x1b";
        std::string m = std::to_string(1 + mask);
        int fn = 0;
        if (isFnKey(key, &fn)) {
            if (fn <= 4) {
                char c = "PQRS"[fn - 1];
                seq = mask ? ESC + "[1;" + m + c : ESC + "O" + c;
            } else {
                static const int codes[] = {15, 17, 18, 19, 20, 21, 23, 24};
                std::string c = std::to_string(codes[fn - 5]);
                seq = mask ? ESC + "[" + c + ";" + m + "~" : ESC + "[" + c + "~";
            }
        } else if (key == "up" || key == "down" || key == "left" || key == "right" || key == "home" || key == "end") {
            char c = key == "up" ? 'A' : key == "down" ? 'B' : key == "right" ? 'C' : key == "left" ? 'D' : key == "home" ? 'H' : 'F';
            seq = mask ? ESC + "[1;" + m + c : ESC + "[" + c;
        } else if (key == "delete") {
            seq = mask ? ESC + "[3;" + m + "~" : ESC + "[3~";
        } else {
            std::string base;
            if (key == "enter") base = "\r";
            else if (key == "esc") base = ESC;
            else if (key == "tab") base = "\t";
            else if (key == "space") base = " ";
            else if (key == "backspace") base = "\x7f";
            else base = key;
            char c = base[0];
            if (mask & 4) {  // ctrl + chữ cái
                if (base.size() == 1 && isalpha((unsigned char)c)) c = (char)(tolower(c) - 'a' + 1);
                else fail(ps, "ctrl chỉ kết hợp được với chữ cái");
            } else if ((mask & 1) && isalpha((unsigned char)c)) {
                c = (char)toupper(c);
            }
            base[0] = c;
            seq = (mask & 2) ? ESC + base : base;
        }
        auto e = mk(EK::Chord, ps);
        e->s = seq;
        return e;
    }

    bool chordAhead() const {
        if (cur().kind != T::Ident) return false;
        const std::string& w = cur().text;
        if (w == "ctrl" || w == "alt" || w == "shift") return isOpAt(1, "+");
        return kKeyNames.count(w) > 0 || isFnKey(w);
    }

    ExprP parsePrimary() {
        const Token& k = cur();
        Pos ps = k.pos;
        if (k.kind == T::Int) {
            auto e = mk(EK::Int, ps);
            e->i = next().ival;
            return e;
        }
        if (k.kind == T::Float) {
            auto e = mk(EK::Float, ps);
            e->f = next().fval;
            return e;
        }
        if (k.kind == T::Str) {
            auto e = mk(EK::Str, ps);
            e->s = next().text;
            return e;
        }
        if (isOp("(")) {
            next();
            auto e = parseExpr();
            expectOp(")");
            return e;
        }
        if (isOp("[")) {
            next();
            auto e = mk(EK::List, ps);
            if (!isOp("]")) {
                while (true) {
                    e->a.push_back(parseExpr());
                    if (isOp(",")) next();
                    else break;
                }
            }
            expectOp("]");
            return e;
        }
        if (k.kind != T::Ident) err("mong đợi một biểu thức nhưng gặp " + describe(k));
        std::string w = k.text;
        if (w == "true" || w == "false") {
            next();
            auto e = mk(EK::Bool, ps);
            e->i = (w == "true");
            return e;
        }
        if (w == "nil") {
            next();
            return mk(EK::Nil, ps);
        }
        if (w == "random") {
            next();
            expectOp("(");
            auto e = mk(EK::Random, ps);
            if (!isOp(")")) {
                e->a.push_back(parseExpr());
                if (!isId("to")) err("random cần dạng random(a to b)");
                next();
                e->a.push_back(parseExpr());
            }
            expectOp(")");
            return e;
        }
        if (w == "input") {
            next();
            if (chordAhead()) return parseChord(ps);
            auto e = mk(EK::Input, ps);
            bool none = isOp(";") || isOp(")") || isOp(",") || isOp("}") || cur().nl || atEof();
            if (!none) e->a.push_back(parseConcat());
            return e;
        }
        if (w == "drive" && isOpAt(1, "(")) {  // drive(a).add(...)
            next();
            next();
            auto e = parseExpr();
            expectOp(")");
            return e;
        }
        if (kKeywords.count(w) && !(rt && w.rfind("rt_", 0) == 0)) err("không mong đợi từ khoá '" + w + "' trong biểu thức");
        next();
        if (isOp("(")) {  // gọi hàm
            auto e = mk(EK::Call, ps);
            e->s = w;
            next();
            if (!isOp(")")) {
                while (true) {
                    e->a.push_back(parseExpr());
                    if (isOp(",")) next();
                    else break;
                }
            }
            expectOp(")");
            return e;
        }
        auto e = mk(EK::Var, ps);
        e->s = w;
        return e;
    }

    // ---------------- phục hồi sau lỗi ----------------
    static bool isStmtStart(const Token& k) {
        static const std::set<std::string> kw = {"msg", "if", "while", "break", "continue", "return", "exit", "db", "int",
                                                 "float", "string", "drive", "define", "obj", "global", "var", "entry",
                                                 "public", "private"};
        return k.kind == T::Ident && kw.count(k.text);
    }
    static void note(const CompileError& e) { reportError(e); }

    // Bỏ token cho tới ranh giới câu lệnh kế tiếp: sau ';', sau một khối {...} đã đóng,
    // trước '}' của khối bao ngoài, hoặc trước từ khoá mở đầu câu lệnh ở đầu dòng mới.
    void syncStmt(size_t st) {
        int depth = 0;
        while (!atEof()) {
            if (isOp(";") && depth == 0) {
                next();
                break;
            }
            if (isOp("}")) {
                if (depth == 0) break;
                depth--;
                next();
                if (depth == 0) break;
                continue;
            }
            if (isOp("{")) {
                depth++;
                next();
                continue;
            }
            if (depth == 0 && p > st && cur().nl && isStmtStart(cur())) break;
            next();
        }
        if (p == st && !atEof()) next();  // luôn tiến lên ít nhất một token
    }

    // Bỏ token cho tới khai báo cấp cao kế tiếp (global/var/entry/define/obj/#import ở đầu dòng).
    void syncTop(size_t st) {
        int depth = 0;
        for (size_t k = st; k < p; k++) {
            if (t[k].kind == T::Op && t[k].text == "{") depth++;
            else if (t[k].kind == T::Op && t[k].text == "}") depth--;
        }
        if (depth < 0) depth = 0;
        while (!atEof()) {
            if (isOp("{")) {
                depth++;
                next();
                continue;
            }
            if (isOp("}")) {
                if (depth > 0) depth--;
                next();
                if (depth == 0) break;
                continue;
            }
            if (depth == 0 && p > st && cur().nl &&
                (isId("global") || isId("var") || isId("entry") || isId("define") || isId("obj") || isOp("#")))
                break;
            next();
        }
        if (p == st && !atEof()) next();
    }

    // ---------------- câu lệnh ----------------
    Block parseBlock() {
        expectOp("{");
        Block b;
        while (!isOp("}")) {
            if (atEof()) {
                note(CompileError(cur().pos, "thiếu '}' đóng khối"));
                return b;
            }
            size_t st = p;
            try {
                b.s.push_back(parseStmt());
            } catch (CompileError& e) {
                note(e);
                syncStmt(st);
            }
        }
        next();
        return b;
    }

    StmtP mkS(SK k, Pos ps) {
        auto s = std::make_unique<Stmt>();
        s->k = k;
        s->pos = ps;
        return s;
    }

    StmtP parseStmt() {
        Pos ps = cur().pos;
        if (isOp("{")) {
            auto s = mkS(SK::Nested, ps);
            s->body = parseBlock();
            return s;
        }
        if (cur().kind == T::Ident) {
            const std::string w = cur().text;
            if (w == "db" || (kTypes.count(w) && !(w == "drive" && isOpAt(1, "(")) && peekTok().kind == T::Ident)) {
                note(CompileError(cur().pos, "trong hàm/entry không khai báo bằng db hoặc kiểu dữ liệu; chỉ cần viết: ten = giá_trị;"));
                if (w == "db") next();
                if (cur().kind == T::Ident && kTypes.count(cur().text) && peekTok().kind == T::Ident) next();
                // phần còn lại được đọc như một phép gán bình thường
            }
            if (w == "input" && isOpAt(1, "=")) {  // input = a;  ->  đọc một dòng vào a (như a = input;)
                next();
                next();
                auto target = parseExpr();
                if (target->k != EK::Var && target->k != EK::Field && target->k != EK::Index)
                    fail(target->pos, "sau 'input =' phải là biến, trường đối tượng hoặc phần tử drive (ví dụ: input = a;)");
                auto s = mkS(SK::Assign, ps);
                s->target = std::move(target);
                s->e = mk(EK::Input, ps);
                endStmt();
                return s;
            }
            if (w == "msg") {
                next();
                auto s = mkS(SK::Msg, ps);
                s->e = parseExpr();
                if (isOp(",")) {
                    next();
                    Pos cp = cur().pos;
                    std::string c = expectIdent("tên màu");
                    if (!kColors.count(c)) fail(cp, "màu không hỗ trợ: '" + c + "' (black red green yellow blue magenta cyan white gray)");
                    s->color = c;
                }
                endStmt();
                return s;
            }
            if (w == "if") {
                next();
                auto s = mkS(SK::If, ps);
                expectOp("(");
                s->conds.push_back(parseExpr());
                expectOp(")");
                s->blocks.push_back(parseBlock());
                while (isId("else")) {
                    next();
                    if (isId("if")) {
                        next();
                        expectOp("(");
                        s->conds.push_back(parseExpr());
                        expectOp(")");
                        s->blocks.push_back(parseBlock());
                    } else {
                        s->blocks.push_back(parseBlock());
                        break;
                    }
                }
                return s;
            }
            if (w == "while") {
                next();
                auto s = mkS(SK::While, ps);
                expectOp("(");
                if (!isOp(")")) {
                    s->e = parseExpr();
                    if (s->e->k == EK::Bool && s->e->i == 1) {
                        s->whileForever = true;
                        s->e.reset();
                    }
                }
                expectOp(")");
                s->body = parseBlock();
                return s;
            }
            if (w == "break" || w == "continue") {
                next();
                auto s = mkS(w == "break" ? SK::Break : SK::Continue, ps);
                endStmt();
                return s;
            }
            if (w == "return" || w == "exit") {
                next();
                auto s = mkS(w == "return" ? SK::Return : SK::Exit, ps);
                if (!(isOp(";") || isOp("}") || cur().nl || atEof())) s->e = parseExpr();
                endStmt();
                return s;
            }
            if (w == "else") err("'else' không có 'if' đi trước");
        }
        // gán hoặc biểu thức
        auto lhs = parseExpr();
        if (isOp("=")) {
            Pos ap = next().pos;
            if (lhs->k != EK::Var && lhs->k != EK::Field && lhs->k != EK::Index) fail(ap, "vế trái của '=' phải là biến, trường đối tượng hoặc phần tử drive");
            auto s = mkS(SK::Assign, ps);
            s->target = std::move(lhs);
            s->e = parseExpr();
            endStmt();
            return s;
        }
        auto s = mkS(SK::ExprS, ps);
        s->e = std::move(lhs);
        endStmt();
        return s;
    }

    // ---------------- khai báo cấp cao ----------------
    StmtP parseDecl(bool needDb) {
        Pos ps = cur().pos;
        auto s = mkS(SK::Decl, ps);
        if (isId("db")) {
            if (!needDb) note(CompileError(cur().pos, "trong global không dùng db; viết: int a = 6;"));
            next();
            s->db = true;
        } else if (needDb) {
            note(CompileError(cur().pos, "trong var phải khai báo bằng db; viết: db int a = 6;"));
        }
        Pos tp = cur().pos;
        s->type = expectIdent("kiểu dữ liệu (int, float, string, drive)");
        if (!kTypes.count(s->type)) fail(tp, "kiểu dữ liệu không hợp lệ: '" + s->type + "'");
        s->name = expectName("tên biến");
        if (isOp("=")) {
            next();
            while (true) {
                s->list.push_back(parseExpr());
                if (isOp(",") && s->type == "drive") next();
                else break;
            }
        }
        endStmt();
        return s;
    }

    void parseVarBlock(std::vector<StmtP>& out, bool needDb) {
        expectOp("{");
        while (!isOp("}")) {
            if (atEof()) {
                note(CompileError(cur().pos, "thiếu '}' đóng khối"));
                return;
            }
            size_t st = p;
            try {
                out.push_back(parseDecl(needDb));
            } catch (CompileError& e) {
                note(e);
                syncStmt(st);
            }
        }
        next();
    }

    void parseObj(Module& m) {
        ObjDef o;
        o.pos = next().pos;  // 'obj'
        o.name = expectName("tên đối tượng");
        expectOp("{");
        while (!isOp("}")) {
            if (!(isId("public") || isId("private"))) err("trong obj mong đợi khối public { } hoặc private { }");
            bool priv = next().text == "private";
            expectOp("{");
            while (!isOp("}")) {
                if (atEof()) err("thiếu '}' đóng khối");
                size_t st = p;
                try {
                    ObjField f;
                    f.pos = cur().pos;
                    f.priv = priv;
                    f.name = expectName("tên trường");
                    if (isOp("=")) {
                        next();
                        f.init = parseExpr();
                    }
                    endStmt();
                    o.fields.push_back(std::move(f));
                } catch (CompileError& e) {
                    note(e);
                    syncStmt(st);
                }
            }
            next();
        }
        next();
        m.objs.push_back(std::move(o));
    }

    void parseTop(Module& m) {
            Pos ps = cur().pos;
            if (isOp("#")) {
                next();
                if (!isId("import")) err("sau '#' mong đợi 'import'");
                next();
                if (cur().kind != T::Str) err("#import cần đường dẫn dạng chuỗi: #import \"scr/shell.2o\"");
                m.imports.push_back({ps, next().text});
            } else if (isId("global")) {
                if (m.hasGlobal) fail(ps, "khối global bị khai báo hai lần");
                m.hasGlobal = true;
                m.globalPos = ps;
                next();
                parseVarBlock(m.globals, false);
            } else if (isId("var")) {
                if (m.hasVar) fail(ps, "khối var bị khai báo hai lần");
                m.hasVar = true;
                m.varPos = ps;
                next();
                parseVarBlock(m.vars, true);
            } else if (isId("entry")) {
                if (m.hasEntry) fail(ps, "khối entry bị khai báo hai lần");
                m.hasEntry = true;
                m.entryPos = ps;
                next();
                m.entry = parseBlock();
            } else if (isId("define")) {
                next();
                FuncDef f;
                f.pos = ps;
                f.name = expectName("tên hàm");
                expectOp("(");
                if (!isOp(")")) {
                    while (true) {
                        f.params.push_back(expectName("tên tham số"));
                        if (isOp(",")) next();
                        else break;
                    }
                }
                expectOp(")");
                f.body = parseBlock();
                m.funcs.push_back(std::move(f));
            } else if (isId("obj")) {
                parseObj(m);
            } else {
                err("ở cấp ngoài cùng chỉ có thể có global, var, define, obj, entry hoặc #import (gặp " + describe(cur()) + ")");
            }
    }

    Module parseModule() {
        Module m;
        while (!atEof()) {
            size_t st = p;
            try {
                parseTop(m);
            } catch (CompileError& e) {
                note(e);
                syncTop(st);
            }
        }
        return m;
    }
};

}  // namespace

Module parseModule(const std::vector<Token>& toks, bool isRuntime) {
    Parser ps(toks, isRuntime);
    return ps.parseModule();
}
