#pragma once
#include "game/specials.h"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

// Cor (BGR) que representa o especial de cada personagem.
cv::Scalar specialColor(SpecialKind k);

// Projetil do especial desenhado em pixel art (usado quando nao existe
// assets/sprites/spells/especial_<id>.png para o personagem).
//  c: centro; dir: +1 indo para a direita, -1 para a esquerda; H: altura da arena.
void drawSpecialProjectile(cv::Mat &view, SpecialKind k, cv::Point c, int dir, float time, int H);

// Baltar - "Furia de Maxwell": o gato vira tigre ao lado do Baltar, corre ate o alvo e
// termina com a garra. frames = 6 quadros (gato, transformacao, tigre, rugido, salto, garra).
void drawMaxwellTiger(cv::Mat &view, const std::vector<cv::Mat> &frames, int dir, float t,
                      int xFrom, int xTo, int H, float time);

// Neri - "Jesus Cristo!": feixe de luz desce sobre o alvo, forma uma grande cruz azul e
// se desfaz em pixels. t = progresso do especial (0..1).
void drawHolyCross(cv::Mat &view, int xTarget, float t, int H, float time);

// Venegas - "La integral es la soma de las partes!": curva que avanca do mago ate o alvo
// com as barras da soma de Riemann surgindo uma a uma. front01 = quanto ja avancou.
void drawIntegralWave(cv::Mat &view, int xFrom, int xTo, float front01, int H, float time);

// Derzu - bug na tela: faixa vertical com cores separadas, linhas rasgadas, blocos
// corrompidos e mensagens de erro, centrada em cx (acompanha o poder).
void drawScreenBug(cv::Mat &img, int cx, float time, int seed);

// "Cut-in" do super, como nos jogos de luta: faixa diagonal que atravessa a tela com o
// retrato do personagem e o nome do golpe. t = tempo desde o lancamento (s).
// side: 0 = jogador da esquerda, 1 = da direita.
void drawSuperCutIn(cv::Mat &img, int side, float t, const cv::Mat &portrait,
                    const std::string &charName, SpecialKind k);
