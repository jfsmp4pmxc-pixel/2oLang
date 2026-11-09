// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Chương trình này là phần mềm tự do: bạn có thể phân phối lại và/hoặc sửa đổi nó theo các điều khoản của
// Giấy phép Công cộng GNU phiên bản 3 hoặc (tuỳ bạn chọn) bất kỳ phiên bản nào sau đó. Xem file LICENSE.
// Chương trình được phân phối với hy vọng hữu ích nhưng KHÔNG CÓ BẢO HÀNH NÀO.

// 2oLang compiler - bộ sinh mã máy trực tiếp từ AST
//
// Biểu diễn giá trị lúc chạy (một từ máy: 4 byte trên i386, 8 byte trên x86_64):
//   bit thấp = 1   : số nguyên (n << 1 | 1), tràn thì tự đổi sang số thực
//   0              : nil
//   2 / 4          : false / true
//   con trỏ chẵn   : đối tượng heap, từ đầu là mã kiểu (1 float, 2 string, 3 drive)
#pragma once
#include <map>
#include <set>

#include "common.hpp"
#include "image.hpp"

// mods: thứ tự phụ thuộc, module chính nằm cuối. rt: module runtime (chế độ "raw").
std::vector<uint8_t> generate(const Target& t, Module& rt, std::vector<Module*>& mods);

struct FnInfo {
    int label = -1;
    size_t arity = 0;
    bool raw = false;
    bool defined = false;
    const FuncDef* def = nullptr;
    int mod = 0;
    Pos pos;
};

struct ObjInfo {
    Pos pos;
    std::map<std::string, std::pair<uint32_t, bool>> fields;  // tên -> (offset data, private)
};

struct LoopCtx {
    int brk, cont;
    bool forever;
};

struct Ctx {
    bool raw = false;
    bool isEntry = false;
    int mod = 0;
    std::map<std::string, int32_t> slots;  // tên -> offset so với ebp
    std::vector<LoopCtx> loops;
    int retLabel = 0;
};

struct Gen {
    const Target& tg;
    int W;
    Asm as;
    std::vector<uint8_t> data;
    std::map<std::string, FnInfo> fns;
    std::map<std::string, ObjInfo> objs;
    std::map<std::string, uint32_t> vars;  // "g:ten" hoặc "<mod>:ten"
    std::map<std::string, uint32_t> strPool, cstrPool;
    std::map<uint64_t, uint32_t> floatPool;
    Ctx cx;

    explicit Gen(const Target& t) : tg(t), W(t.bits / 8), as(t.bits / 8) {}

    // ---- vùng data ----
    uint32_t dataAlloc(size_t n);
    void putWord(uint32_t off, uint64_t v, int bytes);
    uint32_t strObj(const std::string& s);
    uint32_t cstr(const std::string& s);
    uint32_t floatObj(double d);
    int64_t intMax() const { return W == 8 ? (int64_t(1) << 62) - 1 : (int64_t(1) << 30) - 1; }

    // ---- hàm / biến ----
    int fnLabel(const std::string& name);
    void registerFn(const FuncDef& f, int mod, bool raw);
    bool resolveVar(const std::string& name, int32_t& off, bool& local) const;
    void loadVar(const Expr& e);
    void storeVar(const Expr& e);
    std::pair<uint32_t, bool> objField(const Expr& e, bool forWrite);

    // ---- phát mã ----
    void emitFunction(const FnInfo& fi, const FuncDef& f);
    void emitMain(Module& mainMod, std::vector<Module*>& mods);
    void emitDeclInit(const Stmt& d, int mod, bool global);
    void collectAssigned(const Block& b, std::vector<std::string>& out, std::set<std::string>& seen);
    void emitBlock(const Block& b);
    void emitStmt(const Stmt& s);
    void eval(const Expr& e);
    void evalCall(const Expr& e);
    bool evalIntrinsic(const Expr& e);
    void evalRawBinary(const Expr& e);
    void callRt(const std::string& name, int nargs);
    void callRtWith(const std::string& name, const std::vector<const Expr*>& args);
    void jumpIfFalse(const Expr& e, int label);
    void emitSyscall();
    void emitExit();
    void needLiteral(const Expr& e, const char* what);
};
