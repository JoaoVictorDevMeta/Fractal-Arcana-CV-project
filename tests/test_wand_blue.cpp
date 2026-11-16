// tests/test_wand_blue.cpp
//
// Teste da varinha AZUL: verifica se o OpenCV a identifica e se o desenho no ar funciona.
//
// Dois modos:
//   --selftest         Autoteste sintético (sem câmera). Gera imagens com pontos azuis e
//                      distratores (vermelho, verde, ciano, magenta) e confere centroide,
//                      rejeição de ruído e desenho da trajetória. Retorna 0 se tudo passar.
//   (padrão) ao vivo   Abre a webcam, detecta a ponta azul e desenha a trajetória na tela.
//
// Uso:
//   ./test_wand_blue --selftest
//   ./test_wand_blue [--camera N] [--config config/wand_blue.yaml]
//
// Teclas (modo ao vivo):
//   Clique esquerdo  amostra a cor sob o cursor e ajusta a faixa HSV automaticamente
//   P                liga/desliga a caneta (desenho)
//   C                limpa o desenho
//   M                mostra/oculta a janela da máscara
//   S                salva a faixa HSV em --config
//   ESC              sai

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// OpenCV 5 moveu contourArea, moments, boundingRect etc. para o módulo geometry
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry/2d.hpp>
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// Detector da varinha azul
// ============================================================================

// Faixa HSV do OpenCV: H em [0,179], S e V em [0,255]. Azul puro ~ H=120.
struct HsvRange
{
    int hLo = 100, hHi = 130;
    int sLo = 120, sHi = 255;
    int vLo = 100, vHi = 255;
};

struct Detection
{
    bool found = false;
    cv::Point2f center{0.f, 0.f};
    double area = 0.0;
    cv::Rect bbox;
};

class BlueWandDetector
{
public:
    HsvRange range;
    double minArea = 30.0; // px^2; abaixo disso é tratado como ruído

    Detection detect(const cv::Mat &bgr, cv::Mat *maskOut = nullptr) const
    {
        cv::Mat blurred, hsv, mask;
        cv::GaussianBlur(bgr, blurred, cv::Size(5, 5), 0);
        cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv,
                    cv::Scalar(range.hLo, range.sLo, range.vLo),
                    cv::Scalar(range.hHi, range.sHi, range.vHi),
                    mask);

        // Limpeza morfológica: remove pontos soltos e fecha buracos.
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel_);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel_);
        if (maskOut)
            *maskOut = mask;

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        int best = -1;
        double bestArea = 0.0;
        for (size_t i = 0; i < contours.size(); ++i)
        {
            double a = cv::contourArea(contours[i]);
            if (a > bestArea)
            {
                bestArea = a;
                best = static_cast<int>(i);
            }
        }

        Detection d;
        if (best < 0 || bestArea < minArea)
            return d;

        cv::Moments m = cv::moments(contours[best]);
        if (m.m00 <= 0.0)
            return d;

        d.found = true;
        d.area = bestArea;
        d.center = cv::Point2f(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00));
        d.bbox = cv::boundingRect(contours[best]);
        return d;
    }

private:
    cv::Mat kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
};

// ============================================================================
// Caneta no ar: acumula a trajetória num canvas
// ============================================================================

class AirPen
{
public:
    cv::Mat canvas;
    bool penDown = true;
    float alpha = 0.5f; // suavização exponencial (1.0 = sem suavização)
    int maxMissed = 5;  // frames sem detecção antes de quebrar o traço
    int thickness = 4;
    cv::Scalar color{0, 255, 255}; // amarelo, contrasta com a varinha azul

    void reset(const cv::Size &size)
    {
        canvas = cv::Mat::zeros(size, CV_8UC3);
        hasLast_ = hasSmooth_ = false;
        missed_ = 0;
    }

