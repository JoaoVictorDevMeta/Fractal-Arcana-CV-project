#pragma once
#include "game/match.h"
#include <opencv2/core.hpp>
#include <string>

// Animacoes dos magos no estilo luta de arcade, feitas sobre o sprite unico de cada
// personagem: poses em quadros discretos (15 qps), transformacoes com vizinho mais
// proximo (o pixel art continua nitido) e efeitos desenhados com blocos de pixel.
//
//  - Dano:     congelamento + flash branco, recuo com inclinacao, faisca e poeira.
//  - Nocaute:  arremessado para tras em arco, cai deitado, quica e fica no chao.
//  - Feitico:  cada personagem tem um golpe especial proprio (MoveStyle).
//  - Vitoria:  pulinho de comemoracao depois do nocaute do oponente.

enum class MoveStyle
{
    Padrao,
    Derzu,     // Golpe do Cajado: recua e investe, runa roxa no chao
    Neri,      // Giro do Vortice: gira no proprio eixo dentro de um redemoinho
    Menegueti, // Ascensao do Compasso: levita diante do esquadro e compasso dourados
    Venegas,   // Investida do General: avanco rapido com imagens residuais
    Baltar,    // Pisao da Terra: salta e pisa, levantando terra e folhas
    Subramana, // Mantra Cibernetico: flutua emitindo aneis de mana
};

MoveStyle moveStyleFor(const std::string &characterId);
cv::Scalar moveAccent(MoveStyle s); // cor (BGR) dos efeitos do personagem

// Desenha o mago animado.
//  spr:   sprite ja no tamanho final e virado para o lado certo (BGRA).
//  side:  0 = esquerda (olha para a direita), 1 = direita (olha para a esquerda).
//  feet:  ponto do chao embaixo do mago (centro inferior do sprite em repouso).
//  idleTime: relogio para a respiracao em guarda.
//  afterimages: false desliga as imagens residuais (mais leve em PCs fracos).
// Retorna o centro do corpo na tela (para posicionar o escudo).
cv::Point drawFighter(cv::Mat &view, const cv::Mat &spr, int side, cv::Point feet,
                      const FighterAnim &st, MoveStyle style, float idleTime,
                      bool afterimages = true);

// Intensidade (0..1) do escurecimento da arena durante o "super" (Ataque Especial).
float superDarken(const FighterAnim &st);

// Letreiro "K.O." em blocos de pixel. koT = tempo desde o nocaute.
void drawKOBanner(cv::Mat &view, float koT);
