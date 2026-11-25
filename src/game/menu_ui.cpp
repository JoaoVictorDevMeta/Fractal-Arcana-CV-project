#include "game/menu_ui.h"
#include "game/specials.h"
#include "game/spells.h"
#include "render/pixel_fx.h"
#include "render/special_fx.h"
#include "render/sprites.h"
#include "render/ui_widgets.h"
#include <array>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <map>
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
        {"derzu", "Derzu Omaia", "derzu_select", "derzu_player.png", "O Programador Arcano"},
        {"menegueti", "Meneghetti, o Macom", "menegueti_select", "menegueti_player.png", "Mestre da Loja Dourada"},
        {"venegas", "Venegas", "venegas_select", "venegas_player.png", "O General das Runas"},
        {"neri", "Neri", "neri-select", "neri_player.png", "Tecnomago do Vortice"},
        {"baltar", "Baltar MST", "baltar_select", "baltar_player.png", "Guardiao da Floresta"},
        {"subramana", "SubraMana", "anand_select", "anand_player.png", "O Ciborgue da Mana"},
    };
}

const cv::Mat &MenuUI::characterPortrait(int idx) const
{
    static const cv::Mat empty;
    if (idx < 0 || idx >= static_cast<int>(chars_.size()))
        return empty;
    return chars_[idx].portrait;
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

        // Sprite de corpo inteiro para o painel lateral da selecao
        cv::Mat body = cv::imread(assetsDir + "/sprites/players/" + chars_[i].playerImage, cv::IMREAD_UNCHANGED);
        if (!body.empty())
        {
            if (body.depth() == CV_16U)
                body.convertTo(body, CV_8U, 1.0 / 257.0);
            if (body.channels() == 3)
                cv::cvtColor(body, body, cv::COLOR_BGR2BGRA);
            else if (body.channels() == 1)
                cv::cvtColor(body, body, cv::COLOR_GRAY2BGRA);
            if (body.channels() == 4)
                chars_[i].fullBody = cropToAlpha(body);
        }
    }
    lastSize_ = cv::Size(0, 0); // refaz os caches de desenho
    bgCache_.release();
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
    hoverIdx_[0] = hoverIdx_[1] = -1;
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

    // Logo da tela inicial (PNG com transparencia).
    // O arquivo deve ficar em assets/main_title.png.
    if (titleLogoSrc_.empty())
    {
        cv::Mat logo = cv::imread("assets/main_title.png", cv::IMREAD_UNCHANGED);
        if (!logo.empty())
        {
            if (logo.depth() == CV_16U)
                logo.convertTo(logo, CV_8U, 1.0 / 257.0);
            if (logo.channels() == 3)
                cv::cvtColor(logo, logo, cv::COLOR_BGR2BGRA);
            else if (logo.channels() == 1)
                cv::cvtColor(logo, logo, cv::COLOR_GRAY2BGRA);

            if (logo.channels() == 4)
            {
                cv::Mat alpha, mask;
                cv::extractChannel(logo, alpha, 3);
                cv::threshold(alpha, mask, 8, 255, cv::THRESH_BINARY);
                std::vector<cv::Point> points;
                cv::findNonZero(mask, points);
                if (!points.empty())
                {
                    // Calcula manualmente o retangulo dos pixels nao transparentes.
                    // Evita depender de cv::boundingRect entre versoes do OpenCV.
                    int minX = logo.cols, minY = logo.rows;
                    int maxX = -1, maxY = -1;
                    for (const cv::Point &pt : points)
                    {
                        minX = std::min(minX, pt.x);
                        minY = std::min(minY, pt.y);
                        maxX = std::max(maxX, pt.x);
                        maxY = std::max(maxY, pt.y);
                    }

                    const int pad = 8;
                    const int x0 = std::max(0, minX - pad);
                    const int y0 = std::max(0, minY - pad);
                    const int x1 = std::min(logo.cols, maxX + pad + 1);
                    const int y1 = std::min(logo.rows, maxY + pad + 1);
                    const cv::Rect bounds(x0, y0, x1 - x0, y1 - y0);
                    titleLogoSrc_ = logo(bounds).clone();
                }
            }
        }
        if (titleLogoSrc_.empty())
            std::fprintf(stderr, "Aviso: nao foi possivel carregar assets/main_title.png\\n");
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
        "assets/button_effects_on.png",   // kBtnEffectsOn
        "assets/button_effects_off.png"}; // kBtnEffectsOff

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

    // Logo centralizado, mantendo a proporcao original e deixando espaco para os botoes.
    titleLogoNorm_.release();
    titleLogoGlow_.release();
    titleLogoGlowPad_ = 0;
    if (!titleLogoSrc_.empty())
    {
        const double maxW = w * 0.82;
        const double maxH = h * 0.28;
        const double scale = std::min(maxW / titleLogoSrc_.cols,
                                      maxH / titleLogoSrc_.rows);
        const int logoW = std::max(1, cvRound(titleLogoSrc_.cols * scale));
        const int logoH = std::max(1, cvRound(titleLogoSrc_.rows * scale));
        cv::resize(titleLogoSrc_, titleLogoNorm_, cv::Size(logoW, logoH),
                   0, 0, scale < 1.0 ? cv::INTER_AREA : cv::INTER_CUBIC);

        titleFrameRect_ = cv::Rect((w - logoW) / 2,
                                   cvRound(h * 0.32 - logoH / 2.0),
                                   logoW, logoH);

        // Mascara desfocada para o halo roxo, com margem para nao cortar o brilho.
        titleLogoGlowPad_ = std::max(14, cvRound(logoW * 0.025));
        cv::Mat alpha, glowMask;
        cv::extractChannel(titleLogoNorm_, alpha, 3);
        cv::Mat padded = cv::Mat::zeros(logoH + 2 * titleLogoGlowPad_,
                                        logoW + 2 * titleLogoGlowPad_, CV_8U);
        alpha.copyTo(padded(cv::Rect(titleLogoGlowPad_, titleLogoGlowPad_, logoW, logoH)));
        cv::GaussianBlur(padded, glowMask, cv::Size(0, 0), titleLogoGlowPad_ * 0.55);

        titleLogoGlow_ = cv::Mat::zeros(glowMask.size(), CV_8UC3);
        std::vector<cv::Mat> glowChannels;
        cv::split(titleLogoGlow_, glowChannels);
        // Roxo/lavanda em BGR.
        glowMask.convertTo(glowChannels[0], CV_8U, 0.72);
        glowMask.convertTo(glowChannels[1], CV_8U, 0.28);
        glowMask.convertTo(glowChannels[2], CV_8U, 1.00);
        cv::merge(glowChannels, titleLogoGlow_);
    }
    else
    {
        titleFrameRect_ = cv::Rect(w / 2 - w * 28 / 100,
                                   h / 2 - h * 11 / 100,
                                   w * 56 / 100, h * 22 / 100);
    }

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
    int visW = ((w * 94 / 100 - 2 * rowGap) / 3) * 0.8;
    int visH = (cvRound(visW / innerAspect)) * 0.8;
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

    // Estilo jogo de luta: cabecalho, grade 3x2 de retratos no centro e um painel
    // lateral por jogador mostrando o mago em destaque.
    const int m = std::max(10, w / 90);
    const int headerH = std::max(64, h * 14 / 100);
    const int footerH = std::max(30, h * 6 / 100);
    const int sideW = std::max(160, w * 22 / 100);
    selectHeader_ = cv::Rect(0, 0, w, headerH);
    selectFooter_ = cv::Rect(0, h - footerH, w, footerH);
    const int bodyY = headerH + m / 2;
    const int bodyH = std::max(100, h - headerH - footerH - m);
    previewRect_[0] = cv::Rect(m, bodyY, sideW, bodyH);
    previewRect_[1] = cv::Rect(w - m - sideW, bodyY, sideW, bodyH);

    const int cols = 3;
    const int rows = 2;
    const int gap = std::max(8, w / 100);
    const int gridX = previewRect_[0].br().x + 2 * m;
    const int gridW = std::max(cols * 60, previewRect_[1].x - 2 * m - gridX);
    const int cardW = (gridW - gap * (cols - 1)) / cols;
    const int cardH = (bodyH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < chars_.size(); ++i)
    {
        int r = static_cast<int>(i) / cols;
        int c = static_cast<int>(i) % cols;
        cv::Rect rect(gridX + c * (cardW + gap), bodyY + r * (cardH + gap), cardW, cardH);
        chars_[i].cardRect = rect;
        if (chars_[i].drawLayer.empty() || chars_[i].drawLayer.size() != rect.size())
        {
            chars_[i].drawLayer = cv::Mat::zeros(rect.size(), CV_8UC3);
        }

        // retrato preenchendo o card sem distorcer (foco no rosto)
        const int inset = 5;
        chars_[i].cardImg = coverCrop(chars_[i].portrait,
                                      cv::Size(std::max(1, cardW - 2 * inset), std::max(1, cardH - 2 * inset)), 0.32f);

        // corpo inteiro para os paineis laterais (mantendo a proporcao)
        chars_[i].preview[0].release();
        chars_[i].preview[1].release();
        const cv::Mat &fb = chars_[i].fullBody;
        if (!fb.empty())
        {
            const double maxW = sideW - 24.0, maxH = bodyH * 0.50;
            const double s = std::min(maxW / fb.cols, maxH / fb.rows);
            chars_[i].preview[0] = resizeBGRA(fb, s);
            cv::flip(chars_[i].preview[0], chars_[i].preview[1], 1);
        }
    }
}

