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
    cv::Mat s = spr(cv::Rect(clip.x - want.x, clip.y - want.y, clip.width, clip.height));

    for (int y = 0; y < d.rows; ++y)
        for (int x = 0; x < d.cols; ++x)
        {
            const cv::Vec4b &p = s.at<cv::Vec4b>(y, x);
            if (p[3] == 0)
                continue;
            float a = p[3] / 255.f;
            cv::Vec3b &q = d.at<cv::Vec3b>(y, x);
            for (int c = 0; c < 3; ++c)
                q[c] = static_cast<uchar>(p[c] * a + q[c] * (1.f - a));
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

bool Assets::load(const std::string &dir)
{
    cv::Mat sheet = loadSprite(dir + "/Idle.png"); // sem redimensionar aqui
    if (!sheet.empty())
    {
        const int fw = sheet.cols / kWizardFrames;

        // recorta os frames
        std::vector<cv::Mat> raw;
        for (int i = 0; i < kWizardFrames; ++i)
            raw.push_back(sheet(cv::Rect(i * fw, 0, fw, sheet.rows)).clone());

        // caixa que contém o mago em TODOS os frames (mesmo recorte para todos,
        // senão o mago "pula" entre quadros)
        cv::Mat alphaMax = cv::Mat::zeros(sheet.rows, fw, CV_8U);
        for (const cv::Mat &f : raw)
        {
            std::vector<cv::Mat> ch;
            cv::split(f, ch);
            cv::max(alphaMax, ch[3], alphaMax);
        }
        cv::Rect bb = cv::boundingRect(alphaMax > 0);
        if (bb.empty())
            bb = cv::Rect(0, 0, fw, sheet.rows);

        const double s = 160.0 / bb.height; // altura final do mago em pixels
        for (const cv::Mat &f : raw)
        {
            cv::Mat c;
            cv::resize(f(bb), c, {}, s, s, cv::INTER_NEAREST); // NEAREST mantém o pixel art nítido
            wizard[0].push_back(c);

            cv::Mat flipped;
            cv::flip(c, flipped, 1); // espelha cada frame
            wizard[1].push_back(flipped);
        }
    }

    shield = loadSprite(dir + "/shield.png", 200);

    // nomes de exemplo; ajuste para os seus arquivos
    bolt[int(SpellType::AtaqueFraco)] = loadSprite(dir + "/fireball1.png", 120); // azul (lightning) e vermelho (magicblast): 48 quadros de 32x32
    boltAnim[int(SpellType::AtaqueMedio)] = loadStrip(dir + "/orb_projectile_32x32_blue.png", 32, 100);
    boltAnim[int(SpellType::AtaqueEspecial)] = loadStrip(dir + "/orb_projectile_32x32_red.png", 32, 180);
    bolt[int(SpellType::AtaqueForte)] = loadSprite(dir + "/rock.png", 70);

    cv::Mat bg = cv::imread(dir + "/background.png");
    if (!bg.empty())
        cv::resize(bg, background, {1280, 720});

    return !wizard[0].empty(); // o minimo para jogar
}