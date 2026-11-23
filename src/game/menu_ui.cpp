#include "game/menu_ui.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Desenha texto centralizado com sombra pixelada
void putCenteredText(cv::Mat& img, const std::string& text, const cv::Rect& box,
                     int fontFace, double scale, const cv::Scalar& fg, int thickness = 2) {
    int baseline = 0;
    cv::Size ts = cv::getTextSize(text, fontFace, scale, thickness, &baseline);
    cv::Point org(box.x + (box.width - ts.width) / 2,
                  box.y + (box.height + ts.height) / 2);
    cv::putText(img, text, org + cv::Point(2, 2), fontFace, scale, cv::Scalar(10, 8, 15), thickness + 1, cv::LINE_8);
    cv::putText(img, text, org, fontFace, scale, fg, thickness, cv::LINE_8);
}

// Gera um retrato pixelado de contingencia caso a imagem ainda nao esteja na pasta assets/
cv::Mat makeFallbackPortrait(int index, const std::string& name) {
    cv::Mat p(220, 180, CV_8UC3);
    const std::array<cv::Scalar, 6> palette{{
        cv::Scalar(95, 75, 110),   // Derzu Omaia
        cv::Scalar(140, 60, 35),   // Meneghetti, o Macom
        cv::Scalar(120, 45, 45),   // Venegas
        cv::Scalar(150, 95, 30),   // Neri
        cv::Scalar(45, 110, 60),   // Baltar MST
        cv::Scalar(160, 110, 30)   // SubraMana
    }};
    p.setTo(palette[index % palette.size()]);
    cv::rectangle(p, {8, 8}, {p.cols - 9, p.rows - 9}, {50, 190, 230}, 2);
    putCenteredText(p, name, {12, 70, p.cols - 24, 60}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {245, 240, 220}, 2);
    putCenteredText(p, "PIX-ART", {12, 130, p.cols - 24, 30}, cv::FONT_HERSHEY_PLAIN, 1.1, {180, 240, 200}, 1);
    return p;
}

} // namespace

// Tenta carregar imagem testando extensoes em ordem; retorna Mat vazia se nenhuma funcionar.
static cv::Mat tryLoadImage(const std::string& basePath,
                            const std::vector<std::string>& exts = {".png", ".jpeg", ".jpg"}) {
    for (const auto& e : exts) {
        cv::Mat m = cv::imread(basePath + e, cv::IMREAD_COLOR);
        if (!m.empty()) return m;
    }
    return {};
}

MenuUI::MenuUI() {
    // selectImage: imagem exibida no menu (xxx_select.{png,jpeg})
    // playerImage: spritesheet usado no jogo (xxx_player.png)
    chars_ = {
        {"derzu",      "Derzu Omaia",         "derzu_select",      "derzu_player.png"},
        {"menegueti",  "Meneghetti, o Macom", "menegueti_select",  "menegueti_player.png"},
        {"venegas",    "Venegas",             "venegas_select",    "venegas_player.png"},
        {"neri",       "Neri",                "neri-select",       "neri_player.png"},
        {"baltar",     "Baltar MST",          "baltar_select",     "baltar_player.png"},
        {"subramana",  "SubraMana",           "anand_select",      "anand_player.png"},
    };
}

bool MenuUI::loadCharacters(const std::string& assetsDir) {
    bool allOk = true;
    for (size_t i = 0; i < chars_.size(); ++i) {
        // Tenta carregar a imagem de selecao (xxx_select) com extensoes .png e .jpeg
        const std::string base = assetsDir + "/sprites/players/" + chars_[i].selectImage;
        cv::Mat img = tryLoadImage(base);
        if (img.empty()) {
            allOk = false;
            chars_[i].portrait = makeFallbackPortrait(static_cast<int>(i), chars_[i].name);
        } else {
            chars_[i].portrait = img;
        }
    }
    return allOk;
}

std::string MenuUI::characterImagePath(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(chars_.size())) return "";
    return chars_[idx].selectImage;
}

std::string MenuUI::characterPlayerImage(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(chars_.size())) return "";
    return chars_[idx].playerImage;
}

std::string MenuUI::characterId(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(chars_.size())) return "";
    return chars_[idx].id;
}