void MenuUI::drawPixelPanel_(cv::Mat &img, const cv::Rect &r,
                             const cv::Scalar &fill,
                             const cv::Scalar &borderOuter,
                             const cv::Scalar &borderInner,
                             bool highlight,
                             const cv::Scalar &cornerCol) const
{
    cv::rectangle(img, r, fill, cv::FILLED);

    // Borda dupla estilo pergaminho / placa medieval
    cv::rectangle(img, r, highlight ? cv::Scalar(80, 250, 255) : borderOuter, 3);
    cv::Rect inner(r.x + 6, r.y + 6, std::max(1, r.width - 12), std::max(1, r.height - 12));
    cv::rectangle(img, inner, borderInner, 1);

    // Cantoneiras decorativas nos 4 cantos
    const int cLen = 12;
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
        hoverIdx_[playerIdx] = -1;
        return;
    }

    cv::Point2f cur = trk.pos;
    cv::Point pt(cvRound(cur.x), cvRound(cur.y));
    hoverIdx_[playerIdx] = hoveredCard_(pt);

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
    if (screen_ != GameScreen::Title) // relogio das animacoes das outras telas
        titleTime_ += float(dt);

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

    // Logo "Fractal Arcana" com halo roxo suave, pulsando (cresce e encolhe).
    if (!titleLogoNorm_.empty())
    {
        constexpr float kAmp = 0.06f;
        const float breathe = 1.f + kAmp * std::sin(titleTime_ * 2.0f);

        // A escala e aplicada com warpAffine em subpixel, num quadro de tamanho FIXO
        // (imagem + margem para o maximo crescimento). Assim nem o tamanho nem a posicao
        // sao arredondados a cada frame, e o logo nao treme.
        auto scaledCanvas = [&](const cv::Mat &src, cv::Point &topLeft) -> cv::Mat
        {
            const int padX = cvCeil(src.cols * kAmp) + 2;
            const int padY = cvCeil(src.rows * kAmp) + 2;
            const cv::Size canvas(src.cols + 2 * padX, src.rows + 2 * padY);
            const double cx = src.cols * 0.5, cy = src.rows * 0.5;
            const double M[2][3] = {
                {breathe, 0.0, (cx + padX) - breathe * cx},
                {0.0, breathe, (cy + padY) - breathe * cy}};
            cv::Mat out;
            cv::warpAffine(src, out, cv::Mat(2, 3, CV_64F, (void *)M), canvas,
                           cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar::all(0));
            topLeft = cv::Point(-padX, -padY);
            return out;
        };

        const cv::Point logoTL = titleFrameRect_.tl();

        if (glowEnabled_ && !titleLogoGlow_.empty())
        {
            const float pulse = 0.82f + 0.12f * std::sin(titleTime_ * 2.0f);
            cv::Point off;
            const cv::Mat glowImg = scaledCanvas(titleLogoGlow_, off);
            // O glow original ja comeca em logoTL - glowPad.
            const cv::Point glowTL = logoTL - cv::Point(titleLogoGlowPad_, titleLogoGlowPad_) + off;
            const cv::Rect glowRect(glowTL, glowImg.size());
            const cv::Rect visible = glowRect & cv::Rect(0, 0, frame.cols, frame.rows);
            if (!visible.empty())
            {
                cv::Mat glow;
                glowImg(cv::Rect(visible.x - glowRect.x, visible.y - glowRect.y,
                                 visible.width, visible.height))
                    .convertTo(glow, CV_8UC3, pulse * 0.72);
                cv::Mat roi = frame(visible);
                cv::add(roi, glow, roi);
            }
        }

        cv::Point off;
        const cv::Mat logoImg = scaledCanvas(titleLogoNorm_, off);
        const cv::Point tl = logoTL + off;
        drawSprite(frame, logoImg, tl + cv::Point(logoImg.cols / 2, logoImg.rows / 2));
    }

    // Estilo dos botoes: painel escuro, borda roxa, texto roxo suave
    const cv::Scalar panelFill(28, 22, 40);
    const cv::Scalar panelBorder(200, 120, 160); // BGR: roxo da borda
    const cv::Scalar panelInner(110, 60, 90);    // BGR: roxo escuro (borda interna)
    const cv::Scalar titlePurple(230, 170, 200); // BGR: roxo suave

    // Painel de botao: halo branco atras (fraco em repouso, forte e pulsando no hover;
    // desligado junto com os demais brilhos) + painel com borda roxa que clareia no hover.
    auto drawButtonPanel = [&](const cv::Rect &r, int slot)
    {
        const float hv = btnHover_[slot];

        if (glowEnabled_)
        {
            const float k = (0.30f + 0.70f * hv) * (1.f + 0.08f * hv * std::sin(titleTime_ * 7.f));
            const int pad = std::max(14, cvRound(r.width * 0.09));
            const cv::Rect big(r.x - pad, r.y - pad, r.width + 2 * pad, r.height + 2 * pad);
            const cv::Rect vis = big & cv::Rect(0, 0, frame.cols, frame.rows);
            if (!vis.empty())
            {
                cv::Mat glow = cv::Mat::zeros(big.size(), frame.type());
                cv::rectangle(glow, cv::Rect(pad, pad, r.width, r.height),
                              cv::Scalar::all(255.0 * 0.9 * std::min(1.f, k)), cv::FILLED);
                cv::GaussianBlur(glow, glow, cv::Size(0, 0), pad * 0.45);
                cv::Mat dstRoi = frame(vis);
                cv::add(dstRoi, glow(cv::Rect(vis.x - big.x, vis.y - big.y, vis.width, vis.height)), dstRoi);
            }
        }

        const cv::Scalar hot(255, 210, 235);
        const cv::Scalar border = panelBorder * (1.f - hv) + hot * hv;
        drawPixelPanel_(frame, r, panelFill, border, panelInner, false, titlePurple);
    };

    // Botao esquerdo: "Jogar"
    float pProg = std::max(playHover_[0], playHover_[1]);
    if (!drawImageButton_(frame, kBtnPlay, btnPlayRect_, pProg, cv::Scalar(80, 255, 140)))
    {
        drawButtonPanel(btnPlayRect_, 0);
        if (pProg > 0.01f)
        {
            int barW = cvRound(btnPlayRect_.width * pProg);
            cv::rectangle(frame, {btnPlayRect_.x, btnPlayRect_.br().y - 6},
                          {btnPlayRect_.x + barW, btnPlayRect_.br().y}, cv::Scalar(80, 255, 140), cv::FILLED);
        }
        putCenteredText(frame, "Jogar", btnPlayRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, titlePurple, 2);
    }

    // Botao direito: "info"
    float iProg = std::max(infoHover_[0], infoHover_[1]);
    if (!drawImageButton_(frame, kBtnInfo, btnInfoRect_, iProg, cv::Scalar(120, 230, 255)))
    {
        drawButtonPanel(btnInfoRect_, 1);
        if (iProg > 0.01f)
        {
            int barW = cvRound(btnInfoRect_.width * iProg);
            cv::rectangle(frame, {btnInfoRect_.x, btnInfoRect_.br().y - 6},
                          {btnInfoRect_.x + barW, btnInfoRect_.br().y}, cv::Scalar(120, 230, 255), cv::FILLED);
        }
        putCenteredText(frame, "info", btnInfoRect_, cv::FONT_HERSHEY_DUPLEX, 0.95, titlePurple, 2);
    }

    // Botao de brilhos (ligado/desligado)
    float gProg = std::max(glowHover_[0], glowHover_[1]);
    const bool on = glowEnabled_;
    if (!drawImageButton_(frame, on ? kBtnEffectsOn : kBtnEffectsOff, btnGlowRect_, gProg,
                          cv::Scalar(255, 180, 240)))
    {
        drawButtonPanel(btnGlowRect_, 2);
        if (gProg > 0.01f)
        {
            int barW = cvRound(btnGlowRect_.width * gProg);
            cv::rectangle(frame, {btnGlowRect_.x, btnGlowRect_.br().y - 5},
                          {btnGlowRect_.x + barW, btnGlowRect_.br().y}, cv::Scalar(255, 180, 240), cv::FILLED);
        }
        putCenteredText(frame, on ? "Brilhos: LIGADOS" : "Brilhos: DESLIGADOS", btnGlowRect_,
                        cv::FONT_HERSHEY_DUPLEX, 0.7,
                        on ? titlePurple : cv::Scalar(150, 130, 150), 1);
    }

    drawLastMatch_(frame);
}

