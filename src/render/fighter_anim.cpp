#include "render/fighter_anim.h"
#include "render/sprites.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace
{

    constexpr float kPi = 3.14159265f;
    constexpr float kAnimFps = 15.f; // poses em quadros discretos, como sprite de arcade

    // ---------------------------------------------------------------------------
    // Utilitarios de tempo / interpolacao
    // ---------------------------------------------------------------------------

    float stepT(float t) { return std::floor(t * kAnimFps) / kAnimFps; }
    float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
    float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); } // progresso em [a,b]
    float easeOut(float u)
    {
        u = clamp01(u);
        return 1.f - (1.f - u) * (1.f - u);
    }
    float easeIn(float u)
    {
        u = clamp01(u);
        return u * u;
    }
    float easeInOut(float u)
    {
        u = clamp01(u);
        return u * u * (3.f - 2.f * u);
    }
    float arc(float u)
    {
        u = clamp01(u);
        return 4.f * u * (1.f - u);
    } // 0 -> 1 -> 0
    float lerp(float a, float b, float u) { return a + (b - a) * u; }

    // ruido deterministico em [-1, 1]
    float hash11(int a, int b)
    {
        unsigned h = unsigned(a) * 374761393u + unsigned(b) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (h & 0xFFFF) / 65535.f * 2.f - 1.f;
    }
    float hash01(int a, int b) { return 0.5f + 0.5f * hash11(a, b); }

    // Dissolve em pixels: o bloco i some quando 'fade' (0..1) passa do seu limiar.
    bool visible(int i, int seed, float fade) { return hash01(i, seed) >= fade; }

    cv::Scalar lighten(const cv::Scalar &c, double k)
    {
        return cv::Scalar(c[0] + (255 - c[0]) * k, c[1] + (255 - c[1]) * k, c[2] + (255 - c[2]) * k);
    }
    cv::Scalar darken(const cv::Scalar &c, double k) { return c * (1.0 - k); }

    // ---------------------------------------------------------------------------
    // Primitivas em blocos de pixel (alinhadas a uma grade de 'px')
    // ---------------------------------------------------------------------------

    void pix(cv::Mat &v, float x, float y, int px, const cv::Scalar &c, int mult = 1)
    {
        const int gx = int(std::floor(x / px)) * px;
        const int gy = int(std::floor(y / px)) * px;
        cv::rectangle(v, cv::Rect(gx, gy, px * mult, px * mult), c, cv::FILLED);
    }

    void pixLine(cv::Mat &v, cv::Point2f a, cv::Point2f b, int px, const cv::Scalar &c,
                 float fade = 0.f, int seed = 0)
    {
        const float len = std::hypot(b.x - a.x, b.y - a.y);
        const int n = std::max(1, int(len / px));
        for (int i = 0; i <= n; ++i)
        {
            if (!visible(i, seed, fade))
                continue;
            const float u = float(i) / n;
            pix(v, lerp(a.x, b.x, u), lerp(a.y, b.y, u), px, c);
        }
    }

    // Faisca de impacto: raios de blocos que se abrem a partir do centro.
    void burst(cv::Mat &v, cv::Point2f c, float u, float radius, const cv::Scalar &col, int px, int seed)
    {
        if (u <= 0.f || u >= 1.f || radius <= 0.f)
            return;
        const cv::Scalar white(255, 255, 255);
        const int rays = 8;
        const float d0 = radius * (0.2f + 0.8f * easeOut(u));
        const float len = radius * 0.55f * (1.f - u);
        for (int r = 0; r < rays; ++r)
        {
            const float a = r * 2.f * kPi / rays + hash11(r, seed) * 0.25f;
            const float ca = std::cos(a), sa = std::sin(a);
            const float reach = d0 * (0.7f + 0.3f * hash01(r, seed + 1));
            for (float d = std::max(0.f, reach - len); d <= reach; d += px)
                pix(v, c.x + ca * d, c.y + sa * d, px, d > reach - 1.5f * px ? white : col);
        }
        if (u < 0.35f) // clarao no centro
        {
            const int s = std::max(px, int((1.f - u / 0.35f) * radius * 0.35f / px) * px);
            for (int dy = -s; dy <= s; dy += px)
            {
                const int w = s - std::abs(dy); // losango
                for (int dx = -w; dx <= w; dx += px)
                    pix(v, c.x + dx, c.y + dy, px, std::abs(dx) + std::abs(dy) < s / 2 ? white : col);
            }
        }
    }

    // Nuvem de poeira no chao, espalhando para os dois lados.
    void dust(cv::Mat &v, cv::Point2f ground, float u, float spread, int px, int seed)
    {
        if (u <= 0.f || u >= 1.f)
            return;
        const cv::Scalar light(165, 180, 195), dark(115, 125, 140);
        for (int i = 0; i < 6; ++i)
            for (int s = -1; s <= 1; s += 2)
            {
                const int id = i * 2 + (s > 0);
                if (!visible(id, seed, u))
                    continue;
                const float k = 0.45f + 0.55f * (i / 5.f);
                const float x = ground.x + s * (0.15f + 0.85f * easeOut(u)) * spread * k;
                const float y = ground.y - px - arc(u) * spread * 0.22f * (0.4f + 0.6f * hash01(id, seed + 3));
                pix(v, x, y, px, i % 2 ? light : dark, u < 0.5f ? 2 : 1);
            }
    }

    // Anel (ou elipse, com ryScale < 1) de blocos.
    void pixelRing(cv::Mat &v, cv::Point2f c, float r, float ryScale, const cv::Scalar &col,
                   int px, float fade, int seed, float spin = 0.f)
    {
        if (r <= 0.f)
            return;
        const int n = std::max(12, int(2.f * kPi * r / (px * 1.6f)));
        for (int i = 0; i < n; ++i)
        {
            if (!visible(i, seed, fade))
                continue;
            const float a = 2.f * kPi * i / n + spin;
            pix(v, c.x + std::cos(a) * r, c.y + std::sin(a) * r * ryScale, px,
                i % 3 == 0 ? lighten(col, 0.6) : col);
        }
    }

    // ---------------------------------------------------------------------------
    // Poses
    // ---------------------------------------------------------------------------

    struct Pose
    {
        float dx = 0.f, dy = 0.f; // deslocamento do pivo (px); dx > 0 = para frente; dy < 0 = sobe
        float lean = 0.f;         // graus; > 0 inclina para tras (para longe do oponente)
        float sx = 1.f, sy = 1.f; // squash & stretch (sx < 0 = espelhado, usado no giro)
        float pivot = 1.f;        // 1 = pes, 0.5 = centro do corpo
        float white = 0.f;        // flash branco (0..1)
        float red = 0.f;          // tinta vermelha de dano (0..1)
        float shake = 0.f;        // amplitude do tremor (px)
    };

    float spellIntensity(SpellType t)
    {
        switch (t)
        {
        case SpellType::AtaqueFraco:
            return 0.70f;
        case SpellType::AtaqueMedio:
            return 0.85f;
        case SpellType::AtaqueForte:
            return 1.00f;
        case SpellType::AtaqueEspecial:
            return 1.25f;
        default:
            return 0.80f;
        }
    }

    float castDuration(SpellType t) { return t == SpellType::Escudo ? 0.45f : kCastAnimTime; }

    Pose hitPose(float t, float power, float U)
    {
        Pose p;
        const float k = 0.5f + 0.5f * power;
        if (t < 0.07f) // hit-stop: congela no impacto com flash branco
        {
            p.white = 1.f;
            p.shake = 4.f * U * k;
            p.sy = 0.96f;
        }
        const float push = easeOut(seg(t, 0.f, 0.16f)) * (1.f - easeInOut(seg(t, 0.22f, kHitAnimTime)));
        p.dx = -(14.f + 22.f * power) * U * push;
        p.lean = 4.f * k * (t < 0.07f) + (8.f + 10.f * power) * push;
        p.sy *= 1.f - 0.04f * push;
        p.sx = 1.f + 0.03f * push;
        p.red = std::max(0.f, 0.75f * (1.f - t / 0.35f));
        if (t < 0.2f)
            p.shake = std::max(p.shake, 3.f * U * k * (1.f - t / 0.2f));
        return p;
    }

    // rest = distancia do centro do corpo ao chao com o mago deitado
    Pose koPose(float t, float U, float h, float rest)
    {
        Pose p;
        p.pivot = 0.5f;
        const float D = 28.f * U;            // quanto e arremessado para tras
        const float lying = h * 0.5f - rest; // dy do centro quando deitado
        if (t < 0.10f)
        {
            p.white = 1.f;
            p.lean = 10.f;
            p.shake = 6.f * U;
            p.red = 0.6f;
        }
        else if (t < 0.70f) // voo em arco caindo de costas
        {
            const float u = seg(t, 0.10f, 0.70f);
            p.lean = lerp(10.f, 90.f, easeInOut(u));
            p.dx = -D * u;
            p.dy = lerp(0.f, lying, easeIn(u)) - 0.28f * h * arc(u);
            p.red = 0.5f * (1.f - u);
        }
        else if (t < 1.0f) // quica no chao
        {
            const float v = seg(t, 0.70f, 1.0f);
            p.lean = 90.f - 8.f * arc(v);
            p.dx = -D - 10.f * U * v;
            p.dy = lying - 0.09f * h * arc(v);
        }
        else // deitado
        {
            p.lean = 90.f;
            p.dx = -D - 10.f * U;
            p.dy = lying;
        }
        return p;
    }

    // Golpe especial: tres quadros-chave (repouso -> preparo -> golpe -> repouso).
    struct KF
    {
        float dx, dy, lean, sx, sy;
    };
    struct MoveDef
    {
        KF windup, action;
        bool slam; // o golpe acelera para baixo (pisao)
    };

    KF lerpKF(const KF &a, const KF &b, float u)
    {
        return {lerp(a.dx, b.dx, u), lerp(a.dy, b.dy, u), lerp(a.lean, b.lean, u),
                lerp(a.sx, b.sx, u), lerp(a.sy, b.sy, u)};
    }

    //               dx   dy  lean   sx     sy          (dx/dy em unidades de U)
    MoveDef moveDef(MoveStyle s)
    {
        switch (s)
        {
        case MoveStyle::Derzu:
            return {{-6, 0, 12, 1.03f, 0.94f}, {24, 0, -16, 0.97f, 1.06f}, false};
        case MoveStyle::Neri:
            return {{-2, -4, 0, 1.f, 0.97f}, {10, -10, -6, 1.f, 1.03f}, false};
        case MoveStyle::Menegueti:
            return {{0, -14, 4, 0.98f, 1.03f}, {6, -26, -6, 1.f, 1.04f}, false};
        case MoveStyle::Venegas:
            return {{-8, 0, 10, 1.02f, 0.96f}, {46, 0, -14, 1.04f, 0.97f}, false};
        case MoveStyle::Baltar:
            return {{-4, -30, -4, 0.96f, 1.06f}, {12, 0, -6, 1.12f, 0.84f}, true};
        case MoveStyle::Subramana:
            return {{0, -6, 6, 1.f, 1.02f}, {4, -18, -4, 1.f, 1.03f}, false};
        default:
            return {{-4, 0, 8, 1.02f, 0.95f}, {14, 0, -10, 0.98f, 1.04f}, false};
        }
    }

    Pose castPose(MoveStyle s, SpellType type, float t, float U)
    {
        Pose p;
        if (type == SpellType::Escudo) // guarda: agacha e se prepara para o impacto
        {
            const float a = arc(seg(t, 0.f, castDuration(type)));
            p.sy = 1.f - 0.07f * a;
            p.sx = 1.f + 0.03f * a;
            p.lean = 6.f * a;
            p.dx = -4.f * U * a;
            return p;
        }

        const float I = spellIntensity(type);
        const MoveDef m = moveDef(s);
        const KF rest{0, 0, 0, 1.f, 1.f};
        KF k;
        if (t < 0.18f)
            k = lerpKF(rest, m.windup, easeOut(seg(t, 0.f, 0.18f)));
        else if (t < 0.40f)
        {
            const float u = seg(t, 0.18f, 0.40f);
            k = lerpKF(m.windup, m.action, m.slam ? easeIn(u) : easeOut(u));
        }
        else
            k = lerpKF(m.action, rest, easeInOut(seg(t, 0.40f, kCastAnimTime)));

        p.dx = k.dx * U * I;
        p.dy = k.dy * U * I;
        p.lean = k.lean * I;
        p.sx = 1.f + (k.sx - 1.f) * I;
        p.sy = 1.f + (k.sy - 1.f) * I;

        if (s == MoveStyle::Neri) // giro completo no proprio eixo
        {
            const float u = seg(t, 0.12f, 0.52f);
            if (u > 0.f && u < 1.f)
            {
                float c = std::cos(2.f * kPi * u);
                if (std::fabs(c) < 0.1f)
                    c = c < 0.f ? -0.1f : 0.1f;
                p.sx *= c;
            }
        }
        if (s == MoveStyle::Menegueti && t > 0.18f && t < 0.55f) // flutua
            p.dy += std::sin(t * 25.f) * 2.f * U;
        return p;
    }

    // ---------------------------------------------------------------------------
    // Super (Ataque Especial): carga -> disparo -> recuperacao, em kSpecialAnimTime.
    // A carga termina em kSuperRelease, junto com o launchDelay do especial.
    // ---------------------------------------------------------------------------

    constexpr float kSuperRelease = 0.50f;
    constexpr float kSuperRecover = 0.80f;

    //                 carga                      disparo
    void superKeys(MoveStyle s, KF &charge, KF &release)
    {
        switch (s)
        {
        case MoveStyle::Derzu: // ergue o cajado flutuando e investe
            charge = {-4, -14, 8, 0.98f, 1.05f};
            release = {30, -4, -18, 1.03f, 0.97f};
            break;
        case MoveStyle::Menegueti: // sobe alto diante do Olho
            charge = {-2, -26, 6, 0.98f, 1.04f};
            release = {10, -16, -10, 1.02f, 1.02f};
            break;
        case MoveStyle::Venegas: // posicao de sentido, depois a carga
            charge = {-6, 0, 4, 0.97f, 1.06f};
            release = {8, 0, -6, 1.f, 1.f};
            break;
        case MoveStyle::Neri: // pulo girando, depois aponta
            charge = {0, -12, 0, 1.f, 1.02f};
            release = {20, 0, -14, 1.03f, 0.97f};
            break;
        case MoveStyle::Baltar: // agacha juntando forca da terra
            charge = {-4, 0, 4, 1.07f, 0.88f};
            release = {14, 0, -8, 1.12f, 0.84f};
            break;
        case MoveStyle::Subramana: // meditacao levitando
            charge = {0, -22, 0, 1.f, 1.02f};
            release = {8, -10, -8, 1.f, 1.08f};
            break;
        default:
            charge = {-4, 0, 8, 1.03f, 0.93f};
            release = {20, 0, -12, 0.98f, 1.05f};
            break;
        }
    }

    Pose superPose(MoveStyle s, float t, float U, float h)
    {
        KF charge, release;
        superKeys(s, charge, release);
        const KF rest{0, 0, 0, 1.f, 1.f};
        KF k;
        if (t < kSuperRelease)
            k = lerpKF(rest, charge, easeOut(seg(t, 0.f, 0.25f)));
        else if (t < kSuperRecover)
            k = lerpKF(charge, release, easeOut(seg(t, kSuperRelease, kSuperRelease + 0.12f)));
        else
            k = lerpKF(release, rest, easeInOut(seg(t, kSuperRecover, kSpecialAnimTime)));

        Pose p;
        p.dx = k.dx * U;
        p.dy = k.dy * U;
        p.lean = k.lean;
        p.sx = k.sx;
        p.sy = k.sy;

        switch (s)
        {
        case MoveStyle::Neri: // dois giros completos durante a carga
        {
            const float u = seg(t, 0.05f, 0.45f);
            if (u > 0.f && u < 1.f)
            {
                float c = std::cos(4.f * kPi * u);
                if (std::fabs(c) < 0.1f)
                    c = c < 0.f ? -0.1f : 0.1f;
                p.sx *= c;
            }
            break;
        }
        case MoveStyle::Menegueti:
        case MoveStyle::Subramana: // flutua oscilando
            if (t > 0.2f && t < kSuperRecover)
                p.dy += std::sin(t * 14.f) * 2.5f * U;
            break;
        case MoveStyle::Venegas: // tres investidas, uma por disparo
            for (int i = 0; i < 3; ++i)
            {
                const float a = kSuperRelease + 0.18f * i;
                const float u = seg(t, a, a + 0.16f);
                if (u > 0.f && u < 1.f)
                {
                    p.dx += 26.f * U * arc(u);
                    p.lean -= 10.f * arc(u);
                }
            }
            break;
        case MoveStyle::Baltar: // salta no disparo e pisa
        {
            const float u = seg(t, kSuperRelease, kSuperRelease + 0.2f);
            if (u > 0.f && u < 1.f)
            {
                p.dy -= 0.16f * h * arc(u);
                p.sy = 1.04f;
                p.sx = 0.97f;
            }
            break;
        }
        default:
            break;
        }
        return p;
    }

    // ---------------------------------------------------------------------------
    // Desenho do sprite com pose
    // ---------------------------------------------------------------------------

    // Copia do sprite misturada com 'col' (k) e com o alfa multiplicado.
    cv::Mat tinted(const cv::Mat &spr, const cv::Scalar &col, float k, float alphaMul)
    {
        if (k <= 0.f && alphaMul >= 1.f)
            return spr;
        cv::Mat out = spr.clone();
        if (out.type() != CV_8UC4)
            return out;
        k = clamp01(k);
        for (int y = 0; y < out.rows; ++y)
        {
            cv::Vec4b *row = out.ptr<cv::Vec4b>(y);
            for (int x = 0; x < out.cols; ++x)
            {
                cv::Vec4b &px = row[x];
                if (px[3] == 0)
                    continue;
                for (int c = 0; c < 3; ++c)
                    px[c] = cv::saturate_cast<uchar>(px[c] * (1.f - k) + col[c] * k);
                px[3] = cv::saturate_cast<uchar>(px[3] * alphaMul);
            }
        }
        return out;
    }

    // Coeficientes de rotacao+escala da pose (mapeia sprite -> tela, em torno do pivo).
    struct Affine
    {
        float a, b, c, d;
    };
    Affine poseAffine(const Pose &p, int f)
    {
        const float th = -f * p.lean * kPi / 180.f; // inclinar para tras = topo vai para -f
        const float cs = std::cos(th), sn = std::sin(th);
        return {cs * p.sx, -sn * p.sy, sn * p.sx, cs * p.sy};
    }

    cv::Point2f pivotWorld(const Pose &p, int f, cv::Point feet, float h)
    {
        return {feet.x + f * p.dx, feet.y + p.dy - (1.f - p.pivot) * h};
    }

    void drawPosed(cv::Mat &view, const cv::Mat &spr, const Pose &p, int f, cv::Point feet,
                   const cv::Scalar &tintCol, float tintK, float alphaMul)
    {
        const float w = float(spr.cols), h = float(spr.rows);
        const cv::Point2f pw = pivotWorld(p, f, feet, h);
        const cv::Mat img = tinted(spr, tintCol, tintK, alphaMul);

        if (p.lean == 0.f && p.sx == 1.f && p.sy == 1.f) // so translacao: sem reamostrar
        {
            const cv::Point2f c(pw.x, pw.y + (0.5f - p.pivot) * h);
            drawSprite(view, img, cv::Point(cvRound(c.x), cvRound(c.y)));
            return;
        }

        const Affine A = poseAffine(p, f);
        const cv::Point2f sp(w * 0.5f, p.pivot * h); // pivo no sprite
        const int R = int(std::ceil(std::hypot(w * 0.5f * std::fabs(p.sx),
                                               std::max(p.pivot, 1.f - p.pivot) * h * std::fabs(p.sy)))) +
                      2;
        const cv::Mat M(cv::Matx23d(A.a, A.b, R - (A.a * sp.x + A.b * sp.y),
                                    A.c, A.d, R - (A.c * sp.x + A.d * sp.y)));
        cv::Mat canvas;
        cv::warpAffine(img, canvas, M, cv::Size(2 * R, 2 * R), cv::INTER_NEAREST,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
        drawSprite(view, canvas, cv::Point(cvRound(pw.x), cvRound(pw.y))); // centro do canvas = pivo
    }

    // Distancia do centro do corpo ao chao com o mago deitado de costas (lado oposto ao oponente).
    float koRest(const cv::Mat &spr, int f)
    {
        if (spr.type() != CV_8UC4)
            return spr.cols * 0.4f;

        // Colunas extremas com algum pixel visivel (alfa > 8): equivale a boundingRect(alpha > 8)
        // na horizontal, sem depender do header de contornos do OpenCV.
        int minX = spr.cols, maxX = -1;
        for (int y = 0; y < spr.rows; ++y)
        {
            const cv::Vec4b *row = spr.ptr<cv::Vec4b>(y);
            for (int x = 0; x < spr.cols; ++x)
                if (row[x][3] > 8)
                {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                }
        }
        if (maxX < minX)
            return spr.cols * 0.4f;

        const float half = spr.cols * 0.5f;
        const float back = f > 0 ? half - minX : float(maxX + 1) - half;
        return std::max(4.f, back * 0.85f); // afunda um pouco: as bordas sao capa/cajado
    }

    // ---------------------------------------------------------------------------
    // Efeitos (atras e na frente do mago)
    // ---------------------------------------------------------------------------

    enum class Mode
    {
        Idle,
        Hit,
        Cast,
        Super,
        Ko,
        Victory
    };
    enum class Layer
    {
        Back,
        Front
    };

    struct FxCtx
    {
        cv::Mat &view;
        Mode mode;
        MoveStyle style;
        const FighterAnim &st;
        float t; // tempo da animacao ativa (quantizado)
        int f;   // +1 olha para a direita, -1 para a esquerda
        int side;
        float h, U;
        int px;
        cv::Point feet;   // pes em repouso
        cv::Point2f body; // centro do corpo com a pose
        cv::Point2f pos;  // pes com a pose (x movido; y = chao)
        cv::Scalar accent;
    };

    void castFx(const FxCtx &c, Layer L)
    {
        const float t = c.t;
        const SpellType type = c.st.castType;
        if (type == SpellType::Escudo)
            return;
        const float I = spellIntensity(type);
        const int seed = c.side * 101 + int(type) * 13;
        const int frame = int(t * kAnimFps);

        switch (c.style)
        {
        case MoveStyle::Derzu: // runa roxa no chao, girando
            if (L == Layer::Back && t > 0.08f && t < 0.62f)
            {
                const float g = easeOut(seg(t, 0.08f, 0.35f));
                const float fade = seg(t, 0.42f, 0.62f);
                const cv::Point2f g0(c.pos.x, c.feet.y - c.px);
                pixelRing(c.view, g0, (0.18f + 0.30f * g) * c.h * I, 0.22f, c.accent, c.px, fade, seed, t * 3.f);
                pixelRing(c.view, g0, (0.10f + 0.16f * g) * c.h * I, 0.22f, lighten(c.accent, 0.4), c.px, fade,
                          seed + 7, -t * 4.f);
            }
            break;

        case MoveStyle::Neri: // redemoinho ciano e dourado ao redor do corpo
            if (t > 0.10f && t < 0.58f)
            {
                const float fade = seg(t, 0.42f, 0.58f);
                const cv::Scalar cyan(255, 220, 90), gold(60, 200, 250);
                for (int i = 0; i < 16; ++i)
                {
                    const float a = i * 2.f * kPi / 16.f + t * 12.f;
                    const bool front = std::sin(a) > 0.f;
                    if ((L == Layer::Front) != front || !visible(i, seed + frame, fade))
                        continue;
                    const float x = c.body.x + std::cos(a) * 0.38f * c.h;
                    const float y = c.body.y + std::sin(a) * 0.10f * c.h + ((i % 3) - 1) * 0.16f * c.h;
                    pix(c.view, x, y, c.px, i % 2 ? cyan : gold, 2);
                }
            }
            break;

        case MoveStyle::Menegueti: // esquadro e compasso dourados atras + faiscas subindo
            if (L == Layer::Back && t > 0.05f && t < 0.62f)
            {
                const float g = easeOut(seg(t, 0.05f, 0.30f));
                const float fade = seg(t, 0.45f, 0.62f);
                const float s = 0.40f * c.h * g;
                const cv::Point2f o = c.body;
                const cv::Scalar gold = c.accent, pale = lighten(c.accent, 0.5);
                pixLine(c.view, {o.x, o.y - s}, {o.x - s, o.y + s * 0.6f}, c.px, gold, fade, seed); // compasso
                pixLine(c.view, {o.x, o.y - s}, {o.x + s, o.y + s * 0.6f}, c.px, gold, fade, seed + 1);
                pixLine(c.view, {o.x - s, o.y - s * 0.3f}, {o.x, o.y + s}, c.px, pale, fade, seed + 2); // esquadro
                pixLine(c.view, {o.x + s, o.y - s * 0.3f}, {o.x, o.y + s}, c.px, pale, fade, seed + 3);
            }
            if (L == Layer::Front && t > 0.15f)
            {
                const float fade = seg(t, 0.45f, kCastAnimTime);
                for (int i = 0; i < 10; ++i)
                {
                    if (!visible(i, seed + frame, fade))
                        continue;
                    const float rise = std::fmod(t * 1.6f + hash01(i, seed), 1.f);
                    pix(c.view, c.body.x + hash11(i, seed + 5) * 0.32f * c.h,
                        c.feet.y - rise * c.h, c.px, i % 3 ? c.accent : cv::Scalar(255, 255, 255));
                }
            }
            break;

        case MoveStyle::Venegas: // linhas de velocidade atras da investida
            if (L == Layer::Back && t > 0.18f && t < 0.46f)
            {
                const float u = seg(t, 0.18f, 0.46f);
                for (int i = 0; i < 7; ++i)
                {
                    const float y = c.feet.y - c.h * (0.12f + 0.12f * i);
                    const float x0 = c.body.x - c.f * 0.15f * c.h;
                    const float len = (0.25f + 0.35f * hash01(i, seed)) * c.h * (1.f - u);
                    pixLine(c.view, {x0, y}, {x0 - c.f * len, y}, c.px,
                            i % 2 ? lighten(c.accent, 0.5) : cv::Scalar(70, 60, 210), u, seed + i);
                }
            }
            break;

        case MoveStyle::Baltar: // terra e folhas voando no pisao
            if (L == Layer::Front && t >= 0.40f)
            {
                const float u = seg(t, 0.40f, kCastAnimTime);
                const cv::Scalar green(70, 210, 80), brown(40, 90, 140);
                for (int i = 0; i < 9; ++i)
                    for (int s = -1; s <= 1; s += 2)
                    {
                        const int id = i * 2 + (s > 0);
                        if (!visible(id, seed, u * u))
                            continue;
                        const float vx = (0.25f + 0.5f * hash01(id, seed)) * c.h * I;
                        const float vy = -(0.30f + 0.40f * hash01(id, seed + 9)) * c.h * I;
                        const float x = c.pos.x + s * vx * u;
                        const float y = c.feet.y + vy * u + 0.9f * c.h * u * u;
                        if (y < c.feet.y)
                            pix(c.view, x, y, c.px, id % 3 ? green : brown, 2);
                    }
                dust(c.view, {c.pos.x, float(c.feet.y)}, u, 0.55f * c.h * I, c.px, seed);
            }
            break;

        case MoveStyle::Subramana: // aneis de mana se expandindo a partir do peito
            if (L == Layer::Back)
                for (int k = 0; k < 3; ++k)
                {
                    const float uk = seg(t, 0.15f + 0.10f * k, 0.50f + 0.10f * k);
                    if (uk <= 0.f || uk >= 1.f)
                        continue;
                    pixelRing(c.view, {c.body.x, c.body.y - 0.1f * c.h}, (0.10f + 0.48f * uk) * c.h * I, 1.f,
                              k % 2 ? lighten(c.accent, 0.4) : c.accent, c.px, uk, seed + k);
                }
            break;

        default:
            break;
        }

        // Comum a todos: faisca na mao no instante do golpe
        if (L == Layer::Front)
        {
            const cv::Point2f hand(c.body.x + c.f * 0.30f * c.h, c.body.y - 0.12f * c.h);
            burst(c.view, hand, seg(t, 0.18f, 0.38f), 0.20f * c.h * I, c.accent, c.px, seed + 31);
            if (type == SpellType::AtaqueEspecial) // super: grande explosao de energia no corpo
                burst(c.view, c.body, seg(t, 0.0f, 0.30f), 0.55f * c.h, lighten(c.accent, 0.3), c.px, seed + 57);
        }
    }

    // Olho que tudo ve: triangulo dourado com o olho no meio (blocos de pixel).
    void drawEyeGlyph(cv::Mat &v, cv::Point2f c, float s, int px, const cv::Scalar &gold, float fade, int seed)
    {
        const cv::Point2f top(c.x, c.y - s), bl(c.x - s, c.y + s * 0.75f), br(c.x + s, c.y + s * 0.75f);
        pixLine(v, top, bl, px, gold, fade, seed);
        pixLine(v, top, br, px, gold, fade, seed + 1);
        pixLine(v, bl, br, px, gold, fade, seed + 2);
        const cv::Scalar white(255, 255, 255), iris(60, 40, 20);
        const float ew = s * 0.45f, eh = s * 0.18f, ey = c.y + s * 0.15f;
        for (float x = -ew; x <= ew; x += px)
        {
            const float yy = eh * std::sqrt(std::max(0.f, 1.f - (x * x) / (ew * ew)));
            for (float y = -yy; y <= yy; y += px)
                pix(v, c.x + x, ey + y, px, std::hypot(x, y) < eh * 0.8f ? iris : white);
        }
        pix(v, c.x, ey - px, px, white); // brilho no olho
    }

    void superFx(const FxCtx &c, Layer L)
    {
        const float t = c.t;
        const int seed = c.side * 131 + 17;
        const int frame = int(t * kAnimFps);
        const bool charging = t < kSuperRelease;
        const float chargeU = seg(t, 0.f, kSuperRelease);
        const float fadeOut = seg(t, kSuperRecover, kSpecialAnimTime);
        const cv::Scalar white(255, 255, 255);

        // Comum: energia convergindo para o corpo durante a carga
        if (L == Layer::Front && charging)
            for (int i = 0; i < 14; ++i)
            {
                const float a = i * 2.f * kPi / 14.f + hash11(i, seed) * 0.4f;
                const float ph = std::fmod(t * 2.2f + hash01(i, seed + 1), 1.f);
                const float r = (1.f - ph) * 0.65f * c.h;
                pix(c.view, c.body.x + std::cos(a) * r, c.body.y + std::sin(a) * r * 0.8f, c.px,
                    i % 3 ? c.accent : white, ph > 0.7f ? 2 : 1);
            }

        switch (c.style)
        {
        case MoveStyle::Derzu: // chuva de codigo: colunas de blocos caindo
            if (L == Layer::Back && t < kSpecialAnimTime)
            {
                const cv::Scalar green(90, 230, 90), dark(40, 120, 40);
                for (int col = 0; col < 9; ++col)
                {
                    const float x = c.body.x + (col - 4) * 0.12f * c.h;
                    const float speed = 0.9f + 0.6f * hash01(col, seed);
                    const float head = std::fmod(t * speed * 1.6f + hash01(col, seed + 2), 1.f);
                    for (int k = 0; k < 6; ++k)
                    {
                        const float y = c.feet.y - c.h * 1.05f + (head - k * 0.05f) * c.h * 1.05f;
                        if (y < c.feet.y - c.h * 1.05f || !visible(col * 7 + k, seed + frame, fadeOut))
                            continue;
                        pix(c.view, x, y, c.px, k == 0 ? white : (k < 3 ? green : dark));
                    }
                }
                if (!charging)
                    pixelRing(c.view, {c.pos.x, c.feet.y - float(c.px)}, (0.3f + 0.3f * seg(t, 0.5f, 0.8f)) * c.h,
                              0.22f, c.accent, c.px, fadeOut, seed, t * 3.f);
            }
            break;

        case MoveStyle::Menegueti: // o Olho surge acima da cabeca
            if (L == Layer::Back)
            {
                const float g = easeOut(seg(t, 0.05f, 0.35f));
                drawEyeGlyph(c.view, {c.body.x, c.body.y - 0.15f * c.h}, 0.42f * c.h * g, c.px,
                             c.accent, fadeOut, seed);
            }
            break;

        case MoveStyle::Venegas: // estandarte: faixas azul/vermelha subindo + linhas de velocidade
            if (L == Layer::Back)
            {
                const cv::Scalar blue(200, 90, 40), red(50, 50, 220);
                for (int i = 0; i < 8; ++i)
                {
                    const float x = c.body.x - c.f * (0.25f + 0.04f * i) * c.h;
                    const float rise = std::fmod(t * 1.8f + hash01(i, seed), 1.f);
                    const float y = c.feet.y - rise * c.h;
                    if (visible(i, seed + frame, fadeOut))
                        pixLine(c.view, {x, y}, {x, y - 0.12f * c.h}, c.px, i % 2 ? blue : red);
                }
                if (!charging && t < kSuperRecover + 0.2f)
                    for (int i = 0; i < 6; ++i)
                    {
                        const float y = c.feet.y - c.h * (0.15f + 0.13f * i);
                        const float x0 = c.body.x - c.f * 0.15f * c.h;
                        pixLine(c.view, {x0, y}, {x0 - c.f * (0.3f + 0.3f * hash01(i, seed)) * c.h, y}, c.px,
                                lighten(c.accent, 0.5), seg(t, kSuperRelease, kSuperRecover + 0.2f), seed + i);
                    }
            }
            break;

        case MoveStyle::Neri: // falha digital: retangulos coloridos piscando
            if (L == Layer::Front && t < kSuperRecover)
            {
                const cv::Scalar cols[4] = {{255, 220, 60}, {200, 60, 230}, {60, 230, 255}, {80, 220, 90}};
                for (int i = 0; i < 7; ++i)
                {
                    if (hash01(i, frame + seed) < 0.45f)
                        continue;
                    const float x = c.body.x + hash11(i, frame + seed + 3) * 0.45f * c.h;
                    const float y = c.body.y + hash11(i, frame + seed + 5) * 0.5f * c.h;
                    const int w = c.px * (2 + int(hash01(i, frame + seed + 7) * 6));
                    cv::rectangle(c.view, cv::Rect(int(x), int(y), w, c.px * 2), cols[i % 4], cv::FILLED);
                }
            }
            break;

        case MoveStyle::Baltar: // trepadeiras crescendo do chao + folhas
            if (L == Layer::Back)
            {
                const cv::Scalar green(70, 200, 70), leaf(90, 235, 120);
                const float grow = easeOut(seg(t, 0.05f, 0.45f));
                for (int v = 0; v < 6; ++v)
                {
                    const float x0 = c.pos.x + (v - 2.5f) * 0.16f * c.h;
                    const float len = grow * (0.35f + 0.35f * hash01(v, seed)) * c.h;
                    for (float d = 0.f; d < len; d += c.px)
                    {
                        if (!visible(v * 50 + int(d), seed, fadeOut))
                            continue;
                        const float x = x0 + std::sin(d * 0.08f + v) * 0.04f * c.h;
                        pix(c.view, x, c.feet.y - d, c.px, green);
                        if (int(d / c.px) % 6 == 3)
                            pix(c.view, x + c.px, c.feet.y - d, c.px, leaf, 2);
                    }
                }
            }
            if (L == Layer::Front && t >= kSuperRelease + 0.2f) // pisao: terra voando
            {
                const float u = seg(t, kSuperRelease + 0.2f, kSpecialAnimTime);
                dust(c.view, {c.pos.x, float(c.feet.y)}, u, 0.7f * c.h, c.px, seed);
            }
            break;

        case MoveStyle::Subramana: // aneis encolhendo na carga, explodindo no disparo
            if (L == Layer::Back)
                for (int k = 0; k < 3; ++k)
                {
                    float r;
                    if (charging)
                        r = (0.7f - 0.55f * std::fmod(chargeU * 1.5f + k / 3.f, 1.f)) * c.h;
                    else
                        r = (0.15f + 0.8f * seg(t, kSuperRelease + 0.05f * k, kSuperRecover + 0.1f * k)) * c.h;
                    pixelRing(c.view, c.body, r, 1.f, k % 2 ? lighten(c.accent, 0.4) : c.accent, c.px,
                              charging ? 0.f : seg(t, kSuperRelease, kSpecialAnimTime), seed + k, t * (k + 1));
                }
            break;

        default:
            break;
        }

        // Comum: explosao na mao no disparo
        if (L == Layer::Front)
        {
            const cv::Point2f hand(c.body.x + c.f * 0.32f * c.h, c.body.y - 0.12f * c.h);
            burst(c.view, hand, seg(t, kSuperRelease, kSuperRelease + 0.25f), 0.45f * c.h,
                  lighten(c.accent, 0.3), c.px, seed + 57);
        }
    }

    void drawFx(const FxCtx &c, Layer L)
    {
        switch (c.mode)
        {
        case Mode::Super:
            superFx(c, L);
            break;
        case Mode::Hit:
        {
            const float k = 0.6f + 0.4f * c.st.hitPower;
            if (L == Layer::Front)
            {
                const cv::Point2f chest(c.body.x + c.f * 0.12f * c.h, c.body.y - 0.10f * c.h);
                burst(c.view, chest, seg(c.t, 0.f, 0.22f), 0.30f * c.h * k, cv::Scalar(90, 230, 255), c.px,
                      c.side * 7 + 3);
            }
            else
                dust(c.view, {c.pos.x - c.f * 0.1f * c.h, float(c.feet.y)}, seg(c.t, 0.02f, 0.35f),
                     0.30f * c.h * k, c.px, c.side * 11 + 5);
            break;
        }
        case Mode::Ko:
            if (L == Layer::Front)
                burst(c.view, c.body, seg(c.t, 0.f, 0.28f), 0.5f * c.h, cv::Scalar(90, 230, 255), c.px, c.side + 77);
            else
            {
                const cv::Point2f g(c.body.x, float(c.feet.y));
                dust(c.view, g, seg(c.t, 0.70f, 0.95f), 0.6f * c.h, c.px, c.side + 13);
                dust(c.view, g, seg(c.t, 1.00f, 1.25f), 0.4f * c.h, c.px, c.side + 29);
            }
            break;
        case Mode::Cast:
            castFx(c, L);
            break;
        case Mode::Victory:
            if (L == Layer::Back)
                dust(c.view, {c.pos.x, float(c.feet.y)}, seg(c.t, 0.42f, 0.70f), 0.25f * c.h, c.px, c.side + 41);
            break;
        default:
            break;
        }
    }

} // namespace

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