void MenuUI::setScreen(GameScreen s) {
    screen_ = s;
    for (int i = 0; i < 2; ++i) {
        playHover_[i] = infoHover_[i] = backHover_[i] = 0.f;
    }
    if (s == GameScreen::CharacterSelect) {
        resetSelection();
    }
}

void MenuUI::resetSelection() {
    chosen_[0] = chosen_[1] = -1;
    for (auto& c : chars_) {
        c.drawProgress[0] = c.drawProgress[1] = 0.f;
        if (!c.drawLayer.empty()) c.drawLayer.setTo(0);
    }
}

std::string MenuUI::characterName(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(chars_.size())) return "Nenhum";
    return chars_[idx].name;
}

void MenuUI::ensureBackground_(const cv::Size& size) {
    if (!bgCache_.empty() && bgCache_.size() == size) return;
    lastSize_ = size;
    
    cv::Mat bg = tryLoadImage("assets/sprites/ui/fundoMenu");
    
    if (!bg.empty()){
        cv::resize(bg, bgCache_, size, 0, 0, cv::INTER_LINEAR);
    } else {
        bgCache_ = cv::Mat(size, CV_8UC3, cv::Scalar(20, 15, 30));
    }

    buildTitleLayout_(size);
    buildCharacterLayout_(size);
}

void MenuUI::buildTitleLayout_(const cv::Size& size) {
    const int w = size.width;
    const int h = size.height;

    // Retangulo estilizado central para "Fractal Arcana"
    int boxW = std::clamp((w * 56) / 100, 320, 760);
    int boxH = std::clamp((h * 22) / 100, 100, 170);
    int boxX = (w - boxW) / 2;
    int boxY = (h - boxH) / 2 - h / 14;
    titleFrameRect_ = cv::Rect(boxX, boxY, boxW, boxH);

    // Dois retangulos estilizados na regiao inferior: "Jogar" (esquerda) e "info" (direita)
    int btnW = std::clamp((w * 20) / 100, 150, 260);
    int btnH = std::clamp((h * 11) / 100, 56, 92);
    int gap  = std::clamp(w / 14, 36, 120);
    int btnY = h - btnH - std::max(36, h / 11);

    int leftX  = w / 2 - gap / 2 - btnW;
    int rightX = w / 2 + gap / 2;

    btnPlayRect_ = cv::Rect(leftX, btnY, btnW, btnH);
    btnInfoRect_ = cv::Rect(rightX, btnY, btnW, btnH);
    btnBackRect_ = cv::Rect(w / 2 - btnW / 2, btnY, btnW, btnH);
}

void MenuUI::buildCharacterLayout_(const cv::Size& size) {
    const int w = size.width;
    const int h = size.height;

    const int cols = 3;
    const int rows = 2;
    const int topMargin = std::max(72, h / 9);
    const int bottomMargin = std::max(64, h / 10);
    const int sideMargin = std::max(40, w / 16);
    const int gapX = 24;
    const int gapY = 20;

    int availW = w - 2 * sideMargin - gapX * (cols - 1);
    int availH = h - topMargin - bottomMargin - gapY * (rows - 1);
    int cardW = std::max(120, availW / cols);
    int cardH = std::max(110, availH / rows);

    int totalW = cols * cardW + (cols - 1) * gapX;
    int startX = (w - totalW) / 2;

    for (size_t i = 0; i < chars_.size(); ++i) {
        int r = static_cast<int>(i) / cols;
        int c = static_cast<int>(i) % cols;
        cv::Rect rect(startX + c * (cardW + gapX),
                      topMargin + r * (cardH + gapY),
                      cardW, cardH);
        chars_[i].cardRect = rect;
        if (chars_[i].drawLayer.empty() || chars_[i].drawLayer.size() != rect.size()) {
            chars_[i].drawLayer = cv::Mat::zeros(rect.size(), CV_8UC3);
        }
    }
}

