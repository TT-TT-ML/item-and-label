// item_and_label.cpp — 自制特殊数据库 ItemAndLabel
// 编译: g++ -O2 -std=c++17 item_and_label.cpp -o db.exe
//
// 用法概览:
//   db.exe <数据库文件.mdb> --new_item --label_add "-name" -v --char "xiaoming" --main_label "-name"
//   db.exe <数据库文件.mdb> --new_item --label_add "-name" -v --char "p" --main_label "-name" --cnt 50
//   db.exe <数据库文件.mdb> --new_item --label_add "-name" -v --char "*_a" --main_label "-name" --cnt 10  (值 1_a..10_a)
//   db.exe <数据库文件.mdb> --new_item --label_add "-score" -v --float "**0.5" --main_label "-name" --cnt 4   (值 1 1.5 2 2.5)
//   db.exe <数据库文件.mdb> --label_add "-age" -v --int "*" "*"   (给全部项加 age=1,2,3...)
//   db.exe <数据库文件.mdb> --new_item --label_add "-color" -v --char "@(red,green,blue)" --main_label "-name" --cnt 6
//   db.exe <数据库文件.mdb> --label_add "-age" -v --int "0" "*"
//   db.exe <数据库文件.mdb> --label_add "-tag" -v --char "a" "-name" "-age"
//   db.exe <数据库文件.mdb> --label_del "-age" "*"
//   db.exe <数据库文件.mdb> --search "-name" --and "age>=18" --or "active=true"
//   db.exe <数据库文件.mdb> --list / --count / --show <id> / --delete <标签...|*> / --set <id> <标签> -v <类型> <值>
//
// 存储格式: ItemAndLabel DB v1 文本 / v2 紧凑二进制(ILDB 魔数)。
// 类型: char / int / float / bool。标签名仅支持数字、英文、'_'。'*' 为通配。

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <random>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cmath>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using namespace std;

static const string HEADER = "ItemAndLabel DB v1";

enum class LType { CHAR, INT, FLOAT, BOOL };

static string tname(LType t) {
    switch (t) {
        case LType::CHAR:  return "char";
        case LType::INT:   return "int";
        case LType::FLOAT: return "float";
        case LType::BOOL:  return "bool";
    }
    return "char";
}
static LType tparse(string s) {
    if (s.rfind("--", 0) == 0) s = s.substr(2);
    if (s == "char")  return LType::CHAR;
    if (s == "int")   return LType::INT;
    if (s == "float") return LType::FLOAT;
    if (s == "bool")  return LType::BOOL;
    return LType::CHAR;
}

struct Label { LType type = LType::CHAR; string value; };
struct Item  { string id; string mainLabel; map<string, Label> labels; };

static string esc(const string& s) { string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; } return r; }
static string quote(const string& s) { return "\"" + esc(s) + "\""; }

