#pragma once

#include "k2d/SceneTransition.h"

#include <ct/string.hpp>

namespace k2d
{
    class Assets;
    class GameObject;
    class Scene;

    // Runtime scene loader. Requests are deferred so gameplay code can ask for
    // a transition during update and apply it safely at the end of a frame.
    class SceneManager
    {
    public:
        GameObject *Load(Scene &scene, Assets &assets, const char *path);
        // With a transition the frame that asked keeps the old scene on screen
        // and is captured; the new scene comes in at the next frame.
        void Request(const char *path, SceneTransitionKind transition = SceneTransitionKind::None,
                     float seconds = 0.5f);
        bool HasRequest() const { return !mRequestedPath.empty(); }
        bool WantsCapture() const { return HasRequest() && mRequestedTransition != SceneTransitionKind::None; }
        GameObject *ApplyRequest(Scene &scene, Assets &assets);
        void ClearRequest() { mRequestedPath.clear(); }
        const char *CurrentPath() const { return mCurrentPath.c_str(); }

        GameObject *load(Scene &scene, Assets &assets, const char *path) { return Load(scene, assets, path); }
        void request(const char *path) { Request(path); }
        bool hasRequest() const { return HasRequest(); }
        GameObject *applyRequest(Scene &scene, Assets &assets) { return ApplyRequest(scene, assets); }
        const char *currentPath() const { return CurrentPath(); }

    private:
        ct::String mCurrentPath;
        ct::String mRequestedPath;
        SceneTransitionKind mRequestedTransition = SceneTransitionKind::None;
        float mRequestedSeconds = 0.5f;
    };

    SceneManager &GetSceneManager();
}