void MenuUI::drawPixelPanel_(cv::Mat& img, const cv::Rect& r,
                             const cv::Scalar& fill,
                             const cv::Scalar& borderOuter,
                             const cv::Scalar& borderInner,
                             bool highlight) const {
    cv::rectangle(img, r, fill, cv::FILLED);

    // Borda dupla estilo pergaminho / placa medieval
    cv::rectangle(img, r, highlight ? cv::Scalar(80, 250, 255) : borderOuter, 3);
    cv::Rect inner(r.x + 6, r.y + 6, std::max(1, r.width - 12), std::max(1, r.height - 12));
    cv::rectangle(img, inner, borderInner, 1);

    // Cantoneiras decorativas nos 4 cantos
    const int cLen = 12;
    const cv::Scalar cornerCol(70, 215, 245);
    // Top-left
    cv::line(img, {r.x, r.y}, {r.x + cLen, r.y}, cornerCol, 3);
    cv::line(img, {r.x, r.y}, {r.x, r.y + cLen}, cornerCol, 3);
    // Top-right
    cv::line(img, {r.br().x, r.y}, {r.br().x - cLen, r.y}, cornerCol, 3);
    cv::line(img, {r.br().x, r.y}, {r.br().x, r.y + cLen}, cornerCol, 3);
    // Bottom-left
    cv::line(img, {r.x, r.br().y}, {r.x + cLen, r.br().y}, cornerCol, 3);
    cv::line(img, {r.x, r.br().y}, {r.x, r.br().y - cLen}, cornerCol, 3);
    // Bottom-right
    cv::line(img, {r.br().x, r.br().y}, {r.br().x - cLen, r.br().y}, cornerCol, 3);
    cv::line(img, {r.br().x, r.br().y}, {r.br().x, r.br().y - cLen}, cornerCol, 3);
}

void MenuUI::processWandOnTitle_(int playerIdx, const Tracked& trk, double dt) {
    if (!trk.valid) {
        playHover_[playerIdx] = std::max(0.f, playHover_[playerIdx] - float(dt) * 1.5f);
        infoHover_[playerIdx] = std::max(0.f, infoHover_[playerIdx] - float(dt) * 1.5f);
        return;
    }

    cv::Point pt(cvRound(trk.pos.x), cvRound(trk.pos.y));
    float speedBonus = trk.detected ? 1.f : 0.4f;

    if (btnPlayRect_.contains(pt)) {
        playHover_[playerIdx] = std::min(1.f, playHover_[playerIdx] + float(dt) * 1.6f * speedBonus);
        if (playHover_[playerIdx] >= 1.f) {
            setScreen(GameScreen::CharacterSelect);
            return;
        }
    } else {
        playHover_[playerIdx] = std::max(0.f, playHover_[playerIdx] - float(dt) * 1.2f);
    }

    if (btnInfoRect_.contains(pt)) {
        infoHover_[playerIdx] = std::min(1.f, infoHover_[playerIdx] + float(dt) * 1.6f * speedBonus);
        if (infoHover_[playerIdx] >= 1.f) {
            setScreen(GameScreen::Info);
            return;
        }
    } else {
        infoHover_[playerIdx] = std::max(0.f, infoHover_[playerIdx] - float(dt) * 1.2f);
    }
}

void MenuUI::processWandOnSelect_(int playerIdx, const Tracked& trk, double dt) {
    if (!trk.valid) {
        hasLastWand_[playerIdx] = false;
        return;
    }

    cv::Point2f cur = trk.pos;
    cv::Point pt(cvRound(cur.x), cvRound(cur.y));

    if (chosen_[playerIdx] < 0) {
        for (size_t i = 0; i < chars_.size(); ++i) {
            auto& c = chars_[i];
            if (!c.cardRect.contains(pt)) continue;

            // Desenha o traco da varinha sobre o personagem quando ha movimento
            if (hasLastWand_[playerIdx] && c.cardRect.contains(cv::Point(cvRound(lastWandPos_[playerIdx].x), cvRound(lastWandPos_[playerIdx].y)))) {
                float dist = cv::norm(cur - lastWandPos_[playerIdx]);
                if (dist > 1.5f) {
                    cv::Point p1Local(cvRound(lastWandPos_[playerIdx].x) - c.cardRect.x,
                                      cvRound(lastWandPos_[playerIdx].y) - c.cardRect.y);
                    cv::Point p2Local(pt.x - c.cardRect.x, pt.y - c.cardRect.y);
                    cv::Scalar inkCol = (playerIdx == 0) ? cv::Scalar(255, 160, 30) : cv::Scalar(80, 255, 100);
                    cv::line(c.drawLayer, p1Local, p2Local, inkCol, 4, cv::LINE_AA);

                    // Acumula desenho sobre o personagem ate confirmar a escolha
                    c.drawProgress[playerIdx] = std::min(1.f, c.drawProgress[playerIdx] + dist / 140.f);
                }
            } else {
                // Pequeno avanco ao manter a varinha sobre o card
                c.drawProgress[playerIdx] = std::min(1.f, c.drawProgress[playerIdx] + float(dt) * 0.25f);
            }

            if (c.drawProgress[playerIdx] >= 1.f) {
                chosen_[playerIdx] = static_cast<int>(i);
            }
        }
    }

    lastWandPos_[playerIdx] = cur;
    hasLastWand_[playerIdx] = true;

    if (chosen_[0] >= 0 && chosen_[1] >= 0) {
        screen_ = GameScreen::DuelReady;
    }
}

