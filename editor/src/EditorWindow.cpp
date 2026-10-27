#include "EditorWindow.h"

#include <k2d/FileSystem.h>
#include <k2d/OpenGL.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/SceneManager.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
const char* const kHierarchyPanel = "Hierarchy";
const char* const kScenePanel = "Scene";
const char* const kInspectorPanel = "Inspector";
const char* const kConsolePanel = "Console";

ct::String directoryOf(const char* path)
{
    char buffer[4096];
    path_directory(path, buffer, sizeof buffer);
    return ct::String(buffer);
}

ct::String joinPath(const ct::String& base, const char* relative)
{
    char buffer[4096];
    return path_join(buffer, sizeof buffer, base.c_str(), relative) ? ct::String(buffer) : ct::String(relative);
}
} // namespace

void EditorWindow::build(rt::WidgetApp& app, PlatformWindow* window)
{
    mWindow = window;
    rt::Widget* root = app.addStage("main");
    rt::BoxLayout* outer = root->createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    outer->setStretch(1.0f);
    outer->setPadding(0.0f);
    outer->setSpacing(0.0f);

    rt::MenuBar* menuBar = outer->createChild<rt::MenuBar>();
    rt::Menu* file = menuBar->addMenu("File");
    rt::MenuAction* quit = file->addAction("Quit");
    quit->setShortcut("Ctrl+Q");
    quit->triggered.connect([this]() { window_set_should_close(mWindow, true); });
    rt::Menu* view = menuBar->addMenu("View");
    rt::MenuAction* resetLayout = view->addAction("Reset Layout");
    resetLayout->triggered.connect([this]() { this->resetLayout(); });

    mDock = outer->createChild<rt::DockPanel>();
    mDock->setStretch(1.0f);
    mDock->addPanel<rt::Label>(kHierarchyPanel, "No scene");
    mViewport = mDock->addPanel<ViewportPanel>(kScenePanel);
    mDock->addPanel<rt::Label>(kInspectorPanel, "Nothing selected");
    mDock->addPanel<rt::Label>(kConsolePanel, "");
    mDock->splitOff(kHierarchyPanel, rt::DockSide::Left, 0.2f);
    mDock->splitOff(kInspectorPanel, rt::DockSide::Right, 0.25f);
    mDock->splitOff(kConsolePanel, rt::DockSide::Bottom, 0.25f);
    mDefaultLayout = mDock->saveLayout();

    mViewport->panned.connect([this](float dx, float dy)
    {
        mCamera.position.x -= dx / mCamera.zoom.x;
        mCamera.position.y -= dy / mCamera.zoom.y;
    });
    mViewport->zoomed.connect([this](float steps, float, float)
    {
        const float factor = steps > 0.0f ? 1.1f : 1.0f / 1.1f;
        mCamera.zoom.x *= factor;
        mCamera.zoom.y *= factor;
    });

    mStatus = outer->createChild<rt::StatusBar>();
    mStatus->setSpacing(16.0f);
    mSceneLabel = mStatus->createChild<rt::Label>("No scene");
    mFpsLabel = mStatus->createChild<rt::Label>("0 fps");
}

void EditorWindow::resetLayout()
{
    if (mDock)
        mDock->restoreLayout(mDefaultLayout);
}

bool EditorWindow::openScene(const char* path)
{
    static bool registered = false;
    if (!registered)
    {
        RegisterZenScriptSerializer();
        RegisterPhysics2DSerializers();
        registered = true;
    }
    const ct::String sceneDirectory = directoryOf(path);
    const ct::String projectRoot = directoryOf(sceneDirectory.c_str());
    FileSystem& files = FileSystem::Instance();
    files.AddSearchPath(projectRoot.c_str());
    files.AddSearchPath(joinPath(projectRoot, "assets").c_str());
    files.AddSearchPath(sceneDirectory.c_str());

    mScene.clear();
    mScene.setSimulationEnabled(false);
    if (!GetSceneManager().Load(mScene, mAssets, path))
        return false;
    mSceneLabel->setText(path_filename(path));
    return true;
}

void EditorWindow::frame(float deltaTime, float pixelScale)
{
    mFpsTime += deltaTime;
    ++mFpsFrames;
    if (mFpsTime >= 1.0f)
    {
        char text[32];
        std::snprintf(text, sizeof text, "%.0f fps", mFpsFrames / mFpsTime);
        mFpsLabel->setText(text);
        mFpsTime = 0.0f;
        mFpsFrames = 0;
    }
    mStatus->layout();

    if (!mViewport || !mViewport->isVisible())
        return;
    const rt::Rect area = mViewport->absoluteRect();
    renderScene(area.w, area.h, pixelScale);
}

void EditorWindow::renderScene(float width, float height, float pixelScale)
{
    if (!mCanvasReady && !mCanvasFailed)
    {
        mCanvasReady = mCanvas.Init();
        mCanvasFailed = !mCanvasReady;
    }
    const int pixelWidth = static_cast<int>(width * pixelScale + 0.5f);
    const int pixelHeight = static_cast<int>(height * pixelScale + 0.5f);
    if (!mCanvasReady || !mTarget.resize(pixelWidth, pixelHeight))
        return;

    mTarget.bind();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mCanvas.SetProjection(mCamera.Projection(width, height));
    mScene.setRenderCamera(&mCamera, width, height);
    mScene.render(mCanvas);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rt::TextureHandle texture;
    texture.value = mTarget.texture();
    mViewport->setTexture(texture);
}

void EditorWindow::shutdown()
{
    mTarget.destroy();
    if (mCanvasReady)
        mCanvas.Shutdown();
    mCanvasReady = false;
}

} // namespace k2d::editor
