#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
#include "gesture/shape_classif.h"
#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"
#include "vision/split_screen.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

int main(int argc, char **argv)
{
    int camIdx = 0;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--camera") && i + 1 < argc)
            camIdx = std::atoi(argv[++i]);

    Camera cam;
    if (!cam.open(camIdx))
    {
        std::fprintf(stderr, "Erro: nao abriu a camera %d\n", camIdx);
        return 2;
    }
    std::printf("Camera %d aberta. ESC sair | C limpar | P caneta | D mascaras | R reiniciar | 1-5 feitico P1\n", camIdx);

    // --- janelas ---
    const char *camWin = "Camera";
    const char *gameWin = "Duelo";
    cv::namedWindow(camWin);
    cv::namedWindow(gameWin, cv::WINDOW_AUTOSIZE);
    cv::moveWindow(camWin, 0, 0);
    cv::moveWindow(gameWin, 700, 0);
    cv::Mat gameView(720, 1280, CV_8UC3);

    // --- visao: tela dividida (2 detectores, 2 Kalman, 2 canetas) ---
    SplitScreen game;
    // Padroes: P1 azul (padrao do HsvRange), P2 verde.
    game.detector[1].range = HsvRange{40, 85, 100, 255, 100, 255};

    if (loadRange("config/vision_p1.yaml", game.detector[0].range) ||
        loadRange("config/vision.yaml", game.detector[0].range))
        std::printf("P1: faixa HSV carregada\n");
    else
        std::printf("P1: faixa HSV padrao (azul)\n");

    if (loadRange("config/vision_p2.yaml", game.detector[1].range))
        std::printf("P2: faixa HSV carregada\n");
    else
        std::printf("P2: faixa HSV padrao (verde)\n");

    // --- gestos (um resultado/cooldown por jogador) ---
    ShapeClassifier classifier;
    ShapeResult lastResult[2];
    const float cooldownMs = 400.0f;
    int64 lastClassifyTime[2] = {0, 0};

    // --- jogo ---
    Assets assets;
    if (!assets.load("assets"))
        std::printf("Sprites nao encontrados, usando formas simples\n");
    Match match;

    std::ofstream logPen("log_pen.csv");
    logPen << "Timestamp_ms,Player,Found,X,Y\n";
    std::ofstream logShape("log_shape.csv");
    logShape << "Timestamp_ms,Player,Tempo_Classificacao_ms,Shape_ID,Confianca,DebugText\n";

    bool debug = false, penDown = true;
    cv::Mat frame;
    double fps = 0;
    int64 tPrev = cv::getTickCount();

    for (;;)
    {
        if (!cam.read(frame))
        {
            std::fprintf(stderr, "Falha no read\n");
            break;
        }

        const int64 t = cv::getTickCount();
        const double dt = (t - tPrev) / cv::getTickFrequency();
        tPrev = t;
        if (dt > 0)
            fps = (fps == 0) ? 1.0 / dt : 0.9 * fps + 0.1 / dt;
        const double currentTimeMs = t * 1000.0 / cv::getTickFrequency();

        // deteccao + Kalman + tinta dos dois jogadores
        game.update(frame, double(t) / cv::getTickFrequency());

        // --- CLASSIFICACAO (por jogador) ---
        float sinceMs[2];
        for (int p = 0; p < 2; ++p)
        {
            const Detection &d = game.player(p).det;
            logPen << currentTimeMs << "," << p << "," << (d.found ? 1 : 0) << ","
                   << d.center.x << "," << d.center.y << "\n";

            AirPen &pen = game.pen[p];
            sinceMs[p] = (t - lastClassifyTime[p]) * 1000.0f / cv::getTickFrequency();

            if (pen.hasFinishedStroke() && sinceMs[p] > cooldownMs)
            {
                std::vector<cv::Point2f> pts = pen.getStroke();

                if (pts.size() > 10)
                {
                    int64 c0 = cv::getTickCount();
                    lastResult[p] = classifier.classify(pts);
                    double classifyTimeMs = (cv::getTickCount() - c0) * 1000.0 / cv::getTickFrequency();

                    logShape << currentTimeMs << "," << p << "," << classifyTimeMs << ","
                             << (int)lastResult[p].shape << "," << lastResult[p].confidence << ","
                             << "\"" << lastResult[p].resultText << "\"\n";

                    if (lastResult[p].shape == Shape::Nenhum)
                        lastResult[p].resultText = "ERRO: Forma nao reconhecida!";
                    else
                        match.cast(p, lastResult[p].shape); // forma reconhecida lanca o feitico
                }

                pen.reset(pen.canvas.size());
                pen.resetStroke();
                lastClassifyTime[p] = t;
            }
        }

        // --- CAMERA (debug) ---
        game.draw(frame); // tinta, divisoria, marcadores

        char fpsText[64];
        std::snprintf(fpsText, sizeof(fpsText), "FPS %.0f", fps);
        cv::putText(frame, fpsText, {10, frame.rows - 12}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

        // HUD por jogador (x muda conforme a metade da tela)
        for (int p = 0; p < 2; ++p)
        {
            const int x = game.zone(p).x + 10;
            const ShapeResult &r = lastResult[p];

            if (r.shape != Shape::Nenhum)
            {
                char resText[160];
                std::snprintf(resText, sizeof(resText), "MAGIA: %s (%.2f)",
                              spellFor(r.shape).name.c_str(), r.confidence);
                cv::putText(frame, resText, {x, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                            cv::Scalar(0, 255, 255), 2);
                cv::putText(frame, r.resultText, {x, 100}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(200, 200, 200), 1);
            }
            else if (!r.resultText.empty())
            {
                cv::putText(frame, r.resultText, {x, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                            cv::Scalar(0, 0, 255), 2);
            }

            if (sinceMs[p] < cooldownMs)
            {
                float pct = 1.0f - (sinceMs[p] / cooldownMs);
                cv::putText(frame, "Aguarde...", {x, 130}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                            cv::Scalar(150, 150, 150), 2);
                cv::rectangle(frame, cv::Rect(x, 140, (int)(200 * pct), 10),
                              cv::Scalar(150, 150, 150), -1);
            }
        }

        match.update(dt);
        drawGame(gameView, match, assets);

        cv::imshow(camWin, frame);
        cv::imshow(gameWin, gameView);
        if (debug)
            cv::imshow("Mascaras", game.combinedMask());

        // --- TECLAS (um unico waitKey) ---
        const int k = cv::waitKey(1);
        if (k == 27)
            break;
        if (k == 'c' || k == 'C')
        {
            game.clearPens();
            lastResult[0] = lastResult[1] = ShapeResult();
        }
        if (k == 'p' || k == 'P')
        {
            penDown = !penDown;
            game.setPenDown(penDown);
        }
        if (k == 'd' || k == 'D')
        {
            debug = !debug;
            if (!debug)
                cv::destroyWindow("Mascaras");
        }
        if (k == 'r' || k == 'R')
            match.reset();
        // teclado simula o classificador (P1): 1=Linha 2=Raio 3=Circulo 4=Triangulo 5=Estrela
        if (k >= '1' && k <= '5')
            match.cast(0, static_cast<Shape>(k - '1'));
    }

    logPen.close();
    logShape.close();
    cam.close();
    return 0;
}