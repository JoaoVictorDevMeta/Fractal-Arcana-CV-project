#include "render/combat_ui.h"
#include "game/match.h"
#include "render/hud.h"
#include "render/pixel_fx.h"
#include "render/special_fx.h"
#include "render/sprites.h"
#include "render/ui_widgets.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>

namespace
{
    const int F = cv::FONT_HERSHEY_SIMPLEX, FD = cv::FONT_HERSHEY_DUPLEX;
    const cv::Scalar kWhite(245, 240, 250), kSoft(200, 185, 205), kGold(90, 210, 250);

    // Retrato recortado em cache (o recorte de uma foto grande nao acontece a cada frame).
    const cv::Mat &portraitFit(const cv::Mat &src, cv::Size size)
    {
        static std::map<std::array<long long, 3>, cv::Mat> cache;
        const std::array<long long, 3> key = {(long long)(std::uintptr_t)src.data, size.width, size.height};
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;
        if (cache.size() > 16)
            cache.clear();
        return cache.emplace(key, coverCrop(src, size, 0.28f)).first->second;
    }

    void copyClipped(cv::Mat &dst, const cv::Mat &src, cv::Point tl)
    {
        const cv::Rect want(tl, src.size());
        const cv::Rect vis = want & cv::Rect(0, 0, dst.cols, dst.rows);
        if (vis.empty() || src.type() != dst.type())
            return;
        src(cv::Rect(vis.x - want.x, vis.y - want.y, vis.width, vis.height)).copyTo(dst(vis));
    }
} // namespace

