#include "settings.h"
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <map>
#include <cstdlib>

namespace fs = std::filesystem;

static std::string ConfigPath(const char* addonDir)
{
    return std::string(addonDir) + "/settings.ini";
}

// ── Minimal key=value store (one entry per line) ────────────────────────────
using KV = std::map<std::string, std::string>;

static KV ParseKV(std::istream& in)
{
    KV kv;
    std::string line;
    while (std::getline(in, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

static bool GetBool(const KV& kv, const std::string& key, bool def)
{
    auto it = kv.find(key);
    return it == kv.end() ? def : (it->second == "1");
}

static float GetFloat(const KV& kv, const std::string& key, float def)
{
    auto it = kv.find(key);
    return it == kv.end() ? def : std::strtof(it->second.c_str(), nullptr);
}

static void GetColor(const KV& kv, const std::string& key, float* out)
{
    auto it = kv.find(key);
    if (it == kv.end()) return;
    std::stringstream ss(it->second);
    std::string part;
    for (int i = 0; i < 4 && std::getline(ss, part, ','); i++)
        out[i] = std::strtof(part.c_str(), nullptr);
}

static void LoadRing(const KV& kv, const std::string& prefix, Settings::RingSettings& ring)
{
    ring.Enabled   = GetBool (kv, prefix + ".enabled",   ring.Enabled);
    ring.Radius    = GetFloat(kv, prefix + ".radius",    ring.Radius);
    ring.Thickness = GetFloat(kv, prefix + ".thickness", ring.Thickness);
    GetColor(kv, prefix + ".color", ring.Color);
}

static void SaveRing(std::ostream& out, const std::string& prefix, const Settings::RingSettings& ring)
{
    out << prefix << ".enabled="   << (ring.Enabled ? 1 : 0) << "\n";
    out << prefix << ".radius="    << ring.Radius             << "\n";
    out << prefix << ".thickness=" << ring.Thickness          << "\n";
    out << prefix << ".color="     << ring.Color[0] << "," << ring.Color[1]
                                   << "," << ring.Color[2] << "," << ring.Color[3] << "\n";
}

void Settings::Load(const char* addonDir)
{
    std::ifstream f(ConfigPath(addonDir));
    if (!f.is_open()) return;

    KV kv = ParseKV(f);

    LoadRing(kv, "outer", Outer);
    LoadRing(kv, "inner", Inner);
}

void Settings::Save(const char* addonDir)
{
    fs::create_directories(addonDir);

    std::ofstream f(ConfigPath(addonDir));
    SaveRing(f, "outer", Outer);
    SaveRing(f, "inner", Inner);
}
