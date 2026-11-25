#include "game/duel_screen.h"
#include "game/match.h"
#include "game/menu_ui.h"
#include "game/specials.h"
#include "game/spells.h"
#include "render/hud.h"
#include "render/special_fx.h"
#include "render/sprites.h"
#include "vision/split_screen.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace
{
    // Recorta a zona de um jogador mantendo a proporcao da janela da camera.
    cv::Mat makeCameraView(const cv::Mat &source, const cv::Rect &zone, const cv::Size &outputSize)
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
        cv::Mat view;
        cv::resize(source(cv::Rect(x, y, cropWidth, cropHeight)), view, outputSize, 0, 0, cv::INTER_AREA);
        return view;
    }
} // namespace

void DuelScreen::begin(const MenuUI &menu, Match &match, Assets &assets)
{
    const int idx[2] = {menu.selectedP1(), menu.selectedP2()};
    std::string file[2] = {menu.characterPlayerImage(idx[0]), menu.characterPlayerImage(idx[1])};
    if (file[0].empty())
        file[0] = "mage1.png";
    if (file[1].empty())
        file[1] = "Idle.png";
    assets.load("assets", file[0], file[1], menu.characterId(idx[0]), menu.characterId(idx[1]));

    for (int p = 0; p < 2; ++p)
    {
        side_[p].name = menu.characterName(idx[p]);
        side_[p].portrait = menu.characterPortrait(idx[p]);
        side_[p].special = specialFor(menu.characterId(idx[p]));
        match.setSpecial(p, side_[p].special);
        status_[p].clear();
        statusAge_[p] = 99.f;
    }
    match.reset(); // mantem os especiais definidos acima
}

void DuelScreen::onGesture(int p, const ShapeResult &result, Match &match)
{
    const Shape s = result.shape;
    if (s == Shape::Nenhum)
    {
        if (result.resultText.empty()) // nada foi desenhado
            return;
        statusOk_[p] = false;
        status_[p] = "Forma nao reconhecida";
        statusAge_[p] = 0.f;
        return;
    }

    statusOk_[p] = match.cast(p, s);
    if (statusOk_[p])
        status_[p] = s == Shape::Estrela ? std::string(specialDef(match.special(p)).name) : spellFor(s).name;
    else if (match.inIntro())
        status_[p] = "Aguarde o inicio!";
    else if (match.mana(p) < spellFor(s).manaCost)
        status_[p] = "Mana insuficiente";
    else
        status_[p] = "Aguarde a recarga";
    statusAge_[p] = 0.f;
}

void DuelScreen::tick(double dt)
{
    for (float &a : statusAge_)
        a += float(dt);
}

void DuelScreen::render(cv::Mat &display, cv::Mat &frame, const SplitScreen &game, const Match &match,
                        const Assets &assets, bool debug, bool glow) const
{
    const int width = frame.cols;
    const int height = frame.rows;
    const int hudHeight = std::max(64, height * 18 / 100);
    const int arenaHeight = std::max(100, height * 30 / 100);
    const int cameraHeight = std::max(1, height - hudHeight - arenaHeight);
    const int margin = std::max(8, width / 80);
    const int cameraWidth = std::max(1, width / 2 - 2 * margin);
    const int cameraPanelHeight = std::max(1, cameraHeight - 2 * margin);
    const int playerWidth = width / 2;

    display.create(height, width, CV_8UC3);
    display.setTo(cv::Scalar(22, 14, 30));

    // HUD: retratos, vida, mana, especial e relogio
    cv::Mat hud = display(cv::Rect(0, 0, width, hudHeight));
    drawCombatHud(hud, match, assets, side_);

    // cameras dos dois jogadores (ou as mascaras de cor no modo debug)
    cv::Mat cameraSource;
    if (debug)
        cv::cvtColor(game.combinedMask(), cameraSource, cv::COLOR_GRAY2BGR);
    else
    {
        cameraSource = frame;
        game.draw(cameraSource); // tinta da varinha, divisoria e marcadores
    }
    for (int p = 0; p < 2; ++p)
    {
        const cv::Rect camRect(p * playerWidth + (playerWidth - cameraWidth) / 2,
                               hudHeight + (cameraHeight - cameraPanelHeight) / 2, cameraWidth, cameraPanelHeight);
        makeCameraView(cameraSource, game.zone(p), camRect.size()).copyTo(display(camRect));
        drawCameraFrame(display, camRect, p, side_[p], match, status_[p], statusOk_[p], statusAge_[p]);
    }

    // arena com os magos e os feiticos
    cv::Mat arena = display(cv::Rect(0, height - arenaHeight, width, arenaHeight));
    drawGame(arena, match, assets, glow);
    cv::line(display, {0, height - arenaHeight}, {width, height - arenaHeight}, cv::Scalar(90, 210, 250), 2,
             cv::LINE_AA);

    // super: faixa com o retrato e o nome do especial atravessando a area das cameras
    cv::Mat upper = display(cv::Rect(0, 0, width, height - arenaHeight));
    for (int p = 0; p < 2; ++p)
    {
        const FighterAnim a = match.anim(p);
        if (a.castType == SpellType::AtaqueEspecial && a.castT >= 0.f && a.koT < 0.f)
            drawSuperCutIn(upper, p, a.castT, side_[p].portrait, side_[p].name, side_[p].special);
    }

    // Derzu: o especial "bugga" a tela numa faixa que acompanha o bloco de codigo
    // (usa o relogio da partida: com o jogo pausado, o bug congela junto)
    for (const Projectile &pr : match.projectiles())
        if (pr.type == SpellType::AtaqueEspecial && pr.special == SpecialKind::ChuvaDeCodigo && pr.t >= 0.f &&
            pr.t < 1.f)
            drawScreenBug(display, arenaProjectileX(pr, width), match.time(), pr.owner);
}
