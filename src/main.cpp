#include "engine.hpp"

int main()
{
    cpt::Engine engine("ComputePathTracer", 1080u, 720u);
    engine.run();

    return 0;
}