// Placar da ultima partida, entre o logo e os botoes (ou no canto, se nao couber).
void MenuUI::drawLastMatch_(cv::Mat &frame) const
{
    if (!lastMatch_.valid)
        return;
    const int W = frame.cols, H = frame.rows;
    const int FD = cv::FONT_HERSHEY_DUPLEX;
    const double k = std::clamp(H / 720.0, 0.6, 1.6);
    const cv::Scalar white(245, 240, 250), gold(90, 210, 250), digits(120, 235, 255);

    const int gapTop = titleFrameRect_.br().y + H * 3 / 100;
    const int gapBot = btnPlayRect_.y - H * 4 / 100;
    int ph = std::min(gapBot - gapTop, int(120 * k));
    cv::Rect r;
    if (ph >= int(80 * k))
        r = cv::Rect(W * 24 / 100, gapTop + (gapBot - gapTop - ph) / 2, W * 52 / 100, ph);
    else
    {
        ph = int(110 * k);
        r = cv::Rect(W / 60, H / 40 + int(14 * k), W * 34 / 100, ph);
    }

    ui::panel(frame, r, gold, 0.85);
    ui::panelTitle(frame, r, "ULTIMA PARTIDA", gold, 0.55 * k);

    const int half = r.width / 2;
    const int px = std::max(3, ph / 18);
    for (int p = 0; p < 2; ++p)
    {
        const cv::Rect hr(r.x + p * half, r.y, half, r.height);
        const cv::Scalar pc = ui::playerColor(p);
        const bool won = lastMatch_.winner == p;

        // "1P Nome"
        const std::string tag = p == 0 ? "1P" : "2P";
        const std::string &name = lastMatch_.name[p];
        const double ns = ui::fitScale(name, FD, 0.6 * k, 1, hr.width - 4 * px * 2 - 30);
        const cv::Size nz = pfx::textSize(name, FD, ns, 1);
        const int rowW = pfx::pixelTextWidth(tag, px) + 8 + nz.width;
        const int nameY = hr.y + ph * 40 / 100;
        const int x0 = hr.x + (hr.width - rowW) / 2;
        pfx::pixelText(frame, tag, {x0, nameY - 5 * px}, px, pc);
        pfx::outlinedText(frame, name, {x0 + pfx::pixelTextWidth(tag, px) + 8, nameY}, FD, ns, white, 1);

        // pontuacao em digitos de fliperama
        const std::string sc = cv::format("%06d", lastMatch_.score[p]);
        const int big = px * 3 / 2;
        const int sw = pfx::pixelTextWidth(sc, big);
        const cv::Point stl(hr.x + (hr.width - sw) / 2, nameY + int(10 * k));
        pfx::pixelText(frame, sc, stl + cv::Point(big / 2, big / 2), big, cv::Scalar(20, 10, 25));
        pfx::pixelText(frame, sc, stl, big, won ? gold : digits);

        if (won)
        {
            const std::string w = "VENCEU!";
            const cv::Size wz = pfx::textSize(w, FD, 0.5 * k, 1);
            const int wy = stl.y + 5 * big + int(22 * k);
            ui::drawStar(frame, {hr.x + (hr.width - wz.width) / 2 - int(14 * k), wy - wz.height / 2}, int(8 * k), gold);
            pfx::outlinedText(frame, w, {hr.x + (hr.width - wz.width) / 2, wy}, FD, 0.5 * k, gold, 1);
        }
    }
    // divisoria com "X" no meio
    cv::line(frame, {r.x + half, r.y + 14}, {r.x + half, r.br().y - 10}, gold * 0.5, 1);
    const int xw = pfx::pixelTextWidth("X", px);
    pfx::pixelText(frame, "X", {r.x + half - xw / 2, r.y + ph / 2 - 2 * px}, px, gold);
    if (lastMatch_.winner < 0)
        putCenteredText(frame, "partida interrompida", {r.x, r.br().y - int(24 * k), r.width, int(20 * k)},
                        cv::FONT_HERSHEY_SIMPLEX, 0.42 * k, cv::Scalar(200, 185, 205), 1);
}

