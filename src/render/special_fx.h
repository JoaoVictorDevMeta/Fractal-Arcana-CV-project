#pragma once
#include "game/specials.h"
#include <opencv2/core.hpp>
#include <string>

// Cor (BGR) que representa o especial de cada personagem.
cv::Scalar specialColor(SpecialKind k);

// Projetil do especial desenhado em pixel art (usado quando nao existe
// assets/sprites/spells/especial_<id>.png para o personagem).
//  c: centro; dir: +1 indo para a direita, -1 para a esquerda; H: altura da arena.
void drawSpecialProjectile(cv::Mat &view, SpecialKind k, cv::Point c, int dir, float time, int H);

// "Cut-in" do super, como nos jogos de luta: faixa diagonal que atravessa a tela com o
// retrato do personagem e o nome do golpe. t = tempo desde o lancamento (s).
// side: 0 = jogador da esquerda, 1 = da direita.
void drawSuperCutIn(cv::Mat &img, int side, float t, const cv::Mat &portrait,
                    const std::string &charName, SpecialKind k);
