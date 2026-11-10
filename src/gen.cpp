// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Chương trình này là phần mềm tự do: bạn có thể phân phối lại và/hoặc sửa đổi nó theo các điều khoản của
// Giấy phép Công cộng GNU phiên bản 3 hoặc (tuỳ bạn chọn) bất kỳ phiên bản nào sau đó. Xem file LICENSE.
// Chương trình được phân phối với hy vọng hữu ích nhưng KHÔNG CÓ BẢO HÀNH NÀO.

// 2oLang compiler - sinh mã: vùng data, hàm, câu lệnh, chương trình chính
#include "gen.hpp"

#include <cstring>

static const uint32_t kCells = 64 * 8;  // 64 ô runtime ở đầu vùng data

// Chạy f; nếu có lỗi biên dịch thì ghi lại rồi đi tiếp, để báo hết lỗi một lượt.
template <class F>
static void guarded(F f) {
    try {
        f();
    } catch (CompileError& e) {
        reportError(e);
    }
}

// ============================ vùng data ============================
uint32_t Gen::dataAlloc(size_t n) {
    size_t off = (data.size() + 7) & ~size_t(7);
    data.resize(off + (n ? n : 1), 0);
    return (uint32_t)off;
}

void Gen::putWord(uint32_t off, uint64_t v, int bytes) {
    for (int i = 0; i < bytes; i++) data[off + i] = (v >> (8 * i)) & 0xFF;
}

uint32_t Gen::strObj(const std::string& s) {
    auto it = strPool.find(s);
    if (it != strPool.end()) return it->second;
    uint32_t off = dataAlloc(2 * W + s.size() + 1);
    putWord(off, 2, W);
    putWord(off + W, s.size(), W);
    memcpy(&data[off + 2 * W], s.data(), s.size());
    strPool[s] = off;
    return off;
}

uint32_t Gen::cstr(const std::string& s) {
    auto it = cstrPool.find(s);
    if (it != cstrPool.end()) return it->second;
    uint32_t off = dataAlloc(s.size() + 1);
    memcpy(&data[off], s.data(), s.size());
    cstrPool[s] = off;
    return off;
}

uint32_t Gen::floatObj(double d) {
    uint64_t bits;
    memcpy(&bits, &d, 8);
    auto it = floatPool.find(bits);
    if (it != floatPool.end()) return it->second;
    uint32_t off = dataAlloc(16);
    putWord(off, 1, W);
    putWord(off + 8, bits, 8);
    floatPool[bits] = off;
    return off;
}

// ============================ hàm / biến ============================
int Gen::fnLabel(const std::string& name) {
    FnInfo& f = fns[name];
    if (f.label < 0) f.label = as.newLabel();
    return f.label;
}

void Gen::registerFn(const FuncDef& f, int mod, bool raw) {
    if (!raw && f.name.rfind("rt_", 0) == 0) fail(f.pos, "tên hàm '" + f.name + "' bắt đầu bằng rt_ là phần dành riêng cho runtime");
    auto it = fns.find(f.name);
    if (it != fns.end() && it->second.defined)
        fail(f.pos, "hàm '" + f.name + "' đã được định nghĩa ở " + posStr(it->second.pos));
    FnInfo& fi = fns[f.name];
    fnLabel(f.name);
    fi.arity = f.params.size();
    fi.raw = raw;
    fi.defined = true;
    fi.def = &f;
    fi.mod = mod;
    fi.pos = f.pos;
}