    void update(const Detection &d)
    {
        if (canvas.empty())
            return;
        if (!penDown)
        { // caneta levantada: o próximo traço começa do zero
            hasLast_ = hasSmooth_ = false;
            return;
        }
        if (!d.found)
        {
            if (++missed_ > maxMissed)
                hasLast_ = hasSmooth_ = false;
            return;
        }
        missed_ = 0;

        cv::Point2f p = d.center;
        if (hasSmooth_)
            p = alpha * d.center + (1.f - alpha) * smooth_;
        smooth_ = p;
        hasSmooth_ = true;

        if (hasLast_)
        {
            cv::line(canvas, cv::Point(cvRound(last_.x), cvRound(last_.y)),
                     cv::Point(cvRound(p.x), cvRound(p.y)), color, thickness, cv::LINE_AA);
        }
        last_ = p;
        hasLast_ = true;
    }

private:
    cv::Point2f last_{0.f, 0.f}, smooth_{0.f, 0.f};
    bool hasLast_ = false, hasSmooth_ = false;
    int missed_ = 0;
};

// ============================================================================
// Persistência da faixa HSV (YAML)
// ============================================================================

static bool saveRange(const std::string &path, const HsvRange &r)
{
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened())
        return false;
    fs << "wand_blue" << "{"
       << "h_min" << r.hLo << "h_max" << r.hHi
       << "s_min" << r.sLo << "s_max" << r.sHi
       << "v_min" << r.vLo << "v_max" << r.vHi << "}";
    return true;
}

static bool loadRange(const std::string &path, HsvRange &r)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened())
        return false;
    cv::FileNode n = fs["wand_blue"];
    if (n.empty())
        return false;
    n["h_min"] >> r.hLo;
    n["h_max"] >> r.hHi;
    n["s_min"] >> r.sLo;
    n["s_max"] >> r.sHi;
    n["v_min"] >> r.vLo;
    n["v_max"] >> r.vHi;
    return true;
}

// ============================================================================
// Autoteste sintético
// ============================================================================

static int g_total = 0, g_fail = 0;

static void check(bool ok, const char *fmt, ...)
{
    ++g_total;
    if (!ok)
        ++g_fail;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::printf("  [%s] %s\n", ok ? " OK  " : "FALHA", buf);
}

// Cores em BGR
static const cv::Scalar kBlue(255, 80, 0);    // H ~ 110
static const cv::Scalar kPureBlue(255, 0, 0); // H = 120
static const cv::Scalar kRed(0, 0, 255);
static const cv::Scalar kGreen(0, 255, 0);
static const cv::Scalar kMagenta(255, 0, 255); // H = 150, o par "verde e magenta" do README
static const cv::Scalar kCyan(255, 255, 0);    // H = 90, vizinho mais próximo do azul

static cv::Mat makeBackground()
{
    return cv::Mat(480, 640, CV_8UC3, cv::Scalar(30, 30, 30));
}

static bool hasInk(const cv::Mat &canvas, cv::Point p, int r = 3)
{
    cv::Rect roi(p.x - r, p.y - r, 2 * r + 1, 2 * r + 1);
    roi &= cv::Rect(0, 0, canvas.cols, canvas.rows);
    cv::Mat g;
    cv::cvtColor(canvas(roi), g, cv::COLOR_BGR2GRAY);
    return cv::countNonZero(g) > 0;
}

static void testSingleBlue()
{
    std::printf("\n[1] Ponto azul isolado\n");
    BlueWandDetector det;
    for (const cv::Scalar &c : {kBlue, kPureBlue})
    {
        cv::Mat img = makeBackground();
        cv::circle(img, cv::Point(320, 240), 12, c, cv::FILLED);
        Detection d = det.detect(img);
        check(d.found, "azul BGR(%.0f,%.0f,%.0f) foi detectado", c[0], c[1], c[2]);
        if (d.found)
        {
            float err = cv::norm(d.center - cv::Point2f(320.f, 240.f));
            check(err < 2.0f, "centroide a %.2f px do centro real (tolerância 2 px)", err);
        }
    }
}

static void testDistractors()
{
    std::printf("\n[2] Somente cores que NÃO são azul\n");
    BlueWandDetector det;
    struct Case
    {
        const char *name;
        cv::Scalar color;
    };
    const Case cases[] = {{"vermelho", kRed}, {"verde", kGreen}, {"magenta", kMagenta}, {"ciano", kCyan}};
    for (const Case &c : cases)
    {
        cv::Mat img = makeBackground();
        cv::circle(img, cv::Point(320, 240), 20, c.color, cv::FILLED);
        check(!det.detect(img).found, "%s é ignorado", c.name);
    }
}

