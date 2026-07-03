/*
 * Rockworld Gray Stone
 * Compilar (Windows, MSYS2/MinGW):
 *   g++ rockWorld.cpp -o rockWorld -std=c++17 -O2 -lglfw3 -lopengl32 -lgdi32 -luser32
 *   ./rockWorld.exe
 * 
 * Compilar (Linux):
 *  g++ rockWorld.cpp -o rockWorld -std=c++17 -O2 -lglfw -lGL
 *  ./rockWorld
 *  
 * Controles na selecao:
 *   Seta esquerda/direita — navegar entre as 3 pedras
 *   ENTER / SPACE         — confirmar escolha
 *
 * Controles na batalha:
 *   1 / Seta cima  — Habilidade 1
 *   2 / Seta baixo — Habilidade 2
 *   R              — Reiniciar (apos game over)
 *   ESC            — Sair
 *
 * Pedras iniciais:
 *   Ignithar  — tipo Fogo  | habilidades: Raio de Fogo, Circulo de Fogo
 *   Torrean   — tipo Agua  | habilidades: Bolhas,       Cachoeira
 *   Crustolid — tipo Terra | habilidades: Raio de Fogo, Bolhas
 */

 #define GLFW_INCLUDE_NONE
 #include <GLFW/glfw3.h>
 
 #ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #endif
 
 #include <GL/gl.h>
 
 #include <algorithm>
 #include <cmath>
 #include <cstdio>
 #include <cstdlib>
 #include <random>
 #include <string>
 #include <vector>
 #include <cstring>

// ---------------------------------------------------------------------------
// Fases do jogo
// ---------------------------------------------------------------------------
enum class GamePhase {
    CharSelect,
    DifficultySelect,
    BattleScene1,
    BattleScene2,
    BattleScene3,
    BattleScene4
};

static GamePhase currentPhase = GamePhase::CharSelect;

// Ao trocar de tela por confirmação (ENTER/SPACE), evitamos que a tecla
// permaneça "presa" e dispare imediatamente uma ação no próximo frame.
static bool g_consumeConfirm = false;

// ---------------------------------------------------------------------------
// Utilitarios
// ---------------------------------------------------------------------------
static std::mt19937 rng{std::random_device{}()};

static float randf(float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(rng);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ---------------------------------------------------------------------------
// Particulas
// ---------------------------------------------------------------------------
struct Particle {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float life = 0, maxLife = 1;
    float size = 4;
    float r = 1, g = 0.5f, b = 0;
    float alpha = 1;
};

class ParticleSystem {
public:
    std::vector<Particle> particles;

    void clear() { particles.clear(); }

    void update(float dt) {
        for (auto& p : particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt;
            float t = 1.f - p.life / p.maxLife;
            p.alpha = std::max(0.f, 1.f - t);
            p.size  = std::max(0.5f, p.size * (1.f - dt * 0.8f));
        }
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                           [](const Particle& p){ return p.life <= 0; }),
            particles.end());
    }

    void draw() const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_POINT_SMOOTH);

        for (const auto& p : particles) {
            float glow = p.alpha;
            glPointSize(p.size * 2.2f);
            glColor4f(p.r, p.g * 0.6f, p.b * 0.2f, glow * 0.35f);
            glBegin(GL_POINTS); glVertex2f(p.x, p.y); glEnd();

            glPointSize(p.size);
            glColor4f(p.r, p.g, p.b * 0.3f, glow);
            glBegin(GL_POINTS); glVertex2f(p.x, p.y); glEnd();
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void spawn(const Particle& p) { particles.push_back(p); }
};

// ---------------------------------------------------------------------------
// Tipos de ataque
// ---------------------------------------------------------------------------
enum class AttackType { None, FireRay, FireCircle, Bubbles, Waterfall, RazorLeaf, GigaDrain };

// ---------------------------------------------------------------------------
// FireRayAttack — Raio de Fogo
// ---------------------------------------------------------------------------
class FireRayAttack {
public:
    bool  active = false;
    float timer = 0, duration = 1.4f;
    float fromX = 0, fromY = 0, toX = 0, toY = 0;
    ParticleSystem* ps = nullptr;

    explicit FireRayAttack(ParticleSystem& system) : ps(&system) {}

    void start(float fx, float fy, float tx, float ty) {
        active = true; timer = 0;
        fromX = fx; fromY = fy; toX = tx; toY = ty;
        ps->clear();
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        float progress = std::min(1.f, timer / duration);
        float headX = lerp(fromX, toX, progress);
        float headY = lerp(fromY, toY, progress);

        for (int i = 0; i < 6; ++i) {
            Particle p;
            p.x = headX + randf(-8, 8);
            p.y = headY + randf(-8, 8);
            float dx = toX - fromX, dy = toY - fromY;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len > 0.001f) { dx /= len; dy /= len; }
            p.vx = dx * randf(180, 320) + randf(-60, 60);
            p.vy = dy * randf(180, 320) + randf(-60, 60);
            p.life = p.maxLife = randf(0.25f, 0.55f);
            p.size = randf(5, 11);
            p.r = 1.f; p.g = randf(0.35f, 0.75f); p.b = randf(0.f, 0.15f);
            ps->spawn(p);
        }
        for (int i = 0; i < 3; ++i) {
            float t = randf(0.f, progress);
            Particle trail;
            trail.x = lerp(fromX, toX, t) + randf(-12, 12);
            trail.y = lerp(fromY, toY, t) + randf(-12, 12);
            trail.vx = randf(-40, 40); trail.vy = randf(30, 90);
            trail.life = trail.maxLife = randf(0.2f, 0.45f);
            trail.size = randf(3, 7);
            trail.r = 1.f; trail.g = randf(0.2f, 0.5f); trail.b = 0.f;
            ps->spawn(trail);
        }
        if (timer >= duration) {
            for (int i = 0; i < 40; ++i) {
                Particle impact;
                impact.x = toX + randf(-15, 15);
                impact.y = toY + randf(-15, 15);
                float angle = randf(0, 6.28318f), speed = randf(80, 260);
                impact.vx = std::cos(angle) * speed;
                impact.vy = std::sin(angle) * speed;
                impact.life = impact.maxLife = randf(0.3f, 0.7f);
                impact.size = randf(6, 14);
                impact.r = 1.f; impact.g = randf(0.3f, 0.6f); impact.b = 0.f;
                ps->spawn(impact);
            }
            active = false;
        }
    }
};

// ---------------------------------------------------------------------------
// FireCircleAttack — Circulo de Fogo
// ---------------------------------------------------------------------------
class FireCircleAttack {
public:
    bool  active = false;
    float timer = 0, duration = 2.2f;
    float centerX = 0, centerY = 0, startRadius = 110;
    bool  exploded = false;
    ParticleSystem* ps = nullptr;

    explicit FireCircleAttack(ParticleSystem& system) : ps(&system) {}

    void start(float cx, float cy) {
        active = true; timer = 0;
        centerX = cx; centerY = cy; exploded = false;
        ps->clear();
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        float t = std::min(1.f, timer / duration);
        float radius = lerp(startRadius, 8.f, t * t);

        for (int i = 0; i < 24; ++i) {
            float angle = (float(i) / 24) * 6.28318f + timer * 3.f;
            Particle p;
            p.x = centerX + std::cos(angle)*radius + randf(-3, 3);
            p.y = centerY + std::sin(angle)*radius + randf(-3, 3);
            p.vx = -std::cos(angle)*randf(20, 60);
            p.vy = -std::sin(angle)*randf(20, 60);
            p.life = p.maxLife = randf(0.35f, 0.65f);
            p.size = randf(5, 10);
            p.r = 1.f; p.g = randf(0.25f, 0.55f); p.b = randf(0.f, 0.1f);
            ps->spawn(p);
        }
        for (int i = 0; i < 8; ++i) {
            float angle = randf(0, 6.28318f);
            Particle spiral;
            spiral.x = centerX + std::cos(angle)*radius*randf(0.6f, 1.1f);
            spiral.y = centerY + std::sin(angle)*radius*randf(0.6f, 1.1f);
            spiral.vx = randf(-30, 30); spiral.vy = randf(20, 70);
            spiral.life = spiral.maxLife = randf(0.25f, 0.5f);
            spiral.size = randf(4, 8);
            spiral.r = 1.f; spiral.g = randf(0.3f, 0.6f); spiral.b = 0.f;
            ps->spawn(spiral);
        }
        if (t >= 0.92f && !exploded) {
            exploded = true;
            for (int i = 0; i < 80; ++i) {
                Particle boom;
                boom.x = centerX + randf(-10, 10);
                boom.y = centerY + randf(-10, 10);
                float angle = randf(0, 6.28318f), speed = randf(100, 350);
                boom.vx = std::cos(angle)*speed; boom.vy = std::sin(angle)*speed;
                boom.life = boom.maxLife = randf(0.4f, 0.9f);
                boom.size = randf(8, 18);
                boom.r = 1.f; boom.g = randf(0.2f, 0.55f); boom.b = 0.f;
                ps->spawn(boom);
            }
        }
        if (timer >= duration + 0.5f) active = false;
    }
};

// ---------------------------------------------------------------------------
// BubblesAttack — Bolhas
// ---------------------------------------------------------------------------
class BubblesAttack {
public:
    bool  active = false;
    float timer = 0, duration = 2.0f;
    float fromX = 0, fromY = 0, toX = 0, toY = 0;
    ParticleSystem* ps = nullptr;

    struct Bubble { float x, y, vx, vy, radius, life, maxLife, wobble; };
    std::vector<Bubble> bubbles;

    explicit BubblesAttack(ParticleSystem& system) : ps(&system) {}

    void start(float fx, float fy, float tx, float ty) {
        active = true; timer = 0;
        fromX = fx; fromY = fy; toX = tx; toY = ty;
        bubbles.clear(); ps->clear();
        for (int i = 0; i < 12; ++i) {
            Bubble b;
            b.x = fx; b.y = fy;
            float ang = std::atan2(ty - fy, tx - fx) + randf(-0.5f, 0.5f);
            float sp  = randf(180.f, 320.f);
            b.vx = std::cos(ang)*sp; b.vy = std::sin(ang)*sp;
            b.radius = randf(10.f, 22.f);
            b.life = b.maxLife = randf(1.2f, 2.0f);
            b.wobble = randf(0.f, 6.28f);
            bubbles.push_back(b);
        }
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        for (auto& b : bubbles) {
            b.x += b.vx * dt; b.y += b.vy * dt;
            b.vy += 20.f * dt;
            b.vx += std::sin(b.wobble + timer * 6.f) * 15.f * dt;
            b.life -= dt; b.wobble += dt * 4.f;
            if (randf(0,1) < 0.4f) {
                Particle p;
                p.x = b.x + randf(-b.radius, b.radius);
                p.y = b.y + randf(-b.radius, b.radius);
                p.vx = randf(-20, 20); p.vy = randf(20, 60);
                p.life = p.maxLife = randf(0.1f, 0.25f);
                p.size = randf(2, 5);
                p.r = 0.5f; p.g = 0.85f; p.b = 1.f;
                ps->spawn(p);
            }
        }
        bubbles.erase(std::remove_if(bubbles.begin(), bubbles.end(),
            [](const Bubble& b){ return b.life <= 0; }), bubbles.end());
        if (timer >= duration && bubbles.empty()) active = false;
    }

    void draw() const {
        if (!active) return;
        for (const auto& b : bubbles) {
            float alpha = std::min(1.f, b.life / b.maxLife);
            int segs = 24;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.5f, 0.85f, 1.f, alpha * 0.35f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(b.x, b.y);
            for (int i = 0; i <= segs; ++i) {
                float a = (float(i) / segs) * 6.28318f;
                glVertex2f(b.x + std::cos(a)*b.radius, b.y + std::sin(a)*b.radius);
            }
            glEnd();
            glLineWidth(2.f);
            glColor4f(0.7f, 0.95f, 1.f, alpha * 0.8f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segs; ++i) {
                float a = (float(i) / segs) * 6.28318f;
                glVertex2f(b.x + std::cos(a)*b.radius, b.y + std::sin(a)*b.radius);
            }
            glEnd();
            glColor4f(1.f, 1.f, 1.f, alpha * 0.6f);
            glPointSize(3.f);
            glBegin(GL_POINTS);
            glVertex2f(b.x - b.radius*0.35f, b.y + b.radius*0.35f);
            glEnd();
        }
    }
};

// ---------------------------------------------------------------------------
// WaterfallAttack — Cachoeira
// ---------------------------------------------------------------------------
class WaterfallAttack {
public:
    bool  active = false;
    float timer = 0, duration = 2.5f;
    float targetX = 0, targetY = 0;
    ParticleSystem* ps = nullptr;

    explicit WaterfallAttack(ParticleSystem& system) : ps(&system) {}

    void start(float tx, float ty) {
        active = true; timer = 0;
        targetX = tx; targetY = ty;
        ps->clear();
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        for (int i = 0; i < 18; ++i) {
            Particle p;
            float ox = randf(-40.f, 40.f);
            p.x = targetX + ox;
            p.y = targetY + 250.f + randf(0, 60.f);
            p.vx = ox * randf(0.1f, 0.3f);
            p.vy = -randf(220.f, 380.f);
            p.life = p.maxLife = randf(0.3f, 0.6f);
            p.size = randf(4, 9);
            p.r = 0.3f; p.g = 0.7f; p.b = 1.f;
            ps->spawn(p);
        }
        if (timer > 0.3f) {
            for (int i = 0; i < 6; ++i) {
                Particle sp;
                sp.x = targetX + randf(-35.f, 35.f);
                sp.y = targetY + randf(0, 20.f);
                float ang = randf(0.f, 6.28318f), spd = randf(40.f, 120.f);
                sp.vx = std::cos(ang)*spd;
                sp.vy = std::sin(ang)*spd * 0.5f + 30.f;
                sp.life = sp.maxLife = randf(0.2f, 0.5f);
                sp.size = randf(3, 7);
                sp.r = 0.5f; sp.g = 0.85f; sp.b = 1.f;
                ps->spawn(sp);
            }
        }
        if (timer >= duration) active = false;
    }
};