// Halo branco atras de uma caixa (mesmo efeito dos botoes do menu): fraco em repouso,
// forte e pulsando de leve no hover/selecao. A mascara borrada fica em cache por tamanho,
// entao nao ha GaussianBlur por frame.
static void addRectGlow(cv::Mat &frame, const cv::Rect &r, float hv, float time)
{
    const float k = (0.30f + 0.70f * hv) * (1.f + 0.08f * hv * std::sin(time * 7.f));
    const int pad = std::max(14, cvRound(r.width * 0.09));
    const cv::Rect big(r.x - pad, r.y - pad, r.width + 2 * pad, r.height + 2 * pad);
    const cv::Rect vis = big & cv::Rect(0, 0, frame.cols, frame.rows);
    if (vis.empty())
        return;

    static std::map<std::pair<int, int>, cv::Mat> cache;
    const std::pair<int, int> key(r.width, r.height);
    auto it = cache.find(key);
    if (it == cache.end())
    {
        if (cache.size() > 16)
            cache.clear();
        cv::Mat m = cv::Mat::zeros(big.size(), CV_8U);
        cv::rectangle(m, cv::Rect(pad, pad, r.width, r.height), cv::Scalar(255), cv::FILLED);
        cv::GaussianBlur(m, m, cv::Size(0, 0), pad * 0.45);
        it = cache.emplace(key, std::move(m)).first;
    }

    cv::Mat g, g3;
    it->second(cv::Rect(vis.x - big.x, vis.y - big.y, vis.width, vis.height))
        .convertTo(g, CV_8U, 0.9 * std::min(1.f, k));
    cv::cvtColor(g, g3, cv::COLOR_GRAY2BGR);
    cv::Mat roi = frame(vis);
    cv::add(roi, g3, roi);
}


// ---------------------------------------------------------------------------
// Tela "info": grimorio organizado em tres paineis (como jogar, feiticos, especiais)
// ---------------------------------------------------------------------------

