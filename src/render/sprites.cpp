#include "render/sprites.h"
#include "game/spells.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <algorithm>
#include <cstdio>

cv::Mat loadSprite(const std::string &path, int height)
{
    cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED); // UNCHANGED preserva o alfa
    if (img.empty())
    {
        std::fprintf(stderr, "Aviso: nao carregou %s\n", path.c_str());
        return {};
    }
    if (img.channels() == 1)
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGRA);
    else if (img.channels() == 3)
        cv::cvtColor(img, img, cv::COLOR_BGR2BGRA); // sem transparencia

    if (height > 0 && img.rows != height)
    {
        double s = double(height) / img.rows;
        cv::resize(img, img, {}, s, s, cv::INTER_AREA);
    }
    return img;
}

void drawSprite(cv::Mat &dst, const cv::Mat &spr, cv::Point center)
{
    if (spr.empty())
        return;

    cv::Rect want(center.x - spr.cols / 2, center.y - spr.rows / 2, spr.cols, spr.rows);
    cv::Rect clip = want & cv::Rect(0, 0, dst.cols, dst.rows);
    if (clip.empty())
        return;

    cv::Mat d = dst(clip);
    cv::Rect srcRoi(clip.x - want.x, clip.y - want.y, clip.width, clip.height);
    cv::Mat s = spr(srcRoi);

    // Caminho rapido: BGRA -> BGR em 8 bits, com blend inteiro, sem converter para float.
    if (s.type() == CV_8UC4 && d.type() == CV_8UC3)
    {
        for (int y = 0; y < s.rows; ++y)
        {
            const uchar *sp = s.ptr<uchar>(y);
            uchar *dp = d.ptr<uchar>(y);
            for (int x = 0; x < s.cols; ++x, sp += 4, dp += 3)
            {
                const int a = sp[3];
                if (a == 0)
                    continue;
                if (a == 255)
                {
                    dp[0] = sp[0];
                    dp[1] = sp[1];
                    dp[2] = sp[2];
                }
                else
                {
                    dp[0] = uchar(dp[0] + ((sp[0] - dp[0]) * a) / 255);
                    dp[1] = uchar(dp[1] + ((sp[1] - dp[1]) * a) / 255);
                    dp[2] = uchar(dp[2] + ((sp[2] - dp[2]) * a) / 255);
                }
            }
        }
        return;
    }

    if (s.channels() == 4)
    {
        std::vector<cv::Mat> ch;
        cv::split(s, ch);
        cv::Mat alpha = ch[3];
        cv::Mat bgr;
        cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, bgr);

        // Blending alpha: destino = src*a + dst*(1-a)
        cv::Mat alphaf, invAlphaf;
        alpha.convertTo(alphaf, CV_32F, 1.0 / 255.0);
        invAlphaf = 1.0f - alphaf;

        cv::Mat srcf, dstf;
        bgr.convertTo(srcf, CV_32FC3);
        d.convertTo(dstf, CV_32FC3);

        cv::Mat result = cv::Mat::zeros(d.size(), CV_32FC3);
        for (int c = 0; c < 3; ++c)
        {
            cv::Mat sc, dc, rc;
            cv::extractChannel(srcf, sc, c);
            cv::extractChannel(dstf, dc, c);
            rc = sc.mul(alphaf) + dc.mul(invAlphaf);
            cv::insertChannel(rc, result, c);
        }
        result.convertTo(d, CV_8UC3);
    }
    else
    {
        s.copyTo(d);
    }
}

// ------------------------------------------------------------
// Helpers de spritesheet / imagem unica
// ------------------------------------------------------------

