#include "k2d/Scene.h"

namespace k2d
{

    Scene::Scene() : mRoot("root"), mNextId(1), mObjectCount(0), mLateUpdateCount(0)
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

    GameObject *Scene::find(const char *name) const
    {
        return findRecursive(&mRoot, name);
    }

    std::size_t Scene::objectCount() const
    {
        return mObjectCount;
    }

    void Scene::update(float deltaTime)
    {
        GameObject **objects = mObjects.data();
        const std::size_t count = mObjects.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            GameObject *object = objects[i];
            if (!object->disposed() && object->isActiveInHierarchy())
                object->updateComponents(deltaTime);
        }
        if (mLateUpdateCount > 0)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                GameObject *object = objects[i];
                if (!object->disposed() && object->isActiveInHierarchy())
                    object->lateUpdateComponents(deltaTime);
            }
        }
        flushDisposed();
    }

    void Scene::render(BatchRenderer &batch)
    {
        GameObject **objects = mObjects.data();
        const std::size_t count = mObjects.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            GameObject *object = objects[i];
            if (object->isActiveAndVisibleInHierarchy())
                object->renderComponents(batch);
        }
    }

    void Scene::clear()
    {
        mRoot.deleteChildrenRaw();
        mObjects.clear();
        mLateUpdateCount = 0;
        mObjectCount = 0;
        mNextId = 1;
    }

    void Scene::registerBranch(GameObject *object)
    {
        object->mScene = this;
        object->mId = mNextId++;
        ++mObjectCount;
        mObjects.push_back(object);
        for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count); ++i)
        {
            Component *component = object->mComponents[i];
            if (component && (component->mEvents & ComponentEventLateUpdate) != 0)
                ++mLateUpdateCount;
        }
        for (std::size_t i = 0; i < object->childCount(); ++i)
            registerBranch(object->child(i));
    }

    void Scene::unregisterBranch(GameObject *object)
    {
        for (std::size_t i = 0; i < object->childCount(); ++i)
            unregisterBranch(object->child(i));
        for (uint8_t i = 0; i < static_cast<uint8_t>(ComponentType::Count); ++i)
        {
            Component *component = object->mComponents[i];
            if (component && (component->mEvents & ComponentEventLateUpdate) != 0)
                --mLateUpdateCount;
        }
        object->mScene = nullptr;
        --mObjectCount;
        for (std::size_t i = 0; i < mObjects.size(); ++i)
        {
            if (mObjects[i] == object)
            {
                mObjects.erase(mObjects.begin() + i);
                break;
            }
        }
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
