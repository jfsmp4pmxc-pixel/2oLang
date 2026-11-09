// 2oLang compiler - mã nguồn runtime (phương ngữ raw) được biên dịch cùng mọi chương trình
#include <string>

std::string runtimeSource() {
    return std::string(
#include "rt_core.inc"
               ) +
           std::string(
#include "rt_io.inc"
           );
}
