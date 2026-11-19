#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>

// Resoluciones de pantalla Nintendo 3DS
#define TOP_WIDTH  400
#define TOP_HEIGHT 240
#define BOT_WIDTH  320
#define BOT_HEIGHT 240

// Constantes de física calibradas a 60 FPS (Bloque = 24x24 px)
constexpr float BLOCK_SIZE      = 24.0f;
constexpr float GROUND_Y        = 192.0f;
constexpr float CEILING_Y       = 24.0f;
constexpr float SCROLL_SPEED    = 5.18f;  // Velocidad 1x original escalada a 24px/bloque
constexpr float GRAVITY_CUBE    = 0.86f;
constexpr float JUMP_FORCE      = -10.85f; // Permite saltar triple spike con precisión
constexpr float PAD_FORCE       = -14.8f;
constexpr float ORB_FORCE       = -10.6f;
constexpr float SHIP_GRAVITY    = 0.34f;
constexpr float SHIP_THRUST     = -0.72f;
constexpr float MAX_FALL_SPEED  = 11.5f;

enum GameMode {
    MODE_CUBE,
    MODE_SHIP
};

enum ObjectType {
    OBJ_BLOCK = 1,
    OBJ_SPIKE = 2,
    OBJ_PAD_YELLOW = 3,
    OBJ_ORB_YELLOW = 4,
    OBJ_PORTAL_SHIP = 5,
    OBJ_PORTAL_CUBE = 6,
    OBJ_PORTAL_GRAV_UP = 7,
    OBJ_PORTAL_GRAV_DOWN = 8,
    OBJ_SLAB = 9
};

struct LevelObject {
    float x;      // Posición en bloques
    float y;      // Altura en bloques desde el suelo (0 = sobre el piso)
    ObjectType type;
    bool triggered = false;
};

struct Checkpoint {
    float playerX;
    float playerY;
    float velY;
    GameMode mode;
    int gravDir;
};

struct LevelData {
    std::string name;
    int stars;
    u32 bgColor;
    u32 groundColor;
    std::vector<LevelObject> objects;
    float lengthBlocks;
};

// Estado global del jugador
struct Player {
    float x = 0.0f;
    float y = GROUND_Y - BLOCK_SIZE;
    float velY = 0.0f;
    float rotation = 0.0f;
    bool onGround = true;
    GameMode mode = MODE_CUBE;
    int gravDir = 1; // 1 = normal, -1 = invertida
    bool dead = false;
    int attempts = 1;
    bool orbUsedThisPress = false;
};

std::vector<LevelData> levels;
int currentLevelIdx = 0;
bool inMenu = true;
bool practiceMode = false;
std::vector<Checkpoint> checkpoints;

// Colores personalizables
u32 primaryColors[] = {
    C2D_Color32(0, 255, 50, 255),
    C2D_Color32(0, 200, 255, 255),
    C2D_Color32(255, 220, 0, 255),
    C2D_Color32(255, 60, 60, 255),
    C2D_Color32(200, 50, 255, 255)
};
u32 secondaryColors[] = {
    C2D_Color32(0, 220, 255, 255),
    C2D_Color32(255, 255, 255, 255),
    C2D_Color32(255, 100, 0, 255),
    C2D_Color32(50, 255, 100, 255),
    C2D_Color32(255, 50, 180, 255)
};
int pColorIdx = 0;
int sColorIdx = 0;