void MenuUI::update(const cv::Size& frameSize, const Tracked& p1, const Tracked& p2, double dt) {
    ensureBackground_(frameSize);
    dt = std::clamp(dt, 0.0, 0.2);

    if (screen_ == GameScreen::Title) {
        processWandOnTitle_(0, p1, dt);
        if (screen_ == GameScreen::Title) processWandOnTitle_(1, p2, dt);
    } else if (screen_ == GameScreen::CharacterSelect) {
        processWandOnSelect_(0, p1, dt);
        processWandOnSelect_(1, p2, dt);
    } else if (screen_ == GameScreen::Info || screen_ == GameScreen::DuelReady) {
        for (int idx = 0; idx < 2; ++idx) {
            const Tracked& trk = (idx == 0) ? p1 : p2;
            if (!trk.valid) {
                backHover_[idx] = std::max(0.f, backHover_[idx] - float(dt) * 1.5f);
                continue;
            }
            cv::Point pt(cvRound(trk.pos.x), cvRound(trk.pos.y));
            if (btnBackRect_.contains(pt)) {
                backHover_[idx] = std::min(1.f, backHover_[idx] + float(dt) * 1.6f);
                if (backHover_[idx] >= 1.f) {
                    setScreen(GameScreen::Title);
                    return;
                }
            } else {
                backHover_[idx] = std::max(0.f, backHover_[idx] - float(dt) * 1.2f);
            }
        }
    }
}

void MenuUI::onMouseClick(int x, int y, const cv::Size& frameSize) {
    ensureBackground_(frameSize);
    cv::Point pt(x, y);

    if (screen_ == GameScreen::Title) {
        if (btnPlayRect_.contains(pt)) setScreen(GameScreen::CharacterSelect);
        else if (btnInfoRect_.contains(pt)) setScreen(GameScreen::Info);
    } else if (screen_ == GameScreen::Info) {
        if (btnBackRect_.contains(pt)) setScreen(GameScreen::Title);
    } else if (screen_ == GameScreen::CharacterSelect) {
        for (size_t i = 0; i < chars_.size(); ++i) {
            if (chars_[i].cardRect.contains(pt)) {
                if (chosen_[0] < 0) chosen_[0] = static_cast<int>(i);
                else if (chosen_[1] < 0) chosen_[1] = static_cast<int>(i);
                if (chosen_[0] >= 0 && chosen_[1] >= 0) screen_ = GameScreen::DuelReady;
                break;
            }
        }
    } else if (screen_ == GameScreen::DuelReady) {
        if (btnBackRect_.contains(pt)) setScreen(GameScreen::Title);
    }
}

void MenuUI::draw(cv::Mat& frame) const {
    if (frame.empty()) return;
    if (bgCache_.size() != frame.size()) {
        const_cast<MenuUI*>(this)->ensureBackground_(frame.size());
    }

    bgCache_.copyTo(frame);

    if (screen_ == GameScreen::Title) {
        drawTitleScreen_(frame);
    } else if (screen_ == GameScreen::Info) {
        drawInfoScreen_(frame);
    } else {
        drawCharacterSelect_(frame);
    }
}

