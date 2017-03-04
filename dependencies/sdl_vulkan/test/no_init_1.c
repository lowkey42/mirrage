#include "test.h"

int main() {
    EXPECT(SDL_GetVulkanInstanceExtensions(0, 0), SDL_FALSE, BOOL);
    EXPECT(SDL_GetError(), "No video driver - has SDL_Init(SDL_INIT_VIDEO) been called?", STR);
    return 0;
}
