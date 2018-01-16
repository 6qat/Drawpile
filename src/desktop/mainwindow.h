/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QUrl>

#include "tools/tool.h"

class QActionGroup;
class QMessageBox;
class QUrl;
class QLabel;
class QSplitter;
class QTimer;
class QToolButton;

class Document;

namespace widgets {
	class CanvasView;
	class NetStatus;
	class ChatBox;
	class UserList;
	class ViewStatus;
}
namespace docks {
	class ToolSettings;
	class BrushPalette;
	class InputSettings;
	class LayerList;
	class PaletteBox;
	class ColorBox;
	class Navigator;
}
namespace dialogs {
	class PlaybackDialog;
	class HostDialog;
	class SessionSettingsDialog;
	class ServerLogDialog;
}
namespace drawingboard {
	class CanvasScene;
}
namespace canvas {
	class CanvasModel;
	class SessionLoader;
}

namespace net {
	class Client;
	class LoginHandler;
}

namespace recording {
	class Writer;
	class Reader;
}

namespace tools {
	class ToolController;
}

class ShortcutDetector;

//! The application main window
class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow(bool restoreWindowPosition=true);
	~MainWindow();

	MainWindow *loadDocument(canvas::SessionLoader &loader);
	MainWindow *loadRecording(recording::Reader *reader);

	//! Host a session using the settings from the given dialog
	void hostSession(dialogs::HostDialog *dlg);

	//! Connect to a host and join a session if full URL is provided.
	void joinSession(const QUrl& url, bool autoRecord=false);

	//! Check if the current board can be replaced
	bool canReplace() const;

	//! Save settings and exit
	void exit();

public slots:
	// Triggerable actions
	void showNew();
	void open();
	void open(const QUrl &url);
	void save();
	void saveas();
	void exportAnimation();
	void showFlipbook();

	static void showSettings();
	void reportAbuse();
	void tryToGainOp();
	void resetSession();
	void terminateSession();

	void host();
	void join(const QUrl &defaultUrl=QUrl());
	void leave();

	void toggleFullscreen();
	void setShowAnnotations(bool show);
	void setShowLaserTrails(bool show);

	void selectTool(QAction *tool);

	static void about();
	static void homepage();

	//! Create a blank new document
	void newDocument(const QSize &size, const QColor &background);

private slots:
	void toggleRecording();

	void onOperatorModeChange(bool op);
	void onImageCmdLockChange(bool lock);

	void onServerConnected();
	void onServerLogin();
	void onServerDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
	void onNsfmChanged(bool nsfm);

	void updateLockWidget();
	void setRecorderStatus(bool on);

	void updateShortcuts();
	void updateTabletSupportMode();

	void paste();
	void pasteFile();
	void pasteFile(const QUrl &url);
	void pasteImage(const QImage &image, const QPoint *point=nullptr);
	void dropUrl(const QUrl &url);

	void clearOrDelete();

	void resizeCanvas();
	void markSpotForRecording();

	void toolChanged(tools::Tool::Type tool);

	void selectionRemoved();

	void hotBorderMenubar(bool show);

	void setFreezeDocks(bool freeze);

	void updateTitle();

	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onCanvasSaveStarted();
	void onCanvasSaved(const QString &errorMessage);

protected:
	void closeEvent(QCloseEvent *event);
	bool event(QEvent *event);

private:
	//! Confirm saving of image in a format that doesn't support all required features
	bool confirmFlatten(QString& file) const;

	QAction *makeAction(const char *name, const char *icon, const QString& text, const QString& tip = QString(), const QKeySequence& shortcut = QKeySequence(), bool checkable=false);
	QAction *getAction(const QString &name);

	//! Load customized shortcuts
	void loadShortcuts();

	//! Add a new entry to recent files list
	void addRecentFile(const QString& file);

	//! Enable or disable drawing tools
	void setDrawingToolsEnabled(bool enable);

	//! Display an error message
	void showErrorMessage(const QString& message, const QString& details=QString());

	void readSettings(bool windowpos=true);
	void writeSettings();

	void createDocks();
	void setupActions();

	QSplitter *_splitter;

	docks::ToolSettings *_dock_toolsettings;
	docks::BrushPalette *m_dockBrushPalette;
	docks::InputSettings *_dock_input;
	docks::LayerList *_dock_layers;
	docks::ColorBox *_dock_colors;
	widgets::ChatBox *m_chatbox;
	widgets::UserList *_userlist;
	docks::Navigator *_dock_navigator;

	widgets::CanvasView *_view;

	QStatusBar *m_viewStatusBar;
	QLabel *_lockstatus;
	QLabel *_recorderstatus;
	widgets::NetStatus *m_netstatus;
	widgets::ViewStatus *_viewstatus;
	QToolButton *_statusChatButton;

	dialogs::PlaybackDialog *m_playbackDialog;
	dialogs::SessionSettingsDialog *m_sessionSettings;
	dialogs::ServerLogDialog *m_serverLogDialog;

	drawingboard::CanvasScene *_canvasscene;

	QMenu *_recent;

	QActionGroup *_currentdoctools; // actions relating to the currently open document
	QActionGroup *m_admintools; // session operator actions
	QActionGroup *m_modtools; // session moderator tools
	QActionGroup *m_docadmintools; // current document related operator actions
	QActionGroup *m_drawingtools; // drawing tool selection
	QActionGroup *m_brushSlots; // tool slot shortcuts

	int _lastToolBeforePaste; // Last selected tool before Paste was used

	// Remember window state to return from fullscreen mode
	QByteArray m_fullscreenOldState;
	QRect m_fullscreenOldGeometry;
	bool m_fullscreenOldMaximized;

	QElapsedTimer _toolChangeTime; // how long the user has held down the tool change button
	ShortcutDetector *_tempToolSwitchShortcut;

	Document *m_doc;
	bool m_exitAfterSave;
};

#endif