namespace
{
    // Miniatura do retrato (quadrada, foco no rosto) em cache por personagem e tamanho.
    const cv::Mat &thumbFor(int idx, const cv::Mat &portrait, int side)
    {
        static std::map<std::array<int, 2>, cv::Mat> cache;
        const std::array<int, 2> key = {idx, side};
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;
        if (cache.size() > 32)
            cache.clear();
        return cache.emplace(key, coverCrop(portrait, cv::Size(side, side), 0.28f)).first->second;
    }

    const char *spellBlurb(Shape s)
    {
        switch (s)
        {
        case Shape::Linha:
            return "Rapido e barato";
        case Shape::Raio:
            return "Corta a arena num instante";
        case Shape::Circulo:
            return "Bloqueia 1 golpe por 4s";
        case Shape::Triangulo:
            return "Lento, mas pesado";
        case Shape::Estrela:
            return "Especial do mago (ao lado)";
        default:
            return "";
        }
    }
} // namespace

void MenuUI::drawInfoScreen_(cv::Mat &frame) const
{
    const int W = frame.cols, H = frame.rows;
    const int F = cv::FONT_HERSHEY_SIMPLEX, FD = cv::FONT_HERSHEY_DUPLEX;
    const cv::Scalar white(245, 240, 250), soft(200, 185, 205), gold(90, 210, 250);
    const cv::Scalar lavender(230, 170, 200), cyan(255, 210, 120);
    const double k = std::clamp(H / 720.0, 0.6, 1.6); // escala de fonte pela altura da tela

    ui::fillTranslucent(frame, {0, 0, W, H}, cv::Scalar(8, 5, 12), 0.45);

    // --- cabecalho ---
    const std::string title = "GRIMORIO DO DUELO";
    const double ts = ui::fitScale(title, FD, 1.25 * k, 2, W * 80 / 100);
    const cv::Size tsz = pfx::textSize(title, FD, ts, 2);
    const int titleY = std::max(tsz.height + 8, H * 8 / 100);
    pfx::outlinedText(frame, title, {(W - tsz.width) / 2, titleY}, FD, ts, gold, 2);
    putCenteredText(frame, "Tudo o que um mago precisa saber antes do duelo",
                    {0, titleY + 6, W, int(28 * k)}, F, 0.5 * k, soft, 1);

    // --- tres paineis ---
    const int m = std::max(12, W / 70);
    const int top = titleY + int(54 * k);
    const int bottom = btnBackRect_.y - std::max(14, H / 45);
    const int panelH = std::max(120, bottom - top);
    const int avail = W - 4 * m;
    const cv::Rect pA(m, top, avail * 29 / 100, panelH);
    const cv::Rect pB(pA.br().x + m, top, avail * 33 / 100, panelH);
    const cv::Rect pC(pB.br().x + m, top, W - m - (pB.br().x + m), panelH);

    // A) Como jogar
    ui::panel(frame, pA, lavender);
    ui::panelTitle(frame, pA, "COMO JOGAR", lavender, 0.6 * k);
    {
        const char *steps[] = {
            "Cada jogador fica numa metade da camera, com sua varinha.",
            "Pare a varinha, desenhe a forma no ar e pare de novo.",
            "Forma reconhecida + mana suficiente = feitico lancado!",
            "O Escudo (circulo) segura o proximo golpe.",
            "Zere a vida do oponente para vencer.",
        };
        const double fs = 0.46 * k;
        const int badge = int(22 * k);
        const int textX = pA.x + 18 + badge + 10;
        const int maxW = pA.br().x - 14 - textX;
        int y = pA.y + int(34 * k);
        for (int i = 0; i < 5; ++i)
        {
            const cv::Rect b(pA.x + 18, y - badge + 6, badge, badge);
            cv::rectangle(frame, b, lavender * 0.5, cv::FILLED);
            cv::rectangle(frame, b, lavender, 1);
            putCenteredText(frame, std::to_string(i + 1), b, FD, 0.5 * k, white, 1);
            y = ui::putWrapped(frame, steps[i], {textX, y}, maxW, F, fs, white, 1, int(6 * k)) + int(8 * k);
        }

        // teclas
        y += int(6 * k);
        cv::line(frame, {pA.x + 16, y - int(14 * k)}, {pA.br().x - 16, y - int(14 * k)}, lavender * 0.5, 1);
        cv::putText(frame, "TECLAS", {pA.x + 18, y}, FD, 0.5 * k, lavender, 1, cv::LINE_AA);
        y += int(24 * k);
        const char *keys[][2] = {{"ESPACO", "pausa"}, {"R", "reinicia"}, {"M", "menu"}, {"D", "debug"}, {"ESC", "sai"}};
        int x = pA.x + 18;
        for (const auto &kv : keys)
        {
            const cv::Size ks = pfx::textSize(kv[0], F, 0.42 * k, 1);
            const cv::Size ds = pfx::textSize(kv[1], F, 0.42 * k, 1);
            const int itemW = ks.width + 12 + 6 + ds.width + 14;
            if (x + itemW > pA.br().x - 14)
            {
                x = pA.x + 18;
                y += int(26 * k);
            }
            if (y > pA.br().y - 8)
                break;
            const cv::Rect kb(x, y - ks.height - 6, ks.width + 12, ks.height + 12);
            cv::rectangle(frame, kb, cv::Scalar(50, 38, 60), cv::FILLED);
            cv::rectangle(frame, kb, lavender, 1);
            cv::putText(frame, kv[0], {x + 6, y}, F, 0.42 * k, white, 1, cv::LINE_AA);
            cv::putText(frame, kv[1], {kb.br().x + 6, y}, F, 0.42 * k, soft, 1, cv::LINE_AA);
            x += itemW;
        }
    }

    // B) Feiticos: forma -> magia, custo e efeito (valores lidos da tabela do jogo)
    ui::panel(frame, pB, cyan);
    ui::panelTitle(frame, pB, "FEITICOS", cyan, 0.6 * k);
    {
        const Shape order[5] = {Shape::Linha, Shape::Raio, Shape::Circulo, Shape::Triangulo, Shape::Estrela};
        const int rowsTop = pB.y + int(22 * k);
        const int rowH = (pB.br().y - 10 - rowsTop) / 5;
        const int icon = std::min(rowH - 10, int(48 * k));
        for (int i = 0; i < 5; ++i)
        {
            const SpellDef &sd = spellFor(order[i]);
            const int y0 = rowsTop + i * rowH;
            if (i > 0)
                cv::line(frame, {pB.x + 14, y0}, {pB.br().x - 14, y0}, cyan * 0.3, 1);
            const cv::Rect ib(pB.x + 16, y0 + (rowH - icon) / 2, icon, icon);
            cv::rectangle(frame, ib, cv::Scalar(45, 30, 30), cv::FILLED);
            cv::rectangle(frame, ib, cyan * 0.7, 1);
            ui::drawShapeIcon(frame, order[i], ib, order[i] == Shape::Estrela ? gold : cyan, 2);

            const int tx = ib.br().x + 12;
            const int maxW = pB.br().x - 14 - tx;
            const int nameY = y0 + rowH / 2 - int(4 * k);
            cv::putText(frame, sd.name, {tx, nameY}, FD, ui::fitScale(sd.name, FD, 0.55 * k, 1, maxW), white, 1,
                        cv::LINE_AA);
            std::string stats = "Mana " + std::to_string(int(sd.manaCost));
            if (sd.type == SpellType::Escudo)
                stats += "  |  " + std::string(spellBlurb(order[i]));
            else
                stats += "  |  Dano " + std::to_string(int(sd.damage)) + "  |  " + spellBlurb(order[i]);
            cv::putText(frame, stats, {tx, nameY + int(22 * k)}, F, ui::fitScale(stats, F, 0.42 * k, 1, maxW), soft, 1,
                        cv::LINE_AA);
        }
    }

    // C) Especiais de cada mago (Estrela)
    ui::panel(frame, pC, gold);
    ui::panelTitle(frame, pC, "ESPECIAIS (ESTRELA)", gold, 0.6 * k);
    {
        const int n = static_cast<int>(chars_.size());
        const int rowsTop = pC.y + int(22 * k);
        const int rowH = std::max(30, (pC.br().y - 10 - rowsTop) / std::max(1, n));
        const int th = std::max(20, rowH - 10);
        for (int i = 0; i < n; ++i)
        {
            const CharacterDef &c = chars_[i];
            const SpecialKind sk = specialFor(c.id);
            const SpecialDef &sd = specialDef(sk);
            const cv::Scalar sc = specialColor(sk);
            const int y0 = rowsTop + i * rowH;
            if (i > 0)
                cv::line(frame, {pC.x + 14, y0}, {pC.br().x - 14, y0}, gold * 0.3, 1);

            const cv::Rect tb(pC.x + 16, y0 + (rowH - th) / 2, th, th);
            const cv::Mat &thumb = thumbFor(i, c.portrait, th);
            if (!thumb.empty() && thumb.size() == tb.size() && thumb.type() == frame.type() &&
                (tb & cv::Rect(0, 0, W, H)) == tb)
                thumb.copyTo(frame(tb));
            cv::rectangle(frame, tb, sc, 2);

            const int tx = tb.br().x + 12;
            const int maxW = pC.br().x - 14 - tx;
            const int line1 = y0 + rowH / 2 - int(6 * k);
            const std::string head = c.name + "  -  " + sd.name;
            cv::putText(frame, c.name, {tx, line1}, FD, ui::fitScale(head, FD, 0.5 * k, 1, maxW), white, 1, cv::LINE_AA);
            const int nameW = pfx::textSize(c.name, FD, ui::fitScale(head, FD, 0.5 * k, 1, maxW), 1).width;
            cv::putText(frame, std::string("  -  ") + sd.name, {tx + nameW, line1}, FD,
                        ui::fitScale(head, FD, 0.5 * k, 1, maxW), sc, 1, cv::LINE_AA);
            cv::putText(frame, sd.effect, {tx, line1 + int(20 * k)}, F, ui::fitScale(sd.effect, F, 0.42 * k, 1, maxW),
                        soft, 1, cv::LINE_AA);
        }
    }

    // --- botao voltar ---
    const float bProg = std::clamp(std::max(backHover_[0], backHover_[1]), 0.f, 1.f);
    if (glowEnabled_)
        addRectGlow(frame, btnBackRect_, bProg, titleTime_);
    ui::panel(frame, btnBackRect_, bProg > 0.05f ? cv::Scalar(255, 210, 235) : lavender, 0.9);
    if (bProg > 0.01f)
        cv::rectangle(frame, cv::Rect(btnBackRect_.x + 6, btnBackRect_.br().y - 9,
                                      cvRound((btnBackRect_.width - 12) * bProg), 4),
                      cv::Scalar(80, 255, 140), cv::FILLED);
    putCenteredText(frame, "Voltar", btnBackRect_, FD, 0.85 * k, white, 2);
}

