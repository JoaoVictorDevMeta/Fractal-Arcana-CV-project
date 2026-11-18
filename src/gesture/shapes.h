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

struct ShapeFeatures {
    bool closed = false;
    int numVertices = 0;
    double area = 0.0;
    double perimeter = 0.0;
    float circularity = 0.0;
    float lineAngleDeg = 0.0;
    float maxPerpDev = 0.0;
    float cornerDefinition = 0.0;
    float radialRatio = 0.0;
};
