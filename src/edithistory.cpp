#include "edithistory.h"

bool EditHistory::commit(const EditRecipe &state, const QString &label) {
    if (state == current()) return false;
    m_states.resize(m_index + 1);
    m_states.append({state, label});
    if (m_states.size() > m_limit) m_states.removeFirst();
    m_index = m_states.size() - 1;
    return true;
}

QStringList EditHistory::labels() const {
    QStringList out;
    for (const auto &step : m_states) out.append(step.label);
    return out;
}
