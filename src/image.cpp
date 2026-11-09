// 2oLang compiler - ghi ELF32 / ELF64 / Mach-O x86_64 / flat binary
#include "image.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

struct Out {
    std::vector<uint8_t> v;
    void u8(uint8_t x) { v.push_back(x); }
    void u16(uint16_t x) { for (int i = 0; i < 2; i++) u8((x >> (8 * i)) & 0xFF); }
    void u32(uint32_t x) { for (int i = 0; i < 4; i++) u8((x >> (8 * i)) & 0xFF); }
    void u64(uint64_t x) { for (int i = 0; i < 8; i++) u8((x >> (8 * i)) & 0xFF); }
    void name16(const char* s) {
        char buf[16] = {0};
        strncpy(buf, s, 15);
        for (char c : buf) u8((uint8_t)c);
    }
    void padTo(size_t n) { while (v.size() < n) u8(0); }
    void bytes(const std::vector<uint8_t>& b) { v.insert(v.end(), b.begin(), b.end()); }
};

uint64_t alignUp(uint64_t x, uint64_t a) { return (x + a - 1) / a * a; }

std::vector<uint8_t> buildElf64(const Target& t, Asm& as, const std::vector<uint8_t>& data) {
    const uint64_t hdr = 64 + 56 * 2;
    uint64_t codeOff = hdr, codeEnd = codeOff + as.code.size();
    uint64_t dataOff = alignUp(codeEnd, 0x1000);
    uint64_t dataSpan = alignUp(data.size(), 0x1000);
    as.patchData(t.org + codeOff, t.org + dataOff, t.org + dataOff + dataSpan);
    Out o;
    const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (uint8_t c : ident) o.u8(c);
    o.u16(2); o.u16(62); o.u32(1);                // ET_EXEC, EM_X86_64
    o.u64(t.org + codeOff);                       // entry
    o.u64(64); o.u64(0);                          // phoff, shoff
    o.u32(0); o.u16(64); o.u16(56); o.u16(2);     // flags, ehsize, phentsize, phnum
    o.u16(0); o.u16(0); o.u16(0);                 // shentsize, shnum, shstrndx
    // PT_LOAD #1: header + code (R X)
    o.u32(1); o.u32(5); o.u64(0); o.u64(t.org); o.u64(t.org); o.u64(codeEnd); o.u64(codeEnd); o.u64(0x1000);
    // PT_LOAD #2: data (RW)
    o.u32(1); o.u32(6); o.u64(dataOff); o.u64(t.org + dataOff); o.u64(t.org + dataOff);
    o.u64(data.size()); o.u64(dataSpan + kHeapSize); o.u64(0x1000);
    o.padTo(codeOff);
    o.bytes(as.code);
    o.padTo(dataOff);
    o.bytes(data);
    return o.v;
}

std::vector<uint8_t> buildElf32(const Target& t, Asm& as, const std::vector<uint8_t>& data) {
    const uint64_t hdr = 52 + 32 * 2;
    uint64_t codeOff = alignUp(hdr, 16), codeEnd = codeOff + as.code.size();
    uint64_t dataOff = alignUp(codeEnd, 0x1000);
    uint64_t dataSpan = alignUp(data.size(), 0x1000);
    as.patchData(t.org + codeOff, t.org + dataOff, t.org + dataOff + dataSpan);
    Out o;
    const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (uint8_t c : ident) o.u8(c);
    o.u16(2); o.u16(3); o.u32(1);                 // ET_EXEC, EM_386
    o.u32((uint32_t)(t.org + codeOff));           // entry
    o.u32(52); o.u32(0);                          // phoff, shoff
    o.u32(0); o.u16(52); o.u16(32); o.u16(2);
    o.u16(0); o.u16(0); o.u16(0);
    // p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align
    o.u32(1); o.u32(0); o.u32((uint32_t)t.org); o.u32((uint32_t)t.org);
    o.u32((uint32_t)codeEnd); o.u32((uint32_t)codeEnd); o.u32(5); o.u32(0x1000);
    o.u32(1); o.u32((uint32_t)dataOff); o.u32((uint32_t)(t.org + dataOff)); o.u32((uint32_t)(t.org + dataOff));
    o.u32((uint32_t)data.size()); o.u32((uint32_t)(dataSpan + kHeapSize)); o.u32(6); o.u32(0x1000);
    o.padTo(codeOff);
    o.bytes(as.code);
    o.padTo(dataOff);
    o.bytes(data);
    return o.v;
}

std::vector<uint8_t> buildFlat(const Target& t, Asm& as, const std::vector<uint8_t>& data) {
    uint64_t dataOff = alignUp(as.code.size(), 16);
    as.patchData(t.org, t.org + dataOff, t.org + alignUp(dataOff + data.size(), 16));
    Out o;
    o.bytes(as.code);
    o.padTo(dataOff);
    o.bytes(data);
    return o.v;
}

