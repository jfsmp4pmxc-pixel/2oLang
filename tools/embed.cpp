// Copyright (C) 2026 những người đóng góp 2oLang
// SPDX-License-Identifier: GPL-3.0-or-later
// Công cụ build: nhúng mọi file Lib/*.2o vào trình biên dịch (sinh ra src/twolib_data.inc).
// Dùng: embed <thư mục Lib> <file .inc đầu ra>
#include <dirent.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "dùng: embed <Lib> <out.inc>\n"; return 2; }
    std::vector<std::string> names;
    if (DIR* d = opendir(argv[1])) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 3 && n.compare(n.size() - 3, 3, ".2o") == 0) names.push_back(n);
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    std::ofstream out(argv[2], std::ios::trunc);
    out << "// Sinh tự động bởi tools/embed.cpp - đừng sửa tay\n";
    for (auto& n : names) {
        std::ifstream f(std::string(argv[1]) + "/" + n, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        out << "{\"" << n.substr(0, n.size() - 3) << "\", {";
        for (unsigned char c : s) out << (int)(signed char)c << ",";
        out << "0}},\n";
    }
    return out ? 0 : 1;
}
