// 2oLang compiler - chương trình chính
//   ./2oLang <file.2o> '-(<mục tiêu>)->' <file đầu ra>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "gen.hpp"

std::string runtimeSource();

static const char* kVersion = "2oLang 1.0";

static std::map<int, std::string> g_src;       // id file -> nội dung (để in dòng lỗi)
static std::map<int, std::string> g_names;     // id file -> đường dẫn

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static std::string dirOf(const std::string& p) {
    size_t k = p.find_last_of('/');
    return k == std::string::npos ? "." : p.substr(0, k);
}

static std::string canon(const std::string& p) {
    char buf[4096];
    if (realpath(p.c_str(), buf)) return buf;
    return p;
}

static void printError(const CompileError& e) {
    std::cerr << posStr(e.pos) << ": lỗi: " << e.what() << "\n";
    auto it = g_src.find(e.pos.file);
    if (it != g_src.end() && e.pos.line > 0) {
        std::istringstream in(it->second);
        std::string line;
        for (int i = 1; i <= e.pos.line && std::getline(in, line); i++) {
            if (i == e.pos.line) {
                std::cerr << "  " << line << "\n  ";
                // đếm cột theo ký tự UTF-8 để dấu ^ đúng chỗ
                int col = 1;
                for (size_t k = 0; k < line.size() && col < e.pos.col; k++) {
                    if (((unsigned char)line[k] & 0xC0) != 0x80) {
                        std::cerr << (line[k] == '\t' ? '\t' : ' ');
                        col++;
                    }
                    while (k + 1 < line.size() && ((unsigned char)line[k + 1] & 0xC0) == 0x80) k++;
                }
                std::cerr << "^\n";
            }
        }
    }
}

struct Loader {
    std::vector<std::unique_ptr<Module>> owned;
    std::vector<Module*> order;  // phụ thuộc trước, module chính cuối
    std::set<std::string> done, stack;

    Module* load(const std::string& path, const Pos& from, bool isMain) {
        std::string key = canon(path);
        if (stack.count(key)) fail(from, "#import vòng tròn: " + path);
        if (done.count(key)) return nullptr;
        std::string src;
        if (!readFile(path, src)) {
            if (isMain) throw std::runtime_error("không đọc được file: " + path);
            fail(from, "không tìm thấy file import: " + path);
        }
        int id = registerFile(path);
        g_src[id] = src;
        g_names[id] = path;
        stack.insert(key);
        auto toks = lex(src, id);
        auto m = std::make_unique<Module>(parseModule(toks, false));
        m->id = id;
        m->path = path;
        for (auto& imp : m->imports) {
            std::string p = imp.path;
            std::string cand = dirOf(path) + "/" + p;
            std::string tmp;
            if (!p.empty() && p[0] != '/' && !readFile(cand, tmp)) cand = p;  // thử thêm theo thư mục hiện tại
            else if (!p.empty() && p[0] == '/') cand = p;
            load(cand, imp.pos, false);
        }
        stack.erase(key);
        done.insert(key);
        Module* raw = m.get();
        owned.push_back(std::move(m));
        order.push_back(raw);
        return raw;
    }
};

static void usage() {
    std::cout << kVersion << " - trình biên dịch 2oLang (.2o -> mã máy)\n\n"
              << "Cú pháp:\n"
              << "  2oLang <file.2o> '-(<mục tiêu>)->' <file đầu ra>\n"
              << "  2oLang <file.2o> -t <mục tiêu> -o <file đầu ra>\n\n"
              << "Mục tiêu: " << targetList() << "\n"
              << "  Alpine32bits / Alpine64bits : ELF cho Linux (Alpine, Ubuntu, ...) i386 / x86_64\n"
              << "  MacOS                       : Mach-O x86_64\n"
              << "  i386 / x86_64               : mã máy thô (flat binary, dùng syscall Linux)\n\n"
              << "Lưu ý: trong shell hãy đặt '-(...)->' trong dấu nháy vì ( ) và > có nghĩa đặc biệt.\n"
              << "Tuỳ chọn: --org <địa chỉ> (địa chỉ nạp của mã máy thô), --list-targets, --version, -h\n";
}

int main(int argc, char** argv) {
    std::string input, output, targetName;
    bool haveOrg = false;
    uint64_t org = 0;
    std::vector<std::string> pos;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        if (a == "--version") { std::cout << kVersion << "\n"; return 0; }
        if (a == "--list-targets") { std::cout << targetList() << "\n"; return 0; }
        if ((a == "-t" || a == "--target") && i + 1 < argc) { targetName = argv[++i]; continue; }
        if ((a == "-o" || a == "--output") && i + 1 < argc) { output = argv[++i]; continue; }
        if (a == "--org" && i + 1 < argc) { org = std::stoull(argv[++i], nullptr, 0); haveOrg = true; continue; }
        if (a.size() > 4 && a.rfind("-(", 0) == 0 && a.substr(a.size() - 3) == ")->") {
            targetName = a.substr(2, a.size() - 5);
            continue;
        }
        if (a.size() > 3 && a.rfind("-(", 0) == 0 && a.substr(a.size() - 2) == ")-") {  // phòng khi '>' bị shell nuốt
            targetName = a.substr(2, a.size() - 4);
            continue;
        }
        pos.push_back(a);
    }
    if (pos.size() >= 1) input = pos[0];
    if (pos.size() >= 2 && output.empty()) output = pos[1];
    if (pos.size() > 2) {
        std::cerr << "lỗi: thừa tham số '" << pos[2] << "'. Hãy đặt '-(mục tiêu)->' trong dấu nháy.\n";
        return 2;
    }
    if (input.empty()) { usage(); return 2; }
    if (targetName.empty()) targetName = "Alpine64bits";
    Target tg;
    if (!findTarget(targetName, tg)) {
        std::cerr << "lỗi: mục tiêu không hợp lệ '" << targetName << "'. Hỗ trợ: " << targetList() << "\n";
        return 2;
    }
    if (haveOrg) tg.org = org;
    if (output.empty()) {
        output = input;
        size_t k = output.rfind(".2o");
        if (k != std::string::npos && k + 3 == output.size()) output.resize(k);
        else output += ".out";
        if (tg.fmt == Fmt::FLAT) output += ".bin";
    }

    try {
        // runtime
        std::string rtSrc = runtimeSource();
        int rtId = registerFile("<runtime>");
        g_src[rtId] = rtSrc;
        Module rt = parseModule(lex(rtSrc, rtId), true);
        rt.id = rtId;

        Loader ld;
        ld.load(input, Pos{0, 0, 0}, true);
        std::vector<Module*> mods = ld.order;
        std::vector<uint8_t> img = generate(tg, rt, mods);

        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "lỗi: không ghi được file: " << output << "\n";
            return 1;
        }
        out.write((const char*)img.data(), (std::streamsize)img.size());
        out.close();
        if (tg.fmt != Fmt::FLAT) chmod(output.c_str(), 0755);
        std::cout << "OK: " << input << " -> " << output << " [" << targetName << ", " << img.size() << " byte]\n";
    } catch (const CompileError& e) {
        printError(e);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "lỗi: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
