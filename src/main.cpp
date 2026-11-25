#include "vision/camera.h"
#include "vision/wand_detector.h"
#include "vision/air_pen.h"
#include "gesture/shape_classif.h"
#include "render/combat_ui.h"
#include "render/hud.h"
#include "render/special_fx.h"
#include "render/sprites.h"
#include "game/match.h"
#include "game/spells.h"
#include "vision/split_screen.h"
#include "game/menu_ui.h"
#include "core/timer.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

struct MouseCtx
{
    MenuUI *ui = nullptr;
    cv::Size size{1280, 720};
};

static void onMouse(int event, int x, int y, int, void *userdata)
{
    auto *ctx = static_cast<MouseCtx *>(userdata);
    if (!ctx || !ctx->ui)
        return;
    if (event == cv::EVENT_MOUSEMOVE)
        ctx->ui->onMouseMove(x, y);
    else if (event == cv::EVENT_LBUTTONDOWN)
        ctx->ui->onMouseClick(x, y, ctx->size);
}

static cv::Mat makeCameraView(const cv::Mat &source, const cv::Rect &zone,
                              const cv::Size &outputSize)
{
    const double aspect = double(outputSize.width) / outputSize.height;
    int cropWidth = zone.width;
    int cropHeight = cvRound(cropWidth / aspect);
    if (cropHeight > zone.height)
    {
        cropHeight = zone.height;
        cropWidth = cvRound(cropHeight * aspect);
    }
    const int x = zone.x + (zone.width - cropWidth) / 2;
    const int y = zone.y + (zone.height - cropHeight) / 2;
    cv::Mat square;
    cv::resize(source(cv::Rect(x, y, cropWidth, cropHeight)), square,
               outputSize, 0, 0, cv::INTER_AREA);
    return square;
}