MoveStyle moveStyleFor(const std::string &id)
{
    if (id == "derzu")
        return MoveStyle::Derzu;
    if (id == "neri")
        return MoveStyle::Neri;
    if (id == "menegueti")
        return MoveStyle::Menegueti;
    if (id == "venegas")
        return MoveStyle::Venegas;
    if (id == "baltar")
        return MoveStyle::Baltar;
    if (id == "subramana" || id == "anand")
        return MoveStyle::Subramana;
    return MoveStyle::Padrao;
}

cv::Scalar moveAccent(MoveStyle s)
{
    switch (s)
    {
    case MoveStyle::Derzu:
        return cv::Scalar(230, 80, 170); // roxo do cajado
    case MoveStyle::Neri:
        return cv::Scalar(255, 210, 80); // ciano
    case MoveStyle::Menegueti:
        return cv::Scalar(70, 200, 245); // dourado
    case MoveStyle::Venegas:
        return cv::Scalar(230, 120, 60); // azul da farda
    case MoveStyle::Baltar:
        return cv::Scalar(80, 220, 90); // verde
    case MoveStyle::Subramana:
        return cv::Scalar(255, 235, 140); // ciano claro
    default:
        return cv::Scalar(200, 220, 255);
    }
}

float superDarken(const FighterAnim &st)
{
    if (st.koT >= 0.f || st.hitT >= 0.f || st.castType != SpellType::AtaqueEspecial ||
        st.castT < 0.f || st.castT > kSuperRecover)
        return 0.f;
    return 1.f - seg(stepT(st.castT), kSuperRelease, kSuperRecover);
}

