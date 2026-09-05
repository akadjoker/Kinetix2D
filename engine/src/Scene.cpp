#include "k2d/Scene.h"

#include "k2d/CameraComponent.h"
#include "k2d/Collider2D.h"
#include "k2d/Input.h"
#include "k2d/Joint2D.h"
#include "k2d/Profiler.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Steering2D.h"
#include "k2d/UiControls.h"

namespace k2d
{

    Scene::Scene()
        : mRoot("root"), mNextId(1), mObjectCount(0), mTopologyVersion(0), mComponentListsDirty(false),
          mHasDisposed(false), mRenderCamera(nullptr), mRenderViewportWidth(0.0f), mRenderViewportHeight(0.0f),
          mGravity(0.0f, 980.0f), mUseTree(true), mClock(nullptr), mStepStamp(0), mNextBodyId(1), mFrameStamp(0),
          // Not 0: equal stamps mean "already synced this frame", so matching
          // the initial frame stamp would skip the very first sync and leave
          // every query looking at an empty tree.
          mBroadphaseStamp(0xFFFFFFFFu),
          mVelocityIterations(8), mFixedStep(1.0f / 60.0f), mAccumulator(0.0f), mCollisionCallback(nullptr),
          mCollisionCallbackUser(nullptr), mAnimationEventCallback(nullptr), mAnimationEventCallbackUser(nullptr),
          mActionEventCallback(nullptr), mActionEventCallbackUser(nullptr),
          mSimulationEnabled(false), mHasDirtyBodies(false), mParticles(*this)
    {
        mRoot.mScene = this;
        mNarrowMs = 0.0f;
    }

    Scene::~Scene()
    {
        // Bodies must go before the objects their user data points at.
        clearBodies();
        mRoot.deleteChildrenRaw();
        mRoot.mScene = nullptr;
    }

    void Scene::setAnimationEventCallback(AnimationEventCallback callback, void *user)
    {
        mAnimationEventCallback = callback;
        mAnimationEventCallbackUser = user;
    }

    void Scene::dispatchAnimationEvent(GameObject *object, const char *clip, const char *event, bool finished)
    {
        if (mAnimationEventCallback)
            mAnimationEventCallback(object, clip, event, finished, mAnimationEventCallbackUser);
    }

    void Scene::setActionEventCallback(ActionEventCallback callback, void *user)
    {
        mActionEventCallback = callback;
        mActionEventCallbackUser = user;
    }

    void Scene::dispatchActionEvent(GameObject *object, const char *event)
    {
        if (mActionEventCallback)
            mActionEventCallback(object, event, mActionEventCallbackUser);
    }

    GameObject &Scene::root()
    {
        return mRoot;
    }

    const GameObject &Scene::root() const
    {
        return mRoot;
    }

    GameObject *Scene::createObject(const char *name, GameObject *parent)
    {
        GameObject *object = new GameObject(name);
        GameObject *destination = parent ? parent : &mRoot;
        if (!destination->addChildRaw(object))
        {
            delete object;
            return nullptr;
        }
        registerBranch(object);
        return object;
    }

    bool Scene::add(GameObject *object, GameObject *parent)
    {
        if (!object || object == &mRoot || object->parent())
            return false;
        GameObject *destination = parent ? parent : &mRoot;
        if (!destination->addChildRaw(object))
            return false;
        registerBranch(object);
        return true;
    }

    bool Scene::destroy(GameObject *object)
    {
        if (!object || object == &mRoot || object->scene() != this)
            return false;
        object->dispose();
        return true;
    }

    bool Scene::reparent(GameObject *object, GameObject *newParent)
    {
        if (!object || object == &mRoot || object->scene() != this)
            return false;
        GameObject *destination = newParent ? newParent : &mRoot;
        if (destination->scene() != this || destination == object || destination == object->parent())
            return false;

        GameObject *oldParent = object->parent();
        if (!oldParent || !oldParent->removeChildRaw(object))
            return false;
        if (!destination->addChildRaw(object))
        {
            oldParent->addChildRaw(object);
            return false;
        }
        markTopologyChanged();
        return true;
    }

    GameObject *Scene::find(const char *name) const
    {
        return findRecursive(&mRoot, name);
    }

