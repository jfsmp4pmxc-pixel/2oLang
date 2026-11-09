// 2oLang compiler - sinh mã cho biểu thức (kết quả luôn nằm trong eax)
#include <cmath>

#include "gen.hpp"

void Gen::needLiteral(const Expr& e, const char* what) {
    if (e.k != EK::Int) fail(e.pos, std::string(what) + " phải là hằng số nguyên");
}

// ---------- chế độ raw: toán tử trên số nguyên thô ----------
void Gen::evalRawBinary(const Expr& e) {
    const std::string& op = e.s;
    if (op == "and" || op == "or") {
        int done = as.newLabel();
        eval(*e.a[0]);
        as.test(EAX, EAX);
        as.jcc(op == "and" ? CC_E : CC_NE, done);
        eval(*e.a[1]);
        as.bind(done);
        as.test(EAX, EAX);
        as.setcc(CC_NE);
        as.movzxAl();
        return;
    }
    eval(*e.a[0]);
    as.push(EAX);
    eval(*e.a[1]);
    as.movRR(ECX, EAX);
    as.pop(EAX);
    if (op == "+") as.alu(A_ADD, EAX, ECX);
    else if (op == "-") as.alu(A_SUB, EAX, ECX);
    else if (op == "*") as.imul(EAX, ECX);
    else if (op == "/" || op == "%") {
        as.cdq();
        as.idiv(ECX);
        if (op == "%") as.movRR(EAX, EDX);
    } else {
        Cond c;
        if (op == "==") c = CC_E;
        else if (op == "!=") c = CC_NE;
        else if (op == "<") c = CC_L;
        else if (op == ">") c = CC_G;
        else if (op == "<=") c = CC_LE;
        else if (op == ">=") c = CC_GE;
        else fail(e.pos, "toán tử '" + op + "' không dùng được trong runtime");
        as.alu(A_CMP, EAX, ECX);
        as.setcc(c);
        as.movzxAl();
    }
}

