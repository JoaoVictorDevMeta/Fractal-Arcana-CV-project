#include "game/score_store.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

// Formato do arquivo:
//   # Fractal Arcana - ultimo combate
//   vencedor=0          (0 = P1, 1 = P2, -1 = interrompido)
//   p1_nome=Derzu Omaia
//   p1_pontos=12340
//   p2_nome=Neri
//   p2_pontos=9870

bool ScoreStore::load(LastMatch &out) const
{
    std::ifstream in(path_);
    if (!in)
        return false;

    LastMatch lm;
    bool hasScore[2] = {false, false};
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') // arquivo salvo no Windows
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "vencedor")
            lm.winner = std::atoi(value.c_str());
        for (int p = 0; p < 2; ++p)
        {
            const std::string prefix = p == 0 ? "p1_" : "p2_";
            if (key == prefix + "nome")
                lm.name[p] = value;
            else if (key == prefix + "pontos")
            {
                lm.score[p] = std::max(0, std::atoi(value.c_str()));
                hasScore[p] = true;
            }
        }
    }
    if (!hasScore[0] || !hasScore[1])
        return false;
    if (lm.winner < -1 || lm.winner > 1)
        lm.winner = -1;
    lm.valid = true;
    out = lm;
    return true;
}

bool ScoreStore::save(const LastMatch &lm) const
{
    if (!lm.valid)
        return false;
    std::ofstream outFile(path_, std::ios::trunc);
    if (!outFile)
    {
        std::fprintf(stderr, "Aviso: nao foi possivel salvar o placar em %s\n", path_.c_str());
        return false;
    }
    outFile << "# Fractal Arcana - ultimo combate\n";
    outFile << "vencedor=" << lm.winner << "\n";
    for (int p = 0; p < 2; ++p)
    {
        outFile << "p" << (p + 1) << "_nome=" << lm.name[p] << "\n";
        outFile << "p" << (p + 1) << "_pontos=" << lm.score[p] << "\n";
    }
    std::fprintf(stdout, "Placar salvo em %s\n", path_.c_str());
    return bool(outFile);
}
