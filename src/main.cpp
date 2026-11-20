#include "vision/camera.h"
#include "vision/split_screen.h"
#include "ui/menu_ui.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    if (!cam.open(camIdx)) {
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

        cv::imshow(win, frame);
        if (debug) cv::imshow("Mascaras", game.combinedMask());

        const int k = cv::waitKey(1);
        if (k == 27) break;
        if (k == 'r' || k == 'R') menu.resetSelection();
        if (k == 'm' || k == 'M') menu.setScreen(GameScreen::Title);
        if (k == 'd' || k == 'D') {
            debug = !debug;
            if (!debug) cv::destroyWindow("Mascaras");
        }
    }

    cam.close();
    return 0;
}