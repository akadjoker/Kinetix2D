#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d::editor
{

// A file or folder moved or renamed, as paths relative to the project root; a
// folder takes everything under it along.
struct AssetMove
{
    ct::String from;
    ct::String to;
    bool directory = false;

    // Where path is after the move, written as it was: relative to the root,
    // to assets/ (the other search path), or absolute under the root. False
    // when the move does not touch it.
    bool apply(const ct::String& root, const ct::String& path, ct::String& out) const;
};

// A path relative to the root with no empty, "." or ".." part.
bool isPlainProjectPath(const ct::String& path);
// Deletes a file, or a folder and what is in it. A link is removed, never
// followed: what it points at may be outside the project.
bool removeAssetTree(const ct::String& path);

// Points every string in data that names the moved asset at its new place;
// how many changed.
int rewritePaths(ct::Json& data, const ct::String& root, const AssetMove& move);

// Whether a string in data names the asset (a file, or one under a folder).
bool namesAsset(const ct::Json& data, const ct::String& root, const AssetMove& asset);

// The project's data files by absolute path: scenes, prefabs, tilesets and
// the .json ones (sprites, atlases), outside hidden folders and export/.
// project.k2dproj is not one: the editor holds it.
void projectDataFiles(const ct::String& root, ct::Vector<ct::String>& out);
void projectScripts(const ct::String& root, ct::Vector<ct::String>& out);

// Rewrites the data files that name the moved asset; the changed ones are
// added to changed, relative to the root. False when one could not be written.
bool rewriteProjectFiles(const ct::String& root, const AssetMove& move, ct::Vector<ct::String>& changed);

// The data files that name the asset, and the scripts whose text holds its
// path, relative to the root.
void findReferences(const ct::String& root, const AssetMove& asset, ct::Vector<ct::String>& files,
                    ct::Vector<ct::String>& scripts);

} // namespace k2d::editor
