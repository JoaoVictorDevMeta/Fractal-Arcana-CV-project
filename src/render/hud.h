#pragma once
#include <opencv2/core.hpp>
#include <string>

class Match; // declaração antecipada: evita incluir o match.h inteiro

// Desenha o cenario completo (fundo, jogadores, projeteis, barras) em 'view'.// hud.h
struct Assets;

void drawGame(cv::Mat &view, const Match &match, const Assets &assets);

enum class BarKind
{
    Health,
    Mana
};

// Tamanho que a barra (moldura) ocupa ao caber em 'maxSize' mantendo a proporcao.
// Sem skin carregada, devolve uma barra simples (largura maxima, altura <= 22).
cv::Size statBarSize(const Assets &assets, const cv::Size &maxSize);

// Desenha a barra de vida/mana com a moldura em 'rect' (use statBarSize para obter w x h).
// side 0 = P1 (esvazia para a esquerda); side 1 = P2 (espelhada, esvazia para a direita).
// 'label' (opcional) e escrito no centro da barra.
void drawStatBar(cv::Mat &dst, const Assets &assets, BarKind kind, int side, float v01,
                 const cv::Rect &rect, const std::string &label = "");