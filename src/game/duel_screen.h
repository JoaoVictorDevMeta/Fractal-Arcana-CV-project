#pragma once
#include "gesture/shapes.h"
#include "render/combat_ui.h"
#include <opencv2/core.hpp>
#include <string>

class Match;
class MenuUI;
class SplitScreen;
struct Assets;

// Tela de combate: guarda o que cada lado mostra (nome, retrato, especial, ultimo gesto)
// e monta a imagem final - HUD em cima, cameras dos jogadores no meio, arena embaixo e
// os efeitos de tela cheia (cut-in do super, bug do Derzu).
class DuelScreen
{
public:
    // Novo duelo com os personagens escolhidos no menu: carrega os assets e zera a partida.
    void begin(const MenuUI &menu, Match &match, Assets &assets);

    // Gesto terminado por um jogador: tenta lancar o feitico e guarda a mensagem da camera.
    void onGesture(int p, const ShapeResult &result, Match &match);

    void tick(double dt); // envelhece as mensagens dos gestos

    // Monta a tela do duelo em 'display' (mesmo tamanho do frame da camera).
    // debug = mostra as mascaras de cor no lugar das cameras.
    void render(cv::Mat &display, cv::Mat &frame, const SplitScreen &game, const Match &match,
                const Assets &assets, bool debug, bool glow) const;

    const CombatSide &side(int p) const { return side_[p]; }

private:
    CombatSide side_[2];
    std::string status_[2];             // resultado do ultimo gesto
    bool statusOk_[2] = {false, false}; // true = feitico lancado
    float statusAge_[2] = {99.f, 99.f}; // segundos desde o ultimo gesto
};
