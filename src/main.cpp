#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
#include "gesture/shape_classif.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    int camIdx = 0;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--camera") && i + 1 < argc)
            camIdx = std::atoi(argv[++i]);

    Camera cam;
    if (!cam.open(camIdx)) {
        std::fprintf(stderr, "Erro: nao abriu a camera %d\n", camIdx);
        return 2;
    }

    WandDetector det;
    AirPen pen;
    ShapeClassifier classifier;
    
    // Variáveis de Controle de Estado
    ShapeResult lastResult;
    float cooldownMs = 400.0f; // Delay de 0.8 segundos entre validações
    int64 lastClassifyTime = 0; // Tempo da última classificação
    
    const char* win = "Duelo - webcam";
    cv::namedWindow(win);

    cv::Mat frame, mask, gray, inkMask;
    double fps = 0; int64 tPrev = cv::getTickCount();

    for (;;) {
        if (!cam.read(frame)) break;
        if (pen.canvas.size() != frame.size()) pen.reset(frame.size());

        Detection d = det.detect(frame, &mask);
        pen.update(d);

        // --- LÓGICA DE CLASSIFICAÇÃO ---
        int64 currentTime = cv::getTickCount();
        // Tempo passado desde a última classificação em milissegundos
        float timeSinceLastClassify = (currentTime - lastClassifyTime) * 1000.0f / cv::getTickFrequency();

        // Só classifica se: a caneta terminou o traço E já passou o tempo de cooldown
        if (pen.hasFinishedStroke() && timeSinceLastClassify > cooldownMs) {
            
            std::vector<cv::Point2f> pts = pen.getStroke();
            
            // Só tenta classificar se o jogador desenhou algo (mais de 10 pontos)
            if (pts.size() > 10) {
                lastResult = classifier.classify(pts);
                
                // SE DER ERRADO (Nenhum), ou confiança muito baixa:
                if (lastResult.shape == Shape::Nenhum || lastResult.confidence < 0.55f) {
                    lastResult.resultText = "ERRO: Forma nao reconhecida!";
                    lastResult.shape = Shape::Nenhum; // Garante que a UI saiba que é erro
                }
                
                // Limpa o rastro após classificar (seja acerto ou erro)
                pen.reset(pen.canvas.size());
                pen.resetStroke();
                
                // Marca o tempo para iniciar o delay
                lastClassifyTime = currentTime;
            } else {
                // Se desenhou muito pouco, apenas limpa o rastro silenciosamente
                pen.reset(pen.canvas.size());
                pen.resetStroke();
                lastClassifyTime = currentTime; // Aplica delay para não ficar limpando rápido
            }
        }

        // --- RENDERIZAÇÃO ---
        // Compõe o canvas do desenho no frame da câmera
        cv::cvtColor(pen.canvas, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, inkMask, 0, 255, cv::THRESH_BINARY);
        pen.canvas.copyTo(frame, inkMask);

        // Desenha caixa da varinha
        if (d.found) {
            cv::rectangle(frame, d.bbox, {0,255,0}, 2);
            cv::circle(frame, cv::Point(d.center), 6, {0,0,255}, -1);
        }

        // Desenha FPS
        int64 t = cv::getTickCount();
        double dt = (t - tPrev) / cv::getTickFrequency(); tPrev = t;
        if (dt > 0) fps = (fps == 0) ? 1.0/dt : 0.9*fps + 0.1/dt;
        
        char fpsText[128];
        std::snprintf(fpsText, sizeof(fpsText), "FPS %.0f", fps);
        cv::putText(frame, fpsText, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

        // Desenha Feedback do Feitiço (se houver)
        if (lastResult.shape != Shape::Nenhum) {
            // ACERTO
            std::string shapeText = "";
            if (lastResult.shape == Shape::Linha) shapeText = "Rajada Arcana (Linha)";
            else if (lastResult.shape == Shape::Raio) shapeText = "Relampago (Raio)";
            else if (lastResult.shape == Shape::Circulo) shapeText = "Escudo (Circulo)";
            else if (lastResult.shape == Shape::Quadrado) shapeText = "Bola de Fogo (Quadrado)";
            else if (lastResult.shape == Shape::Triangulo) shapeText = "Lanca de Gelo (Triangulo)";
            else if (lastResult.shape == Shape::Estrela) shapeText = "ESPECIAL (Estrela)";
            
            char resText[128];
            std::snprintf(resText, sizeof(resText), "MAGIA: %s (Confianca: %.2f)", shapeText.c_str(), lastResult.confidence);
            cv::putText(frame, resText, {10, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
            cv::putText(frame, lastResult.resultText, {10, 100}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);
        } 
        else if (!lastResult.resultText.empty() && lastResult.resultText == "ERRO: Forma nao reconhecida!") {
            // ERRO
            cv::putText(frame, lastResult.resultText, {10, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
        }

        // Mostra o tempo de cooldown na tela (opcional, para debug)
        if (timeSinceLastClassify < cooldownMs) {
            float pct = 1.0f - (timeSinceLastClassify / cooldownMs);
            cv::putText(frame, "Aguarde...", {10, 130}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150, 150, 150), 2);
            // Barra de progresso do cooldown
            cv::rectangle(frame, cv::Rect(10, 140, (int)(200 * pct), 10), cv::Scalar(150, 150, 150), -1);
        }

        cv::imshow(win, frame);
        int k = cv::waitKey(1);
        
        if (k == 27) break;
        if (k == 'c' || k == 'C') {
            pen.reset(pen.canvas.size());
            pen.resetStroke();
            lastResult = ShapeResult(); // Limpa a tela e o feedback
        }
    }
    cam.close();
    return 0;
}