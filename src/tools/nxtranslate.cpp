#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <map>
#include <filesystem>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <json.hpp>
#include <utf8.h>

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

struct CommandDef {
    const char *name;
    int nparams;
};

// TSC commands and parameter counts
static const CommandDef cmd_table[] = {
    {"AE+", 0}, {"AM+", 2}, {"AM-", 1}, {"AMJ", 2}, {"ANP", 3}, {"BOA", 1}, {"BSL", 1}, {"CAT", 0}, {"CIL", 0},
    {"CLO", 0}, {"CLR", 0}, {"CMP", 3}, {"CMU", 1}, {"CNP", 3}, {"CPS", 0}, {"CRE", 0}, {"CSS", 0}, {"DNA", 1},
    {"DNP", 1}, {"ECJ", 2}, {"END", 0}, {"EQ+", 1}, {"EQ-", 1}, {"ESC", 0}, {"EVE", 1}, {"FAC", 1}, {"FAI", 1},
    {"FAO", 1}, {"FL+", 1}, {"FL-", 1}, {"FLA", 0}, {"FLJ", 2}, {"FMU", 0}, {"FOB", 2}, {"FOM", 1}, {"FON", 2},
    {"FRE", 0}, {"GIT", 1}, {"HMC", 0}, {"INI", 0}, {"INP", 3}, {"IT+", 1}, {"IT-", 1}, {"ITJ", 2}, {"KEY", 0},
    {"LDP", 0}, {"LI+", 1}, {"ML+", 1}, {"MLP", 0}, {"MM0", 0}, {"MNA", 0}, {"MNP", 4}, {"MOV", 2}, {"MP+", 1},
    {"MPJ", 1}, {"MS2", 0}, {"MS3", 0}, {"MSG", 0}, {"MYB", 1}, {"MYD", 1}, {"NCJ", 2}, {"NOD", 0}, {"NUM", 1},
    {"PRI", 0}, {"PS+", 2}, {"QUA", 1}, {"RMU", 0}, {"SAT", 0}, {"SIL", 1}, {"SK+", 1}, {"SK-", 1}, {"SKJ", 2},
    {"SLP", 0}, {"SMC", 0}, {"SMP", 2}, {"SNP", 4}, {"SOU", 1}, {"SPS", 0}, {"SSS", 1}, {"STC", 0}, {"SVP", 0},
    {"TAM", 3}, {"TRA", 4}, {"TUR", 0}, {"UNI", 1}, {"UNJ", 1}, {"WAI", 1}, {"WAS", 0}, {"XX1", 1}, {"YNJ", 1},
    {"ZAM", 0}, {"ACH", 1}
};

static int get_cmd_params(const char *cmd) {
    for (const auto &c : cmd_table) {
        if (strncmp(c.name, cmd, 3) == 0) return c.nparams;
    }
    return -1;
}

static std::vector<uint8_t> decrypt_tsc(const std::vector<uint8_t> &raw) {
    if (raw.size() < 2) return raw;

    size_t fsize = raw.size();
    size_t keypos = fsize / 2;
    uint8_t key = raw[keypos];

    std::vector<uint8_t> dec = raw;
    for (size_t i = 0; i < keypos; ++i) {
        dec[i] = static_cast<uint8_t>(dec[i] - key);
    }
    for (size_t i = keypos + 1; i < fsize; ++i) {
        dec[i] = static_cast<uint8_t>(dec[i] - key);
    }

    // Score valid script markers to determine if decrypted text or raw was plaintext
    auto count_tags = [](const std::vector<uint8_t> &b) {
        size_t count = 0;
        for (size_t i = 0; i + 3 < b.size(); ++i) {
            if (b[i] == '<' && isupper(b[i + 1]) && isupper(b[i + 2])) count++;
            if (b[i] == '#' && isdigit(b[i + 1]) && isdigit(b[i + 2])) count++;
            if (b[i] == '[' && isprint(b[i + 1])) count++;
        }
        return count;
    };

    if (count_tags(dec) >= count_tags(raw)) {
        return dec;
    }
    return raw;
}