// Fatia um spritesheet horizontal em quadros de largura frameW.
static std::vector<cv::Mat> sliceStrip(const cv::Mat &sheet, int frameW, int height)
{
    std::vector<cv::Mat> frames;
    const int n = sheet.cols / frameW;
    const double s = double(height) / sheet.rows;
    for (int i = 0; i < n; ++i)
    {
        cv::Mat f = sheet(cv::Rect(i * frameW, 0, frameW, sheet.rows)).clone();
        cv::resize(f, f, {}, s, s, cv::INTER_NEAREST); // mantem o pixel art nitido
        frames.push_back(f);
    }
    return frames;
}

// So considera spritesheet se a largura for multiplo exato de frameW
// e a altura for compativel com quadros de frameW (ex.: 32x32).
// frameW <= 0 => nunca e strip.
static bool looksLikeStrip(const cv::Mat &sheet, int frameW)
{
    return frameW > 0 &&
           sheet.cols >= 2 * frameW &&
           sheet.cols % frameW == 0 &&
           sheet.rows <= 2 * frameW;
}

// Corta a margem transparente e escala mantendo proporcao,
// respeitando altura maxima E largura maxima (sem distorcer).
static cv::Mat cropAndFit(const cv::Mat &src, int maxH, int maxW)
{
    cv::Mat out = src;
    if (src.channels() == 4)
    {
        cv::Mat alpha, mask;
        cv::extractChannel(src, alpha, 3);
        cv::threshold(alpha, mask, 8, 255, cv::THRESH_BINARY);
        cv::Rect bb = cv::boundingRect(mask);
        if (!bb.empty())
            out = src(bb);
    }
    const double s = std::min(double(maxH) / out.rows, double(maxW) / out.cols);
    cv::Mat r;
    cv::resize(out, r, {}, s, s, s < 1.0 ? cv::INTER_AREA : cv::INTER_NEAREST);
    return r;
}

// Declarada em sprites.h. Retorna vazio se a imagem nao for spritesheet.
std::vector<cv::Mat> loadStrip(const std::string &path, int frameW, int height)
{
    cv::Mat sheet = loadSprite(path);
    if (sheet.empty() || !looksLikeStrip(sheet, frameW))
        return {};
    return sliceStrip(sheet, frameW, height);
}

// ------------------------------------------------------------
// Carrega wizard: spritesheet (kWizardFrames) ou imagem unica.
// out[0] = frames normais, out[1] = frames espelhados (oponente).
// ------------------------------------------------------------
bool loadWizardFrames(const std::string &path, std::vector<cv::Mat> out[2])
{
    out[0].clear();
    out[1].clear();

    cv::Mat sheet = loadSprite(path);
    if (sheet.empty())
        return false;

    const bool isStrip = (sheet.cols >= sheet.rows * 2);

    std::vector<cv::Mat> raw;
    if (isStrip)
    {
        const int fw = sheet.cols / kWizardFrames;
        for (int i = 0; i < kWizardFrames; ++i)
            raw.push_back(sheet(cv::Rect(i * fw, 0, fw, sheet.rows)).clone());
    }
    else
    {
        for (int i = 0; i < kWizardFrames; ++i)
            raw.push_back(sheet.clone());
    }

    // Bounding box unificado (para nao ter "pulo" entre frames)
    const int fw0 = raw[0].cols;
    cv::Mat alphaMax = cv::Mat::zeros(raw[0].rows, fw0, CV_8U);
    for (const cv::Mat &f : raw)
    {
        if (f.channels() == 4)
        {
            std::vector<cv::Mat> ch;
            cv::split(f, ch);
            cv::max(alphaMax, ch[3], alphaMax);
        }
        else
        {
            alphaMax.setTo(255);
        }
    }
    cv::Rect bb = cv::boundingRect(alphaMax > 0);
    if (bb.empty())
        bb = cv::Rect(0, 0, fw0, raw[0].rows);

    const double s = 160.0 / bb.height; // altura final do mago em pixels
    for (const cv::Mat &f : raw)
    {
        cv::Mat c;
        cv::resize(f(bb), c, {}, s, s, cv::INTER_NEAREST);
        out[0].push_back(c);

        cv::Mat flipped;
        cv::flip(c, flipped, 1);
        out[1].push_back(flipped);
    }
    return true;
}