// ---------------------------------------------------------------------------
// RazorLeafAttack — Folhas Cortantes
// ---------------------------------------------------------------------------
class RazorLeafAttack {
public:
    bool  active = false;
    float timer = 0, duration = 1.6f;
    float fromX = 0, fromY = 0, toX = 0, toY = 0;
    ParticleSystem* ps = nullptr;

    struct Leaf {
        float x, y, vx, vy, angle, spin, size, life, maxLife;
    };
    std::vector<Leaf> leaves;

    explicit RazorLeafAttack(ParticleSystem& system) : ps(&system) {}

    void start(float fx, float fy, float tx, float ty) {
        active = true; timer = 0;
        fromX = fx; fromY = fy; toX = tx; toY = ty;
        leaves.clear(); ps->clear();
        float baseAng = std::atan2(ty - fy, tx - fx);
        for (int i = 0; i < 12; ++i) {
            Leaf l;
            l.x = fx + randf(-18, 18);
            l.y = fy + randf(-10, 10);
            float spread = randf(-0.35f, 0.35f);
            float sp = randf(240.f, 400.f);
            l.vx = std::cos(baseAng + spread) * sp;
            l.vy = std::sin(baseAng + spread) * sp;
            l.angle = randf(0, 6.28318f);
            l.spin  = randf(-10.f, 10.f);
            l.size  = randf(9.f, 15.f);
            l.life  = l.maxLife = randf(0.8f, 1.4f);
            leaves.push_back(l);
        }
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        // Vento — particulas horizontais ao longo da trajetoria
        float baseAng = std::atan2(toY - fromY, toX - fromX);
        for (int i = 0; i < 6; ++i) {
            float t = randf(0.f, std::min(1.f, timer / duration));
            Particle p;
            p.x = lerp(fromX, toX, t) + randf(-25, 25);
            p.y = lerp(fromY, toY, t) + randf(-18, 18);
            p.vx = std::cos(baseAng) * randf(100, 220) + randf(-30, 30);
            p.vy = std::sin(baseAng) * randf(100, 220) + randf(-25, 25);
            p.life = p.maxLife = randf(0.08f, 0.2f);
            p.size = randf(2, 5);
            p.r = 0.55f; p.g = 1.f; p.b = 0.3f;
            ps->spawn(p);
        }
        // Atualiza folhas
        for (auto& l : leaves) {
            l.x     += l.vx * dt;
            l.y     += l.vy * dt;
            l.vy    -= 25.f * dt;
            l.angle += l.spin * dt;
            l.life  -= dt;
        }
        leaves.erase(std::remove_if(leaves.begin(), leaves.end(),
            [](const Leaf& l){ return l.life <= 0; }), leaves.end());
        // Explosao de folhas no impacto
        if (timer >= duration * 0.85f && timer - dt < duration * 0.85f) {
            for (int i = 0; i < 20; ++i) {
                Particle p;
                p.x = toX + randf(-10, 10); p.y = toY + randf(-10, 10);
                float a = randf(0, 6.28318f), sp = randf(60, 220);
                p.vx = std::cos(a)*sp; p.vy = std::sin(a)*sp;
                p.life = p.maxLife = randf(0.25f, 0.55f);
                p.size = randf(4, 10);
                p.r = 0.2f; p.g = 0.95f; p.b = 0.25f;
                ps->spawn(p);
            }
        }
        if (timer >= duration && leaves.empty()) active = false;
    }

    void draw() const {
        if (!active) return;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto& l : leaves) {
            float alpha = std::clamp(l.life / l.maxLife * 2.f, 0.f, 1.f);
            float ca = std::cos(l.angle), sa = std::sin(l.angle);
            float hw = l.size, hh = l.size * 0.38f;
            // Folha como losango alongado
            float pts[4][2] = {
                {  ca*hw,         sa*hw        },
                { -sa*hh,         ca*hh        },
                { -ca*hw,        -sa*hw        },
                {  sa*hh,        -ca*hh        }
            };
            // Preenchimento verde
            glColor4f(0.15f, 0.72f, 0.12f, alpha);
            glBegin(GL_QUADS);
            for (auto& pt : pts) glVertex2f(l.x + pt[0], l.y + pt[1]);
            glEnd();
            // Borda brilhante
            glColor4f(0.55f, 1.f, 0.35f, alpha * 0.75f);
            glBegin(GL_LINE_LOOP);
            for (auto& pt : pts) glVertex2f(l.x + pt[0], l.y + pt[1]);
            glEnd();
            // Nervura central
            glColor4f(0.8f, 1.f, 0.6f, alpha * 0.5f);
            glBegin(GL_LINES);
            glVertex2f(l.x - ca*hw*0.9f, l.y - sa*hw*0.9f);
            glVertex2f(l.x + ca*hw*0.9f, l.y + sa*hw*0.9f);
            glEnd();
        }
    }
};

// ---------------------------------------------------------------------------
// GigaDrainAttack — Giga Dreno
// ---------------------------------------------------------------------------
class GigaDrainAttack {
public:
    bool  active = false;
    float timer = 0, duration = 2.0f;
    float fromX = 0, fromY = 0; // origem: inimigo
    float toX   = 0, toY   = 0; // destino: jogador
    bool  healed = false;        // flag para aplicar cura exatamente uma vez
    ParticleSystem* ps = nullptr;

    explicit GigaDrainAttack(ParticleSystem& system) : ps(&system) {}

    void start(float fx, float fy, float tx, float ty) {
        active = true; timer = 0; healed = false;
        fromX = fx; fromY = fy; toX = tx; toY = ty;
        ps->clear();
        // Explosao inicial no inimigo
        for (int i = 0; i < 35; ++i) {
            Particle p;
            float ang = randf(0, 6.28318f), sp = randf(60, 180);
            p.x = fx + std::cos(ang)*randf(5, 22);
            p.y = fy + std::sin(ang)*randf(5, 22);
            p.vx = std::cos(ang)*sp; p.vy = std::sin(ang)*sp;
            p.life = p.maxLife = randf(0.3f, 0.7f);
            p.size = randf(5, 11);
            p.r = 0.1f; p.g = randf(0.8f, 1.f); p.b = 0.25f;
            ps->spawn(p);
        }
    }

    void update(float dt) {
        if (!active) return;
        timer += dt;
        float progress = std::min(1.f, timer / duration);
        // Particulas verdes fluindo do inimigo para o jogador
        for (int i = 0; i < 5; ++i) {
            float t = randf(0.f, progress);
            Particle p;
            p.x = lerp(fromX, toX, t) + randf(-14, 14);
            p.y = lerp(fromY, toY, t) + randf(-14, 14);
            float dx = toX - fromX, dy = toY - fromY;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len > 0.001f) { dx /= len; dy /= len; }
            p.vx = dx * randf(120, 240) + randf(-25, 25);
            p.vy = dy * randf(120, 240) + randf(-25, 25);
            p.life = p.maxLife = randf(0.18f, 0.45f);
            p.size = randf(3, 8);
            p.r = 0.1f; p.g = randf(0.75f, 1.f); p.b = 0.2f;
            ps->spawn(p);
        }
        // Chispas de cura chegando no jogador
        if (timer > 0.5f) {
            for (int i = 0; i < 3; ++i) {
                Particle sp;
                sp.x = toX + randf(-18, 18); sp.y = toY + randf(-12, 18);
                float ang = randf(0, 6.28318f);
                sp.vx = std::cos(ang)*randf(40, 130);
                sp.vy = std::sin(ang)*randf(40, 130);
                sp.life = sp.maxLife = randf(0.15f, 0.35f);
                sp.size = randf(4, 8);
                sp.r = 0.4f; sp.g = 1.f; sp.b = 0.55f;
                ps->spawn(sp);
            }
        }
        if (timer >= duration) active = false;
    }
};

// ---------------------------------------------------------------------------
// Definicao das tres pedras iniciais
// ---------------------------------------------------------------------------

// Elemento para seleção de modelo visual
enum class ElemType { Fire, Water, Plant };

struct RockDef {
    const char* name;
    const char* type;
    const char* desc;        // descricao curta para tela de selecao
    const char* move1Name;
    const char* move2Name;
    AttackType  move1;
    AttackType  move2;
    float r, g, b;           // cor do sprite
    float maxHp;
    float dmg1, dmg2;        // dano de cada habilidade
    float heal2 = 0.f;       // cura do movimento 2 (se aplicavel)
    ElemType elemType = ElemType::Fire;
};

// Ignithar  — pedra de magma, tipo Fogo
// Torrean   — pedra porosa encharcada, tipo Agua
// Crustolid — pedra crosta dura, tipo Terra (usa fogo+bolhas = lascas + poeira)
static RockDef ROCK_DEFS[3] = {
    {
        "Ignithar", "Fogo",
        "Pedra de magma vulcanico",
        "Raio de Fogo", "Circulo de Fogo",
        AttackType::FireRay, AttackType::FireCircle,
        0.92f, 0.38f, 0.12f,   // laranja/vermelho
        100.f, 28.f, 35.f, 0.f,
        ElemType::Fire
    },
    {
        "Torrean", "Agua",
        "Pedra porosa encharcada",
        "Bolhas", "Cachoeira",
        AttackType::Bubbles, AttackType::Waterfall,
        0.32f, 0.55f, 0.90f,   // azul
        100.f, 26.f, 34.f, 0.f,
        ElemType::Water
    },
    {
        "Herbrok", "Grama",
        "Pedra coberta de musgo",
        "Folhas Cortantes", "Giga Dreno",
        AttackType::RazorLeaf, AttackType::GigaDrain,
        0.22f, 0.65f, 0.20f,   // verde musgo
        100.f, 31.f, 20.f,
        14.f,   // GigaDreno cura 12 HP
        ElemType::Plant
    }
};

// ---------------------------------------------------------------------------
// Inimigos escalados por fase:
//   Fase 0 — Medio    : 80 HP  | dano baixo   (jogador vence com qualidade razoável)
//   Fase 1 — Dificil  : 140 HP | dano alto     (exige boas jogadas)
//   Fase 2 — Boss     : 200 HP | dano muito alto (morre em ~3 hits; jogador só vence
//                                                 acertando TODOS no ÓTIMO, mult=1.0)
//
// Dano do jogador por ataques (mult máximo 1.0):
//   dmg1~28, dmg2~35  => acertando tudo no verde:  6 hits dmg1 = 168 > 200? não.
//   Ajuste: boss com 200 HP, jogador precisa de ~6 hits no ótimo (6*35=210 > 200 OK)
//   Com vermelho (mult 0.25): 6*35*0.25 = 52 — impossível vencer.
//
// Dano do inimigo no boss: 34 por ataque => 3 hits matam o jogador (102 > 100)
// ---------------------------------------------------------------------------

// Tabelas: [tipo][fase] = {hp, dmg1, dmg2, heal2}
struct EnemyStats { float hp, dmg1, dmg2, heal2; };

static constexpr EnemyStats FIRE_STATS[3] = {
    {  80.f, 20.f, 26.f, 0.f },   // Fase 0: Medio
    { 140.f, 26.f, 34.f, 0.f },   // Fase 1: Dificil
    { 200.f, 32.f, 35.f, 0.f }    // Fase 2: Boss
};

static constexpr EnemyStats WATER_STATS[3] = {
    {  80.f, 18.f, 25.f, 0.f },   // Fase 0: Medio
    { 140.f, 24.f, 32.f, 0.f },   // Fase 1: Dificil
    { 200.f, 30.f, 38.f, 0.f }    // Fase 2: Boss
};

static constexpr EnemyStats PLANT_STATS[3] = {
    {  80.f, 18.f, 14.f, 8.f  },  // Fase 0: Medio
    { 140.f, 24.f, 18.f, 12.f },  // Fase 1: Dificil
    { 200.f, 30.f, 22.f, 16.f }   // Fase 2: Boss
};

// RockDef gerado dinamicamente para cada inimigo+fase
static RockDef g_enemyFireDef, g_enemyWaterDef, g_enemyPlantDef;

static void buildEnemyDefs(int stage) {
    int s = std::clamp(stage, 0, 2);

    g_enemyFireDef = {
        "Flamforge", "Fogo",
        "Golem de magma enfurecido",
        "Raio de Fogo", "Circulo de Fogo",
        AttackType::FireRay, AttackType::FireCircle,
        0.9f, 0.3f, 0.12f,
        FIRE_STATS[s].hp, FIRE_STATS[s].dmg1, FIRE_STATS[s].dmg2, FIRE_STATS[s].heal2,
        ElemType::Fire
    };

    g_enemyWaterDef = {
        "Aquarock", "Agua",
        "Pedra encharcada e afiada",
        "Bolhas", "Cachoeira",
        AttackType::Bubbles, AttackType::Waterfall,
        0.32f, 0.55f, 0.9f,
        WATER_STATS[s].hp, WATER_STATS[s].dmg1, WATER_STATS[s].dmg2, WATER_STATS[s].heal2,
        ElemType::Water
    };

    g_enemyPlantDef = {
        "Thornstone", "Planta",
        "Pedra coberta de musgo",
        "Lascas Cortantes", "Giga Dreno",
        AttackType::RazorLeaf, AttackType::GigaDrain,
        0.25f, 0.7f, 0.25f,
        PLANT_STATS[s].hp, PLANT_STATS[s].dmg1, PLANT_STATS[s].dmg2, PLANT_STATS[s].heal2,
        ElemType::Plant
    };
}

// Current battle stage (0..2)
static int g_battleStage = 0;

// Retorna o inimigo para a fase 'stage' dependendo da pedra escolhida pelo jogador
static const RockDef& getEnemyForStage(int playerIndex, int stage) {
    // sequences per player: fire -> plant, fire, water
    // water -> fire, water, plant
    // plant -> water, plant, fire
    buildEnemyDefs(stage);
    static const RockDef* seq[3][3] = {
        { &g_enemyPlantDef, &g_enemyFireDef,  &g_enemyWaterDef }, // player = Ignithar (fire)
        { &g_enemyFireDef,  &g_enemyWaterDef, &g_enemyPlantDef }, // player = Torrean (water)
        { &g_enemyWaterDef, &g_enemyPlantDef, &g_enemyFireDef  }  // player = Crustolid (plant)
    };
    int idx = std::clamp(playerIndex, 0, 2);
    int s = std::clamp(stage, 0, 2);
    return *seq[idx][s];
}