cv::Point drawFighter(cv::Mat &view, const cv::Mat &spr, int side, cv::Point feet,
                      const FighterAnim &st, MoveStyle style, float idleTime, bool afterimages)
{
    if (spr.empty())
        return feet - cv::Point(0, 40);

    const int f = side == 0 ? 1 : -1;
    const float h = float(spr.rows);
    const float U = h / 200.f; // unidade de movimento proporcional ao sprite
    const int px = std::max(2, cvRound(h / 64.f));

    Mode mode = Mode::Idle;
    float t = 0.f;
    Pose p;
    if (st.koT >= 0.f)
    {
        mode = Mode::Ko;
        t = stepT(st.koT);
        p = koPose(t, U, h, koRest(spr, f));
    }
    else if (st.hitT >= 0.f)
    {
        mode = Mode::Hit;
        t = stepT(st.hitT);
        p = hitPose(t, st.hitPower, U);
    }
    else if (st.castT >= 0.f && st.castType == SpellType::AtaqueEspecial && st.castT < kSpecialAnimTime)
    {
        mode = Mode::Super;
        t = stepT(st.castT);
        p = superPose(style, t, U, h);
        p.red = 0.65f * st.hitFlash; // super armadura: apanha sem interromper
    }
    else if (st.castT >= 0.f && st.castT < castDuration(st.castType))
    {
        mode = Mode::Cast;
        t = stepT(st.castT);
        p = castPose(style, st.castType, t, U);
    }
    else if (st.victoryT >= 1.0f && st.victoryT < 1.7f)
    {
        mode = Mode::Victory; // pulinho de comemoracao
        t = stepT(st.victoryT - 1.0f);
        const float u = seg(t, 0.f, 0.45f);
        p.dy = -0.14f * h * arc(u);
        if (t >= 0.45f && t < 0.6f)
        {
            p.sy = 0.94f;
            p.sx = 1.04f;
        }
    }
    else // guarda: respiracao em 2 quadros, como a base de um jogo de luta
    {
        p.dy = (int(idleTime * 3.f) % 2) ? -std::max(1.f, std::round(1.5f * U)) : 0.f;
    }

    if (p.shake > 0.f)
    {
        const int tick = int(t * 30.f);
        p.dx += hash11(tick, side * 7 + 1) * p.shake;
        p.dy += hash11(tick, side * 7 + 2) * p.shake * 0.5f;
    }

    // mantem a cabeca dentro da arena (o nocaute pode sair: e um arremesso)
    if (mode != Mode::Ko)
    {
        const float top = feet.y + p.dy - h * p.sy;
        if (top < 1.f)
            p.dy += 1.f - top;
    }

    // centro do corpo na tela, ja com rotacao e escala
    const cv::Point2f pw = pivotWorld(p, f, feet, h);
    const Affine A = poseAffine(p, f);
    const float cy = (0.5f - p.pivot) * h;
    const cv::Point2f body(pw.x + A.b * cy, pw.y + A.d * cy);

    const FxCtx fx{view, mode, style, st, t, f, side, h, U, px, feet,
                   body, cv::Point2f(feet.x + f * p.dx, float(feet.y)), moveAccent(style)};
    drawFx(fx, Layer::Back);

    // imagens residuais: investida do Venegas
    if (afterimages && mode == Mode::Cast && st.castType != SpellType::Escudo && style == MoveStyle::Venegas)
    {
        for (int k = 3; k >= 1; --k)
        {
            const float tk = stepT(std::max(0.f, st.castT - 0.05f * k));
            const Pose q = castPose(style, st.castType, tk, U);
            drawPosed(view, spr, q, f, feet, fx.accent, 0.75f, 0.50f - 0.12f * k);
        }
    }

    if (mode == Mode::Super)
    {
        // aura: silhueta maior na cor do personagem, pulsando em 2 quadros (carga e disparo)
        if (t < kSuperRecover)
        {
            Pose aura = p;
            aura.sx *= 1.08f;
            aura.sy *= 1.06f;
            aura.red = aura.white = 0.f;
            const float a = (int(t * kAnimFps) % 2) ? 0.55f : 0.30f;
            drawPosed(view, spr, aura, f, feet, lighten(fx.accent, 0.2), 1.f, a);
        }
        // imagens residuais no disparo
        if (afterimages && t >= kSuperRelease && t < kSpecialAnimTime)
            for (int k = 3; k >= 1; --k)
            {
                const Pose q = superPose(style, stepT(std::max(0.f, st.castT - 0.05f * k)), U, h);
                drawPosed(view, spr, q, f, feet, fx.accent, 0.75f, 0.50f - 0.12f * k);
            }
        // Neri: falha digital com canais de cor deslocados
        if (style == MoveStyle::Neri && t < kSuperRecover && int(t * kAnimFps) % 3 != 2)
        {
            Pose g = p;
            g.red = g.white = 0.f;
            g.dx = p.dx + 4.f * U;
            drawPosed(view, spr, g, f, feet, cv::Scalar(255, 230, 0), 0.9f, 0.45f);
            g.dx = p.dx - 4.f * U;
            drawPosed(view, spr, g, f, feet, cv::Scalar(230, 0, 230), 0.9f, 0.45f);
        }
    }

    if (p.white > 0.f)
        drawPosed(view, spr, p, f, feet, cv::Scalar(255, 255, 255), 0.85f * p.white, 1.f);
    else
        drawPosed(view, spr, p, f, feet, cv::Scalar(40, 40, 255), 0.65f * p.red, 1.f);

    drawFx(fx, Layer::Front);
    return cv::Point(cvRound(body.x), cvRound(body.y));
}

