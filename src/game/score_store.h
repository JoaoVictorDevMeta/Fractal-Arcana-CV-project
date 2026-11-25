#pragma once
#include <string>
#include <utility>

// Resultado de um combate (o ultimo jogado), mostrado na tela inicial.
struct LastMatch
{
    bool valid = false;
    std::string name[2];
    int score[2] = {0, 0};
    int winner = -1; // -1 = partida interrompida antes do nocaute
};

// Guarda o ultimo combate num arquivo de texto simples (chave=valor), para que a tela
// inicial mostre o placar mesmo depois de fechar e abrir o jogo.
class ScoreStore
{
public:
    explicit ScoreStore(std::string path = "config/ultimo_combate.txt") : path_(std::move(path)) {}

    bool load(LastMatch &out) const; // false se o arquivo nao existe ou esta invalido
    bool save(const LastMatch &lm) const;
    const std::string &path() const { return path_; }

private:
    std::string path_;
};
