#include <SDL2/SDL.h>
#include <math.h>

typedef struct
{
    float x;
    float y;
}
Point;

static Point turn(const Point a, const float t)
{
    const Point b = {
        a.x * cosf(t) - a.y * sinf(t),
        a.x * sinf(t) + a.y * cosf(t),
    };
    return b;
}

// Right angle rotation.
static Point rag(const Point a)
{
    const Point b = {
        -a.y,
        +a.x,
    };
    return b;
}

static Point sub(const Point a, const Point b)
{
    const Point c = {
        a.x - b.x,
        a.y - b.y,
    };
    return c;
}

static Point add(const Point a, const Point b)
{
    const Point c = {
        a.x + b.x,
        a.y + b.y,
    };
    return c;
}

static Point mul(const Point a, const float n)
{
    const Point b = {
        a.x * n,
        a.y * n,
    };
    return b;
}

static float mag(const Point a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Point dvd(const Point a, const float n)
{
    const Point b = {
        a.x / n,
        a.y / n,
    };
    return b;
}

static Point unt(const Point a)
{
    return dvd(a, mag(a));
}

static float slope(const Point a)
{
    return a.y / a.x;
}

// Fast floor (math).
static int fl(const float x)
{
    return (int) x - (x < (int) x);
}

// Fast ceil (math).
static int cl(const float x)
{
    return (int) x + (x > (int) x);
}

// Steps horizontally along a grid.
static Point sh(const Point a, const Point b)
{
    const float x = b.x > 0.0f ? fl(a.x + 1.0f) : cl(a.x - 1.0f);
    const float y = slope(b) * (x - a.x) + a.y;
    const Point c = { x, y };
    return c;
}

// Steps vertically along a grid.
static Point sv(const Point a, const Point b)
{
    const float y = b.y > 0.0f ? fl(a.y + 1.0f) : cl(a.y - 1.0f);
    const float x = (y - a.y) / slope(b) + a.x;
    const Point c = { x, y };
    return c;
}

static Point cmp(const Point a, const Point b, const Point c)
{
    return mag(sub(b, a)) < mag(sub(c, a)) ? b : c;
}

static char tile(const Point a, const char** const tiles)
{
    const int x = a.x;
    const int y = a.y;
    return tiles[y][x] - '0';
}

typedef struct
{
    char tile;
    Point where;
}
Hit;

// Floating point decimal.
static float dec(const float x)
{
    return x - (int) x;
}

static Hit cast(const Point where, const Point direction, const char** const walling)
{
    const Point ray = cmp(where, sh(where, direction), sv(where, direction));
    const Point delta = mul(direction, 0.01f);
    const Point dx = { delta.x, 0.0f };
    const Point dy = { 0.0f, delta.y };
    const Point test = add(ray,
        dec(ray.x) == 0.0f && dec(ray.y) == 0.0f ? delta :
        dec(ray.x) == 0.0f ? dx : dy);
    const Hit hit = { tile(test, walling), test };
    return hit.tile ? hit : cast(ray, direction, walling);
}

typedef struct
{
    Point a;
    Point b;
}
Line;

// Party casting (flooring and ceiling)
static float pcast(const float size, const int yres, const int y)
{
    return size / (2 * (y + 1) - yres);
}

static Line rotate(const Line l, const float t)
{
    const Line line = {
        turn(l.a, t),
        turn(l.b, t),
    };
    return line;
}

// Linear interpolation.
static Point lerp(const Line l, const float n)
{
    return add(l.a, mul(sub(l.b, l.a), n));
}

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int xres;
    int yres;
}
Gpu;

static Gpu setup(const int xres, const int yres)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* const window = SDL_CreateWindow("littlewolf", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, xres, yres, SDL_WINDOW_SHOWN);
    SDL_Renderer* const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // Notice the flip between xres and yres. The texture is 90 degrees flipped on its side for fast cache access.
    SDL_Texture* const texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, yres, xres);
    const Gpu gpu = { window, renderer, texture, xres, yres };
    return gpu;
}