// Generador de los 7 niveles clásicos de la v1.0 con sus mecánicas distintivas
void initLevels() {
    levels.clear();

    // 1. Stereo Madness (Cubo + Nave básica + Triple Spike final)
    {
        LevelData lvl;
        lvl.name = "Stereo Madness";
        lvl.stars = 1;
        lvl.bgColor = C2D_Color32(20, 65, 180, 255);
        lvl.groundColor = C2D_Color32(10, 40, 130, 255);
        lvl.objects = {
            {12, 0, OBJ_SPIKE},
            {22, 0, OBJ_SPIKE}, {23, 0, OBJ_SPIKE},
            {34, 0, OBJ_BLOCK}, {38, 0, OBJ_BLOCK}, {38, 1, OBJ_BLOCK},
            {42, 0, OBJ_BLOCK}, {42, 1, OBJ_BLOCK}, {42, 2, OBJ_BLOCK},
            {52, 0, OBJ_SPIKE}, {53, 0, OBJ_SPIKE},
            // Sección Nave
            {65, 2, OBJ_PORTAL_SHIP},
            {75, 0, OBJ_BLOCK}, {75, 1, OBJ_BLOCK}, {75, 5, OBJ_BLOCK}, {75, 6, OBJ_BLOCK},
            {88, 0, OBJ_BLOCK}, {88, 1, OBJ_BLOCK}, {88, 2, OBJ_BLOCK}, {88, 6, OBJ_BLOCK},
            {100, 0, OBJ_BLOCK}, {100, 4, OBJ_BLOCK}, {100, 5, OBJ_BLOCK}, {100, 6, OBJ_BLOCK},
            // Regreso a Cubo + Triple Spike clásico
            {112, 2, OBJ_PORTAL_CUBE},
            {124, 0, OBJ_SPIKE}, {125, 0, OBJ_SPIKE},
            {138, 0, OBJ_SPIKE}, {139, 0, OBJ_SPIKE}, {140, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 152.0f;
        levels.push_back(lvl);
    }

    // 2. Back on Track (Introduce Jump Pads amarillos)
    {
        LevelData lvl;
        lvl.name = "Back on Track";
        lvl.stars = 2;
        lvl.bgColor = C2D_Color32(130, 25, 160, 255);
        lvl.groundColor = C2D_Color32(90, 15, 115, 255);
        lvl.objects = {
            {12, 0, OBJ_PAD_YELLOW},
            {16, 0, OBJ_BLOCK}, {16, 1, OBJ_BLOCK}, {16, 2, OBJ_BLOCK},
            {17, 0, OBJ_BLOCK}, {17, 1, OBJ_BLOCK}, {17, 2, OBJ_BLOCK},
            {25, 0, OBJ_SPIKE}, {26, 0, OBJ_SPIKE},
            {34, 0, OBJ_PAD_YELLOW},
            {39, 3, OBJ_BLOCK}, {40, 3, OBJ_BLOCK}, {41, 3, OBJ_BLOCK},
            {48, 0, OBJ_SPIKE}, {49, 0, OBJ_SPIKE}, {50, 0, OBJ_SPIKE},
            {60, 2, OBJ_PORTAL_SHIP},
            {72, 1, OBJ_BLOCK}, {72, 2, OBJ_BLOCK}, {72, 6, OBJ_BLOCK},
            {85, 0, OBJ_BLOCK}, {85, 4, OBJ_BLOCK}, {85, 5, OBJ_BLOCK},
            {98, 2, OBJ_PORTAL_CUBE},
            {110, 0, OBJ_PAD_YELLOW},
            {115, 2, OBJ_BLOCK}, {116, 2, OBJ_BLOCK},
            {126, 0, OBJ_SPIKE}, {127, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 140.0f;
        levels.push_back(lvl);
    }

    // 3. Polargeist (Introduce Orbes amarillos)
    {
        LevelData lvl;
        lvl.name = "Polargeist";
        lvl.stars = 3;
        lvl.bgColor = C2D_Color32(20, 145, 95, 255);
        lvl.groundColor = C2D_Color32(10, 100, 65, 255);
        lvl.objects = {
            {14, 0, OBJ_SPIKE}, {15, 0, OBJ_SPIKE}, {16, 1, OBJ_ORB_YELLOW},
            {17, 0, OBJ_SPIKE}, {18, 0, OBJ_SPIKE},
            {28, 0, OBJ_BLOCK}, {28, 1, OBJ_BLOCK},
            {32, 2, OBJ_ORB_YELLOW},
            {36, 3, OBJ_BLOCK}, {37, 3, OBJ_BLOCK},
            {46, 0, OBJ_SPIKE}, {47, 0, OBJ_SPIKE}, {48, 0, OBJ_SPIKE},
            {58, 2, OBJ_PORTAL_SHIP},
            {70, 0, OBJ_BLOCK}, {70, 1, OBJ_BLOCK}, {70, 5, OBJ_BLOCK},
            {84, 2, OBJ_PORTAL_CUBE},
            {95, 1, OBJ_ORB_YELLOW}, {96, 0, OBJ_SPIKE}, {97, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 112.0f;
        levels.push_back(lvl);
    }

    // 4. Dry Out (Introduce Portales de Gravedad Invertida)
    {
        LevelData lvl;
        lvl.name = "Dry Out";
        lvl.stars = 4;
        lvl.bgColor = C2D_Color32(175, 45, 45, 255);
        lvl.groundColor = C2D_Color32(125, 25, 25, 255);
        lvl.objects = {
            {14, 0, OBJ_SPIKE}, {15, 0, OBJ_SPIKE},
            {25, 1, OBJ_PORTAL_GRAV_UP},
            {36, 6, OBJ_SPIKE}, {37, 6, OBJ_SPIKE},
            {48, 6, OBJ_BLOCK}, {52, 6, OBJ_BLOCK}, {52, 5, OBJ_BLOCK},
            {64, 5, OBJ_PORTAL_GRAV_DOWN},
            {75, 0, OBJ_SPIKE}, {76, 0, OBJ_SPIKE}, {77, 0, OBJ_SPIKE},
            {88, 1, OBJ_ORB_YELLOW}, {89, 0, OBJ_SPIKE}, {90, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 105.0f;
        levels.push_back(lvl);
    }

    // 5. Base After Base
    {
        LevelData lvl;
        lvl.name = "Base After Base";
        lvl.stars = 5;
        lvl.bgColor = C2D_Color32(25, 110, 175, 255);
        lvl.groundColor = C2D_Color32(15, 75, 125, 255);
        lvl.objects = {
            {12, 0, OBJ_BLOCK}, {15, 1, OBJ_BLOCK}, {18, 2, OBJ_BLOCK},
            {22, 3, OBJ_ORB_YELLOW}, {26, 3, OBJ_BLOCK}, {27, 3, OBJ_BLOCK},
            {36, 0, OBJ_SPIKE}, {37, 0, OBJ_SPIKE}, {38, 0, OBJ_SPIKE},
            {48, 2, OBJ_PORTAL_SHIP},
            {60, 0, OBJ_BLOCK}, {60, 1, OBJ_BLOCK}, {60, 2, OBJ_BLOCK}, {60, 6, OBJ_BLOCK},
            {74, 2, OBJ_PORTAL_CUBE},
            {85, 0, OBJ_PAD_YELLOW}, {90, 2, OBJ_ORB_YELLOW},
            {96, 0, OBJ_SPIKE}, {97, 0, OBJ_SPIKE}, {98, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 112.0f;
        levels.push_back(lvl);
    }

    // 6. Can't Let Go
    {
        LevelData lvl;
        lvl.name = "Can't Let Go";
        lvl.stars = 6;
        lvl.bgColor = C2D_Color32(180, 95, 20, 255);
        lvl.groundColor = C2D_Color32(130, 60, 10, 255);
        lvl.objects = {
            {12, 0, OBJ_SPIKE}, {13, 0, OBJ_SPIKE}, {14, 0, OBJ_SPIKE},
            {22, 0, OBJ_BLOCK}, {25, 1, OBJ_BLOCK}, {28, 2, OBJ_BLOCK},
            {32, 2, OBJ_ORB_YELLOW}, {36, 3, OBJ_BLOCK},
            {45, 1, OBJ_PORTAL_GRAV_UP},
            {55, 6, OBJ_SPIKE}, {56, 6, OBJ_SPIKE},
            {66, 5, OBJ_PORTAL_GRAV_DOWN},
            {76, 0, OBJ_PAD_YELLOW}, {81, 2, OBJ_ORB_YELLOW},
            {88, 0, OBJ_SPIKE}, {89, 0, OBJ_SPIKE}, {90, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 105.0f;
        levels.push_back(lvl);
    }

    // 7. Jumper
    {
        LevelData lvl;
        lvl.name = "Jumper";
        lvl.stars = 7;
        lvl.bgColor = C2D_Color32(45, 45, 150, 255);
        lvl.groundColor = C2D_Color32(25, 25, 105, 255);
        lvl.objects = {
            {12, 0, OBJ_PAD_YELLOW},
            {17, 2, OBJ_ORB_YELLOW},
            {22, 3, OBJ_BLOCK}, {23, 3, OBJ_BLOCK},
            {30, 0, OBJ_SPIKE}, {31, 0, OBJ_SPIKE}, {32, 0, OBJ_SPIKE},
            {42, 2, OBJ_PORTAL_SHIP},
            {52, 0, OBJ_BLOCK}, {52, 1, OBJ_BLOCK}, {52, 5, OBJ_BLOCK}, {52, 6, OBJ_BLOCK},
            {64, 1, OBJ_PORTAL_GRAV_UP},
            {76, 6, OBJ_BLOCK}, {76, 5, OBJ_BLOCK}, {76, 1, OBJ_BLOCK}, {76, 0, OBJ_BLOCK},
            {88, 5, OBJ_PORTAL_GRAV_DOWN},
            {96, 2, OBJ_PORTAL_CUBE},
            {106, 0, OBJ_SPIKE}, {107, 0, OBJ_SPIKE}, {108, 0, OBJ_SPIKE}
        };
        lvl.lengthBlocks = 122.0f;
        levels.push_back(lvl);
    }
}

void resetPlayer(Player& p, bool fullReset) {
    for (auto& obj : levels[currentLevelIdx].objects) {
        obj.triggered = false;
    }

    if (practiceMode && !fullReset && !checkpoints.empty()) {
        Checkpoint cp = checkpoints.back();
        p.x = cp.playerX;
        p.y = cp.playerY;
        p.velY = cp.velY;
        p.mode = cp.mode;
        p.gravDir = cp.gravDir;
        p.dead = false;
        p.attempts++;
        return;
    }

    p.x = 0.0f;
    p.y = GROUND_Y - BLOCK_SIZE;
    p.velY = 0.0f;
    p.rotation = 0.0f;
    p.onGround = true;
    p.mode = MODE_CUBE;
    p.gravDir = 1;
    p.dead = false;
    if (fullReset) {
        p.attempts = 1;
        checkpoints.clear();
    } else {
        p.attempts++;
    }
}

bool checkAABB(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

void updateGame(Player& p, bool held, bool justPressed) {
    if (p.dead) {
        resetPlayer(p, false);
        return;
    }

    if (!held) {
        p.orbUsedThisPress = false;
    }

    LevelData& lvl = levels[currentLevelIdx];
    p.x += SCROLL_SPEED;

    // Verificar victoria del nivel
    if (p.x >= lvl.lengthBlocks * BLOCK_SIZE) {
        inMenu = true;
        return;
    }

    // Físicas según el modo
    if (p.mode == MODE_CUBE) {
        if (held && p.onGround) {
            p.velY = JUMP_FORCE * p.gravDir;
            p.onGround = false;
        }
        p.velY += GRAVITY_CUBE * p.gravDir;
        if (p.gravDir == 1 && p.velY > MAX_FALL_SPEED) p.velY = MAX_FALL_SPEED;
        if (p.gravDir == -1 && p.velY < -MAX_FALL_SPEED) p.velY = -MAX_FALL_SPEED;

        p.y += p.velY;

        if (!p.onGround) {
            p.rotation += 6.2f * p.gravDir;
        } else {
            // Alinear rotación a múltiplos de 90 grados al tocar superficie
            p.rotation = std::round(p.rotation / 90.0f) * 90.0f;
        }
    } else if (p.mode == MODE_SHIP) {
        if (held) {
            p.velY += SHIP_THRUST * p.gravDir;
        } else {
            p.velY += SHIP_GRAVITY * p.gravDir;
        }
        if (p.velY > 6.5f) p.velY = 6.5f;
        if (p.velY < -6.5f) p.velY = -6.5f;

        p.y += p.velY;
        p.rotation = p.velY * 3.5f;
    }

    p.onGround = false;

    // Colisión con el suelo y el techo
    if (p.gravDir == 1) {
        if (p.y + BLOCK_SIZE >= GROUND_Y) {
            p.y = GROUND_Y - BLOCK_SIZE;
            p.velY = 0;
            p.onGround = true;
        }
        if (p.y < CEILING_Y) {
            if (p.mode == MODE_SHIP) {
                p.y = CEILING_Y;
                p.velY = 0;
            } else {
                p.dead = true;
            }
        }
    } else {
        if (p.y <= CEILING_Y) {
            p.y = CEILING_Y;
            p.velY = 0;
            p.onGround = true;
        }
        if (p.y + BLOCK_SIZE > GROUND_Y) {
            if (p.mode == MODE_SHIP) {
                p.y = GROUND_Y - BLOCK_SIZE;
                p.velY = 0;
            } else {
                p.dead = true;
            }
        }
    }

    // Colisiones con objetos del nivel
    float playerScreenX = 80.0f;
    for (auto& obj : lvl.objects) {
        float objScreenX = (obj.x * BLOCK_SIZE) - p.x + playerScreenX;
        if (objScreenX < -50 || objScreenX > TOP_WIDTH + 50) continue;

        float objScreenY = GROUND_Y - ((obj.y + 1) * BLOCK_SIZE);

        if (obj.type == OBJ_BLOCK) {
            if (checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE, objScreenX, objScreenY, BLOCK_SIZE, BLOCK_SIZE)) {
                // Determinar si aterriza arriba/abajo del bloque o choca de frente
                float prevY = p.y - p.velY;
                if (p.gravDir == 1 && prevY + BLOCK_SIZE <= objScreenY + 6.0f && p.velY >= 0) {
                    p.y = objScreenY - BLOCK_SIZE;
                    p.velY = 0;
                    p.onGround = true;
                } else if (p.gravDir == -1 && prevY >= objScreenY + BLOCK_SIZE - 6.0f && p.velY <= 0) {
                    p.y = objScreenY + BLOCK_SIZE;
                    p.velY = 0;
                    p.onGround = true;
                } else if (p.mode == MODE_SHIP && prevY >= objScreenY + BLOCK_SIZE - 6.0f) {
                    p.y = objScreenY + BLOCK_SIZE;
                    p.velY = 0;
                } else {
                    p.dead = true;
                }
            }
        } else if (obj.type == OBJ_SPIKE) {
            // Hitbox interior más justa igual que en Geometry Dash original
            float padX = 7.0f;
            float padY = 6.0f;
            if (checkAABB(playerScreenX + 3, p.y + 3, BLOCK_SIZE - 6, BLOCK_SIZE - 6,
                          objScreenX + padX, objScreenY + padY, BLOCK_SIZE - padX * 2, BLOCK_SIZE - padY)) {
                p.dead = true;
            }
        } else if (obj.type == OBJ_PAD_YELLOW) {
            if (!obj.triggered && checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE,
                                            objScreenX, objScreenY + 14, BLOCK_SIZE, 10)) {
                p.velY = PAD_FORCE * p.gravDir;
                p.onGround = false;
                obj.triggered = true;
            }
        } else if (obj.type == OBJ_ORB_YELLOW) {
            if (!obj.triggered && held && !p.orbUsedThisPress &&
                checkAABB(playerScreenX - 4, p.y - 4, BLOCK_SIZE + 8, BLOCK_SIZE + 8,
                          objScreenX, objScreenY, BLOCK_SIZE, BLOCK_SIZE)) {
                p.velY = ORB_FORCE * p.gravDir;
                p.onGround = false;
                obj.triggered = true;
                p.orbUsedThisPress = true;
            }
        } else if (obj.type == OBJ_PORTAL_SHIP) {
            if (checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE, objScreenX, objScreenY - 12, BLOCK_SIZE, BLOCK_SIZE * 2)) {
                p.mode = MODE_SHIP;
            }
        } else if (obj.type == OBJ_PORTAL_CUBE) {
            if (checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE, objScreenX, objScreenY - 12, BLOCK_SIZE, BLOCK_SIZE * 2)) {
                p.mode = MODE_CUBE;
            }
        } else if (obj.type == OBJ_PORTAL_GRAV_UP) {
            if (!obj.triggered && checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE, objScreenX, objScreenY - 12, BLOCK_SIZE, BLOCK_SIZE * 2)) {
                p.gravDir = -1;
                obj.triggered = true;
            }
        } else if (obj.type == OBJ_PORTAL_GRAV_DOWN) {
            if (!obj.triggered && checkAABB(playerScreenX, p.y, BLOCK_SIZE, BLOCK_SIZE, objScreenX, objScreenY - 12, BLOCK_SIZE, BLOCK_SIZE * 2)) {
                p.gravDir = 1;
                obj.triggered = true;
            }
        }
    }
}

void renderTopScreen(C3D_RenderTarget* top, const Player& p) {
    C2D_TargetClear(top, levels[currentLevelIdx].bgColor);
    C2D_SceneBegin(top);

    LevelData& lvl = levels[currentLevelIdx];
    u32 white = C2D_Color32(255, 255, 255, 255);
    u32 black = C2D_Color32(0, 0, 0, 255);

    // Suelo y techo
    C2D_DrawRectSolid(0, GROUND_Y, 0.5f, TOP_WIDTH, TOP_HEIGHT - GROUND_Y, lvl.groundColor);
    C2D_DrawRectSolid(0, GROUND_Y, 0.6f, TOP_WIDTH, 2, white);
    C2D_DrawRectSolid(0, 0, 0.5f, TOP_WIDTH, CEILING_Y, lvl.groundColor);
    C2D_DrawRectSolid(0, CEILING_Y - 2, 0.6f, TOP_WIDTH, 2, white);

    // Dibujar objetos del nivel
    float playerScreenX = 80.0f;
    for (const auto& obj : lvl.objects) {
        float sx = (obj.x * BLOCK_SIZE) - p.x + playerScreenX;
        if (sx < -40 || sx > TOP_WIDTH + 40) continue;
        float sy = GROUND_Y - ((obj.y + 1) * BLOCK_SIZE);

        if (obj.type == OBJ_BLOCK) {
            C2D_DrawRectSolid(sx, sy, 0.5f, BLOCK_SIZE, BLOCK_SIZE, black);
            C2D_DrawRectSolid(sx + 2, sy + 2, 0.55f, BLOCK_SIZE - 4, BLOCK_SIZE - 4, C2D_Color32(40, 40, 40, 255));
            // Borde blanco estilo 1.0
            C2D_DrawRectSolid(sx, sy, 0.6f, BLOCK_SIZE, 2, white);
            C2D_DrawRectSolid(sx, sy + BLOCK_SIZE - 2, 0.6f, BLOCK_SIZE, 2, white);
            C2D_DrawRectSolid(sx, sy, 0.6f, 2, BLOCK_SIZE, white);
            C2D_DrawRectSolid(sx + BLOCK_SIZE - 2, sy, 0.6f, 2, BLOCK_SIZE, white);
        } else if (obj.type == OBJ_SPIKE) {
            C2D_DrawTriangle(sx, sy + BLOCK_SIZE, black,
                             sx + BLOCK_SIZE / 2.0f, sy, black,
                             sx + BLOCK_SIZE, sy + BLOCK_SIZE, black, 0.55f);
            C2D_DrawTriangle(sx + 3, sy + BLOCK_SIZE - 1, white,
                             sx + BLOCK_SIZE / 2.0f, sy + 4, white,
                             sx + BLOCK_SIZE - 3, sy + BLOCK_SIZE - 1, white, 0.6f);
        } else if (obj.type == OBJ_PAD_YELLOW) {
            C2D_DrawRectSolid(sx + 2, sy + BLOCK_SIZE - 6, 0.6f, BLOCK_SIZE - 4, 6, C2D_Color32(255, 230, 0, 255));
        } else if (obj.type == OBJ_ORB_YELLOW) {
            C2D_DrawCircleSolid(sx + BLOCK_SIZE / 2, sy + BLOCK_SIZE / 2, 0.6f, 10, C2D_Color32(255, 230, 0, 255));
            C2D_DrawCircleSolid(sx + BLOCK_SIZE / 2, sy + BLOCK_SIZE / 2, 0.65f, 6, white);
        } else if (obj.type == OBJ_PORTAL_SHIP) {
            C2D_DrawRectSolid(sx + 4, sy - 12, 0.6f, 16, BLOCK_SIZE * 2, C2D_Color32(255, 80, 220, 255));
        } else if (obj.type == OBJ_PORTAL_CUBE) {
            C2D_DrawRectSolid(sx + 4, sy - 12, 0.6f, 16, BLOCK_SIZE * 2, C2D_Color32(50, 255, 100, 255));
        } else if (obj.type == OBJ_PORTAL_GRAV_UP) {
            C2D_DrawRectSolid(sx + 4, sy - 12, 0.6f, 16, BLOCK_SIZE * 2, C2D_Color32(50, 150, 255, 255));
        } else if (obj.type == OBJ_PORTAL_GRAV_DOWN) {
            C2D_DrawRectSolid(sx + 4, sy - 12, 0.6f, 16, BLOCK_SIZE * 2, C2D_Color32(255, 200, 30, 255));
        }
    }

    // Dibujar Checkpoints en modo práctica
    if (practiceMode) {
        for (const auto& cp : checkpoints) {
            float cpx = cp.playerX - p.x + playerScreenX;
            if (cpx >= -20 && cpx <= TOP_WIDTH + 20) {
                C2D_DrawRectSolid(cpx + 6, cp.playerY + 4, 0.65f, 12, 16, C2D_Color32(0, 255, 180, 255));
            }
        }
    }

    // Dibujar Jugador
    u32 pCol = primaryColors[pColorIdx];
    u32 sCol = secondaryColors[sColorIdx];

    if (p.mode == MODE_CUBE) {
        C2D_DrawRectSolid(playerScreenX, p.y, 0.7f, BLOCK_SIZE, BLOCK_SIZE, pCol);
        C2D_DrawRectSolid(playerScreenX + 5, p.y + 5, 0.75f, BLOCK_SIZE - 10, BLOCK_SIZE - 10, sCol);
        // Ojos clásicos del icono 1 de GD
        C2D_DrawRectSolid(playerScreenX + 6, p.y + 6, 0.8f, 4, 4, black);
        C2D_DrawRectSolid(playerScreenX + 14, p.y + 6, 0.8f, 4, 4, black);
        C2D_DrawRectSolid(playerScreenX + 6, p.y + 14, 0.8f, 12, 3, black);
    } else {
        // Nave (Ship)
        C2D_DrawRectSolid(playerScreenX + 4, p.y + 2, 0.7f, 14, 12, pCol);
        C2D_DrawRectSolid(playerScreenX, p.y + 12, 0.75f, BLOCK_SIZE, 10, sCol);
    }

    // Barra de progreso superior
    float progress = p.x / (lvl.lengthBlocks * BLOCK_SIZE);
    if (progress > 1.0f) progress = 1.0f;
    C2D_DrawRectSolid(100, 8, 0.8f, 200, 6, C2D_Color32(0, 0, 0, 150));
    C2D_DrawRectSolid(101, 9, 0.85f, 198 * progress, 4, C2D_Color32(0, 255, 80, 255));
}

int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    consoleInit(GFX_BOTTOM, NULL);

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    initLevels();
    Player player;
    resetPlayer(player, true);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        bool jumpHeld = (kHeld & (KEY_A | KEY_UP | KEY_TOUCH));
        bool jumpDown = (kDown & (KEY_A | KEY_UP | KEY_TOUCH));

        if (inMenu) {
            if (kDown & KEY_RIGHT) currentLevelIdx = (currentLevelIdx + 1) % levels.size();
            if (kDown & KEY_LEFT)  currentLevelIdx = (currentLevelIdx - 1 + levels.size()) % levels.size();
            if (kDown & KEY_Y)     practiceMode = !practiceMode;
            if (kDown & KEY_L)     pColorIdx = (pColorIdx + 1) % 5;
            if (kDown & KEY_R)     sColorIdx = (sColorIdx + 1) % 5;

            if (kDown & KEY_A) {
                inMenu = false;
                resetPlayer(player, true);
            }
        } else {
            if (kDown & KEY_B) {
                inMenu = true;
            }
            if (practiceMode) {
                if (kDown & KEY_X) {
                    checkpoints.push_back({player.x, player.y, player.velY, player.mode, player.gravDir});
                }
                if (kDown & KEY_Y && !checkpoints.empty()) {
                    checkpoints.pop_back();
                }
            }
            updateGame(player, jumpHeld, jumpDown);
        }

        // Actualizar consola en pantalla inferior
        consoleClear();
        printf("\x1b[1;1H=== GEOMETRY DASH v1.0 (3DS OLD) ===\n\n");
        if (inMenu) {
            printf("Nivel [< / >]: %s (%d Estrellas)\n", levels[currentLevelIdx].name.c_str(), levels[currentLevelIdx].stars);
            printf("Modo Practica [Y]: %s\n", practiceMode ? "ACTIVADO" : "DESACTIVADO");
            printf("Color Primario [L]: %d | Secundario [R]: %d\n\n", pColorIdx + 1, sColorIdx + 1);
            printf("Presiona [A] para JUGAR\n");
            printf("Presiona [START] para Salir\n");
        } else {
            int pct = (int)((player.x / (levels[currentLevelIdx].lengthBlocks * BLOCK_SIZE)) * 100.0f);
            if (pct > 100) pct = 100;
            printf("Nivel: %s\n", levels[currentLevelIdx].name.c_str());
            printf("Intento: %d\n", player.attempts);
            printf("Progreso: %d%%\n", pct);
            printf("Modo: %s\n\n", player.mode == MODE_CUBE ? "Cubo" : "Nave");
            if (practiceMode) {
                printf("[X] Poner Checkpoint (%zu)\n", checkpoints.size());
                printf("[Y] Borrar Checkpoint\n");
            }
            printf("[A / Tactil] Saltar / Volar\n");
            printf("[B] Volver al Menu\n");
        }

        // Renderizar a 60 FPS bloqueados por VSync
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        renderTopScreen(top, player);
        C3D_FrameEnd(0);
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
