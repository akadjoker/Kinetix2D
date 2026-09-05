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
class SceneViewportPanel;

class EditorApplication
{
  public:
    // A ticket, not a copy: the scene JSON it refers to lives in
    // mFrameSnapshot until a commit actually needs it. beginChange() runs
    // dozens of times a frame, and all but one of those never commit.
    struct SceneChange
    {
        uint64_t snapshotFrame = 0;
        uint64_t selection = 0;
        bool hadSelection = false;
    };

    enum class FileDialogPurpose
    {
        None,
        NewProjectFolder,
        OpenProject,
        OpenScene,
        SaveScene,
        NewPrefab,
        OpenPrefab,
        SaveSelectionAsPrefab
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

    struct SceneCursor
    {
        bool enabled = false;
        ct::String image;
        Math::Vec2 offset = Math::Vec2(0.0f, 0.0f);
    };

    SceneCursor& sceneCursor()
    {
        return mSceneCursor;
    }
    void applyCursorSettings();

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

    bool prefabModeActive() const
    {
        return mPrefabModeActive;
    }
    const ct::String& prefabModePath() const
    {
        return mPrefabModePath;
    }
    bool enterPrefabMode(const char* path);
    void exitPrefabMode(bool save);
    void requestSaveObjectAsPrefab(uint64_t objectId, const ct::String& suggestedName);

    void restartEditPreview();

    void preloadTextures(const ct::Json& node);
    Texture* particlePlaceholderTexture();
    Texture* loadOrGetTexture(const ct::String& imagePath);
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
    void focusOnObject(GameObject& object);
    void openFileDialog(FileDialogPurpose purpose, ImGuiFileDialog::Mode mode, const ct::String& startDirectory,
                        const ct::String& initialName = ct::String());

  private:
    void createPanels();
    void drawWorkspace();
    void drawMenuBar();
    void drawToolbar();
    void drawFileDialog();
    void drawNewProjectNameDialog();
    void drawPrefabModeBar();
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
    void applyRuntimeSceneRequest();
    void runStandalone();
    void exportWeb(bool runAfterExport);
    void tickEditPreview(GameObject& object, float deltaTime);
    void restartEditPreview(GameObject& object);
    void startPrefabAnimationPreview(GameObject& object);

    Device mDevice;
    Assets mAssets;
    Scene mScene;
    ct::Json mPlaySnapshot;
    uint64_t mPlaySelectionId = 0;
    bool mPlayHadSelection = false;
    EditorSelection mSelection;
    EditorSettings mSettings;
    UserData mUserData;
    Project mProject;
    EditorToasts mToasts;
    CommandStack mCommands;
    ct::Vector<ct::Unique<EditorPanel>> mPanels;
    ScriptEditorPanel* mScriptEditor = nullptr;
    ImageEditorPanel* mImageEditor = nullptr;
    SceneViewportPanel* mSceneViewport = nullptr;
    ct::String mConsoleText;
    ct::String mCurrentScenePath;
    ct::String mPreviewPrefabPath;
    bool mPrefabModeActive = false;
    ct::String mPrefabModePath;
    uint64_t mPrefabModeRootId = 0;
    ct::Json mPrefabModeSavedScene;
    ct::String mPrefabModeSavedPath;
    uint64_t mPrefabModeSavedSelectionId = 0;
    bool mPrefabModeHadSelection = false;
    uint64_t mPendingPrefabExportId = 0;
    bool mInitialized = false;
    bool mPlaying = false;
    ScenePhysics mScenePhysics;
    SceneCursor mSceneCursor;
    bool mPaused = false;
    bool mScriptErrorPaused = false;
    ct::String mScriptOutputBuffer;
    ct::String mScriptErrorBuffer;
    int mThemeKind = 0;
    bool mDefaultLayoutPending = true;
    bool mLayoutResetRequested = false;
    bool mDefaultFocusPending = false;
    bool mProfilerOpen = false;
    bool mCaptureScreenshotPending = false;
    bool mTransactionActive = false;
    ct::String mTransactionLabel;
    SceneChange mTransactionBefore;
    // A transaction outlives the frame that opened it, so unlike a ticket it
    // has to own its "before" scene.
    ct::Json mTransactionBeforeScene;
    // beginChange() runs once per editable widget -- 63 times a frame in the
    // Inspector alone -- but the scene cannot change between them: nothing is
    // edited until a widget reports activation. One snapshot per frame serves
    // them all, and frames where nothing is edited never build one.
    ct::Json mFrameSnapshot;
    uint64_t mFrameSnapshotFrame = 0;
    bool mFrameSnapshotValid = false;
    uint64_t mFrameCounter = 0;

    ImGuiFileDialog mFileDialog;
    FileDialogPurpose mFileDialogPurpose = FileDialogPurpose::None;
    ct::String mNewProjectParentDirectory;
    char mNewProjectName[128] = {};
    float mStatsSmoothedDelta = 0.0f;
};

} // namespace k2d::editor