// Thứ tự tìm: biến cục bộ/tham số -> var của chính file -> global -> var của các file được #import.
int Gen::resolveVar(const std::string& name, int32_t& off, bool& local, std::string* ambig) const {
    auto it = cx.slots.find(name);
    if (it != cx.slots.end()) {
        off = it->second;
        local = true;
        return 1;
    }
    local = false;
    auto v = vars.find(std::to_string(cx.mod) + ":" + name);
    if (v == vars.end()) v = vars.find("g:" + name);
    if (v != vars.end()) {
        off = (int32_t)v->second;
        return 1;
    }
    // chưa thấy: tìm trong các file import (trực tiếp rồi gián tiếp)
    std::vector<int> queue{cx.mod};
    std::set<int> seen{cx.mod};
    std::vector<std::pair<int, uint32_t>> found;
    for (size_t qi = 0; qi < queue.size(); qi++) {
        auto d = deps.find(queue[qi]);
        if (d == deps.end()) continue;
        for (int c : d->second) {
            if (!seen.insert(c).second) continue;
            queue.push_back(c);
            auto cv = vars.find(std::to_string(c) + ":" + name);
            if (cv != vars.end()) found.push_back({c, cv->second});
        }
    }
    if (found.empty()) return 0;
    if (found.size() > 1) {
        if (ambig) {
            for (auto& f : found) {
                auto p = modPath.find(f.first);
                *ambig += (ambig->empty() ? "" : ", ") + (p != modPath.end() ? p->second : std::to_string(f.first));
            }
        }
        return 2;
    }
    off = (int32_t)found[0].second;
    return 1;
}

void Gen::loadVar(const Expr& e) {
    int32_t off;
    bool local;
    std::string amb;
    int found = resolveVar(e.s, off, local, &amb);
    if (found == 2) fail(e.pos, "biến '" + e.s + "' mơ hồ: được khai báo trong nhiều file import (" + amb + "); hãy đổi tên một trong số đó");
    if (!found) {
        if (objs.count(e.s)) fail(e.pos, "'" + e.s + "' là đối tượng, hãy truy xuất trường: " + e.s + ".ten_truong");
        fail(e.pos, "biến '" + e.s + "' chưa được khai báo");
    }
    if (local) {
        as.load(EAX, EBP, off);
    } else {
        as.leaData(ECX, (uint32_t)off);
        as.load(EAX, ECX, 0);
    }
}

void Gen::storeVar(const Expr& e) {  // giá trị đang ở eax
    int32_t off;
    bool local;
    std::string amb;
    int found = resolveVar(e.s, off, local, &amb);
    if (found == 2) fail(e.pos, "biến '" + e.s + "' mơ hồ: được khai báo trong nhiều file import (" + amb + "); hãy đổi tên một trong số đó");
    if (!found) fail(e.pos, "biến '" + e.s + "' chưa được khai báo");
    if (local) {
        as.store(EBP, off, EAX);
    } else {
        as.leaData(ECX, (uint32_t)off);
        as.store(ECX, 0, EAX);
    }
}

std::pair<uint32_t, bool> Gen::objField(const Expr& e, bool forWrite) {
    const Expr& base = *e.a[0];
    if (base.k != EK::Var) fail(e.pos, "chỉ truy xuất được trường của đối tượng đã khai báo bằng obj");
    auto o = objs.find(base.s);
    if (o == objs.end()) fail(base.pos, "'" + base.s + "' không phải là đối tượng (obj)");
    auto f = o->second.fields.find(e.s);
    if (f == o->second.fields.end()) fail(e.pos, "đối tượng '" + base.s + "' không có trường '" + e.s + "'");
    if (f->second.second && !cx.isEntry)
        fail(e.pos, "trường private '" + base.s + "." + e.s + "' chỉ truy xuất được trong entry");
    if (forWrite && !cx.isEntry)
        fail(e.pos, "chỉ entry mới có quyền sửa trường '" + base.s + "." + e.s + "'");
    return f->second;
}

// ============================ gọi hàm ============================
void Gen::callRt(const std::string& name, int nargs) {
    auto it = fns.find(name);
    if (it == fns.end() || !it->second.defined) throw std::runtime_error("thiếu hàm runtime: " + name);
    as.call(it->second.label);
    if (nargs) as.aluI(A_ADD, ESP, nargs * W);
}

void Gen::callRtWith(const std::string& name, const std::vector<const Expr*>& args) {
    for (auto* a : args) {
        eval(*a);
        as.push(EAX);
    }
    callRt(name, (int)args.size());
}

void Gen::jumpIfFalse(const Expr& e, int label) {
    eval(e);
    if (!cx.raw) {
        as.push(EAX);
        callRt("rt_truthy", 1);
    }
    as.test(EAX, EAX);
    as.jcc(CC_E, label);
}

