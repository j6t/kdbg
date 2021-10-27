/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef PREFDEBUGGER_H
#define PREFDEBUGGER_H

#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>

class PrefDebugger : public QWidget
{
public:
    PrefDebugger(QWidget* parent);

    QGridLayout m_grid;

    // --- the hint about defaults
protected:
    QLabel m_defaultHint;

    // --- the debugger command
protected:
    QLabel m_debuggerCCppLabel;
    QLineEdit m_debuggerCCpp;
public:
    QString debuggerCmd() const { return m_debuggerCCpp.text(); }
    void setDebuggerCmd(const QString& cmd) { m_debuggerCCpp.setText(cmd); }

    // --- the output terminal
protected:
    QLabel m_terminalHint;
    QLabel m_terminalLabel;
    QLineEdit m_terminal;
    QLabel m_disassLabel;
    QComboBox m_disassCombo;

public:
    QString terminal() const { return m_terminal.text(); }
    void setTerminal(const QString& t) { m_terminal.setText(t); }
    QString globalDisassemblyFlavor() const;
    void setGlobalDisassemblyFlavor(const QString& flavor);
};

#endif // PREFDEBUGGER_H