static bool validName(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!(isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

// char 类型标签值禁止包含这些符号: % & # \ / ('*' 与 '@' 除外, 它们是批量通配符语法)
static const char* CHAR_BAD = "%&#\\/";
static bool validCharValue(const string& ty, const string& va) {
    if (tparse(ty) != LType::CHAR) return true;
    if (va.find_first_of(CHAR_BAD) != string::npos) {
        cerr << "错误: char 类型值不能包含符号 % & # \\ / (当前值: \"" << va << "\")\n";
        return false;
    }
    size_t a = va.find('@');
    if (a != string::npos && (a + 1 >= va.size() || va[a + 1] != '(')) {
        cerr << "错误: char 类型值中的 @ 需配合括号遍历 @(...) (当前值: \"" << va << "\")\n";
        return false;
    }
    return true;
}

static bool parseNum(const string& s, size_t& i, double& out) {
    size_t j = i;
    if (j < s.size() && (s[j] == '+' || s[j] == '-')) j++;
    bool digit = false;
    while (j < s.size() && isdigit((unsigned char)s[j])) { j++; digit = true; }
    if (j < s.size() && s[j] == '.') { j++; while (j < s.size() && isdigit((unsigned char)s[j])) { j++; digit = true; } }
    if (!digit) return false;
    out = atof(s.substr(i, j - i).c_str());
    i = j;
    return true;
}

static string fmtNum(double v, LType ty, int width) {
    char buf[64];
    double vi = floor(v + 0.5);
    bool isInt = (fabs(v - vi) < 1e-9);
    if (ty == LType::INT) {
        long long x = (long long)llround(v);
        if (width > 0) snprintf(buf, sizeof buf, "%0*lld", width, (long long)x);
        else snprintf(buf, sizeof buf, "%lld", (long long)x);
    } else if (isInt) {
        if (width > 0) snprintf(buf, sizeof buf, "%0*lld", width, (long long)vi);
        else snprintf(buf, sizeof buf, "%lld", (long long)vi);
    } else snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

// 批量值通配符展开(值模板可含多个占位符), idx 从 1 开始。
static bool wildValue(const string& va, LType ty, long long idx, string& out, string& err) {
    err.clear();
    string s = va;
    bool any = false;
    {
        size_t pos = 0;
        while ((pos = s.find("@(", pos)) != string::npos) {
            size_t close = s.find(')', pos + 2);
            if (close == string::npos) { err = "通配符 @(...) 缺少右括号 )"; return true; }
            string body = s.substr(pos + 2, close - pos - 2);
            vector<string> items;
            size_t b = 0;
            while (true) {
                size_t e = body.find(',', b);
                string item = body.substr(b, (e == string::npos ? body.size() : e) - b);
                size_t l = item.find_first_not_of(" \t");
                if (l != string::npos) item = item.substr(l);
                size_t r = item.find_last_not_of(" \t");
                if (r != string::npos) item = item.substr(0, r + 1);
                items.push_back(item);
                if (e == string::npos) break;
                b = e + 1;
            }
            bool allEmpty = true; for (auto& it : items) if (!it.empty()) { allEmpty = false; break; }
            if (allEmpty) { err = "枚举 @(...) 不能为空"; return true; }
            long long j = (idx - 1) % (long long)items.size();
            if (j < 0) j += (long long)items.size();
            s = s.substr(0, pos) + items[(size_t)j] + s.substr(close + 1);
            any = true;
            pos = close;
        }
    }
    {
        size_t p = s.find('*');
        while (p != string::npos) {
            double step = 1.0, start = 1.0;
            size_t wend = p + 1;
            int width = 0;
            if (wend < s.size() && s[wend] == '*') {
                size_t q = wend + 1;
                double st;
                if (q < s.size() && (isdigit((unsigned char)s[q]) || s[q] == '+' || s[q] == '-' || s[q] == '.')) {
                    if (parseNum(s, q, st)) { step = st; if (step == 0.0) step = 1.0; }
                }
                if (q < s.size() && s[q] == '@') {
                    size_t r = q + 1; double s2;
                    if (parseNum(s, r, s2)) { start = s2; q = r; }
                }
                wend = q;
            } else if (wend < s.size() && isdigit((unsigned char)s[wend])) {
                size_t r = wend;
                while (r < s.size() && isdigit((unsigned char)s[r])) r++;
                width = atoi(s.substr(wend, r - wend).c_str());
                wend = r;
            }
            double v = start + (double)(idx - 1) * step;
            string num = fmtNum(v, ty, width);
            s = s.substr(0, p) + num + s.substr(wend);
            any = true;
            p = s.find('*', p + num.size());
        }
    }
    out = s;
    return any;
}
static bool hasWild(const string& v) {
    return v.find('*') != string::npos || v.find("@(") != string::npos;
}
static bool validWildUse(const string& ty, const string& va, long long batch, const char* where) {
    if (!hasWild(va)) return true;
    if (tparse(ty) == LType::BOOL) {
        cerr << "错误: bool 类型不支持通配符 (值: \"" << va << "\")\n";
        return false;
    }
    if (batch <= 1) {
        cerr << "错误: 通配符(* / @(...)) 仅在批量(" << where << " > 1 项)时有效 (值: \"" << va << "\")\n";
        return false;
    }
    return true;
}
static string stripDash(string s) { while (!s.empty() && s[0] == '-') s.erase(0, 1); return s; }

static vector<string> tokenize(const string& s) {
    vector<string> out;
    size_t i = 0, n = s.size();
    while (i < n) {
        while (i < n && isspace((unsigned char)s[i])) i++;
        if (i >= n) break;
        string tok; bool inq = false;
        while (i < n) {
            char c = s[i];
            if (c == '"') { inq = !inq; i++; if (!inq) { if (i >= n || isspace((unsigned char)s[i])) break; else continue; } continue; }
            if (inq) { if (c == '\\' && i + 1 < n && (s[i + 1] == '"' || s[i + 1] == '\\')) { tok += s[i + 1]; i += 2; continue; } tok += c; i++; }
            else { if (isspace((unsigned char)c)) break; tok += c; i++; }
        }
        out.push_back(tok);
    }
    return out;
}

static Item parseItem(const string& line) {
    Item it;
    auto tk = tokenize(line);
    for (size_t i = 0; i < tk.size(); i++) {
        string t = tk[i];
        if (t == "-id") { if (i + 1 < tk.size()) it.id = tk[++i]; continue; }
        if (t.rfind("-main_label:", 0) == 0) {
            string body = t.substr(strlen("-main_label:"));
            size_t p1 = body.find(':');
            size_t p2 = body.find(':', p1 + 1);
            if (p1 == string::npos || p2 == string::npos) continue;
            string name = body.substr(0, p1);
            string typ  = body.substr(p1 + 1, p2 - p1 - 1);
            string val  = body.substr(p2 + 1);
            it.mainLabel = name;
            it.labels[name] = { tparse(typ), val };
            continue;
        }
        if (t.size() > 1 && t[0] == '-') {
            string body = t.substr(1);
            size_t p1 = body.find(':');
            size_t p2 = body.find(':', p1 + 1);
            if (p1 == string::npos || p2 == string::npos) continue;
            string name = body.substr(0, p1);
            string typ  = body.substr(p1 + 1, p2 - p1 - 1);
            string val  = body.substr(p2 + 1);
            it.labels[name] = { tparse(typ), val };
        }
    }
    return it;
}

static string formatItem(const Item& it) {
    string line = "-id " + it.id;
    if (!it.mainLabel.empty() && it.labels.count(it.mainLabel)) {
        const Label& l = it.labels.at(it.mainLabel);
        line += " -main_label:" + it.mainLabel + ":" + tname(l.type) + ":" + quote(l.value);
    }
    for (auto& kv : it.labels) {
        if (kv.first == it.mainLabel) continue;
        line += " -" + kv.first + ":" + tname(kv.second.type) + ":" + quote(kv.second.value);
    }
    return line;
}

// ===================== 存储层 =====================
static long long toLL(const string& s);
static double   toD (const string& s);
static bool     toB (const string& s);
static const unsigned char MAGIC[4] = { 'I', 'L', 'D', 'B' };
static const unsigned char FMT_VERSION = 2;

static void putUv(std::ostream& os, unsigned long long v) {
    while (v >= 0x80) { os.put((char)((v & 0x7f) | 0x80)); v >>= 7; }
    os.put((char)v);
}
static unsigned long long getUv(std::istream& is) {
    unsigned long long v = 0; int shift = 0;
    while (true) {
        unsigned char b = 0;
        if (!is.get((char&)b)) break;
        v |= (unsigned long long)(b & 0x7f) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
        if (shift > 70) break;
    }
    return v;
}
static void putZig(std::ostream& os, long long n) { putUv(os, (unsigned long long)((n << 1) ^ (n >> 63))); }
static long long getZig(std::istream& is) { unsigned long long u = getUv(is); return (long long)((u >> 1) ^ -(long long)(u & 1)); }

static void putString(std::ostream& os, const string& s) { putUv(os, s.size()); os.write(s.data(), (streamsize)s.size()); }
static string getString(std::istream& is) {
    unsigned long long len = getUv(is);
    if (len > (1u << 28)) return "";
    string s((size_t)len, '\0');
    if (len) is.read(&s[0], (streamsize)len);
    return s;
}

struct LoadResult { vector<Item> items; string error; bool isV2 = false; };

static vector<Item> readV2(std::istream& f, string& err) {
    vector<Item> items;
    unsigned char ver, flags; char rs[2];
    f.read((char*)&ver, 1); f.read((char*)&flags, 1); f.read(rs, 2);
    unsigned long long dc = getUv(f);
    if (dc > (1u << 20)) { err = "标签字典过大"; return items; }
    vector<string> dict((size_t)dc);
    for (auto& d : dict) d = getString(f);
    while (true) {
        char id[10];
        f.read(id, 10);
        if (f.gcount() < 10) break;
        Item it; it.id.assign(id, 10);
        unsigned long long lc = getUv(f);
        if (lc > (1u << 16)) { err = "标签数异常"; break; }
        unsigned long long mainIdx = getUv(f);
        bool bad = false;
        for (unsigned long long k = 0; k < lc; k++) {
            unsigned long long ni = getUv(f);
            int t = f.get();
            if (t < 0) { bad = true; break; }
            string name = (ni < dict.size()) ? dict[(size_t)ni] : ("?idx" + to_string(ni));
            Label lb; lb.type = (LType)(t & 3);
            if (lb.type == LType::CHAR) lb.value = getString(f);
            else if (lb.type == LType::INT) lb.value = to_string(getZig(f));
            else if (lb.type == LType::FLOAT) {
                double d = 0; f.read((char*)&d, 8);
                if (f.gcount() < 8) { bad = true; break; }
                char b[40]; snprintf(b, sizeof(b), "%.17g", d); lb.value = b;
            }
            else { int bb = f.get(); lb.value = (bb == 1) ? "true" : "false"; }
            it.labels[name] = lb;
        }
        if (bad) { err = "记录损坏"; break; }
        if (mainIdx < dict.size() && it.labels.count(dict[(size_t)mainIdx])) it.mainLabel = dict[(size_t)mainIdx];
        items.push_back(it);
    }
    return items;
}

static void writeV2(std::ostream& f, const vector<Item>& items) {
    set<string> names;
    for (auto& it : items) for (auto& kv : it.labels) names.insert(kv.first);
    vector<string> dict(names.begin(), names.end());
    map<string, unsigned long long> idx;
    for (unsigned long long i = 0; i < dict.size(); i++) idx[dict[(size_t)i]] = i;
    f.write((const char*)MAGIC, 4);
    f.put((char)FMT_VERSION); f.put((char)0); f.put((char)0); f.put((char)0);
    putUv(f, dict.size());
    for (auto& d : dict) putString(f, d);
    for (auto& it : items) {
        size_t il = it.id.size();
        f.write(it.id.data(), il < 10 ? il : 10);
        for (size_t i = il; i < 10; i++) f.put(' ');
        putUv(f, it.labels.size());
        unsigned long long mi = (unsigned long long)-1;
        auto mf = it.labels.find(it.mainLabel);
        if (mf != it.labels.end()) mi = idx[it.mainLabel];
        putUv(f, mi);
        for (auto& kv : it.labels) {
            putUv(f, idx[kv.first]);
            const Label& l = kv.second;
            f.put((char)l.type);
            if (l.type == LType::CHAR) putString(f, l.value);
            else if (l.type == LType::INT) putZig(f, toLL(l.value));
            else if (l.type == LType::FLOAT) { double d = toD(l.value); f.write((const char*)&d, 8); }
            else f.put(toB(l.value) ? 1 : 0);
        }
    }
}

static LoadResult readFile(const string& path) {
    LoadResult r;
    ifstream f(path, ios::binary);
    if (!f) return r;
    char m[4];
    f.read(m, 4);
    if (f.gcount() == 4 && m[0] == (char)MAGIC[0] && m[1] == (char)MAGIC[1] && m[2] == (char)MAGIC[2] && m[3] == (char)MAGIC[3]) {
        r.isV2 = true;
        r.items = readV2(f, r.error);
    } else {
        f.close();
        ifstream tf(path);
        string line;
        getline(tf, line);
        while (getline(tf, line)) { if (line.empty()) continue; r.items.push_back(parseItem(line)); }
    }
    return r;
}

#ifdef _WIN32
static wstring utf8ToWide(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    wstring w((size_t)n, 0);
    if (n) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
#endif

static void atomicReplace(const string& tmp, const string& dst) {
#ifdef _WIN32
    if (!MoveFileExW(utf8ToWide(tmp).c_str(), utf8ToWide(dst).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(utf8ToWide(dst).c_str());
        MoveFileW(utf8ToWide(tmp).c_str(), utf8ToWide(dst).c_str());
    }
#else
    std::remove(dst.c_str());
    std::rename(tmp.c_str(), dst.c_str());
#endif
}

static void writeFile(const string& path, const vector<Item>& items) {
    string tmp = path + ".tmp";
    {
        ofstream f(tmp, ios::binary);
        if (f) { writeV2(f, items); f.flush(); }
    }
    atomicReplace(tmp, path);
}

static uintmax_t fileSize(const string& p) {
    ifstream f(p, ios::binary | ios::ate);
    return f ? (uintmax_t)f.tellg() : 0;
}
static void copyFile(const string& src, const string& dst) {
    ifstream in(src, ios::binary);
    ofstream out(dst, ios::binary);
    if (in && out) out << in.rdbuf();
}

static string genId(mt19937& rng) {
    static const char A[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string id(10, 0);
    for (int i = 0; i < 10; i++) id[i] = A[rng() % 62];
    return id;
}

static long long toLL(const string& s) { try { return stoll(s); } catch (...) { return 0; } }
static double   toD (const string& s) { try { return stod (s); } catch (...) { return 0; } }
static bool     toB  (const string& s) { string t = s; for (auto& c : t) c = tolower((unsigned char)c); return t == "true" || t == "1" || t == "yes"; }

static bool wildcardMatch(const string& text, const string& pat) {
    size_t ti = 0, pi = 0, star = string::npos, mt = 0;
    while (ti < text.size()) {
        if (pi < pat.size() && (pat[pi] == '?' || pat[pi] == text[ti])) { ti++; pi++; }
        else if (pi < pat.size() && pat[pi] == '*') { star = pi++; mt = ti; }
        else if (star != string::npos) { pi = star + 1; mt++; ti = mt; }
        else return false;
    }
    while (pi < pat.size() && pat[pi] == '*') pi++;
    return pi == pat.size();
}

struct Cond { string label; string op = "="; string rhs; bool hasValue = false; };

static Cond parseCond(string t) {
    Cond c;
    static const char* ops[] = { ">=", "<=", "!=", ">", "<", "=" };
    for (const char* o : ops) {
        size_t p = t.find(o);
        if (p != string::npos) {
            c.label = t.substr(0, p);
            c.rhs = t.substr(p + strlen(o));
            c.op = o;
            c.hasValue = true;
            return c;
        }
    }
    size_t p = t.find(':');
    if (p != string::npos) { c.label = t.substr(0, p); c.rhs = t.substr(p + 1); c.op = "="; c.hasValue = true; }
    else { c.label = t; }
    c.label = stripDash(c.label);
    return c;
}

static bool condMatch(const Item& it, const Cond& c) {
    if (c.label == "*" && !c.hasValue) return true;
    auto f = it.labels.find(c.label);
    if (f == it.labels.end()) return false;
    if (!c.hasValue) return true;
    const Label& l = f->second;
    if (c.op == "=" || c.op == ":") {
        if (l.type == LType::CHAR) return wildcardMatch(l.value, c.rhs);
        if (l.type == LType::BOOL) return toB(l.value) == toB(c.rhs);
        if (l.type == LType::INT)  return toLL(l.value) == toLL(c.rhs);
        return toD(l.value) == toD(c.rhs);
    }
    if (c.op == "!=") {
        Cond eq = c; eq.op = "="; return !condMatch(it, eq);
    }
    if (l.type == LType::CHAR) {
        int r = l.value.compare(c.rhs);
        if (c.op == ">")  return r > 0;
        if (c.op == "<")  return r < 0;
        if (c.op == ">=") return r >= 0;
        return r <= 0;
    }
    if (l.type == LType::INT) {
        long long a = toLL(l.value), b = toLL(c.rhs);
        if (c.op == ">")  return a >  b;
        if (c.op == "<")  return a <  b;
        if (c.op == ">=") return a >= b;
        return a <= b;
    }
    double a = toD(l.value), b = toD(c.rhs);
    if (c.op == ">")  return a >  b;
    if (c.op == "<")  return a <  b;
    if (c.op == ">=") return a >= b;
    return a <= b;
}

static bool anyValueMatch(const Item& it, const string& v) {
    for (auto& kv : it.labels) {
        const Label& l = kv.second;
        bool m = false;
        if (l.type == LType::CHAR) m = wildcardMatch(l.value, v);
        else if (l.type == LType::BOOL) m = toB(l.value) == toB(v);
        else if (l.type == LType::INT)  m = toLL(l.value) == toLL(v);
        else m = toD(l.value) == toD(v);
        if (m) return true;
    }
    return false;
}

static void printItem(const Item& it, ostream& os = cout) {
    os << "  ID    : " << it.id << "\n";
    os << "  MAIN  : " << (it.mainLabel.empty() ? "(none)" : it.mainLabel) << "\n";
    for (auto& kv : it.labels)
        os << "  " << kv.first << " : " << tname(kv.second.type) << "(" << kv.second.value << ")\n";
    os << "\n";
}

static bool matchFilters(const Item& it, const vector<string>& filters) {
    if (filters.empty()) return true;
    for (auto& f : filters) if (f == "*") return true;
    for (auto& f : filters) {
        Cond c = parseCond(stripDash(f));
        if (c.hasValue) { if (!condMatch(it, c)) return false; }
        else { if (!it.labels.count(c.label)) return false; }
    }
    return true;
}

static void help() {
    cout <<
        "ItemAndLabel 特殊数据库工具\n"
        "\n"
        "db.exe <数据库.mdb> --new_item --label_add \"-标签\" -v --类型 \"值\" --main_label \"-标签\" [--cnt N]\n"
        "    创建新项(必须设置 main_label)。--cnt N 批量创建 N 项, 每项独立 10 位唯一 id。\n"
        "    类型: --char / --int / --float / --bool。\n"
        "db.exe <数据库.mdb> --label_add \"-标签\" -v --类型 \"值\" [过滤标签... | *]\n"
        "    给满足过滤(同时含这些标签)的项加标签, * 表示全部项。\n"
        "db.exe <数据库.mdb> --label_del \"-标签\" [过滤标签... | *]\n"
        "    从满足过滤的项删除标签, * 表示全部项。\n"
        "db.exe <数据库.mdb> --search \"条件\" [--and \"条件\" | --or \"条件\"]...\n"
        "    条件: 标签名 / 标签=值 / 标签>=值 等(值支持 * ? 通配)。从左到右结合。\n"
        "    --search -v \"值\" 表示按任意标签的值搜索(也支持 * ? 通配)。\n"
        "db.exe <数据库.mdb> --list | --count | --show <id> | --delete [标签...|*] | --set <id> <标签> -v --类型 \"值\"\n"
        "db.exe <数据库.mdb> -g \"标签\"  输出含该标签所有项的值, 多个值以换行隔开(纯文本)。\n"
        "db.exe <数据库.mdb> --stats | --vacuum | --check | --backup <目标>\n"
        "    --stats 数据库统计(大小/项数/标签去重)。 --vacuum 压缩重写为紧凑格式。\n"
        "    --check 完整性校验。 --backup <目标> 复制备份。\n"
        "\n"
        "标签名仅支持数字、英文和 '_';  '*' 为通配符。\n"
        "批量值通配符(--cnt > 1 创建或批量加标签时, 值可含):\n"
        "    '*'          递增序号(从 1, 步长 1),        char \"*_a\" x10 → 1_a 2_a ... 10_a\n"
        "    '**<step>'   递增, 步长 step(可小数),      \"**0.5\" x4 → 1 1.5 2 2.5\n"
        "    '**<step>@<s>' 递增, 从 s 开始步长 step,    \"**2@10\" x4 → 10 12 14 16\n"
        "    '*<w>'        序号补零到 w 位,             \"*04\" x5 → 0001 0002 ... 0005\n"
        "    '@(a,b,c)'    枚举遍历, 轮流取值,          \"@(红,绿,蓝)\" x4 → 红 绿 蓝 红\n"
        "    多个占位符可组合, 如 \"@(A,B)-*04\"。int 值可直接用 \"*\"。\n"
        "    char 类型值禁止 % & # \\ / 符号; '*' 与 '@' 仅作为批量通配符语法。\n";
}

struct Op {
    string kind;
    string name, type, value, id, mainLabel;
    int cnt = 1;
    vector<string> filters;
    vector<pair<string, string>> query;
};

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 3) { help(); return 0; }
    string path = argv[1];
    vector<string> a;
    for (int i = 2; i < argc; i++) a.push_back(argv[i]);

    bool newItemMode = false;
    for (auto& t : a) if (t == "--new_item") { newItemMode = true; break; }

    vector<Op> ops;
    for (size_t i = 0; i < a.size(); i++) {
        string t = a[i];
        if (t == "--new_item") { Op o; o.kind = "new"; ops.push_back(o); }
        else if (t == "--cnt") {
            Op o; o.kind = "cnt"; o.cnt = (i + 1 < a.size()) ? (int)toLL(a[++i]) : 1; ops.push_back(o);
        }
        else if (t == "--label_add") {
            Op o; o.kind = newItemMode ? "newlabel" : "add";
            o.name = stripDash(a[++i]);
            if (i + 1 < a.size() && a[i + 1] == "-v") { o.type = a[i + 2]; o.value = a[i + 3]; i += 3; }
            if (!newItemMode)
                while (i + 1 < a.size() && a[i + 1].rfind("--", 0) != 0) o.filters.push_back(stripDash(a[++i]));
            ops.push_back(o);
        }
        else if (t == "--main_label") { Op o; o.kind = "mainlabel"; o.name = stripDash(a[++i]); ops.push_back(o); }
        else if (t == "--label_del") {
            Op o; o.kind = "del"; o.name = stripDash(a[++i]);
            while (i + 1 < a.size() && a[i + 1].rfind("--", 0) != 0) o.filters.push_back(stripDash(a[++i]));
            ops.push_back(o);
        }
        else if (t == "--search" || t == "--and" || t == "--or") {
            string conn = (t == "--search") ? "" : (t == "--and" ? "and" : "or");
            string term;
            if (i + 1 < a.size() && a[i + 1] == "-v") {
                term = "@v:" + a[i + 2];
                i += 2;
            } else {
                term = a[++i];
            }
            if (ops.empty() || ops.back().kind != "search") { Op o; o.kind = "search"; ops.push_back(o); }
            ops.back().query.push_back({ conn, term });
        }
        else if (t == "--list") { Op o; o.kind = "list"; ops.push_back(o); }
        else if (t == "--count") { Op o; o.kind = "count"; ops.push_back(o); }
        else if (t == "-g") { Op o; o.kind = "get"; o.name = stripDash(a[++i]); ops.push_back(o); }
        else if (t == "--stats") { Op o; o.kind = "stats"; ops.push_back(o); }
        else if (t == "--vacuum") { Op o; o.kind = "vacuum"; ops.push_back(o); }
        else if (t == "--check") { Op o; o.kind = "check"; ops.push_back(o); }
        else if (t == "--backup") { Op o; o.kind = "backup"; o.name = a[++i]; ops.push_back(o); }
        else if (t == "--show") { Op o; o.kind = "show"; o.id = a[++i]; ops.push_back(o); }
        else if (t == "--delete") {
            Op o; o.kind = "rm";
            while (i + 1 < a.size() && a[i + 1].rfind("--", 0) != 0) o.filters.push_back(stripDash(a[++i]));
            ops.push_back(o);
        }
        else if (t == "--set") {
            Op o; o.kind = "set"; o.id = a[++i]; o.name = stripDash(a[++i]);
            if (i + 1 < a.size() && a[i + 1] == "-v") { o.type = a[i + 2]; o.value = a[i + 3]; i += 3; }
            ops.push_back(o);
        }
        else if (t == "--help" || t == "-h") { help(); return 0; }
        else { cerr << "未知参数: " << t << "\n"; help(); return 1; }
    }

    vector<tuple<string, string, string>> newLabels;
    string newMain; int cnt = 1;
    for (auto& o : ops) {
        if (o.kind == "newlabel") newLabels.push_back({ o.name, o.type, o.value });
        else if (o.kind == "mainlabel") newMain = o.name;
        else if (o.kind == "cnt") cnt = o.cnt;
    }

    LoadResult lr = readFile(path);
    vector<Item> items = lr.items;
    set<string> ids;
    for (auto& it : items) ids.insert(it.id);
    bool modified = false;
    bool doCheck = false; for (auto& o : ops) if (o.kind == "check") doCheck = true;
    if (!lr.error.empty() && !doCheck) cerr << "警告: 数据库可能损坏: " << lr.error << "\n";
    mt19937 rng(random_device{}());

    for (auto& o : ops) {
        if (o.kind == "new") {
            if (newMain.empty()) { cerr << "错误: 新建项必须指定 --main_label\n"; return 1; }
            bool has = false; for (auto& nl : newLabels) if (get<0>(nl) == newMain) has = true;
            if (!has) { cerr << "错误: 主标签 " << newMain << " 未通过 --label_add 设置\n"; return 1; }
            for (auto& nl : newLabels) {
                if (!validCharValue(get<1>(nl), get<2>(nl))) return 1;
                if (!validWildUse(get<1>(nl), get<2>(nl), cnt, "批量创建 --cnt")) return 1;
            }
            for (int k = 0; k < cnt; k++) {
                Item it;
                string id;
                do { id = genId(rng); } while (ids.count(id));
                it.id = id; ids.insert(id);
                for (auto& nl : newLabels) {
                    string nm = get<0>(nl), ty = get<1>(nl), va = get<2>(nl);
                    if (!validName(nm)) { cerr << "错误: 非法标签名 " << nm << "\n"; return 1; }
                    string vv = va; string werr;
                    wildValue(va, tparse(ty), (long long)k + 1, vv, werr);
                    if (!werr.empty()) { cerr << "错误: " << werr << "\n"; return 1; }
                    it.labels[nm] = { tparse(ty), vv };
                }
                it.mainLabel = newMain;
                items.push_back(it);
            }
            modified = true;
            cout << "已创建 " << cnt << " 个项\n";
        }
        else if (o.kind == "add") {
            if (!validName(o.name)) { cerr << "错误: 非法标签名 " << o.name << "\n"; return 1; }
            if (!validCharValue(o.type, o.value)) return 1;
            vector<size_t> idxs;
            for (size_t i = 0; i < items.size(); i++) if (matchFilters(items[i], o.filters)) idxs.push_back(i);
            if (!validWildUse(o.type, o.value, (long long)idxs.size(), "加标签匹配")) return 1;
            LType ty = tparse(o.type);
            for (size_t j = 0; j < idxs.size(); j++) {
                string vv = o.value; string werr;
                wildValue(o.value, ty, (long long)j + 1, vv, werr);
                if (!werr.empty()) { cerr << "错误: " << werr << "\n"; return 1; }
                items[idxs[j]].labels[o.name] = { ty, vv };
            }
            modified = true;
            cout << "已为 " << idxs.size() << " 个项添加标签 " << o.name << "\n";
        }
        else if (o.kind == "del") {
            if (!validName(o.name)) { cerr << "错误: 非法标签名 " << o.name << "\n"; return 1; }
            int n = 0;
            for (auto& it : items) if (matchFilters(it, o.filters)) {
                if (it.labels.erase(o.name)) { if (it.mainLabel == o.name) it.mainLabel.clear(); n++; }
            }
            modified = true;
            cout << "已从 " << n << " 个项删除标签 " << o.name << "\n";
        }
        else if (o.kind == "set") {
            if (!validCharValue(o.type, o.value)) return 1;
            if (hasWild(o.value)) {
                cerr << "错误: 通配符(* / @(...)) 仅可用于批量场景(--cnt 创建或批量加标签)\n"; return 1;
            }
            bool found = false;
            for (auto& it : items) if (it.id == o.id) {
                it.labels[o.name] = { tparse(o.type), o.value }; found = true; break;
            }
            if (!found) { cerr << "错误: 未找到 id " << o.id << "\n"; return 1; }
            modified = true;
            cout << "已更新 " << o.id << " 的标签 " << o.name << "\n";
        }
        else if (o.kind == "rm") {
            // 修复: --delete "*"(删除全部)是合法操作; 仅完全未提供过滤标签时才报错
            if (o.filters.empty()) {
                cerr << "错误: --delete 需要明确的过滤标签(或用 * 删除全部)\n"; return 1;
            }
            size_t before = items.size();
            items.erase(remove_if(items.begin(), items.end(),
                [&](const Item& it) { return matchFilters(it, o.filters); }), items.end());
            for (auto& it : items) ids.insert(it.id);
            modified = true;
            cout << "已删除 " << (before - items.size()) << " 个项\n";
        }
        else if (o.kind == "search") {
            auto itemMatch = [&](const Item& it) {
                bool acc = false, first = true;
                for (auto& q : o.query) {
                    bool r;
                    if (q.second.rfind("@v:", 0) == 0) r = anyValueMatch(it, q.second.substr(3));
                    else r = condMatch(it, parseCond(q.second));
                    if (first) { acc = r; first = false; }
                    else if (q.first == "and") acc = acc && r;
                    else if (q.first == "or")  acc = acc || r;
                    else acc = r;
                }
                return acc;
            };
            int n = 0;
            cout << "匹配结果:\n";
            for (auto& it : items) if (itemMatch(it)) { printItem(it); n++; }
            cout << "共 " << n << " 个项匹配\n";
        }
        else if (o.kind == "list") {
            cout << "共 " << items.size() << " 个项:\n";
            for (auto& it : items) printItem(it);
        }
        else if (o.kind == "count") {
            cout << items.size() << "\n";
        }
        else if (o.kind == "get") {
            vector<string> vals;
            for (auto& it : items) {
                auto f = it.labels.find(o.name);
                if (f != it.labels.end()) vals.push_back(f->second.value);
            }
            for (size_t k = 0; k < vals.size(); k++) { if (k) cout << "\n"; cout << vals[k]; }
            cout << "\n";
        }
        else if (o.kind == "show") {
            bool found = false;
            for (auto& it : items) if (it.id == o.id) { printItem(it); found = true; break; }
            if (!found) { cerr << "错误: 未找到 id " << o.id << "\n"; return 1; }
        }
        else if (o.kind == "stats") {
            set<string> uniq;
            size_t total = 0;
            for (auto& it : items) for (auto& kv : it.labels) { uniq.insert(kv.first); total++; }
            cout << "路径      : " << path << "\n";
            cout << "文件大小  : " << fileSize(path) << " 字节" << (lr.isV2 ? " (v2 紧凑)" : " (v1 文本)") << "\n";
            cout << "项数      : " << items.size() << "\n";
            cout << "标签实例数: " << total << "\n";
            cout << "唯一标签名: " << uniq.size() << "\n";
            cout << "字典化节省: " << (total - uniq.size()) << " 处重复标签名\n";
        }
        else if (o.kind == "vacuum") {
            uintmax_t before = fileSize(path);
            if (!items.empty() || lr.isV2) {
                writeFile(path, items);
                cout << "vacuum: " << before << " 字节 -> " << fileSize(path) << " 字节 ("
                     << (before > fileSize(path) ? "已压缩" : "大小基本不变") << ")\n";
            } else {
                cout << "vacuum: 数据库为空或无变化\n";
            }
        }
        else if (o.kind == "check") {
            if (lr.error.empty()) {
                cout << "OK: " << lr.items.size() << " 项, 格式=" << (lr.isV2 ? "v2 紧凑二进制" : "v1 文本") << ", 文件完整\n";
            } else {
                cout << "损坏: " << lr.error << " (已读取 " << lr.items.size() << " 项)\n";
            }
        }
        else if (o.kind == "backup") {
            copyFile(path, o.name);
            cout << "已备份到 " << o.name << " (" << fileSize(o.name) << " 字节)\n";
        }
    }

    if (modified) writeFile(path, items);
    return 0;
}
