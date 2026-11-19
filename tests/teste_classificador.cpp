#include "gesture/shape_classif.h"
#include <iostream>
#include <cmath>

int main() {
    ShapeClassifier classifier;

    // 1. Criando um Quadrado com muitos pontos (traços contínuos)
    std::vector<cv::Point2f> quadrado;
    for (int i = 0; i <= 20; i++) quadrado.push_back({(float)i, 0.0f});       // Topo
    for (int i = 0; i <= 20; i++) quadrado.push_back({20.0f, (float)i});      // Direita
    for (int i = 20; i >= 0; i--) quadrado.push_back({(float)i, 20.0f});      // Base
    for (int i = 20; i >= 0; i--) quadrado.push_back({0.0f, (float)i});       // Esquerda

    // 2. Criando um Círculo com muitos pontos
    std::vector<cv::Point2f> circulo;
    for (int i = 0; i <= 50; ++i) {
        float angle = i * 7.2f * CV_PI / 180.0f;
        circulo.push_back({ std::cos(angle) * 20.0f, std::sin(angle) * 20.0f });
    }

    // Testando Quadrado
    ShapeResult resQuad = classifier.classify(quadrado);
    std::cout << "Quadrado -> ID: " << (int)resQuad.shape 
              << " | Confianca: " << resQuad.confidence << "\n"
              << "Debug: " << resQuad.resultText << "\n\n";

    // Testando Círculo
    ShapeResult resCirc = classifier.classify(circulo);
    std::cout << "Circulo  -> ID: " << (int)resCirc.shape 
              << " | Confianca: " << resCirc.confidence << "\n"
              << "Debug: " << resCirc.resultText << "\n";

    return 0;
}