// ---------------------------------------------------------------------------
// Selecao de personagens: grade de retratos + painel de destaque por jogador
// ---------------------------------------------------------------------------

int MenuUI::hoveredCard_(cv::Point pt) const
{
    for (size_t i = 0; i < chars_.size(); ++i)
        if (chars_[i].cardRect.contains(pt))
            return static_cast<int>(i);
    return -1;
}

void MenuUI::drawCharacterPreview_(cv::Mat &frame, int p) const
{
    const cv::Rect r = previewRect_[p];
    if (r.width <= 0 || r.height <= 0)
        return;
    const int F = cv::FONT_HERSHEY_SIMPLEX, FD = cv::FONT_HERSHEY_DUPLEX;
    const double k = std::clamp(frame.rows / 720.0, 0.6, 1.6);
    const cv::Scalar pc = ui::playerColor(p);
    const cv::Scalar white(245, 240, 250), soft(200, 185, 205);

    ui::panel(frame, r, pc, 0.80);

    // faixa "JOGADOR X"
    const int tagH = std::max(26, int(32 * k));
    const cv::Rect tag(r.x + 4, r.y + 4, r.width - 8, tagH);
    ui::fillTranslucent(frame, tag, pc * 0.45, 0.95);
    putCenteredText(frame, p == 0 ? "JOGADOR 1" : "JOGADOR 2", tag, FD, 0.6 * k, white, 1);

    // quem mostrar: o escolhido, senao o que a varinha (ou o mouse) aponta
    int idx = chosen_[p] >= 0 ? chosen_[p] : hoverIdx_[p];
    if (idx < 0)
    {
        const int next = chosen_[0] < 0 ? 0 : (chosen_[1] < 0 ? 1 : -1);
        if (next == p)
            idx = hoveredCard_(mousePos_);
    }

    const int cx = r.x + r.width / 2;
    const int imgTop = tag.br().y + int(10 * k);
    const int base = imgTop + int(r.height * 0.50); // linha do "chao" do pedestal
    const int pedW = r.width * 36 / 100, pedH = std::max(6, int(10 * k));

    if (idx < 0)
    {
        cv::ellipse(frame, {cx, base}, {pedW, pedH}, 0, 0, 360, pc * 0.25, cv::FILLED, cv::LINE_AA);
        cv::ellipse(frame, {cx, base}, {pedW, pedH}, 0, 0, 360, pc * 0.6, 1, cv::LINE_AA);
        const bool blink = int(titleTime_ * 2.f) % 2 == 0;
        putCenteredText(frame, "?", {r.x, imgTop, r.width, base - imgTop}, FD, 3.0 * k, blink ? pc : pc * 0.6, 4);
        ui::putWrapped(frame, "Aponte a varinha para um mago e desenhe sobre o retrato",
                       {r.x + 16, base + int(40 * k)}, r.width - 32, F, 0.45 * k, soft, 1, int(6 * k));
        return;
    }

    const CharacterDef &c = chars_[idx];
    const SpecialKind sk = specialFor(c.id);
    const SpecialDef &sd = specialDef(sk);
    const cv::Scalar sc = specialColor(sk);

    // pedestal + mago (respiracao em 2 quadros, como na luta)
    cv::ellipse(frame, {cx, base}, {pedW, pedH}, 0, 0, 360, sc * 0.3, cv::FILLED, cv::LINE_AA);
    cv::ellipse(frame, {cx, base}, {pedW, pedH}, 0, 0, 360, sc, 2, cv::LINE_AA);
    const cv::Mat &img = c.preview[p];
    if (!img.empty())
    {
        const int bob = (int(titleTime_ * 3.f) % 2) ? int(2 * k) : 0;
        drawSprite(frame, img, {cx, base - img.rows / 2 + pedH / 2 - bob});
    }
    else if (!c.cardImg.empty())
    {
        const cv::Rect pr(cx - (base - imgTop) * 3 / 8, imgTop, (base - imgTop) * 3 / 4, base - imgTop);
        const cv::Mat face = coverCrop(c.portrait, pr.size(), 0.3f);
        if (!face.empty() && (pr & cv::Rect(0, 0, frame.cols, frame.rows)) == pr)
            face.copyTo(frame(pr));
    }

    // nome, epiteto e especial
    int y = base + pedH + int(30 * k);
    const int textW = r.width - 28;
    const double ns = ui::fitScale(c.name, FD, 0.75 * k, 2, textW);
    const cv::Size nsz = pfx::textSize(c.name, FD, ns, 2);
    pfx::outlinedText(frame, c.name, {cx - nsz.width / 2, y}, FD, ns, white, 2);
    y += int(20 * k);
    cv::line(frame, {r.x + 18, y}, {r.br().x - 18, y}, pc * 0.5, 1);
    y += int(26 * k);

    ui::drawStar(frame, {r.x + 24, y - int(6 * k)}, std::max(6, int(9 * k)), sc, true);
    const std::string sname = sd.name;
    cv::putText(frame, sname, {r.x + 40, y}, FD, ui::fitScale(sname, FD, 0.55 * k, 1, r.width - 56), sc, 1,
                cv::LINE_AA);
    y += int(24 * k);
    y = ui::putWrapped(frame, sd.effect, {r.x + 18, y}, textW, F, 0.44 * k, soft, 1, int(5 * k));

    // estado da escolha
    const cv::Rect st(r.x + 14, r.br().y - int(44 * k), r.width - 28, int(32 * k));
    if (chosen_[p] == idx)
    {
        const bool blink = int(titleTime_ * 3.f) % 2 == 0;
        ui::fillTranslucent(frame, st, pc * 0.5, 0.9);
        cv::rectangle(frame, st, blink ? white : pc, 2);
        putCenteredText(frame, "PRONTO!", st, FD, 0.65 * k, white, 2);
    }
    else
    {
        const float prog = std::clamp(c.drawProgress[p], 0.f, 1.f);
        cv::rectangle(frame, st, pc * 0.6, 1);
        if (prog > 0.f)
            cv::rectangle(frame, cv::Rect(st.x + 2, st.y + 2, cvRound((st.width - 4) * prog), st.height - 4), pc * 0.7,
                          cv::FILLED);
        putCenteredText(frame, prog > 0.f ? "Desenhando..." : "Desenhe para escolher", st, F, 0.45 * k, white, 1);
    }
}

