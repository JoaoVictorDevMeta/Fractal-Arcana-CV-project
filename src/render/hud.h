#pragma once
#include <opencv2/core.hpp>

class Match; // declaração antecipada: evita incluir o match.h inteiro

// Desenha o cenario completo (fundo, jogadores, projeteis, barras) em 'view'.
void drawGame(cv::Mat &view, const Match &match);