static void testBlueAmongDistractors()
{
    std::printf("\n[3] Azul entre distratores maiores\n");
    BlueWandDetector det;
    cv::Mat img = makeBackground();
    cv::circle(img, cv::Point(100, 100), 40, kRed, cv::FILLED);
    cv::circle(img, cv::Point(540, 120), 35, kGreen, cv::FILLED);
    cv::circle(img, cv::Point(120, 400), 30, kCyan, cv::FILLED);
    cv::circle(img, cv::Point(520, 380), 28, kMagenta, cv::FILLED);
    cv::circle(img, cv::Point(300, 260), 10, kBlue, cv::FILLED);
    Detection d = det.detect(img);
    check(d.found, "azul encontrado mesmo com 4 distratores maiores");
    if (d.found)
    {
        float err = cv::norm(d.center - cv::Point2f(300.f, 260.f));
        check(err < 2.0f, "centroide no azul (erro %.2f px)", err);
    }
}

static void testLargestBlob()
{
    std::printf("\n[4] Dois blobs azuis: escolhe o maior\n");
    BlueWandDetector det;
    cv::Mat img = makeBackground();
    cv::circle(img, cv::Point(150, 150), 8, kBlue, cv::FILLED);
    cv::circle(img, cv::Point(450, 300), 16, kBlue, cv::FILLED);
    Detection d = det.detect(img);
    check(d.found, "blob detectado");
    if (d.found)
    {
        float err = cv::norm(d.center - cv::Point2f(450.f, 300.f));
        check(err < 2.0f, "escolheu o blob maior (erro %.2f px)", err);
    }
}

static void testNoiseRejected()
{
    std::printf("\n[5] Ruído pequeno e frame vazio\n");
    BlueWandDetector det;
    cv::Mat img = makeBackground();
    cv::circle(img, cv::Point(200, 200), 2, kBlue, cv::FILLED);
    check(!det.detect(img).found, "ponto azul minúsculo (r=2) é rejeitado como ruído");
    check(!det.detect(makeBackground()).found, "frame sem azul não gera detecção");
}

static void testTrajectoryAndDrawing()
{
    std::printf("\n[6] Trajetória circular: detecção por frame + desenho\n");
    BlueWandDetector det;
    AirPen pen;
    pen.alpha = 1.0f; // sem suavização para comparar com a verdade
    pen.reset(cv::Size(640, 480));

    const int N = 72;
    const cv::Point2f c0(320.f, 240.f);
    const float R = 100.f;
    int detected = 0;
    float maxErr = 0.f;
    std::vector<cv::Point> truth;

    for (int i = 0; i < N; ++i)
    {
        double ang = 2.0 * CV_PI * i / N;
        cv::Point p(cvRound(c0.x + R * std::cos(ang)), cvRound(c0.y + R * std::sin(ang)));
        truth.push_back(p);

        cv::Mat img = makeBackground();
        cv::circle(img, p, 10, kBlue, cv::FILLED);
        Detection d = det.detect(img);
        if (d.found)
        {
            ++detected;
            maxErr = std::max(maxErr, static_cast<float>(cv::norm(d.center - cv::Point2f(p))));
        }
        pen.update(d);
    }

    check(detected == N, "detectou a varinha em %d/%d frames", detected, N);
    check(maxErr < 2.0f, "erro máximo de posição %.2f px (tolerância 2 px)", maxErr);

    int inked = 0;
    for (const cv::Point &p : truth)
        inked += hasInk(pen.canvas, p) ? 1 : 0;
    check(inked == N, "traço cobre %d/%d pontos da trajetória", inked, N);
    check(!hasInk(pen.canvas, cv::Point(320, 240)), "centro do círculo (fora da trajetória) está vazio");
}

