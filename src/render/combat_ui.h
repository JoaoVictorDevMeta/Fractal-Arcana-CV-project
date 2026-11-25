#pragma once
#include "game/specials.h"
#include <opencv2/core.hpp>
#include <string>

class Match;
struct Assets;

// Dados de cada lado da luta usados pela interface de combate.
struct CombatSide
{
    std::string name;   // nome do personagem
    cv::Mat portrait;   // retrato (BGR) da selecao
    SpecialKind special = SpecialKind::Padrao;
};

// Barra superior: retrato, nome, vida, mana, indicador do especial, recarga e relogio central.
void drawCombatHud(cv::Mat &hud, const Match &m, const Assets &as, const CombatSide side[2]);

// Moldura da camera de um jogador: borda na cor do jogador, etiqueta com o nome,
// resultado do ultimo gesto e a legenda das formas (apagada quando falta mana).
void drawCameraFrame(cv::Mat &display, const cv::Rect &r, int p, const CombatSide &side, const Match &m,
                     const std::string &status, bool statusOk, float statusAge);
