#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
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
    std::printf("Camera %d aberta. ESC para sair.\n", camIdx);

    WandDetector det;
    if (loadRange("config/vision.yaml", det.range))
        std::printf("Faixa HSV carregada de config/vision.yaml\n");
    else
        std::printf("Usando faixa HSV padrao (azul)\n");

    AirPen pen;
    const char* win = "Duelo - webcam";
    cv::namedWindow(win);

    cv::Mat frame, mask, gray, inkMask;
    double fps = 0; int64 tPrev = cv::getTickCount();

    for (;;) {
        if (!cam.read(frame)) { std::fprintf(stderr, "Falha no read\n"); break; }
        if (pen.canvas.size() != frame.size()) pen.reset(frame.size());

        Detection d = det.detect(frame, &mask);
        pen.update(d);

        // compoe canvas sobre o frame
        cv::cvtColor(pen.canvas, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, inkMask, 0, 255, cv::THRESH_BINARY);
        pen.canvas.copyTo(frame, inkMask);

        if (d.found) {
            cv::rectangle(frame, d.bbox, {0,255,0}, 2);
            cv::circle(frame, cv::Point(d.center), 6, {0,0,255}, -1);
        }

        int64 t = cv::getTickCount();
        double dt = (t - tPrev) / cv::getTickFrequency(); tPrev = t;
        if (dt > 0) fps = (fps == 0) ? 1.0/dt : 0.9*fps + 0.1/dt;

        char line[128];
        std::snprintf(line, sizeof(line), "%s | FPS %.0f",
                      d.found ? "DETECTADA" : "NAO DETECTADA", fps);
        cv::putText(frame, line, {10, 28}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    d.found ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255), 2);

        cv::imshow(win, frame);
        int k = cv::waitKey(1);
        if (k == 27) break;
        if (k == 'c' || k == 'C') pen.reset(pen.canvas.size());
        if (k == 'p' || k == 'P') pen.penDown = !pen.penDown;
    }
    cam.close();
    return 0;
}