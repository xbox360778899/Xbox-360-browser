#pragma once
#include <cstddef>
#include <string>
#include <vector>

struct Entry {
    std::string name;
    std::string url;
};

class Store {
public:
    void load();
    void save() const;

    const std::vector<Entry>& favorites() const { return m_fav; }
    const std::vector<Entry>& history() const { return m_hist; }

    bool add_favorite(const std::string& name, const std::string& url);
    void remove_favorite(size_t i);
    bool is_favorite(const std::string& url) const;
    void add_history(const std::string& name, const std::string& url);

    // Case-insensitive substring match over favorites then history, deduped by URL.
    std::vector<Entry> suggest(const std::string& text, size_t max) const;

private:
    static std::vector<Entry> read(const char* path);
    static void write(const char* path, const std::vector<Entry>& v);
    static std::string lower(const std::string& s);

    std::vector<Entry> m_fav;
    std::vector<Entry> m_hist;
};