void drawCombatHud(cv::Mat &hud, const Match &m, const Assets &as, const CombatSide side[2])
{
    const int W = hud.cols, H = hud.rows;
    if (W <= 0 || H <= 0)
        return;
    const double k = std::clamp(H / 130.0, 0.5, 1.6);

    // fundo: faixa escura com filete dourado embaixo
    hud.setTo(cv::Scalar(24, 14, 32));
    ui::fillTranslucent(hud, {0, 0, W, H / 2}, cv::Scalar(36, 22, 46), 0.6);
    cv::line(hud, {0, H - 2}, {W, H - 2}, kGold * 0.7, 2);

    const int centerW = std::max(90, W / 11);
    const int areaW = (W - centerW) / 2;
    const int pad = std::max(6, H / 14);

    for (int p = 0; p < 2; ++p)
    {
        const cv::Scalar pc = ui::playerColor(p);
        const int x0 = p == 0 ? 0 : areaW + centerW;
        const int ps = H - 2 * pad; // retrato quadrado
        const cv::Rect pr(p == 0 ? x0 + pad : x0 + areaW - pad - ps, pad, ps, ps);

        // retrato (cinza no nocaute, vermelho ao apanhar)
        if (!side[p].portrait.empty())
        {
            cv::Mat face = portraitFit(side[p].portrait, pr.size()).clone();
            const FighterAnim a = m.anim(p);
            if (a.koT >= 0.f && face.channels() == 3)
            {
                cv::Mat g;
                cv::cvtColor(face, g, cv::COLOR_BGR2GRAY);
                cv::cvtColor(g, face, cv::COLOR_GRAY2BGR);
            }
            copyClipped(hud, face, pr.tl());
            if (a.hitFlash > 0.f)
                ui::fillTranslucent(hud, pr, cv::Scalar(40, 40, 255), 0.5 * a.hitFlash);
        }
        else
            cv::rectangle(hud, pr, pc * 0.4, cv::FILLED);
        cv::rectangle(hud, pr, cv::Scalar(10, 6, 14), 5);
        cv::rectangle(hud, pr, pc, 3);

        // area de texto e barras
        const int cx0 = p == 0 ? pr.br().x + pad : x0 + pad;
        const int cx1 = p == 0 ? x0 + areaW - pad : pr.x - pad;
        const int cw = std::max(10, cx1 - cx0);
        const int rowH = H * 32 / 100;

        // nome + etiqueta do jogador
        const std::string tag = p == 0 ? "P1" : "P2";
        const cv::Size tz = pfx::textSize(tag, FD, 0.5 * k, 1);
        const cv::Rect tagR(p == 0 ? cx0 : cx1 - tz.width - 10, pad, tz.width + 10, tz.height + 10);
        cv::rectangle(hud, tagR, pc, cv::FILLED);
        cv::putText(hud, tag, {tagR.x + 5, tagR.br().y - 5}, FD, 0.5 * k, kWhite, 1, cv::LINE_AA);

        // indicador do especial (estrela): acende quando ha mana para a Estrela
        const float cost = spellFor(Shape::Estrela).manaCost;
        const bool ready = m.mana(p) >= cost && m.state() == MatchState::Running;
        const std::string spTxt = ready ? "ESPECIAL!" : "ESPECIAL";
        const cv::Size sz = pfx::textSize(spTxt, F, 0.42 * k, 1);
        const int starR = std::max(5, int(8 * k));
        const int spW = starR * 2 + 6 + sz.width;
        const int spX = p == 0 ? cx1 - spW : cx0;
        const int spY = pad + tz.height / 2 + 5;
        const bool pulse = int(m.time() * 4.f) % 2 == 0;
        const cv::Scalar sc = specialColor(side[p].special);
        ui::drawStar(hud, {spX + starR, spY}, starR, ready ? (pulse ? sc : pfx::lighten(sc, 0.5)) : cv::Scalar(70, 60, 80),
                     true);
        cv::putText(hud, spTxt, {spX + starR * 2 + 6, spY + sz.height / 2}, F, 0.42 * k, ready ? sc : kSoft * 0.6, 1,
                    cv::LINE_AA);

        const int nameX0 = p == 0 ? tagR.br().x + 8 : spX + spW + 8;
        const int nameX1 = p == 0 ? spX - 8 : tagR.x - 8;
        const int nameW = std::max(10, nameX1 - nameX0);
        const double ns = ui::fitScale(side[p].name, FD, 0.6 * k, 1, nameW);
        const cv::Size nz = pfx::textSize(side[p].name, FD, ns, 1);
        const int nameX = p == 0 ? nameX0 : nameX1 - nz.width;
        pfx::outlinedText(hud, side[p].name, {nameX, tagR.br().y - 4}, FD, ns, kWhite, 1);

        // barras de vida e mana lado a lado (P2 espelhado: vida na borda de fora)
        const int recH = std::max(4, int(5 * k));
        const int barTop = pad + rowH;
        const int barMaxH = std::max(1, H - barTop - pad - recH - 4);
        const int gap = std::max(6, pad);
        const cv::Size bs = statBarSize(as, {std::max(1, (cw - gap) / 2), barMaxH});
        const int rowW = 2 * bs.width + gap;
        const int bx = p == 0 ? cx0 : cx1 - rowW;
        const int by = barTop + (barMaxH - bs.height) / 2;
        const cv::Rect healthR(p == 0 ? bx : bx + bs.width + gap, by, bs.width, bs.height);
        const cv::Rect manaR(p == 0 ? bx + bs.width + gap : bx, by, bs.width, bs.height);
        drawStatBar(hud, as, BarKind::Health, p, m.health01(p), healthR,
                    cv::format("%d/100", std::max(0, cvRound(m.health01(p) * 100.f))));
        drawStatBar(hud, as, BarKind::Mana, p, m.mana01(p), manaR, cv::format("%d/100", cvRound(m.mana01(p) * 100.f)));

        // recarga entre feiticos
        const float cd = std::clamp(m.cooldown(p) / 1.0f, 0.f, 1.f);
        const cv::Rect rec(bx, H - pad - recH, rowW, recH);
        cv::rectangle(hud, rec, cv::Scalar(45, 35, 55), cv::FILLED);
        if (cd > 0.f)
        {
            const int w = cvRound(rec.width * cd);
            cv::rectangle(hud, cv::Rect(p == 0 ? rec.x : rec.br().x - w, rec.y, w, rec.height), cv::Scalar(200, 200, 210),
                          cv::FILLED);
        }
    }

    // centro: "VS" e relogio da luta
    const cv::Rect cr(areaW + 4, pad, centerW - 8, H - 2 * pad);
    ui::panel(hud, cr, kGold, 0.9, cv::Scalar(20, 12, 28), 2);
    const int secs = int(m.time());
    const std::string clock = cv::format("%02d:%02d", secs / 60, secs % 60);
    const double cs = ui::fitScale(clock, FD, 0.8 * k, 2, cr.width - 14);
    const cv::Size cz = pfx::textSize(clock, FD, cs, 2);
    const cv::Size vz = pfx::textSize("VS", FD, 0.7 * k, 2);
    const int vy = cr.y + cr.height * 40 / 100;
    pfx::outlinedText(hud, "VS", {cr.x + (cr.width - vz.width) / 2, vy}, FD, 0.7 * k, kGold, 2);
    pfx::outlinedText(hud, clock, {cr.x + (cr.width - cz.width) / 2, cr.y + cr.height * 82 / 100}, FD, cs, kWhite, 2);
}

