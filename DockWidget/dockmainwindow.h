/***************************************************************************
                         DockWidget part of KDEStudio
                             -------------------
    copyright            : (C) 1999 by Judin Max
    email                : novaprint@mtu-net.ru
 ***************************************************************************/

#ifndef DOCKMAINWINDOW_H
#define DOCKMAINWINDOW_H

#include <kmainwindow.h>
#include <qpixmap.h>

#include "dockmanager.h"

class QPopupMenu;
class KToolBar;
class KConfig;

#ifdef DOCK_ORIGINAL
struct dockPosData
{
  DockWidget* dock;
  DockWidget* dropDock;
  DockPosition pos;
  int sepPos;
};
#endif

class DockMainWindow : public KMainWindow
{Q_OBJECT
public:
  DockMainWindow( const char *name = 0L );
  ~DockMainWindow();

  void setDockManager( DockManager* manager );
  DockManager* manager(){ return dockManager; }

  void setView( QWidget* );
  DockWidget* getMainViewDockWidget(){ return viewDock; }

#ifdef DOCK_ORIGINAL
  void setMainDockWidget( DockWidget* );
  DockWidget* getMainDockWidget(){ return mainDockWidget; }
#endif

  DockWidget* createDockWidget( const char* name, const QPixmap &pixmap, QWidget* parent = 0L );

  void writeDockConfig( KConfig* c = 0L, QString group = QString() );
  void readDockConfig ( KConfig* c = 0L, QString group = QString() );

  void activateDock(){ dockManager->activate(); }

#ifdef DOCK_ORIGINAL
  QPopupMenu* dockMenu(){ return dockManager->dockMenu(); }
#endif

  void makeDockVisible( DockWidget* dock );
  void makeWidgetDockVisible( QWidget* widget );

  void setDockView( QWidget* );

#ifdef DOCK_ORIGINAL
protected slots:
//  void slotDockChange();
//  void slotToggled( int );
//  void slotReplaceDock( DockWidget* oldDock, DockWidget* newDock );

protected:
  void toolBarManager( bool toggled, dockPosData &data );

  DockWidget* mainDockWidget;
#endif
  DockManager* dockManager;

#ifdef DOCK_ORIGINAL
  dockPosData DockL;
  dockPosData DockR;
  dockPosData DockT;
  dockPosData DockB;

  KToolBar* toolbar;
#endif
  DockWidget* viewDock;
};

#endif


