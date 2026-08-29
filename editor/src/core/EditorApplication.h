#pragma once

#include "EditorPanel.h"
#include "EditorSelection.h"
#include "EditorSettings.h"
#include "EditorToasts.h"
#include "CommandStack.h"
#include "Project.h"

#include <k2d/Assets.h>
#include <k2d/Device.h>
#include <k2d/Scene.h>
#include <k2d/UserData.h>

#include <ct/ptr.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <ImGuiFileDialog.h>

namespace k2d::editor
{

class ScriptEditorPanel;
class ImageEditorPanel;

class EditorApplication
{
  public:
    struct SceneChange
    {
        ct::Json scene;
        uint64_t selection = 0;
        bool hadSelection = false;
    };

    enum class FileDialogPurpose
    {
        None,
        NewProjectFolder,
        OpenProject,
        OpenScene,
        SaveScene
    };

    EditorApplication() = default;
    ~EditorApplication();

    bool initialize();
    int run();
    void shutdown();

    Scene& scene()
    {
        return mScene;
    }
    Scene& runtimeScene()
    {
        return mRuntimeScene;
    }
    Assets& assets()
    {
        return mAssets;
    }
    EditorSelection& selection()
    {
        return mSelection;
    }
    EditorSettings& settings()
    {
        return mSettings;
    }
    Project& project()
    {
        return mProject;
    }
    EditorToasts& toasts()
    {
        return mToasts;
    }
    const ct::String& consoleText() const
    {
        return mConsoleText;
    }
    void clearConsole()
    {
        mConsoleText.clear();
    }
    void log(const char* message);
    void log(const ct::String& message);
    bool playing() const
    {
        return mPlaying;
    }
    void applyPhysicsSettings();
    void reloadChangedScripts();

    struct ScenePhysics
    {
        bool overrideGravity = false;
        Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    };

    ScenePhysics& scenePhysics()
    {
        return mScenePhysics;
    }
    Math::Vec2 effectiveGravity() const;
    bool paused() const
    {
        return mPaused;
    }
    SceneChange beginChange();
    void commitChange(const char* label, const SceneChange& before);
    void beginTransaction(const char* label, const SceneChange& before);
    void commitTransaction();
    void undo();
    void redo();
    bool canUndo() const
    {
        return mCommands.canUndo();
    }
    bool canRedo() const
    {
        return mCommands.canRedo();
    }
    const char* undoName() const
    {
        return mCommands.undoName();
    }
    const char* redoName() const
    {
        return mCommands.redoName();
    }

    const ct::String& previewPrefabPath() const
    {
        return mPreviewPrefabPath;
    }
    void previewPrefab(const char* path)
    {
        mPreviewPrefabPath = path ? path : "";
    }

    void restartEditPreview();

    void preloadTextures(const ct::Json& node);
    Texture* particlePlaceholderTexture();
    GameObject* createSpriteNodeFromImage(const ct::String& imagePath, GameObject* parent = nullptr,
                                          const Math::Vec2* worldPosition = nullptr);

    const ct::String& currentScenePath() const
    {
        return mCurrentScenePath;
    }
    bool openScene(const char* path);
    bool saveScene(const char* path);
    bool newProject(const char* rootDirectory, const char* name);
    bool openProject(const char* projectFile);
    void openScriptEditor(const char* path);
    void openImageEditor(const char* path);
    void openFileDialog(FileDialogPurpose purpose, ImGuiFileDialog::Mode mode, const ct::String& startDirectory,
                        const ct::String& initialName = ct::String());

  private:
    void createPanels();
    void drawWorkspace();
    void drawMenuBar();
    void drawToolbar();
    void drawFileDialog();
    void drawNewProjectNameDialog();
    void drawStatusBar();
    void createDefaultDockLayout(unsigned int dockspaceId);
    void newScene();
    void createShapesExampleScene();
    void createPhysicsExampleScene();
    void createBunnymarkExampleScene();
    void requestScreenshot();
    void toggleGifCapture();
    ct::Json snapshotScene();
    void restoreScene(const ct::Json& snapshot, uint64_t selectedId, bool hadSelection);
    void handleShortcuts();
    void loadSettings();
    void saveSettings();
    void startPlay();
    void stopPlay();
    void stepPlay();
    void runStandalone();
    void exportWeb(bool runAfterExport);
    void tickEditPreview(GameObject& object, float deltaTime);
    void restartEditPreview(GameObject& object);

    Device mDevice;
    Assets mAssets;
    Scene mScene;
    Scene mRuntimeScene;
    EditorSelection mSelection;
    EditorSettings mSettings;
    UserData mUserData;
    Project mProject;
    EditorToasts mToasts;
    CommandStack mCommands;
    ct::Vector<ct::Unique<EditorPanel>> mPanels;
    ScriptEditorPanel* mScriptEditor = nullptr;
    ImageEditorPanel* mImageEditor = nullptr;
    ct::String mConsoleText;
    ct::String mCurrentScenePath;
    ct::String mPreviewPrefabPath;
    bool mInitialized = false;
    bool mPlaying = false;
    ScenePhysics mScenePhysics;
    bool mPaused = false;
    int mThemeKind = 0;
    bool mDefaultLayoutPending = true;
    bool mLayoutResetRequested = false;
    bool mDefaultFocusPending = false;
    bool mProfilerOpen = false;
    bool mCaptureScreenshotPending = false;
    bool mTransactionActive = false;
    ct::String mTransactionLabel;
    SceneChange mTransactionBefore;

    ImGuiFileDialog mFileDialog;
    FileDialogPurpose mFileDialogPurpose = FileDialogPurpose::None;
    ct::String mNewProjectParentDirectory;
    char mNewProjectName[128] = {};
    float mStatsSmoothedDelta = 0.0f;
};

} // namespace k2d::editor
