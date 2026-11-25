#include "game/score_store.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

namespace
{
    // Remove o separador e quebras de linha (os nomes vao numa linha so).
    std::string cleanField(std::string s)
    {
        for (char &c : s)
            if (c == '|' || c == '\n' || c == '\r')
                c = ' ';
        return s;
    }

    std::vector<std::string> split(const std::string &line, char sep)
    {
        std::vector<std::string> out;
        std::string cur;
        std::istringstream in(line);
        while (std::getline(in, cur, sep))
            out.push_back(cur);
        return out;
    }

    std::string formatLine(const ScoreRecord &r)
    {
        std::ostringstream o;
        o << r.id << '|' << r.when << '|' << r.winner << '|' << cleanField(r.name[0]) << '|' << r.score[0] << '|'
          << cleanField(r.name[1]) << '|' << r.score[1];
        return o.str();
    }
} // namespace

std::string ScoreStore::cleanId(const std::string &id)
{
    std::string out;
    for (char c : id)
    {
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z')
            out += c;
        if (out.size() == 3)
            break;
    }
    while (out.size() < 3)
        out += 'A';
    return out;
}

std::string ScoreStore::nowString()
{
    const std::time_t t = std::time(nullptr);
    const std::tm *lt = std::localtime(&t); // um unico thread usa isto: localtime basta
    const std::tm tmv = lt ? *lt : std::tm{};
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmv);
    return buf;
}

bool ScoreStore::load()
{
    records_.clear();
    std::ifstream in(path_);
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') // arquivo editado no Windows
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> f = split(line, '|');
        if (f.size() < 7)
            continue; // linha invalida: ignora
        ScoreRecord r;
        r.id = cleanId(f[0]);
        r.when = f[1];
        r.winner = std::atoi(f[2].c_str());
        if (r.winner < -1 || r.winner > 1)
            r.winner = -1;
        r.name[0] = f[3];
        r.score[0] = std::max(0, std::atoi(f[4].c_str()));
        r.name[1] = f[5];
        r.score[1] = std::max(0, std::atoi(f[6].c_str()));
        records_.push_back(r);
    }
    return !records_.empty();
}

bool ScoreStore::append(const ScoreRecord &recIn)
{
    ScoreRecord rec = recIn;
    rec.id = cleanId(rec.id);
    if (rec.when.empty())
        rec.when = nowString();
    records_.push_back(rec);

    // arquivo novo ganha um cabecalho explicando o formato
    const bool exists = bool(std::ifstream(path_));
    std::ofstream out(path_, std::ios::app);
    if (!out)
    {
        std::fprintf(stderr, "Aviso: nao foi possivel salvar o placar em %s\n", path_.c_str());
        return false;
    }
    if (!exists)
        out << "# Fractal Arcana - recordes (ID|data hora|vencedor 0/1/-1|nome P1|pontos P1|nome P2|pontos P2)\n";
    out << formatLine(rec) << "\n";
    std::fprintf(stdout, "Combate %s salvo em %s\n", rec.id.c_str(), path_.c_str());
    return bool(out);
}

std::vector<ScoreRecord> ScoreStore::recentBest(int n) const
{
    // indices dos n mais recentes, para desempatar pelo mais novo
    std::vector<std::pair<int, ScoreRecord>> recent;
    const int total = static_cast<int>(records_.size());
    for (int i = std::max(0, total - n); i < total; ++i)
        recent.push_back({i, records_[i]});
    std::stable_sort(recent.begin(), recent.end(), [](const auto &a, const auto &b) {
        if (a.second.best() != b.second.best())
            return a.second.best() > b.second.best();
        return a.first > b.first;
    });
    std::vector<ScoreRecord> out;
    for (const auto &r : recent)
        out.push_back(r.second);
    return out;
}

std::string ScoreStore::nextId() const
{
    // continua a sequencia a partir do numero de combates ja registrados
    int k = static_cast<int>(records_.size()) % (26 * 26 * 26);
    std::string id(3, 'A');
    for (int i = 2; i >= 0; --i)
    {
        id[i] = char('A' + k % 26);
        k /= 26;
    }
    return id;
}