    std::size_t Scene::objectCount() const
    {
        return mObjectCount;
    }

    CameraComponent *Scene::activeCamera()
    {
        CameraComponent *camera = nullptr;
        for (std::size_t i = 0; i < mCameras.size(); ++i)
        {
            CameraComponent *candidate = mCameras[i];
            GameObject *object = candidate ? candidate->owner() : nullptr;
            if (!candidate || !object || !candidate->active() || !object->isActiveInHierarchy())
                continue;
            if (!camera || candidate->renderPriority() > camera->renderPriority() ||
                (candidate->renderPriority() == camera->renderPriority() &&
                 object->id() < camera->owner()->id()))
                camera = candidate;
        }
        return camera;
    }

    const CameraComponent *Scene::activeCamera() const
    {
        return const_cast<Scene *>(this)->activeCamera();
    }

    void Scene::makeCameraActive(CameraComponent &camera)
    {
        int highest = camera.renderPriority();
        for (std::size_t i = 0; i < mCameras.size(); ++i)
        {
            CameraComponent *other = mCameras[i];
            if (other && other != &camera && other->renderPriority() > highest)
                highest = other->renderPriority();
        }
        camera.setRenderPriority(highest + 1);
    }

    void Scene::update(float deltaTime)
    {
        ProfileScope profileScope("scene.update");
        ++mFrameStamp;
        // Capture the counts: components added from an update begin on the next
        // frame. Removed components become null entries until compaction, so a
        // callback can never invalidate the list being iterated.
        const std::size_t count = mAllComponents.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            Component *component = mAllComponents[i];
            GameObject *object = component ? component->owner() : nullptr;
            if (object && !object->disposed() && object->isActiveInHierarchy())
                object->updateComponent(component, deltaTime);
        }