static float getPlayerMaxHpForStage(float baseHp, int stage) {
    switch (stage) {
        case 1: return 150.f;
        case 2: return 200.f;
        default: return baseHp;
    }
}

// Indice da pedra escolhida pelo jogador (0..2)
static int g_chosenRock = 0;

// ---------------------------------------------------------------------------
// Dificuldade — define a velocidade da barra de timing
// ---------------------------------------------------------------------------
enum class Difficulty { Easy, Medium, Hard };

struct DifficultyDef {
    const char* name;
    const char* desc;
    float speed;
    float r, g, b;
};

static const DifficultyDef DIFFICULTY_DEFS[3] = {
    { "Facil",   "Barra de timing mais lenta",  1.0f, 0.25f, 0.85f, 0.30f },
    { "Medio",   "Barra de timing moderada",    1.5f, 0.95f, 0.80f, 0.20f },
    { "Dificil", "Barra de timing mais rapida", 2.0f, 0.92f, 0.20f, 0.18f }
};

// Indice da dificuldade escolhida pelo jogador (0..2)
static int g_chosenDifficulty = 0;

// ---------------------------------------------------------------------------
// Estado da batalha
// ---------------------------------------------------------------------------
enum class BattleState {
    ChooseMove,
    TimingBar,       // mini-game de timing antes de atacar
    PlayerAttacking,
    EnemyAttacking,
    PlayerWon,
    EnemyWon
};

// ---------------------------------------------------------------------------
// TimingBarState — mini-game de precisao estilo Undertale
// ---------------------------------------------------------------------------
struct TimingBarState {
    bool  active   = false;
    float pos      = 0.f;    // 0..1: posicao do cursor
    float dir      = 1.f;    // +1 ou -1
    float speed    = 1.4f;  // bar-widths/sec (vai e volta)
    bool  locked   = false;
    float lockTimer    = 0.f;
    float lockDuration = 0.55f;
    float multiplier   = 1.f;
    bool  ignoreInput = false;
    AttackType pendingAttack = AttackType::None;

    // Limite de retornos: se o cursor bater na ponta direita (pos == 1) esse
    // numero de vezes sem o jogador travar a barra, ela se fecha
    // automaticamente com o dano equivalente ao vermelho (pior multiplicador).
    int  bounceCount = 0;
    static constexpr int maxBounces = 2;

    // Zonas simetricas em torno de 0.5, alinhadas com a largura desenhada da
    // barra (greenW = barW*0.11 e yellowW = barW*0.50 em renderTimingBar):
    //   Verde  : |pos-0.5| < 0.055 -> x1.00 (cursor realmente sobre o verde)
    //   Amarelo: 0.055 a 0.25      -> x0.60
    //   Vermelho: > 0.25           -> x0.25
    static constexpr float greenHalfWidth  = 0.055f; // (barW*0.11)/2 / barW
    static constexpr float yellowHalfWidth = 0.25f;  // (barW*0.50)/2 / barW

    float evalMultiplier(float p) const {
        float d = std::abs(p - 0.5f);
        if (d < greenHalfWidth)  return 1.00f;
        if (d < yellowHalfWidth) return 0.60f;
        return 0.25f;
    }

    const char* zoneLabel(float p) const {
        float d = std::abs(p - 0.5f);
        if (d < greenHalfWidth)  return "OTIMO!";
        if (d < yellowHalfWidth) return "BOM!";
        return "FRACO...";
    }

    void getZoneColor(float p, float& r, float& g, float& b) const {
        float d = std::abs(p - 0.5f);
        if (d < greenHalfWidth)  { r=0.15f; g=0.95f; b=0.25f; }        // verde
        else if (d < yellowHalfWidth) { r=0.95f; g=0.85f; b=0.1f; }   // amarelo
        else { r=0.92f; g=0.15f; b=0.1f; }                    // vermelho
    }

    void start(AttackType atk) {
        active = true; pos = 0.f; dir = 1.f;
        locked = false; lockTimer = 0.f; multiplier = 1.f;
        ignoreInput = true;
        bounceCount = 0;
        pendingAttack = atk;
    }

    void lock() {
        if (locked) return;
        locked = true; lockTimer = 0.f;
        multiplier = evalMultiplier(pos);
    }

    // Trava a barra automaticamente com o pior multiplicador (vermelho),
    // usado quando o limite de retornos eh atingido sem o jogador travar.
    void lockAsMiss() {
        if (locked) return;
        locked = true; lockTimer = 0.f;
        multiplier = 0.25f; // equivalente a zona vermelha
    }

    bool isDone() const { return active && locked && lockTimer >= lockDuration; }

    void update(float dt) {
        if (!active) return;
        if (!locked) {
            pos += dir * speed * dt;
            if (pos >= 1.f) {
                pos = 1.f; dir = -1.f;
                ++bounceCount;
            }
            if (pos <= 0.f) {
                pos = 0.f; dir =  1.f;
            }
            if (bounceCount >= maxBounces) {
                lockAsMiss();
            }
        } else {
            lockTimer += dt;
        }
    }
};


struct Fighter {
    std::string name;
    float x, y;
    float hp, maxHp;
    float spriteW, spriteH;
    float r, g, b;
    bool  dead = false;
    // danos das habilidades proprias
    float dmg1 = 28.f, dmg2 = 35.f;
    float heal2 = 0.f;       // cura do movimento 2 (GigaDreno)
    // nomes das habilidades
    std::string moveName1, moveName2;
    AttackType  moveType1 = AttackType::FireRay;
    AttackType  moveType2 = AttackType::FireCircle;
    // modelo visual
    ElemType elemType = ElemType::Fire;
    int      sizeLevel = 0;   // 0=pequeno, 1=medio, 2=grande
};

struct BattleScene {
    Fighter player;
    Fighter enemy;

    BattleState state       = BattleState::ChooseMove;
    int         selectedMove = 0;
    AttackType  currentAttack = AttackType::None;

    ParticleSystem particles;
    ParticleSystem enemyParticles;

    FireRayAttack    fireRay;
    FireCircleAttack fireCircle;
    BubblesAttack    bubblesAtk;
    WaterfallAttack  waterfall;

    // versões de ataque usadas pelo inimigo (usam enemyParticles)
    FireRayAttack    enemyFireRay;
    FireCircleAttack enemyFireCircle;

    // segundo sistema para o jogador (Crustolid precisa de bolhas + fogo)
    BubblesAttack    playerBubbles;

    // ataques do tipo Grama
    RazorLeafAttack  razorLeaf;
    GigaDrainAttack  gigaDrain;
    RazorLeafAttack  enemyRazorLeaf;
    GigaDrainAttack  enemyGigaDrain;

    float animCooldown  = 0;
    float enemyShake    = 0;
    float flashAlpha    = 0;
    float playerFlash   = 0;
    float playerShake   = 0;
    float gameOverTimer = 0;

    TimingBarState timingBar;
    float dmgMultiplier = 1.f;  // definido pelo timing bar antes de atacar

    BattleScene()
        : fireRay(particles), fireCircle(particles),
          bubblesAtk(enemyParticles), waterfall(enemyParticles),
          enemyFireRay(enemyParticles), enemyFireCircle(enemyParticles),
          playerBubbles(particles),
          razorLeaf(particles), gigaDrain(particles),
          enemyRazorLeaf(enemyParticles), enemyGigaDrain(enemyParticles) {}

    void reset(const RockDef& enemyDef) {
        // Reconstroi fighters a partir das definicoes globais
        const RockDef& rd = ROCK_DEFS[g_chosenRock];
        player.name      = rd.name;
        player.x = 220; player.y = 165;
        player.maxHp = getPlayerMaxHpForStage(rd.maxHp, g_battleStage);
        player.hp = player.maxHp;
        player.r = rd.r; player.g = rd.g; player.b = rd.b;
        player.dead = false;
        player.dmg1 = rd.dmg1; player.dmg2 = rd.dmg2;
        player.heal2 = rd.heal2;
        player.moveName1 = rd.move1Name; player.moveName2 = rd.move2Name;
        player.moveType1 = rd.move1; player.moveType2 = rd.move2;
        player.elemType  = rd.elemType;
        player.sizeLevel = g_battleStage;   // 0=pequeno, 1=medio, 2=grande

        // Tamanho do sprite do jogador escala com a fase
        if (g_battleStage == 0) {
            player.spriteW = 72; player.spriteH = 48;
            player.y = 155;
        } else if (g_battleStage == 1) {
            player.spriteW = 108; player.spriteH = 72;
            player.y = 155;
        } else {
            player.spriteW = 160; player.spriteH = 110;
            player.y = 155;
        }

        enemy.name       = enemyDef.name;
        enemy.x = 1020; enemy.y = 490;
        enemy.hp = enemy.maxHp = enemyDef.maxHp;
        enemy.r = enemyDef.r; enemy.g = enemyDef.g; enemy.b = enemyDef.b;
        enemy.dead = false;
        enemy.dmg1 = enemyDef.dmg1; enemy.dmg2 = enemyDef.dmg2;
        enemy.heal2 = enemyDef.heal2;
        enemy.moveName1 = enemyDef.move1Name; enemy.moveName2 = enemyDef.move2Name;
        enemy.moveType1 = enemyDef.move1; enemy.moveType2 = enemyDef.move2;
        enemy.elemType  = enemyDef.elemType;
        enemy.sizeLevel = g_battleStage;

        // Tamanho do sprite do inimigo escala com a fase
        if (g_battleStage == 0) {
            enemy.spriteW = 80; enemy.spriteH = 56;
            enemy.y = 490;
        } else if (g_battleStage == 1) {
            enemy.spriteW = 120; enemy.spriteH = 84;
            enemy.y = 455;
        } else {
            enemy.spriteW = 180; enemy.spriteH = 130;
            enemy.y = 430;
        }

        state          = BattleState::ChooseMove;
        selectedMove   = 0;
        currentAttack  = AttackType::None;
        particles.clear(); enemyParticles.clear();
        fireRay.active    = false; fireRay.timer    = 0;
        fireCircle.active = false; fireCircle.timer = 0;
        bubblesAtk.active = false; bubblesAtk.timer = 0; bubblesAtk.bubbles.clear();
        waterfall.active  = false; waterfall.timer  = 0;
        playerBubbles.active = false; playerBubbles.timer = 0; playerBubbles.bubbles.clear();
        razorLeaf.active = false; razorLeaf.timer = 0; razorLeaf.leaves.clear();
        gigaDrain.active = false; gigaDrain.timer = 0; gigaDrain.healed = false;
        // Reset enemy attack instances as well (they use enemyParticles)
        enemyFireRay.active = false; enemyFireRay.timer = 0;
        enemyFireCircle.active = false; enemyFireCircle.timer = 0;
        enemyRazorLeaf.active = false; enemyRazorLeaf.timer = 0; enemyRazorLeaf.leaves.clear();
        enemyGigaDrain.active = false; enemyGigaDrain.timer = 0; enemyGigaDrain.healed = false;
        timingBar.active = false; timingBar.locked = false; timingBar.pos = 0.f; timingBar.ignoreInput = false;
        timingBar.bounceCount = 0;
        dmgMultiplier = 1.f;
        animCooldown  = 0; enemyShake = 0; flashAlpha = 0;
        playerFlash   = 0; playerShake = 0; gameOverTimer = 0;
    }

    bool isAnimating() const {
        return state == BattleState::PlayerAttacking ||
               state == BattleState::EnemyAttacking  ||
               state == BattleState::TimingBar        ||
               fireRay.active || fireCircle.active    ||
               bubblesAtk.active || waterfall.active  ||
               playerBubbles.active || razorLeaf.active ||
               gigaDrain.active || animCooldown > 0;
    }

    bool isOver() const {
        return state == BattleState::PlayerWon || state == BattleState::EnemyWon;
    }

    // Abre o mini-game de timing antes de lancar o ataque
    void startTimingBar(AttackType type) {
        if (isAnimating() || isOver()) return;
        state = BattleState::TimingBar;
        timingBar.start(type);
    }

    void launchPlayerAttack(AttackType type) {
        if (isAnimating() || isOver()) return;
        currentAttack = type;
        state = BattleState::PlayerAttacking;
        if (type == AttackType::FireRay) {
            fireRay.start(player.x + 40, player.y + 30, enemy.x - 20, enemy.y + 10);
        } else if (type == AttackType::FireCircle) {
            fireCircle.start(enemy.x, enemy.y + 10);
        } else if (type == AttackType::Bubbles) {
            playerBubbles.start(player.x, player.y + 20, enemy.x - 20, enemy.y + 10);
        } else if (type == AttackType::Waterfall) {
            waterfall.start(enemy.x, enemy.y + 10);
        } else if (type == AttackType::RazorLeaf) {
            razorLeaf.start(player.x + 36, player.y + 24, enemy.x - 20, enemy.y + 20);
        } else if (type == AttackType::GigaDrain) {
            gigaDrain.start(enemy.x - 20, enemy.y + 20, player.x + 36, player.y + 24);
        }
    }

    void launchEnemyAttack() {
        int pick = std::uniform_int_distribution<int>(0,1)(rng);
        AttackType et = (pick == 0) ? enemy.moveType1 : enemy.moveType2;
        currentAttack = et;
        state = BattleState::EnemyAttacking;
        if (et == AttackType::Bubbles) {
            bubblesAtk.start(enemy.x - 30, enemy.y + 20, player.x + 20, player.y + 20);
        } else if (et == AttackType::Waterfall) {
            waterfall.start(player.x + 10, player.y + 20);
        } else if (et == AttackType::FireRay) {
            enemyFireRay.start(enemy.x - 30, enemy.y + 20, player.x + 20, player.y + 20);
        } else if (et == AttackType::FireCircle) {
            enemyFireCircle.start(player.x, player.y + 10);
        } else if (et == AttackType::RazorLeaf) {
            // inimigo usa folhas em direcao ao jogador (versao inimiga)
            enemyRazorLeaf.start(enemy.x - 20, enemy.y + 20, player.x + 36, player.y + 24);
        } else if (et == AttackType::GigaDrain) {
            // Drena do jogador para o inimigo
            enemyGigaDrain.start(player.x + 36, player.y + 24, enemy.x - 20, enemy.y + 20);
        }
    }

