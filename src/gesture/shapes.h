#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

// enum em vez de vetor
// só legibilidade
enum class Shape {
    Linha,      // >>> ataque fraco
    Raio,       // >>> ataque médio
    Circulo,    // >>> escudo
    Quadrado,   // >>> ataque forte
    Triangulo,  // >>> ataque medio 2
    Estrela,    // >>> ataque especial
    Nenhum,  
};

struct Stroke {
    std::vector<cv::Point2f> pts;
};

struct ShapeResult {
    Shape shape = Shape::Nenhum;
    float confidence = 0.0;
    std::string resultText;
};