void drawKOBanner(cv::Mat &view, float koT)
{
    if (koT < 0.10f || koT > 1.6f)
        return;
    const float t = stepT(koT);
    if (t > 1.3f && int(t * 10.f) % 2) // pisca antes de sumir
        return;

    // fonte 5x5 em blocos: K . O .
    struct Glyph
    {
        int w;
        const char *rows[5];
    };
    static const Glyph K = {5, {"X...X", "X..X.", "XXX..", "X..X.", "X...X"}};
    static const Glyph O = {5, {".XXX.", "X...X", "X...X", "X...X", ".XXX."}};
    static const Glyph D = {1, {".", ".", ".", ".", "X"}};
    const Glyph *text[4] = {&K, &D, &O, &D};

    const float pop = 1.f + (1.f - easeOut(seg(t, 0.10f, 0.25f))); // entra grande e assenta
    const int b = std::max(3, cvRound(view.rows / 22.f * pop));
    int totalW = 0;
    for (const Glyph *g : text)
        totalW += (g->w + 1) * b;
    totalW -= b;
    const int x0 = view.cols / 2 - totalW / 2;
    const int y0 = view.rows / 2 - 5 * b / 2;
    const int ol = std::max(1, b / 4);

    const cv::Scalar rowCol[5] = {{90, 245, 255}, {60, 225, 255}, {40, 190, 255}, {30, 140, 245}, {20, 100, 230}};
    for (int pass = 0; pass < 2; ++pass) // 0 = contorno/sombra, 1 = preenchimento
    {
        int x = x0;
        for (const Glyph *g : text)
        {
            for (int r = 0; r < 5; ++r)
                for (int col = 0; col < g->w; ++col)
                {
                    if (g->rows[r][col] != 'X')
                        continue;
                    const cv::Rect cell(x + col * b, y0 + r * b, b, b);
                    if (pass == 0)
                    {
                        cv::rectangle(view, cell + cv::Point(ol, ol), cv::Scalar(30, 0, 60), cv::FILLED);
                        cv::rectangle(view, cv::Rect(cell.x - ol, cell.y - ol, b + 2 * ol, b + 2 * ol),
                                      cv::Scalar(20, 10, 40), cv::FILLED);
                    }
                    else
                        cv::rectangle(view, cell, rowCol[r], cv::FILLED);
                }
            x += (g->w + 1) * b;
        }
    }
}