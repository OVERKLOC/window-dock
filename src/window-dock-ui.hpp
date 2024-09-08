#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <windows.h>
#include <shellscalingapi.h>
#include <tchar.h>
#include <psapi.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QDialog>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidgetItem>
#include <QScreen>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QTableWidget>
#include <QHeaderView>
#include <QWidget>
#include <QDockWidget>
#include <QStandardPaths>
#include <QDir>
#include <QSet>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>

#include <vector>
#include <string>
#include <utility>

#pragma comment(lib, "Shcore.lib")


constexpr const char* PLUGIN_PREFIX = "window_dock_";
constexpr const char* CONFIG_FILE = "config.json";


class EmbeddedWindowWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmbeddedWindowWidget(QWidget *parent = nullptr)
        : QWidget(parent), embeddedHwnd(nullptr) {
        setContentsMargins(0, 0, 0, 0);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_DontCreateNativeAncestors, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // Allow resizing
    }

    explicit EmbeddedWindowWidget(HWND hwnd, QWidget *parent = nullptr)
        : QWidget(parent), embeddedHwnd(hwnd) {
        setContentsMargins(0, 0, 0, 0);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_DontCreateNativeAncestors, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // Allow resizing
        adjustWindowSize();
    }

    HWND getEmbeddedHwnd() const {
        return embeddedHwnd;
    }

    void setEmbeddedHwnd(HWND hwnd) {
        embeddedHwnd = hwnd;
        if (hwnd) {
            SetParent(hwnd, (HWND)this->winId());
            adjustWindowSize();
        }
    }

    void adjustWindowSize() {
        // blog(LOG_INFO, "adjustWindowSize called");
        if (!embeddedHwnd) {
            // blog(LOG_WARNING, "adjustWindowSize called but embeddedHwnd is NULL.");
            return;
        }

        // Get the DPI of the source window
        HMONITOR hMonitor = MonitorFromWindow(embeddedHwnd, MONITOR_DEFAULTTONEAREST);
        UINT sourceDpiX, sourceDpiY;
        if (GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &sourceDpiX, &sourceDpiY) != S_OK) {
            sourceDpiX = 96; // Default to 96 DPI if unable to retrieve
            sourceDpiY = 96;
        }

        // Get the DPI of the destination window (OBS dock)
        HMONITOR hDestMonitor = MonitorFromWindow((HWND)this->winId(), MONITOR_DEFAULTTONEAREST);
        UINT destDpiX, destDpiY;
        if (GetDpiForMonitor(hDestMonitor, MDT_EFFECTIVE_DPI, &destDpiX, &destDpiY) != S_OK) {
            destDpiX = 96; // Default to 96 DPI if unable to retrieve
            destDpiY = 96;
        }

        // Calculate the scaling factor
        float scaleX = (float)destDpiX / (float)sourceDpiX;
        float scaleY = (float)destDpiY / (float)sourceDpiY;

        // Get the size of the OBS dock
        RECT rect;
        GetClientRect((HWND)this->winId(), &rect);

        // Calculate the new size for the embedded window
        int newWidth = (int)((rect.right - rect.left) * scaleX);
        int newHeight = (int)((rect.bottom - rect.top) * scaleY);

        // Set the new window size and position
        SetWindowPos(embeddedHwnd, NULL, rect.left, rect.top, newWidth, newHeight, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        // blog(LOG_INFO, "Adjusted window size: new width = %d, new height = %d, scaleX = %f, scaleY = %f", newWidth, newHeight, scaleX, scaleY);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        // blog(LOG_INFO, "resizeEvent called");
        QWidget::resizeEvent(event);

        // blog(LOG_INFO, "EmbeddedWindowWidget resized: new width = %d, new height = %d", width(), height());

        if (embeddedHwnd) {
            // blog(LOG_INFO, "embeddedHwnd");
            // blog(LOG_INFO, "Widget resized: new width = %d, new height = %d", width(), height());
            adjustWindowSize(); // Adjust the window size during resize
        }
    }

private:
    HWND embeddedHwnd;
};


struct DockEntry {
    QString oldDesktopWindow;
    QString oldDesktopWindowWithProgramName;
    QString oldDockId;
    QString oldDockName;
    QString newDesktopWindow;
    QString newDesktopWindowWithProgramName;
    QString newDockId;
    QString newDockName;

    bool isNew() const {
        return oldDockId.isEmpty();
    }

    bool isModified() const {
        return oldDesktopWindow != newDesktopWindow ||
            oldDesktopWindowWithProgramName != newDesktopWindowWithProgramName ||
            oldDockId != newDockId ||
            oldDockName != newDockName;
    }

    bool isUnchanged() const {
        return oldDesktopWindow == newDesktopWindow &&
            oldDesktopWindowWithProgramName == newDesktopWindowWithProgramName &&
            oldDockId == newDockId &&
            oldDockName == newDockName;
    }
};


class WindowDockUI : public QWidget {
    Q_OBJECT

public:
    explicit WindowDockUI(QWidget *parent = nullptr);
    void openCustomWindowDocksUI(QWidget *parent = nullptr);

    QJsonArray loadConfigFile();
    void detachEmbeddedWindow(const QString &dockId);
    void releaseEmbeddedWindow(EmbeddedWindowWidget *dockWidget);
    void releaseEmbeddedWindowByDockId(const QString &dockId);
    void freeEmbeddedWindowsOnClose();
    void clearLayout(QLayout *layout);
    void restoreDocksOnStartup();
    void applyChanges();

private:
    void addNewRow(QTableWidget *tableWidget);
    void populateDesktopWindowsComboBox(QComboBox* comboBox);
    void addDetachButtonsToTable(QTableWidget *tableWidget);
    void addTrashButtonsToTable(QTableWidget *tableWidget);
    
    void loadDockEntries(QTableWidget *tableWidget);
    void saveDockEntries(const QList<DockEntry> &dockEntries);

    QString extractWindowTitle(const QString &fullName);
    QJsonObject getDockConfigById(const QString &dockId);
    void attemptWindowCapture(const QString &dockId, const QString &windowTitle);

    EmbeddedWindowWidget* createBlankDockContent(const QString &dockId, const QString &windowTitle);
    EmbeddedWindowWidget* createDockContent(const QString &dockId, const QString &dockName, const QString &windowTitle);

    void createOrUpdateDock(const QString &dockId, const QString &dockName, const QString &windowTitle);
    void updateDockContent(EmbeddedWindowWidget *dockWidget, const QString &dockId, const QString &windowTitle);
    void initiateDockCreationOnStartup(const QString &dockId, const QString &dockName, const QString &windowTitle);

    QWidget *customWindowDocksUI = nullptr;
    QMap<QString, EmbeddedWindowWidget*> activeDocks;
    std::vector<std::pair<QString, HWND>> getDesktopWindows();
    QList<DockEntry> dockEntries;
};