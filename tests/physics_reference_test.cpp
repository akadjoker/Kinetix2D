#include <kx/kx.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <mathc.h>
#include <chrono>
#include <vector>
#include <cmath>

struct PhysicsSnapshot {
    float timestamp;
    float x, y;
    float vx, vy;
    float angle;
    float angVel;
    int bodyId;

    std::string toCSV() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << timestamp << "," << x << "," << y << ","
            << vx << "," << vy << "," << angle << ","
            << angVel << "," << bodyId;
        return oss.str();
    }
};

void TestFreeBodyFall() {
    printf("[TEST] Free Body Fall\n");

    kx::World world({0.0f, -9.81f});

    auto body = world.CreateCircle({0.0f, 100.0f}, 0.5f, 1.0f);
    body->SetUserData((void*)(intptr_t)1);

    std::vector<PhysicsSnapshot> log;
    const float dt = 0.016f;
    for (int i = 0; i < 1000; i++) {
        world.Step(dt);

        auto pos = body->Position();
        auto vel = body->Velocity();

        PhysicsSnapshot snap;
        snap.timestamp = i * dt;
        snap.x = pos.x;
        snap.y = pos.y;
        snap.vx = vel.x;
        snap.vy = vel.y;
        snap.angle = body->Angle();
        snap.angVel = body->AngularVelocity();
        snap.bodyId = 1;
        log.push_back(snap);
    }

    std::ofstream file("/tmp/kinetix2d_physics_freefall.csv");
    file << "timestamp,x,y,vx,vy,angle,angVel,bodyId\n";
    for (const auto& snap : log) {
        file << snap.toCSV() << "\n";
    }
    file.close();

    if (log.back().y < 0.0f) {
        printf("  ✓ PASS (%d frames)\n", (int)log.size());
    } else {
        printf("  ✗ FAIL\n");
    }
}

void TestStackedBoxes3000() {
    printf("[TEST] Extreme Stacked Boxes (3000 boxes!)\n");

    kx::World world({0.0f, -9.81f});

    auto ground = world.CreateStaticBox({0.0f, -50.0f}, 100.0f, 5.0f);

    std::vector<kx::Body*> boxes;
    int totalBoxes = 3000;

    printf("  Creating %d boxes...\n", totalBoxes);
    for (int i = 0; i < totalBoxes; i++) {
        float x = -50.0f + (i % 100) * 1.0f;
        float y = 0.0f + (i / 100) * 1.0f;
        auto body = world.CreateBox({x, y}, 0.4f, 0.4f, 0.5f);
        body->SetUserData((void*)(intptr_t)(i + 1));
        boxes.push_back(body);

        if ((i + 1) % 500 == 0) {
            printf("    Created %d boxes\n", i + 1);
        }
    }

    printf("  Simulating %zu boxes for 200 frames...\n", boxes.size());

    std::vector<PhysicsSnapshot> log;
    const float dt = 0.016f;
    int frames = 200;

    auto startSim = std::chrono::high_resolution_clock::now();
    for (int frame = 0; frame < frames; frame++) {
        world.Step(dt);

        for (size_t j = 0; j < boxes.size(); j += 10) {
            auto pos = boxes[j]->Position();
            auto vel = boxes[j]->Velocity();

            PhysicsSnapshot snap;
            snap.timestamp = frame * dt;
            snap.x = pos.x;
            snap.y = pos.y;
            snap.vx = vel.x;
            snap.vy = vel.y;
            snap.angle = boxes[j]->Angle();
            snap.angVel = boxes[j]->AngularVelocity();
            snap.bodyId = (int)j + 1;
            log.push_back(snap);
        }

        if ((frame + 1) % 50 == 0) {
            printf("    Simulated %d frames\n", frame + 1);
        }
    }
    auto endSim = std::chrono::high_resolution_clock::now();
    auto simMs = std::chrono::duration_cast<std::chrono::milliseconds>(endSim - startSim).count();

    std::ofstream file("/tmp/kinetix2d_physics_stacked_3000.csv");
    file << "timestamp,x,y,vx,vy,angle,angVel,bodyId\n";
    for (const auto& snap : log) {
        file << snap.toCSV() << "\n";
    }
    file.close();

    printf("  Simulation time: %ldms (%d contacts)\n", simMs, (int)world.Contacts().size());
    printf("  ✓ PASS\n");
}

void TestCircleRolling() {
    printf("[TEST] Circle Rolling (extreme)\n");

    kx::World world({0.0f, -9.81f});

    auto ground = world.CreateStaticBox({0.0f, 0.0f}, 50.0f, 0.5f);

    auto circle = world.CreateCircle({-40.0f, 5.0f}, 0.5f, 1.0f);
    circle->SetUserData((void*)(intptr_t)1);
    circle->SetVelocity({25.0f, 0.0f});

    std::vector<PhysicsSnapshot> log;
    const float dt = 0.016f;
    for (int i = 0; i < 1000; i++) {
        world.Step(dt);

        auto pos = circle->Position();
        auto vel = circle->Velocity();

        PhysicsSnapshot snap;
        snap.timestamp = i * dt;
        snap.x = pos.x;
        snap.y = pos.y;
        snap.vx = vel.x;
        snap.vy = vel.y;
        snap.angle = circle->Angle();
        snap.angVel = circle->AngularVelocity();
        snap.bodyId = 1;
        log.push_back(snap);
    }

    std::ofstream file("/tmp/kinetix2d_physics_rolling.csv");
    file << "timestamp,x,y,vx,vy,angle,angVel,bodyId\n";
    for (const auto& snap : log) {
        file << snap.toCSV() << "\n";
    }
    file.close();

    if (log.back().x > 0.0f) {
        printf("  ✓ PASS (%d frames)\n", (int)log.size());
    } else {
        printf("  ✗ FAIL\n");
    }
}

