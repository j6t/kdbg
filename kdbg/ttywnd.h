/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef TTYWND_H
#define TTYWND_H

#include <QPlainTextEdit>

class QSocketNotifier;

/**
 * This class is cortesy Judin Max <novaprint@mtu-net.ru>.
 *
 * The master side of the TTY is the emulator.
 *
 * The slave side is where a client process can write output and can read
 * input. For this purpose, it must open the file (terminal device) whose
 * name is returned by @ref slaveTTY for both reading and writing. To
 * establish the stdin, stdout, and stderr channels the file descriptor
 * obtained by this must be dup'd to file descriptors 0, 1, and 2, resp.
 */
class STTY : public QObject
{
    Q_OBJECT
public: 
    STTY();
    ~STTY();

    QString slaveTTY(){ return m_slavetty; };

protected slots:
    void outReceived(int);

signals:
    void output(char* buffer, int charlen);

protected:
    int m_masterfd;
    int m_slavefd;
    QSocketNotifier* m_outNotifier;
    QString m_slavetty;
    bool findTTY();
};

class TTYWindow : public QPlainTextEdit
{
    Q_OBJECT
public:
    TTYWindow(QWidget* parent);
    ~TTYWindow();

    QString activate();
    void deactivate();

protected:
    STTY* m_tty;
    QTextCursor m_pos;		//!< tracks horizontal cursor position
    virtual void contextMenuEvent(QContextMenuEvent*);

protected slots:
    void slotAppend(char* buffer, int count);
    void slotClear();
};

#endif // TTYWND_H
