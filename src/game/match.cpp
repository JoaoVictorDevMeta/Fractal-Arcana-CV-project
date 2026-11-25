#include "game/match.h"
#include <algorithm>

bool Match::cast(int p, Shape s)
{
    if (state_ != MatchState::Running || !isCastable(s))
        return false;

    Player &me = players[p];
    const SpellDef &sp = spellFor(s);
    if (me.cooldown > 0.f || me.mana < sp.manaCost)
        return false;

    me.mana -= sp.manaCost;
    me.cooldown = kGlobalCooldown;
    me.castGlow = kCastGlowTime;
    me.castType = sp.type;
    me.castAnim = 0.f;

    if (sp.type == SpellType::Escudo)
    {
        me.shieldTime = sp.duration;
        me.shieldBlocks = sp.blocks;
    }
    else
    {
        proj_.push_back({p, sp.type, sp.damage, sp.travelTime, 0.f});
    }
    return true;
}

void Match::update(double dtd)
{
    const float dt = static_cast<float>(dtd);

    // o efeito de dano termina mesmo depois do golpe final
    // relogios das animacoes (continuam depois do golpe final, para o nocaute terminar)
    auto advance = [dt](float &timer, float maxT)
    {
        if (timer < 0.f)
            return;
        timer += dt;
        if (timer > maxT)
            timer = -1.f;
    };
    for (Player &pl : players)
    {
        pl.hitFlash = std::max(0.f, pl.hitFlash - dt);
        advance(pl.hitAnim, kHitAnimTime);
        advance(pl.castAnim, kCastAnimTime);
        if (pl.koTime >= 0.f)
            pl.koTime = std::min(kKoAnimMax, pl.koTime + dt);
    }

    if (state_ != MatchState::Running)
        return;

    time_ += dt;

    for (Player &pl : players)
    {
        pl.mana = std::min(kStartMana, pl.mana + kManaRegenPerS * dt);
        pl.cooldown = std::max(0.f, pl.cooldown - dt);
        pl.castGlow = std::max(0.f, pl.castGlow - dt);
        if (pl.shieldTime > 0.f)
        {
            pl.shieldTime -= dt;
            if (pl.shieldTime <= 0.f)
                pl.shieldBlocks = 0;
        }
    }

    for (std::size_t i = 0; i < proj_.size();)
    {
        proj_[i].t += dt / proj_[i].travelTime;
        if (proj_[i].t < 1.f)
        {
            ++i;
            continue;
        }

        Player &target = players[1 - proj_[i].owner];
        if (target.shieldBlocks > 0)
        {
            if (--target.shieldBlocks == 0)
                target.shieldTime = 0.f;
        }
        else
        {
            target.health -= proj_[i].damage;
            target.hitFlash = kHitFlashTime;
            target.hitAnim = 0.f;
            target.hitPower = std::clamp(proj_[i].damage / 40.f, 0.25f, 1.f);
            target.castAnim = -1.f; // levar o golpe interrompe o movimento especial
        }
        proj_.erase(proj_.begin() + i);
    }

    if (players[0].health <= 0.f)
    {
        state_ = MatchState::Player1Won;
        players[0].koTime = 0.f;
    }
    else if (players[1].health <= 0.f)
    {
        state_ = MatchState::Player0Won;
        players[1].koTime = 0.f;
    }
}