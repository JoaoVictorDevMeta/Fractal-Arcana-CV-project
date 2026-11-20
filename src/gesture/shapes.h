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
    Triangulo,  // >>> ataque forte
    Estrela,    // >>> ataque especial
    Nenhum,  
};

struct ShapeResult {
    Shape shape = Shape::Nenhum;
    float confidence = 0.0f;
    std::string resultText;
};

struct ShapeFeatures {
    bool closed = false;
    float endGapRatio = 1.0f;
    float windingAngleDeg = 0.0f;
    int numVertices = 0;
    int polyVertices = 0;
    double area = 0.0;
    double hullArea = 0.0;
    double perimeter = 0.0;
    float circularity = 0.0f;
    float hullCircularity = 0.0f;
    float aspectRatio = 0.0f;
    float lineAngleDeg = 0.0f;
    float maxPerpDev = 0.0f;
    float cornerDefinition = 0.0f;
    float radialRatio = 0.0f;
    int turns = 0;
    bool altRadii = false;
    float starTipRatio = 0.0f;
};