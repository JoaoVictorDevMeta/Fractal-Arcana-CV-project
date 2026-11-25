#include "core/app.h"
#include "core/hsv_range.h"
#include "render/ui_widgets.h"
#include <opencv2/highgui.hpp>
#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

App::App(AppOptions opt) : opt_(std::move(opt)), scores_(opt_.scoreFile) {}

bool App::init()
{
    if (!cam_.open(opt_.camera))
    {
        std::fprintf(stderr, "Erro: nao abriu a camera %d\n", opt_.camera);
        return false;
    }

    // cores das varinhas: P1 calibrada em config/, P2 verde por padrao
    game_.detector[1].range = HsvRange{40, 85, 100, 255, 100, 255};
    if (!loadRange("config/vision_p1.yaml", game_.detector[0].range))
        loadRange("config/vision.yaml", game_.detector[0].range);
    loadRange("config/vision_p2.yaml", game_.detector[1].range);

    menu_.loadCharacters("assets");

    // placar do ultimo combate salvo na sessao anterior
    LastMatch last;
    if (scores_.load(last))
        menu_.setLastMatch(last);

    cv::namedWindow(kWindow, cv::WINDOW_NORMAL);
    cv::resizeWindow(kWindow, 960, 600);
    cv::setMouseCallback(kWindow, &App::onMouse_, this);
    return true;
}

int App::run()
{
    while (step_())
    {
    }
    saveScore_();
    cam_.close();
    return 0;
}

void App::onMouse_(int event, int x, int y, int, void *self)
{
    auto *app = static_cast<App *>(self);
    if (!app)
        return;
    if (event == cv::EVENT_MOUSEMOVE)
        app->menu_.onMouseMove(x, y);
    else if (event == cv::EVENT_LBUTTONDOWN)
        app->menu_.onMouseClick(x, y, app->frameSize_);
}

bool App::step_()
{
    if (!cam_.read(frame_))
        return false;
    frameSize_ = frame_.size();
    const bool wasInDuel = menu_.screen() == GameScreen::DuelReady;

    const double dt = timer_.tick();
    if (!paused_)
    {
        game_.update(frame_, timer_.now()); // deteccao + Kalman das duas varinhas
        duel_.tick(dt);
        handleStrokes_();
    }

    if (menu_.screen() == GameScreen::DuelReady)
        updateDuel_(dt);
    else
    {
        inDuel_ = false; // permite trocar de personagem no proximo duelo
        if (!paused_)
            menu_.update(frame_.size(), game_.player(0).trk, game_.player(1).trk, dt);
    }

    present_();
    if (!handleKey_(cv::waitKey(1)))
        return false;

    // saiu do duelo (tecla M ou clique em "Menu") antes do nocaute: a partida tambem conta
    if (wasInDuel && menu_.screen() != GameScreen::DuelReady)
        recordIfPlayed_();
    return true;
}

void App::handleStrokes_()
{
    for (int p = 0; p < 2; ++p)
    {
        AirPen &pen = game_.pen[p];
        if (!pen.hasFinishedStroke())
            continue;
        if (menu_.screen() == GameScreen::DuelReady)
        {
            std::vector<cv::Point2f> stroke = pen.getStroke();
            duel_.onGesture(p, classifier_.classify(stroke), match_);
        }
        pen.resetStroke();
        pen.canvas.setTo(cv::Scalar::all(0));
    }
}

void App::updateDuel_(double dt)
{
    if (!inDuel_) // primeiro quadro do duelo
    {
        duel_.begin(menu_, match_, assets_);
        inDuel_ = true;
        matchRecorded_ = false;
    }
    if (!paused_)
    {
        // media exponencial do dt (com teto): evita saltos quando um frame demora mais
        matchDt_ += (std::min(dt, 0.1) - matchDt_) * 0.2;
        match_.update(matchDt_);
    }
    if (!matchRecorded_ && match_.state() != MatchState::Running) // nocaute
        recordMatch_();
}

void App::present_()
{
    if (menu_.screen() != GameScreen::DuelReady)
    {
        menu_.draw(frame_);
        if (paused_)
            ui::pausedOverlay(frame_);
        cv::imshow(kWindow, frame_);
        return;
    }
    cv::Mat display;
    duel_.render(display, frame_, game_, match_, assets_, debug_, menu_.glowEnabled());
    if (paused_)
        ui::pausedOverlay(display);
    cv::imshow(kWindow, display);
}

bool App::handleKey_(int key)
{
    switch (key)
    {
    case 27: // ESC: sai (o placar e salvo em run())
        recordIfPlayed_();
        return false;
    case 'r':
    case 'R':
        recordIfPlayed_();
        menu_.resetSelection();
        match_.reset();
        matchRecorded_ = false;
        break;
    case ' ':
        paused_ = !paused_;
        break;
    case 'm':
    case 'M':
        menu_.setScreen(GameScreen::Title);
        break;
    case 'd':
    case 'D':
        debug_ = !debug_;
        if (!debug_)
            cv::destroyWindow("Mascaras");
        break;
    default:
        // teclado simula o classificador para testes sem varinha:
        // P1: 1=Linha 2=Raio 3=Circulo 4=Triangulo 5=Estrela | P2: 6..9 e 0=Estrela
        if (key >= '1' && key <= '5')
            match_.cast(0, static_cast<Shape>(key - '1'));
        else if (key >= '6' && key <= '9')
            match_.cast(1, static_cast<Shape>(key - '6'));
        else if (key == '0')
            match_.cast(1, Shape::Estrela);
        break;
    }
    return true;
}

void App::recordMatch_()
{
    LastMatch lm;
    lm.valid = true;
    for (int p = 0; p < 2; ++p)
    {
        lm.name[p] = duel_.side(p).name;
        lm.score[p] = match_.score(p);
    }
    lm.winner = match_.winner();
    menu_.setLastMatch(lm);
    matchRecorded_ = true;
}

void App::recordIfPlayed_()
{
    if (inDuel_ && !matchRecorded_ && match_.time() > kIntroTime)
        recordMatch_();
}

void App::saveScore_()
{
    if (menu_.lastMatch().valid)
        scores_.save(menu_.lastMatch());
}