        const std::size_t lateCount = mLateUpdateComponents.size();
        for (std::size_t i = 0; i < lateCount; ++i)
        {
            Component *component = mLateUpdateComponents[i];
            GameObject *object = component ? component->owner() : nullptr;
            if (object && !object->disposed() && object->isActiveInHierarchy())
                object->lateUpdateComponent(component, deltaTime);
        }
        flushDisposed();
        compactComponentLists();
        updateUi();
        stepBodies(deltaTime);
    }

    void Scene::setRenderCamera(const Camera2D *camera, float viewportWidth, float viewportHeight)
    {
        mRenderCamera = camera;
        mRenderViewportWidth = viewportWidth;
        mRenderViewportHeight = viewportHeight;
    }

    void Scene::render(CanvasRenderer &canvas)
    {
        buildRenderQueue().Flush(canvas);
    }

    RenderQueue &Scene::buildRenderQueue()
    {
        ProfileScope profileScope("scene.render_queue");
        mRenderQueue.Clear();
        const std::size_t count = mRenderComponents.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            Component *component = mRenderComponents[i];
            GameObject *object = component ? component->owner() : nullptr;
            if (object && !object->disposed() && object->isActiveAndVisibleInHierarchy())
                object->renderComponent(component, mRenderQueue);
        }
        mParticles.submit(mRenderQueue);
        compactComponentLists();
        return mRenderQueue;
    }

    void Scene::clear()
    {
        clearBodies();
        mRoot.deleteChildrenRaw();
        mAllComponents.clear();
        mLateUpdateComponents.clear();
        mRenderComponents.clear();
        mCameras.clear();
        mUiControls.clear();
        mSteerings.clear();
        mNeighbourBodies.clear();
        mNeighbours.clear();
        mObjectCount = 0;
        mNextId = 1;
        mComponentListsDirty = false;
        mParticles.clear();
    }

    void Scene::registerBranch(GameObject *object)
    {
        markTopologyChanged();
        if (object->disposed())
            mHasDisposed = true;
        object->mScene = this;
        object->mId = mNextId++;
        ++mObjectCount;
        for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count); ++i)
            for (Component *component = object->mComponents[i]; component; component = component->mNextSibling)
                registerComponent(component);
        for (std::size_t i = 0; i < object->childCount(); ++i)
            registerBranch(object->child(i));
    }

    void Scene::unregisterBranch(GameObject *object)
    {
        markTopologyChanged();
        for (std::size_t i = 0; i < object->childCount(); ++i)
            unregisterBranch(object->child(i));
        for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count); ++i)
            for (Component *component = object->mComponents[i]; component; component = component->mNextSibling)
                unregisterComponent(component);
        object->mScene = nullptr;
        --mObjectCount;
    }

    void Scene::registerComponent(Component *component)
    {
        if (!component)
            return;
        markTopologyChanged();
        component->mSceneAllIndex = mAllComponents.size();
        mAllComponents.push_back(component);
        if ((component->mEvents & ComponentEventLateUpdate) != 0)
        {
            component->mSceneLateIndex = mLateUpdateComponents.size();
            mLateUpdateComponents.push_back(component);
        }
        if ((component->mEvents & ComponentEventRender) != 0)
        {
            component->mSceneRenderIndex = mRenderComponents.size();
            mRenderComponents.push_back(component);
        }
        if (component->mType == ComponentType::Camera)
            mCameras.push_back(static_cast<CameraComponent *>(component));
        if (UiControl *control = component->uiControl())
            mUiControls.push_back(control);
        if (component->mType == ComponentType::RigidBody && simulationEnabled())
            attachBody(*static_cast<RigidBody2D *>(component));
        if (component->mType == ComponentType::Collider && simulationEnabled())
        {
            GameObject *object = component->owner();
            RigidBody2D *rigidBody = object ? object->getComponent<RigidBody2D>() : nullptr;
            if (rigidBody && rigidBody->inWorld())
                markBodyDirty(*rigidBody);
        }
        if (component->mType == ComponentType::Joint)
            mJoints.push_back(static_cast<Joint2D *>(component));
        if (component->mType == ComponentType::Steering)
            mSteerings.push_back(static_cast<Steering2D *>(component));
    }

    void Scene::unregisterComponent(Component *component)
    {
        markTopologyChanged();
        mComponentListsDirty = true;
        const auto removeFrom = [component](ct::Vector<Component *> &components, std::size_t &index)
        {
            if (index < components.size() && components[index] == component)
                components[index] = nullptr;
            index = Component::InvalidSceneListIndex;
        };
        removeFrom(mAllComponents, component->mSceneAllIndex);
        removeFrom(mLateUpdateComponents, component->mSceneLateIndex);
        removeFrom(mRenderComponents, component->mSceneRenderIndex);
        if (component->mType == ComponentType::Camera)
            for (std::size_t i = 0; i < mCameras.size(); ++i)
                if (mCameras[i] == component)
                {
                    mCameras[i] = nullptr;
                    break;
                }
        if (component->uiControl())
            for (std::size_t i = 0; i < mUiControls.size(); ++i)
                if (mUiControls[i] == component->uiControl())
                {
                    mUiControls[i] = nullptr;
                    break;
                }
        if (component->mType == ComponentType::Collider)
        {
            GameObject *object = component->owner();
            RigidBody2D *rigidBody = object ? object->getComponent<RigidBody2D>() : nullptr;
            if (rigidBody && rigidBody->inWorld())
                markBodyDirty(*rigidBody);
        }
        if (component->mType == ComponentType::Steering)
            for (std::size_t i = 0; i < mSteerings.size(); ++i)
                if (mSteerings[i] == component)
                {
                    mSteerings[i] = nullptr;
                    break;
                }
        if (component->mType == ComponentType::RigidBody)
            detachBody(*static_cast<RigidBody2D *>(component));
        if (component->mType == ComponentType::Joint)
        {
            Joint2D *joint = static_cast<Joint2D *>(component);
            for (std::size_t i = 0; i < mJoints.size(); ++i)
            {
                if (mJoints[i] == joint)
                {
                    mJoints[i] = mJoints.back();
                    mJoints.pop_back();
                    break;
                }
            }
            for (std::size_t i = 0; i < mJoints.size(); ++i)
                if (mJoints[i]->dependsOnJoint(joint))
                    mJoints[i]->invalidate();
        }
    }

    void Scene::compactComponentLists()
    {
        if (!mComponentListsDirty)
            return;

        const auto compact = [](ct::Vector<Component *> &components, int list)
        {
            std::size_t write = 0;
            for (std::size_t read = 0; read < components.size(); ++read)
                if (components[read])
                {
                    Component *component = components[read];
                    components[write] = component;
                    if (list == 0)
                        component->mSceneAllIndex = write;
                    else if (list == 1)
                        component->mSceneLateIndex = write;
                    else
                        component->mSceneRenderIndex = write;
                    ++write;
                }
            components.resize(write);
        };
        compact(mAllComponents, 0);
        compact(mLateUpdateComponents, 1);
        compact(mRenderComponents, 2);
        std::size_t cameraWrite = 0;
        for (std::size_t cameraRead = 0; cameraRead < mCameras.size(); ++cameraRead)
            if (mCameras[cameraRead])
                mCameras[cameraWrite++] = mCameras[cameraRead];
        mCameras.resize(cameraWrite);
        std::size_t uiWrite = 0;
        for (std::size_t uiRead = 0; uiRead < mUiControls.size(); ++uiRead)
            if (mUiControls[uiRead])
                mUiControls[uiWrite++] = mUiControls[uiRead];
        mUiControls.resize(uiWrite);
        std::size_t steeringWrite = 0;
        for (std::size_t steeringRead = 0; steeringRead < mSteerings.size(); ++steeringRead)
            if (mSteerings[steeringRead])
                mSteerings[steeringWrite++] = mSteerings[steeringRead];
        mSteerings.resize(steeringWrite);
        mComponentListsDirty = false;
    }

    void Scene::updateUi()
    {
        const UiViewport &viewport = GetUiViewport();
        if (!viewport.valid)
            return;

        // UiControls owns the input pointer; this call has no global input
        // access by design, so controls cannot each independently consume it.
        // The viewport gate also keeps editor chrome out of game controls.
        extern Input *GetUiInputInternal();
        Input *input = GetUiInputInternal();
        float x = 0.0f;
        float y = 0.0f;
        bool canHit = false;
        if (input)
        {
            const float outputX = input->MouseX() - viewport.x;
            const float outputY = input->MouseY() - viewport.y;
            canHit = outputX >= 0.0f && outputY >= 0.0f && outputX < viewport.outputWidth &&
                     outputY < viewport.outputHeight;
            if (canHit)
            {
                x = outputX * viewport.width / viewport.outputWidth;
                y = outputY * viewport.height / viewport.outputHeight;
            }
        }

        UiControl *hit = nullptr;
        for (std::size_t i = 0; i < mUiControls.size(); ++i)
        {
            UiControl *control = mUiControls[i];
            GameObject *object = control ? control->owner() : nullptr;
            if (!object || object->disposed() || !object->isActiveAndVisibleInHierarchy() || !control->active())
                continue;
            control->updateLayout();
            control->resetInput();
            if (!control->interactive() || !canHit || !control->contains(x, y))
                continue;
            if (!hit || object->zIndex() >= hit->owner()->zIndex())
                hit = control;
        }
        if (hit)
            hit->handleInput(x, y, input->MouseDown(0), input->MousePressed(0), input->MouseReleased(0));
    }

    void Scene::flushDisposed()
    {
        if (!mHasDisposed)
            return;
        mHasDisposed = false;
        ct::Vector<GameObject *> roots;
        collectDisposed(&mRoot, roots);
        for (GameObject *object : roots)
        {
            unregisterBranch(object);
            if (object->parent())
                object->parent()->removeChildRaw(object);
            delete object;
        }
    }

    void Scene::collectDisposed(GameObject *object, ct::Vector<GameObject *> &out)
    {
        for (std::size_t i = 0; i < object->childCount(); ++i)
        {
            GameObject *child = object->child(i);
            if (child->disposed())
                out.push_back(child);
            else
                collectDisposed(child, out);
        }
    }

    GameObject *Scene::findRecursive(const GameObject *from, const char *name) const
    {
        for (std::size_t i = 0; i < from->childCount(); ++i)
        {
            GameObject *child = from->child(i);
            if (child->name() == name)
                return child;
            if (GameObject *found = findRecursive(child, name))
                return found;
        }
        return nullptr;
    }

}
