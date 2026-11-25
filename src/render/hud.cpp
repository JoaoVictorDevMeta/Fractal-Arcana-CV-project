#include "render/hud.h"
#include "render/sprites.h"
#include "render/fighter_anim.h"
#include "render/pixel_fx.h"
#include "render/special_fx.h"
#include "render/ui_widgets.h"
#include "game/match.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <cmath>
#include <string>
#include <vector>

// Reduz o sprite se ele for mais alto que maxH (em vez de cortar).
// O resultado fica em cache: o resize deixa de acontecer a cada frame.
static const cv::Mat &fitSprite(int maxH, const cv::Mat &spr)
{
    if (maxH <= 0 || spr.empty() || spr.rows <= maxH)
        return spr;

    // chave: endereco + tamanho + amostra de pixels (protege contra recarga de assets)
    long long sample = 0;
    const size_t bytes = spr.total() * spr.elemSize();
    if (spr.isContinuous() && bytes > 0)
        for (int i = 0; i < 16; ++i)
            sample = sample * 131 + spr.data[(bytes - 1) * size_t(i) / 15];
    const std::array<long long, 5> key = {(long long)(std::uintptr_t)spr.data,
                                          spr.rows, spr.cols, maxH, sample};

    static std::map<std::array<long long, 5>, cv::Mat> cache;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    if (cache.size() > 64)
        cache.clear();

    cv::Mat r;
    const double sc = double(maxH) / spr.rows;
    cv::resize(spr, r, {}, sc, sc, cv::INTER_AREA);
    return cache.emplace(key, std::move(r)).first->second;
}

// Cabe na altura da arena.
static const cv::Mat &fitSprite(const cv::Mat &view, const cv::Mat &spr)
{
    return fitSprite(view.rows - 4, spr);
}

static void drawFit(cv::Mat &view, const cv::Mat &spr, cv::Point center)
{
    drawSprite(view, fitSprite(view, spr), center);
}

static float rnd(int a, int b) // ruido deterministico em [-1, 1]
{
    unsigned h = unsigned(a) * 374761393u + unsigned(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFF) / 65535.f * 2.f - 1.f;
}

// Rastro de raio atras do projetil: head = ponta, dir = +1 (P1) ou -1 (P2).
static void drawLightningTrail(cv::Mat &view, cv::Point tail, cv::Point head,
                               const cv::Scalar &col, float time, int seed, bool glowEffects)
{
    const int dir = (head.x >= tail.x) ? 1 : -1;
    const int len = std::abs(head.x - tail.x);
    const int step = 10;
    const int n = len / step;
    if (n < 2)
        return;

    const int tick = int(time * 24); // o raio "tremula" 24x por segundo

    // pontos a partir do mago; zigue-zague maior no meio, zero nas pontas
    std::vector<cv::Point> pts;
    for (int j = 0; j <= n; ++j)
    {
        float f = float(j) / n; // 0 = mago, 1 = projetil
        float amp = 3.f + 16.f * std::sin(f * 3.14159f);
        int x = (j == n) ? head.x : tail.x + dir * j * step;
        int y = tail.y + int(rnd(j + seed * 97, tick) * amp);
        if (j == 0)
            y = tail.y;
        if (j == n)
            y = head.y;
        pts.push_back({x, y});
    }

    cv::Rect roi(std::min(tail.x, head.x) - 25, tail.y - 60, len + 50, 120);
    roi &= cv::Rect(0, 0, view.cols, view.rows);
    if (roi.empty())
        return;
    const cv::Point off = roi.tl();

    cv::Mat sub = view(roi);

    // brilho (camada aditiva desfocada) - pulado quando os brilhos estao desligados
    if (glowEffects)
    {
        cv::Mat glow = cv::Mat::zeros(roi.size(), view.type());
        for (int j = 0; j < n; ++j)
        {
            double b = 0.55 + 0.45 * double(j) / n; // mais forte perto do projetil
            cv::line(glow, pts[j] - off, pts[j + 1] - off, col * b, 9, cv::LINE_AA);
        }
        cv::GaussianBlur(glow, glow, {13, 13}, 0);
        cv::add(sub, glow, sub);
    }

    // nucleo + ramificacoes
    const cv::Scalar white(255, 255, 255);
    for (int j = 0; j < n; ++j)
    {
        double b = 0.55 + 0.45 * double(j) / n;
        cv::Scalar mid = col * b;
        cv::Scalar core = (col * 0.35 + white * 0.65) * b;
        cv::line(sub, pts[j] - off, pts[j + 1] - off, mid, 4, cv::LINE_AA);
        cv::line(sub, pts[j] - off, pts[j + 1] - off, core, 2, cv::LINE_AA);

        if (j > 0 && j < n - 1 && rnd(j * 13 + seed, tick + 7) > 0.5f)
        {
            int side = rnd(j, tick + 3) > 0 ? 1 : -1;
            cv::Point mid1 = pts[j] + cv::Point(dir * 10, side * 14);
            cv::Point end1 = mid1 + cv::Point(dir * 8, side * 12);
            cv::line(sub, pts[j] - off, mid1 - off, mid, 2, cv::LINE_AA);
            cv::line(sub, mid1 - off, end1 - off, mid, 1, cv::LINE_AA);
        }
    }
}