    void applyPlayerDamage() {
        float dmg = (currentAttack == enemy.moveType1) ? enemy.dmg1 : enemy.dmg2;
        player.hp   = std::max(0.f, player.hp - dmg);
        playerShake = 0.35f; playerFlash = 0.7f;
        if (currentAttack == AttackType::GigaDrain && enemy.heal2 > 0.f) {
            enemy.hp = std::min(enemy.maxHp, enemy.hp + enemy.heal2 * dmgMultiplier);
        }
        if (player.hp <= 0) { player.dead = true; state = BattleState::EnemyWon; }
        else                { state = BattleState::ChooseMove; }
        currentAttack = AttackType::None;
    }

    void applyEnemyDamage() {
        float dmg = (currentAttack == player.moveType1) ? player.dmg1 : player.dmg2;
        dmg *= dmgMultiplier;
        enemy.hp   = std::max(0.f, enemy.hp - dmg);
        enemyShake = 0.35f; flashAlpha = 0.7f;
        // GigaDreno cura o jogador (proporcional ao timing)
        if (currentAttack == AttackType::GigaDrain && player.heal2 > 0.f) {
            player.hp = std::min(player.maxHp, player.hp + player.heal2 * dmgMultiplier);
            playerFlash = 0.5f;
        }
        dmgMultiplier = 1.f; // reset para proximo ataque
        if (enemy.hp <= 0) { enemy.dead = true; state = BattleState::PlayerWon; }
    }

    void update(float dt) {
        // ---- Timing Bar: atualiza e faz transicao para o ataque ----
        if (state == BattleState::TimingBar) {
            timingBar.update(dt);
            particles.update(dt); enemyParticles.update(dt);
            if (timingBar.isDone()) {
                dmgMultiplier = timingBar.multiplier;
                timingBar.active = false;
                // Temporariamente volta para ChooseMove para que
                // launchPlayerAttack possa ser chamado sem ser bloqueado
                state = BattleState::ChooseMove;
                launchPlayerAttack(timingBar.pendingAttack);
            }
            return;
        }

        if (isOver()) {
            gameOverTimer += dt;
            particles.update(dt); enemyParticles.update(dt);
            if (enemyShake  > 0) enemyShake  = std::max(0.f, enemyShake  - dt);
            if (flashAlpha  > 0) flashAlpha  = std::max(0.f, flashAlpha  - dt * 1.5f);
            if (playerShake > 0) playerShake = std::max(0.f, playerShake - dt);
            if (playerFlash > 0) playerFlash = std::max(0.f, playerFlash - dt * 1.5f);

            // Se o jogador venceu e ainda nao terminou as 3 fases, avanca para proxima fase
            if (state == BattleState::PlayerWon && gameOverTimer > 1.2f) {
                if (g_battleStage < 2) {
                    g_battleStage++;
                    reset(getEnemyForStage(g_chosenRock, g_battleStage));
                    return;
                }
            }

            return;
        }

        fireRay.update(dt); fireCircle.update(dt);
        enemyFireRay.update(dt); enemyFireCircle.update(dt);
        enemyRazorLeaf.update(dt); enemyGigaDrain.update(dt);
        bubblesAtk.update(dt); waterfall.update(dt);
        // enemy razor/giga use same instances as player but their particle effects
        // are spawned into the shared systems; update them as well
        playerBubbles.update(dt);
        razorLeaf.update(dt); gigaDrain.update(dt);
        particles.update(dt); enemyParticles.update(dt);

        // Turno do jogador: espera animacao terminar -> aplica dano -> turno inimigo
        if (state == BattleState::PlayerAttacking) {
            bool done = !fireRay.active && !fireCircle.active &&
                        !playerBubbles.active && !waterfall.active &&
                        !razorLeaf.active && !gigaDrain.active;
            if (done && animCooldown <= 0) animCooldown = 0.03f;
            if (done && animCooldown > 0) {
                animCooldown -= dt;
                if (animCooldown <= 0) {
                    applyEnemyDamage();
                    if (!isOver()) {
                        animCooldown  = 0.18f;
                        state         = BattleState::EnemyAttacking;
                        currentAttack = AttackType::None;
                    }
                }
            }
        }

        // Turno do inimigo: pausa -> lanca ataque -> aplica dano
        if (state == BattleState::EnemyAttacking) {
            bool done = !bubblesAtk.active && !waterfall.active &&
                        !enemyFireRay.active && !enemyFireCircle.active &&
                        !enemyRazorLeaf.active && !enemyGigaDrain.active;
            if (currentAttack == AttackType::None && animCooldown > 0) {
                animCooldown -= dt;
                if (animCooldown <= 0) launchEnemyAttack();
            } else if (currentAttack != AttackType::None && done && animCooldown <= 0) {
                animCooldown = 0.03f;
            } else if (currentAttack != AttackType::None && done && animCooldown > 0) {
                animCooldown -= dt;
                if (animCooldown <= 0) applyPlayerDamage();
            }
        }

        if (enemyShake  > 0) enemyShake  = std::max(0.f, enemyShake  - dt);
        if (flashAlpha  > 0) flashAlpha  = std::max(0.f, flashAlpha  - dt * 1.5f);
        if (playerShake > 0) playerShake = std::max(0.f, playerShake - dt);
        if (playerFlash > 0) playerFlash = std::max(0.f, playerFlash - dt * 1.5f);
    }
};

static BattleScene battle;

// ---------------------------------------------------------------------------
// Tela de selecao de personagem
// ---------------------------------------------------------------------------
struct CharSelectState {
    int  cursor      = 0;   // 0..2
    bool confirmed   = false;
    float titlePulse = 0.f;

    // Particulas de preview por pedra
    ParticleSystem previews[3];
    float previewTimer = 0.f;

    void update(float dt) {
        titlePulse  += dt;
        previewTimer += dt;

        // Pequenas particulas ao redor de cada card
        for (int i = 0; i < 3; ++i) {
            const RockDef& rd = ROCK_DEFS[i];
            if (previewTimer > 0.06f) {
                Particle p;
                float cx = 220.f + i * 280.f + 100.f; // centro do card
                float cy = 360.f;
                p.x = cx + randf(-40, 40);
                p.y = cy + randf(-40, 40);
                p.vx = randf(-20, 20); p.vy = randf(10, 50);
                p.life = p.maxLife = randf(0.5f, 1.2f);
                p.size = randf(3, 7);
                p.r = rd.r + randf(-0.1f, 0.1f);
                p.g = rd.g + randf(-0.1f, 0.1f);
                p.b = rd.b + randf(-0.1f, 0.1f);
                previews[i].spawn(p);
            }
            previews[i].update(dt);
        }
        if (previewTimer > 0.06f) previewTimer = 0.f;
    }
};

static CharSelectState charSelect;

// ---------------------------------------------------------------------------
// Estado da tela de selecao de dificuldade
// ---------------------------------------------------------------------------
struct DifficultySelectState {
    int   cursor     = 0;   // 0..2
    float titlePulse = 0.f;

    ParticleSystem previews[3];
    float previewTimer = 0.f;

    void update(float dt) {
        titlePulse  += dt;
        previewTimer += dt;

        for (int i = 0; i < 3; ++i) {
            const DifficultyDef& dd = DIFFICULTY_DEFS[i];
            if (previewTimer > 0.06f) {
                Particle p;
                float cx = 220.f + i * 280.f + 100.f;
                float cy = 360.f;
                p.x = cx + randf(-40, 40);
                p.y = cy + randf(-40, 40);
                p.vx = randf(-20, 20); p.vy = randf(10, 50);
                p.life = p.maxLife = randf(0.5f, 1.2f);
                p.size = randf(3, 7);
                p.r = dd.r + randf(-0.1f, 0.1f);
                p.g = dd.g + randf(-0.1f, 0.1f);
                p.b = dd.b + randf(-0.1f, 0.1f);
                previews[i].spawn(p);
            }
            previews[i].update(dt);
        }
        if (previewTimer > 0.06f) previewTimer = 0.f;
    }
};

static DifficultySelectState difficultySelect;

// ---------------------------------------------------------------------------
// Desenho primitivo
// ---------------------------------------------------------------------------
static void drawRect(float x, float y, float w, float h,
                     float r, float g, float b, float a = 1) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

static void drawRoundedPanel(float x, float y, float w, float h,
                             float r, float g, float b, float border = 3) {
    drawRect(x - border, y - border, w + border*2, h + border*2, 0.15f, 0.15f, 0.2f);
    drawRect(x, y, w, h, r, g, b);
}

static void drawHPBar(float x, float y, float w, float h, float ratio, bool /*isEnemy*/) {
    drawRect(x, y, w, h, 0.2f, 0.2f, 0.25f);
    float cr = ratio > 0.5f ? 0.2f  : (ratio > 0.25f ? 0.9f  : 0.95f);
    float cg = ratio > 0.5f ? 0.85f : (ratio > 0.25f ? 0.75f : 0.2f);
    float cb = ratio > 0.5f ? 0.25f : (ratio > 0.25f ? 0.15f : 0.2f);
    drawRect(x + 2, y + 2, (w - 4)*std::clamp(ratio, 0.f, 1.f), h - 4, cr, cg, cb);
}

// ---------------------------------------------------------------------------
// drawFighter — 3 tamanhos x 3 elementos (9 modelos + o original = base)
// ---------------------------------------------------------------------------
static void drawFighterSmall(const Fighter& f, float ox, float oy) {
    // Modelo PEQUENO original — bloco robusto de pedra
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    // Sombra
    drawRect(x + W*0.1f, y - 8, W*0.8f, 10, 0.f, 0.f, 0.f, 0.25f);
    // Corpo
    drawRect(x + W*0.08f, y + H*0.08f, W*0.84f, H*0.84f,
             f.r*0.7f, f.g*0.7f, f.b*0.7f);
    drawRect(x + W*0.15f, y + H*0.20f, W*0.70f, H*0.68f, f.r, f.g, f.b);
    // Rachadura horizontal
    drawRect(x + W*0.20f, y + H - H*0.48f, W*0.52f, H*0.10f,
             f.r*0.85f, f.g*0.85f, f.b*0.85f);
    // Rachadura vertical
    drawRect(x + W*0.43f, y + H*0.24f, W*0.05f, H*0.52f,
             f.r*0.6f, f.g*0.6f, f.b*0.6f);
    // Olhos
    drawRect(x + W*0.28f, y + H*0.55f, 9, 9, 0.9f, 1.f, 1.f);
    drawRect(x + W*0.55f, y + H*0.55f, 9, 9, 0.9f, 1.f, 1.f);
    drawRect(x + W*0.31f, y + H*0.58f, 5, 5, 0.1f, 0.3f, 0.5f);
    drawRect(x + W*0.58f, y + H*0.58f, 5, 5, 0.1f, 0.3f, 0.5f);
}

static void drawFighterMedium_Fire(const Fighter& f, float ox, float oy) {
    // MÉDIO Fogo — pedra vulcânica angular com ombros salientes
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    // Sombra oval
    drawRect(x + W*0.05f, y - 10, W*0.9f, 14, 0.f, 0.f, 0.f, 0.28f);
    // Pernas (dois blocos pequenos)
    drawRect(x + W*0.12f, y,        W*0.22f, H*0.30f, f.r*0.65f, f.g*0.60f, f.b*0.50f);
    drawRect(x + W*0.66f, y,        W*0.22f, H*0.30f, f.r*0.65f, f.g*0.60f, f.b*0.50f);
    // Torso largo
    drawRect(x + W*0.06f, y + H*0.26f, W*0.88f, H*0.54f,
             f.r*0.72f, f.g*0.65f, f.b*0.55f);
    drawRect(x + W*0.13f, y + H*0.32f, W*0.74f, H*0.44f, f.r, f.g, f.b);
    // Ombros largos
    drawRect(x - W*0.06f, y + H*0.52f, W*0.22f, H*0.24f,
             f.r*0.80f, f.g*0.72f, f.b*0.60f);
    drawRect(x + W*0.84f, y + H*0.52f, W*0.22f, H*0.24f,
             f.r*0.80f, f.g*0.72f, f.b*0.60f);
    // Cabeça
    drawRect(x + W*0.22f, y + H*0.72f, W*0.56f, H*0.30f,
             f.r*0.85f, f.g*0.78f, f.b*0.65f);
    // Veias de magma no torso
    drawRect(x + W*0.30f, y + H*0.34f, W*0.06f, H*0.38f, 1.f, 0.45f, 0.05f, 0.85f);
    drawRect(x + W*0.55f, y + H*0.36f, W*0.04f, H*0.30f, 1.f, 0.35f, 0.05f, 0.70f);
    // Olhos brilhantes
    drawRect(x + W*0.31f, y + H*0.78f, 11, 11, 1.f, 0.85f, 0.2f);
    drawRect(x + W*0.56f, y + H*0.78f, 11, 11, 1.f, 0.85f, 0.2f);
    drawRect(x + W*0.34f, y + H*0.81f, 6,  6,  0.8f, 0.1f, 0.f);
    drawRect(x + W*0.59f, y + H*0.81f, 6,  6,  0.8f, 0.1f, 0.f);
}