void MenuUI::drawCharacterSelect_(cv::Mat &frame) const
{
    const int W = frame.cols, H = frame.rows;
    const int F = cv::FONT_HERSHEY_SIMPLEX, FD = cv::FONT_HERSHEY_DUPLEX;
    const double k = std::clamp(H / 720.0, 0.6, 1.6);
    const cv::Scalar white(245, 240, 250), soft(200, 185, 205), gold(90, 210, 250);
    const cv::Scalar lavender(230, 170, 200), hotPurple(255, 210, 235);

    ui::fillTranslucent(frame, {0, 0, W, H}, cv::Scalar(8, 5, 12), 0.35);

    // --- cabecalho ---
    ui::fillTranslucent(frame, selectHeader_, cv::Scalar(22, 12, 30), 0.85);
    cv::line(frame, {0, selectHeader_.br().y}, {W, selectHeader_.br().y}, gold * 0.8, 2);
    {
        const std::string title = "ESCOLHA SEU MAGO";
        const double ts = ui::fitScale(title, FD, 1.2 * k, 2, W * 60 / 100);
        const cv::Size tsz = pfx::textSize(title, FD, ts, 2);
        const int ty = selectHeader_.y + selectHeader_.height * 55 / 100;
        pfx::outlinedText(frame, title, {(W - tsz.width) / 2, ty}, FD, ts, gold, 2);
        putCenteredText(frame, "Desenhe sobre um retrato com a varinha (ou clique) para escolher",
                        {0, ty + 4, W, selectHeader_.br().y - ty - 4}, F, 0.45 * k, soft, 1);
    }

    // --- cards ---
    const int mouseCard = hoveredCard_(mousePos_);
    const int mouseFor = chosen_[0] < 0 ? 0 : (chosen_[1] < 0 ? 1 : -1);
    const bool alt = int(titleTime_ * 4.f) % 2 == 0;

    for (size_t ii = 0; ii < chars_.size(); ++ii)
    {
        const int i = static_cast<int>(ii);
        const CharacterDef &c = chars_[ii];
        const cv::Rect &r = c.cardRect;
        bool on[2], sel[2];
        for (int p = 0; p < 2; ++p)
        {
            sel[p] = chosen_[p] == i;
            on[p] = sel[p] || (chosen_[p] < 0 && (hoverIdx_[p] == i || (mouseFor == p && mouseCard == i)));
        }

        float hv = std::max(c.drawProgress[0], c.drawProgress[1]);
        if (on[0] || on[1])
            hv = 1.f;
        if (glowEnabled_)
            addRectGlow(frame, r, std::clamp(hv, 0.f, 1.f), titleTime_);

        cv::Scalar border = lavender * 0.8;
        if (on[0] && on[1])
            border = alt ? ui::playerColor(0) : ui::playerColor(1);
        else if (on[0])
            border = ui::playerColor(0);
        else if (on[1])
            border = ui::playerColor(1);
        ui::panel(frame, r, border, 1.0, cv::Scalar(20, 14, 26), (sel[0] || sel[1]) ? 5 : 3);

        // retrato
        const int inset = 5;
        const cv::Rect ir(r.x + inset, r.y + inset, r.width - 2 * inset, r.height - 2 * inset);
        if (!c.cardImg.empty() && c.cardImg.size() == ir.size() && c.cardImg.type() == frame.type() &&
            (ir & cv::Rect(0, 0, W, H)) == ir)
        {
            c.cardImg.copyTo(frame(ir));
            if (!on[0] && !on[1] && (chosen_[0] >= 0 || chosen_[1] >= 0))
                ui::fillTranslucent(frame, ir, cv::Scalar(0, 0, 0), 0.25); // destaca quem foi escolhido
        }

        // tracos da varinha sobre o retrato
        if (!c.drawLayer.empty() && c.drawLayer.size() == r.size() && (r & cv::Rect(0, 0, W, H)) == r)
        {
            cv::Mat gray, mask;
            cv::cvtColor(c.drawLayer, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, mask, 1, 255, cv::THRESH_BINARY);
            c.drawLayer.copyTo(frame(r), mask);
        }

        // placa com nome e epiteto
        const int plateH = std::max(int(40 * k), r.height * 22 / 100);
        const cv::Rect plate(ir.x, ir.br().y - plateH, ir.width, plateH);
        ui::fillTranslucent(frame, plate, cv::Scalar(12, 8, 18), 0.78);
        const double ns = ui::fitScale(c.name, FD, 0.6 * k, 1, plate.width - 12);
        putCenteredText(frame, c.name, plate, FD, ns, white, 1);

        // progresso do desenho de cada jogador (P1 laranja, P2 verde)
        for (int p = 0; p < 2; ++p)
            if (c.drawProgress[p] > 0.f && !sel[p])
                cv::rectangle(frame, cv::Rect(plate.x + 4, plate.y - 6 - p * 6,
                                              cvRound((plate.width - 8) * std::min(1.f, c.drawProgress[p])), 4),
                              ui::playerColor(p), cv::FILLED);

        // etiquetas P1/P2 (cheia = escolhido, piscando = apontando)
        for (int p = 0; p < 2; ++p)
        {
            if (!on[p] || (!sel[p] && !alt))
                continue;
            const std::string tagTxt = p == 0 ? "P1" : "P2";
            const cv::Size tz = pfx::textSize(tagTxt, FD, 0.55 * k, 1);
            const cv::Rect tagR(p == 0 ? ir.x + 4 : ir.br().x - tz.width - 16, ir.y + 4, tz.width + 12, tz.height + 12);
            cv::rectangle(frame, tagR, sel[p] ? ui::playerColor(p) : ui::playerColor(p) * 0.5, cv::FILLED);
            cv::rectangle(frame, tagR, white, 1);
            putCenteredText(frame, tagTxt, tagR, FD, 0.55 * k, white, 1);
        }
    }

    // --- paineis de destaque ---
    drawCharacterPreview_(frame, 0);
    drawCharacterPreview_(frame, 1);

    // --- rodape ---
    ui::fillTranslucent(frame, selectFooter_, cv::Scalar(22, 12, 30), 0.85);
    cv::line(frame, {0, selectFooter_.y}, {W, selectFooter_.y}, gold * 0.5, 1);
    putCenteredText(frame, "P1 = laranja    |    P2 = verde    |    R: refazer escolha    |    M: menu", selectFooter_,
                    F, 0.45 * k, soft, 1);

    if (screen_ == GameScreen::DuelReady)
    {
        const cv::Rect banner(W / 4, H / 2 - int(40 * k), W / 2, int(80 * k));
        ui::panel(frame, banner, hotPurple, 0.92);
        putCenteredText(frame, characterName(chosen_[0]) + "  VS  " + characterName(chosen_[1]), banner, FD, 0.8 * k,
                        white, 2);
    }
}
