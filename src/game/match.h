#pragma once
#include "game/spells.h"
#include "game/specials.h"
#include <string>
#include <vector>

constexpr float kCastGlowTime = 0.35f; // duracao (s) do brilho ao lancar feitico
constexpr float kHitFlashTime = 0.30f; // duracao (s) do flash vermelho/tremor ao levar dano
constexpr float kHitAnimTime = 0.50f;  // duracao (s) da animacao de dano (recuo)
constexpr float kCastAnimTime = 0.70f; // duracao (s) do golpe especial ao lancar feitico
constexpr float kKoAnimMax = 10.f;     // o relogio do nocaute para de contar depois disso
constexpr float kIntroTime = 1.8f;     // "PREPAREM-SE... LUTEM!" (sem feiticos ate acabar)
constexpr float kCombatTextLife = 1.2f;

// Estado de animacao de um mago, lido pelo render (tempos decorridos em s; < 0 = inativo).
struct FighterAnim
{
    float hitT = -1.f;     // desde o ultimo golpe recebido
    float hitPower = 0.f;  // 0..1, forca do golpe (define o recuo)
    float hitFlash = 0.f;  // 0..1, flash de dano (tambem quando o golpe nao interrompe)
    float castT = -1.f;    // desde o ultimo feitico lancado
    SpellType castType = SpellType::Nenhum;
    SpecialKind special = SpecialKind::Padrao; // especial deste mago
    float koT = -1.f;      // desde o nocaute (este mago perdeu)
    float victoryT = -1.f; // desde o nocaute do oponente (este mago venceu)
};

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
    float hitAnim = -1.f;                   // tempo desde o golpe (-1 = sem animacao)
    float hitPower = 0.f;                   // forca do ultimo golpe (0..1)
    float castAnim = -1.f;                  // tempo desde o lancamento (-1 = sem animacao)
    float koTime = -1.f;                    // tempo desde o nocaute (-1 = de pe)
    SpecialKind special = SpecialKind::Padrao;
};

struct Projectile
{
    int owner; // 0 = esquerda, 1 = direita
    SpellType type;
    float damage;
    float travelTime;
    float t = 0.f; // progresso de 0 a 1 (< 0 = ainda carregando, nao aparece)
    SpecialKind special = SpecialKind::Padrao; // so para AtaqueEspecial
};

// Texto flutuante sobre um mago ("-30", "ESCUDO!", "+15 VIDA"...).
enum class CombatTextKind
{
    Damage,
    Blocked,
    Heal,
    Mana,
    Drain,
    Pierce
};
struct CombatText
{
    int player;
    CombatTextKind kind;
    std::string text;
    float age = 0.f;
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
    // Recomeca a partida mantendo os especiais escolhidos.
    void reset()
    {
        const SpecialKind s0 = players[0].special, s1 = players[1].special;
        *this = Match();
        players[0].special = s0;
        players[1].special = s1;
    }
    void setSpecial(int p, SpecialKind k) { players[p].special = k; }
    SpecialKind special(int p) const { return players[p].special; }

    // acessores usados pelo HUD
    float health01(int p) const { return players[p].health / kStartHealth; }
    float mana01(int p) const { return players[p].mana / kStartMana; }
    float mana(int p) const { return players[p].mana; }
    float cooldown(int p) const { return players[p].cooldown; }
    bool hasShield(int p) const { return players[p].shieldBlocks > 0; }
    float castGlow01(int p) const { return players[p].castGlow / kCastGlowTime; }
    SpellType castType(int p) const { return players[p].castType; }
    float hitFlash01(int p) const { return players[p].hitFlash / kHitFlashTime; }
    FighterAnim anim(int p) const
    {
        const Player &me = players[p];
        FighterAnim a;
        a.hitT = me.hitAnim;
        a.hitPower = me.hitPower;
        a.hitFlash = me.hitFlash / kHitFlashTime;
        a.castT = me.castAnim;
        a.castType = me.castType;
        a.special = me.special;
        a.koT = me.koTime;
        a.victoryT = players[1 - p].koTime;
        return a;
    }
    const std::vector<Projectile> &projectiles() const { return proj_; }
    const std::vector<CombatText> &combatTexts() const { return texts_; }
    MatchState state() const { return state_; }
    float time() const { return time_; }
    // Pontuacao: 10 por ponto de dano, +50 por bloquear, +150 por especial que acerta,
    // +100 por perfurar escudo e, no nocaute, +1000 com bonus de tempo e o bonus de vida
    // do vencedor (kLifeBonusPerHp por ponto de vida restante; +500 extra com a vida cheia).
    static constexpr int kKoBonus = 1000;
    static constexpr int kLifeBonusPerHp = 25;
    static constexpr int kPerfectBonus = 500;
    int score(int p) const { return score_[p]; }
    int lastLifeBonus() const { return lastLifeBonus_; } // bonus de vida do ultimo K.O.
    int winner() const { return state_ == MatchState::Player0Won ? 0 : (state_ == MatchState::Player1Won ? 1 : -1); }
    bool inIntro() const { return state_ == MatchState::Running && time_ < kIntroTime; }

private:
    void hit_(const Projectile &pr);
    void say_(int p, CombatTextKind k, const std::string &txt) { texts_.push_back({p, k, txt, 0.f}); }

    float time_ = 0.f;
    Player players[2];
    std::vector<Projectile> proj_;
    std::vector<CombatText> texts_;
    MatchState state_ = MatchState::Running;
    int score_[2] = {0, 0};
    int lastLifeBonus_ = 0;
    void koBonus_(int winner);
};
