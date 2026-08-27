#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/ZenRuntime.h>
#include <k2d/ZenScriptComponent.h>

#include "k2d_test_paths.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
bool nearEqual(float a, float b, float tolerance = 0.001f)
{
    return std::fabs(a - b) < tolerance;
}

bool writeScript(const std::string &path)
{
    FILE *file = std::fopen(path.c_str(), "w");
    if (!file)
        return false;
    std::fputs("class BytecodeSpinner:\n"
               "    def __init__(self, node):\n"
               "        self.node = node\n"
               "    def on_start(self):\n"
               "        self.node.set_z_index(7)\n"
               "    def on_update(self, dt):\n"
               "        self.node.rotate(45 * dt)\n",
               file);
    return std::fclose(file) == 0;
}

bool testBytecodeRunsWithoutSource()
{
    const std::string source = k2d_tests::tempPath("k2d_zen_bytecode_spinner.py");
    const std::string bytecode = k2d_tests::tempPath("k2d_zen_bytecode_spinner.zbc");
    std::remove(bytecode.c_str());

    bool ok = writeScript(source);
    ct::String error;
    if (ok)
        ok = k2d::ZenRuntime::instance().compileFileToBytecode(source.c_str(), bytecode.c_str(), true, &error);
    std::remove(source.c_str());

    k2d::ZenRuntime::instance().reset();
    const std::size_t compilesBefore = k2d::ZenRuntime::instance().compileCount();
    k2d::Scene scene;
    k2d::GameObject *object = scene.createObject("bytecode_spinner");
    k2d::ZenScriptComponent *script = object->addComponent<k2d::ZenScriptComponent>();
    ok = ok && script->loadFile(bytecode.c_str());
    scene.update(1.0f);

    ok = ok && script->loaded();
    ok = ok && object->zIndex() == 7;
    ok = ok && nearEqual(object->rotationDegrees(), 45.0f);
    ok = ok && k2d::ZenRuntime::instance().compileCount() == compilesBefore;
    std::printf("  bytecode: loaded_without_source=%s z=%d rot=%.1f%s%s\n",
                ok ? "yes" : "no", object->zIndex(), object->rotationDegrees(),
                error.empty() ? "" : " error=", error.empty() ? "" : error.c_str());

    std::remove(bytecode.c_str());
    return ok;
}

bool testBundleRegistersScriptsWithoutSourceFiles()
{
    const std::string bytecode = k2d_tests::tempPath("k2d_zen_bytecode_bundle.zbc");
    std::remove(bytecode.c_str());

    const char* source =
        "class BundleSpinner:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_update(self, dt):\n"
        "        self.node.rotate(30 * dt)\n"
        "\n"
        "class BundleMarker:\n"
        "    def __init__(self, node):\n"
        "        self.node = node\n"
        "    def on_start(self):\n"
        "        self.node.set_z_index(9)\n";

    ct::String error;
    bool ok = k2d::ZenRuntime::instance().compileSourceToBytecode(source, "bundle", bytecode.c_str(), true, &error);
    k2d::ZenRuntime::instance().reset();
    ok = ok && k2d::ZenRuntime::instance().loadBytecodeBundle(bytecode.c_str(), &error);
    ok = ok && k2d::ZenRuntime::instance().registerBytecodeScript("scripts/spinner.py", "BundleSpinner", &error);
    ok = ok && k2d::ZenRuntime::instance().registerBytecodeScript("scripts/marker.py", "BundleMarker", &error);

    k2d::Scene scene;
    k2d::GameObject* spinner = scene.createObject("spinner");
    k2d::GameObject* marker = scene.createObject("marker");
    ok = ok && spinner->addComponent<k2d::ZenScriptComponent>()->loadFile("scripts/spinner.py");
    ok = ok && marker->addComponent<k2d::ZenScriptComponent>()->loadFile("scripts/marker.py");
    scene.update(1.0f);
    ok = ok && nearEqual(spinner->rotationDegrees(), 30.0f);
    ok = ok && marker->zIndex() == 9;

    std::printf("  bundle: registered_without_source=%s rot=%.1f z=%d%s%s\n",
                ok ? "yes" : "no", spinner->rotationDegrees(), marker->zIndex(),
                error.empty() ? "" : " error=", error.empty() ? "" : error.c_str());
    std::remove(bytecode.c_str());
    return ok;
}
} // namespace

int main()
{
    k2d::SetZenScriptsEnabled(true);
    const bool bytecode = testBytecodeRunsWithoutSource();
    const bool bundle = testBundleRegistersScriptsWithoutSourceFiles();
    std::printf("zen bytecode: bytecode=%s bundle=%s\n", bytecode ? "pass" : "fail",
                bundle ? "pass" : "fail");
    return bytecode && bundle ? 0 : 1;
}
