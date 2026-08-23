#pragma once

#include "k2d/Component.h"

#include <ct/json.hpp>

namespace k2d
{

    class GameObject;
    class Scene;
    class Assets;

    // Central factory + registry for turning a GameObject subtree into JSON
    // and back. Prefab and Scene (and later the editor) both call into this
    // ONE walker instead of each re-implementing "walk the tree, dump the
    // components" -- and Serializer itself never switches on ComponentType:
    // it looks each type up in a small table (see Serializer.cpp). Adding a
    // new serializable component is one row in that table, not a change to
    // every caller.
    //
    // `assets`, when given, resolves Texture (and future asset) references
    // to/from their registered name so a texture pointer never gets written
    // raw into the file. Pass nullptr and those fields just don't round-trip
    // -- everything else still does.
    class Serializer
    {
    public:
        static ct::Json WriteObject(const GameObject &object, Assets *assets = nullptr);
        static GameObject *ReadObject(Scene &scene, const ct::Json &json,
                                       GameObject *parent = nullptr, Assets *assets = nullptr);

        // Whether `type` has a registered writer/reader -- lets a caller warn
        // instead of silently dropping a component it expected to round-trip.
        static bool IsRegistered(ComponentType type);
    };

}
