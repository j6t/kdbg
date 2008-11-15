/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef PGMSETTINGS_H
#define PGMSETTINGS_H

#include <qtabdialog.h>

class QButtonGroup;
class QLineEdit;


class ChooseDriver : public QWidget
{
public:
    ChooseDriver(QWidget* parent);
    void setDebuggerCmd(const QString& cmd);
    QString debuggerCmd() const;
protected:
    QLineEdit* m_debuggerCmd;
};


class OutputSettings : public QWidget
{
    Q_OBJECT
public:
    OutputSettings(QWidget* parent);
    void setTTYLevel(int l);
    int ttyLevel() const { return m_ttyLevel; }
protected:
    int m_ttyLevel;
    QButtonGroup* m_group;
protected slots:
    void slotLevelChanged(int);
};


class ProgramSettings : public QTabDialog
{
    Q_OBJECT
public:
    ProgramSettings(QWidget* parent, QString exeName, bool modal = true);

public:
    ChooseDriver m_chooseDriver;
    OutputSettings m_output;
};

#endif
