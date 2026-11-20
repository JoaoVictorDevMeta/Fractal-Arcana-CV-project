#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
#include "gesture/shape_classif.h"
#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"
#include "vision/split_screen.h"
#include "ui/menu_ui.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

struct MouseCtx {
    MenuUI* ui = nullptr;
    cv::Size size{1280, 720};
};

static void onMouse(int event, int x, int y, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) return;
    auto* ctx = static_cast<MouseCtx*>(userdata);
    if (ctx && ctx->ui) ctx->ui->onMouseClick(x, y, ctx->size);
}

int main(int argc, char** argv) {
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

    SplitScreen game;
    game.detector[1].range = HsvRange{40, 85, 100, 255, 100, 255};
    if (!loadRange("config/vision_p1.yaml", game.detector[0].range))
        loadRange("config/vision.yaml", game.detector[0].range);
    loadRange("config/vision_p2.yaml", game.detector[1].range);

    MenuUI menu;
    menu.loadCharacters("assets");

    const char* win = "Fractal Arcana";
    cv::namedWindow(win);
    MouseCtx mouseCtx{&menu, {1280, 720}};
    cv::setMouseCallback(win, onMouse, &mouseCtx);

    bool debug = false;
    cv::Mat frame;
    int64 tPrev = cv::getTickCount();

    for (;;) {
        if (!cam.read(frame)) break;
        mouseCtx.size = frame.size();

        const int64 t = cv::getTickCount();
        const double dt = (t - tPrev) / cv::getTickFrequency();
        tPrev = t;
        const double nowSec = double(t) / cv::getTickFrequency();

        // Atualiza deteccao/Kalman das duas varinhas
        game.update(frame, nowSec);
        Tracked t1 = game.player(0).trk;
        Tracked t2 = game.player(1).trk;

        if (menu.screen() == GameScreen::DuelReady) {
            game.draw(frame);
        } else {
            menu.update(frame.size(), t1, t2, dt);
            menu.draw(frame);
        }

        // Marcadores visuais das varinhas P1 e P2
        if (t1.valid) cv::circle(frame, {cvRound(t1.pos.x), cvRound(t1.pos.y)}, 8, {255, 140, 20}, -1, cv::LINE_AA);
        if (t2.valid) cv::circle(frame, {cvRound(t2.pos.x), cvRound(t2.pos.y)}, 8, {60, 255, 80}, -1, cv::LINE_AA);

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
        if (k == 27) break;
        if (k == 'r' || k == 'R') menu.resetSelection();
        if (k == 'm' || k == 'M') menu.setScreen(GameScreen::Title);
        if (k == 'd' || k == 'D') {
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

    cam.close();
    return 0;
}