// Desenha um sprite com o canto superior esquerdo em 'tl'.
static void drawSpriteTL(cv::Mat &view, const cv::Mat &spr, cv::Point tl)
{
    drawSprite(view, spr, tl + cv::Point(spr.cols / 2, spr.rows / 2));
}

// Cor do brilho (BGR) conforme o feitico lancado.
static cv::Scalar castGlowColor(SpellType t)
{
    switch (t)
    {
    case SpellType::AtaqueFraco:
        return cv::Scalar(30, 120, 255); // laranja
    case SpellType::AtaqueMedio:
        return cv::Scalar(255, 220, 120); // azul claro
    case SpellType::AtaqueForte:
        return cv::Scalar(50, 90, 220); // vermelho
    case SpellType::Escudo:
        return cv::Scalar(255, 230, 100); // ciano
    case SpellType::AtaqueEspecial:
        return cv::Scalar(255, 80, 220); // magenta
    default:
        return cv::Scalar(255, 255, 255);
    }
}

// Mascara de brilho (branca, ja desfocada) em cache por raio. Evita um GaussianBlur
// grande a cada frame; a cor e a intensidade sao aplicadas depois com uma multiplicacao.
static const cv::Mat &glowMask(int radius, double circleFrac, double sigmaFrac)
{
    static std::map<std::array<int, 3>, cv::Mat> cache;
    const std::array<int, 3> key = {radius, int(circleFrac * 100), int(sigmaFrac * 100)};
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    if (cache.size() > 32)
        cache.clear();

    const int pad = radius * 2;
    cv::Mat m = cv::Mat::zeros(2 * pad, 2 * pad, CV_8UC3);
    cv::circle(m, {pad, pad}, cvRound(radius * circleFrac), cv::Scalar::all(255),
               cv::FILLED, cv::LINE_AA);
    cv::GaussianBlur(m, m, {0, 0}, radius * sigmaFrac);
    return cache.emplace(key, std::move(m)).first->second;
}

// Soma aditiva do brilho em cache, tingido por 'col' * intensity.
static void addGlow(cv::Mat &view, cv::Point c, int radius, double circleFrac, double sigmaFrac,
                    const cv::Scalar &col, double intensity)
{
    if (radius <= 0 || intensity <= 0.0)
        return;
    const int pad = radius * 2;
    const cv::Rect roi(c.x - pad, c.y - pad, 2 * pad, 2 * pad);
    const cv::Rect vis = roi & cv::Rect(0, 0, view.cols, view.rows);
    if (vis.empty())
        return;

    const cv::Mat &mask = glowMask(radius, circleFrac, sigmaFrac);
    cv::Mat glow;
    cv::multiply(mask(cv::Rect(vis.x - roi.x, vis.y - roi.y, vis.width, vis.height)),
                 col * intensity, glow, 1.0 / 255.0);
    cv::Mat sub = view(vis);
    cv::add(sub, glow, sub);
}

