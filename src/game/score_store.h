#pragma once
#include <string>
#include <utility>
#include <vector>

// Um combate registrado na tabela de recordes.
struct ScoreRecord
{
    std::string id;   // registro de 3 letras maiusculas (ex.: "ABC"), escolhido no fim da luta
    std::string when; // data e hora: "AAAA-MM-DD HH:MM"
    std::string name[2];
    int score[2] = {0, 0};
    int winner = -1; // 0 = P1, 1 = P2, -1 = combate interrompido antes do K.O.

    int best() const { return score[0] > score[1] ? score[0] : score[1]; }
};

// Guarda TODOS os combates num arquivo de texto (uma linha por combate), para que a
// tabela de recordes da tela inicial continue la depois de fechar e abrir o jogo.
//
// Formato da linha:  ID|AAAA-MM-DD HH:MM|vencedor|nome P1|pontos P1|nome P2|pontos P2
class ScoreStore
{
public:
    explicit ScoreStore(std::string path = "config/placares.txt") : path_(std::move(path)) {}

    bool load();                          // le todos os combates do arquivo
    bool append(const ScoreRecord &rec);  // registra um combate (memoria + arquivo)
    const std::vector<ScoreRecord> &all() const { return records_; }

    // Os n combates mais recentes, do maior para o menor placar (empate: o mais novo primeiro).
    std::vector<ScoreRecord> recentBest(int n = 5) const;

    // Proximo registro livre na sequencia AAA, AAB, ..., ZZZ (sugestao para o jogador).
    std::string nextId() const;

    static std::string nowString(); // data e hora atuais no formato do arquivo
    static std::string cleanId(const std::string &id); // 3 letras A-Z (completa com 'A')

private:
    std::string path_;
    std::vector<ScoreRecord> records_; // na ordem em que foram jogados
};
