// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

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
