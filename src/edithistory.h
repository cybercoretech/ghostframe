#pragma once

#include "editrecipe.h"

#include <QList>

// Undo/redo over whole recipe states. Slider drags are committed once, on
// release, so one drag is one undo step.
class EditHistory {
public:
    explicit EditHistory(int limit = 200) : m_limit(limit) { reset({}); }

    void reset(const EditRecipe &initial) { m_states = {initial}; m_index = 0; }
    // Records `state` as the newest step and drops any redo branch.
    // Returns false if it equals the current state (nothing to record).
    bool commit(const EditRecipe &state);
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_states.size() - 1; }
    EditRecipe undo() { if (canUndo()) --m_index; return current(); }
    EditRecipe redo() { if (canRedo()) ++m_index; return current(); }
    EditRecipe current() const { return m_states.at(m_index); }
    int size() const { return int(m_states.size()); }

private:
    QList<EditRecipe> m_states;
    qsizetype m_index = 0;
    int m_limit;
};
