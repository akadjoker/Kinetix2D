#include "core/EditorApplication.h"

int main()
{
    k2d::editor::EditorApplication editor;
    if (!editor.initialize())
        return 1;
    const int result = editor.run();
    editor.shutdown();
    return result;
}
