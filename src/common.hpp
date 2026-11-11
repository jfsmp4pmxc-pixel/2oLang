// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Chương trình này là phần mềm tự do: bạn có thể phân phối lại và/hoặc sửa đổi nó theo các điều khoản của
// Giấy phép Công cộng GNU phiên bản 3 hoặc (tuỳ bạn chọn) bất kỳ phiên bản nào sau đó. Xem file LICENSE.
// Chương trình được phân phối với hy vọng hữu ích nhưng KHÔNG CÓ BẢO HÀNH NÀO.

// 2oLang compiler - các định nghĩa dùng chung
#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Vị trí trong mã nguồn (file là chỉ số vào bảng tên file)
struct Pos {
    int file = 0, line = 0, col = 0;
};

int registerFile(const std::string& name);
std::string posStr(const Pos& p);

struct CompileError : std::runtime_error {
    Pos pos;
    CompileError(Pos p, const std::string& m) : std::runtime_error(m), pos(p) {}
};

[[noreturn]] inline void fail(const Pos& p, const std::string& m) { throw CompileError(p, m); }

// Danh sách lỗi toàn cục: lexer, parser và codegen ghi lỗi vào đây rồi đi tiếp,
// để báo hết mọi lỗi trong một lần biên dịch thay vì dừng ở lỗi đầu tiên.
void reportError(const CompileError& e);
std::vector<CompileError>& allErrors();

// ---------------- Token ----------------
enum class T { Ident, Int, Float, Str, Op, Eof };

struct Token {
    T kind = T::Eof;
    std::string text;  // ident / op / nội dung chuỗi đã giải mã escape
    int64_t ival = 0;
    double fval = 0;
    Pos pos;
    bool nl = false;  // có xuống dòng ngay trước token này
};

std::vector<Token> lex(const std::string& src, int fileId);

// ---------------- AST ----------------
enum class EK { Int, Float, Str, Nil, Bool, Var, Field, Index, Call, Method, Unary, Binary, Random, Input, Chord, List, Sys };

struct Expr;
using ExprP = std::unique_ptr<Expr>;

struct Expr {
    EK k = EK::Nil;
    Pos pos;
    int64_t i = 0;       // Int / Bool
    double f = 0;        // Float
    std::string s;       // tên / toán tử / chuỗi / dãy byte của phím tắt
    std::vector<ExprP> a;  // con
};

struct Stmt;
using StmtP = std::unique_ptr<Stmt>;
struct Block {
    std::vector<StmtP> s;
};

enum class SK { Decl, Assign, ExprS, Msg, If, While, Break, Continue, Return, Exit, Nested };

struct Stmt {
    SK k = SK::ExprS;
    Pos pos;
    std::string name;           // Decl: tên biến
    std::string type;           // Decl: int|float|string|drive
    bool db = false;            // Decl: có từ khoá db
    std::vector<ExprP> list;    // Decl: danh sách khởi tạo
    ExprP target;               // Assign: vế trái
    ExprP e;                    // biểu thức chính (rhs / cond / msg / return / exit)
    std::string color;          // Msg
    std::vector<ExprP> conds;   // If: các điều kiện
    std::vector<Block> blocks;  // If: các nhánh (conds.size() hoặc +1 nếu có else)
    bool whileForever = false;  // while (true): chạy vô điều kiện, không break được
    Block body;                 // While / Nested
};

struct FuncDef {
    Pos pos;
    std::string name;
    std::vector<std::string> params;
    Block body;
};

struct ObjField {
    Pos pos;
    std::string name;
    bool priv = false;
    ExprP init;
};

struct ObjDef {
    Pos pos;
    std::string name;
    std::vector<ObjField> fields;
};

struct ImportDecl {
    Pos pos;
    std::string path;
};

struct Module {
    int id = 0;
    std::string path;
    bool hasGlobal = false, hasVar = false, hasEntry = false;
    Pos globalPos, varPos, entryPos;
    std::vector<StmtP> globals, vars;
    std::vector<FuncDef> funcs;
    std::vector<ObjDef> objs;
    Block entry;
    std::vector<ImportDecl> imports;
    std::vector<int> depIds;  // id các module mà file này #import (Loader điền)
};

// isRuntime = true: cho phép cú pháp mức thấp của runtime và không bắt buộc global/var/entry
Module parseModule(const std::vector<Token>& toks, bool isRuntime);
