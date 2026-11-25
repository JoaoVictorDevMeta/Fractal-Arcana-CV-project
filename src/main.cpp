// Fractal Arcana - duelo de magos controlado por visao computacional (C++17 + OpenCV).
//
// Uso: duelo_magos [--camera N]
//
// Toda a logica fica em App (src/core/app.*): menu, duelo, placar e teclado.

#include "core/app.h"
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv)
{
    AppOptions opt;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--camera") && i + 1 < argc)
            opt.camera = std::atoi(argv[++i]);

    App app(opt);
    if (!app.init())
        return 2;
    return app.run();
}
