#pragma once
#include <string>

// Ataque especial (forma: Estrela) de cada personagem.
// O custo de mana e o da Estrela em spellTable(); aqui ficam o nome, o efeito e os numeros
// proprios de cada mago. Todos tem uma "carga" (launchDelay) antes de soltar o golpe,
// sincronizada com a animacao de super.

enum class SpecialKind
{
    Padrao = 0,
    ChuvaDeCodigo,      // Derzu
    OlhoQueTudoVe,      // Menegueti
    CargaDoGeneral,     // Venegas: "La integral es la soma de las partes!"
    SobrecargaSistema,  // Neri: "Jesus Cristo!"
    FuriaDaFloresta,    // Baltar: "Furia de Maxwell"
    SingularidadeMana,  // Subramana
    Count
};

struct SpecialDef
{
    const char *name = "Explosao de Mana";
    const char *effect = "Projetil mais poderoso do jogo";
    float damage = 40.f;      // dano de CADA disparo
    float travelTime = 1.5f;  // segundos para cruzar a arena
    float launchDelay = 0.5f; // carga antes do disparo (s)
    int shots = 1;            // disparos em sequencia
    float shotGap = 0.f;      // intervalo entre disparos (s)
    bool pierceShield = false;
    float drainMana = 0.f;    // mana que o alvo perde ao ser atingido
    float healSelf = 0.f;     // vida que o lancador recupera ao acertar
    float refundMana = 0.f;   // mana que o lancador recupera ao acertar
};

constexpr int kSpecialCount = static_cast<int>(SpecialKind::Count);
constexpr float kSpecialAnimTime = 1.25f; // duracao da animacao de super (s)

inline const SpecialDef &specialDef(SpecialKind k)
{
    static const SpecialDef table[kSpecialCount] = {
        // nome, efeito, dano, travessia, carga, disparos, intervalo, perfura, drena, cura, devolve
        {"Explosao de Mana", "Projetil mais poderoso do jogo", 40.f, 1.5f, 0.5f, 1, 0.f, false, 0.f, 0.f, 0.f},
        {"Chuva de Codigo", "Bloco de codigo devastador", 40.f, 1.5f, 0.5f, 1, 0.f, false, 0.f, 0.f, 0.f},
        {"Olho que Tudo Ve", "Atravessa e destroi o escudo", 30.f, 1.4f, 0.5f, 1, 0.f, true, 0.f, 0.f, 0.f},
        // nomes sem acento: as fontes Hershey do OpenCV nao desenham caracteres acentuados
        {"La integral es la soma de las partes!", "5 partes de 9 de dano: a soma acerta 45", 9.f, 1.0f, 0.5f, 5, 0.12f, false, 0.f, 0.f, 0.f},
        {"Jesus Cristo!", "Cruz de luz: o alvo perde 35 de mana", 28.f, 1.3f, 0.5f, 1, 0.f, false, 35.f, 0.f, 0.f},
        {"Furia de Maxwell", "O tigre Maxwell ataca: recupera 15 de vida", 30.f, 1.8f, 0.5f, 1, 0.f, false, 0.f, 15.f, 0.f},
        {"Singularidade de Mana", "Absorve: devolve 35 de mana", 30.f, 1.5f, 0.5f, 1, 0.f, false, 0.f, 0.f, 35.f},
    };
    const int i = static_cast<int>(k);
    return table[(i >= 0 && i < kSpecialCount) ? i : 0];
}

inline SpecialKind specialFor(const std::string &characterId)
{
    if (characterId == "derzu")
        return SpecialKind::ChuvaDeCodigo;
    if (characterId == "menegueti")
        return SpecialKind::OlhoQueTudoVe;
    if (characterId == "venegas")
        return SpecialKind::CargaDoGeneral;
    if (characterId == "neri")
        return SpecialKind::SobrecargaSistema;
    if (characterId == "baltar")
        return SpecialKind::FuriaDaFloresta;
    if (characterId == "subramana" || characterId == "anand")
        return SpecialKind::SingularidadeMana;
    return SpecialKind::Padrao;
}