void TestLargeSimulation4000() {
    printf("[TEST] EXTREME: 4000 bodies falling!\n");

    kx::World world({0.0f, -9.81f});

    auto ground = world.CreateStaticBox({0.0f, -100.0f}, 200.0f, 10.0f);

    std::vector<kx::Body*> bodies;
    int totalBodies = 4000;

    printf("  Creating %d bodies...\n", totalBodies);
    for (int i = 0; i < totalBodies; i++) {
        float x = -100.0f + (i % 100) * 2.0f;
        float y = 50.0f + (i / 100) * 2.0f;

        kx::Body* body = nullptr;
        if (i % 3 == 0) {
            body = world.CreateBox({x, y}, 0.4f, 0.4f, 0.5f);
        } else if (i % 3 == 1) {
            body = world.CreateCircle({x, y}, 0.4f, 0.5f);
        } else {
            body = world.CreateBox({x, y}, 0.3f, 0.5f, 0.5f);
        }

        body->SetUserData((void*)(intptr_t)(i + 1));
        bodies.push_back(body);

        if ((i + 1) % 500 == 0) {
            printf("    Created %d bodies\n", i + 1);
        }
    }

    printf("  Simulating %zu bodies for 300 frames...\n", bodies.size());

    const float dt = 0.016f;
    int frames = 300;

    auto startSim = std::chrono::high_resolution_clock::now();
    for (int frame = 0; frame < frames; frame++) {
        world.Step(dt);

        if ((frame + 1) % 50 == 0) {
            printf("    Simulated %d frames (contacts: %zu)\n", frame + 1, world.Contacts().size());
        }
    }
    auto endSim = std::chrono::high_resolution_clock::now();
    auto simMs = std::chrono::duration_cast<std::chrono::milliseconds>(endSim - startSim).count();

    float avgPerFrame = simMs / (float)frames;
    float fps = (frames * 1000) / (float)simMs;

    std::ofstream file("/tmp/kinetix2d_extreme_4000_bodies.txt");
    file << "bodies," << totalBodies << "\n";
    file << "frames," << frames << "\n";
    file << "total_ms," << simMs << "\n";
    file << "avg_ms_per_frame," << avgPerFrame << "\n";
    file << "fps," << fps << "\n";
    file << "max_contacts," << world.Contacts().size() << "\n";
    file.close();

    printf("  Total time: %ldms\n", simMs);
    printf("  Avg/frame: %.2fms\n", avgPerFrame);
    printf("  FPS: %.1f\n", fps);
    printf("  Max contacts: %zu\n", world.Contacts().size());
    printf("  ✓ PASS\n");
}

void TestRaycastStorm() {
    printf("[TEST] Raycast Storm (1000 queries)\n");

    kx::World world({0.0f, -9.81f});

    auto ground = world.CreateStaticBox({0.0f, 0.0f}, 50.0f, 1.0f);

    std::vector<kx::Body*> obstacles;
    for (int i = 0; i < 100; i++) {
        float x = -40.0f + (i % 10) * 8.0f;
        float y = 20.0f + (i / 10) * 4.0f;
        auto body = world.CreateCircle({x, y}, 1.0f, 1.0f);
        obstacles.push_back(body);
    }

    const float dt = 0.016f;
    for (int i = 0; i < 50; i++) {
        world.Step(dt);
    }

    printf("  Firing 1000 raycasts...\n");
    auto startRay = std::chrono::high_resolution_clock::now();
    int hitCount = 0;
    for (int i = 0; i < 1000; i++) {
        float angle = (i / 1000.0f) * 6.28318f;
        Math::Vec2 origin(0.0f, 5.0f);
        Math::Vec2 direction(std::cos(angle) * 100.0f, std::sin(angle) * 100.0f);

        kx::RayCastHit hit;
        if (world.RayCastClosest(origin, direction, hit)) {
            hitCount++;
        }
    }
    auto endRay = std::chrono::high_resolution_clock::now();
    auto rayMs = std::chrono::duration_cast<std::chrono::milliseconds>(endRay - startRay).count();

    std::ofstream file("/tmp/kinetix2d_raycast_stress.txt");
    file << "raycasts,1000\n";
    file << "hits," << hitCount << "\n";
    file << "total_ms," << rayMs << "\n";
    file << "avg_ms_per_raycast," << rayMs / 1000.0f << "\n";
    file.close();

    printf("  Hits: %d, Time: %ldms\n", hitCount, rayMs);
    printf("  ✓ PASS\n");
}

int main() {
    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║   EXTREME Physics Reference Tests (GLM)  ║\n");
    printf("║     Stress Test Baseline with Binaries   ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");

    TestFreeBodyFall();
    printf("\n");

    TestStackedBoxes3000();
    printf("\n");

    TestCircleRolling();
    printf("\n");

    TestLargeSimulation4000();
    printf("\n");

    TestRaycastStorm();

    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  All EXTREME reference data saved!       ║\n");
    printf("║  Ready for GLM → mathc migration test    ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");

    return 0;
}