// Cor dominante (BGR) dos pixels opacos e saturados do sprite, clareada.
static cv::Scalar computeTrailColor(const cv::Mat &m, const cv::Scalar &fallback)
{
    if (m.empty() || m.type() != CV_8UC4)
        return fallback;

    double sb = 0, sg = 0, sr = 0, n = 0;
    for (int y = 0; y < m.rows; ++y)
        for (int x = 0; x < m.cols; ++x)
        {
            const cv::Vec4b &px = m.at<cv::Vec4b>(y, x);
            if (px[3] < 128)
                continue;
            int mx = std::max({px[0], px[1], px[2]});
            int mn = std::min({px[0], px[1], px[2]});
            if (mx - mn < 60) // ignora branco/cinza
                continue;
            sb += px[0];
            sg += px[1];
            sr += px[2];
            n += 1;
        }
    if (n < 1)
        return fallback;

    sb /= n;
    sg /= n;
    sr /= n;
    const double mx = std::max({sb, sg, sr});
    const double k = mx > 0 ? 255.0 / mx : 1.0;
    return cv::Scalar(sb * k, sg * k, sr * k);
}

// ------------------------------------------------------------
// Barras de vida / mana: PNG com a moldura e o preenchimento lado a lado
// ------------------------------------------------------------

// Posicao do "slot" (miolo escuro da moldura) como fracao da moldura recortada
// (medido no asset original: moldura 353x118, slot em x=80, y=45, 200x30).
static constexpr float kSlotX = 80.f / 353.f;
static constexpr float kSlotY = 45.f / 118.f;
static constexpr float kSlotW = 200.f / 353.f;
static constexpr float kSlotH = 30.f / 118.f;
// Contorno escuro do preenchimento (px no asset original) descartado, pois a moldura ja tem borda.
static constexpr int kFillOutline = 4;

bool loadBarSkin(const std::string &path, int frameWidth, BarSkin &out)
{
    out = BarSkin();
    cv::Mat img = loadSprite(path);
    if (img.empty() || img.channels() != 4 || frameWidth <= 0)
        return false;

    cv::Mat alpha;
    cv::extractChannel(img, alpha, 3);

    // Acha a maior lacuna horizontal vazia: separa moldura (esq.) do preenchimento (dir.)
    cv::Mat colMax;
    cv::reduce(alpha > 8, colMax, 0, cv::REDUCE_MAX, CV_8U);
    int first = -1, last = -1, bestGap = 0, gapStart = -1;
    for (int x = 0, prev = -1; x < colMax.cols; ++x)
    {
        if (!colMax.at<uchar>(0, x))
            continue;
        if (first < 0)
            first = x;
        if (prev >= 0 && x - prev - 1 > bestGap)
        {
            bestGap = x - prev - 1;
            gapStart = prev;
        }
        prev = x;
        last = x;
    }
    if (first < 0 || bestGap < 2)
    {
        std::fprintf(stderr, "Aviso: %s nao tem moldura + preenchimento\n", path.c_str());
        return false;
    }

    auto cropPart = [&](int x0, int x1)
    {
        cv::Rect strip(x0, 0, x1 - x0 + 1, img.rows);
        cv::Mat mask = alpha(strip) > 8;
        cv::Rect bb = cv::boundingRect(mask);
        return img(cv::Rect(strip.x + bb.x, bb.y, bb.width, bb.height)).clone();
    };
    cv::Mat frame = cropPart(first, gapStart);
    cv::Mat fill = cropPart(gapStart + bestGap + 1, last);
    if (fill.cols > 2 * kFillOutline + 4 && fill.rows > 2 * kFillOutline + 4)
        fill = fill(cv::Rect(kFillOutline, kFillOutline,
                             fill.cols - 2 * kFillOutline,
                             fill.rows - 2 * kFillOutline))
                   .clone();

    // Escala a moldura para a largura pedida (pixel art: vizinho mais proximo)
    const double s = double(frameWidth) / frame.cols;
    cv::resize(frame, frame, {}, s, s, cv::INTER_NEAREST);

    cv::Rect slot(cvRound(kSlotX * frame.cols), cvRound(kSlotY * frame.rows),
                  cvRound(kSlotW * frame.cols), cvRound(kSlotH * frame.rows));
    cv::resize(fill, fill, slot.size(), 0, 0, cv::INTER_NEAREST);

    out.frame[0] = frame;
    out.fill[0] = fill;
    out.slot[0] = slot;

    // Lado P2: tudo espelhado (o preenchimento encolhe da esquerda para a direita)
    cv::flip(frame, out.frame[1], 1);
    cv::flip(fill, out.fill[1], 1);
    out.slot[1] = cv::Rect(frame.cols - slot.x - slot.width, slot.y, slot.width, slot.height);
    return true;
}