// Brilho aditivo ao redor do mago + anel que se expande. g01: 1 = recem lancado, 0 = apagado.
static void drawCastGlow(cv::Mat &view, cv::Point c, int radius,
                         const cv::Scalar &col, float g01)
{
    g01 = std::clamp(g01, 0.f, 1.f);
    if (g01 <= 0.f || radius <= 0)
        return;

    addGlow(view, c, radius, 0.6, 0.3, col, 0.9 * g01);

    // anel que cresce enquanto o brilho some
    const int ringR = cvRound(radius * (0.35 + 0.65 * (1.f - g01)));
    cv::circle(view, c, ringR, col * g01, 2, cv::LINE_AA);
}

// Cor media (BGR) do sprite, ponderada pelo alfa e normalizada para ficar brilhante.
// Amostra uma grade de ate ~24x24 pixels: custo baixo, sem precisar de cache.
static cv::Scalar spriteGlowColor(const cv::Mat &spr)
{
    if (spr.empty() || spr.depth() != CV_8U || spr.channels() < 3)
        return cv::Scalar(255, 255, 255);

    const int stepY = std::max(1, spr.rows / 24);
    const int stepX = std::max(1, spr.cols / 24);
    const int cn = spr.channels();
    double sb = 0, sg = 0, sr = 0, sw = 0;
    for (int y = 0; y < spr.rows; y += stepY)
    {
        const uchar *row = spr.ptr<uchar>(y);
        for (int x = 0; x < spr.cols; x += stepX)
        {
            const uchar *px = row + x * cn;
            const double a = (cn == 4) ? px[3] / 255.0 : 1.0;
            if (a < 0.1)
                continue;
            sb += px[0] * a;
            sg += px[1] * a;
            sr += px[2] * a;
            sw += a;
        }
    }
    if (sw <= 0.0)
        return cv::Scalar(255, 255, 255);

    cv::Scalar c(sb / sw, sg / sw, sr / sw);
    const double mx = std::max({c[0], c[1], c[2], 1.0});
    const double k = 255.0 / mx; // brilho maximo mantendo o matiz
    return cv::Scalar(c[0] * k, c[1] * k, c[2] * k);
}

// Brilho pulsante aditivo ao redor de um feitico em voo.
static void drawSpellGlow(cv::Mat &view, cv::Point c, int radius,
                          const cv::Scalar &col, float time)
{
    if (radius <= 0)
        return;
    const double pulse = 0.65 + 0.35 * std::sin(time * 14.0);
    addGlow(view, c, radius, 0.5, 0.4, col, 0.8 * pulse);
}

// Faiscas brilhosas expelidas pelo feitico em voo, com a cor media do sprite.
// Sem estado: cada faisca e recalculada a partir do tempo da partida e do progresso do
// projetil (posicao onde ele estava quando a faisca nasceu), entao nao precisa de lista
// de particulas e acompanha pausa/reset sozinha. Desenha na camada 'layer' (aditiva).
static void drawProjectileSparks(cv::Mat &layer, const Projectile &pr, int xFrom, int xTo,
                                 int H, int sw, int sh, const cv::Scalar &col, float now)
{
    constexpr float kRate = 70.f; // faiscas por segundo
    constexpr float kLife = 0.6f; // duracao de cada faisca (s)
    const int count = int(kRate * kLife);
    const int dir = (xTo >= xFrom) ? 1 : -1;
    const int base = int(std::floor(now * kRate));
    const int salt = pr.owner * 31 + int(pr.type) * 7;
    const cv::Scalar white(255, 255, 255);

    for (int j = 0; j < count; ++j)
    {
        const int n = base - j;
        if (n < 0)
            continue;
        const float age = now - n / kRate;
        if (age < 0.f || age >= kLife || pr.travelTime <= 0.f)
            continue;
        const float tEmit = pr.t - age / pr.travelTime; // onde o projetil estava ao nascer
        if (tEmit < 0.f)
            continue;

        const float life01 = age / kLife;
        const float u1 = (rnd(n, salt + 1) + 1.f) * 0.5f;
        const float u2 = (rnd(n, salt + 2) + 1.f) * 0.5f;

        // nasce na traseira do feitico e e jogada para tras/para os lados
        const float ox = xFrom + (xTo - xFrom) * tEmit - dir * sw * 0.25f;
        const float oy = H / 2 + rnd(n, salt + 3) * sh * 0.35f;
        const float vx = -dir * (40.f + 90.f * u1);
        const float vy = rnd(n, salt + 4) * 70.f;
        const cv::Point pt(cvRound(ox + vx * age), cvRound(oy + vy * age + 25.f * age * age));

        const float fade = (1.f - life01) * (0.65f + 0.35f * std::sin(now * 28.f + n));
        const int radius = 1 + int(2.f * (1.f - life01) * (0.5f + 0.5f * u2));

        cv::circle(layer, pt, radius, col * (0.75f * fade), cv::FILLED, cv::LINE_AA);
        cv::circle(layer, pt, std::max(0, radius - 1), (col * 0.3 + white * 0.7) * fade,
                   cv::FILLED, cv::LINE_AA); // nucleo quase branco
    }
}

