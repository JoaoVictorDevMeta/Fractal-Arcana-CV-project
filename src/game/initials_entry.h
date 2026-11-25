#pragma once
#include <opencv2/core.hpp>
#include <string>

// Registro do combate no fim da luta, como nos fliperamas antigos: os jogadores digitam
// 3 letras maiusculas (o "nome do save"). Ja comeca com uma sugestao (AAA, AAB...):
// Enter confirma, Backspace apaga, letras substituem a sugestao da esquerda para a direita.
class InitialsEntry
{
public:
    void start(const std::string &suggestion); // abre a tela de registro
    void reset();                              // fecha (novo duelo)

    bool typing() const { return state_ == State::Typing; }
    bool saved() const { return state_ == State::Saved; }

    // Trata uma tecla enquanto digita. Devolve true se a tecla foi usada;
    // 'confirmed' fica true quando o jogador apertou Enter.
    bool handleKey(int key, bool &confirmed);

    std::string id() const;                 // letras digitadas, completadas com a sugestao
    void markSaved(const std::string &id);  // mostra "COMBATE XXX SALVO!"

    // Desenha a caixa de registro (ou o aviso de salvo) por cima da tela do duelo.
    void draw(cv::Mat &img, float time) const;

private:
    enum class State
    {
        Off,
        Typing,
        Saved
    };
    State state_ = State::Off;
    std::string typed_;
    std::string suggestion_ = "AAA";
    std::string savedId_;
};
