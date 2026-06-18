#include "nema/apps/badusb_parser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace nema::badusb {

struct Token {
    const char* start = nullptr;
    size_t len = 0;
};

static Token nextArg(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end || *p == '\n' || *p == '\r' || *p == '#') return {};
    const char* s = p;
    while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    return {s, (size_t)(p - s)};
}

static bool tokEq(const Token& t, const char* s) {
    if (!t.start) return false;
    return std::strncmp(t.start, s, t.len) == 0 && s[t.len] == '\0';
}

static bool tokToUint(const Token& t, uint32_t& out) {
    if (!t.start) return false;
    out = 0;
    for (size_t i = 0; i < t.len; i++) {
        if (t.start[i] < '0' || t.start[i] > '9') return false;
        out = out * 10 + (t.start[i] - '0');
    }
    return true;
}

static bool tokToUint8(const Token& t, uint8_t& out) {
    uint32_t v = 0;
    if (!tokToUint(t, v) || v > 255) return false;
    out = (uint8_t)v;
    return true;
}

struct KeyEntry { const char* name; uint8_t mod; uint8_t code; };

static const KeyEntry kKeyMap[] = {
    {"CTRL",     0x80, 0}, // KEY_LEFT_CTRL
    {"ALT",      0x82, 0}, // KEY_LEFT_ALT
    {"SHIFT",    0x81, 0}, // KEY_LEFT_SHIFT
    {"GUI",      0x83, 0}, // KEY_LEFT_GUI
    {"WINDOWS",  0x83, 0},
    {"ENTER",    0,    0xB0}, // KEY_RETURN
    {"ESC",      0,    0xB1}, // KEY_ESC
    {"ESCAPE",   0,    0xB1},
    {"TAB",      0,    0xB3},
    {"SPACE",    0,    0x20},
    {"BACKSPACE",0,    0xB2},
    {"DELETE",   0,    0xD4},
    {"UP",       0,    0xDA},
    {"DOWN",     0,    0xD9},
    {"LEFT",     0,    0xD8},
    {"RIGHT",    0,    0xD7},
    {"UPARROW",  0,    0xDA},
    {"DOWNARROW",0,    0xD9},
    {"LEFTARROW",0,    0xD8},
    {"RIGHTARROW",0,   0xD7},
    {"HOME",     0,    0xD2},
    {"END",      0,    0xD5},
    {"INSERT",   0,    0xD1},
    {"PAGEUP",   0,    0xD3},
    {"PAGEDOWN", 0,    0xD6},
    {"MENU",     0,    0xED},
    {"CAPSLOCK", 0,    0xC1},
    {"F1",       0,    0xC2},
    {"F2",       0,    0xC3},
    {"F3",       0,    0xC4},
    {"F4",       0,    0xC5},
    {"F5",       0,    0xC6},
    {"F6",       0,    0xC7},
    {"F7",       0,    0xC8},
    {"F8",       0,    0xC9},
    {"F9",       0,    0xCA},
    {"F10",      0,    0xCB},
    {"F11",      0,    0xCC},
    {"F12",      0,    0xCD},
};

static constexpr int kKeyCount = sizeof(kKeyMap) / sizeof(kKeyMap[0]);

static bool lookupKey(const Token& t, uint8_t& mod, uint8_t& code) {
    for (int i = 0; i < kKeyCount; i++) {
        if (tokEq(t, kKeyMap[i].name)) {
            mod = kKeyMap[i].mod;
            code = kKeyMap[i].code;
            return true;
        }
    }
    return false;
}

static Command parseLine(const char* line, const char* end, uint32_t /*defaultDelay*/) {
    Command cmd;
    const char* p = line;

    Token cmdTok = nextArg(p, end);
    if (!cmdTok.start) return cmd;

    if (tokEq(cmdTok, "REM") || tokEq(cmdTok, "//") || tokEq(cmdTok, "#")) {
        cmd.type = Command::None;
        return cmd;
    }
    if (tokEq(cmdTok, "STRING") || tokEq(cmdTok, "STRINGLN")) {
        cmd.type = Command::String;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p < end) cmd.text.assign(p, (size_t)(end - p));
        while (!cmd.text.empty() && (cmd.text.back() == '\r' || cmd.text.back() == '\n'))
            cmd.text.pop_back();
        if (tokEq(cmdTok, "STRINGLN")) cmd.text += '\n';
        return cmd;
    }
    if (tokEq(cmdTok, "DELAY")) {
        cmd.type = Command::Delay;
        Token v = nextArg(p, end);
        if (!tokToUint(v, cmd.delayMs)) return cmd;
        return cmd;
    }
    if (tokEq(cmdTok, "DEFAULTDELAY") || tokEq(cmdTok, "DEFAULT_DELAY")) {
        cmd.type = Command::Delay;
        Token v = nextArg(p, end);
        if (!tokToUint(v, cmd.delayMs)) return cmd;
        return cmd;
    }
    if (tokEq(cmdTok, "REPEAT")) {
        cmd.type = Command::Repeat;
        Token v = nextArg(p, end);
        if (!tokToUint8(v, cmd.repeatCount)) return cmd;
        return cmd;
    }

    // Combo: CTRL-ALT key, CTRL key, ALT key, GUI key, SHIFT key
    uint8_t comboMod = 0;
    const char* savedP = p;
    Token tok = cmdTok;

    // Check for combo like "CTRL-ALT" or "CTRL SHIFT" etc
    bool isCombo = false;
    for (;;) {
        uint8_t m = 0, c = 0;
        if (lookupKey(tok, m, c) && m != 0) {
            comboMod |= m;
            isCombo = true;
            tok = nextArg(p, end);
            savedP = p;
            if (!tok.start) break;
        } else {
            break;
        }
    }
    p = savedP;

    if (isCombo) {
        // If there's a final key after modifiers, it's a combo
        if (tok.start) {
            uint8_t mk = 0, ck = 0;
            if (lookupKey(tok, mk, ck)) {
                comboMod |= mk;
                cmd.type = Command::Key;
                cmd.modifier = comboMod;
                cmd.keycode = ck;
                return cmd;
            }
        }
        // Just a modifier chord without a key
        cmd.type = Command::Key;
        cmd.modifier = comboMod;
        cmd.keycode = 0;
        return cmd;
    }

    // Single key
    uint8_t mk = 0, ck = 0;
    if (lookupKey(cmdTok, mk, ck)) {
        cmd.type = Command::Key;
        cmd.modifier = mk;
        cmd.keycode = ck;
        return cmd;
    }

    return cmd;
}

Script parse(const char* source, size_t len) {
    Script script;
    const char* end = source + len;
    const char* line = source;
    uint32_t defaultDelay = 0;
    std::vector<Command> repeatBlock;

    while (line < end) {
        const char* nl = (const char*)std::memchr(line, '\n', (size_t)(end - line));
        if (!nl) nl = end;

        Command cmd = parseLine(line, nl, defaultDelay);

        if (cmd.type == Command::Repeat) {
            repeatBlock.clear();
            for (uint8_t r = 0; r < cmd.repeatCount; r++) {
                for (auto& c : repeatBlock) script.push_back(c);
            }
            continue;
        }

        if (cmd.type == Command::Delay && cmd.delayMs > 0) {
            script.push_back(cmd);
        } else if (cmd.type != Command::None) {
            script.push_back(cmd);
        }

        line = nl + 1;
    }

    return script;
}

Script parseFile(const uint8_t* data, size_t len) {
    return parse((const char*)data, len);
}

} // namespace nema::badusb