static void drawFighterMedium_Water(const Fighter& f, float ox, float oy) {
    // MÉDIO Água — pedra arredondada com estrutura em camadas como coral
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    drawRect(x + W*0.08f, y - 10, W*0.84f, 13, 0.f, 0.f, 0.f, 0.22f);
    // Base larga arredondada
    drawRect(x + W*0.04f, y,        W*0.92f, H*0.30f, f.r*0.70f, f.g*0.80f, f.b*0.90f);
    // Camadas empilhadas
    drawRect(x + W*0.10f, y + H*0.26f, W*0.80f, H*0.22f,
             f.r*0.75f, f.g*0.85f, f.b*0.95f);
    drawRect(x + W*0.16f, y + H*0.44f, W*0.68f, H*0.20f, f.r, f.g, f.b);
    drawRect(x + W*0.22f, y + H*0.60f, W*0.56f, H*0.20f,
             f.r*0.90f, f.g*0.95f, f.b*1.00f);
    // Cabeça oval
    drawRect(x + W*0.28f, y + H*0.76f, W*0.44f, H*0.26f,
             f.r*0.85f, f.g*0.92f, f.b*1.00f);
    // Linhas de camada (separações)
    drawRect(x + W*0.06f, y + H*0.24f, W*0.88f, H*0.035f,
             f.r*0.55f, f.g*0.70f, f.b*0.90f);
    drawRect(x + W*0.12f, y + H*0.42f, W*0.76f, H*0.035f,
             f.r*0.55f, f.g*0.70f, f.b*0.90f);
    drawRect(x + W*0.18f, y + H*0.58f, W*0.64f, H*0.035f,
             f.r*0.55f, f.g*0.70f, f.b*0.90f);
    // Cristais de gelo/água nas laterais
    drawRect(x - W*0.04f, y + H*0.30f, W*0.12f, H*0.34f, 0.65f, 0.88f, 1.f, 0.75f);
    drawRect(x + W*0.92f, y + H*0.30f, W*0.12f, H*0.34f, 0.65f, 0.88f, 1.f, 0.75f);
    // Olhos
    drawRect(x + W*0.34f, y + H*0.81f, 10, 10, 0.88f, 1.f, 1.f);
    drawRect(x + W*0.58f, y + H*0.81f, 10, 10, 0.88f, 1.f, 1.f);
    drawRect(x + W*0.37f, y + H*0.84f, 5,  5,  0.1f,  0.2f, 0.6f);
    drawRect(x + W*0.61f, y + H*0.84f, 5,  5,  0.1f,  0.2f, 0.6f);
}

static void drawFighterMedium_Plant(const Fighter& f, float ox, float oy) {
    // MÉDIO Planta — pedra com raízes saindo dos lados e musgo abundante
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    drawRect(x + W*0.08f, y - 10, W*0.84f, 13, 0.f, 0.f, 0.f, 0.20f);
    // Raízes laterais
    drawRect(x - W*0.12f, y + H*0.08f, W*0.20f, H*0.12f,
             0.18f, 0.48f, 0.10f);
    drawRect(x + W*0.92f, y + H*0.08f, W*0.20f, H*0.12f,
             0.18f, 0.48f, 0.10f);
    drawRect(x - W*0.08f, y,            W*0.16f, H*0.10f,
             0.15f, 0.40f, 0.08f);
    drawRect(x + W*0.92f, y,            W*0.16f, H*0.10f,
             0.15f, 0.40f, 0.08f);
    // Corpo principal
    drawRect(x + W*0.08f, y + H*0.04f, W*0.84f, H*0.68f,
             f.r*0.72f, f.g*0.72f, f.b*0.60f);
    drawRect(x + W*0.14f, y + H*0.10f, W*0.72f, H*0.58f, f.r, f.g, f.b);
    // Musgo no topo
    drawRect(x + W*0.10f, y + H*0.66f, W*0.80f, H*0.16f,
             0.12f, 0.62f, 0.14f);
    drawRect(x + W*0.18f, y + H*0.78f, W*0.64f, H*0.10f,
             0.18f, 0.72f, 0.20f);
    // Cabeça
    drawRect(x + W*0.24f, y + H*0.76f, W*0.52f, H*0.26f,
             f.r*0.85f, f.g*0.88f, f.b*0.70f);
    // Pequenos brotos no topo da cabeça
    drawRect(x + W*0.36f, y + H*0.98f, W*0.08f, H*0.10f, 0.18f, 0.80f, 0.20f);
    drawRect(x + W*0.54f, y + H*0.98f, W*0.08f, H*0.10f, 0.15f, 0.70f, 0.18f);
    // Olhos
    drawRect(x + W*0.33f, y + H*0.80f, 10, 10, 0.88f, 1.f, 0.75f);
    drawRect(x + W*0.57f, y + H*0.80f, 10, 10, 0.88f, 1.f, 0.75f);
    drawRect(x + W*0.36f, y + H*0.83f, 5,  5,  0.1f,  0.35f, 0.1f);
    drawRect(x + W*0.60f, y + H*0.83f, 5,  5,  0.1f,  0.35f, 0.1f);
}

static void drawFighterLarge_Fire(const Fighter& f, float ox, float oy) {
    // GRANDE Fogo — golem de magma tipo Tiny: corpo maciço, braços cilíndricos
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    // Sombra
    drawRect(x + W*0.02f, y - 14, W*0.96f, 18, 0.f, 0.f, 0.f, 0.35f);
    // Pernas curtas e largas
    drawRect(x + W*0.06f, y,        W*0.30f, H*0.25f, f.r*0.60f, f.g*0.55f, f.b*0.42f);
    drawRect(x + W*0.64f, y,        W*0.30f, H*0.25f, f.r*0.60f, f.g*0.55f, f.b*0.42f);
    // Corpo central massivo
    drawRect(x + W*0.04f, y + H*0.20f, W*0.92f, H*0.55f,
             f.r*0.70f, f.g*0.62f, f.b*0.50f);
    drawRect(x + W*0.10f, y + H*0.26f, W*0.80f, H*0.45f, f.r, f.g, f.b);
    // Fissuras de magma no torso (linhas de luz)
    drawRect(x + W*0.22f, y + H*0.28f, W*0.08f, H*0.40f, 1.f, 0.50f, 0.05f, 0.90f);
    drawRect(x + W*0.50f, y + H*0.30f, W*0.06f, H*0.36f, 1.f, 0.38f, 0.02f, 0.80f);
    drawRect(x + W*0.68f, y + H*0.32f, W*0.05f, H*0.28f, 1.f, 0.55f, 0.10f, 0.70f);
    // Braços largos saindo dos lados
    drawRect(x - W*0.14f, y + H*0.50f, W*0.22f, H*0.38f,
             f.r*0.78f, f.g*0.70f, f.b*0.55f);
    drawRect(x + W*0.92f, y + H*0.50f, W*0.22f, H*0.38f,
             f.r*0.78f, f.g*0.70f, f.b*0.55f);
    // Punhos
    drawRect(x - W*0.18f, y + H*0.34f, W*0.24f, H*0.20f,
             f.r*0.85f, f.g*0.78f, f.b*0.62f);
    drawRect(x + W*0.94f, y + H*0.34f, W*0.24f, H*0.20f,
             f.r*0.85f, f.g*0.78f, f.b*0.62f);
    // Cabeça pequena em relação ao corpo (estilo Tiny)
    drawRect(x + W*0.28f, y + H*0.72f, W*0.44f, H*0.32f,
             f.r*0.88f, f.g*0.80f, f.b*0.65f);
    // Chifres/pontas no topo
    drawRect(x + W*0.30f, y + H*0.98f, W*0.08f, H*0.10f,
             f.r*0.70f, f.g*0.60f, f.b*0.48f);
    drawRect(x + W*0.58f, y + H*0.96f, W*0.10f, H*0.12f,
             f.r*0.70f, f.g*0.60f, f.b*0.48f);
    // Olhos grandes brilhantes (magma)
    drawRect(x + W*0.33f, y + H*0.78f, 14, 14, 1.f, 0.80f, 0.10f);
    drawRect(x + W*0.57f, y + H*0.78f, 14, 14, 1.f, 0.80f, 0.10f);
    drawRect(x + W*0.37f, y + H*0.82f, 7,  7,  0.9f, 0.15f, 0.f);
    drawRect(x + W*0.61f, y + H*0.82f, 7,  7,  0.9f, 0.15f, 0.f);
    // Brilho interior nos olhos
    drawRect(x + W*0.34f, y + H*0.80f, 4, 4, 1.f, 1.f, 0.6f, 0.8f);
    drawRect(x + W*0.58f, y + H*0.80f, 4, 4, 1.f, 1.f, 0.6f, 0.8f);
}

static void drawFighterLarge_Water(const Fighter& f, float ox, float oy) {
    // GRANDE Água — monolito de pedra aquática, largo e alto, com correntes
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    drawRect(x + W*0.04f, y - 14, W*0.92f, 18, 0.f, 0.f, 0.f, 0.28f);
    // Pernas grossas
    drawRect(x + W*0.08f, y,        W*0.28f, H*0.22f, f.r*0.65f, f.g*0.75f, f.b*0.88f);
    drawRect(x + W*0.64f, y,        W*0.28f, H*0.22f, f.r*0.65f, f.g*0.75f, f.b*0.88f);
    // Corpo principal em bloco largo
    drawRect(x + W*0.02f, y + H*0.18f, W*0.96f, H*0.58f,
             f.r*0.72f, f.g*0.82f, f.b*0.94f);
    drawRect(x + W*0.08f, y + H*0.24f, W*0.84f, H*0.48f, f.r, f.g, f.b);
    // Camadas horizontais de estratificação
    for (int i = 0; i < 4; ++i) {
        float yy = y + H*(0.26f + i*0.11f);
        drawRect(x + W*0.08f, yy, W*0.84f, H*0.018f,
                 f.r*0.55f, f.g*0.72f, f.b*0.92f);
    }
    // Braços — colunas de água cristalizada
    drawRect(x - W*0.12f, y + H*0.40f, W*0.18f, H*0.44f,
             f.r*0.78f, f.g*0.88f, f.b*0.98f);
    drawRect(x + W*0.94f, y + H*0.40f, W*0.18f, H*0.44f,
             f.r*0.78f, f.g*0.88f, f.b*0.98f);
    // Cristais nos braços
    drawRect(x - W*0.15f, y + H*0.80f, W*0.22f, H*0.14f,
             0.70f, 0.92f, 1.f, 0.80f);
    drawRect(x + W*0.93f, y + H*0.80f, W*0.22f, H*0.14f,
             0.70f, 0.92f, 1.f, 0.80f);
    // Cabeça larga
    drawRect(x + W*0.22f, y + H*0.74f, W*0.56f, H*0.30f,
             f.r*0.88f, f.g*0.94f, f.b*1.00f);
    // Ondas no topo da cabeça
    drawRect(x + W*0.24f, y + H*0.98f, W*0.52f, H*0.05f,
             0.60f, 0.88f, 1.f, 0.70f);
    // Olhos
    drawRect(x + W*0.32f, y + H*0.80f, 13, 13, 0.85f, 1.f, 1.f);
    drawRect(x + W*0.58f, y + H*0.80f, 13, 13, 0.85f, 1.f, 1.f);
    drawRect(x + W*0.36f, y + H*0.84f, 7,  7,  0.05f, 0.15f, 0.65f);
    drawRect(x + W*0.62f, y + H*0.84f, 7,  7,  0.05f, 0.15f, 0.65f);
    drawRect(x + W*0.33f, y + H*0.81f, 4, 4, 0.7f, 0.95f, 1.f, 0.85f);
    drawRect(x + W*0.59f, y + H*0.81f, 4, 4, 0.7f, 0.95f, 1.f, 0.85f);
}

static void drawFighterLarge_Plant(const Fighter& f, float ox, float oy) {
    // GRANDE Planta — pedra-árvore massiva, raízes largas, copa de vegetação
    float x = ox, y = oy;
    float W = f.spriteW, H = f.spriteH;
    drawRect(x + W*0.04f, y - 14, W*0.92f, 18, 0.f, 0.f, 0.f, 0.22f);
    // Raízes espessas (base)
    drawRect(x - W*0.20f, y,        W*0.30f, H*0.18f, 0.16f, 0.42f, 0.08f);
    drawRect(x + W*0.90f, y,        W*0.30f, H*0.18f, 0.16f, 0.42f, 0.08f);
    drawRect(x - W*0.12f, y + H*0.14f, W*0.22f, H*0.12f, 0.18f, 0.48f, 0.10f);
    drawRect(x + W*0.90f, y + H*0.14f, W*0.22f, H*0.12f, 0.18f, 0.48f, 0.10f);
    // Corpo (tronco de pedra)
    drawRect(x + W*0.08f, y + H*0.10f, W*0.84f, H*0.62f,
             f.r*0.72f, f.g*0.72f, f.b*0.58f);
    drawRect(x + W*0.14f, y + H*0.16f, W*0.72f, H*0.52f, f.r, f.g, f.b);
    // Veias de seiva (linhas verde-brilhante)
    drawRect(x + W*0.28f, y + H*0.18f, W*0.06f, H*0.48f, 0.25f, 0.95f, 0.28f, 0.75f);
    drawRect(x + W*0.56f, y + H*0.20f, W*0.05f, H*0.44f, 0.20f, 0.85f, 0.22f, 0.65f);
    // Braços-galho
    drawRect(x - W*0.16f, y + H*0.46f, W*0.28f, H*0.30f,
             f.r*0.80f, f.g*0.80f, f.b*0.64f);
    drawRect(x + W*0.88f, y + H*0.46f, W*0.28f, H*0.30f,
             f.r*0.80f, f.g*0.80f, f.b*0.64f);
    // Folhas nos braços
    drawRect(x - W*0.22f, y + H*0.72f, W*0.32f, H*0.16f, 0.14f, 0.72f, 0.16f, 0.85f);
    drawRect(x + W*0.90f, y + H*0.72f, W*0.32f, H*0.16f, 0.14f, 0.72f, 0.16f, 0.85f);
    // Copa de vegetação no topo
    drawRect(x + W*0.06f, y + H*0.70f, W*0.88f, H*0.22f,
             0.12f, 0.65f, 0.14f);
    drawRect(x + W*0.12f, y + H*0.88f, W*0.76f, H*0.16f,
             0.18f, 0.78f, 0.20f);
    // Cabeça
    drawRect(x + W*0.26f, y + H*0.74f, W*0.48f, H*0.28f,
             f.r*0.88f, f.g*0.90f, f.b*0.72f);
    // Galhinhos/antenas
    drawRect(x + W*0.32f, y + H*0.98f, W*0.06f, H*0.10f, 0.20f, 0.88f, 0.22f);
    drawRect(x + W*0.50f, y + H*1.00f, W*0.08f, H*0.12f, 0.15f, 0.78f, 0.18f);
    drawRect(x + W*0.62f, y + H*0.98f, W*0.06f, H*0.09f, 0.20f, 0.85f, 0.22f);
    // Olhos
    drawRect(x + W*0.34f, y + H*0.78f, 13, 13, 0.82f, 1.f, 0.65f);
    drawRect(x + W*0.58f, y + H*0.78f, 13, 13, 0.82f, 1.f, 0.65f);
    drawRect(x + W*0.38f, y + H*0.82f, 7,  7,  0.08f, 0.30f, 0.08f);
    drawRect(x + W*0.62f, y + H*0.82f, 7,  7,  0.08f, 0.30f, 0.08f);
    drawRect(x + W*0.35f, y + H*0.79f, 4, 4, 0.65f, 1.f, 0.50f, 0.80f);
    drawRect(x + W*0.59f, y + H*0.79f, 4, 4, 0.65f, 1.f, 0.50f, 0.80f);
}