static void testStrokeBreak()
{
    std::printf("\n[7] Perda da varinha quebra o traço\n");
    BlueWandDetector det;
    AirPen pen;
    pen.alpha = 1.0f;
    pen.maxMissed = 5;
    pen.reset(cv::Size(640, 480));

    const cv::Point A(100, 100), B(500, 400);
    auto frameAt = [&](const cv::Point *p)
    {
        cv::Mat img = makeBackground();
        if (p)
            cv::circle(img, *p, 10, kBlue, cv::FILLED);
        pen.update(det.detect(img));
    };

    for (int i = 0; i < 5; ++i)
        frameAt(&A);
    for (int i = 0; i < 10; ++i)
        frameAt(nullptr); // varinha some por 10 frames
    for (int i = 0; i < 5; ++i)
        frameAt(&B);

    check(hasInk(pen.canvas, A), "ponto A foi desenhado");
    check(hasInk(pen.canvas, B), "ponto B foi desenhado");
    check(!hasInk(pen.canvas, cv::Point(300, 250), 6), "nenhuma linha liga A a B depois da perda");
}

static int runSelfTest()
{
    std::printf("=== Autoteste: varinha azul (sem câmera) ===\n");
    testSingleBlue();
    testDistractors();
    testBlueAmongDistractors();
    testLargestBlob();
    testNoiseRejected();
    testTrajectoryAndDrawing();
    testStrokeBreak();
    std::printf("\nResultado: %d/%d verificações passaram\n", g_total - g_fail, g_total);
    return g_fail == 0 ? 0 : 1;
}

// ============================================================================
// Modo ao vivo (webcam)
// ============================================================================

static const char *kMainWin = "Varinha azul";
static const char *kMaskWin = "Mascara";
static const char *kCtrlWin = "Controles HSV";

struct LiveCtx
{
    cv::Mat clean; // último frame (espelhado), sem overlays
    BlueWandDetector *det = nullptr;
};

static void onMouse(int event, int x, int y, int, void *user)
{
    if (event != cv::EVENT_LBUTTONDOWN)
        return;
    auto *ctx = static_cast<LiveCtx *>(user);
    if (ctx->clean.empty())
        return;

    cv::Rect roi(x - 3, y - 3, 7, 7);
    roi &= cv::Rect(0, 0, ctx->clean.cols, ctx->clean.rows);
    if (roi.area() == 0)
        return;

    cv::Mat hsv;
    cv::cvtColor(ctx->clean(roi), hsv, cv::COLOR_BGR2HSV);
    cv::Scalar m = cv::mean(hsv);

    HsvRange &r = ctx->det->range;
    r.hLo = std::clamp(cvRound(m[0]) - 12, 0, 179);
    r.hHi = std::clamp(cvRound(m[0]) + 12, 0, 179);
    r.sLo = std::clamp(cvRound(m[1]) - 70, 40, 255);
    r.sHi = 255;
    r.vLo = std::clamp(cvRound(m[2]) - 70, 40, 255);
    r.vHi = 255;

    cv::setTrackbarPos("H min", kCtrlWin, r.hLo);
    cv::setTrackbarPos("H max", kCtrlWin, r.hHi);
    cv::setTrackbarPos("S min", kCtrlWin, r.sLo);
    cv::setTrackbarPos("V min", kCtrlWin, r.vLo);
    std::printf("Amostra HSV=(%.0f,%.0f,%.0f) -> H[%d,%d] S[%d,255] V[%d,255]\n",
                m[0], m[1], m[2], r.hLo, r.hHi, r.sLo, r.vLo);
}

