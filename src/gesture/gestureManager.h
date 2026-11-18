#pragma once
#include "vision/air_pen.h"
#include "vision/wand_detector.h"
#include "gesture/shape_classif.h"

class gestureManager
{
private:
    AirPen pen;
    ShapeClassifier classifier;
    Shape lastShape = Shape::Nenhum;
public:

    void update(Detection& wandPos){
        // 1. Atualiza a caneta com a posição da varinha
        pen.update(wandPos);

        // 2. Se a caneta "levantou" (fim do traço), chama o classificador
        if (pen.hasFinishedStroke()) {
            std::vector<cv::Point2f> pts = pen.getStroke();
            ShapeResult res = classifier.classify(pts);
            lastShape = res.shape;
            
            // Aqui futuramente você pode disparar um evento no jogo: "lastShape" foi desenhado!
        }
    };

    Shape getRecognizedShape(){
        return lastShape;
    };
};