void Gen::emitSyscall() {
    // eax = số syscall, tham số trong (ebx|edi), (ecx|esi), edx
    if (tg.os == OS::MAC) {
        as.aluI(A_ADD, EAX, 0x2000000);
        as.syscall64();
        int ok = as.newLabel();
        as.jcc(CC_AE, ok);  // CF=0: thành công
        as.neg(EAX);        // lỗi: trả -errno như Linux
        as.bind(ok);
    } else if (tg.bits == 64) {
        as.syscall64();
    } else {
        as.int80();
    }
}

void Gen::emitExit() {  // eax = mã thoát thô
    as.movRR(tg.bits == 32 ? EBX : EDI, EAX);
    as.movRI(EAX, tg.os == OS::MAC ? 1 : (tg.bits == 64 ? 60 : 1));
    emitSyscall();
}

// ============================ hàm người dùng / runtime ============================
void Gen::collectAssigned(const Block& b, std::vector<std::string>& out, std::set<std::string>& seen) {
    for (auto& sp : b.s) {
        const Stmt& s = *sp;
        if (s.k == SK::Assign && s.target->k == EK::Var && seen.insert(s.target->s).second) out.push_back(s.target->s);
        if (s.k == SK::If)
            for (auto& bl : s.blocks) collectAssigned(bl, out, seen);
        if (s.k == SK::While || s.k == SK::Nested) collectAssigned(s.body, out, seen);
    }
}

void Gen::emitFunction(const FnInfo& fi, const FuncDef& f) {
    cx = Ctx{};
    cx.raw = fi.raw;
    cx.mod = fi.mod;
    size_t n = f.params.size();
    for (size_t j = 0; j < n; j++) {
        if (cx.slots.count(f.params[j])) {
            reportError(CompileError(f.pos, "tham số '" + f.params[j] + "' bị trùng tên"));
            continue;
        }
        cx.slots[f.params[j]] = (int32_t)(2 * W + (n - 1 - j) * W);
    }
    std::vector<std::string> locals;
    std::set<std::string> seen;
    collectAssigned(f.body, locals, seen);
    int nl = 0;
    for (auto& name : locals) {
        int32_t off;
        bool loc;
        if (cx.slots.count(name)) continue;
        if (!cx.raw && resolveVar(name, off, loc)) continue;  // gán vào var/global đã có
        cx.slots[name] = -(int32_t)((++nl) * W);
    }
    as.bind(fns[f.name].label);
    as.push(EBP);
    as.movRR(EBP, ESP);
    for (int i = 0; i < nl; i++) as.pushI(0);
    cx.retLabel = as.newLabel();
    emitBlock(f.body);
    as.movRI(EAX, 0);
    as.bind(cx.retLabel);
    as.movRR(ESP, EBP);
    as.pop(EBP);
    as.ret();
}

// ============================ khai báo biến ============================
void Gen::emitDeclInit(const Stmt& d, int mod, bool global) {
    cx.mod = mod;
    std::string key = global ? "g:" + d.name : std::to_string(mod) + ":" + d.name;
    uint32_t off = vars.at(key);
    auto lit = [&](const Expr& e) { return e.k == EK::Int || e.k == EK::Float || e.k == EK::Str || e.k == EK::Bool || e.k == EK::Nil; };
    if (d.type == "drive") {
        as.call(fns.at("rt_drive_new").label);
        for (auto& e : d.list) {
            as.push(EAX);
            eval(*e);
            as.push(EAX);
            callRt("rt_drive_push", 2);
        }
    } else {
        if (d.list.size() > 1) fail(d.pos, "biến " + d.type + " '" + d.name + "' chỉ nhận một giá trị (nhiều giá trị chỉ dùng cho drive)");
        if (d.list.empty()) {
            if (d.type == "int") as.movRI(EAX, 1);
            else if (d.type == "float") as.leaData(EAX, floatObj(0));
            else as.leaData(EAX, strObj(""));
        } else {
            const Expr& e = *d.list[0];
            if (lit(e) && e.k != EK::Nil) {
                bool ok = (d.type == "int" && e.k == EK::Int) || (d.type == "float" && (e.k == EK::Float || e.k == EK::Int)) ||
                          (d.type == "string" && e.k == EK::Str);
                if (!ok) fail(e.pos, "giá trị không khớp kiểu " + d.type + " của biến '" + d.name + "'");
            }
            if (d.type == "float" && e.k == EK::Int) as.leaData(EAX, floatObj((double)e.i));
            else eval(e);
        }
    }
    as.leaData(ECX, off);
    as.store(ECX, 0, EAX);
}

