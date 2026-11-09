// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Chương trình này là phần mềm tự do: bạn có thể phân phối lại và/hoặc sửa đổi nó theo các điều khoản của
// Giấy phép Công cộng GNU phiên bản 3 hoặc (tuỳ bạn chọn) bất kỳ phiên bản nào sau đó. Xem file LICENSE.
// Chương trình được phân phối với hy vọng hữu ích nhưng KHÔNG CÓ BẢO HÀNH NÀO.

// 2oLang compiler - bộ mã hoá lệnh x86 dùng chung cho i386 (W=4) và x86_64 (W=8)
// Chỉ dùng các thanh ghi cũ (eax..edi) nên bảng mã giống nhau, x86_64 thêm tiền tố REX.W.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

enum Reg { EAX = 0, ECX = 1, EDX = 2, EBX = 3, ESP = 4, EBP = 5, ESI = 6, EDI = 7 };
enum Cond { CC_B = 2, CC_AE = 3, CC_E = 4, CC_NE = 5, CC_BE = 6, CC_A = 7, CC_P = 0xA, CC_L = 0xC, CC_GE = 0xD, CC_LE = 0xE, CC_G = 0xF };
enum Alu { A_ADD = 0, A_OR = 1, A_AND = 4, A_SUB = 5, A_XOR = 6, A_CMP = 7 };
enum Shift { S_SHL = 4, S_SHR = 5, S_SAR = 7 };

constexpr uint32_t HEAP_SENTINEL = 0xFFFFFFFFu;  // tham chiếu tới vùng heap zerofill (macOS)

struct Asm {
    int W;
    std::vector<uint8_t> code;
    std::vector<int> labelPos;
    struct Fix {
        size_t at;
        int label;
    };
    struct DFix {
        size_t at;
        uint32_t off;
    };
    std::vector<Fix> fixes;
    std::vector<DFix> dfixes;

    explicit Asm(int w) : W(w) {}

    // ---- byte thô ----
    void b(uint8_t x) { code.push_back(x); }
    void d32(uint32_t x) {
        for (int i = 0; i < 4; i++) b((x >> (8 * i)) & 0xFF);
    }
    void d64(uint64_t x) {
        for (int i = 0; i < 8; i++) b((x >> (8 * i)) & 0xFF);
    }
    void rex() {
        if (W == 8) b(0x48);
    }
    void modrmRR(int reg, int rm) { b(0xC0 | (reg << 3) | rm); }
    void modrmMem(int reg, int base, int32_t disp) {
        bool sib = (base == ESP);
        if (disp == 0 && base != EBP) {
            b((reg << 3) | base);
            if (sib) b(0x24);
        } else if (disp >= -128 && disp <= 127) {
            b(0x40 | (reg << 3) | base);
            if (sib) b(0x24);
            b((uint8_t)disp);
        } else {
            b(0x80 | (reg << 3) | base);
            if (sib) b(0x24);
            d32((uint32_t)disp);
        }
    }

    // ---- nhãn ----
    int newLabel() {
        labelPos.push_back(-1);
        return (int)labelPos.size() - 1;
    }
    void bind(int l) { labelPos[l] = (int)code.size(); }
    size_t pos() const { return code.size(); }
    void jmp(int l) {
        b(0xE9);
        fixes.push_back({code.size(), l});
        d32(0);
    }
    void jcc(Cond c, int l) {
        b(0x0F);
        b(0x80 | c);
        fixes.push_back({code.size(), l});
        d32(0);
    }
    void call(int l) {
        b(0xE8);
        fixes.push_back({code.size(), l});
        d32(0);
    }
    void finalizeLabels() {
        for (auto& f : fixes) {
            if (labelPos[f.label] < 0) throw std::runtime_error("nhãn chưa được định nghĩa (lỗi nội bộ)");
            int32_t rel = labelPos[f.label] - (int32_t)(f.at + 4);
            for (int i = 0; i < 4; i++) code[f.at + i] = (rel >> (8 * i)) & 0xFF;
        }
    }
    // codeAddr = địa chỉ ảo của byte đầu tiên của code, dataAddr = địa chỉ ảo vùng data
    void patchData(uint64_t codeAddr, uint64_t dataAddr, uint64_t heapAddr) {
        for (auto& f : dfixes) {
            uint64_t target = (f.off == HEAP_SENTINEL) ? heapAddr : dataAddr + f.off;
            uint64_t v = (W == 8) ? target - (codeAddr + f.at + 4) : target;
            for (int i = 0; i < 4; i++) code[f.at + i] = (v >> (8 * i)) & 0xFF;
        }
    }

    // ---- di chuyển dữ liệu ----
    void movRI(int r, int64_t imm) {
        if (W == 4) {
            b(0xB8 + r);
            d32((uint32_t)imm);
        } else if (imm >= INT32_MIN && imm <= INT32_MAX) {
            b(0x48);
            b(0xC7);
            modrmRR(0, r);
            d32((uint32_t)imm);
        } else {
            b(0x48);
            b(0xB8 + r);
            d64((uint64_t)imm);
        }
    }
    void movRR(int dst, int src) {
        if (dst == src) return;
        rex();
        b(0x89);
        modrmRR(src, dst);
    }
    void load(int r, int base, int32_t disp) {
        rex();
        b(0x8B);
        modrmMem(r, base, disp);
    }
    void store(int base, int32_t disp, int r) {
        rex();
        b(0x89);
        modrmMem(r, base, disp);
    }
    void load32(int r, int base, int32_t disp) {  // mov r32, [m]  (64-bit: xoá nửa cao)
        b(0x8B);
        modrmMem(r, base, disp);
    }
    void store32(int base, int32_t disp, int r) {
        b(0x89);
        modrmMem(r, base, disp);
    }
    void load8(int r, int base, int32_t disp) {  // movzx r32, byte [m]
        b(0x0F);
        b(0xB6);
        modrmMem(r, base, disp);
    }
    void store8(int base, int32_t disp, int r) {  // r phải là eax/ecx/edx/ebx
        b(0x88);
        modrmMem(r, base, disp);
    }
    void lea(int r, int base, int32_t disp) {
        rex();
        b(0x8D);
        modrmMem(r, base, disp);
    }
    // r = địa chỉ vùng data + off
    void leaData(int r, uint32_t off) {
        if (W == 8) {
            b(0x48);
            b(0x8D);
            b((r << 3) | 0x05);  // [rip+disp32]
        } else {
            b(0xB8 + r);
        }
        dfixes.push_back({code.size(), off});
        d32(0);
    }

