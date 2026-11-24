#include "game/menu_ui.h"
#include "render/sprites.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{

    // Desenha texto centralizado com sombra pixelada
    void putCenteredText(cv::Mat &img, const std::string &text, const cv::Rect &box,
                         int fontFace, double scale, const cv::Scalar &fg, int thickness = 2)
    {
        int baseline = 0;
        cv::Size ts = cv::getTextSize(text, fontFace, scale, thickness, &baseline);
        cv::Point org(box.x + (box.width - ts.width) / 2,
                      box.y + (box.height + ts.height) / 2);
        cv::putText(img, text, org + cv::Point(2, 2), fontFace, scale, cv::Scalar(10, 8, 15), thickness + 1, cv::LINE_8);
        cv::putText(img, text, org, fontFace, scale, fg, thickness, cv::LINE_8);
    }

    // Redimensiona BGRA com alfa pre-multiplicado (evita bordas escuras/claras nas transparencias)
    cv::Mat resizeBGRA(const cv::Mat &src, double scale)
    {
        cv::Mat f;
        src.convertTo(f, CV_32F, 1.0 / 255.0);
        std::vector<cv::Mat> ch;
        cv::split(f, ch);
        for (int i = 0; i < 3; ++i)
            ch[i] = ch[i].mul(ch[3]);
        cv::merge(ch, f);
        cv::resize(f, f, cv::Size(), scale, scale, cv::INTER_AREA);
        cv::split(f, ch);
        const cv::Mat a = cv::max(ch[3], 1e-4);
        for (int i = 0; i < 3; ++i)
            cv::divide(ch[i], a, ch[i]);
        cv::merge(ch, f);
        cv::Mat out;
        f.convertTo(out, CV_8U, 255.0);

        // Realce leve de nitidez nos canais de cor (compensa a suavizacao da reducao)
        if (scale < 1.0)
        {
            cv::split(out, ch);
            for (int i = 0; i < 3; ++i)
            {
                cv::Mat blur;
                cv::GaussianBlur(ch[i], blur, cv::Size(0, 0), 0.9);
                cv::addWeighted(ch[i], 1.6, blur, -0.6, 0, ch[i]);
            }
            cv::merge(ch, out);
        }
        return out;
    }

    // Gera um retrato pixelado de contingencia caso a imagem ainda nao esteja na pasta assets/
    cv::Mat makeFallbackPortrait(int index, const std::string &name)
    {
        cv::Mat p(220, 180, CV_8UC3);
        const std::array<cv::Scalar, 6> palette{{
            cv::Scalar(95, 75, 110), // Derzu Omaia
            cv::Scalar(140, 60, 35), // Meneghetti, o Macom
            cv::Scalar(120, 45, 45), // Venegas
            cv::Scalar(150, 95, 30), // Neri
            cv::Scalar(45, 110, 60), // Baltar MST
            cv::Scalar(160, 110, 30) // SubraMana
        }};
        p.setTo(palette[index % palette.size()]);
        cv::rectangle(p, {8, 8}, {p.cols - 9, p.rows - 9}, {50, 190, 230}, 2);
        putCenteredText(p, name, {12, 70, p.cols - 24, 60}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {245, 240, 220}, 2);
        putCenteredText(p, "PIX-ART", {12, 130, p.cols - 24, 30}, cv::FONT_HERSHEY_PLAIN, 1.1, {180, 240, 200}, 1);
        return p;
    }

} // namespace

// Tenta carregar imagem testando extensoes em ordem; retorna Mat vazia se nenhuma funcionar.
static cv::Mat tryLoadImage(const std::string &basePath,
                            const std::vector<std::string> &exts = {".png", ".jpeg", ".jpg"})
{
    for (const auto &e : exts)
    {
        cv::Mat m = cv::imread(basePath + e, cv::IMREAD_COLOR);
        if (!m.empty())
            return m;
    }
    return {};
}

MenuUI::MenuUI()
{
    // selectImage: imagem exibida no menu (xxx_select.{png,jpeg})
    // playerImage: spritesheet usado no jogo (xxx_player.png)
    chars_ = {
        {"derzu", "Derzu Omaia", "derzu_select", "derzu_player.png"},
        {"menegueti", "Meneghetti, o Macom", "menegueti_select", "menegueti_player.png"},
        {"venegas", "Venegas", "venegas_select", "venegas_player.png"},
        {"neri", "Neri", "neri-select", "neri_player.png"},
        {"baltar", "Baltar MST", "baltar_select", "baltar_player.png"},
        {"subramana", "SubraMana", "anand_select", "anand_player.png"},
    };
}

bool MenuUI::loadCharacters(const std::string &assetsDir)
{
    bool allOk = true;
    for (size_t i = 0; i < chars_.size(); ++i)
    {
        // Tenta carregar a imagem de selecao (xxx_select) com extensoes .png e .jpeg
        const std::string base = assetsDir + "/sprites/players/" + chars_[i].selectImage;
        cv::Mat img = tryLoadImage(base);
        if (img.empty())
        {
            allOk = false;
            chars_[i].portrait = makeFallbackPortrait(static_cast<int>(i), chars_[i].name);
        }
        else
        {
            chars_[i].portrait = img;
        }
    }
    return allOk;
}

std::string MenuUI::characterImagePath(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(chars_.size()))
        return "";
    return chars_[idx].selectImage;
}

