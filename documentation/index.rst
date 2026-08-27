Kinetix2D documentation
=======================

Kinetix2D is a 2D game engine with an editor. The physics library ``kx``,
the scene and component engine ``k2d``, Python-syntax scripting on the zen VM,
and an ImGui editor that edits the same scene model the runner plays.

The project is C++14 and uses the ``ct`` containers instead of the STL.
Rendering targets the GLES 3.0 subset (WebGL2-compatible) for desktop, web and
mobile.

These pages describe what the code actually exposes. They are written from the
public headers and the script bindings, so every function and argument listed
here exists in the source.

.. toctree::
   :maxdepth: 2
   :caption: Scripting

   scripting/index

.. toctree::
   :maxdepth: 2
   :caption: C++

   cpp/index
