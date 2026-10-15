#pragma once

#include "editrecipe.h"

#include <QList>
#include <QString>
#include <QStringList>

// Undo/redo over whole recipe states, each with a short label for the
// history panel. Slider drags are committed once, on release, so one drag
// is one undo step.
class EditHistory {
public:
    explicit EditHistory(int limit = 200) : m_limit(limit) { reset({}); }

    void reset(const EditRecipe &initial, const QString &label = QStringLiteral("Original")) {
        m_states = {{initial, label}};
        m_index = 0;
    }
    // Records `state` as the newest step and drops any redo branch.
    // Returns false if it equals the current state (nothing to record).
    bool commit(const EditRecipe &state, const QString &label = QStringLiteral("Edit"));
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_states.size() - 1; }
    EditRecipe undo() { if (canUndo()) --m_index; return current(); }
    EditRecipe redo() { if (canRedo()) ++m_index; return current(); }
    // Jumps to any step (from the history panel); the redo branch is kept.
    EditRecipe jumpTo(int index) { m_index = qBound(qsizetype(0), qsizetype(index), m_states.size() - 1); return current(); }
    EditRecipe current() const { return m_states.at(m_index).recipe; }
    int index() const { return int(m_index); }
    int size() const { return int(m_states.size()); }
    QStringList labels() const;

private:
    struct Step { EditRecipe recipe; QString label; };
    QList<Step> m_states;
    qsizetype m_index = 0;
    int m_limit;
};
