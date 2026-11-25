#pragma once
#include "game/score_store.h"
#include "vision/wand_detector.h"
#include "vision/wand_kalman.h"
#include <opencv2/core.hpp>
#include <array>
#include <string>
#include <vector>

enum class GameScreen
{
    Title,
    Info,
    CharacterSelect,
    DuelReady
};

struct CharacterDef
{
    std::string id;
    std::string name;
    std::string selectImage;            // xxx_select.{png,jpeg}  retrato no menu
    std::string playerImage;            // xxx_player.png         spritesheet no jogo
    std::string title;                  // epiteto mostrado na selecao
    cv::Mat portrait;                   // carregada do disco ou gerada como fallback
    cv::Rect cardRect;                  // area na tela de selecao
    float drawProgress[2] = {0.f, 0.f}; // progresso desenhado pela varinha de P1 e P2
    cv::Mat drawLayer;                  // tracos desenhados sobre o card

    // caches de desenho (refeitos quando o tamanho da tela muda)
    cv::Mat fullBody;   // sprite do jogo (BGRA) recortado no corpo
    cv::Mat cardImg;    // retrato recortado para preencher o card
    cv::Mat preview[2]; // corpo inteiro para o painel lateral de P1 (0) e P2 (1, espelhado)
};

class MenuUI
{
public:
    // Tabela de recordes da tela inicial (ja filtrada e ordenada: ScoreStore::recentBest).
    void setScoreboard(std::vector<ScoreRecord> rows) { board_ = std::move(rows); }
    MenuUI();

    bool loadCharacters(const std::string &assetsDir = "assets");
    void update(const cv::Size &frameSize,
                const Tracked &p1,
                const Tracked &p2,
                double dt);
    void draw(cv::Mat &frame) const;

    void onMouseClick(int x, int y, const cv::Size &frameSize);
    void onMouseMove(int x, int y);
    void resetSelection();

    GameScreen screen() const { return screen_; }
    void setScreen(GameScreen s);

    int selectedP1() const { return chosen_[0]; }
    int selectedP2() const { return chosen_[1]; }
    std::string characterName(int idx) const;
    std::string characterImagePath(int idx) const;
    std::string characterPlayerImage(int idx) const;
    std::string characterId(int idx) const;
    const cv::Mat &characterPortrait(int idx) const;

    bool glowEnabled() const { return glowEnabled_; }
    void setGlowEnabled(bool on) { glowEnabled_ = on; }

private:
    void ensureBackground_(const cv::Size &size);
    void buildTitleLayout_(const cv::Size &size);
    void buildCharacterLayout_(const cv::Size &size);
    void loadButtonImages_();
    bool drawImageButton_(cv::Mat &frame, int idx, const cv::Rect &hit,
                          float prog, const cv::Scalar &barCol) const;

    void drawPixelPanel_(cv::Mat &img, const cv::Rect &r,
                         const cv::Scalar &fill,
                         const cv::Scalar &borderOuter,
                         const cv::Scalar &borderInner,
                         bool highlight = false,
                         const cv::Scalar &cornerCol = cv::Scalar(70, 215, 245)) const;

    void drawTitleScreen_(cv::Mat &frame) const;
    void updateManaBlocks_(const cv::Size &size, double dt);
    void drawManaBlocks_(cv::Mat &frame) const;
    void drawInfoScreen_(cv::Mat &frame) const;
    void drawCharacterSelect_(cv::Mat &frame) const;

    void processWandOnTitle_(int playerIdx, const Tracked &trk, double dt);
    void processWandOnSelect_(int playerIdx, const Tracked &trk, double dt);

    struct ManaBlock
    {
        float x, y;
        float size;
        float speed;
        float drift;
        float phase;
        float angle;
        float spin;
        int colorIdx;
    };
    std::vector<ManaBlock> manaBlocks_;
    float titleTime_ = 0.f;
    unsigned rngState_ = 1234567u;

    GameScreen screen_ = GameScreen::Title;
    cv::Size lastSize_{0, 0};
    cv::Mat bgCache_;
    cv::Mat titleBgCache_;

    // Logo da tela inicial e seu brilho.
    // Carregada de assets/main_title.png.
    cv::Mat titleLogoSrc_;
    cv::Mat titleLogoNorm_;
    cv::Mat titleLogoGlow_;
    int titleLogoGlowPad_ = 0;

    cv::Rect titleFrameRect_;
    cv::Rect btnPlayRect_;
    cv::Rect btnInfoRect_;
    cv::Rect btnBackRect_;
    cv::Rect btnGlowRect_;

    float playHover_[2] = {0.f, 0.f};
    float infoHover_[2] = {0.f, 0.f};
    float backHover_[2] = {0.f, 0.f};
    float glowHover_[2] = {0.f, 0.f};
    bool glowLatch_[2] = {false, false};
    bool glowEnabled_ = true;

    enum
    {
        kBtnPlay = 0,
        kBtnInfo,
        kBtnEffectsOn,
        kBtnEffectsOff,
        kBtnCount
    };
    cv::Mat btnSrc_[kBtnCount];
    cv::Mat btnNorm_[kBtnCount];
    cv::Mat btnHot_[kBtnCount];
    cv::Point btnOffset_[kBtnCount];
    cv::Mat btnGlow_[kBtnCount];
    int btnGlowPad_ = 0;
    float btnHover_[3] = {0.f, 0.f, 0.f};
    cv::Point mousePos_{-1, -1};
    cv::Rect2f btnInner_{0.08f, 0.15f, 0.84f, 0.70f};
    bool btnLoaded_ = false;
    bool btnImagesOk_ = false;

    std::vector<CharacterDef> chars_;
    int chosen_[2] = {-1, -1};
    int hoverIdx_[2] = {-1, -1}; // card sob a varinha de cada jogador
    cv::Rect previewRect_[2];    // paineis laterais da selecao (P1 esquerda, P2 direita)
    cv::Rect selectHeader_;
    cv::Rect selectFooter_;
    std::vector<ScoreRecord> board_;
    void drawScoreboard_(cv::Mat &frame) const;
    int hoveredCard_(cv::Point pt) const;
    void drawCharacterPreview_(cv::Mat &frame, int p) const;
    cv::Point2f lastWandPos_[2]{{0.f, 0.f}, {0.f, 0.f}};
    bool hasLastWand_[2]{false, false};
};
