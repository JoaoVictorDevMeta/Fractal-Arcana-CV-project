#include "game/match.h"
#include <algorithm>

bool Match::cast(int p, Shape s)
{
    if (state_ != MatchState::Running || inIntro() || !isCastable(s))
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
    else if (sp.type == SpellType::AtaqueEspecial)
    {
        // Especial do personagem: carrega (t < 0) e solta um ou mais disparos.
        // O tempo de carga em segundos vira progresso negativo: t = -atraso / travessia.
        const SpecialDef &sd = specialDef(me.special);
        me.cooldown = std::max(me.cooldown, sd.launchDelay + sd.shotGap * (sd.shots - 1));
        for (int i = 0; i < sd.shots; ++i)
        {
            const float delay = sd.launchDelay + sd.shotGap * i;
            proj_.push_back({p, sp.type, sd.damage, sd.travelTime, -delay / sd.travelTime, me.special});
        }
    }
    else
    {
        proj_.push_back({p, sp.type, sp.damage, sp.travelTime, 0.f});
    }
    return true;
}

void Match::hit_(const Projectile &pr)
{
    Player &target = players[1 - pr.owner];
    Player &caster = players[pr.owner];
    const bool special = pr.type == SpellType::AtaqueEspecial;
    const SpecialDef &sd = specialDef(pr.special);

    if (target.shieldBlocks > 0)
    {
        if (special && sd.pierceShield)
        {
            target.shieldBlocks = 0; // o Olho ve atraves: destroi o escudo e acerta
            target.shieldTime = 0.f;
            say_(1 - pr.owner, CombatTextKind::Pierce, "PERFURADO!");
        }
        else
        {
            if (--target.shieldBlocks == 0)
                target.shieldTime = 0.f;
            say_(1 - pr.owner, CombatTextKind::Blocked, "BLOQUEADO!");
            score_[1 - pr.owner] += 50;
            return;
        }
        score_[pr.owner] += 100; // perfurou o escudo
    }

    const float dealt = std::min(pr.damage, std::max(0.f, target.health));
    score_[pr.owner] += int(dealt * 10.f + 0.5f) + (special ? 150 : 0);
    target.health -= pr.damage;
    target.hitFlash = kHitFlashTime;
    say_(1 - pr.owner, CombatTextKind::Damage, "-" + std::to_string(int(pr.damage + 0.5f)));

    // Super armadura: durante a carga do especial o golpe nao interrompe a animacao
    const bool armored = target.castType == SpellType::AtaqueEspecial && target.castAnim >= 0.f &&
                         target.castAnim < specialDef(target.special).launchDelay + 0.3f;
    if (!armored)
    {
        target.hitAnim = 0.f;
        target.hitPower = std::clamp(pr.damage / 40.f, 0.25f, 1.f);
        target.castAnim = -1.f; // levar o golpe interrompe o movimento
    }

    if (!special)
        return;
    if (sd.drainMana > 0.f)
    {
        target.mana = std::max(0.f, target.mana - sd.drainMana);
        say_(1 - pr.owner, CombatTextKind::Drain, "-" + std::to_string(int(sd.drainMana)) + " MANA");
    }
    if (sd.healSelf > 0.f && caster.health > 0.f)
    {
        caster.health = std::min(kStartHealth, caster.health + sd.healSelf);
        say_(pr.owner, CombatTextKind::Heal, "+" + std::to_string(int(sd.healSelf)) + " VIDA");
    }
    if (sd.refundMana > 0.f)
    {
        caster.mana = std::min(kStartMana, caster.mana + sd.refundMana);
        say_(pr.owner, CombatTextKind::Mana, "+" + std::to_string(int(sd.refundMana)) + " MANA");
    }
}

void Match::update(double dtd)
{
    const float dt = static_cast<float>(dtd);

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
        advance(pl.castAnim, pl.castType == SpellType::AtaqueEspecial ? kSpecialAnimTime : kCastAnimTime);
        if (pl.koTime >= 0.f)
            pl.koTime = std::min(kKoAnimMax, pl.koTime + dt);
    }
    for (CombatText &ct : texts_)
        ct.age += dt;
    texts_.erase(std::remove_if(texts_.begin(), texts_.end(),
                                [](const CombatText &ct) { return ct.age > kCombatTextLife; }),
                 texts_.end());

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
        const Projectile pr = proj_[i];
        proj_.erase(proj_.begin() + i);
        hit_(pr);
    }

    if (players[0].health <= 0.f)
    {
        state_ = MatchState::Player1Won;
        players[0].koTime = 0.f;
        koBonus_(1);
    }
    else if (players[1].health <= 0.f)
    {
        state_ = MatchState::Player0Won;
        players[1].koTime = 0.f;
        koBonus_(0);
    }
}

void Match::koBonus_(int w)
{
    int bonus = 1000;
    bonus += std::max(0, int(90.f - time_)) * 20; // vitoria rapida
    if (players[w].health >= kStartHealth)
        bonus += 500; // vitoria perfeita
    score_[w] += bonus;
    say_(w, CombatTextKind::Heal, "+" + std::to_string(bonus) + " K.O.!");
}
