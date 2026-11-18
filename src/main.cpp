#include "vision/camera.h"
#include "vision/split_screen.h"
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
    std::printf("Camera %d aberta. ESC sair | C limpar | P caneta | D mascaras\n", camIdx);

    SplitScreen game;

    // Padroes: P1 azul (padrao do HsvRange), P2 verde.
    game.detector[1].range = HsvRange{40, 85, 100, 255, 100, 255};

    // P1 tenta vision_p1.yaml e, se nao existir, o antigo vision.yaml.
    if (loadRange("config/vision_p1.yaml", game.detector[0].range) ||
        loadRange("config/vision.yaml",    game.detector[0].range))
        std::printf("P1: faixa HSV carregada\n");
    else
        std::printf("P1: faixa HSV padrao (azul)\n");

    if (loadRange("config/vision_p2.yaml", game.detector[1].range))
        std::printf("P2: faixa HSV carregada\n");
    else
        std::printf("P2: faixa HSV padrao (verde)\n");

    const char* win = "Duelo - webcam";
    cv::namedWindow(win);

    bool debug = false, penDown = true;
    cv::Mat frame;
    double fps = 0;
    int64 tPrev = cv::getTickCount();

    for (;;) {
        if (!cam.read(frame)) { std::fprintf(stderr, "Falha no read\n"); break; }

        const int64 t = cv::getTickCount();
        const double dt = (t - tPrev) / cv::getTickFrequency();
        tPrev = t;
        if (dt > 0) fps = (fps == 0) ? 1.0 / dt : 0.9 * fps + 0.1 / dt;

        game.update(frame, double(t) / cv::getTickFrequency());
        game.draw(frame);

        char line[64];
        std::snprintf(line, sizeof(line), "FPS %.0f", fps);
        cv::putText(frame, line, {10, frame.rows - 12}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    {255, 255, 255}, 2, cv::LINE_AA);

        cv::imshow(win, frame);
        if (debug) cv::imshow("Mascaras", game.combinedMask());

        const int k = cv::waitKey(1);
        if (k == 27) break;
        if (k == 'c' || k == 'C') game.clearPens();
        if (k == 'p' || k == 'P') { penDown = !penDown; game.setPenDown(penDown); }
        if (k == 'd' || k == 'D') {
            debug = !debug;
            if (!debug) cv::destroyWindow("Mascaras");
        }
    }
    cam.close();
    return 0;
}