std::string MenuUI::characterPlayerImage(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(chars_.size()))
        return "";
    return chars_[idx].playerImage;
}

std::string MenuUI::characterId(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(chars_.size()))
        return "";
    return chars_[idx].id;
}

void MenuUI::setScreen(GameScreen s)
{
    screen_ = s;
    for (int i = 0; i < 2; ++i)
    {
        playHover_[i] = infoHover_[i] = backHover_[i] = glowHover_[i] = 0.f;
        glowLatch_[i] = false;
    }
    btnHover_[0] = btnHover_[1] = btnHover_[2] = 0.f;
    if (s == GameScreen::CharacterSelect)
    {
        resetSelection();
    }
}

void MenuUI::resetSelection()
{
    chosen_[0] = chosen_[1] = -1;
    for (auto &c : chars_)
    {
        c.drawProgress[0] = c.drawProgress[1] = 0.f;
        if (!c.drawLayer.empty())
            c.drawLayer.setTo(0);
    }
}

std::string MenuUI::characterName(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(chars_.size()))
        return "Nenhum";
    return chars_[idx].name;
}

void MenuUI::ensureBackground_(const cv::Size &size)
{
    if (!bgCache_.empty() && bgCache_.size() == size)
        return;
    lastSize_ = size;

    cv::Mat bg = tryLoadImage("assets/sprites/ui/fundoMenu");

    if (!bg.empty())
    {
        cv::resize(bg, bgCache_, size, 0, 0, cv::INTER_LINEAR);
    }
    else
    {
        bgCache_ = cv::Mat(size, CV_8UC3, cv::Scalar(20, 15, 30));
    }

    // Fundo exclusivo da tela inicial: preenche a tela mantendo a proporcao (corta o excesso)
    titleBgCache_.release();
    cv::Mat tbg = cv::imread("assets/menu_image.jpg", cv::IMREAD_COLOR);
    if (!tbg.empty())
    {
        const double s = std::max(double(size.width) / tbg.cols, double(size.height) / tbg.rows);
        cv::Mat scaled;
        cv::resize(tbg, scaled, {}, s, s, s < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);
        const int x = std::max(0, (scaled.cols - size.width) / 2);
        const int y = std::max(0, (scaled.rows - size.height) / 2);
        cv::Rect crop(x, y, std::min(size.width, scaled.cols), std::min(size.height, scaled.rows));
        scaled(crop).copyTo(titleBgCache_);
        if (titleBgCache_.size() != size)
            cv::resize(titleBgCache_, titleBgCache_, size);
    }

    if (!btnLoaded_)
        loadButtonImages_();
    buildTitleLayout_(size);
    buildCharacterLayout_(size);
}