static void drawFighter(const Fighter& f, float shakeX = 0) {
    float ox = f.x - f.spriteW * 0.5f + shakeX;
    float oy = f.y;

    if (f.sizeLevel == 0) {
        drawFighterSmall(f, ox, oy);
    } else if (f.sizeLevel == 1) {
        switch (f.elemType) {
            case ElemType::Fire:  drawFighterMedium_Fire(f, ox, oy);  break;
            case ElemType::Water: drawFighterMedium_Water(f, ox, oy); break;
            case ElemType::Plant: drawFighterMedium_Plant(f, ox, oy); break;
        }
    } else {
        switch (f.elemType) {
            case ElemType::Fire:  drawFighterLarge_Fire(f, ox, oy);  break;
            case ElemType::Water: drawFighterLarge_Water(f, ox, oy); break;
            case ElemType::Plant: drawFighterLarge_Plant(f, ox, oy); break;
        }
    }
}

// ---------------------------------------------------------------------------
// stb_easy_font embutido (dominio publico)
// ---------------------------------------------------------------------------
#ifndef STB_EASY_FONT_INCLUDED
#define STB_EASY_FONT_INCLUDED
struct StbEasyFontColor { unsigned char c[4]; };
static struct { unsigned char advance, h_seg, v_seg; } stb_ef_charinfo[96] = {
    {6,0,0},{3,0,0},{5,1,1},{7,1,4},{7,3,7},{7,6,12},{7,8,19},{4,16,21},
    {4,17,22},{4,19,23},{23,21,24},{23,22,31},{20,23,34},{22,23,36},{19,24,36},{21,25,36},
    {6,25,39},{6,27,43},{6,28,45},{6,30,49},{6,33,53},{6,34,57},{6,40,58},{6,46,59},
    {6,47,62},{6,55,64},{19,57,68},{20,59,68},{21,61,69},{22,66,69},{21,68,69},{7,73,69},
    {9,75,74},{6,78,81},{6,80,85},{6,83,90},{6,85,91},{6,87,95},{6,90,96},{7,92,97},
    {6,96,102},{5,97,106},{6,99,107},{6,100,110},{6,100,115},{7,101,116},{6,101,121},
    {6,101,125},{6,102,129},{7,103,133},{6,104,140},{6,105,145},{7,107,149},{6,108,151},
    {7,109,155},{7,109,160},{7,109,165},{7,118,167},{6,118,172},{4,120,176},{6,122,177},
    {4,122,181},{23,124,182},{22,129,182},{4,130,182},{22,131,183},{6,133,187},{22,135,191},
    {6,137,192},{22,139,196},{6,144,197},{22,147,198},{6,150,202},{19,151,206},{21,152,207},
    {6,155,209},{3,160,210},{23,160,211},{22,164,216},{22,165,220},{22,167,224},{22,169,228},
    {21,171,232},{21,173,233},{5,178,233},{22,179,234},{23,180,238},{23,180,243},{23,180,248},
    {22,189,248},{22,191,252},{5,196,252},{3,203,252},{5,203,253},{22,210,253},{0,214,253},
};
static unsigned char stb_ef_hseg[214] = {
    97,37,69,84,28,51,2,18,10,49,98,41,65,25,81,105,33,9,97,1,97,37,37,36,81,10,98,107,3,100,3,99,
    58,51,4,99,58,8,73,81,10,50,98,8,73,81,4,10,50,98,8,25,33,65,81,10,50,17,65,97,25,33,25,49,9,65,20,
    68,1,65,25,49,41,11,105,13,101,76,10,50,10,50,98,11,99,10,98,11,50,99,11,50,11,99,8,57,58,3,99,99,107,
    10,10,11,10,99,11,5,100,41,65,57,41,65,9,17,81,97,3,107,9,97,1,97,33,25,9,25,41,100,41,26,82,42,98,27,
    83,42,98,26,51,82,8,41,35,8,10,26,82,114,42,1,114,8,9,73,57,81,41,97,18,8,8,25,26,26,82,26,82,26,82,41,
    25,33,82,26,49,73,35,90,17,81,41,65,57,41,65,25,81,90,114,20,84,73,57,41,49,25,33,65,81,9,97,1,97,25,
    33,65,81,57,33,25,41,25,
};
static unsigned char stb_ef_vseg[253] = {
    4,2,8,10,15,8,15,33,8,15,8,73,82,73,57,41,82,10,82,18,66,10,21,29,1,65,27,8,27,9,65,8,10,50,97,74,66,
    42,10,21,57,41,29,25,14,81,73,57,26,8,8,26,66,3,8,8,15,19,21,90,58,26,18,66,18,105,89,28,74,17,8,73,57,
    26,21,8,42,41,42,8,28,22,8,8,30,7,8,8,26,66,21,7,8,8,29,7,7,21,8,8,8,59,7,8,8,15,29,8,8,14,7,57,43,10,82,
    7,7,25,42,25,15,7,25,41,15,21,105,105,29,7,57,57,26,21,105,73,97,89,28,97,7,57,58,26,82,18,57,57,74,8,30,
    6,8,8,14,3,58,90,58,11,7,74,43,74,15,2,82,2,42,75,42,10,67,57,41,10,7,2,42,74,106,15,2,35,8,8,29,7,8,8,59,
    35,51,8,8,15,35,30,35,8,8,30,7,8,8,60,36,8,45,7,7,36,8,43,8,44,21,8,8,44,35,8,8,43,23,8,8,43,35,8,8,31,21,
    15,20,8,8,28,18,58,89,58,26,21,89,73,89,29,20,8,8,30,7,
};
static int stb_ef_draw_segs(float x, float y, unsigned char* segs, int n, int vert,
                            StbEasyFontColor col, char* buf, int bufSz, int off) {
    for (int i = 0; i < n; ++i) {
        int len = segs[i] & 7;
        x += float((segs[i] >> 3) & 1);
        if (len && off + 64 <= bufSz) {
            float y0 = y + float(segs[i] >> 4);
            for (int j = 0; j < 4; ++j) {
                *(float*)(buf + off + 0) = x + (j==1||j==2 ? (vert?1.f:float(len)) : 0.f);
                *(float*)(buf + off + 4) = y0 + (j>=2     ? (vert?float(len):1.f)  : 0.f);
                *(float*)(buf + off + 8) = 0.f;
                *(StbEasyFontColor*)(buf + off + 12) = col;
                off += 16;
            }
        }
    }
    return off;
}
static int stb_ef_print(float x, float y, const char* text, unsigned char color[4],
                        void* vbuf, int vbufSz) {
    char* buf = (char*)vbuf;
    float sx = x; int off = 0;
    StbEasyFontColor c = {255,255,255,255};
    if (color) { c.c[0]=color[0]; c.c[1]=color[1]; c.c[2]=color[2]; c.c[3]=color[3]; }
    while (*text && off < vbufSz) {
        if (*text == '\n') { y += 12; x = sx; }
        else if (*text >= 32 && *text < 128) {
            unsigned char adv = stb_ef_charinfo[*text-32].advance;
            float ych = (adv & 16) ? y+1 : y;
            int hs = stb_ef_charinfo[*text-32].h_seg;
            int vs = stb_ef_charinfo[*text-32].v_seg;
            int nh = stb_ef_charinfo[*text-32+1].h_seg - hs;
            int nv = stb_ef_charinfo[*text-32+1].v_seg - vs;
            off = stb_ef_draw_segs(x,ych,&stb_ef_hseg[hs],nh,0,c,buf,vbufSz,off);
            off = stb_ef_draw_segs(x,ych,&stb_ef_vseg[vs],nv,1,c,buf,vbufSz,off);
            x += adv & 15;
        }
        ++text;
    }
    return off / 64;
}
#endif

static void drawText(float x, float y, float scale, const char* text,
                     float r = 0.95f, float g = 0.95f, float b = 0.9f) {
    static char vbuf[65536];
    unsigned char col[4] = {
        (unsigned char)(r*255),(unsigned char)(g*255),(unsigned char)(b*255),255
    };
    int quads = stb_ef_print(0, 0, text, col, vbuf, sizeof(vbuf));
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, -scale, 1); // flip Y: stb cresce para baixo, OpenGL para cima
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 16, vbuf);
    glColor3f(r, g, b);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

// ---------------------------------------------------------------------------
// Render — Tela de selecao de personagem
// ---------------------------------------------------------------------------
static void renderCharSelect() {
    const float W = 1280, H = 720;

    // Fundo pedregoso escuro
    drawRect(0, 0, W, H, 0.08f, 0.07f, 0.06f);
    // Faixa superior
    drawRect(0, H - 80, W, 80, 0.12f, 0.10f, 0.08f);
    // Linha decorativa
    drawRect(0, H - 84, W, 4, 0.6f, 0.45f, 0.2f);

    // Titulo pulsante
    float pulse = 0.85f + 0.15f * std::sin(charSelect.titlePulse * 2.5f);
    drawText(W*0.5f - 320, H - 22, 5.0f * pulse, "Rockworld Gray Stone",
             0.95f, 0.80f, 0.30f);

    // Subtitulo
    drawText(W*0.5f - 200, H - 60, 2.2f, "Escolha sua pedra inicial:",
             0.85f, 0.82f, 0.75f);

    // Cards das 3 pedras
    for (int i = 0; i < 3; ++i) {
        const RockDef& rd = ROCK_DEFS[i];
        float cx = 180.f + i * 300.f;
        float cy = 140.f;
        float cw = 240.f, ch = 360.f;
        bool  sel = (charSelect.cursor == i);

        // Particulas de preview
        charSelect.previews[i].draw();

        // Borda extra se selecionado
        if (sel) {
            float t = std::sin(charSelect.titlePulse * 4.f) * 0.5f + 0.5f;
            drawRect(cx - 8, cy - 8, cw + 16, ch + 16,
                     rd.r * 0.8f + t * 0.2f,
                     rd.g * 0.8f + t * 0.2f,
                     rd.b * 0.8f + t * 0.2f, 0.7f);
        }

        // Card fundo
        float bright = sel ? 1.0f : 0.65f;
        drawRoundedPanel(cx, cy, cw, ch,
                         rd.r * 0.22f * bright,
                         rd.g * 0.22f * bright,
                         rd.b * 0.22f * bright, 4);

        // Mini sprite da pedra no card (posicionado abaixo dos textos)
        float sx = cx + cw*0.5f - 36;
        // colocar o sprite mais para baixo para não ser sobreposto pelos textos
        float sy = cy + 120;
        // Corpo
        drawRect(sx + 4,  sy,      64,  50, rd.r*0.65f, rd.g*0.65f, rd.b*0.65f);
        drawRect(sx + 8,  sy + 6,  56,  38, rd.r,       rd.g,       rd.b);
        // Rachadura
        drawRect(sx + 26, sy + 8,  4,   30, rd.r*0.55f, rd.g*0.55f, rd.b*0.55f);
        // Olhos
        drawRect(sx + 14, sy + 20, 10, 10, 0.9f, 1.f, 1.f);
        drawRect(sx + 48, sy + 20, 10, 10, 0.9f, 1.f, 1.f);
        drawRect(sx + 17, sy + 23, 5,   5, 0.1f, 0.3f, 0.5f);
        drawRect(sx + 51, sy + 23, 5,   5, 0.1f, 0.3f, 0.5f);

        // Nome
        drawText(cx + 12, cy + ch - 22, 2.4f * bright, rd.name,
                 rd.r * 1.1f, rd.g * 1.1f, rd.b * 1.1f);
        // Tipo
        char typeBuf[32];
        std::snprintf(typeBuf, sizeof(typeBuf), "Tipo: %s", rd.type);
        drawText(cx + 12, cy + ch - 46, 1.7f, typeBuf, 0.8f, 0.8f, 0.75f);
        // Descricao
        drawText(cx + 12, cy + ch - 70, 1.5f, rd.desc, 0.65f, 0.65f, 0.6f);

        // HP
        char hpBuf[32];
        std::snprintf(hpBuf, sizeof(hpBuf), "HP: %.0f", rd.maxHp);
        drawText(cx + 12, cy + ch - 95, 1.7f, hpBuf, 0.3f, 0.9f, 0.4f);

        // Habilidades
        drawText(cx + 12, cy + 105, 1.6f, "Habilidades:", 0.9f, 0.75f, 0.7f);

        // Move 1
        float m1r = 1.f, m1g = 0.6f, m1b = 0.2f;
        if (rd.move1 == AttackType::Bubbles || rd.move1 == AttackType::Waterfall)
            { m1r = 0.3f; m1g = 0.75f; m1b = 1.f; }
        else if (rd.move1 == AttackType::RazorLeaf || rd.move1 == AttackType::GigaDrain)
            { m1r = 0.2f; m1g = 0.9f; m1b = 0.25f; }
        char m1[48]; std::snprintf(m1, sizeof(m1), "1: %s", rd.move1Name);
        drawText(cx + 16, cy + 82, 1.7f, m1, m1r, m1g, m1b);
        char d1[32]; std::snprintf(d1, sizeof(d1), "   dano: %d", (int)rd.dmg1);
        drawText(cx + 16, cy + 62, 1.5f, d1, 0.7f, 0.7f, 0.7f);

        // Move 2
        float m2r = 1.f, m2g = 0.6f, m2b = 0.2f;
        if (rd.move2 == AttackType::Bubbles || rd.move2 == AttackType::Waterfall)
            { m2r = 0.3f; m2g = 0.75f; m2b = 1.f; }
        else if (rd.move2 == AttackType::RazorLeaf || rd.move2 == AttackType::GigaDrain)
            { m2r = 0.2f; m2g = 0.9f; m2b = 0.25f; }
        char m2[48]; std::snprintf(m2, sizeof(m2), "2: %s", rd.move2Name);
        drawText(cx + 16, cy + 40, 1.7f, m2, m2r, m2g, m2b);
        char d2[32];
        if (rd.heal2 > 0.f && rd.move2 == AttackType::GigaDrain)
            std::snprintf(d2, sizeof(d2), "   dano: %d | cura: %d", (int)rd.dmg2, (int)rd.heal2);
        else
            std::snprintf(d2, sizeof(d2), "   dano: %d", (int)rd.dmg2);
        drawText(cx + 16, cy + 20, 1.5f, d2, 0.7f, 0.7f, 0.7f);

        // Seta indicadora embaixo do selecionado
        if (sel) {
            float arrowX = cx + cw * 0.5f - 12;
            drawText(arrowX, cy - 14, 2.5f, "^", rd.r, rd.g, rd.b);
        }
    }

    // Instrucoes na base
    drawRoundedPanel(0, 0, W, 50, 0.12f, 0.10f, 0.08f, 0);
    drawText(W*0.5f - 300, 36, 2.f,
             "<< Setas para navegar  |  ENTER para confirmar >>",
             0.65f, 0.62f, 0.55f);
    drawText(W - 220, 14, 1.5f, "F11 - Tela Cheia", 0.45f, 0.42f, 0.40f);
}