bool Assets::load(const std::string &dir,
                  const std::string &p1PlayerFile,
                  const std::string &p2PlayerFile,
                  const std::string &p1Id,
                  const std::string &p2Id)
{
    // --- Limpa estado antigo (ao trocar de personagem) ---
    wizard[0].clear();
    wizard[1].clear();
    for (int p = 0; p < 2; ++p)
    {
        shield[p] = cv::Mat();
        for (int i = 0; i < 6; ++i)
        {
            bolt[p][i] = cv::Mat();
            boltFlipped[p][i] = cv::Mat();
            boltAnim[p][i].clear();
            boltAnimFlipped[p][i].clear();
        }
    }

    // --- Wizards ---
    {
        std::vector<cv::Mat> sides1[2];
        if (loadWizardFrames(dir + "/sprites/players/" + p1PlayerFile, sides1))
        {
            wizard[0] = sides1[0];
            wizard[1] = sides1[1]; // sera sobrescrito por P2 se carregar
        }
    }
    {
        std::vector<cv::Mat> sides2[2];
        if (loadWizardFrames(dir + "/sprites/players/" + p2PlayerFile, sides2))
            wizard[1] = sides2[1]; // espelhado para o lado direito
    }

    // --- Escudo ---
    cv::Mat defaultShield = loadSprite(dir + "/sprites/spells/shield.png", 200);
    if (defaultShield.empty())
        defaultShield = loadSprite(dir + "/shield.png", 200);

    shield[0] = defaultShield.clone();
    if (!p1Id.empty())
    {
        cv::Mat sc = loadSprite(dir + "/sprites/spells/escudo_" + p1Id + ".png", 200);
        if (!sc.empty())
            shield[0] = sc;
    }
    shield[1] = defaultShield.clone();
    if (!p2Id.empty())
    {
        cv::Mat sc = loadSprite(dir + "/sprites/spells/escudo_" + p2Id + ".png", 200);
        if (!sc.empty())
            shield[1] = sc;
    }

    // --- Feiticos globais (fallback) ---
    cv::Mat defFraco = loadSprite(dir + "/fireball1.png", 120);
    std::vector<cv::Mat> defMedio = loadStrip(dir + "/orb_projectile_32x32_blue.png", 32, 100);
    std::vector<cv::Mat> defEspecial = loadStrip(dir + "/orb_projectile_32x32_red.png", 32, 180);
    cv::Mat defForte = loadSprite(dir + "/rock.png", 70);

    auto flipAll = [](const std::vector<cv::Mat> &in)
    {
        std::vector<cv::Mat> out(in.size());
        for (size_t i = 0; i < in.size(); ++i)
            cv::flip(in[i], out[i], 1);
        return out;
    };
    const std::vector<cv::Mat> defMedioF = flipAll(defMedio);
    const std::vector<cv::Mat> defEspecialF = flipAll(defEspecial);

    // --- Feiticos por personagem ---
    // IMPORTANTE: o slot e o valor do enum SpellType, porque o HUD
    // acessa bolt[owner][int(pr.type)].
    const std::string spells = dir + "/sprites/spells/";
    for (int p = 0; p < 2; ++p)
    {
        std::string id = (p == 0) ? p1Id : p2Id;
        if (id.empty())
            id = "padrao";

        auto loadSlot = [&](SpellType type, const std::string &file, int frameW, int h, int maxW,
                            const cv::Mat &defStill,
                            const std::vector<cv::Mat> &defAnim,
                            const std::vector<cv::Mat> &defAnimF)
        {
            const int slot = static_cast<int>(type);
            cv::Mat sheet = loadSprite(spells + file); // sem redimensionar
            if (!sheet.empty())
            {
                if (looksLikeStrip(sheet, frameW))
                {
                    boltAnim[p][slot] = sliceStrip(sheet, frameW, h);
                    boltAnimFlipped[p][slot] = flipAll(boltAnim[p][slot]);
                    std::fprintf(stdout, "OK %s (animado, %zu quadros)\n",
                                 file.c_str(), boltAnim[p][slot].size());
                }
                else
                {
                    bolt[p][slot] = cropAndFit(sheet, h, maxW);
                    cv::flip(bolt[p][slot], boltFlipped[p][slot], 1);
                    std::fprintf(stdout, "OK %s (estatico %dx%d)\n", file.c_str(),
                                 bolt[p][slot].cols, bolt[p][slot].rows);
                }
                return;
            }
            // arquivo nao existe -> fallback global
            if (!defAnim.empty())
            {
                boltAnim[p][slot] = defAnim;
                boltAnimFlipped[p][slot] = defAnimF;
            }
            else if (!defStill.empty())
            {
                bolt[p][slot] = defStill;
                cv::flip(defStill, boltFlipped[p][slot], 1);
            }
        };

        const std::vector<cv::Mat> none;
        loadSlot(SpellType::AtaqueFraco, "projectile_" + id + ".png", 0, 120, 180, defFraco, none, none);
        loadSlot(SpellType::AtaqueMedio, "pierce_" + id + ".png", 32, 100, 220, cv::Mat(), defMedio, defMedioF);
        loadSlot(SpellType::AtaqueForte, "ray_" + id + ".png", 0, 150, 200, defForte, none, none);
        loadSlot(SpellType::AtaqueEspecial, "especial_" + id + ".png", 32, 180, 220, cv::Mat(), defEspecial, defEspecialF);
    }

    for (int p = 0; p < 2; ++p)
    {
        const int slot = static_cast<int>(SpellType::AtaqueMedio);
        const cv::Mat *src = nullptr;
        if (!bolt[p][slot].empty())
            src = &bolt[p][slot];
        else if (!boltAnim[p][slot].empty())
            src = &boltAnim[p][slot][0];

        trailColor[p] = src ? computeTrailColor(*src, trailColor[p]) : trailColor[p];
    }

    // --- Background ---
    cv::Mat bg = cv::imread(dir + "/sprites/ui/fundoBatalha.jpg");
    if (!bg.empty())
        cv::resize(bg, background, {1280, 720});

    // --- Barras de vida e mana (largura final da moldura: kBarFrameWidth) ---
    auto loadBar = [&](BarSkin &skin, const std::string &file)
    {
        if (!loadBarSkin(dir + "/sprites/ui/" + file, kBarFrameWidth, skin))
            loadBarSkin(dir + "/" + file, kBarFrameWidth, skin); // fallback: raiz de assets
    };
    healthBar = BarSkin();
    manaBar = BarSkin();
    loadBar(healthBar, "health_bar.png");
    loadBar(manaBar, "mana_bar.png");

    return !wizard[0].empty(); // o minimo para jogar
}