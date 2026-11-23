#pragma once
#include "gesture/shapes.h"
#include <array>
#include <cstddef>
#include <string>

enum class SpellType
{
    Nenhum = 0, // 0 de proposito: entrada vazia = nao lancavel
    AtaqueFraco,
    AtaqueMedio,
    AtaqueForte,
    Escudo,
    AtaqueEspecial,
};

struct SpellDef
{
    std::string name = "";
    SpellType type = SpellType::Nenhum;
    float manaCost = 0.f;

    // Projeteis
    float damage = 0.f;
    float travelTime = 0.f; // segundos para cruzar a tela

    // Escudo
    float duration = 0.f; // duracao do escudo
    int blocks = 0;       // feiticos que o escudo absorve
};

constexpr float kStartHealth = 100.f;
constexpr float kStartMana = 100.f;
constexpr float kManaRegenPerS = 8.f;   // mana por segundo
constexpr float kGlobalCooldown = 0.5f; // entre feiticos

constexpr std::size_t kShapeCount = static_cast<std::size_t>(Shape::Nenhum) + 1;
static_assert(static_cast<std::size_t>(Shape::Nenhum) == 5,
              "Shape::Nenhum deve ser o ultimo; atualize spellTable()");

// A ORDEM segue o enum Shape: Linha, Raio, Circulo, Triangulo, Estrela, Nenhum.
inline std::array<SpellDef, kShapeCount> &spellTable()
{
    static std::array<SpellDef, kShapeCount> table = {{
        //  nome                tipo            mana  dano  travessia  dur  blocks
        {"Bola de fogo", SpellType::AtaqueFraco, 8.f, 6.f, 1.6f, 0.f, 0},          // Linha
        {"Relampago", SpellType::AtaqueMedio, 30.f, 14.f, 0.5f, 0.f, 0},           // Raio
        {"Escudo", SpellType::Escudo, 20.f, 0.f, 0.f, 4.f, 1},                     // Circulo
        {"Chuva de pedras", SpellType::AtaqueForte, 45.f, 24.f, 2.4f, 0.f, 0},     // Triangulo
        {"Explosao de Mana", SpellType::AtaqueEspecial, 80.f, 40.f, 1.5f, 0.f, 0}, // Estrela
        {}                                                                         // Nenhum
    }};
    return table;
}

inline const SpellDef &spellFor(Shape s)
{
    return spellTable()[static_cast<std::size_t>(s)];
}

inline bool isCastable(Shape s)
{
    return spellFor(s).type != SpellType::Nenhum;
}