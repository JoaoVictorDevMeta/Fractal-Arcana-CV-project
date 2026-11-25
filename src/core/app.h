#pragma once
#include "core/timer.h"
#include "game/duel_screen.h"
#include "game/initials_entry.h"
#include "game/match.h"
#include "game/menu_ui.h"
#include "game/score_store.h"
#include "gesture/shape_classif.h"
#include "render/sprites.h"
#include "vision/camera.h"
#include "vision/split_screen.h"
#include <opencv2/core.hpp>
#include <string>

struct AppOptions
{
    int camera = 0;                                     // indice da webcam (--camera N)
    std::string scoreFile = "config/placares.txt"; // todos os combates (tabela de recordes)
};

// Aplicacao do jogo: abre a camera e a janela, e roda o loop principal
// (varinhas -> gestos -> menu ou duelo -> desenho -> teclado).
class App
{
public:
    explicit App(AppOptions opt);

    bool init(); // false se a camera nao abriu
    int run();   // loop principal; termina no ESC (salvando o combate em andamento)

private:
    bool step_();                  // um quadro do jogo; false = sair
    void handleStrokes_();         // gestos terminados viram feiticos no duelo
    void updateDuel_(double dt);   // entra no duelo, avanca a partida, guarda o placar no K.O.
    void present_();               // desenha a tela atual e mostra na janela
    bool handleKey_(int key);      // false = sair
    void onKnockout_();                       // K.O.: guarda o resultado e abre o registro de 3 letras
    void finishMatch_();                      // saindo do duelo: salva o combate que ainda nao foi salvo
    void savePending_(const std::string &id); // grava o combate com o registro escolhido
    ScoreRecord currentRecord_() const;       // resultado atual da partida
    static void onMouse_(int event, int x, int y, int flags, void *self);

    static constexpr const char *kWindow = "Fractal Arcana";

    AppOptions opt_;
    Camera cam_;
    SplitScreen game_;
    MenuUI menu_;
    ShapeClassifier classifier_;
    Match match_;
    Assets assets_;
    DuelScreen duel_;
    ScoreStore scores_;
    InitialsEntry entry_;     // registro de 3 letras no fim da luta
    ScoreRecord pending_;     // combate terminado esperando o registro
    bool hasPending_ = false;
    FrameTimer timer_;

    cv::Mat frame_;
    cv::Size frameSize_{1280, 720};
    bool inDuel_ = false;        // assets do duelo carregados
    bool matchRecorded_ = false; // resultado desta partida ja foi guardado
    bool debug_ = false;
    bool paused_ = false;
    double matchDt_ = 1.0 / 30.0; // dt suavizado: movimento dos feiticos fica regular
};