// ---------- các lệnh nội tại của runtime ----------
bool Gen::evalIntrinsic(const Expr& e) {
    const std::string& n = e.s;
    auto argc = [&](size_t k) {
        if (e.a.size() != k) fail(e.pos, n + " cần " + std::to_string(k) + " tham số");
    };
    auto pushAll = [&]() {
        for (auto& a : e.a) {
            eval(*a);
            as.push(EAX);
        }
    };
    const int r1 = (tg.bits == 32) ? EBX : EDI, r2 = (tg.bits == 32) ? ECX : ESI;

    if (n == "rt_load") { argc(1); eval(*e.a[0]); as.load(EAX, EAX, 0); }
    else if (n == "rt_load8") { argc(1); eval(*e.a[0]); as.load8(EAX, EAX, 0); }
    else if (n == "rt_load32") { argc(1); eval(*e.a[0]); as.load32(EAX, EAX, 0); }
    else if (n == "rt_store" || n == "rt_store8" || n == "rt_store32") {
        argc(2);
        eval(*e.a[0]);
        as.push(EAX);
        eval(*e.a[1]);
        as.pop(ECX);
        if (n == "rt_store") as.store(ECX, 0, EAX);
        else if (n == "rt_store8") as.store8(ECX, 0, EAX);
        else as.store32(ECX, 0, EAX);
    } else if (n == "rt_cell") {
        argc(1);
        needLiteral(*e.a[0], "chỉ số ô");
        if (e.a[0]->i < 0 || e.a[0]->i >= 64) fail(e.pos, "rt_cell chỉ có 64 ô");
        as.leaData(EAX, (uint32_t)(e.a[0]->i * 8));
    } else if (n == "rt_sys") {
        argc(4);
        pushAll();
        as.pop(EDX);
        as.pop(r2);
        as.pop(r1);
        as.pop(EAX);
        emitSyscall();
    } else if (n == "rt_os") { argc(0); as.movRI(EAX, tg.os == OS::MAC ? 1 : 0); }
    else if (n == "rt_wordsize") { argc(0); as.movRI(EAX, W); }
    else if (n == "rt_intmax") { argc(0); as.movRI(EAX, intMax()); }
    else if (n == "rt_sysno") {  // 0 read, 1 write, 2 exit, 3 ioctl
        argc(1);
        needLiteral(*e.a[0], "mã syscall");
        static const int lin32[] = {3, 4, 1, 54}, lin64[] = {0, 1, 60, 16}, mac[] = {3, 4, 1, 54};
        int k = (int)e.a[0]->i;
        if (k < 0 || k > 3) fail(e.pos, "rt_sysno: mã không hợp lệ");
        as.movRI(EAX, tg.os == OS::MAC ? mac[k] : (tg.bits == 64 ? lin64[k] : lin32[k]));
    } else if (n == "rt_const") {  // hằng số termios
        argc(1);
        needLiteral(*e.a[0], "mã hằng số");
        bool mac = tg.os == OS::MAC;
        int64_t v[] = {mac ? 0x40487413 : 0x5401,  // 0: lấy termios
                       mac ? 0x80487414 : 0x5402,  // 1: đặt termios
                       mac ? 0x588 : 0x800B,       // 2: ICANON | ECHO | ISIG | IEXTEN
                       mac ? 24 : 12,              // 3: offset c_lflag
                       mac ? 48 : 23,              // 4: offset c_cc[VMIN]
                       mac ? 49 : 22,              // 5: offset c_cc[VTIME]
                       mac ? 0x300 : 0x500};       // 6: ICRNL | IXON (c_iflag, offset 0)
        int k = (int)e.a[0]->i;
        if (k < 0 || k > 6) fail(e.pos, "rt_const: mã không hợp lệ");
        as.movRI(EAX, v[k]);
    } else if (n == "rt_and" || n == "rt_or" || n == "rt_xor" || n == "rt_shl" || n == "rt_shr") {
        argc(2);
        eval(*e.a[0]);
        as.push(EAX);
        eval(*e.a[1]);
        as.movRR(ECX, EAX);
        as.pop(EAX);
        if (n == "rt_and") as.alu(A_AND, EAX, ECX);
        else if (n == "rt_or") as.alu(A_OR, EAX, ECX);
        else if (n == "rt_xor") as.alu(A_XOR, EAX, ECX);
        else as.shiftCL(n == "rt_shl" ? S_SHL : S_SHR, EAX);
    } else if (n == "rt_rdtsc") {
        argc(0);
        as.rdtsc();  // eax = 32 bit thấp (x86_64 tự xoá nửa cao của rax)
    } else if (n == "rt_fop") {
        argc(4);
        needLiteral(*e.a[0], "phép toán");
        static const uint8_t ops[] = {0x58, 0x5C, 0x59, 0x5E};
        int k = (int)e.a[0]->i;
        if (k < 0 || k > 3) fail(e.pos, "rt_fop: phép toán 0..3");
        for (size_t i = 1; i < 4; i++) {
            eval(*e.a[i]);
            as.push(EAX);
        }
        as.pop(ECX);  // b
        as.pop(EAX);  // a
        as.pop(EDX);  // dst
        as.sd(0x10, EAX);
        as.sd(ops[k], ECX);
        as.sd(0x11, EDX);
    } else if (n == "rt_fcmp") {
        argc(2);
        pushAll();
        as.pop(ECX);
        as.pop(EAX);
        as.sd(0x10, EAX);
        as.ucomisd(ECX);
        int done = as.newLabel();
        as.movRI(EAX, 3);
        as.jcc(CC_P, done);
        as.movRI(EAX, 0);
        as.jcc(CC_E, done);
        as.movRI(EAX, 1);
        as.jcc(CC_B, done);
        as.movRI(EAX, 2);
        as.bind(done);
    } else if (n == "rt_i2f") {
        argc(2);
        eval(*e.a[0]);
        as.push(EAX);
        eval(*e.a[1]);
        as.pop(EDX);
        as.cvtsi2sd(EAX);
        as.sd(0x11, EDX);
    } else if (n == "rt_f2i") {
        argc(1);
        eval(*e.a[0]);
        as.cvttsd2si(EAX, EAX);
    } else if (n == "rt_fround") {
        argc(3);
        needLiteral(*e.a[2], "chế độ làm tròn");
        eval(*e.a[0]);
        as.push(EAX);
        eval(*e.a[1]);
        as.movRR(ECX, EAX);
        as.pop(EDX);
        as.fround((int)e.a[2]->i);
    } else if (n == "rt_fpow") {
        argc(3);
        pushAll();
        as.pop(ECX);
        as.pop(EAX);
        as.pop(EDX);
        as.fpow();
    } else {
        return false;
    }
    return true;
}

// ---------- gọi hàm ----------
void Gen::evalCall(const Expr& e) {
    if (cx.raw && evalIntrinsic(e)) return;
    if (!cx.raw && e.s.rfind("rt_", 0) == 0) fail(e.pos, "tên rt_* dành riêng cho runtime");
    auto it = fns.find(e.s);
    if (it == fns.end() || !it->second.defined) fail(e.pos, "hàm '" + e.s + "' chưa được định nghĩa");
    if (it->second.arity != e.a.size())
        fail(e.pos, "hàm '" + e.s + "' cần " + std::to_string(it->second.arity) + " tham số nhưng được gọi với " + std::to_string(e.a.size()));
    for (auto& a : e.a) {
        eval(*a);
        as.push(EAX);
    }
    as.call(it->second.label);
    if (!e.a.empty()) as.aluI(A_ADD, ESP, (int32_t)(e.a.size() * W));
}

