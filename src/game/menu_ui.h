#pragma once
#include "vision/wand_detector.h"
#include "vision/wand_kalman.h"
#include <opencv2/core.hpp>
#include <array>
#include <string>
#include <vector>

enum class GameScreen {
    Title,
    Info,
    CharacterSelect,
    DuelReady
};

struct CharacterDef {
    std::string id;
    std::string name;
    std::string imagePath;
    cv::Mat portrait;       // carregada do disco ou gerada como fallback
    cv::Rect cardRect;      // area na tela de selecao
    float drawProgress[2] = {0.f, 0.f}; // progresso desenhado pela varinha de P1 e P2
    cv::Mat drawLayer;      // tracos desenhados sobre o card
};

class MenuUI {
public:
    MenuUI();

    // Carrega as 6 imagens dos personagens (com fallback caso algum arquivo nao exista ainda)
    bool loadCharacters(const std::string& assetsDir = "assets");

    // Atualiza estado a partir das posicoes das varinhas (P1 e P2) e/ou clique de mouse
    void update(const cv::Size& frameSize,
                const Tracked& p1,
                const Tracked& p2,
                double dt);

    // Desenha a tela atual sobre o frame
    void draw(cv::Mat& frame) const;

    // Eventos auxiliares (mouse / teclado)
    void onMouseClick(int x, int y, const cv::Size& frameSize);
    void resetSelection();

    GameScreen screen() const { return screen_; }
    void setScreen(GameScreen s);

    // Indice do personagem escolhido por P1 e P2 (-1 = ainda nao escolheu)
    int selectedP1() const { return chosen_[0]; }
    int selectedP2() const { return chosen_[1]; }
    std::string characterName(int idx) const;

private:
    void ensureBackground_(const cv::Size& size);
    void buildTitleLayout_(const cv::Size& size);
    void buildCharacterLayout_(const cv::Size& size);

    void drawPixelPanel_(cv::Mat& img, const cv::Rect& r,
                         const cv::Scalar& fill,
                         const cv::Scalar& borderOuter,
                         const cv::Scalar& borderInner,
                         bool highlight = false) const;

    void drawMedievalBackground_(cv::Mat& bg) const;
    void drawTitleScreen_(cv::Mat& frame) const;
    void drawInfoScreen_(cv::Mat& frame) const;
    void drawCharacterSelect_(cv::Mat& frame) const;

    void processWandOnTitle_(int playerIdx, const Tracked& trk, double dt);
    void processWandOnSelect_(int playerIdx, const Tracked& trk, double dt);

    GameScreen screen_ = GameScreen::Title;
    cv::Size lastSize_{0, 0};
    cv::Mat bgCache_;

    // Botoes da tela inicial
    cv::Rect titleFrameRect_;
    cv::Rect btnPlayRect_;
    cv::Rect btnInfoRect_;
    cv::Rect btnBackRect_;

    // Hover / progresso de ativacao por varinha nos botoes iniciais
    float playHover_[2] = {0.f, 0.f};
    float infoHover_[2] = {0.f, 0.f};
    float backHover_[2] = {0.f, 0.f};

    std::vector<CharacterDef> chars_;
    int chosen_[2] = {-1, -1};
    cv::Point2f lastWandPos_[2]{{0.f, 0.f}, {0.f, 0.f}};
    bool hasLastWand_[2]{false, false};
};