    // ---- số học ----
    void alu(Alu op, int dst, int src) {
        rex();
        b((op << 3) | 1);
        modrmRR(src, dst);
    }
    void aluI(Alu op, int r, int32_t imm) {
        rex();
        if (imm >= -128 && imm <= 127) {
            b(0x83);
            modrmRR(op, r);
            b((uint8_t)imm);
        } else {
            b(0x81);
            modrmRR(op, r);
            d32((uint32_t)imm);
        }
    }
    void imul(int dst, int src) {
        rex();
        b(0x0F);
        b(0xAF);
        modrmRR(dst, src);
    }
    void neg(int r) { rex(); b(0xF7); modrmRR(3, r); }
    void notr(int r) { rex(); b(0xF7); modrmRR(2, r); }
    void idiv(int r) { rex(); b(0xF7); modrmRR(7, r); }
    void cdq() { rex(); b(0x99); }  // cdq / cqo
    void shiftCL(Shift s, int r) { rex(); b(0xD3); modrmRR(s, r); }
    void test(int a, int c) { rex(); b(0x85); modrmRR(c, a); }
    void setcc(Cond c) { b(0x0F); b(0x90 | c); b(0xC0); }  // set<c> al
    void movzxAl() { b(0x0F); b(0xB6); b(0xC0); }          // movzx eax, al

    // ---- ngăn xếp / điều khiển ----
    void push(int r) { b(0x50 + r); }
    void pop(int r) { b(0x58 + r); }
    void pushI(int32_t imm) { b(0x68); d32((uint32_t)imm); }
    void ret() { b(0xC3); }
    void syscall64() { b(0x0F); b(0x05); }
    void int80() { b(0xCD); b(0x80); }
    void rdtsc() { b(0x0F); b(0x31); }

    // ---- SSE2 (double, thao tác trên bộ nhớ) ----
    // op: 0x10 movsd load, 0x11 movsd store, 0x58 add, 0x59 mul, 0x5C sub, 0x5E div
    void sd(uint8_t op, int base) { b(0xF2); b(0x0F); b(op); modrmMem(0, base, 0); }
    void ucomisd(int base) { b(0x66); b(0x0F); b(0x2E); modrmMem(0, base, 0); }
    void cvtsi2sd(int r) { b(0xF2); rex(); b(0x0F); b(0x2A); modrmRR(0, r); }       // xmm0 = (double)r
    void cvttsd2si(int r, int base) { b(0xF2); rex(); b(0x0F); b(0x2C); modrmMem(r, base, 0); }  // r = (word)[base]

    // ---- x87 ----
    // *edx = round(*ecx) theo chế độ mode (0 gần nhất, 1 xuống, 2 lên, 3 cắt phần lẻ); dùng eax làm tạm
    void fround(int mode) {
        aluI(A_SUB, ESP, 8);
        b(0xD9); modrmMem(7, ESP, 0);                 // fnstcw [esp]
        b(0x0F); b(0xB7); modrmMem(EAX, ESP, 0);      // movzx eax, word [esp]
        aluI(A_AND, EAX, 0xF3FF);
        aluI(A_OR, EAX, mode << 10);
        b(0x66); b(0x89); modrmMem(EAX, ESP, 2);      // mov [esp+2], ax
        b(0xD9); modrmMem(5, ESP, 2);                 // fldcw [esp+2]
        b(0xDD); modrmMem(0, ECX, 0);                 // fld qword [ecx]
        b(0xD9); b(0xFC);                             // frndint
        b(0xDD); modrmMem(3, EDX, 0);                 // fstp qword [edx]
        b(0xD9); modrmMem(5, ESP, 0);                 // fldcw [esp]
        aluI(A_ADD, ESP, 8);
    }
    // *edx = (*eax) ^ (*ecx) với số mũ thực
    void fpow() {
        b(0xDD); modrmMem(0, ECX, 0);  // fld y
        b(0xDD); modrmMem(0, EAX, 0);  // fld x
        b(0xD9); b(0xF1);              // fyl2x
        b(0xD9); b(0xC0);              // fld st0
        b(0xD9); b(0xFC);              // frndint
        b(0xDC); b(0xE9);              // fsub st1, st0
        b(0xD9); b(0xC9);              // fxch st1
        b(0xD9); b(0xF0);              // f2xm1
        b(0xD9); b(0xE8);              // fld1
        b(0xDE); b(0xC1);              // faddp st1, st0
        b(0xD9); b(0xFD);              // fscale
        b(0xDD); b(0xD9);              // fstp st1
        b(0xDD); modrmMem(3, EDX, 0);  // fstp qword [edx]
    }
};
