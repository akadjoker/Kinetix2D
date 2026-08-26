#include "k2d/Scene.h"

#include "k2d/CameraComponent.h"
#include "k2d/Input.h"
#include "k2d/Profiler.h"
#include "k2d/UiControls.h"

namespace k2d
{

    Scene::Scene() : mRoot("root"), mNextId(1), mObjectCount(0), mComponentListsDirty(false)
    {
        mRoot.mScene = this;
    }

    Scene::~Scene()
    {
        mRoot.deleteChildrenRaw();
        mRoot.mScene = nullptr;
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

    void Scene::update(float deltaTime)
    {
        ProfileScope profileScope("scene.update");
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
        compactComponentLists();
        return mRenderQueue;
    }

    void Scene::clear()
    {
        mRoot.deleteChildrenRaw();
        mAllComponents.clear();
        mLateUpdateComponents.clear();
        mRenderComponents.clear();
        mCameras.clear();
        mUiControls.clear();
        mObjectCount = 0;
        mNextId = 1;
        mComponentListsDirty = false;
    }

    void Scene::registerBranch(GameObject *object)
    {
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
    }

    void Scene::unregisterComponent(Component *component)
    {
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
        mComponentListsDirty = false;
    }

    void Scene::updateUi()
    {
        const UiViewport &viewport = GetUiViewport();
        if (!viewport.valid)
            return;

        UiControl *top = nullptr;
        for (std::size_t i = 0; i < mUiControls.size(); ++i)
        {
            UiControl *control = mUiControls[i];
            GameObject *object = control ? control->owner() : nullptr;
            if (!object || object->disposed() || !object->isActiveAndVisibleInHierarchy() || !control->active())
                continue;
            control->updateLayout();
            control->resetInput();
            if (!control->interactive())
                continue;
            if (!top || object->zIndex() >= top->owner()->zIndex())
                top = control;
        }

        Input *input = nullptr;
        // UiControls owns the input pointer; this call has no global input
        // access by design, so controls cannot each independently consume it.
        // The viewport gate also keeps editor chrome out of game controls.
        extern Input *GetUiInputInternal();
        input = GetUiInputInternal();
        if (!top || !input)
            return;
        const float x = input->MouseX() - viewport.x;
        const float y = input->MouseY() - viewport.y;
        if (x < 0.0f || y < 0.0f || x >= viewport.width || y >= viewport.height)
            return;

        UiControl *hit = nullptr;
        for (std::size_t i = 0; i < mUiControls.size(); ++i)
        {
            UiControl *control = mUiControls[i];
            GameObject *object = control ? control->owner() : nullptr;
            if (!control || !object || !object->isActiveAndVisibleInHierarchy() || !control->active() ||
                !control->interactive() || !control->contains(x, y))
                continue;
            if (!hit || object->zIndex() >= hit->owner()->zIndex())
                hit = control;
        }
        if (hit)
            hit->handleInput(x, y, input->MouseDown(0), input->MousePressed(0), input->MouseReleased(0));
    }

    void Scene::flushDisposed()
    {
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
