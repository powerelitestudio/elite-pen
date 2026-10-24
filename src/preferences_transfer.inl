// Included in preferences.cpp; no dependency or executable paths in the format.
namespace {
using SettingsMap = std::map<std::string, std::string>;
SettingsMap settings_map(const std::string& ini) {
    SettingsMap result;
    std::istringstream lines(ini);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto equal = line.find('=');
        if (equal != std::string::npos) result.emplace(line.substr(0, equal), line.substr(equal + 1));
    }
    return result;
}
std::string json_string(const std::string& value) {
    std::string result = "\"";
    for (char character : value) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') result += '\\';
        if (c < 32) return {}; // Canonical settings never contain control characters.
        result += static_cast<char>(c);
    }
    return result + '"';
}
struct SettingsJsonReader {
    std::string_view text;
    std::size_t at{};
    void whitespace() { while (at < text.size() && (text[at] == ' ' || text[at] == '\r' || text[at] == '\n' || text[at] == '\t')) ++at; }
    bool take(char c) { whitespace(); if (at == text.size() || text[at] != c) return false; ++at; return true; }
    bool string(std::string& result) {
        result.clear();
        if (!take('"')) return false;
        while (at < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[at++]);
            if (c == '"') return result.size() <= 512 &&
                (result.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, result.data(), static_cast<int>(result.size()), nullptr, 0) > 0);
            if (c < 32 || result.size() > 512) return false;
            if (c != '\\') { result += static_cast<char>(c); continue; }
            if (at == text.size()) return false;
            const char escaped = text[at++];
            if (escaped == '"' || escaped == '\\' || escaped == '/') { result += escaped; continue; }
            // Values are single-line settings. Escaped controls are deliberately rejected.
            if (escaped != 'u') return false;
            const auto hex4 = [&](unsigned& value) {
                value = 0;
                for (int i = 0; i < 4; ++i) {
                    if (at == text.size()) return false;
                    const char h = text[at++];
                    unsigned digit;
                    if (h >= '0' && h <= '9') digit = static_cast<unsigned>(h - '0');
                    else if (h >= 'A' && h <= 'F') digit = 10U + static_cast<unsigned>(h - 'A');
                    else if (h >= 'a' && h <= 'f') digit = 10U + static_cast<unsigned>(h - 'a');
                    else return false;
                    value = value * 16 + digit;
                }
                return true;
            };
            unsigned code{};
            if (!hex4(code) || code < 32) return false;
            if (code >= 0xD800 && code <= 0xDBFF) {
                if (at + 2 > text.size() || text.substr(at, 2) != "\\u") return false;
                at += 2;
                unsigned low{};
                if (!hex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
                code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
            } else if (code >= 0xDC00 && code <= 0xDFFF) return false;
            if (code < 0x80) result += static_cast<char>(code);
            else {
                if (code >= 0x10000) result += static_cast<char>(0xF0 | (code >> 18));
                if (code >= 0x800) result += static_cast<char>((code >= 0x10000 ? 0x80 : 0xE0) | ((code >> 12) & (code >= 0x10000 ? 0x3F : 0x0F)));
                result += static_cast<char>((code >= 0x800 ? 0x80 : 0xC0) | ((code >> 6) & (code >= 0x800 ? 0x3F : 0x1F)));
                result += static_cast<char>(0x80 | (code & 0x3F));
            }
        }
        return false;
    }
    bool read(SettingsMap& settings) {
        if (text.starts_with("\xEF\xBB\xBF")) at = 3;
        if (!take('{')) return false;
        bool format = false, version = false, values = false;
        do {
            std::string key;
            if (!string(key) || !take(':')) return false;
            if (key == "format" && !format) {
                std::string value;
                if (!string(value) || value != "ElitePen.Settings") return false;
                format = true;
            } else if (key == "version" && !version) {
                if (!take('1')) return false;
                version = true;
            } else if (key == "settings" && !values) {
                if (!take('{')) return false;
                do {
                    std::string name, value;
                    if (!string(name) || !take(':') || !string(value) ||
                        name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos ||
                        !settings.emplace(name, value).second || settings.size() > 512) return false;
                } while (take(','));
                if (!take('}')) return false;
                values = true;
            } else return false;
        } while (take(','));
        if (!take('}')) return false;
        whitespace();
        return at == text.size() && format && version && values;
    }
};
}
bool PreferencesStore::export_json(const std::wstring& target, const Preferences& preferences, std::wstring& error) const {
    error.clear();
    std::string content = "{\n  \"format\": \"ElitePen.Settings\",\n  \"version\": 1,\n  \"settings\": {\n";
    const auto values = settings_map(preferences_ini(preferences));
    for (auto i = values.begin(); i != values.end(); ++i)
        content += "    " + json_string(i->first) + ": " + json_string(i->second) + (std::next(i) == values.end() ? "\n" : ",\n");
    content += "  }\n}\n";
    if (write_settings_atomic(target, content)) return true;
    error = L"No se pudo escribir el archivo de configuración.";
    return false;
}
bool PreferencesStore::read_json(const std::wstring& source, Preferences& result, std::wstring& error) const {
    error = L"El archivo no es una configuración válida de Elite Pen o usa una versión no compatible.";
    std::error_code ec;
    const auto size = std::filesystem::file_size(source, ec);
    if (ec || size > 65536) return false;
    std::ifstream input{std::filesystem::path(source), std::ios::binary};
    if (!input) return false;
    std::string content(65537, '\0');
    input.read(content.data(), static_cast<std::streamsize>(content.size()));
    content.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad() || content.size() > 65536) return false;
    SettingsMap values;
    SettingsJsonReader reader{content};
    if (!reader.read(values)) return false;
    const auto allowed = settings_map(preferences_ini(Preferences{}));
    std::string ini = "[ElitePen]\r\n";
    for (const auto& [key, value] : values) {
        // Focus masks only appear when present in a saved scene.
        bool mask = false;
        for (const auto& [name, unused] : allowed) {
            (void)unused;
            if (name.ends_with("MaskCount") && key.starts_with(name.substr(0, name.size() - 5)) &&
                key.size() == name.size() - 4 && key.back() >= '1' && key.back() <= '8') mask = true;
        }
        if ((!allowed.contains(key) && !mask) || value.find_first_of("\r\n") != std::string::npos) return false;
        ini += key + '=' + value + "\r\n";
    }
    // Validate with the same loader as startup, in an isolated, disposable file.
    const auto temporary = path_ + L".import-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    if (!write_settings_atomic(temporary, ini)) { error = L"No se pudo preparar la importación."; return false; }
    const auto candidate = PreferencesStore(temporary).load();
    const auto normalized = settings_map(preferences_ini(candidate));
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
    std::filesystem::remove(temporary, ec);
    for (const auto& [key, value] : values) {
        const auto found = normalized.find(key);
        if (found == normalized.end() || found->second != value) {
            error = L"La configuración contiene valores fuera de rango o atajos incompatibles.";
            return false;
        }
    }
    result = candidate;
    error.clear();
    return true;
}
