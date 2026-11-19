#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
#include "gesture/shape_classif.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"

int main(int argc, char **argv)
{
    const char *camWin = "Camera";
    const char *gameWin = "Duelo";
    cv::namedWindow(camWin);
    cv::namedWindow(gameWin, cv::WINDOW_AUTOSIZE);
    cv::moveWindow(camWin, 0, 0); // evita uma janela por cima da outra
    cv::moveWindow(gameWin, 700, 0);

    cv::Mat gameView(720, 1280, CV_8UC3);

    int camIdx = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--camera") && i + 1 < argc)
        {
            camIdx = std::atoi(argv[++i]);
        }
    }

    Camera cam;
    if (!cam.open(camIdx))
    {
        std::fprintf(stderr, "Erro: nao abriu a camera %d\n", camIdx);
        return 2;
    }

    WandDetector det;
    AirPen pen;
    ShapeClassifier classifier;

    ShapeResult lastResult;
    float cooldownMs = 400.0f;
    int64 lastClassifyTime = 0;

    std::ofstream logPen("log_pen.csv");
    logPen << "Timestamp_ms,Found,X,Y\n";

    std::ofstream logShape("log_shape.csv");
    logShape << "Timestamp_ms,Tempo_Classificacao_ms,Shape_ID,Confianca,DebugText\n";

    char *win = (char *)"Duelo - webcam";

    cv::Mat frame, mask, gray, inkMask;
    double fps = 0;

    int64 tPrev = cv::getTickCount();

    for (;;)
    {
        if (!cam.read(frame))
            break;
        if (pen.canvas.size() != frame.size())
            pen.reset(frame.size());
        Assets assets;
        if (!assets.load("assets"))
            std::printf("Sprites nao encontrados, usando formas simples\n");

        Match match;

        for (;;)
        {
            if (!cam.read(frame))
            {
                std::fprintf(stderr, "Falha no read\n");
                break;
            }
            if (pen.canvas.size() != frame.size())
                pen.reset(frame.size());

            Detection d = det.detect(frame, &mask);
            pen.update(d);

            double currentTimeMs = cv::getTickCount() * 1000.0 / cv::getTickFrequency();
            logPen << currentTimeMs << ","
                   << (d.found ? 1 : 0) << ","
                   << d.center.x << ","
                   << d.center.y << "\n";

            // --- LÓGICA DE CLASSIFICAÇÃO ---
            int64 currentTime = cv::getTickCount();
            float timeSinceLastClassify = (currentTime - lastClassifyTime) * 1000.0f / cv::getTickFrequency();

            if (pen.hasFinishedStroke() && timeSinceLastClassify > cooldownMs)
            {
                std::vector<cv::Point2f> pts = pen.getStroke();

                if (pts.size() > 10)
                {
                    int64 classifyStart = cv::getTickCount();
                    lastResult = classifier.classify(pts);
                    int64 classifyEnd = cv::getTickCount();
                    double classifyTimeMs = (classifyEnd - classifyStart) * 1000.0 / cv::getTickFrequency();

                    logShape << currentTimeMs << ","
                             << classifyTimeMs << ","
                             << (int)lastResult.shape << ","
                             << lastResult.confidence << ","
                             << "\"" << lastResult.resultText << "\"\n";

                    if (lastResult.shape == Shape::Nenhum)
                    {
                        lastResult.resultText = "ERRO: Forma nao reconhecida!";
                    }
                }

                pen.reset(pen.canvas.size());
                pen.resetStroke();
                lastClassifyTime = currentTime;
            }

            // --- RENDERIZAÇÃO ---
            cv::cvtColor(pen.canvas, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, inkMask, 0, 255, cv::THRESH_BINARY);
            pen.canvas.copyTo(frame, inkMask);

            // Renderiza trilha suavizada + assistente de fechamento
            pen.renderAssistant(frame);

            if (d.found)
            {
                cv::rectangle(frame, d.bbox, {0, 255, 0}, 2);
                cv::circle(frame, cv::Point(d.center), 6, {0, 0, 255}, -1);
            }

            int64 t = cv::getTickCount();
            double dt = (t - tPrev) / cv::getTickFrequency();
            tPrev = t;
            if (dt > 0)
                fps = (fps == 0) ? 1.0 / dt : 0.9 * fps + 0.1 / dt;

            char fpsText[128];
            std::snprintf(fpsText, sizeof(fpsText), "FPS %.0f", fps);
            cv::putText(frame, fpsText, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

            if (lastResult.shape != Shape::Nenhum)
            {
                std::string shapeText = "";
                if (lastResult.shape == Shape::Linha)
                    shapeText = "Rajada Arcana (Linha)";
                else if (lastResult.shape == Shape::Raio)
                    shapeText = "Relampago (Raio)";
                else if (lastResult.shape == Shape::Circulo)
                    shapeText = "Escudo (Circulo)";
                else if (lastResult.shape == Shape::Triangulo)
                    shapeText = "Lanca de Gelo (Triangulo)";
                else if (lastResult.shape == Shape::Estrela)
                    shapeText = "ESPECIAL (Estrela)";

                char resText[128];
                std::snprintf(resText, sizeof(resText), "MAGIA: %s (Confianca: %.2f)", shapeText.c_str(), lastResult.confidence);
                cv::putText(frame, resText, {10, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
                cv::putText(frame, lastResult.resultText, {10, 100}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);
            }
            else if (!lastResult.resultText.empty())
            {
                cv::putText(frame, lastResult.resultText, {10, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
            }

            if (timeSinceLastClassify < cooldownMs)
            {
                float pct = 1.0f - (timeSinceLastClassify / cooldownMs);
                cv::putText(frame, "Aguarde...", {10, 130}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150, 150, 150), 2);
                cv::rectangle(frame, cv::Rect(10, 140, (int)(200 * pct), 10), cv::Scalar(150, 150, 150), -1);
            }

            int k = cv::waitKey(1); // UM waitKey serve as duas janelas
            if (k == 27)
                break;
            if (k == 'c' || k == 'C')
            {
                pen.reset(pen.canvas.size());

                pen.resetStroke();
                lastResult = ShapeResult();
            }
        }

        logPen.close();

        if (k >= '1' && k <= '6')
            match.cast(0, static_cast<Shape>(k - '1'));

        match.update(dt);                  // logica
        drawGame(gameView, match, assets); // desenha na janela do jogo

        cv::imshow(camWin, frame);     // debug da camera
        cv::imshow(gameWin, gameView); // jogo
        logShape.close();
        cam.close();
        return 0;
    }