void MenuUI::drawTitleScreen_(cv::Mat& frame) const {
    // Retangulo estilizado central com "Fractal Arcana"
    drawPixelPanel_(frame, titleFrameRect_,
                    cv::Scalar(24, 18, 36),
                    cv::Scalar(65, 195, 235),
                    cv::Scalar(120, 85, 45),
                    true);

    double titleScale = std::clamp(titleFrameRect_.width / 340.0, 1.1, 2.2);
    putCenteredText(frame, "Fractal Arcana", titleFrameRect_,
                    cv::FONT_HERSHEY_TRIPLEX, titleScale, cv::Scalar(140, 245, 255), 2);

    // Botao esquerdo: "Jogar"
    float pProg = std::max(playHover_[0], playHover_[1]);
    drawPixelPanel_(frame, btnPlayRect_,
                    cv::Scalar(25, 48, 32),
                    cv::Scalar(80, 220, 120),
                    cv::Scalar(50, 130, 75),
                    pProg > 0.05f);
    if (pProg > 0.01f) {
        int barW = cvRound(btnPlayRect_.width * pProg);
        cv::rectangle(frame, {btnPlayRect_.x, btnPlayRect_.br().y - 6},
                      {btnPlayRect_.x + barW, btnPlayRect_.br().y}, cv::Scalar(80, 255, 140), cv::FILLED);
    }
    putCenteredText(frame, "Jogar", btnPlayRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, cv::Scalar(220, 255, 225), 2);

    // Botao direito: "info"
    float iProg = std::max(infoHover_[0], infoHover_[1]);
    drawPixelPanel_(frame, btnInfoRect_,
                    cv::Scalar(28, 26, 48),
                    cv::Scalar(220, 175, 70),
                    cv::Scalar(125, 95, 40),
                    iProg > 0.05f);
    if (iProg > 0.01f) {
        int barW = cvRound(btnInfoRect_.width * iProg);
        cv::rectangle(frame, {btnInfoRect_.x, btnInfoRect_.br().y - 6},
                      {btnInfoRect_.x + barW, btnInfoRect_.br().y}, cv::Scalar(120, 230, 255), cv::FILLED);
    }
    putCenteredText(frame, "info", btnInfoRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, cv::Scalar(255, 240, 195), 2);
}

void MenuUI::drawInfoScreen_(cv::Mat& frame) const {
    cv::Rect panel(frame.cols / 8, frame.rows / 7, (frame.cols * 3) / 4, (frame.rows * 5) / 8);
    drawPixelPanel_(frame, panel, cv::Scalar(22, 18, 32), cv::Scalar(70, 200, 235), cv::Scalar(110, 80, 45), false);

    putCenteredText(frame, "FRACTAL ARCANA - INFO", {panel.x, panel.y + 12, panel.width, 45},
                    cv::FONT_HERSHEY_DUPLEX, 0.85, cv::Scalar(140, 245, 255), 2);

    std::vector<std::string> lines{
        "1. Aponte ou desenhe com a varinha sobre 'Jogar' (ou clique).",
        "2. Dois jogadores (P1 e P2) escolhem seus magos desenhando sobre eles.",
        "3. Personagens: Derzu Omaia, Meneghetti o Macom, Venegas,",
        "   Neri, Baltar MST e SubraMana.",
        "4. Use gestos e tracos para conjurar ataques e escudos."
    };

    int y = panel.y + 75;
    for (const auto& l : lines) {
        cv::putText(frame, l, {panel.x + 28, y}, cv::FONT_HERSHEY_SIMPLEX, 0.58,
                    cv::Scalar(235, 235, 245), 1, cv::LINE_AA);
        y += 34;
    }

    float bProg = std::max(backHover_[0], backHover_[1]);
    drawPixelPanel_(frame, btnBackRect_, cv::Scalar(35, 25, 40), cv::Scalar(90, 190, 230), cv::Scalar(80, 70, 110), bProg > 0.05f);
    putCenteredText(frame, "Voltar", btnBackRect_, cv::FONT_HERSHEY_DUPLEX, 0.85, cv::Scalar(240, 240, 250), 2);
}

