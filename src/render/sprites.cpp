#include "render/sprites.h"
#include "game/spells.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
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

    if (s.channels() == 4)
    {
        // Extrai canal alfa como mascara e usa copyTo acelerado pelo OpenCV
        std::vector<cv::Mat> ch;
        cv::split(s, ch);
        cv::Mat alpha = ch[3];
        cv::Mat bgr;
        cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, bgr);

        // Blending alpha: destino = src*a + dst*(1-a)
        // Feito em float para precisao, mas so nos pixels nao-transparentes
        cv::Mat alphaf, invAlphaf;
        alpha.convertTo(alphaf, CV_32F, 1.0 / 255.0);
        invAlphaf = 1.0f - alphaf;  // escalar broadcast

        cv::Mat srcf, dstf;
        bgr.convertTo(srcf, CV_32FC3);
        d.convertTo(dstf, CV_32FC3);

        // Multiplica canal a canal
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
        // Sem alfa: copia diretamente
        s.copyTo(d);
    }
}

std::vector<cv::Mat> loadStrip(const std::string &path, int frameW, int height)
{
    std::vector<cv::Mat> frames;
    cv::Mat sheet = loadSprite(path); // mantém o alfa, sem redimensionar
    if (sheet.empty())
        return frames;

    const int n = sheet.cols / frameW;
    const double s = double(height) / sheet.rows;
    for (int i = 0; i < n; ++i)
    {
        cv::Mat f = sheet(cv::Rect(i * frameW, 0, frameW, sheet.rows)).clone();
        cv::resize(f, f, {}, s, s, cv::INTER_NEAREST); // mantém o pixel art nítido
        frames.push_back(f);
    }
    return frames;
}

// ------------------------------------------------------------
// Carrega wizard: tenta spritesheet (largura = kWizardFrames * h),
// senao carrega como imagem unica. Retorna false se falhou tudo.
// out[0] = frames normais, out[1] = frames espelhados (oponente).
// ------------------------------------------------------------
bool loadWizardFrames(const std::string &path, std::vector<cv::Mat> out[2])
{
    out[0].clear();
    out[1].clear();

    cv::Mat sheet = loadSprite(path);
    if (sheet.empty())
        return false;

    // Detecta se é spritesheet: largura >= kWizardFrames * height/2
    // (heuristica: se tem muitos frames horizontais)
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
        // Imagem unica: usa o mesmo frame para todos os kWizardFrames
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
            // sem alfa: considera tudo opaco
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

// ------------------------------------------------------------
// Tenta carregar um sprite de feitico especifico do personagem;
// se nao existir, usa o fallback global (ja carregado em bolt/boltAnim).
// ------------------------------------------------------------
static cv::Mat tryLoadSpell(const std::string &dir,
                             const std::string &spellName,
                             const std::string &charId,
                             const cv::Mat &fallback,
                             int height)
{
    if (!charId.empty())
    {
        // ex: assets/sprites/spells/escudo_derzu.png
        std::string specific = dir + "/sprites/spells/" + spellName + "_" + charId + ".png";
        cv::Mat m = loadSprite(specific, height);
        if (!m.empty())
        {
            std::fprintf(stdout, "Carregou feitico especifico: %s\n", specific.c_str());
            return m;
        }
    }
    return fallback; // fallback global
}

static std::vector<cv::Mat> tryLoadSpellAnim(const std::string &dir,
                                              const std::string &spellName,
                                              const std::string &charId,
                                              const std::vector<cv::Mat> &fallback,
                                              int frameW, int height)
{
    if (!charId.empty())
    {
        std::string specific = dir + "/sprites/spells/" + spellName + "_" + charId + ".png";
        auto frames = loadStrip(specific, frameW, height);
        if (!frames.empty())
        {
            std::fprintf(stdout, "Carregou animacao especifica: %s\n", specific.c_str());
            return frames;
        }
    }
    return fallback;
}

bool Assets::load(const std::string &dir,
                  const std::string &p1PlayerFile,
                  const std::string &p2PlayerFile,
                  const std::string &p1Id,
                  const std::string &p2Id)
{
    // --- Wizards ---
    // P1 (lado esquerdo = wizard[0], espelhado = wizard[1])
    {
        std::vector<cv::Mat> sides[2];
        if (loadWizardFrames(dir + "/sprites/players/" + p1PlayerFile, sides))
        {
            wizard[0] = sides[0]; // frames normais para o lado esquerdo
            // wizard[1] sera sobrescrito por P2; se P2 falhar, usa o espelhado de P1
            if (wizard[1].empty())
                wizard[1] = sides[1];
        }
    }

    // P2 (lado direito = wizard[1] com sprite espelhado)
    {
        std::vector<cv::Mat> sides[2];
        if (loadWizardFrames(dir + "/sprites/players/" + p2PlayerFile, sides))
        {
            wizard[1] = sides[1]; // frames espelhados para o lado direito
        }
    }

    // --- Escudo ---
    shield = loadSprite(dir + "/sprites/spells/shield.png", 200);
    if (shield.empty())
        shield = loadSprite(dir + "/shield.png", 200);

    // --- Feiticos globais (fallback) ---
    bolt[int(SpellType::AtaqueFraco)]    = loadSprite(dir + "/fireball1.png", 120);
    boltAnim[int(SpellType::AtaqueMedio)]   = loadStrip(dir + "/orb_projectile_32x32_blue.png", 32, 100);
    boltAnim[int(SpellType::AtaqueEspecial)] = loadStrip(dir + "/orb_projectile_32x32_red.png", 32, 180);
    bolt[int(SpellType::AtaqueForte)]    = loadSprite(dir + "/rock.png", 70);

    // --- Feiticos especificos por personagem (com fallback para o global) ---
    // Para simplificar, usa os do P1 (poderia ser expandido para usar por turno)
    // Nomes de arquivo esperados: escudo_<id>.png, especial_<id>.png
    // (use o id do personagem passado; se vazio, nao tenta especifico)
    const std::string mainId = p1Id; // feiticos visuais do P1 por padrao

    bolt[int(SpellType::Escudo)] =
        tryLoadSpell(dir, "escudo", mainId,
                     bolt[int(SpellType::Escudo)], 200);

    bolt[int(SpellType::AtaqueEspecial)] =
        tryLoadSpell(dir, "especial", mainId,
                     bolt[int(SpellType::AtaqueEspecial)], 180);

    // Escudo especifico por personagem tambem pode ser animacao
    if (!mainId.empty())
    {
        std::string shieldSpec = dir + "/sprites/spells/escudo_" + mainId + ".png";
        cv::Mat sc = loadSprite(shieldSpec, 200);
        if (!sc.empty()) shield = sc;
    }

    // --- Background ---
    cv::Mat bg = cv::imread(dir + "/sprites/ui/background.png");
    if (!bg.empty())
        cv::resize(bg, background, {1280, 720});

    // --- Pre-flipa sprites estaticos e animados para uso do P1 (owner==1) ---
    for (int t = 0; t < 6; ++t)
    {
        if (!bolt[t].empty())
        {
            cv::flip(bolt[t], boltFlipped[t], 1);
        }
        boltAnimFlipped[t].resize(boltAnim[t].size());
        for (size_t f = 0; f < boltAnim[t].size(); ++f)
            cv::flip(boltAnim[t][f], boltAnimFlipped[t][f], 1);
    }

    return !wizard[0].empty(); // o minimo para jogar
}