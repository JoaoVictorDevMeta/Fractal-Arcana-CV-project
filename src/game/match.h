#pragma once

// Ajustar futuramente
class Match
{
public:
    void update(double dt) { (void)dt; }

    // acessores usados pelo HUD (valores de 0 a 1)
    float vida01() const { return 1.f; }
    float mana01() const { return 1.f; }
};