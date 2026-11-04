#include "store.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr const char* kFavPath = "game:/ie_fav.txt";
constexpr const char* kHistPath = "game:/ie_hist.txt";
constexpr size_t kHistoryCap = 50;
}

void Store::load()
{
    m_fav = read(kFavPath);
    m_hist = read(kHistPath);
}

void Store::save() const
{
    write(kFavPath, m_fav);
    write(kHistPath, m_hist);
}

std::vector<Entry> Store::read(const char* path)
{
    std::vector<Entry> v;
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return v;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        char* tab = std::strchr(line, '\t');
        if (!tab)
            continue;
        *tab = 0;
        char* nl = std::strchr(tab + 1, '\n');
        if (nl)
            *nl = 0;
        if (*line && *(tab + 1))
            v.push_back({ line, tab + 1 });
    }
    std::fclose(f);
    return v;
}

void Store::write(const char* path, const std::vector<Entry>& v)
{
    FILE* f = std::fopen(path, "wb");
    if (!f)
        return;
    for (const Entry& e : v)
        std::fprintf(f, "%s\t%s\n", e.name.c_str(), e.url.c_str());
    std::fflush(f);
    std::fclose(f);
}

std::string Store::lower(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return out;
}

bool Store::add_favorite(const std::string& name, const std::string& url)
{
    if (is_favorite(url))
        return false;
    m_fav.insert(m_fav.begin(), { name, url });
    return true;
}

void Store::remove_favorite(size_t i)
{
    if (i < m_fav.size())
        m_fav.erase(m_fav.begin() + static_cast<std::ptrdiff_t>(i));
}

bool Store::is_favorite(const std::string& url) const
{
    for (const Entry& e : m_fav)
        if (e.url == url)
            return true;
    return false;
}

void Store::add_history(const std::string& name, const std::string& url)
{
    for (size_t i = 0; i < m_hist.size(); i++)
        if (m_hist[i].url == url) {
            m_hist.erase(m_hist.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    m_hist.insert(m_hist.begin(), { name, url });
    if (m_hist.size() > kHistoryCap)
        m_hist.resize(kHistoryCap);
}

std::vector<Entry> Store::suggest(const std::string& text, size_t max) const
{
    std::vector<Entry> out;
    if (text.empty() || max == 0)
        return out;
    const std::string needle = lower(text);
    auto scan = [&](const std::vector<Entry>& from) {
        for (const Entry& e : from) {
            if (out.size() >= max)
                return;
            if (lower(e.url).find(needle) == std::string::npos
                && lower(e.name).find(needle) == std::string::npos)
                continue;
            bool dup = false;
            for (const Entry& o : out)
                if (o.url == e.url)
                    dup = true;
            if (!dup)
                out.push_back(e);
        }
    };
    scan(m_fav);
    scan(m_hist);
    return out;
}