// ---------------------------------------------------------------------------
// Render — Tela de selecao de dificuldade
// ---------------------------------------------------------------------------
static void renderDifficultySelect() {
    const float W = 1280, H = 720;

    // Fundo pedregoso escuro
    drawRect(0, 0, W, H, 0.08f, 0.07f, 0.06f);
    // Faixa superior
    drawRect(0, H - 80, W, 80, 0.12f, 0.10f, 0.08f);
    // Linha decorativa
    drawRect(0, H - 84, W, 4, 0.6f, 0.45f, 0.2f);

    // Titulo pulsante
    float pulse = 0.85f + 0.15f * std::sin(difficultySelect.titlePulse * 2.5f);
    drawText(W*0.5f - 320, H - 22, 5.0f * pulse, "Rockworld Gray Stone",
             0.95f, 0.80f, 0.30f);

    // Subtitulo
    drawText(W*0.5f - 220, H - 60, 2.2f, "Escolha a dificuldade:",
             0.85f, 0.82f, 0.75f);

    // Cards das 3 dificuldades
    for (int i = 0; i < 3; ++i) {
        const DifficultyDef& dd = DIFFICULTY_DEFS[i];
        float cx = 180.f + i * 300.f;
        float cy = 140.f;
        float cw = 240.f, ch = 360.f;
        bool  sel = (difficultySelect.cursor == i);

        // Particulas de preview
        difficultySelect.previews[i].draw();

        // Borda extra se selecionado
        if (sel) {
            float t = std::sin(difficultySelect.titlePulse * 4.f) * 0.5f + 0.5f;
            drawRect(cx - 8, cy - 8, cw + 16, ch + 16,
                     dd.r * 0.8f + t * 0.2f,
                     dd.g * 0.8f + t * 0.2f,
                     dd.b * 0.8f + t * 0.2f, 0.7f);
        }

        // Card fundo
        float bright = sel ? 1.0f : 0.65f;
        drawRoundedPanel(cx, cy, cw, ch,
                         dd.r * 0.22f * bright,
                         dd.g * 0.22f * bright,
                         dd.b * 0.22f * bright, 4);

        // Indicador visual: circulo colorido no topo do card
        float sx = cx + cw*0.5f - 32;
        float sy = cy + 200;
        drawRect(sx, sy, 64, 64, dd.r * 0.5f, dd.g * 0.5f, dd.b * 0.5f);
        drawRect(sx + 6, sy + 6, 52, 52, dd.r, dd.g, dd.b);

        // Nome
        drawText(cx + 12, cy + ch - 22, 2.6f * bright, dd.name,
                 dd.r * 1.1f, dd.g * 1.1f, dd.b * 1.1f);
        // Descricao
        drawText(cx + 12, cy + ch - 52, 1.6f, dd.desc, 0.8f, 0.8f, 0.75f);

        // Velocidade da barra de timing
        char spdBuf[48];
        std::snprintf(spdBuf, sizeof(spdBuf), "Velocidade: %.1fx", dd.speed);
        drawText(cx + 12, cy + 130, 1.8f, spdBuf, 0.85f, 0.82f, 0.4f);

        // Seta indicadora embaixo do selecionado
        if (sel) {
            float arrowX = cx + cw * 0.5f - 12;
            drawText(arrowX, cy - 14, 2.5f, "^", dd.r, dd.g, dd.b);
        }
    }

    // Instrucoes na base
    drawRoundedPanel(0, 0, W, 50, 0.12f, 0.10f, 0.08f, 0);
    drawText(W*0.5f - 300, 36, 2.f,
             "<< Setas para navegar  |  ENTER para confirmar >>",
             0.65f, 0.62f, 0.55f);
    drawText(W - 220, 14, 1.5f, "F11 - Tela Cheia", 0.45f, 0.42f, 0.40f);
}


// ---------------------------------------------------------------------------
static void drawTimingBar() {
    const TimingBarState& tb = battle.timingBar;
    if (!tb.active) return;

    const float W = 1280.f;
    const float barW = 560.f, barH = 32.f;
    const float barX = (W - barW) * 0.5f;
    const float barY = 390.f;   // centro vertical da arena

    // --- Sombra/borda externa ---
    drawRect(barX - 6, barY - 6, barW + 12, barH + 12, 0.05f, 0.05f, 0.05f, 0.92f);
    drawRect(barX - 3, barY - 3, barW + 6,  barH + 6,  0.22f, 0.22f, 0.28f, 1.f);

    // --- Zonas de cor: Vermelho base ---
    drawRect(barX, barY, barW, barH, 0.82f, 0.12f, 0.08f);

    // --- Amarelo: centro mediano ---
    float yellowW = barW * 0.50f;
    float yellowInset = (barW - yellowW) * 0.5f;
    drawRect(barX + yellowInset, barY, yellowW, barH, 0.88f, 0.78f, 0.06f);

    // --- Verde: centro pequeno ---
    float greenW = barW * 0.11f;
    float greenX = barX + (barW - greenW) * 0.5f;
    drawRect(greenX, barY, greenW, barH, 0.1f, 0.88f, 0.18f);

    // --- Linhas divisorias das zonas ---
    glColor4f(0.f, 0.f, 0.f, 0.45f);
    glLineWidth(1.5f);
    auto vline = [&](float xv) {
        glBegin(GL_LINES);
        glVertex2f(xv, barY);
        glVertex2f(xv, barY + barH);
        glEnd();
    };
    vline(barX + yellowInset);
    vline(barX + yellowInset + yellowW);
    vline(greenX);
    vline(greenX + greenW);

    // --- Label da zona ao travar ---
    if (tb.locked) {
        float zr, zg, zb;
        tb.getZoneColor(tb.pos, zr, zg, zb);
        const char* label = tb.zoneLabel(tb.pos);
        float lx = barX + tb.pos * barW - (float)strlen(label) * 5.5f;
        drawText(lx, barY + barH + 42.f, 2.8f, label, zr, zg, zb);
    }

    // --- Cursor (barra vertical movel) ---
    float cursorX = barX + tb.pos * barW;
    float cr, cg, cb;
    if (tb.locked)
        tb.getZoneColor(tb.pos, cr, cg, cb);
    else
        { cr = 1.f; cg = 1.f; cb = 1.f; }

    // Brilho externo do cursor
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(cr, cg, cb, 0.28f);
    drawRect(cursorX - 6, barY - 4, 12, barH + 8, cr, cg, cb, 0.25f);
    // Cursor solido
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRect(cursorX - 3, barY - 5, 6, barH + 10, cr, cg, cb);

    // --- Texto instrucao ---
    if (!tb.locked) {
        drawText(barX + barW * 0.5f - 155.f, barY + barH + 24.f, 2.f,
                 "PRESSIONE para travar!", 0.95f, 0.92f, 0.75f);
    }

    // Indicadores de zona (icones de texto)
    drawText(barX + 6,                 barY + barH * 0.5f + 4, 1.5f, "FRACO", 1.f, 0.5f, 0.4f);
    drawText(barX + yellowInset + 6,   barY + barH * 0.5f + 4, 1.5f, "BOM",   1.f, 0.92f, 0.3f);
    drawText(greenX + 4,               barY + barH * 0.5f + 4, 1.5f, "TOP",   0.3f, 1.f, 0.4f);
}

