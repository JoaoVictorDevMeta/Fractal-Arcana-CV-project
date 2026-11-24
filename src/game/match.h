#pragma once
#include "game/spells.h"
#include <vector>

constexpr float kCastGlowTime = 0.35f; // duracao (s) do brilho ao lancar feitico
constexpr float kHitFlashTime = 0.30f; // duracao (s) do flash vermelho/tremor ao levar dano

struct Player
{
    float health = kStartHealth;
    float mana = kStartMana;
    float cooldown = 0.f;
    float shieldTime = 0.f;
    int shieldBlocks = 0;
    float castGlow = 0.f;                   // tempo restante do brilho de lancamento
    SpellType castType = SpellType::Nenhum; // ultimo feitico lancado (define a cor)
    float hitFlash = 0.f;                   // tempo restante do efeito de dano
};

struct Projectile
{
    int owner; // 0 = esquerda, 1 = direita
    SpellType type;
    float damage;
    float travelTime;
    float t = 0.f; // progresso de 0 a 1
};

enum class MatchState
{
    Running,
    Player0Won,
    Player1Won
};

class Match
{
public:
    void update(double dt);
    bool cast(int player, Shape s); // false se nao deu para lancar
    void reset() { *this = Match(); }

    // acessores usados pelo HUD
    float health01(int p) const { return players[p].health / kStartHealth; }
    float mana01(int p) const { return players[p].mana / kStartMana; }
    bool hasShield(int p) const { return players[p].shieldBlocks > 0; }
    float castGlow01(int p) const { return players[p].castGlow / kCastGlowTime; }
    SpellType castType(int p) const { return players[p].castType; }
    float hitFlash01(int p) const { return players[p].hitFlash / kHitFlashTime; }
    const std::vector<Projectile> &projectiles() const { return proj_; }
    MatchState state() const { return state_; }
    float time() const { return time_; }

private:
    float time_ = 0.f;
    Player players[2];
    std::vector<Projectile> proj_;
    MatchState state_ = MatchState::Running;
};