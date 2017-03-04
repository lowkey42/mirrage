#include "test.h"

int main() {
    EXPECT(SDL_CreateVulkanSurface(0, VK_NULL_HANDLE, 0), SDL_FALSE, BOOL);
    EXPECT(SDL_GetError(), "'window' is null", STR);
    return 0;
}