static int runLive(int camera, const std::string &configPath)
{
    BlueWandDetector det;
    if (loadRange(configPath, det.range))
        std::printf("Faixa HSV carregada de %s\n", configPath.c_str());
    else
        std::printf("Usando faixa HSV padrão para azul (%s não encontrado)\n", configPath.c_str());

    cv::VideoCapture cap(camera);
    if (!cap.isOpened())
    {
        std::fprintf(stderr, "Erro: não foi possível abrir a câmera %d\n", camera);
        return 2;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS, 60);
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    cv::namedWindow(kMainWin);
    cv::namedWindow(kCtrlWin, cv::WINDOW_NORMAL);
    cv::resizeWindow(kCtrlWin, 400, 250);
    cv::createTrackbar("H min", kCtrlWin, &det.range.hLo, 179);
    cv::createTrackbar("H max", kCtrlWin, &det.range.hHi, 179);
    cv::createTrackbar("S min", kCtrlWin, &det.range.sLo, 255);
    cv::createTrackbar("S max", kCtrlWin, &det.range.sHi, 255);
    cv::createTrackbar("V min", kCtrlWin, &det.range.vLo, 255);
    cv::createTrackbar("V max", kCtrlWin, &det.range.vHi, 255);

    LiveCtx ctx;
    ctx.det = &det;
    cv::setMouseCallback(kMainWin, onMouse, &ctx);

    AirPen pen;
    bool showMask = true;
    double fps = 0.0;
    int64 tPrev = cv::getTickCount();

    std::printf("Clique na ponta da varinha para calibrar | P caneta | C limpar | M máscara | S salvar | ESC sair\n");

    cv::Mat frame, mask, display, gray, inkMask;
    for (;;)
    {
        if (!cap.read(frame) || frame.empty())
        {
            std::fprintf(stderr, "Falha ao ler frame da câmera\n");
            break;
        }
        cv::flip(frame, frame, 1); // espelha como no jogo
        if (pen.canvas.size() != frame.size())
            pen.reset(frame.size());
        frame.copyTo(ctx.clean);

        Detection d = det.detect(frame, &mask);
        pen.update(d);

        // Composição: frame + traço + marcadores da detecção
        frame.copyTo(display);
        cv::cvtColor(pen.canvas, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, inkMask, 0, 255, cv::THRESH_BINARY);
        pen.canvas.copyTo(display, inkMask);

        if (d.found)
        {
            cv::rectangle(display, d.bbox, cv::Scalar(0, 255, 0), 2);
            cv::circle(display, cv::Point(cvRound(d.center.x), cvRound(d.center.y)), 6,
                       cv::Scalar(0, 0, 255), cv::FILLED);
        }

        int64 tNow = cv::getTickCount();
        double dt = (tNow - tPrev) / cv::getTickFrequency();
        tPrev = tNow;
        if (dt > 0)
            fps = (fps == 0.0) ? 1.0 / dt : 0.9 * fps + 0.1 / dt;

        char line[160];
        std::snprintf(line, sizeof(line), "%s | centro=(%.0f,%.0f) area=%.0f | FPS %.0f | caneta %s",
                      d.found ? "DETECTADA" : "NAO DETECTADA", d.center.x, d.center.y, d.area,
                      fps, pen.penDown ? "ON" : "OFF");
        cv::putText(display, line, cv::Point(10, 28), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    d.found ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

        cv::imshow(kMainWin, display);
        if (showMask)
            cv::imshow(kMaskWin, mask);

        int key = cv::waitKey(1);
        if (key == 27)
            break;
        switch (key)
        {
        case 'p':
        case 'P':
            pen.penDown = !pen.penDown;
            break;
        case 'c':
        case 'C':
            pen.reset(pen.canvas.size());
            break;
        case 'm':
        case 'M':
            showMask = !showMask;
            if (!showMask)
                cv::destroyWindow(kMaskWin);
            break;
        case 's':
        case 'S':
            std::printf(saveRange(configPath, det.range) ? "Faixa HSV salva em %s\n"
                                                         : "Erro ao salvar em %s\n",
                        configPath.c_str());
            break;
        default:
            break;
        }
    }
    return 0;
}

// ============================================================================

int main(int argc, char **argv)
{
    int camera = 0;
    std::string configPath = "config/wand_blue.yaml";
    bool selftest = false;

    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--selftest"))
        {
            selftest = true;
        }
        else if (!std::strcmp(argv[i], "--camera") && i + 1 < argc)
        {
            camera = std::atoi(argv[++i]);
        }
        else if (!std::strcmp(argv[i], "--config") && i + 1 < argc)
        {
            configPath = argv[++i];
        }
        else
        {
            std::printf("Uso: %s [--selftest] [--camera N] [--config caminho.yaml]\n", argv[0]);
            return 1;
        }
    }
    return selftest ? runSelfTest() : runLive(camera, configPath);
}