std::vector<uint8_t> buildMachO(const Target&, Asm& as, const std::vector<uint8_t>& data) {
    const uint64_t base = 0x100000000ull;  // __TEXT bắt đầu ngay sau __PAGEZERO
    const uint32_t nCmds = 5;
    const uint32_t cmdsSize = 72 + (72 + 80) + (72 + 80 * 2) + 184 + 24;
    uint64_t codeOff = alignUp(32 + cmdsSize, 16);
    uint64_t textFileEnd = alignUp(codeOff + as.code.size(), 0x1000);
    uint64_t dataOff = textFileEnd;
    uint64_t dataFileSize = alignUp(data.size(), 0x1000);
    uint64_t heapAddr = base + dataOff + dataFileSize;
    as.patchData(base + codeOff, base + dataOff, heapAddr);

    Out o;
    o.u32(0xFEEDFACF); o.u32(0x01000007); o.u32(3); o.u32(2);  // magic, x86_64, subtype, MH_EXECUTE
    o.u32(nCmds); o.u32(cmdsSize); o.u32(1); o.u32(0);          // MH_NOUNDEFS
    // __PAGEZERO
    o.u32(0x19); o.u32(72); o.name16("__PAGEZERO");
    o.u64(0); o.u64(base); o.u64(0); o.u64(0); o.u32(0); o.u32(0); o.u32(0); o.u32(0);
    // __TEXT
    o.u32(0x19); o.u32(72 + 80); o.name16("__TEXT");
    o.u64(base); o.u64(textFileEnd); o.u64(0); o.u64(textFileEnd); o.u32(7); o.u32(5); o.u32(1); o.u32(0);
    o.name16("__text"); o.name16("__TEXT");
    o.u64(base + codeOff); o.u64(as.code.size()); o.u32((uint32_t)codeOff); o.u32(4);
    o.u32(0); o.u32(0); o.u32(0x80000400); o.u32(0); o.u32(0); o.u32(0);
    // __DATA (+ zerofill làm heap)
    o.u32(0x19); o.u32(72 + 80 * 2); o.name16("__DATA");
    o.u64(base + dataOff); o.u64(dataFileSize + kHeapSize); o.u64(dataOff); o.u64(dataFileSize);
    o.u32(7); o.u32(3); o.u32(2); o.u32(0);
    o.name16("__data"); o.name16("__DATA");
    o.u64(base + dataOff); o.u64(data.size()); o.u32((uint32_t)dataOff); o.u32(4);
    o.u32(0); o.u32(0); o.u32(0); o.u32(0); o.u32(0); o.u32(0);
    o.name16("__bss"); o.name16("__DATA");
    o.u64(heapAddr); o.u64(kHeapSize); o.u32(0); o.u32(4);
    o.u32(0); o.u32(0); o.u32(1); o.u32(0); o.u32(0); o.u32(0);  // S_ZEROFILL
    // LC_UNIXTHREAD
    o.u32(0x5); o.u32(184); o.u32(4); o.u32(42);
    for (int i = 0; i < 21; i++) o.u64(i == 16 ? base + codeOff : 0);
    // LC_BUILD_VERSION
    o.u32(0x32); o.u32(24); o.u32(1); o.u32(0x000A0D00); o.u32(0x000A0D00); o.u32(0);

    o.padTo(codeOff);
    o.bytes(as.code);
    o.padTo(dataOff);
    o.bytes(data);
    o.padTo(dataOff + dataFileSize);
    return o.v;
}

}  // namespace

bool findTarget(const std::string& nameIn, Target& out) {
    std::string n = lower(nameIn);
    for (auto& c : n)
        if (c == '-' || c == ' ' || c == '_') c = '-';
    Target t;
    t.name = n;
    if (n == "alpine32bits" || n == "alpine32" || n == "alpine-32bits" || n == "alpine-32") {
        t.bits = 32; t.fmt = Fmt::ELF; t.os = OS::LINUX; t.org = 0x08048000;
    } else if (n == "alpine64bits" || n == "alpine64" || n == "alpine-64bits" || n == "alpine-64") {
        t.bits = 64; t.fmt = Fmt::ELF; t.os = OS::LINUX; t.org = 0x400000;
    } else if (n == "macos" || n == "mac" || n == "macos-x86-64") {
        t.bits = 64; t.fmt = Fmt::MACHO; t.os = OS::MAC; t.org = 0x100000000ull;
    } else if (n == "i386" || n == "x86" || n == "ma-may-i386") {
        t.bits = 32; t.fmt = Fmt::FLAT; t.os = OS::LINUX; t.org = 0x08048000;
    } else if (n == "x86-64" || n == "x86x64" || n == "x64" || n == "amd64" || n == "ma-may-x86-64") {
        t.bits = 64; t.fmt = Fmt::FLAT; t.os = OS::LINUX; t.org = 0x400000;
    } else {
        return false;
    }
    out = t;
    return true;
}

std::string targetList() { return "Alpine32bits, Alpine64bits, MacOS, i386, x86_64"; }

std::vector<uint8_t> buildImage(const Target& t, Asm& as, const std::vector<uint8_t>& data) {
    as.finalizeLabels();
    switch (t.fmt) {
        case Fmt::ELF: return t.bits == 64 ? buildElf64(t, as, data) : buildElf32(t, as, data);
        case Fmt::MACHO: return buildMachO(t, as, data);
        case Fmt::FLAT: return buildFlat(t, as, data);
    }
    throw std::runtime_error("định dạng không hợp lệ");
}
