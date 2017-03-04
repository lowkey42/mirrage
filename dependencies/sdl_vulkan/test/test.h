#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

#define EXPECT(a, b, t) if (!COMP_##t(a, b)) { \
    fprintf(stderr, "Expected \"%s\", got \"%s\"\n", FMT_##t(b), FMT_##t(a)); \
    exit(1); \
}

#define FMT_STR(a) (a)
#define FMT_BOOL(a) (a ? "True" : "False")

#define COMP_STR(a, b) (strcmp(a, b) == 0)
#define COMP_EQ(a, b) (a == b)
#define COMP_BOOL COMP_EQ
