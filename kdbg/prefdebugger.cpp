/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <klocalizedstring.h>			/* i18n */
#include "prefdebugger.h"

PrefDebugger::PrefDebugger(QWidget* parent) :
	QWidget(parent),
	m_grid(this),
	m_defaultHint(this),
	m_debuggerCCppLabel(this),
	m_debuggerCCpp(this),
	m_terminalHint(this),
	m_terminalLabel(this),
	m_terminal(this)
{
    m_defaultHint.setText(i18n("To revert to the default settings, clear the entries."));
    m_defaultHint.setMinimumHeight(m_defaultHint.sizeHint().height());
    m_grid.addWidget(&m_defaultHint, 0, 1);

    m_debuggerCCppLabel.setText(i18n("How to invoke &GDB:"));
    m_debuggerCCppLabel.setMinimumSize(m_debuggerCCppLabel.sizeHint());
    m_debuggerCCppLabel.setBuddy(&m_debuggerCCpp);
    m_debuggerCCpp.setMinimumSize(m_debuggerCCpp.sizeHint());
    m_grid.addWidget(&m_debuggerCCppLabel, 1, 0);
    m_grid.addWidget(&m_debuggerCCpp, 1, 1);

    m_terminalHint.setText(i18n("%T will be replaced with a title string,\n"
				"%C will be replaced by a Bourne shell script that\n"
				"keeps the terminal window open."));
    m_terminalHint.setMinimumHeight(m_terminalHint.sizeHint().height());
    m_grid.addWidget(&m_terminalHint, 2, 1);

    m_terminalLabel.setText(i18n("&Terminal for program output:"));
    m_terminalLabel.setMinimumSize(m_terminalLabel.sizeHint());
    m_terminalLabel.setBuddy(&m_terminal);
    m_terminal.setMinimumSize(m_terminal.sizeHint());
    m_grid.addWidget(&m_terminalLabel, 3, 0);
    m_grid.addWidget(&m_terminal, 3, 1);

    m_grid.setColumnStretch(1, 10);
    // last (empty) row gets all the vertical stretch
    m_grid.setRowStretch(4, 10);
}