// non-throwing UTF-8 sanitizer
static std::string sanitize_utf8(const std::string &input) {
    std::string out;
    out.reserve(input.size() * 2);

    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            i++;
            continue;
        }

        // 2-byte sequence (0xC2 - 0xDF)
        if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 < input.size()) {
                unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                if ((c1 & 0xC0) == 0x80) {
                    out.push_back(static_cast<char>(c));
                    out.push_back(static_cast<char>(c1));
                    i += 2;
                    continue;
                }
            }
        }
        // 3-byte sequence (0xE0 - 0xEF)
        else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 < input.size()) {
                unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
                bool valid = ((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80);
                if (c == 0xE0 && c1 < 0xA0) valid = false; // Overlong
                if (c == 0xED && c1 >= 0xA0) valid = false; // UTF-16 surrogate
                if (valid) {
                    out.push_back(static_cast<char>(c));
                    out.push_back(static_cast<char>(c1));
                    out.push_back(static_cast<char>(c2));
                    i += 3;
                    continue;
                }
            }
        }
        // 4-byte sequence (0xF0 - 0xF4)
        else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 < input.size()) {
                unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
                unsigned char c3 = static_cast<unsigned char>(input[i + 3]);
                bool valid = ((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80);
                if (c == 0xF0 && c1 < 0x90) valid = false; // Overlong
                if (c == 0xF4 && c1 > 0x8F) valid = false; // Beyond U+10FFFF
                if (valid) {
                    out.push_back(static_cast<char>(c));
                    out.push_back(static_cast<char>(c1));
                    out.push_back(static_cast<char>(c2));
                    out.push_back(static_cast<char>(c3));
                    i += 4;
                    continue;
                }
            }
        }

        // Convert any raw / truncated byte into valid 2-byte UTF-8 (U+0080..U+00FF)
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        i++;
    }
    return out;
}

static bool has_printable_text(const std::string &str) {
    for (char c : str) {
        if (static_cast<unsigned char>(c) > ' ') return true;
    }
    return false;
}

static std::string normalize_newlines(const std::string &str) {
    std::string res;
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\r') {
            res.push_back('\n');
            if (i + 1 < str.size() && str[i + 1] == '\n') i++;
        } else {
            res.push_back(str[i]);
        }
    }
    while (!res.empty() && (res.front() == '\n' || res.front() == ' ' || res.front() == '\t')) res.erase(res.begin());
    while (!res.empty() && (res.back() == '\n' || res.back() == ' ' || res.back() == '\t')) res.pop_back();
    return sanitize_utf8(res);
}

static json parse_tsc_text(const std::vector<uint8_t> &buf) {
    json stage_json = json::object();
    const char *p = reinterpret_cast<const char *>(buf.data());
    const char *end = p + buf.size();

    std::string cur_event;
    std::string cur_text;

    auto flush_text = [&]() {
        if (!cur_event.empty() && has_printable_text(cur_text)) {
            std::string norm = normalize_newlines(cur_text);
            if (!norm.empty()) {
                if (!stage_json.contains(cur_event)) {
                    stage_json[cur_event] = json::array();
                }
                stage_json[cur_event].push_back(norm);
            }
        }
        cur_text.clear();
    };

    while (p < end) {
        char ch = *p++;

        if (ch == '#') {
            flush_text();
            cur_event.clear();

            char ev_buf[5] = {0};
            int i = 0;
            while (i < 4 && p < end && isdigit(*p)) {
                ev_buf[i++] = *p++;
            }
            cur_event = ev_buf;

            while (p < end && (*p == '\r' || *p == '\n')) p++;
        } else if (ch == '<' && !cur_event.empty()) {
            flush_text();

            if (p + 3 <= end) {
                char cmd[4] = { p[0], p[1], p[2], '\0' };
                p += 3;
                int params = get_cmd_params(cmd);
                if (params < 0) params = 0;

                for (int i = 0; i < params; ++i) {
                    int digit_count = 0;
                    while (digit_count < 4 && p < end && isdigit(*p)) {
                        digit_count++;
                        p++;
                    }
                    if (i < params - 1 && p < end && *p == ':') p++;
                }
            }
        } else if (!cur_event.empty()) {
            cur_text.push_back(ch);
        }
    }

    flush_text();
    return stage_json;
}

static json parse_credit_tsc(const std::vector<uint8_t> &buf) {
    json list = json::array();
    const char *p = reinterpret_cast<const char *>(buf.data());
    const char *end = p + buf.size();

    while (p < end) {
        while (p < end && (*p == '\r' || *p == '\n')) p++;
        if (p >= end) break;

        if (*p == '[') {
            p++;
            std::string text;
            while (p < end && *p != ']' && *p != '\r' && *p != '\n') {
                text.push_back(*p++);
            }
            if (p < end && *p == ']') p++;

            while (p < end && isdigit(*p)) p++;

            std::string norm = sanitize_utf8(text);
            if (!norm.empty()) {
                list.push_back(norm);
            }
        } else {
            p++;
        }
    }

    return list;
}