static void release(const Gpu gpu)
{
    SDL_DestroyWindow(gpu.window);
    SDL_DestroyRenderer(gpu.renderer);
    SDL_DestroyTexture(gpu.texture);
    SDL_Quit();
}

static void present(const Gpu gpu)
{
    const SDL_Rect dst = {
        (gpu.xres - gpu.yres) / 2,
        (gpu.yres - gpu.xres) / 2,
        gpu.yres,
        gpu.xres,
    };
    SDL_RenderCopyEx(gpu.renderer, gpu.texture, NULL, &dst, -90, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(gpu.renderer);
}

typedef struct
{
    uint32_t* pixels;
    int width;
}
Display;

static Display lock(const Gpu gpu)
{
    void* screen;
    int pitch;
    SDL_LockTexture(gpu.texture, NULL, &screen, &pitch);
    const Display display = { (uint32_t*) screen, pitch / (int) sizeof(uint32_t) };
    return display;
}

static void put(const Display display, const int x, const int y, const uint32_t pixel)
{
    display.pixels[y + x * display.width] = pixel;
}

static void unlock(const Gpu gpu)
{
    SDL_UnlockTexture(gpu.texture);
}

typedef struct
{
    int top;
    int bot;
    float size;
}
Wall;

static Wall project(const int xres, const int yres, const Line fov, const Point corrected)
{
    const float size = 0.5f * fov.a.x * xres / (corrected.x < 1e-2f ? 1e-2f : corrected.x);
    const int top = (yres + size) / 2.0f;
    const int bot = (yres - size) / 2.0f;
    const Wall wall = { top > yres ? yres : top, bot < 0 ? 0 : bot, size };
    return wall;
}

typedef struct
{
    Line fov;
    Point where;
    Point velocity;
    float speed;
    float acceleration;
    float theta;
}
Hero;

static Hero spin(Hero hero, const uint8_t* key)
{
    if(key[SDL_SCANCODE_H]) hero.theta -= 0.1f;
    if(key[SDL_SCANCODE_L]) hero.theta += 0.1f;
    return hero;
}

static Hero move(Hero hero, const char** const walling, const uint8_t* key)
{
    const Point last = hero.where, zero = { 0.0f, 0.0f };
    // Accelerates if <WASD>.
    if(key[SDL_SCANCODE_W] || key[SDL_SCANCODE_S] || key[SDL_SCANCODE_D] || key[SDL_SCANCODE_A])
    {
        const Point reference = { 1.0f, 0.0f };
        const Point direction = turn(reference, hero.theta);
        const Point acceleration = mul(direction, hero.acceleration);
        if(key[SDL_SCANCODE_W]) hero.velocity = add(hero.velocity, acceleration);
        if(key[SDL_SCANCODE_S]) hero.velocity = sub(hero.velocity, acceleration);
        if(key[SDL_SCANCODE_D]) hero.velocity = add(hero.velocity, rag(acceleration));
        if(key[SDL_SCANCODE_A]) hero.velocity = sub(hero.velocity, rag(acceleration));
    }
    // Otherwise, decelerates (exponential decay).
    else hero.velocity = mul(hero.velocity, 1.0f - hero.acceleration / hero.speed);
    // Caps velocity if top speed is exceeded.
    if(mag(hero.velocity) > hero.speed) hero.velocity = mul(unt(hero.velocity), hero.speed);
    // Moves
    hero.where = add(hero.where, hero.velocity);
    // Sets velocity to zero if there is a collision and puts hero back in bounds.
    if(tile(hero.where, walling))
    {
        hero.velocity = zero;
        hero.where = last;
    }
    return hero;
}

typedef struct
{
    const char** ceiling;
    const char** walling;
    const char** floring;
}
Map;

static uint32_t color(const char tile)
{
    return 0xAA << (8 * (tile - 1));
}

static void render(const Hero hero, const Map map, const Gpu gpu)
{
    const int t0 = SDL_GetTicks();
    const Line camera = rotate(hero.fov, hero.theta);
    const Display display = lock(gpu);
    for(int x = 0; x < gpu.xres; x++)
    {
        // Casts a ray.
        const Point column = lerp(camera, x / (float) gpu.xres);
        const Hit hit = cast(hero.where, column, map.walling);
        const Point ray = sub(hit.where, hero.where);
        const Point corrected = turn(ray, -hero.theta);
        const Wall wall = project(gpu.xres, gpu.yres, hero.fov, corrected);
        const Line trace = { hero.where, hit.where };
        // Renders flooring.
        for(int y = 0; y < wall.bot; y++)
            put(display, x, y, color(tile(lerp(trace, -pcast(wall.size, gpu.yres, y)), map.floring)));
        // Renders wall.
        for(int y = wall.bot; y < wall.top; y++)
            put(display, x, y, color(hit.tile));
        // Renders ceiling.
        for(int y = wall.top; y < gpu.xres; y++)
            put(display, x, y, color(tile(lerp(trace, +pcast(wall.size, gpu.yres, y)), map.ceiling)));
    }
    unlock(gpu);
    present(gpu);
    // Caps frame rate.
    const int t1 = SDL_GetTicks();
    const int ms = 15 - (t1 - t0);
    SDL_Delay(ms < 0 ? 0 : ms);
}

static int done()
{
    SDL_Event event;
    SDL_PollEvent(&event);
    return event.type == SDL_QUIT
        || event.key.keysym.sym == SDLK_END
        || event.key.keysym.sym == SDLK_ESCAPE;
}

static Line viewport(const float focal)
{
    const Line fov = {
        { focal, -1.0f },
        { focal, +1.0f },
    };
    return fov;
}

static Hero born()
{
    const Hero hero = {
        viewport(1.0f),
        // Where.
        { 3.5f, 3.5f },
        // Velocity.
        { 0.0f, 0.0f },
        // Speed.
        0.10f,
        // Acceleration.
        0.01f,
        // Theta (Radians).
        0.0f
    };
    return hero;
}

static Map build()
{
    // Note the static. Map lives in .bss in private.
    static const char* ceiling[] = {
        "111111111111111111111111111111111111111111111",
        "122223223232232111111111111111222232232322321",
        "122222221111232111111111111111222222211112321",
        "122221221232323232323232323232222212212323231",
        "122222221111232111111111111111222222211112321",
        "122223223232232111111111111111222232232322321",
        "111111111111111111111111111111111111111111111",
    };
    static const char* walling[] = {
        "111111111111111111111111111111111111111111111",
        "100000000000000111111111111111000000000000001",
        "103330001111000111111111111111033300011110001",
        "103000000000000000000000000000030000030000001",
        "103330001111000111111111111111033300011110001",
        "100000000000000111111111111111000000000000001",
        "111111111111111111111111111111111111111111111",
    };
    static const char* floring[] = {
        "111111111111111111111111111111111111111111111",
        "122223223232232111111111111111222232232322321",
        "122222221111232111111111111111222222211112321",
        "122222221232323323232323232323222222212323231",
        "122222221111232111111111111111222222211112321",
        "122223223232232111111111111111222232232322321",
        "111111111111111111111111111111111111111111111",
    };
    const Map map = { ceiling, walling, floring };
    return map;
}

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    const Gpu gpu = setup(500, 500);
    const Map map = build();
    Hero hero = born();
    while(!done())
    {
        const uint8_t* key = SDL_GetKeyboardState(NULL);
        hero = spin(hero, key);
        hero = move(hero, map.walling, key);
        render(hero, map, gpu);
    }
    release(gpu);
    return 0;
}
