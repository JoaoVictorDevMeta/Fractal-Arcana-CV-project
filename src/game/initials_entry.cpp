#include "game/initials_entry.h"
#include "game/score_store.h"
#include "render/pixel_fx.h"
#include "render/ui_widgets.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

void InitialsEntry::start(const std::string &suggestion)
{
    state_ = State::Typing;
    typed_.clear();
    suggestion_ = ScoreStore::cleanId(suggestion);
    savedId_.clear();
}

void InitialsEntry::reset()
{
    state_ = State::Off;
    typed_.clear();
    savedId_.clear();
}

std::string InitialsEntry::id() const
{
    return ScoreStore::cleanId(typed_ + suggestion_.substr(std::min<size_t>(typed_.size(), 3)));
}

void InitialsEntry::markSaved(const std::string &id)
{
    state_ = State::Saved;
    savedId_ = id;
}

bool InitialsEntry::handleKey(int key, bool &confirmed)
{
    confirmed = false;
    if (state_ != State::Typing || key < 0)
        return false;
    key &= 0xFF;
    if (key == 13 || key == 10) // Enter
    {
        confirmed = true;
        return true;
    }
    if (key == 8 || key == 127) // Backspace
    {
        if (!typed_.empty())
            typed_.pop_back();
        return true;
    }
    if (key >= 'a' && key <= 'z')
        key = key - 'a' + 'A';
    if (key >= 'A' && key <= 'Z')
    {
        if (typed_.size() < 3)
            typed_ += char(key);
        return true;
    }
    return true; // durante o registro, as outras teclas (R, M, numeros...) nao fazem nada
}

void InitialsEntry::draw(cv::Mat &img, float time) const
{
    if (state_ == State::Off || img.empty())
        return;
    const int W = img.cols, H = img.rows;
    const cv::Scalar neon(255, 120, 230), gold(60, 215, 255), white(240, 240, 250), dim(120, 100, 130);
    const cv::Scalar shadow(30, 10, 40);
    const bool blink = int(time * 3.f) % 2 == 0;
    const int px = std::max(2, H / 150);

    auto text = [&](const std::string &s, int x, int y, const cv::Scalar &col, int p)
    {
        pfx::pixelText(img, s, {x + p / 2 + 1, y + p / 2 + 1}, p, shadow);
        pfx::pixelText(img, s, {x, y}, p, col);
    };
    auto centered = [&](const std::string &s, int y, const cv::Scalar &col, int p)
    { text(s, (W - pfx::pixelTextWidth(s, p)) / 2, y, col, p); };

    if (state_ == State::Saved)
    {
        const std::string msg = "COMBATE " + savedId_ + " SALVO!   M: MENU";
        const int w = pfx::pixelTextWidth(msg, px) + 8 * px;
        const cv::Rect r((W - w) / 2, H * 22 / 100, w, 9 * px);
        ui::fillTranslucent(img, r, cv::Scalar(8, 4, 12), 0.85);
        cv::rectangle(img, r, neon, std::max(1, px / 2));
        centered(msg, r.y + 2 * px, blink ? gold : white, px);
        return;
    }

    // caixa de registro
    const int big = px * 5;
    const int slotW = 5 * big;
    const int panelW = std::max(pfx::pixelTextWidth("DIGITE 3 LETRAS - ENTER SALVA", px) + 12 * px, 3 * slotW + 12 * px);
    const int panelH = 7 * px + 7 * big + 14 * px;
    const cv::Rect r((W - panelW) / 2, std::max(4, H * 40 / 100 - panelH / 2), panelW, panelH);
    ui::fillTranslucent(img, r, cv::Scalar(8, 4, 12), 0.92);
    cv::rectangle(img, r, neon, px);
    cv::rectangle(img, cv::Rect(r.x + 2 * px, r.y + 2 * px, r.width - 4 * px, r.height - 4 * px), neon * 0.45, 1);

    int y = r.y + 4 * px;
    centered("REGISTRE O COMBATE", y, gold, px);
    y += 8 * px;

    const std::string cur = id();
    const int x0 = (W - 3 * slotW) / 2 + big; // letras centralizadas nas casas
    for (int i = 0; i < 3; ++i)
    {
        const int x = x0 + i * slotW;
        const bool typedHere = i < int(typed_.size());
        const bool cursor = i == int(std::min<size_t>(typed_.size(), 2)) && typed_.size() < 3;
        text(std::string(1, cur[i]), x, y, typedHere ? white : dim, big);
        // traco embaixo da letra (pisca na casa atual)
        if (!cursor || blink)
            cv::rectangle(img, cv::Rect(x - px, y + 6 * big, 3 * big + 2 * px, px), cursor ? gold : neon * 0.6,
                          cv::FILLED);
    }
    y += 7 * big + 2 * px;
    centered("DIGITE 3 LETRAS - ENTER SALVA", y, white, px);
}