// ---------- biểu thức ----------
void Gen::eval(const Expr& e) {
    switch (e.k) {
        case EK::Int:
            if (cx.raw) {
                as.movRI(EAX, e.i);
            } else if (e.i > intMax() || e.i < -intMax() - 1) {
                as.leaData(EAX, floatObj((double)e.i));
            } else {
                as.movRI(EAX, (e.i << 1) | 1);
            }
            return;
        case EK::Float:
            if (cx.raw) fail(e.pos, "số thực không dùng được trong runtime");
            as.leaData(EAX, floatObj(e.f));
            return;
        case EK::Str:
            as.leaData(EAX, cx.raw ? cstr(e.s) : strObj(e.s));
            return;
        case EK::Nil: as.movRI(EAX, 0); return;
        case EK::Bool: as.movRI(EAX, cx.raw ? e.i : (e.i ? 4 : 2)); return;
        case EK::Var: loadVar(e); return;
        case EK::Field: {
            auto f = objField(e, false);
            as.leaData(ECX, f.first);
            as.load(EAX, ECX, 0);
            return;
        }
        case EK::Index: callRtWith("rt_index", {e.a[0].get(), e.a[1].get()}); return;
        case EK::Call: evalCall(e); return;
        case EK::Method: {
            const Expr& recv = *e.a[0];
            if (e.s == "add") {
                if (e.a.size() != 2 && e.a.size() != 3) fail(e.pos, "add(giá_trị) hoặc add(giá_trị, vị_trí)");
                eval(recv);
                as.push(EAX);
                eval(*e.a[1]);
                as.push(EAX);
                if (e.a.size() == 3) eval(*e.a[2]);
                else as.movRI(EAX, 0);
                as.push(EAX);
                callRt("rt_drive_add", 3);
            } else if (e.s == "rm") {
                if (e.a.size() != 2) fail(e.pos, "rm(giá_trị) hoặc rm(số_thứ_tự)");
                callRtWith("rt_drive_rm", {&recv, e.a[1].get()});
            } else {
                fail(e.pos, "drive không có phương thức '" + e.s + "' (chỉ có add và rm)");
            }
            return;
        }
        case EK::Unary: {
            if (cx.raw) {
                eval(*e.a[0]);
                if (e.s == "neg") as.neg(EAX);
                else if (e.s == "not") {
                    as.test(EAX, EAX);
                    as.setcc(CC_E);
                    as.movzxAl();
                } else fail(e.pos, "toán tử '" + e.s + "' không dùng được trong runtime");
                return;
            }
            static const std::map<std::string, std::string> fn = {{"neg", "rt_neg"}, {"not", "rt_not"}, {"len", "rt_len"}, {"element", "rt_element"}};
            callRtWith(fn.at(e.s), {e.a[0].get()});
            return;
        }
        case EK::Binary: {
            if (cx.raw) {
                evalRawBinary(e);
                return;
            }
            if (e.s == "and" || e.s == "or") {
                int falseL = as.newLabel(), done = as.newLabel();
                if (e.s == "and") {
                    jumpIfFalse(*e.a[0], falseL);
                    jumpIfFalse(*e.a[1], falseL);
                } else {
                    int trueL = as.newLabel();
                    eval(*e.a[0]);
                    as.push(EAX);
                    callRt("rt_truthy", 1);
                    as.test(EAX, EAX);
                    as.jcc(CC_NE, trueL);
                    jumpIfFalse(*e.a[1], falseL);
                    as.bind(trueL);
                }
                as.movRI(EAX, 4);
                as.jmp(done);
                as.bind(falseL);
                as.movRI(EAX, 2);
                as.bind(done);
                return;
            }
            static const std::map<std::string, std::string> fn = {{"+", "rt_add"}, {"-", "rt_sub"}, {"*", "rt_mul"}, {"/", "rt_div"},
                                                                  {"%", "rt_mod"}, {"^", "rt_pow"}, {"cat", "rt_cat"}, {"==", "rt_eq"},
                                                                  {"!=", "rt_ne"}, {"<", "rt_lt"},  {">", "rt_gt"},   {"<=", "rt_le"},
                                                                  {">=", "rt_ge"}};
            callRtWith(fn.at(e.s), {e.a[0].get(), e.a[1].get()});
            return;
        }
        case EK::Random:
            if (e.a.empty()) {
                callRt("rt_random0", 0);
            } else {
                callRtWith("rt_random", {e.a[0].get(), e.a[1].get()});
            }
            return;
        case EK::Input:
            if (e.a.empty()) callRt("rt_input_line", 0);
            else callRtWith("rt_input_match", {e.a[0].get()});
            return;
        case EK::Chord:
            as.leaData(EAX, strObj(e.s));
            as.push(EAX);
            callRt("rt_input_key", 1);
            return;
        case EK::List: {
            as.call(fns.at("rt_drive_new").label);
            for (auto& x : e.a) {
                as.push(EAX);
                eval(*x);
                as.push(EAX);
                callRt("rt_drive_push", 2);
            }
            return;
        }
    }
}