// ---------------------------------------------------------------------------
// Render — Fase 1: campo de batalha
// ---------------------------------------------------------------------------
static void renderBattleScene() {
    const int W = 1280, H = 720;

    // Tela de Game Over / Vitoria
    if (battle.isOver()) {
        bool won = (battle.state == BattleState::PlayerWon);
        drawRect(0, 0, W, H, won ? 0.05f : 0.12f, won ? 0.08f : 0.04f, won ? 0.05f : 0.04f);

        float panelH = 220.f, panelY = (H - panelH) * 0.5f;
        drawRoundedPanel(W*0.18f, panelY, W*0.64f, panelH,
                         won ? 0.12f : 0.18f, won ? 0.18f : 0.08f, won ? 0.08f : 0.08f, 8);

        battle.particles.draw();
        battle.enemyParticles.draw();

        const char* loserName = won ? battle.enemy.name.c_str()
                                    : battle.player.name.c_str();
        char msgBuf[64];
        if (won) {
            drawText(W*0.5f - 110, panelY + panelH*0.78f, 4.5f, "VITORIA!",
                     1.f, 0.92f, 0.2f);
            std::snprintf(msgBuf, sizeof(msgBuf), "%s foi derrotado!", loserName);
        } else {
            drawText(W*0.5f - 145, panelY + panelH*0.78f, 4.5f, "DERROTA!",
                     0.95f, 0.22f, 0.18f);
            std::snprintf(msgBuf, sizeof(msgBuf), "%s foi derrotado...", loserName);
        }
        drawText(W*0.5f - (float)strlen(msgBuf)*5.5f,
                 panelY + panelH*0.48f, 2.2f, msgBuf, 0.9f, 0.9f, 0.7f);

        float blink = std::sin(battle.gameOverTimer * 4.f);
        if (blink > 0) {
            drawText(W*0.5f - 260, panelY + 24.f, 2.f,
                     "R - Reiniciar  |  ESC - Selecao de pedra", 0.72f, 0.72f, 0.72f);
        }
        return;
    }

    // Cenario
    drawRect(0, H*0.45f, W, H*0.55f, 0.45f, 0.55f, 0.65f); // ceu rochoso
    drawRect(0, 0, W, H*0.42f, 0.28f, 0.25f, 0.22f);        // chao de pedra
    drawRect(0, H*0.38f, W, H*0.1f, 0.35f, 0.32f, 0.28f);   // faixa arena

    // Plataformas (escalam com o tamanho dos personagens)
    float platW = 380.f + g_battleStage * 80.f;
    drawRoundedPanel(780 - g_battleStage * 40, 420 - g_battleStage * 50, platW, 50, 0.42f, 0.38f, 0.32f, 5);
    drawRoundedPanel(80, 95 - g_battleStage * 30, platW, 50, 0.42f, 0.38f, 0.32f, 5);

    // Flash dano inimigo
    if (battle.flashAlpha > 0) {
        float ex = battle.enemy.x - battle.enemy.spriteW * 0.5f;
        drawRect(ex, battle.enemy.y, battle.enemy.spriteW, battle.enemy.spriteH,
                 1, 1, 1, battle.flashAlpha * 0.5f);
    }
    // Flash dano jogador
    if (battle.playerFlash > 0) {
        float px = battle.player.x - battle.player.spriteW * 0.5f;
        drawRect(px, battle.player.y, battle.player.spriteW, battle.player.spriteH,
                 0.3f, 0.6f, 1.f, battle.playerFlash * 0.55f);
    }

    float eShake = battle.enemyShake > 0 ? std::sin(battle.enemyShake * 40.f) * 6.f : 0.f;
    float pShake = battle.playerShake > 0 ? std::sin(battle.playerShake * 40.f) * 6.f : 0.f;
    drawFighter(battle.enemy, eShake);
    drawFighter(battle.player, pShake);

    battle.particles.draw();
    battle.enemyParticles.draw();
    // enemyFireRay / enemyFireCircle spawn into enemyParticles - no draw() method
    battle.enemyRazorLeaf.draw();
    battle.bubblesAtk.draw();
    battle.playerBubbles.draw();
    battle.razorLeaf.draw();

    // Timing bar (overlay por cima da arena, abaixo do HUD)
    drawTimingBar();

    // HUD inimigo
    drawRoundedPanel(620, 580, 340, 95, 0.88f, 0.85f, 0.80f, 3);
    drawText(635, 650, 2.2f, battle.enemy.name.c_str(), 0.12f, 0.10f, 0.10f);
    drawHPBar(635, 610, 210, 18, battle.enemy.hp / battle.enemy.maxHp, true);
    char hpE[32];
    std::snprintf(hpE, sizeof(hpE), "HP %d/%d", (int)battle.enemy.hp, (int)battle.enemy.maxHp);
    drawText(855, 612, 1.6f, hpE, 0.12f, 0.12f, 0.12f);

    // Tipo inimigo
    char typeEnemyStr[32];
    std::snprintf(typeEnemyStr, sizeof(typeEnemyStr), "Tipo: %s", battle.enemy.name.c_str()[0] ? getEnemyForStage(g_chosenRock, g_battleStage).type : "Pedra");
    drawText(635, 600, 1.6f, typeEnemyStr, 0.5f, 0.45f, 0.4f);

    // HUD jogador — sobe junto com o personagem (ancorando acima do sprite)
    float pHudY = battle.player.y + battle.player.spriteH + 18.f;
    drawRoundedPanel(80, pHudY, 340, 95, 0.88f, 0.85f, 0.80f, 3);
    drawText(95, pHudY + 70, 2.2f, battle.player.name.c_str(), 0.12f, 0.10f, 0.10f);
    drawHPBar(95, pHudY + 30, 210, 18, battle.player.hp / battle.player.maxHp, false);
    char hpP[32];
    std::snprintf(hpP, sizeof(hpP), "HP %d/%d", (int)battle.player.hp, (int)battle.player.maxHp);
    drawText(315, pHudY + 32, 1.6f, hpP, 0.12f, 0.12f, 0.12f);
    // Tipo jogador
    char typeStr[32];
    std::snprintf(typeStr, sizeof(typeStr), "Tipo: %s", ROCK_DEFS[g_chosenRock].type);
    drawText(95, pHudY + 20, 1.6f, typeStr, 0.5f, 0.45f, 0.4f);

    // Caixa de movimentos
    drawRoundedPanel(0, 0, W, 130, 0.14f, 0.13f, 0.18f, 0);
    drawRect(0, 128, W, 2, 0.3f, 0.28f, 0.4f);

    // Status do turno
    const char* prompt;
    bool enemyTurn = (battle.state == BattleState::EnemyAttacking);
    if (battle.state == BattleState::TimingBar) {
        static char tbp[64];
        const char* movName = (battle.timingBar.pendingAttack == battle.player.moveType1)
                              ? battle.player.moveName1.c_str()
                              : battle.player.moveName2.c_str();
        std::snprintf(tbp, sizeof(tbp), "%s — trave no momento certo!", movName);
        prompt = tbp;
    } else if (enemyTurn) {
        static char ebuf[128];
        const char* mvName = "ataque";
        if (battle.currentAttack != AttackType::None) {
            if (battle.currentAttack == battle.enemy.moveType1) mvName = battle.enemy.moveName1.c_str();
            else if (battle.currentAttack == battle.enemy.moveType2) mvName = battle.enemy.moveName2.c_str();
        } else {
            if (battle.bubblesAtk.active) mvName = "Bolhas";
            else if (battle.waterfall.active) mvName = "Cachoeira";
            else if (battle.fireRay.active) mvName = "Raio de Fogo";
            else if (battle.fireCircle.active) mvName = "Circulo de Fogo";
            else if (battle.razorLeaf.active) mvName = "Lascas Cortantes";
            else if (battle.gigaDrain.active) mvName = "Giga Dreno";
        }
        std::snprintf(ebuf, sizeof(ebuf), "%s usa %s!", battle.enemy.name.c_str(), mvName);
        prompt = ebuf;
    } else if (battle.isAnimating()) {
        static char pa[64];
        std::snprintf(pa, sizeof(pa), "%s ataca!", battle.player.name.c_str());
        prompt = pa;
    } else {
        prompt = "Escolha um ataque (1/2 ou setas):";
    }
    drawText(30, 90, 2.f, prompt);

    bool canChoose = !battle.isAnimating() && !battle.isOver();

    // Botao move 1
    bool sel0 = battle.selectedMove == 0 && canChoose;
    // Cor do botao por tipo do ataque
    const RockDef& rd = ROCK_DEFS[g_chosenRock];
    bool m1Water = (rd.move1 == AttackType::Bubbles || rd.move1 == AttackType::Waterfall);
    bool m1Grass = (rd.move1 == AttackType::RazorLeaf || rd.move1 == AttackType::GigaDrain);
    drawRoundedPanel(30, 14, 370, 56,
        sel0 ? (m1Grass ? 0.1f : m1Water ? 0.2f : 0.95f) : (m1Grass ? 0.08f : m1Water ? 0.15f : 0.75f),
        sel0 ? (m1Grass ? 0.85f : m1Water ? 0.65f : 0.55f) : (m1Grass ? 0.60f : m1Water ? 0.45f : 0.4f),
        sel0 ? (m1Grass ? 0.1f : m1Water ? 0.95f : 0.2f) : (m1Grass ? 0.08f : m1Water ? 0.75f : 0.15f), 2);
    char btn1[48]; std::snprintf(btn1, sizeof(btn1), "1 - %s", rd.move1Name);
    drawText(50, 54, 2.2f, btn1);

    // Botao move 2
    bool sel1 = battle.selectedMove == 1 && canChoose;
    bool m2Water = (rd.move2 == AttackType::Bubbles || rd.move2 == AttackType::Waterfall);
    bool m2Grass = (rd.move2 == AttackType::RazorLeaf || rd.move2 == AttackType::GigaDrain);
    drawRoundedPanel(440, 14, 400, 56,
        sel1 ? (m2Grass ? 0.1f : m2Water ? 0.2f : 0.95f) : (m2Grass ? 0.08f : m2Water ? 0.15f : 0.75f),
        sel1 ? (m2Grass ? 0.85f : m2Water ? 0.65f : 0.55f) : (m2Grass ? 0.60f : m2Water ? 0.45f : 0.4f),
        sel1 ? (m2Grass ? 0.1f : m2Water ? 0.95f : 0.2f) : (m2Grass ? 0.08f : m2Water ? 0.75f : 0.15f), 2);
    char btn2[48]; std::snprintf(btn2, sizeof(btn2), "2 - %s", rd.move2Name);
    drawText(460, 54, 2.2f, btn2);

    // Indicador de fase com nome de evolução
    static const char* stageTierNames[3] = { "Filhote", "Guardiao", "Titan" };
    char phaseLbl[48];
    std::snprintf(phaseLbl, sizeof(phaseLbl), "Fase %d / 3  [%s]",
                  g_battleStage + 1, stageTierNames[g_battleStage]);
    drawText(1000, 660, 1.8f, phaseLbl);
    drawText(1000, 14, 1.5f, "F11 - Tela Cheia", 0.45f, 0.42f, 0.40f);
}

// ---------------------------------------------------------------------------
// Render principal
// ---------------------------------------------------------------------------
static void render() {
    glClearColor(0.08f, 0.07f, 0.06f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 0, 720, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    switch (currentPhase) {
        case GamePhase::CharSelect:
            renderCharSelect();
            break;
        case GamePhase::DifficultySelect:
            renderDifficultySelect();
            break;
        case GamePhase::BattleScene1:
            renderBattleScene();
            break;
        case GamePhase::BattleScene2:
        case GamePhase::BattleScene3:
        case GamePhase::BattleScene4:
            drawRect(0, 0, 1280, 720, 0.08f, 0.07f, 0.06f);
            drawText(400, 380, 3.f, "Fase em breve...", 0.6f, 0.55f, 0.45f);
            break;
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static void processInput(GLFWwindow* window, float dt) {
    (void)dt;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        static bool escPrev = false;
        bool escNow = true;
        if (!escPrev) {
            // ESC sempre volta para menu de selecao
            currentPhase = GamePhase::CharSelect;
        }
        escPrev = escNow;
    }

    // ---- Tela de selecao de personagem ----
    if (currentPhase == GamePhase::CharSelect) {
        static bool leftPrev = false, rightPrev = false, confirmPrev = false;
        bool leftK  = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
        bool rightK = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
        bool confirmK = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_SPACE)  == GLFW_PRESS;

        if (leftK  && !leftPrev)
            charSelect.cursor = (charSelect.cursor + 2) % 3;
        if (rightK && !rightPrev)
            charSelect.cursor = (charSelect.cursor + 1) % 3;

        if (confirmK && !confirmPrev) {
            g_chosenRock      = charSelect.cursor;
            currentPhase      = GamePhase::DifficultySelect;
            difficultySelect.cursor = 0;
            g_consumeConfirm  = true;
        }
        leftPrev = leftK; rightPrev = rightK; confirmPrev = confirmK;
        return;
    }

    // ---- Tela de selecao de dificuldade ----
    if (currentPhase == GamePhase::DifficultySelect) {
        static bool leftPrev = false, rightPrev = false, confirmPrev = false;
        bool leftK  = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
        bool rightK = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
        bool confirmK = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_SPACE)  == GLFW_PRESS;

        if (g_consumeConfirm) {
            if (!confirmK) g_consumeConfirm = false;
        }

        if (leftK  && !leftPrev)
            difficultySelect.cursor = (difficultySelect.cursor + 2) % 3;
        if (rightK && !rightPrev)
            difficultySelect.cursor = (difficultySelect.cursor + 1) % 3;

        if (confirmK && !confirmPrev && !g_consumeConfirm) {
            g_chosenDifficulty = difficultySelect.cursor;
            currentPhase  = GamePhase::BattleScene1;
            g_battleStage = 0;
            battle.reset(getEnemyForStage(g_chosenRock, g_battleStage));
            battle.timingBar.speed = DIFFICULTY_DEFS[g_chosenDifficulty].speed;
            g_consumeConfirm = true;
        }
        leftPrev = leftK; rightPrev = rightK; confirmPrev = confirmK;
        return;
    }

    // ---- Batalha ----
    if (currentPhase != GamePhase::BattleScene1) return;

    // ESC ja tratado acima via static, aqui tratamos R e movimentos
    static bool escPrevBattle = false;
    bool escNow = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escNow && !escPrevBattle) {
        currentPhase = GamePhase::CharSelect;
    }
    escPrevBattle = escNow;

    // R — reiniciar
    static bool rPrev = false;
    bool rKey = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rKey && !rPrev) {
        if (battle.isOver()) battle.reset(getEnemyForStage(g_chosenRock, g_battleStage));
    }
    rPrev = rKey;

    // ---- Timing Bar: trava o cursor ao pressionar qualquer tecla de ataque ----
    if (battle.state == BattleState::TimingBar && !battle.timingBar.locked) {
        static bool tbPrev = false;
        bool tbKey = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_SPACE)  == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_1)      == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_2)      == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_UP)     == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_DOWN)   == GLFW_PRESS;
        if (battle.timingBar.ignoreInput) {
            if (!tbKey) battle.timingBar.ignoreInput = false;
        } else if (tbKey && !tbPrev) {
            battle.timingBar.lock();
        }
        tbPrev = tbKey;
        return;
    }

    if (battle.isAnimating() || battle.isOver()) return;

    bool up    = glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS;
    bool down  = glfwGetKey(window, GLFW_KEY_DOWN)   == GLFW_PRESS;
    bool confirmB = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_SPACE)  == GLFW_PRESS;

    static bool upPrev=false, downPrev=false, confirmBPrev=false;
    // Se acabamos de trocar de tela por um ENTER/SPACE, consumimos esse
    // pressionamento até o usuário soltar a tecla para evitar ações imediatas.
    if (g_consumeConfirm) {
        if (!confirmB) g_consumeConfirm = false;
    }

    if (up   && !upPrev)      battle.selectedMove = 0;
    if (down && !downPrev)    battle.selectedMove = 1;
    if (confirmB && !confirmBPrev && !g_consumeConfirm) {
        const RockDef& rd = ROCK_DEFS[g_chosenRock];
        battle.startTimingBar(battle.selectedMove == 0 ? rd.move1 : rd.move2);
    }
    upPrev=up; downPrev=down; confirmBPrev=confirmB;

    static bool k1Prev=false, k2Prev=false;
    bool k1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool k2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (k1 && !k1Prev) { battle.selectedMove=0; battle.startTimingBar(ROCK_DEFS[g_chosenRock].move1); }
    if (k2 && !k2Prev) { battle.selectedMove=1; battle.startTimingBar(ROCK_DEFS[g_chosenRock].move2); }
    k1Prev=k1; k2Prev=k2;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Falha ao iniciar GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
        "Rockworld Gray Stone", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Falha ao criar janela\n");
        glfwTerminate();
        return 1;
    }

    // Estado de tela cheia
    bool g_fullscreen = false;
    int g_winX = 100, g_winY = 100, g_winW = 1280, g_winH = 720;
    static bool f11Prev = false;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float  dt  = std::min(float(now - lastTime), 0.05f);
        lastTime   = now;

        processInput(window, dt);

        // F11 — alternar tela cheia
        bool f11Now = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Now && !f11Prev) {
            g_fullscreen = !g_fullscreen;
            if (g_fullscreen) {
                glfwGetWindowPos(window, &g_winX, &g_winY);
                glfwGetWindowSize(window, &g_winW, &g_winH);
                GLFWmonitor* mon = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(mon);
                glfwSetWindowMonitor(window, mon, 0, 0,
                                     mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, nullptr, g_winX, g_winY,
                                     g_winW, g_winH, 0);
            }
        }
        f11Prev = f11Now;

        // Viewport sempre 1280x720 com letterbox
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float scale = std::min(float(fbW) / 1280.f, float(fbH) / 720.f);
        int vpW = int(1280.f * scale), vpH = int(720.f * scale);
        int vpX = (fbW - vpW) / 2, vpY = (fbH - vpH) / 2;
        glViewport(vpX, vpY, vpW, vpH);

        if (currentPhase == GamePhase::CharSelect) {
            charSelect.update(dt);
        } else if (currentPhase == GamePhase::DifficultySelect) {
            difficultySelect.update(dt);
        } else if (currentPhase == GamePhase::BattleScene1) {
            battle.update(dt);
        }

        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