int main(int argc, char *argv[]) {
    // Locate directory of the executable
    fs::path exe_dir = fs::absolute(argv[0]).parent_path();
    fs::path data_dir = "data";
    std::string lang = "english";
    fs::path out_dir = exe_dir / "lang";

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--data") == 0 || strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-i") == 0) && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
            lang = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (argv[i][0] != '-') {
            data_dir = argv[i];
        }
    }

    if (!fs::exists(data_dir)) {
        std::cerr << "Error: Input directory '" << data_dir.string() << "' not found.\n";
        std::cerr << "Usage: " << argv[0] << " [path/to/tsc/folder] [--lang <language>] [--out <output_dir>]\n";
        return 1;
    }

    // Output target, <exe_dir>/lang/<language>
    fs::path out_base = out_dir / lang;
    fs::create_directories(out_base);

    // Create metadata file
    fs::path meta_path = out_base / "meta.json";
    if (!fs::exists(meta_path)) {
        json meta = {
            {"name", "English"},
            {"author", "Studio Pixel / Aeon Genesis"},
            {"version", "1.0"},
            {"rtl", false}
        };
        std::ofstream mf(meta_path);
        mf << meta.dump(2, ' ', false, json::error_handler_t::replace) << std::endl;
        std::cout << "[Created] " << meta_path.string() << "\n";
    }

    // Create system.json template for menus and stage titles
    fs::path sys_path = out_base / "system.json";
    if (!fs::exists(sys_path)) {
        json sys = {
            {"rtl", false},
            {"New game", "New game"},
            {"Load game", "Load game"},
            {"Options", "Options"},
            {"Mods", "Mods"},
            {"Quit", "Quit"},
            {"Resume", "Resume"},
            {"Reset", "Reset"},
            {"Return", "Return"},
            {"Graphics", "Graphics"},
            {"Sound", "Sound"},
            {"Controls", "Controls"},
            {"Bind keys", "Bind keys"},
            {"Resolution: ", "Resolution: "},
            {"Fullscreen: ", "Fullscreen: "},
            {"Animated facepics: ", "Animated facepics: "},
            {"Lights: ", "Lights: "},
            {"Music: ", "Music: "},
            {"Tracks: ", "Tracks: "},
            {"Sound: ", "Sound: "},
            {"SFX volume: ", "SFX volume: "},
            {"Music volume: ", "Music volume: "},
            {"Music interpolation: ", "Music interpolation: "},
            {"Force feedback: ", "Force feedback: "},
            {"Strafing: ", "Strafing: "},
            {"Ok: ", "Ok: "},
            {"Cancel: ", "Cancel: "},
            {"Left", "Left"},
            {"Right", "Right"},
            {"Up", "Up"},
            {"Down", "Down"},
            {"Jump", "Jump"},
            {"Strafe", "Strafe"},
            {"Fire", "Fire"},
            {"Wpn Prev", "Wpn Prev"},
            {"Wpn Next", "Wpn Next"},
            {"Inventory", "Inventory"},
            {"Map", "Map"},
            {"Pause", "Pause"},
            {"available", "available"}
        };

        // Extract stage titles from data/stage.dat if present
        fs::path stagedat = data_dir / "stage.dat";
        if (fs::exists(stagedat)) {
            std::ifstream sf(stagedat, std::ios::binary);
            if (sf.is_open()) {
                int nstages = sf.get();
                struct DatRec {
                    char filename[32];
                    char stagename[35];
                    uint8_t dummy[6];
                } rec;
                for (int i = 0; i < nstages; ++i) {
                    sf.read(reinterpret_cast<char *>(&rec), sizeof(rec));
                    if (rec.stagename[0]) {
                        sys[rec.stagename] = rec.stagename;
                        sys[std::string("stage_") + rec.filename] = rec.stagename;
                    }
                }
            }
        }

        std::ofstream sf(sys_path);
        sf << sys.dump(2, ' ', false, json::error_handler_t::replace) << std::endl;
        std::cout << "[Created] " << sys_path.string() << "\n";
    }

    // Scan and extract all .tsc files
    size_t processed = 0;
    for (const auto &entry : fs::recursive_directory_iterator(data_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tsc") {
            // Skip lang output directory if inside search tree
            if (entry.path().string().find("/lang/") != std::string::npos) continue;

            fs::path rel = fs::relative(entry.path(), data_dir);
            fs::path dest = out_base / rel;
            dest.replace_extension(".json");
            fs::create_directories(dest.parent_path());

            try {
                std::ifstream in(entry.path(), std::ios::binary);
                std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                std::vector<uint8_t> plain = decrypt_tsc(raw);

                json result;
                if (entry.path().filename() == "Credit.tsc") {
                    result = parse_credit_tsc(plain);
                } else {
                    result = parse_tsc_text(plain);
                }

                if (!result.empty()) {
                    std::ofstream out(dest);
                    out << result.dump(2, ' ', false, json::error_handler_t::replace) << std::endl;
                    std::cout << "[Extracted] " << rel.string() << " -> " << dest.string() << "\n";
                    processed++;
                }
            } catch (const nlohmann::json::exception &e) {
                std::cerr << "[Error] JSON error in " << rel.string() << ": " << e.what() << "\n";
            } catch (const std::exception &e) {
                std::cerr << "[Error] Failed processing " << rel.string() << ": " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[Error] Unknown error in " << rel.string() << "\n";
            }
        }
    }

    std::cout << "Successfully extracted " << processed << " script files to " << out_base.string() << "\n";
    return 0;
}