// Os magos ficam a 160 px das bordas da arena.
static int wizardX(int p, int W) { return p == 0 ? 160 : W - 160; }

int arenaProjectileX(const Projectile &pr, int W)
{
    const int a = wizardX(pr.owner, W), b = wizardX(1 - pr.owner, W);
    return a + int((b - a) * std::clamp(pr.t, 0.f, 1.f));
}

void drawGame(cv::Mat &view, const Match &m, const Assets &as, bool glowEffects)
{
    if (!as.background.empty())
    {
        if (as.background.size() == view.size())
            as.background.copyTo(view);
        else
        {
            cv::Mat background;
            cv::resize(as.background, background, view.size());
            background.copyTo(view);
        }
    }
    else
        view.setTo(cv::Scalar(30, 20, 20));

    const int W = view.cols, H = view.rows;
    const int xs[2] = {wizardX(0, W), wizardX(1, W)};

    // "super" (Ataque Especial): a arena escurece por um instante, como num jogo de luta
    const float dark = std::max(superDarken(m.anim(0)), superDarken(m.anim(1)));
    if (dark > 0.f)
        view.convertTo(view, -1, 1.0 - 0.55 * dark);

    // magos e escudos
    // os magos ocupam ~84% da altura: sobra espaco para pulos e levitacao
    const int wizardMaxH = std::max(1, int(H * 0.84f));
    for (int p = 0; p < 2; ++p)
    {
        const FighterAnim anim = m.anim(p);

        // brilho atras do mago ao lancar feitico
        if (glowEffects && anim.koT < 0.f)
            drawCastGlow(view, {xs[p], H / 2}, cvRound(H * 0.45f),
                         castGlowColor(m.castType(p)), m.castGlow01(p));

        const auto &fr = as.wizard[p];
        cv::Point body(xs[p], H / 2);
        if (!fr.empty())
        {
            int f = int(m.time() * 8) % int(fr.size()); // 8 quadros por segundo
            const cv::Mat &spr = fitSprite(wizardMaxH, fr[f]);
            const cv::Point feet(xs[p], H / 2 + spr.rows / 2); // mesma posicao de antes em repouso
            body = drawFighter(view, spr, p, feet, anim, moveStyleFor(as.charId[p]),
                               m.time(), glowEffects);
        }
        else
            cv::circle(view, body, 30,
                       p == 0 ? cv::Scalar(255, 200, 80) : cv::Scalar(80, 200, 255), cv::FILLED);
        if (m.hasShield(p) && anim.koT < 0.f)
        {
            if (!as.shield[p].empty())
                drawFit(view, as.shield[p], body);
            else
                cv::circle(view, body, 50, {255, 255, 255}, 3);
        }
    }

    cv::Mat sparkLayer; // camada aditiva das faiscas (criada so se houver projeteis)

    for (const Projectile &pr : m.projectiles())
    {
        if (pr.t < 0.f) // especial ainda carregando: so aparece no disparo
            continue;
        int x = xs[pr.owner] + int((xs[1 - pr.owner] - xs[pr.owner]) * pr.t);

        // especiais com animacao propria
        if (pr.type == SpellType::AtaqueEspecial)
        {
            const int o = pr.owner;
            if (pr.special == SpecialKind::FuriaDaFloresta && !as.specialFrames[o].empty())
            {
                drawMaxwellTiger(view, as.specialFrames[o], o == 0 ? 1 : -1, pr.t, xs[o], xs[1 - o], H, m.time());
                continue;
            }
            if (pr.special == SpecialKind::SobrecargaSistema)
            {
                if (glowEffects && pr.t > 0.18f)
                    drawSpellGlow(view, {xs[1 - o], int(H * 0.36f)}, cvRound(H * 0.6f), cv::Scalar(255, 200, 90),
                                  m.time());
                drawHolyCross(view, xs[1 - o], pr.t, H, m.time());
                continue;
            }
            if (pr.special == SpecialKind::CargaDoGeneral)
                continue; // a onda da integral e desenhada inteira logo abaixo
        }

        // especial sem sprite proprio: desenhado em pixel art pelo codigo
        if (pr.type == SpellType::AtaqueEspecial && !as.customSpecial[pr.owner])
        {
            if (glowEffects)
                drawSpellGlow(view, {x, H / 2}, cvRound(H * 0.38f), specialColor(pr.special), m.time());
            drawSpecialProjectile(view, pr.special, {x, H / 2}, pr.owner == 0 ? 1 : -1, m.time(), H);
            continue;
        }

        const auto &anim = pr.owner == 1 ? as.boltAnimFlipped[1][int(pr.type)]
                                         : as.boltAnim[0][int(pr.type)];
        const cv::Mat *spr = nullptr;
        if (!anim.empty())
        {
            int f = int(m.time() * 24) % int(anim.size()); // 24 quadros por segundo
            spr = &anim[f];
        }
        else
        {
            spr = pr.owner == 1 ? &as.boltFlipped[1][int(pr.type)]
                                : &as.bolt[0][int(pr.type)];
        }

        if (!spr->empty())
        {
            // brilho com a cor media do sprite (atras do feitico)
            if (glowEffects)
            {
                const double fit = spr->rows > H - 4 ? double(H - 4) / spr->rows : 1.0;
                const int glowR = std::min(cvRound(std::max(spr->cols, spr->rows) * fit * 0.7),
                                           cvRound(H * 0.5));
                const cv::Scalar sprCol = spriteGlowColor(*spr);
                drawSpellGlow(view, {x, H / 2}, glowR, sprCol, m.time());

                // faiscas brilhosas com a cor media do sprite
                if (sparkLayer.empty())
                    sparkLayer = cv::Mat::zeros(view.size(), view.type());
                drawProjectileSparks(sparkLayer, pr, xs[pr.owner], xs[1 - pr.owner], H,
                                     cvRound(spr->cols * fit), cvRound(spr->rows * fit),
                                     sprCol, m.time());
            }

            if (pr.type == SpellType::AtaqueMedio) // Raio: corta a arena inteira
            {
                const int dir = (pr.owner == 0) ? 1 : -1;
                const cv::Point tail(xs[pr.owner] + dir * 40, H / 2);
                const cv::Point head(x - dir * (spr->cols / 3), H / 2);
                drawLightningTrail(view, tail, head, as.trailColor[pr.owner],
                                   m.time(), pr.owner, glowEffects);
            }
            drawFit(view, *spr, {x, H / 2});
        }
        else
            cv::circle(view, {x, H / 2}, 10 + int(pr.damage / 3), {80, 80, 255}, cv::FILLED);
    }

    if (!sparkLayer.empty())
        cv::add(view, sparkLayer, view);

    // Venegas: a onda da integral avanca com a parte da frente; as barras sao as "partes"
    for (int o = 0; o < 2; ++o)
    {
        float front = -1.f;
        int parts = 0;
        for (const Projectile &pr : m.projectiles())
            if (pr.owner == o && pr.type == SpellType::AtaqueEspecial && pr.special == SpecialKind::CargaDoGeneral)
            {
                ++parts;
                front = std::max(front, pr.t);
            }
        if (parts == 0 || front < 0.f)
            continue;
        if (parts < specialDef(SpecialKind::CargaDoGeneral).shots) // a primeira parte ja acertou
            front = 1.f;
        if (glowEffects)
            drawSpellGlow(view, {xs[o] + (xs[1 - o] - xs[o]) / 2, H * 2 / 3}, cvRound(H * 0.5f),
                          cv::Scalar(230, 120, 80), m.time());
        drawIntegralWave(view, xs[o], xs[1 - o], front, H, m.time());
    }

    // placar estilo fliperama nos cantos de cima
    {
        const int px = std::max(2, H / 72);
        for (int p = 0; p < 2; ++p)
        {
            const std::string s = cv::format("%dP %06d", p + 1, m.score(p));
            const int w = pfx::pixelTextWidth(s, px);
            const cv::Point tl(p == 0 ? 2 * px : W - 2 * px - w, 2 * px);
            pfx::pixelText(view, s, tl + cv::Point(px / 2 + 1, px / 2 + 1), px, cv::Scalar(20, 10, 25));
            pfx::pixelText(view, s.substr(0, 2), tl, px, ui::playerColor(p));
            pfx::pixelText(view, s.substr(2), tl + cv::Point(pfx::pixelTextWidth(s.substr(0, 2), px), 0), px,
                           cv::Scalar(120, 235, 255));
        }
    }

    // textos de combate subindo sobre os magos ("-30", "BLOQUEADO!", "+15 VIDA"...)
    {
        int stack[2] = {0, 0}; // empilha textos simultaneos do mesmo mago
        const double scale = std::clamp(H / 260.0, 0.45, 1.1);
        for (auto it = m.combatTexts().rbegin(); it != m.combatTexts().rend(); ++it)
        {
            const CombatText &ct = *it;
            if (ct.player < 0 || ct.player > 1)
                continue;
            if (ct.age > kCombatTextLife - 0.3f && int(ct.age * 20) % 2) // pisca antes de sumir
                continue;
            cv::Scalar col;
            switch (ct.kind)
            {
            case CombatTextKind::Damage:
                col = cv::Scalar(70, 90, 255);
                break;
            case CombatTextKind::Blocked:
                col = cv::Scalar(255, 230, 150);
                break;
            case CombatTextKind::Heal:
                col = cv::Scalar(110, 240, 120);
                break;
            case CombatTextKind::Mana:
                col = cv::Scalar(255, 200, 90);
                break;
            case CombatTextKind::Drain:
                col = cv::Scalar(230, 100, 230);
                break;
            default:
                col = cv::Scalar(80, 230, 255);
                break;
            }
            const cv::Size ts = pfx::textSize(ct.text, cv::FONT_HERSHEY_DUPLEX, scale, 2);
            const float rise = pfx::stepT(ct.age, 15.f) * H * 0.35f;
            int x = xs[ct.player] - ts.width / 2;
            if (ct.kind == CombatTextKind::Damage && ct.age < 0.12f)
                x += int(pfx::hash11(int(ct.age * 60), ct.player) * 4);
            const int y = int(H * 0.30f - rise) - stack[ct.player]++ * (ts.height + 8);
            pfx::outlinedText(view, ct.text, {x, std::max(ts.height + 2, y)}, cv::FONT_HERSHEY_DUPLEX, scale, col, 2);
        }
    }

    // abertura: "PREPAREM-SE" e "LUTEM!"
    if (m.inIntro())
    {
        const float t = m.time();
        const bool fight = t >= kIntroTime - 0.7f;
        const std::string txt = fight ? "LUTEM!" : "PREPAREM-SE";
        const float u = pfx::seg(pfx::stepT(t - (fight ? kIntroTime - 0.7f : 0.f), 15.f), 0.f, 0.2f);
        const double scale = std::clamp(H / 110.0, 0.8, 2.6) * (fight ? 1.4 - 0.4 * u : 1.0);
        const cv::Size ts = pfx::textSize(txt, cv::FONT_HERSHEY_DUPLEX, scale, 3);
        pfx::outlinedText(view, txt, {W / 2 - ts.width / 2, H / 2 + ts.height / 2}, cv::FONT_HERSHEY_DUPLEX, scale,
                          fight ? cv::Scalar(60, 220, 255) : cv::Scalar(240, 235, 250), 3);
    }

    if (m.state() != MatchState::Running)
    {
        const int loser = m.state() == MatchState::Player0Won ? 1 : 0;
        const float koT = m.anim(loser).koT;
        drawKOBanner(view, koT); // "K.O." primeiro, depois o nome do vencedor
        if (koT > 1.6f)
            cv::putText(view, m.state() == MatchState::Player0Won ? "JOGADOR 1 VENCEU" : "JOGADOR 2 VENCEU",
                        {W / 2 - 200, H / 2 - 55}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 3);
        if (koT > 2.5f && int(koT * 2.f) % 2 == 0)
        {
            const std::string hint = "Pressione M para voltar ao menu";
            const cv::Size hs = pfx::textSize(hint, cv::FONT_HERSHEY_SIMPLEX, 0.55, 1);
            pfx::outlinedText(view, hint, {W / 2 - hs.width / 2, H - 12}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                              cv::Scalar(240, 235, 250), 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Barras de vida/mana usadas pelo HUD superior (chamadas pelo main.cpp)
// ---------------------------------------------------------------------------

static const BarSkin &skinFor(const Assets &as, BarKind kind)
{
    return kind == BarKind::Health ? as.healthBar : as.manaBar;
}

cv::Size statBarSize(const Assets &as, const cv::Size &maxSize)
{
    const BarSkin &sk = as.healthBar.valid() ? as.healthBar : as.manaBar;
    if (!sk.valid())
        return {std::max(1, maxSize.width), std::max(1, std::min(maxSize.height, 22))};

    const cv::Mat &f = sk.frame[0];
    const double s = std::min({double(maxSize.width) / f.cols,
                               double(maxSize.height) / f.rows, 1.0});
    return {std::max(1, cvRound(f.cols * s)), std::max(1, cvRound(f.rows * s))};
}

void drawStatBar(cv::Mat &dst, const Assets &as, BarKind kind, int side,
                 float v01, const cv::Rect &rect, const std::string &label)
{
    v01 = std::clamp(v01, 0.f, 1.f);
    side = std::clamp(side, 0, 1);
    const BarSkin &sk = skinFor(as, kind);
    cv::Rect textArea = rect;

    if (!sk.valid())
    {
        // Fallback: retangulo simples
        const cv::Scalar col = kind == BarKind::Health ? cv::Scalar(60, 60, 230)
                                                       : cv::Scalar(230, 160, 50);
        cv::rectangle(dst, rect, {20, 20, 20}, cv::FILLED);
        const int w = cvRound((rect.width - 4) * v01);
        const int x = side == 0 ? rect.x + 2 : rect.x + rect.width - 2 - w;
        if (w > 0)
            cv::rectangle(dst, cv::Rect(x, rect.y + 2, w, rect.height - 4), col, cv::FILLED);
        cv::rectangle(dst, rect, {255, 255, 255}, 1);
    }
    else
    {
        // Moldura redimensionada para o rect
        const cv::Mat &src = sk.frame[side];
        const double sx = double(rect.width) / src.cols;
        const double sy = double(rect.height) / src.rows;
        cv::Mat frame;
        cv::resize(src, frame, rect.size(), 0, 0, cv::INTER_AREA);
        drawSpriteTL(dst, frame, rect.tl());

        // Slot interno escalado
        const cv::Rect &sl = sk.slot[side];
        const cv::Rect slot(cvRound(sl.x * sx), cvRound(sl.y * sy),
                            std::max(1, cvRound(sl.width * sx)),
                            std::max(1, cvRound(sl.height * sy)));
        textArea = cv::Rect(rect.x + slot.x, rect.y + slot.y, slot.width, slot.height);

        const int w = cvRound(slot.width * v01);
        if (w > 0)
        {
            cv::Mat fill;
            cv::resize(sk.fill[side], fill, slot.size(), 0, 0, cv::INTER_AREA);
            const int srcX = side == 0 ? 0 : slot.width - w;
            const int dstX = side == 0 ? slot.x : slot.x + slot.width - w;
            drawSpriteTL(dst, fill(cv::Rect(srcX, 0, w, slot.height)),
                         rect.tl() + cv::Point(dstX, slot.y));
        }
    }

    if (!label.empty())
    {
        const double scale = std::clamp(textArea.height / 28.0, 0.3, 0.6);
        int base = 0;
        const cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &base);
        const cv::Point org(textArea.x + (textArea.width - ts.width) / 2,
                            textArea.y + (textArea.height + ts.height) / 2);
        cv::putText(dst, label, org + cv::Point(1, 1), cv::FONT_HERSHEY_SIMPLEX, scale,
                    {0, 0, 0}, 2, cv::LINE_AA);
        cv::putText(dst, label, org, cv::FONT_HERSHEY_SIMPLEX, scale,
                    {255, 255, 255}, 1, cv::LINE_AA);
    }
}