#pragma once
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
    cv::Mat portrait;                   // carregada do disco ou gerada como fallback
    cv::Rect cardRect;                  // area na tela de selecao
    float drawProgress[2] = {0.f, 0.f}; // progresso desenhado pela varinha de P1 e P2
    cv::Mat drawLayer;                  // tracos desenhados sobre o card
};

class MenuUI
{
public:
    MenuUI();

    // Carrega as 6 imagens dos personagens (com fallback caso algum arquivo nao exista ainda)
    bool loadCharacters(const std::string &assetsDir = "assets");

    // Atualiza estado a partir das posicoes das varinhas (P1 e P2) e/ou clique de mouse
    void update(const cv::Size &frameSize,
                const Tracked &p1,
                const Tracked &p2,
                double dt);

    // Desenha a tela atual sobre o frame
    void draw(cv::Mat &frame) const;

    // Eventos auxiliares (mouse / teclado)
    void onMouseClick(int x, int y, const cv::Size &frameSize);
    void onMouseMove(int x, int y);
    void resetSelection();

    GameScreen screen() const { return screen_; }
    void setScreen(GameScreen s);

    // Indice do personagem escolhido por P1 e P2 (-1 = ainda nao escolheu)
    int selectedP1() const { return chosen_[0]; }
    int selectedP2() const { return chosen_[1]; }
    std::string characterName(int idx) const;

    // Retorna o nome do arquivo _select para uso externo (legado)
    std::string characterImagePath(int idx) const;
    // Retorna o nome do arquivo _player para carregar no jogo
    std::string characterPlayerImage(int idx) const;
    // Retorna o id curto do personagem (ex: "derzu", "baltar")
    std::string characterId(int idx) const;

    // Brilhos (glow) ligados/desligados na tela inicial; desligar melhora o desempenho
    bool glowEnabled() const { return glowEnabled_; }
    void setGlowEnabled(bool on) { glowEnabled_ = on; }

private:
    void ensureBackground_(const cv::Size &size);
    void buildTitleLayout_(const cv::Size &size);
    void buildCharacterLayout_(const cv::Size &size);
    void loadButtonImages_();
    // Desenha o botao-imagem 'idx' (ver kBtn*) alinhado ao retangulo de clique 'hit'.
    // Retorna false se as imagens nao foram carregadas (usa-se o desenho antigo).
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

    // Blocos de mana subindo no fundo da tela inicial
    struct ManaBlock
    {
        float x, y;   // centro (px)
        float size;   // lado (px)
        float speed;  // px/s para cima
        float drift;  // amplitude do balanco lateral (px)
        float phase;  // fase do balanco/pulso
        float angle;  // orientacao atual (rad)
        float spin;   // velocidade de rotacao (rad/s)
        int colorIdx; // indice na paleta
    };
    std::vector<ManaBlock> manaBlocks_;
    float titleTime_ = 0.f;
    unsigned rngState_ = 1234567u;

    GameScreen screen_ = GameScreen::Title;
    cv::Size lastSize_{0, 0};
    cv::Mat bgCache_;
    cv::Mat titleBgCache_; // assets/menu_image.jpg (fundo so da tela inicial)

    // Botoes da tela inicial
    cv::Rect titleFrameRect_;
    cv::Rect btnPlayRect_;
    cv::Rect btnInfoRect_;
    cv::Rect btnBackRect_;
    cv::Rect btnGlowRect_;

    // Hover / progresso de ativacao por varinha nos botoes iniciais
    float playHover_[2] = {0.f, 0.f};
    float infoHover_[2] = {0.f, 0.f};
    float backHover_[2] = {0.f, 0.f};
    float glowHover_[2] = {0.f, 0.f};
    bool glowLatch_[2] = {false, false}; // evita alternar em loop enquanto a varinha fica no botao
    bool glowEnabled_ = true;

    // Imagens dos botoes da tela inicial (assets/button_*.png, BGRA com alfa)
    enum
    {
        kBtnPlay = 0,
        kBtnInfo,
        kBtnEffectsOn,
        kBtnEffectsOff,
        kBtnCount
    };
    cv::Mat btnSrc_[kBtnCount];  // originais
    cv::Mat btnNorm_[kBtnCount]; // redimensionadas para o layout atual
    cv::Mat btnHot_[kBtnCount];  // versao mais clara, usada no hover
    cv::Point btnOffset_[kBtnCount];
    cv::Mat btnGlow_[kBtnCount]; // mascara 8U do halo branco (silhueta borrada, com margem btnGlowPad_)
    int btnGlowPad_ = 0;
    float btnHover_[3] = {0.f, 0.f, 0.f}; // suavizado 0..1: Jogar, Info, Efeitos (mouse ou varinha)
    cv::Point mousePos_{-1, -1};
    // Area visivel da moldura dentro da imagem (fracao da largura/altura; o resto e margem transparente)
    cv::Rect2f btnInner_{0.08f, 0.15f, 0.84f, 0.70f};
    bool btnLoaded_ = false;
    bool btnImagesOk_ = false;

    std::vector<CharacterDef> chars_;
    int chosen_[2] = {-1, -1};
    cv::Point2f lastWandPos_[2]{{0.f, 0.f}, {0.f, 0.f}};
    bool hasLastWand_[2]{false, false};
};