void MenuUI::loadButtonImages_()
{
    btnLoaded_ = true;
    static const char *files[kBtnCount] = {
        "assets/button_jogar.png",
        "assets/button_info.png",
        "assets/button_effects_off.png",
        "assets/button_effects_on.png"};

    btnImagesOk_ = true;
    for (int i = 0; i < kBtnCount; ++i)
    {
        cv::Mat m = cv::imread(files[i], cv::IMREAD_UNCHANGED);
        if (m.empty())
        {
            std::fprintf(stderr, "Aviso: nao carregou %s (usando botoes simples)\n", files[i]);
            btnImagesOk_ = false;
            continue;
        }
        if (m.depth() == CV_16U)
            m.convertTo(m, CV_8U, 1.0 / 257.0);
        if (m.channels() == 1)
            cv::cvtColor(m, m, cv::COLOR_GRAY2BGRA);
        else if (m.channels() == 3)
            cv::cvtColor(m, m, cv::COLOR_BGR2BGRA);
        btnSrc_[i] = m;
    }
    if (!btnImagesOk_)
        return;

    // Descobre onde esta a moldura (pixels opacos) na imagem do botao "Jogar";
    // as outras imagens tem a mesma tela, entao usamos a mesma area.
    const cv::Mat &ref = btnSrc_[kBtnPlay];
    int minX = ref.cols, minY = ref.rows, maxX = -1, maxY = -1;
    for (int y = 0; y < ref.rows; ++y)
    {
        const cv::Vec4b *row = ref.ptr<cv::Vec4b>(y);
        for (int x = 0; x < ref.cols; ++x)
            if (row[x][3] > 60)
            {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
    }
    if (maxX > minX && maxY > minY)
        btnInner_ = cv::Rect2f(float(minX) / ref.cols,
                               float(minY) / ref.rows,
                               float(maxX - minX + 1) / ref.cols,
                               float(maxY - minY + 1) / ref.rows);
}

void MenuUI::buildTitleLayout_(const cv::Size &size)
{
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
    int gap = std::clamp(w / 14, 36, 120);
    int btnY = h - btnH - std::max(36, h / 11);

    int leftX = w / 2 - gap / 2 - btnW;
    int rightX = w / 2 + gap / 2;

    btnPlayRect_ = cv::Rect(leftX, btnY, btnW, btnH);
    btnInfoRect_ = cv::Rect(rightX, btnY, btnW, btnH);
    btnBackRect_ = cv::Rect(w / 2 - btnW / 2, btnY, btnW, btnH);

    // Botao de brilhos: centralizado, acima de "Jogar"/"info"
    int glowW = btnW * 2 + gap;
    int glowH = std::max(40, btnH * 3 / 4);
    btnGlowRect_ = cv::Rect(w / 2 - glowW / 2, btnY - glowH - std::max(14, h / 40), glowW, glowH);

    // Botoes com imagem: o retangulo de clique/hover passa a ser a moldura visivel,
    // mantendo a proporcao da arte. Os tres botoes tem o mesmo tamanho.
    if (!btnImagesOk_)
        return;

    const cv::Mat &ref = btnSrc_[kBtnPlay];
    const double innerAspect = (btnInner_.width * ref.cols) / (btnInner_.height * ref.rows);

    // Tres botoes na mesma linha: Jogar | Info | Efeitos
    const int rowGap = std::clamp(w / 40, 12, 40);
    int visW = (w * 94 / 100 - 2 * rowGap) / 3;
    int visH = cvRound(visW / innerAspect);
    const int maxH = std::max(40, (h * 22) / 100);
    if (visH > maxH)
    {
        visH = maxH;
        visW = cvRound(visH * innerAspect);
    }
    const int rowW = 3 * visW + 2 * rowGap;
    const int rowX = (w - rowW) / 2;
    const int imgBtnY = h - visH - std::max(36, h / 11);
    btnPlayRect_ = cv::Rect(rowX, imgBtnY, visW, visH);
    btnInfoRect_ = cv::Rect(rowX + visW + rowGap, imgBtnY, visW, visH);
    btnGlowRect_ = cv::Rect(rowX + 2 * (visW + rowGap), imgBtnY, visW, visH);

    for (int i = 0; i < kBtnCount; ++i)
    {
        const double scale = double(visW) / (btnInner_.width * btnSrc_[i].cols);
        btnNorm_[i] = resizeBGRA(btnSrc_[i], scale);

        // versao "hover": clareia so os canais de cor (o alfa fica igual)
        std::vector<cv::Mat> ch;
        cv::split(btnNorm_[i], ch);
        for (int k = 0; k < 3; ++k)
            cv::add(ch[k], cv::Scalar(45), ch[k]);
        cv::merge(ch, btnHot_[i]);

        // halo branco: silhueta (alfa) borrada numa tela maior que a imagem
        btnGlowPad_ = std::max(14, cvRound(visW * 0.09));
        const int pad = btnGlowPad_;
        cv::Mat gm = cv::Mat::zeros(btnNorm_[i].rows + 2 * pad, btnNorm_[i].cols + 2 * pad, CV_8U);
        ch[3].copyTo(gm(cv::Rect(pad, pad, btnNorm_[i].cols, btnNorm_[i].rows)));
        cv::GaussianBlur(gm, gm, cv::Size(0, 0), pad * 0.45);
        gm.convertTo(btnGlow_[i], CV_8U, 1.8); // ganho: o halo fica mais forte perto da borda

        btnOffset_[i] = cv::Point(cvRound(btnInner_.x * btnNorm_[i].cols),
                                  cvRound(btnInner_.y * btnNorm_[i].rows));
    }
}

bool MenuUI::drawImageButton_(cv::Mat &frame, int idx, const cv::Rect &hit,
                              float prog, const cv::Scalar &barCol) const
{
    if (!btnImagesOk_ || btnNorm_[idx].empty())
        return false;

    const int slot = idx >= kBtnEffectsOn ? 2 : idx;
    const float hv = btnHover_[slot];
    const cv::Mat &img = (prog > 0.05f || hv > 0.5f) ? btnHot_[idx] : btnNorm_[idx];
    const cv::Point tl = hit.tl() - btnOffset_[idx];

    // Halo branco atras do botao: fraco em repouso, forte (e pulsando de leve) no hover.
    // Desligado junto com os demais brilhos quando "Efeitos" esta OFF.
    if (glowEnabled_ && !btnGlow_[idx].empty())
    {
        const float k = (0.30f + 0.70f * hv) * (1.f + 0.08f * hv * std::sin(titleTime_ * 7.f));
        const cv::Point gtl = tl - cv::Point(btnGlowPad_, btnGlowPad_);
        const cv::Rect vis = cv::Rect(gtl, btnGlow_[idx].size()) & cv::Rect(0, 0, frame.cols, frame.rows);
        if (!vis.empty())
        {
            cv::Mat m, m3;
            btnGlow_[idx](cv::Rect(vis.x - gtl.x, vis.y - gtl.y, vis.width, vis.height)).convertTo(m, CV_8U, k);
            cv::cvtColor(m, m3, cv::COLOR_GRAY2BGR);
            cv::Mat roi = frame(vis);
            cv::add(roi, m3, roi);
        }
    }

    drawSprite(frame, img, tl + cv::Point(img.cols / 2, img.rows / 2));

    // barra de progresso de ativacao (varinha parada sobre o botao), logo abaixo da moldura
    if (prog > 0.01f)
    {
        const int barW = cvRound(hit.width * std::min(1.f, prog));
        cv::rectangle(frame, cv::Rect(hit.x, hit.br().y + 4, barW, 5), barCol, cv::FILLED);
    }
    return true;
}

void MenuUI::buildCharacterLayout_(const cv::Size &size)
{
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

    for (size_t i = 0; i < chars_.size(); ++i)
    {
        int r = static_cast<int>(i) / cols;
        int c = static_cast<int>(i) % cols;
        cv::Rect rect(startX + c * (cardW + gapX),
                      topMargin + r * (cardH + gapY),
                      cardW, cardH);
        chars_[i].cardRect = rect;
        if (chars_[i].drawLayer.empty() || chars_[i].drawLayer.size() != rect.size())
        {
            chars_[i].drawLayer = cv::Mat::zeros(rect.size(), CV_8UC3);
        }
    }
}

void MenuUI::drawPixelPanel_(cv::Mat &img, const cv::Rect &r,
                             const cv::Scalar &fill,
                             const cv::Scalar &borderOuter,
                             const cv::Scalar &borderInner,
                             bool highlight) const
{
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

void MenuUI::processWandOnTitle_(int playerIdx, const Tracked &trk, double dt)
{
    if (!trk.valid)
    {
        playHover_[playerIdx] = std::max(0.f, playHover_[playerIdx] - float(dt) * 1.5f);
        infoHover_[playerIdx] = std::max(0.f, infoHover_[playerIdx] - float(dt) * 1.5f);
        glowHover_[playerIdx] = std::max(0.f, glowHover_[playerIdx] - float(dt) * 1.5f);
        glowLatch_[playerIdx] = false;
        return;
    }

    cv::Point pt(cvRound(trk.pos.x), cvRound(trk.pos.y));
    float speedBonus = trk.detected ? 1.f : 0.4f;

    if (btnPlayRect_.contains(pt))
    {
        playHover_[playerIdx] = std::min(1.f, playHover_[playerIdx] + float(dt) * 1.6f * speedBonus);
        if (playHover_[playerIdx] >= 1.f)
        {
            setScreen(GameScreen::CharacterSelect);
            return;
        }
    }
    else
    {
        playHover_[playerIdx] = std::max(0.f, playHover_[playerIdx] - float(dt) * 1.2f);
    }

    if (btnInfoRect_.contains(pt))
    {
        infoHover_[playerIdx] = std::min(1.f, infoHover_[playerIdx] + float(dt) * 1.6f * speedBonus);
        if (infoHover_[playerIdx] >= 1.f)
        {
            setScreen(GameScreen::Info);
            return;
        }
    }
    else
    {
        infoHover_[playerIdx] = std::max(0.f, infoHover_[playerIdx] - float(dt) * 1.2f);
    }

    // Alterna os brilhos; so rearma depois que a varinha sair do botao
    if (btnGlowRect_.contains(pt))
    {
        if (!glowLatch_[playerIdx])
        {
            glowHover_[playerIdx] = std::min(1.f, glowHover_[playerIdx] + float(dt) * 1.6f * speedBonus);
            if (glowHover_[playerIdx] >= 1.f)
            {
                glowEnabled_ = !glowEnabled_;
                glowLatch_[playerIdx] = true;
                glowHover_[playerIdx] = 0.f;
            }
        }
    }
    else
    {
        glowLatch_[playerIdx] = false;
        glowHover_[playerIdx] = std::max(0.f, glowHover_[playerIdx] - float(dt) * 1.2f);
    }
}

void MenuUI::processWandOnSelect_(int playerIdx, const Tracked &trk, double dt)
{
    if (!trk.valid)
    {
        hasLastWand_[playerIdx] = false;
        return;
    }

    cv::Point2f cur = trk.pos;
    cv::Point pt(cvRound(cur.x), cvRound(cur.y));

    if (chosen_[playerIdx] < 0)
    {
        for (size_t i = 0; i < chars_.size(); ++i)
        {
            auto &c = chars_[i];
            if (!c.cardRect.contains(pt))
                continue;

            // Desenha o traco da varinha sobre o personagem quando ha movimento
            if (hasLastWand_[playerIdx] && c.cardRect.contains(cv::Point(cvRound(lastWandPos_[playerIdx].x), cvRound(lastWandPos_[playerIdx].y))))
            {
                float dist = cv::norm(cur - lastWandPos_[playerIdx]);
                if (dist > 1.5f)
                {
                    cv::Point p1Local(cvRound(lastWandPos_[playerIdx].x) - c.cardRect.x,
                                      cvRound(lastWandPos_[playerIdx].y) - c.cardRect.y);
                    cv::Point p2Local(pt.x - c.cardRect.x, pt.y - c.cardRect.y);
                    cv::Scalar inkCol = (playerIdx == 0) ? cv::Scalar(255, 160, 30) : cv::Scalar(80, 255, 100);
                    cv::line(c.drawLayer, p1Local, p2Local, inkCol, 4, cv::LINE_AA);

                    // Acumula desenho sobre o personagem ate confirmar a escolha
                    c.drawProgress[playerIdx] = std::min(1.f, c.drawProgress[playerIdx] + dist / 140.f);
                }
            }
            else
            {
                // Pequeno avanco ao manter a varinha sobre o card
                c.drawProgress[playerIdx] = std::min(1.f, c.drawProgress[playerIdx] + float(dt) * 0.25f);
            }

            if (c.drawProgress[playerIdx] >= 1.f)
            {
                chosen_[playerIdx] = static_cast<int>(i);
            }
        }
    }

    lastWandPos_[playerIdx] = cur;
    hasLastWand_[playerIdx] = true;

    if (chosen_[0] >= 0 && chosen_[1] >= 0)
    {
        screen_ = GameScreen::DuelReady;
    }
}

void MenuUI::update(const cv::Size &frameSize, const Tracked &p1, const Tracked &p2, double dt)
{
    ensureBackground_(frameSize);
    dt = std::clamp(dt, 0.0, 0.2);

    if (screen_ == GameScreen::Title)
    {
        updateManaBlocks_(frameSize, dt);

        const cv::Rect *btnRects[3] = {&btnPlayRect_, &btnInfoRect_, &btnGlowRect_};
        for (int i = 0; i < 3; ++i)
        {
            bool over = btnRects[i]->contains(mousePos_);
            for (const Tracked *t : {&p1, &p2})
                if (t->valid && btnRects[i]->contains(cv::Point(cvRound(t->pos.x), cvRound(t->pos.y))))
                    over = true;
            const float target = over ? 1.f : 0.f;
            btnHover_[i] += (target - btnHover_[i]) * float(std::min(1.0, dt * 10.0));
        }

        processWandOnTitle_(0, p1, dt);
        if (screen_ == GameScreen::Title)
            processWandOnTitle_(1, p2, dt);
    }
    else if (screen_ == GameScreen::CharacterSelect)
    {
        processWandOnSelect_(0, p1, dt);
        processWandOnSelect_(1, p2, dt);
    }
    else if (screen_ == GameScreen::Info || screen_ == GameScreen::DuelReady)
    {
        for (int idx = 0; idx < 2; ++idx)
        {
            const Tracked &trk = (idx == 0) ? p1 : p2;
            if (!trk.valid)
            {
                backHover_[idx] = std::max(0.f, backHover_[idx] - float(dt) * 1.5f);
                continue;
            }
            cv::Point pt(cvRound(trk.pos.x), cvRound(trk.pos.y));
            if (btnBackRect_.contains(pt))
            {
                backHover_[idx] = std::min(1.f, backHover_[idx] + float(dt) * 1.6f);
                if (backHover_[idx] >= 1.f)
                {
                    setScreen(GameScreen::Title);
                    return;
                }
            }
            else
            {
                backHover_[idx] = std::max(0.f, backHover_[idx] - float(dt) * 1.2f);
            }
        }
    }
}

void MenuUI::onMouseMove(int x, int y)
{
    mousePos_ = cv::Point(x, y);
}

void MenuUI::onMouseClick(int x, int y, const cv::Size &frameSize)
{
    ensureBackground_(frameSize);
    cv::Point pt(x, y);

    if (screen_ == GameScreen::Title)
    {
        if (btnPlayRect_.contains(pt))
            setScreen(GameScreen::CharacterSelect);
        else if (btnInfoRect_.contains(pt))
            setScreen(GameScreen::Info);
        else if (btnGlowRect_.contains(pt))
            glowEnabled_ = !glowEnabled_;
    }
    else if (screen_ == GameScreen::Info)
    {
        if (btnBackRect_.contains(pt))
            setScreen(GameScreen::Title);
    }
    else if (screen_ == GameScreen::CharacterSelect)
    {
        for (size_t i = 0; i < chars_.size(); ++i)
        {
            if (chars_[i].cardRect.contains(pt))
            {
                if (chosen_[0] < 0)
                    chosen_[0] = static_cast<int>(i);
                else if (chosen_[1] < 0)
                    chosen_[1] = static_cast<int>(i);
                if (chosen_[0] >= 0 && chosen_[1] >= 0)
                    screen_ = GameScreen::DuelReady;
                break;
            }
        }
    }
    else if (screen_ == GameScreen::DuelReady)
    {
        if (btnBackRect_.contains(pt))
            setScreen(GameScreen::Title);
    }
}

void MenuUI::draw(cv::Mat &frame) const
{
    if (frame.empty())
        return;
    if (bgCache_.size() != frame.size())
    {
        const_cast<MenuUI *>(this)->ensureBackground_(frame.size());
    }

    if (screen_ == GameScreen::Title && !titleBgCache_.empty())
        titleBgCache_.copyTo(frame);
    else
        bgCache_.copyTo(frame);

    if (screen_ == GameScreen::Title)
    {
        drawTitleScreen_(frame);
    }
    else if (screen_ == GameScreen::Info)
    {
        drawInfoScreen_(frame);
    }
    else
    {
        drawCharacterSelect_(frame);
    }
}

namespace
{
    // Paleta (BGR): tons de roxo e azul
    const cv::Scalar kManaPalette[] = {
        cv::Scalar(200, 60, 150),  // roxo
        cv::Scalar(255, 130, 180), // lavanda
        cv::Scalar(230, 70, 90),   // indigo
        cv::Scalar(255, 120, 50),  // azul
        cv::Scalar(255, 170, 80),  // azul claro
    };
    constexpr int kManaPaletteSize = 5;
    constexpr int kManaBlockCount = 30;
    constexpr float kSideBand = 0.16f; // largura de cada faixa lateral (fracao da largura)

    float frand(unsigned &s)
    { // LCG simples em [0, 1)
        s = s * 1664525u + 1013904223u;
        return float((s >> 8) & 0xFFFFFF) / float(0x1000000);
    }

    // Cantos de um quadrado de lado 2*half girado em 'ang'
    void quadPoints(float cx, float cy, float half, float ang, cv::Point out[4])
    {
        const float c = std::cos(ang), s = std::sin(ang);
        const float dx[4] = {-half, half, half, -half};
        const float dy[4] = {-half, -half, half, half};
        for (int i = 0; i < 4; ++i)
            out[i] = cv::Point(cvRound(cx + dx[i] * c - dy[i] * s), cvRound(cy + dx[i] * s + dy[i] * c));
    }
} // namespace

void MenuUI::updateManaBlocks_(const cv::Size &size, double dt)
{
    if (size.width <= 0 || size.height <= 0)
        return;
    const float scale = size.height / 720.f;
    const float fdt = float(dt);
    titleTime_ += fdt;

    // initial = true: espalha abaixo da tela para entrarem escalonados, vindo bem de baixo
    auto spawn = [&](ManaBlock &b, bool initial)
    {
        b.size = (9.f + frand(rngState_) * 22.f) * scale;
        const float band = size.width * kSideBand;
        const bool left = frand(rngState_) < 0.5f;
        const float off = frand(rngState_) * band;
        b.x = left ? off : size.width - off;
        b.y = size.height + b.size * 1.5f + (initial ? frand(rngState_) * size.height * 1.2f : frand(rngState_) * size.height * 0.15f);
        b.speed = ((30.f + frand(rngState_) * 70.f) * (b.size / (18.f * scale)) * 0.6f + 20.f) * scale;
        b.drift = (4.f + frand(rngState_) * 10.f) * scale;
        b.phase = frand(rngState_) * 6.2832f;
        b.angle = frand(rngState_) * 6.2832f;
        b.spin = (frand(rngState_) - 0.5f) * 2.4f; // ~ +-1.2 rad/s
        b.colorIdx = int(frand(rngState_) * kManaPaletteSize) % kManaPaletteSize;
    };

    if (manaBlocks_.empty())
    {
        manaBlocks_.resize(kManaBlockCount);
        for (auto &b : manaBlocks_)
            spawn(b, true);
    }
    for (auto &b : manaBlocks_)
    {
        b.y -= b.speed * fdt;
        b.angle += b.spin * fdt;
        if (b.y < -b.size * 2.f)
            spawn(b, false);
    }
}

void MenuUI::drawManaBlocks_(cv::Mat &frame) const
{
    if (manaBlocks_.empty() || frame.empty())
        return;
    const float H = float(frame.rows);

    auto fade = [&](const ManaBlock &b)
    {
        const float fadeIn = std::clamp((H - b.y) / (H * 0.12f), 0.f, 1.f);
        const float fadeOut = std::clamp(b.y / (H * 0.35f), 0.f, 1.f);
        return fadeIn * fadeOut;
    };
    auto centerX = [&](const ManaBlock &b)
    {
        return b.x + std::sin(titleTime_ * 0.9f + b.phase) * b.drift;
    };

    // Desenha um bloco (nucleo + borda clara + miolo) com offset 'o' em 'dst'
    auto drawBlock = [&](cv::Mat &dst, const ManaBlock &b, double a, cv::Point2f o, bool halo)
    {
        const float cx = centerX(b) - o.x, cy = b.y - o.y;
        const float half = std::max(2.f, b.size * 0.5f);
        const cv::Scalar col = kManaPalette[b.colorIdx];
        cv::Point p[4];
        if (halo)
        {
            quadPoints(cx, cy, half * 2.f, b.angle, p);
            cv::fillConvexPoly(dst, p, 4, col * (0.25 * a));
        }
        quadPoints(cx, cy, half, b.angle, p);
        cv::fillConvexPoly(dst, p, 4, col * (0.55 * a));
        const cv::Point *pp = p;
        const int np = 4;
        cv::polylines(dst, &pp, &np, 1, true, (col * 0.4 + cv::Scalar(255, 255, 255) * 0.6) * a, 1);
        quadPoints(cx, cy, std::max(1.f, half / 3.f), b.angle, p);
        cv::fillConvexPoly(dst, p, 4, (col * 0.3 + cv::Scalar(255, 255, 255) * 0.7) * a);
    };

    if (!glowEnabled_)
    {
        // Modo leve: so os blocos, adicionados em pequenas ROIs, sem desfoque
        for (const ManaBlock &b : manaBlocks_)
        {
            const double a = fade(b) * (0.75 + 0.25 * std::sin(titleTime_ * 3.f + b.phase));
            if (a <= 0.02)
                continue;
            const int R = cvRound(b.size * 0.75f) + 3; // meia diagonal + folga
            cv::Rect r(cvRound(centerX(b)) - R, cvRound(b.y) - R, 2 * R, 2 * R);
            r &= cv::Rect(0, 0, frame.cols, frame.rows);
            if (r.empty())
                continue;
            cv::Mat tmp = cv::Mat::zeros(r.size(), frame.type());
            drawBlock(tmp, b, a, cv::Point2f(float(r.x), float(r.y)), false);
            cv::Mat roi = frame(r);
            cv::add(roi, tmp, roi);
        }
        return;
    }

    // Modo com brilho: camada aditiva + versao desfocada (em baixa resolucao por desempenho)
    cv::Mat layer = cv::Mat::zeros(frame.size(), frame.type());
    for (const ManaBlock &b : manaBlocks_)
    {
        const double a = fade(b) * (0.75 + 0.25 * std::sin(titleTime_ * 3.f + b.phase));
        if (a > 0.02)
            drawBlock(layer, b, a, cv::Point2f(0.f, 0.f), false);
    }

    cv::Mat small, glow;
    cv::resize(layer, small, cv::Size(std::max(1, frame.cols / 4), std::max(1, frame.rows / 4)),
               0, 0, cv::INTER_AREA);
    cv::GaussianBlur(small, small, cv::Size(0, 0), 2.5);
    small.convertTo(small, -1, 2.2);
    cv::resize(small, glow, frame.size(), 0, 0, cv::INTER_LINEAR);

    cv::add(frame, glow, frame);
    cv::add(frame, layer, frame);
}

void MenuUI::drawTitleScreen_(cv::Mat &frame) const
{
    drawManaBlocks_(frame);

    // Botao esquerdo: "Jogar"
    float pProg = std::max(playHover_[0], playHover_[1]);
    if (!drawImageButton_(frame, kBtnPlay, btnPlayRect_, pProg, cv::Scalar(80, 255, 140)))
    {
        drawPixelPanel_(frame, btnPlayRect_,
                        cv::Scalar(25, 48, 32),
                        cv::Scalar(80, 220, 120),
                        cv::Scalar(50, 130, 75),
                        pProg > 0.05f);
        if (pProg > 0.01f)
        {
            int barW = cvRound(btnPlayRect_.width * pProg);
            cv::rectangle(frame, {btnPlayRect_.x, btnPlayRect_.br().y - 6},
                          {btnPlayRect_.x + barW, btnPlayRect_.br().y}, cv::Scalar(80, 255, 140), cv::FILLED);
        }
        putCenteredText(frame, "Jogar", btnPlayRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, cv::Scalar(220, 255, 225), 2);
    }

    // Botao direito: "info"
    float iProg = std::max(infoHover_[0], infoHover_[1]);
    if (!drawImageButton_(frame, kBtnInfo, btnInfoRect_, iProg, cv::Scalar(120, 230, 255)))
    {
        drawPixelPanel_(frame, btnInfoRect_,
                        cv::Scalar(28, 26, 48),
                        cv::Scalar(220, 175, 70),
                        cv::Scalar(125, 95, 40),
                        iProg > 0.05f);
        if (iProg > 0.01f)
        {
            int barW = cvRound(btnInfoRect_.width * iProg);
            cv::rectangle(frame, {btnInfoRect_.x, btnInfoRect_.br().y - 6},
                          {btnInfoRect_.x + barW, btnInfoRect_.br().y}, cv::Scalar(120, 230, 255), cv::FILLED);
        }
        putCenteredText(frame, "info", btnInfoRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, cv::Scalar(255, 240, 195), 2);
    }

    // Botao de brilhos (ligado/desligado)
    float gProg = std::max(glowHover_[0], glowHover_[1]);
    const bool on = glowEnabled_;
    if (!drawImageButton_(frame, on ? kBtnEffectsOn : kBtnEffectsOff, btnGlowRect_, gProg,
                          cv::Scalar(255, 180, 240)))
    {
        drawPixelPanel_(frame, btnGlowRect_,
                        on ? cv::Scalar(40, 30, 56) : cv::Scalar(34, 34, 34),
                        on ? cv::Scalar(255, 150, 220) : cv::Scalar(130, 130, 130),
                        on ? cv::Scalar(140, 80, 120) : cv::Scalar(80, 80, 80),
                        gProg > 0.05f);
        if (gProg > 0.01f)
        {
            int barW = cvRound(btnGlowRect_.width * gProg);
            cv::rectangle(frame, {btnGlowRect_.x, btnGlowRect_.br().y - 5},
                          {btnGlowRect_.x + barW, btnGlowRect_.br().y}, cv::Scalar(255, 180, 240), cv::FILLED);
        }
        putCenteredText(frame, on ? "Brilhos: LIGADOS" : "Brilhos: DESLIGADOS", btnGlowRect_,
                        cv::FONT_HERSHEY_DUPLEX, 0.7,
                        on ? cv::Scalar(255, 220, 245) : cv::Scalar(190, 190, 190), 1);
    }
}

void MenuUI::drawInfoScreen_(cv::Mat &frame) const
{
    cv::Rect panel(frame.cols / 8, frame.rows / 7, (frame.cols * 3) / 4, (frame.rows * 5) / 8);
    drawPixelPanel_(frame, panel, cv::Scalar(22, 18, 32), cv::Scalar(70, 200, 235), cv::Scalar(110, 80, 45), false);

    putCenteredText(frame, "FRACTAL ARCANA - INFO", {panel.x, panel.y + 12, panel.width, 45},
                    cv::FONT_HERSHEY_DUPLEX, 0.85, cv::Scalar(140, 245, 255), 2);

    std::vector<std::string> lines{
        "1. Aponte ou desenhe com a varinha sobre 'Jogar' (ou clique).",
        "2. Dois jogadores (P1 e P2) escolhem seus magos desenhando sobre eles.",
        "3. Personagens: Derzu Omaia, Meneghetti o Macom, Venegas,",
        "   Neri, Baltar MST e SubraMana.",
        "4. Use gestos e tracos para conjurar ataques e escudos."};

    int y = panel.y + 75;
    for (const auto &l : lines)
    {
        cv::putText(frame, l, {panel.x + 28, y}, cv::FONT_HERSHEY_SIMPLEX, 0.58,
                    cv::Scalar(235, 235, 245), 1, cv::LINE_AA);
        y += 34;
    }

    float bProg = std::max(backHover_[0], backHover_[1]);
    drawPixelPanel_(frame, btnBackRect_, cv::Scalar(35, 25, 40), cv::Scalar(90, 190, 230), cv::Scalar(80, 70, 110), bProg > 0.05f);
    putCenteredText(frame, "Voltar", btnBackRect_, cv::FONT_HERSHEY_DUPLEX, 0.85, cv::Scalar(240, 240, 250), 2);
}

void MenuUI::drawCharacterSelect_(cv::Mat &frame) const
{
    std::string header = "ESCOLHA SEU MAGO - P1: " + characterName(chosen_[0]) +
                         " | P2: " + characterName(chosen_[1]);
    putCenteredText(frame, header, {0, 12, frame.cols, 44},
                    cv::FONT_HERSHEY_DUPLEX, 0.72, cv::Scalar(140, 245, 255), 2);

    for (size_t i = 0; i < chars_.size(); ++i)
    {
        const auto &c = chars_[i];
        bool p1Sel = (chosen_[0] == static_cast<int>(i));
        bool p2Sel = (chosen_[1] == static_cast<int>(i));
        bool anyProg = (c.drawProgress[0] > 0.02f || c.drawProgress[1] > 0.02f || p1Sel || p2Sel);

        cv::Scalar borderCol = cv::Scalar(90, 175, 210);
        if (p1Sel && p2Sel)
            borderCol = cv::Scalar(255, 255, 100);
        else if (p1Sel)
            borderCol = cv::Scalar(255, 140, 20);
        else if (p2Sel)
            borderCol = cv::Scalar(60, 255, 80);

        drawPixelPanel_(frame, c.cardRect, cv::Scalar(20, 18, 30), borderCol, cv::Scalar(110, 85, 45), anyProg);

        // Retrato do personagem dentro do card
        cv::Rect imgBox(c.cardRect.x + 10, c.cardRect.y + 10,
                        std::max(1, c.cardRect.width - 20),
                        std::max(1, c.cardRect.height - 44));
        if (!c.portrait.empty() && imgBox.width > 10 && imgBox.height > 10)
        {
            cv::Mat resized;
            cv::resize(c.portrait, resized, imgBox.size(), 0, 0, cv::INTER_NEAREST);
            resized.copyTo(frame(imgBox));
        }

        // Camada de desenho da varinha sobre o personagem
        if (!c.drawLayer.empty() && c.drawLayer.size() == c.cardRect.size())
        {
            cv::Mat gray, mask;
            cv::cvtColor(c.drawLayer, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, mask, 1, 255, cv::THRESH_BINARY);
            c.drawLayer.copyTo(frame(c.cardRect), mask);
        }

        // Barra de progresso do desenho (P1 em cima, P2 embaixo da base do card)
        if (c.drawProgress[0] > 0.f)
        {
            int w1 = cvRound((c.cardRect.width - 16) * c.drawProgress[0]);
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.br().y - 34},
                          {c.cardRect.x + 8 + w1, c.cardRect.br().y - 29}, cv::Scalar(255, 140, 20), cv::FILLED);
        }
        if (c.drawProgress[1] > 0.f)
        {
            int w2 = cvRound((c.cardRect.width - 16) * c.drawProgress[1]);
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.br().y - 28},
                          {c.cardRect.x + 8 + w2, c.cardRect.br().y - 23}, cv::Scalar(60, 255, 80), cv::FILLED);
        }

        // Nome do personagem
        cv::Rect nameBox(c.cardRect.x + 6, c.cardRect.br().y - 24, c.cardRect.width - 12, 22);
        putCenteredText(frame, c.name, nameBox, cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(255, 248, 220), 1);

        // Badges P1 / P2 se escolhido
        if (p1Sel)
        {
            cv::rectangle(frame, {c.cardRect.x + 8, c.cardRect.y + 8}, {c.cardRect.x + 42, c.cardRect.y + 30},
                          cv::Scalar(255, 130, 0), cv::FILLED);
            cv::putText(frame, "P1", {c.cardRect.x + 13, c.cardRect.y + 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                        cv::Scalar(255, 255, 255), 2);
        }
        if (p2Sel)
        {
            cv::rectangle(frame, {c.cardRect.br().x - 42, c.cardRect.y + 8}, {c.cardRect.br().x - 8, c.cardRect.y + 30},
                          cv::Scalar(20, 200, 40), cv::FILLED);
            cv::putText(frame, "P2", {c.cardRect.br().x - 37, c.cardRect.y + 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                        cv::Scalar(255, 255, 255), 2);
        }
    }

    if (screen_ == GameScreen::DuelReady)
    {
        cv::Rect banner(frame.cols / 5, frame.rows / 2 - 42, (frame.cols * 3) / 5, 84);
        drawPixelPanel_(frame, banner, cv::Scalar(25, 20, 40), cv::Scalar(80, 255, 160), cv::Scalar(220, 190, 60), true);
        std::string msg = "PRONTOS! " + characterName(chosen_[0]) + " vs " + characterName(chosen_[1]);
        putCenteredText(frame, msg, banner, cv::FONT_HERSHEY_DUPLEX, 0.75, cv::Scalar(160, 255, 190), 2);

        float bProg = std::max(backHover_[0], backHover_[1]);
        drawPixelPanel_(frame, btnBackRect_, cv::Scalar(35, 25, 40), cv::Scalar(90, 190, 230), cv::Scalar(80, 70, 110), bProg > 0.05f);
        putCenteredText(frame, "Menu", btnBackRect_, cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(240, 240, 250), 2);
    }
}