// ============================ chương trình chính ============================
void Gen::emitMain(Module& mainMod, std::vector<Module*>& mods) {
    cx = Ctx{};
    cx.isEntry = true;
    cx.mod = mainMod.id;
    int startLabel = as.newLabel();
    as.bind(startLabel);  // phải nằm ở byte đầu tiên của code
    as.movRI(EBP, 0);
    // khởi tạo heap: cell0 = con trỏ, cell1 = giới hạn
    as.leaData(EAX, HEAP_SENTINEL);
    as.leaData(ECX, 0);
    as.store(ECX, 0, EAX);
    as.aluI(A_ADD, EAX, (int32_t)kHeapSize);
    as.store(ECX, 8, EAX);  // ô 1 nằm ở offset 8 trên cả hai kiến trúc
    as.push(EBP);
    as.movRR(EBP, ESP);
    std::vector<std::string> locals;
    std::set<std::string> seen;
    collectAssigned(mainMod.entry, locals, seen);
    int nl = 0;
    for (auto& name : locals) {
        int32_t off;
        bool loc;
        if (resolveVar(name, off, loc)) continue;
        cx.slots[name] = -(int32_t)((++nl) * W);
    }
    for (int i = 0; i < nl; i++) as.pushI(0);
    cx.retLabel = as.newLabel();
    for (Module* m : mods) {
        for (auto& d : m->globals) guarded([&] { emitDeclInit(*d, m->id, true); });
        for (auto& d : m->vars) guarded([&] { emitDeclInit(*d, m->id, false); });
        for (auto& o : m->objs)
            for (auto& f : o.fields) {
                cx.mod = m->id;
                guarded([&] {
                    auto oi = objs.find(o.name);
                    if (oi == objs.end()) return;
                    auto fi = oi->second.fields.find(f.name);
                    if (fi == oi->second.fields.end()) return;
                    if (f.init) eval(*f.init);
                    else as.movRI(EAX, 0);
                    as.leaData(ECX, fi->second.first);
                    as.store(ECX, 0, EAX);
                });
            }
    }
    cx.mod = mainMod.id;
    emitBlock(mainMod.entry);
    as.bind(cx.retLabel);
    as.movRI(EAX, 0);
    as.push(EAX);
    callRt("rt_exit", 1);
}

// ============================ câu lệnh ============================
void Gen::emitBlock(const Block& b) {
    for (auto& s : b.s) guarded([&] { emitStmt(*s); });
}

