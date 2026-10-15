#include "edithistory.h"

bool EditHistory::commit(const EditRecipe &state) {
    if (state == current()) return false;
    m_states.resize(m_index + 1);
    m_states.append(state);
    if (m_states.size() > m_limit) m_states.removeFirst();
    m_index = m_states.size() - 1;
    return true;
}
