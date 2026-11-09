// 2oLang compiler - mục tiêu biên dịch và ghi file thực thi
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "asm.hpp"

enum class Fmt { ELF, MACHO, FLAT };
enum class OS { LINUX, MAC };

struct Target {
    std::string name;
    int bits = 64;  // 32 hoặc 64
    Fmt fmt = Fmt::ELF;
    OS os = OS::LINUX;
    uint64_t org = 0;  // địa chỉ nạp (flat có thể đổi bằng --org)
};

// Trả false nếu không nhận ra tên mục tiêu
bool findTarget(const std::string& name, Target& out);
std::string targetList();

// Ráp code + data thành nội dung file đầu ra. code[0] phải là điểm vào (_start).
std::vector<uint8_t> buildImage(const Target& t, Asm& as, const std::vector<uint8_t>& data);

// Kích thước heap (bộ nhớ zero-fill sau vùng data, cấp phát kiểu bump)
constexpr uint64_t kHeapSize = 256ull * 1024 * 1024;