void Gen::emitStmt(const Stmt& s) {
    switch (s.k) {
        case SK::Nested: emitBlock(s.body); break;
        case SK::ExprS: eval(*s.e); break;
        case SK::Msg: {
            static const std::map<std::string, int> col = {{"black", 30}, {"red", 31},  {"green", 32}, {"yellow", 33}, {"blue", 34},
                                                           {"magenta", 35}, {"cyan", 36}, {"white", 37}, {"gray", 90}};
            eval(*s.e);
            as.push(EAX);
            as.movRI(EAX, s.color.empty() ? 1 : (int64_t)((col.at(s.color) << 1) | 1));
            as.push(EAX);
            callRt("rt_msg", 2);
            break;
        }
        case SK::Assign: {
            const Expr& t = *s.target;
            if (t.k == EK::Var) {
                eval(*s.e);
                storeVar(t);
            } else if (t.k == EK::Field) {
                auto f = objField(t, true);
                eval(*s.e);
                as.leaData(ECX, f.first);
                as.store(ECX, 0, EAX);
            } else {  // phần tử drive
                eval(*t.a[0]);
                as.push(EAX);
                eval(*t.a[1]);
                as.push(EAX);
                eval(*s.e);
                as.push(EAX);
                callRt("rt_setindex", 3);
            }
            break;
        }
        case SK::If: {
            int end = as.newLabel();
            for (size_t i = 0; i < s.conds.size(); i++) {
                int next = as.newLabel();
                guarded([&] { jumpIfFalse(*s.conds[i], next); });
                emitBlock(s.blocks[i]);
                as.jmp(end);
                as.bind(next);
            }
            if (s.blocks.size() > s.conds.size()) emitBlock(s.blocks.back());
            as.bind(end);
            break;
        }
        case SK::While: {
            int top = as.newLabel(), end = as.newLabel();
            as.bind(top);
            if (s.e) guarded([&] { jumpIfFalse(*s.e, end); });
            cx.loops.push_back({end, top, s.whileForever});
            emitBlock(s.body);
            cx.loops.pop_back();
            as.jmp(top);
            as.bind(end);
            break;
        }
        case SK::Break:
            if (cx.loops.empty()) fail(s.pos, "'break' chỉ dùng được trong vòng lặp while");
            if (cx.loops.back().forever) fail(s.pos, "while (true) chạy vô điều kiện và không thoát được bằng break; dùng exit hoặc return");
            as.jmp(cx.loops.back().brk);
            break;
        case SK::Continue:
            if (cx.loops.empty()) fail(s.pos, "'continue' chỉ dùng được trong vòng lặp while");
            as.jmp(cx.loops.back().cont);
            break;
        case SK::Return:
            if (s.e) eval(*s.e);
            else as.movRI(EAX, 0);
            as.jmp(cx.retLabel);
            break;
        case SK::Exit:
            if (s.e) eval(*s.e);
            else as.movRI(EAX, 0);
            as.push(EAX);
            callRt("rt_exit", 1);
            break;
        case SK::Decl: fail(s.pos, "khai báo biến chỉ được đặt trong global hoặc var");
    }
}

// ============================ generate ============================
std::vector<uint8_t> generate(const Target& t, Module& rt, std::vector<Module*>& mods) {
    Gen g(t);
    g.data.assign(kCells, 0);
    Module& mainMod = *mods.back();
    auto need = [&](bool has, const char* what) {
        if (!has) reportError(CompileError(Pos{mainMod.id, 1, 1}, std::string("thiếu khối bắt buộc: ") + what));
    };
    need(mainMod.hasGlobal, "global { }");
    need(mainMod.hasVar, "var { }");
    need(mainMod.hasEntry, "entry { }");

    for (auto& f : rt.funcs) g.registerFn(f, -1, true);
    for (Module* m : mods) {
        g.deps[m->id] = m->depIds;
        g.modPath[m->id] = m->path;
        for (auto& f : m->funcs) guarded([&] { g.registerFn(f, m->id, false); });
        for (auto& d : m->globals) {
            std::string k = "g:" + d->name;
            if (g.vars.count(k)) {
                reportError(CompileError(d->pos, "biến global '" + d->name + "' bị khai báo hai lần"));
                continue;
            }
            g.vars[k] = g.dataAlloc(g.W);
        }
        for (auto& d : m->vars) {
            std::string k = std::to_string(m->id) + ":" + d->name;
            if (g.vars.count(k)) {
                reportError(CompileError(d->pos, "biến '" + d->name + "' bị khai báo hai lần trong var"));
                continue;
            }
            g.vars[k] = g.dataAlloc(g.W);
        }
        for (auto& o : m->objs) {
            if (g.objs.count(o.name)) {
                reportError(CompileError(o.pos, "đối tượng '" + o.name + "' bị khai báo hai lần"));
                continue;
            }
            ObjInfo oi;
            oi.pos = o.pos;
            for (auto& f : o.fields) {
                if (oi.fields.count(f.name)) {
                    reportError(CompileError(f.pos, "trường '" + f.name + "' bị khai báo hai lần trong '" + o.name + "'"));
                    continue;
                }
                oi.fields[f.name] = {g.dataAlloc(g.W), f.priv};
            }
            g.objs[o.name] = std::move(oi);
        }
    }
    g.emitMain(mainMod, mods);
    for (auto& kv : std::map<std::string, FnInfo>(g.fns)) {
        if (!kv.second.defined) throw std::runtime_error("hàm chưa định nghĩa: " + kv.first);
        guarded([&] { g.emitFunction(kv.second, *kv.second.def); });
    }
    if (!allErrors().empty()) return {};  // có lỗi: không tạo file đầu ra
    return buildImage(t, g.as, g.data);
}