void drawCameraFrame(cv::Mat &display, const cv::Rect &r, int p, const CombatSide &side, const Match &m,
                     const std::string &status, bool statusOk, float statusAge)
{
    const double k = std::clamp(display.rows / 720.0, 0.6, 1.6);
    const cv::Scalar pc = ui::playerColor(p);

    // moldura com cantoneiras
    cv::rectangle(display, r, cv::Scalar(10, 6, 14), 6);
    cv::rectangle(display, r, pc, 3);
    const int b = std::max(6, int(10 * k));
    const cv::Point corners[4] = {r.tl(), {r.br().x - b, r.y}, {r.x, r.br().y - b}, {r.br().x - b, r.br().y - b}};
    for (const cv::Point &c : corners)
        cv::rectangle(display, cv::Rect(c.x - 2, c.y - 2, b + 4, b + 4), pfx::lighten(pc, 0.5), cv::FILLED);

    // etiqueta "P1 - Nome"
    const std::string label = std::string(p == 0 ? "P1  " : "P2  ") + side.name;
    const cv::Size lz = pfx::textSize(label, cv::FONT_HERSHEY_DUPLEX, 0.5 * k, 1);
    const cv::Rect lr(p == 0 ? r.x + b + 6 : r.br().x - b - 6 - lz.width - 16, r.y + b + 4, lz.width + 16,
                      lz.height + 12);
    ui::fillTranslucent(display, lr, pc * 0.45, 0.85);
    cv::putText(display, label, {lr.x + 8, lr.br().y - 6}, cv::FONT_HERSHEY_DUPLEX, 0.5 * k, kWhite, 1, cv::LINE_AA);

    // resultado do ultimo gesto (some depois de alguns segundos)
    if (!status.empty() && statusAge < 3.f)
    {
        const cv::Size sz = pfx::textSize(status, cv::FONT_HERSHEY_DUPLEX, 0.5 * k, 1);
        const cv::Rect sr(r.x + (r.width - sz.width) / 2 - 12, lr.br().y + 8, sz.width + 24, sz.height + 14);
        ui::fillTranslucent(display, sr, statusOk ? cv::Scalar(40, 90, 30) : cv::Scalar(30, 30, 110), 0.85);
        cv::rectangle(display, sr, statusOk ? cv::Scalar(120, 240, 130) : cv::Scalar(110, 110, 255), 1);
        cv::putText(display, status, {sr.x + 12, sr.br().y - 7}, cv::FONT_HERSHEY_DUPLEX, 0.5 * k, kWhite, 1,
                    cv::LINE_AA);
    }

    // legenda: forma -> custo de mana (apagada sem mana; Estrela dourada quando pronta)
    const Shape order[5] = {Shape::Linha, Shape::Raio, Shape::Circulo, Shape::Triangulo, Shape::Estrela};
    const int slots = 5;
    const int stripH = std::max(30, int(46 * k));
    const int slotW = std::min((r.width - 2 * b) / slots, int(stripH * 1.9));
    const int stripW = slotW * slots;
    const cv::Rect strip(r.x + (r.width - stripW) / 2, r.br().y - b - stripH - 4, stripW, stripH);
    if (strip.y <= lr.br().y)
        return; // camera pequena demais para a legenda
    ui::fillTranslucent(display, strip, cv::Scalar(14, 8, 20), 0.7);
    for (int i = 0; i < slots; ++i)
    {
        const SpellDef &sd = spellFor(order[i]);
        const bool can = m.mana(p) >= sd.manaCost;
        const bool star = order[i] == Shape::Estrela;
        const cv::Scalar col = !can ? cv::Scalar(90, 80, 100) : (star ? specialColor(side.special) : cv::Scalar(255, 220, 150));
        const cv::Rect slot(strip.x + i * slotW, strip.y, slotW, stripH);
        const cv::Rect icon(slot.x + 4, slot.y + 4, stripH - 8, stripH - 8);
        ui::drawShapeIcon(display, order[i], icon, col, 2);
        const std::string cost = std::to_string(int(sd.manaCost));
        cv::putText(display, cost, {icon.br().x + 2, slot.y + stripH / 2 + 6}, cv::FONT_HERSHEY_SIMPLEX, 0.45 * k, col, 1,
                    cv::LINE_AA);
        if (star && can && int(m.time() * 4.f) % 2 == 0)
            cv::rectangle(display, slot, col, 1);
    }
}