// Sobreposicao "PAUSADO" desenhada por cima do frame enquanto a pausa esta ativa.
static void drawPausedOverlay(cv::Mat &img)
{
    if (img.empty())
        return;
    const char *txt = "PAUSADO";
    const double scale = std::max(1.0, img.cols / 900.0);
    int baseline = 0;
    const cv::Size ts = cv::getTextSize(txt, cv::FONT_HERSHEY_DUPLEX, scale, 3, &baseline);
    const cv::Point org((img.cols - ts.width) / 2, (img.rows + ts.height) / 2);
    const cv::Rect box(org.x - 28, org.y - ts.height - 18, ts.width + 56, ts.height + 36);
    cv::rectangle(img, box, cv::Scalar(12, 10, 20), cv::FILLED);
    cv::rectangle(img, box, cv::Scalar(120, 230, 255), 2, cv::LINE_AA);
    cv::putText(img, txt, org, cv::FONT_HERSHEY_DUPLEX, scale,
                cv::Scalar(140, 245, 255), 3, cv::LINE_AA);
}

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

    SplitScreen game;
    game.detector[1].range = HsvRange{40, 85, 100, 255, 100, 255};
    if (!loadRange("config/vision_p1.yaml", game.detector[0].range))
        loadRange("config/vision.yaml", game.detector[0].range);
    loadRange("config/vision_p2.yaml", game.detector[1].range);

    MenuUI menu;
    menu.loadCharacters("assets");

    const char *camWin = "Fractal Arcana";
    cv::namedWindow(camWin, cv::WINDOW_NORMAL);
    cv::resizeWindow(camWin, 960, 600);
    MouseCtx mouseCtx{&menu, {1280, 720}};
    cv::setMouseCallback(camWin, onMouse, &mouseCtx);

    ShapeClassifier classifier;
    ShapeResult lastResult[2];
    double sinceMs[2] = {500.0, 500.0};
    std::string castStatus[2];             // resultado do ultimo gesto, mostrado na camera
    bool castOk[2] = {false, false};       // true = feitico lancado
    float statusAge[2] = {99.f, 99.f};     // segundos desde o ultimo gesto
    CombatSide side[2];                    // nome/retrato/especial de cada jogador na luta
    Match match;
    bool matchRecorded = false; // o placar desta partida ja foi enviado para a tela inicial?
    // Guarda o placar da partida atual como "ultima partida" (mostrada na tela inicial).
    auto recordMatch = [&]()
    {
        LastMatch lm;
        lm.valid = true;
        for (int p = 0; p < 2; ++p)
        {
            lm.name[p] = side[p].name;
            lm.score[p] = match.score(p);
        }
        lm.winner = match.winner();
        menu.setLastMatch(lm);
        matchRecorded = true;
    };
    // Reiniciar (R) no meio do duelo: a partida jogada tambem conta como a ultima.
    auto recordIfPlayed = [&]()
    {
        if (menu.screen() == GameScreen::DuelReady && !matchRecorded && match.time() > kIntroTime)
            recordMatch();
    };
    Assets assets;
    bool assetsLoaded = false;
    bool debug = false;
    bool paused = false;
    cv::Mat frame;
    FrameTimer frameTimer;
    double matchDt = 1.0 / 30.0; // dt suavizado: movimento dos feiticos fica regular

    for (;;)
    {
        if (!cam.read(frame))
            break;
        mouseCtx.size = frame.size();
        const GameScreen screenAtStart = menu.screen();

        const double dt = frameTimer.tick();
        const double nowSec = frameTimer.now();

        // Atualiza deteccao/Kalman das duas varinhas (congeladas quando pausado)
        if (!paused)
            game.update(frame, nowSec);
        Tracked t1 = game.player(0).trk;
        Tracked t2 = game.player(1).trk;

        if (!paused)
            for (int p = 0; p < 2; ++p)
            {
                sinceMs[p] += dt * 1000.0;
                statusAge[p] += float(dt);
                if (game.pen[p].hasFinishedStroke())
                {
                    if (menu.screen() == GameScreen::DuelReady)
                    {
                        std::vector<cv::Point2f> stroke = game.pen[p].getStroke();
                        lastResult[p] = classifier.classify(stroke);
                        const Shape s = lastResult[p].shape;
                        if (s != Shape::Nenhum)
                        {
                            castOk[p] = match.cast(p, s);
                            if (castOk[p])
                            {
                                sinceMs[p] = 0.0;
                                castStatus[p] = s == Shape::Estrela ? std::string(specialDef(match.special(p)).name)
                                                                    : spellFor(s).name;
                            }
                            else if (match.inIntro())
                                castStatus[p] = "Aguarde o inicio!";
                            else if (match.mana(p) < spellFor(s).manaCost)
                                castStatus[p] = "Mana insuficiente";
                            else
                                castStatus[p] = "Aguarde a recarga";
                            statusAge[p] = 0.f;
                        }
                        else if (!lastResult[p].resultText.empty())
                        {
                            castOk[p] = false;
                            castStatus[p] = "Forma nao reconhecida";
                            statusAge[p] = 0.f;
                        }
                    }
                    game.pen[p].resetStroke();
                    game.pen[p].canvas.setTo(cv::Scalar::all(0));
                }
            }

        if (menu.screen() == GameScreen::DuelReady)
        {
            // Carrega assets uma unica vez ao entrar no duelo
            if (!assetsLoaded)
            {
                int i1 = menu.selectedP1();
                int i2 = menu.selectedP2();
                std::string p1File = menu.characterPlayerImage(i1);
                std::string p2File = menu.characterPlayerImage(i2);
                if (p1File.empty())
                    p1File = "mage1.png";
                if (p2File.empty())
                    p2File = "Idle.png";
                assets.load("assets", p1File, p2File,
                            menu.characterId(i1), menu.characterId(i2));
                assetsLoaded = true;

                // nova luta: especiais de cada personagem e dados da interface
                const int idx[2] = {i1, i2};
                for (int p = 0; p < 2; ++p)
                {
                    side[p].name = menu.characterName(idx[p]);
                    side[p].portrait = menu.characterPortrait(idx[p]);
                    side[p].special = specialFor(menu.characterId(idx[p]));
                    match.setSpecial(p, side[p].special);
                    castStatus[p].clear();
                    statusAge[p] = 99.f;
                }
                match.reset();
                matchRecorded = false;
            }
            if (!paused)
            {
                // Media exponencial do dt (com teto): evita saltos quando um frame demora mais.
                matchDt += (std::min(dt, 0.1) - matchDt) * 0.2;
                match.update(matchDt);
            }
            // nocaute: guarda o placar para a tela inicial
            if (!matchRecorded && match.state() != MatchState::Running)
                recordMatch();
        }
        else
        {
            // Fora do duelo, reseta flag para permitir troca de personagem
            assetsLoaded = false;
            if (!paused)
                menu.update(frame.size(), t1, t2, dt);
            menu.draw(frame);
        }

        if (menu.screen() != GameScreen::DuelReady)
        {
            if (paused)
                drawPausedOverlay(frame);
            cv::imshow(camWin, frame);
        }
        else
        {
            const int width = frame.cols;
            const int height = frame.rows;
            const int hudHeight = std::max(64, height * 18 / 100);
            const int arenaHeight = std::max(100, height * 30 / 100);
            const int cameraY = hudHeight;
            const int cameraHeight = std::max(1, height - hudHeight - arenaHeight);
            const int margin = std::max(8, width / 80);
            const int cameraWidth = std::max(1, width / 2 - 2 * margin);
            const int cameraPanelHeight = std::max(1, cameraHeight - 2 * margin);

            cv::Mat display(height, width, CV_8UC3, cv::Scalar(22, 14, 30));
            cv::Mat hud = display(cv::Rect(0, 0, width, hudHeight));
            const int playerWidth = width / 2;
            drawCombatHud(hud, match, assets, side);

            cv::Mat cameraSource;
            if (debug)
            {
                cv::cvtColor(game.combinedMask(), cameraSource, cv::COLOR_GRAY2BGR);
            }
            else
            {
                cameraSource = frame;
                game.draw(cameraSource);
            }

            for (int p = 0; p < 2; ++p)
            {
                const cv::Size cameraSize(cameraWidth, cameraPanelHeight);
                const cv::Mat cameraView = makeCameraView(
                    cameraSource, game.zone(p), cameraSize);
                const int x = p * playerWidth + (playerWidth - cameraWidth) / 2;
                const int y = cameraY + (cameraHeight - cameraPanelHeight) / 2;
                const cv::Rect camRect(x, y, cameraWidth, cameraPanelHeight);
                cameraView.copyTo(display(camRect));
                drawCameraFrame(display, camRect, p, side[p], match, castStatus[p], castOk[p], statusAge[p]);
            }

            cv::Mat arena = display(cv::Rect(0, height - arenaHeight, width, arenaHeight));
            drawGame(arena, match, assets, menu.glowEnabled());
            cv::line(display, {0, height - arenaHeight}, {width, height - arenaHeight},
                     cv::Scalar(90, 210, 250), 2, cv::LINE_AA);

            // super: faixa com o retrato e o nome do especial atravessando a area das cameras
            cv::Mat upper = display(cv::Rect(0, 0, width, height - arenaHeight));
            for (int p = 0; p < 2; ++p)
            {
                const FighterAnim a = match.anim(p);
                if (a.castType == SpellType::AtaqueEspecial && a.castT >= 0.f && a.koT < 0.f)
                    drawSuperCutIn(upper, p, a.castT, side[p].portrait, side[p].name, side[p].special);
            }

            // Derzu: o especial "bugga" a tela numa faixa que acompanha o bloco de codigo
            // (usa o relogio da partida: com o jogo pausado, o bug congela junto)
            for (const Projectile &pr : match.projectiles())
                if (pr.type == SpellType::AtaqueEspecial && pr.special == SpecialKind::ChuvaDeCodigo &&
                    pr.t >= 0.f && pr.t < 1.f)
                    drawScreenBug(display, arenaProjectileX(pr, width), match.time(), pr.owner);
            if (paused)
                drawPausedOverlay(display);
            cv::imshow(camWin, display);
        }

        // --- TECLAS (um unico waitKey) ---
        const int k = cv::waitKey(1);
        if (k == 27)
            break;
        if (k == 'r' || k == 'R')
        {
            recordIfPlayed();
            menu.resetSelection();
            match.reset();
            matchRecorded = false;
        }
        if (k == ' ')
            paused = !paused;
        if (k == 'm' || k == 'M')
            menu.setScreen(GameScreen::Title);
        if (k == 'd' || k == 'D')
        {
            debug = !debug;
            if (!debug)
                cv::destroyWindow("Mascaras");
        }
        // teclado simula o classificador (P1): 1=Linha 2=Raio 3=Circulo 4=Triangulo 5=Estrela
        if (k >= '1' && k <= '5')
            match.cast(0, static_cast<Shape>(k - '1'));
        // idem para P2 (testar dano/nocaute sem varinha): 6=Linha 7=Raio 8=Circulo 9=Triangulo 0=Estrela
        if (k >= '6' && k <= '9')
            match.cast(1, static_cast<Shape>(k - '6'));
        if (k == '0')
            match.cast(1, Shape::Estrela);

        // saiu do duelo (tecla M ou clique em "Menu") antes do nocaute: a partida tambem conta
        if (screenAtStart == GameScreen::DuelReady && menu.screen() != GameScreen::DuelReady && !matchRecorded &&
            match.time() > kIntroTime)
            recordMatch();
    }

    cam.close();
    return 0;
}