void MenuUI::drawCharacterSelect_(cv::Mat& frame) const {
    std::string header = "ESCOLHA SEU MAGO - P1: " + characterName(chosen_[0]) +
                         " | P2: " + characterName(chosen_[1]);
    putCenteredText(frame, header, {0, 12, frame.cols, 44},
                    cv::FONT_HERSHEY_DUPLEX, 0.72, cv::Scalar(140, 245, 255), 2);

    for (size_t i = 0; i < chars_.size(); ++i) {
        const auto& c = chars_[i];
        bool p1Sel = (chosen_[0] == static_cast<int>(i));
        bool p2Sel = (chosen_[1] == static_cast<int>(i));
        bool anyProg = (c.drawProgress[0] > 0.02f || c.drawProgress[1] > 0.02f || p1Sel || p2Sel);

        cv::Scalar borderCol = cv::Scalar(90, 175, 210);
        if (p1Sel && p2Sel) borderCol = cv::Scalar(255, 255, 100);
        else if (p1Sel)     borderCol = cv::Scalar(255, 140, 20);
        else if (p2Sel)     borderCol = cv::Scalar(60, 255, 80);

        drawPixelPanel_(frame, c.cardRect, cv::Scalar(20, 18, 30), borderCol, cv::Scalar(110, 85, 45), anyProg);

        // Retrato do personagem dentro do card
        cv::Rect imgBox(c.cardRect.x + 10, c.cardRect.y + 10,
                        std::max(1, c.cardRect.width - 20),
                        std::max(1, c.cardRect.height - 44));
        if (!c.portrait.empty() && imgBox.width > 10 && imgBox.height > 10) {
            cv::Mat resized;
            cv::resize(c.portrait, resized, imgBox.size(), 0, 0, cv::INTER_NEAREST);
            resized.copyTo(frame(imgBox));
        }

        // Camada de desenho da varinha sobre o personagem
        if (!c.drawLayer.empty() && c.drawLayer.size() == c.cardRect.size()) {
            cv::Mat gray, mask;
            cv::cvtColor(c.drawLayer, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, mask, 1, 255, cv::THRESH_BINARY);
            c.drawLayer.copyTo(frame(c.cardRect), mask);
        }

        // Barra de progresso do desenho (P1 em cima, P2 embaixo da base do card)
        if (c.drawProgress[0] > 0.f) {
            int w1 = cvRound((c.cardRect.width - 16) * c.drawProgress[0]);
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.br().y - 34},
                          {c.cardRect.x + 8 + w1, c.cardRect.br().y - 29}, cv::Scalar(255, 140, 20), cv::FILLED);
        }
        if (c.drawProgress[1] > 0.f) {
            int w2 = cvRound((c.cardRect.width - 16) * c.drawProgress[1]);
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.br().y - 28},
                          {c.cardRect.x + 8 + w2, c.cardRect.br().y - 23}, cv::Scalar(60, 255, 80), cv::FILLED);
        }

        // Nome do personagem
        cv::Rect nameBox(c.cardRect.x + 6, c.cardRect.br().y - 24, c.cardRect.width - 12, 22);
        putCenteredText(frame, c.name, nameBox, cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(255, 248, 220), 1);

        // Badges P1 / P2 se escolhido
        if (p1Sel) {
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.y + 8}, {c.cardRect.x + 42, c.cardRect.y + 30},
                          cv::Scalar(255, 130, 0), cv::FILLED);
            cv::putText(frame, "P1", {c.cardRect.x + 13, c.cardRect.y + 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                        cv::Scalar(255, 255, 255), 2);
        }
        if (p2Sel) {
            cv::rectangle(frame, {c.cardRect.br().x - 42, c.cardRect.y + 8}, {c.cardRect.br().x - 8, c.cardRect.y + 30},
                          cv::Scalar(20, 200, 40), cv::FILLED);
            cv::putText(frame, "P2", {c.cardRect.br().x - 37, c.cardRect.y + 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                        cv::Scalar(255, 255, 255), 2);
        }
    }

    if (screen_ == GameScreen::DuelReady) {
        cv::Rect banner(frame.cols / 5, frame.rows / 2 - 42, (frame.cols * 3) / 5, 84);
        drawPixelPanel_(frame, banner, cv::Scalar(25, 20, 40), cv::Scalar(80, 255, 160), cv::Scalar(220, 190, 60), true);
        std::string msg = "PRONTOS! " + characterName(chosen_[0]) + " vs " + characterName(chosen_[1]);
        putCenteredText(frame, msg, banner, cv::FONT_HERSHEY_DUPLEX, 0.75, cv::Scalar(160, 255, 190), 2);

        float bProg = std::max(backHover_[0], backHover_[1]);
        drawPixelPanel_(frame, btnBackRect_, cv::Scalar(35, 25, 40), cv::Scalar(90, 190, 230), cv::Scalar(80, 70, 110), bProg > 0.05f);
        putCenteredText(frame, "Menu", btnBackRect_, cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(240, 240, 250), 2);
    }
}