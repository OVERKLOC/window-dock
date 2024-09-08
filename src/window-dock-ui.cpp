#include "window-dock-ui.hpp"




/*-------------------------------------------------------------------------------------*/
/*-------------------------------------CONSTRUCTORS------------------------------------*/
/*-------------------------------------------------------------------------------------*/





WindowDockUI::WindowDockUI(QWidget *parent)
    : QWidget(parent) {
    // Constructor logic, if any
}




/*-------------------------------------------------------------------------------------*/
/*-----------------------------------CORE FUNCTIONS------------------------------------*/
/*-------------------------------------------------------------------------------------*/




std::vector<std::pair<QString, HWND>> WindowDockUI::getDesktopWindows() {
    std::vector<std::pair<QString, HWND>> windows;

    auto enumWindowsCallback = [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* windows = reinterpret_cast<std::vector<std::pair<QString, HWND>>*>(lParam);

        if (!hwnd || !IsWindow(hwnd)) {
            // blog(LOG_WARNING, "EnumWindowsProc: Invalid HWND encountered.");
            return TRUE;  // Continue enumeration even if an invalid window handle is found
        }

        char windowTitle[256];
        int length = GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));

        if (length > 0 && IsWindowVisible(hwnd)) {
            // Get the process ID associated with the window
            DWORD processId;
            GetWindowThreadProcessId(hwnd, &processId);

            // Open the process to retrieve the executable name
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (processHandle) {
                char processName[MAX_PATH];
                if (GetModuleBaseNameA(processHandle, NULL, processName, sizeof(processName))) {
                    // Format the string as "[APPLICATION_EXECUTABLE]: WINDOW_NAME"
                    QString title = QString("[%1]: %2").arg(processName).arg(QString::fromUtf8(windowTitle));
                    windows->emplace_back(title, hwnd);
                } else {
                    // blog(LOG_WARNING, "EnumWindowsProc: Failed to get process name for PID: %lu", processId);
                }
                CloseHandle(processHandle);
            } else {
                // blog(LOG_WARNING, "EnumWindowsProc: Failed to open process for PID: %lu", processId);
            }
        } else {
            // blog(LOG_INFO, "EnumWindowsProc: Skipping window: %p (length: %d, visible: %d)", hwnd, length, IsWindowVisible(hwnd));
        }

        return TRUE;  // Continue enumeration
    };

    if (!EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&windows))) {
        blog(LOG_ERROR, "EnumWindows failed with error: %lu", GetLastError());
    }

    return windows;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    QComboBox* comboBox = reinterpret_cast<QComboBox*>(lParam);
    if (!comboBox) {
        // blog(LOG_WARNING, "EnumWindowsProc: comboBox is null");
        return TRUE; // Continue enumeration if comboBox is null
    }

    char windowTitle[256];
    int length = GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));

    if (length > 0 && IsWindowVisible(hwnd)) {
        QString title = QString::fromUtf8(windowTitle);

        // Directly add the item and data to the comboBox
        comboBox->addItem(title, QVariant::fromValue(hwnd));
    }

    return TRUE; // Continue enumeration
}

void WindowDockUI::populateDesktopWindowsComboBox(QComboBox* comboBox) {
    // blog(LOG_INFO, "populateDesktopWindowsComboBox called");

    comboBox->clear();

    // Add the "Select a window..." placeholder
    comboBox->addItem(obs_module_text("DockManagement.DesktopWindowComboBoxPlaceholder"));

    // Get the list of windows
    auto windows = getDesktopWindows();

    // Populate the combo box in the main UI thread
    for (const auto& window : windows) {
        comboBox->addItem(window.first, QVariant::fromValue(window.second));
    }

    // Ensure the "Select a window..." option is selected by default
    comboBox->setCurrentIndex(0);
}

QString WindowDockUI::extractWindowTitle(const QString &fullName) {
    // blog(LOG_INFO, "Extracting window title: %s", fullName.toStdString().c_str());
    
    int delimiterIndex = fullName.indexOf("]:");

    if (delimiterIndex != -1 && delimiterIndex + 2 < fullName.length()) {
        QString windowTitle = fullName.mid(delimiterIndex + 2).trimmed(); // Extract after "]:"
        // blog(LOG_INFO, "Extracted window title: %s", windowTitle.toStdString().c_str());
        return windowTitle;
    }

    // blog(LOG_WARNING, "Delimiter not found or out-of-bounds, returning fullName as window title");
    return fullName;
}

QJsonArray WindowDockUI::loadConfigFile() {
    QString configDir = QDir::homePath() + "/AppData/Roaming/obs-studio/plugin_config/window-dock";
    QString configFilePath = configDir + "/" + CONFIG_FILE;

    // Check if directory exists, if not, create it
    QDir dir(configDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            blog(LOG_ERROR, "Failed to create directory: %s", configDir.toStdString().c_str());
            return QJsonArray(); // Return an empty JSON array on failure to create the directory
        }
    }

    QFile configFile(configFilePath);

    // If the config file doesn't exist, create an empty config file
    if (!configFile.exists()) {
        // blog(LOG_INFO, "Config file does not exist, creating a new empty config file.");

        // Open the file in WriteOnly mode and create it
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            blog(LOG_ERROR, "Failed to create config file: %s", configFilePath.toStdString().c_str());
            return QJsonArray(); // Return an empty JSON array if the file cannot be created
        }

        // Create an empty JSON array and wrap it in a QJsonDocument
        QJsonArray emptyArray;
        QJsonDocument emptyDoc(emptyArray);

        // Write the empty JSON document to the new config file
        configFile.write(emptyDoc.toJson());
        configFile.close();

        return emptyArray; // Return an empty JSON array since the file was just created
    }

    // Open the config file for reading
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        blog(LOG_ERROR, "Failed to open config file: %s", configFilePath.toStdString().c_str());
        return QJsonArray(); // Return an empty JSON array on failure to open
    }

    // Read and parse the JSON content
    QByteArray configData = configFile.readAll();
    configFile.close();

    // blog(LOG_INFO, "Config file contents: %s", configData.toStdString().c_str()); // Log the raw contents

    QJsonDocument configDoc = QJsonDocument::fromJson(configData);
    if (configDoc.isNull()) {
        blog(LOG_ERROR, "Failed to parse JSON document from config file.");
        return QJsonArray(); // Return an empty JSON array on parsing error
    }

    if (!configDoc.isArray()) {
        blog(LOG_ERROR, "JSON document is not an array.");
        return QJsonArray(); // Return an empty JSON array if the JSON document is not an array
    }

    return configDoc.array(); // Return the parsed JSON array
}




/*-------------------------------------------------------------------------------------*/
/*------------------------------------UI FUNCTIONS-------------------------------------*/
/*-------------------------------------------------------------------------------------*/




// https://github.com/obsproject/obs-studio/tree/34735be09441101217c1efc625b1b8d32fccac65/UI/data/themes
// https://doc.qt.io/qt-6/qstyle.html#StandardPixmap-enum

void WindowDockUI::addDetachButtonsToTable(QTableWidget *tableWidget) {
    // blog(LOG_INFO, "addDetachButtonsToTable called");

    int rowCount = tableWidget->rowCount();

    for (int row = 0; row < rowCount - 1; ++row) {
        QPushButton *detachButton = new QPushButton();
        QIcon detachButtonIcon(":/res/images/popout.svg");
        detachButton->setIcon(detachButtonIcon);

        tableWidget->setCellWidget(row, 2, detachButton);

        // Associate the button with the index of the dock entry
        detachButton->setProperty("dockEntryIndex", row);

        QPointer<QTableWidget> safeTableWidget = tableWidget;  // Use QPointer to avoid dangling pointers
        QObject::connect(detachButton, &QPushButton::clicked, [this, safeTableWidget, row]() {
            if (!safeTableWidget) return;  // Check if the table still exists

            // Retrieve the dock entry index associated with the button
            int index = safeTableWidget->cellWidget(row, 2)->property("dockEntryIndex").toInt();

            if (index >= 0 && index < dockEntries.size()) {
                const DockEntry &dockEntry = dockEntries.at(index);
                std::string dockId = dockEntry.oldDockId.toStdString();
                // blog(LOG_INFO, "Set to detach from: %s (Dock Id: %s)", dockEntry.oldDockName.toStdString().c_str(), dockId.c_str());
                detachEmbeddedWindow(QString::fromStdString(dockId));
            }
        });
    }
}

void WindowDockUI::addTrashButtonsToTable(QTableWidget *tableWidget) {
    // blog(LOG_INFO, "addTrashButtonsToTable called");

    int rowCount = tableWidget->rowCount();

    for (int row = 0; row < rowCount - 1; ++row) {
        QPushButton *trashButton = new QPushButton();
        QIcon trashIcon(":/res/images/trash.svg");
        trashButton->setIcon(trashIcon);

        tableWidget->setCellWidget(row, 3, trashButton);

        // Associate the button with the index of the dock entry
        trashButton->setProperty("dockEntryIndex", row);

        QPointer<QTableWidget> safeTableWidget = tableWidget;  // Use QPointer to avoid dangling pointers
        QObject::connect(trashButton, &QPushButton::clicked, [this, safeTableWidget, row]() {
            if (!safeTableWidget) return;  // Check if the table still exists

            // Retrieve the dock entry index associated with the button
            int index = safeTableWidget->cellWidget(row, 3)->property("dockEntryIndex").toInt();

            if (index >= 0 && index < dockEntries.size()) {
                // Remove the DockEntry from the list
                dockEntries.removeAt(index);
                // blog(LOG_INFO, "Removed dock entry at index %d", index);

                // Remove the corresponding row from the table widget
                safeTableWidget->removeRow(row);

                // Re-add the trash buttons to the updated table (optional, in case you want to reassign indices)
                addTrashButtonsToTable(safeTableWidget);
                addDetachButtonsToTable(safeTableWidget);
            }
        });
    }
}

// Add a new row to the table
void WindowDockUI::addNewRow(QTableWidget *tableWidget) {
    // blog(LOG_INFO, "addNewRow called");

    int newRow = tableWidget->rowCount();
    tableWidget->insertRow(newRow);

    QLineEdit *dockNameField = new QLineEdit();
    QComboBox *desktopWindowDropdown = new QComboBox();

    populateDesktopWindowsComboBox(desktopWindowDropdown);

    tableWidget->setCellWidget(newRow, 0, dockNameField);
    tableWidget->setCellWidget(newRow, 1, desktopWindowDropdown);

    addDetachButtonsToTable(tableWidget);
    addTrashButtonsToTable(tableWidget);

    QString previousDockName = dockNameField->text();

    auto saveDockEntryAndAddRow = [this, tableWidget, dockNameField, desktopWindowDropdown, newRow, previousDockName]() mutable {
        QString dockName = dockNameField->text().trimmed();
        QString desktopWindow = extractWindowTitle(desktopWindowDropdown->currentText());
        QString desktopWindowWithProgramName = desktopWindowDropdown->currentText();
        QString dockId = QString::fromStdString(PLUGIN_PREFIX) + dockName;

        int currentRow = tableWidget->indexAt(dockNameField->pos()).row();

        // Check if the dockName is already in use
        for (const DockEntry &entry : dockEntries) {
            // blog(LOG_INFO, "Dock name '%s' check to see if already taken.", entry.newDockName.toStdString().c_str());
            // blog(LOG_INFO, "Previous Dock Name (Current Table Row) '%s'", previousDockName.toStdString().c_str());
            // blog(LOG_INFO, "Current Dock Name (Current Table Row) '%s'", dockName.toStdString().c_str());
            
            if (entry.newDockName == dockName) {
                // Dock name is already taken, reset to previous value
                dockNameField->setText(previousDockName);  // Reset to the previous valid name
                // blog(LOG_INFO, "Dock name '%s' is already taken, resetting to '%s'", dockName.toStdString().c_str(), previousDockName.toStdString().c_str());
                return;
            }
        }

        // If the name is valid, update previousDockName to the current dock name
        previousDockName = dockName;

        // Ensure it's the last row and both fields are populated
        if (currentRow == tableWidget->rowCount() - 1 && !dockName.isEmpty() && desktopWindowDropdown->currentIndex() > 0) {
            // Append the new entry to dockEntries
            DockEntry newEntry;
            newEntry.newDockName = dockName;
            newEntry.newDesktopWindow = desktopWindow;
            newEntry.newDesktopWindowWithProgramName = desktopWindowWithProgramName;
            newEntry.newDockId = dockId;

            dockEntries.append(newEntry);

            // blog(LOG_INFO, "New dock entry saved: Dock Name = %s, Desktop Window = %s", dockName.toStdString().c_str(), desktopWindow.toStdString().c_str());

            // Create a new blank row for data entry
            addNewRow(tableWidget);
        }
    };

    QObject::connect(dockNameField, &QLineEdit::editingFinished, saveDockEntryAndAddRow);
    QObject::connect(desktopWindowDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), saveDockEntryAndAddRow);
}

// Open the custom window docks UI
void WindowDockUI::openCustomWindowDocksUI(QWidget *parent) {
    // blog(LOG_INFO, "openCustomWindowDocksUI called");

    if (customWindowDocksUI) {
        // blog(LOG_INFO, "dock management UI already open, bring to the foreground");
        // If the window is already open, bring it to the foreground
        customWindowDocksUI->show();
        customWindowDocksUI->raise();
        customWindowDocksUI->activateWindow();
    } else {
        // blog(LOG_INFO, "display dock management UI");

        customWindowDocksUI = new QDialog(parent);
        customWindowDocksUI->setWindowTitle(obs_module_text("DockManagement.WindowTitle"));
        customWindowDocksUI->resize(760, 360);

        QVBoxLayout *mainLayout = new QVBoxLayout(customWindowDocksUI);

        QLabel *label = new QLabel(obs_module_text("DockManagement.Description"), customWindowDocksUI);
        label->setWordWrap(true);
        mainLayout->addWidget(label);

        QTableWidget *tableWidget = new QTableWidget(0, 4, customWindowDocksUI);
        tableWidget->setHorizontalHeaderLabels(QStringList() << obs_module_text("DockManagement.DockName") << obs_module_text("DockManagement.DesktopWindow") << "" << "");
        tableWidget->verticalHeader()->setVisible(false);
        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        tableWidget->setColumnWidth(2, 20);
        tableWidget->setColumnWidth(3, 20);

        loadDockEntries(tableWidget);
        mainLayout->addWidget(tableWidget);

        addNewRow(tableWidget);

        addDetachButtonsToTable(tableWidget);
        addTrashButtonsToTable(tableWidget);

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();

        QPushButton *applyButton = new QPushButton(obs_module_text("DockManagement.Apply"), customWindowDocksUI);
        applyButton->setFixedWidth(70);
        buttonLayout->addWidget(applyButton);

        QPushButton *closeButton = new QPushButton(obs_module_text("DockManagement.Close"), customWindowDocksUI);
        closeButton->setFixedWidth(70);
        buttonLayout->addWidget(closeButton);

        mainLayout->addLayout(buttonLayout);

        QObject::connect(applyButton, &QPushButton::clicked, [this, tableWidget]() {
            applyChanges();
            loadDockEntries(tableWidget);
            addNewRow(tableWidget);

            // Bring the dialog back into focus after applying changes
            customWindowDocksUI->raise();  // Bring the dialog to the top
            customWindowDocksUI->activateWindow();  // Set focus back to the dialog

            addDetachButtonsToTable(tableWidget);
            addTrashButtonsToTable(tableWidget);
        });

        QObject::connect(closeButton, &QPushButton::clicked, [this, tableWidget]() {
            applyChanges();
            customWindowDocksUI->close();
        });

        customWindowDocksUI->setAttribute(Qt::WA_DeleteOnClose);
        customWindowDocksUI->show();

        // Reset the pointer when the window is closed
        QObject::connect(customWindowDocksUI, &QDialog::destroyed, [this]() {
            customWindowDocksUI = nullptr;
        });
    }
}

void WindowDockUI::loadDockEntries(QTableWidget *tableWidget) {
    // blog(LOG_INFO, "loadDockEntries called");

    // Clear the active dockEntries list to start fresh
    dockEntries.clear();

    QJsonArray docksArray = loadConfigFile();
    if (docksArray.isEmpty()) {
        // blog(LOG_INFO, "Config file is empty or failed to load. No dock entries to load.");
        return;
    }

    tableWidget->setRowCount(docksArray.size());
    for (int i = 0; i < docksArray.size(); ++i) {
        QJsonObject dockObject = docksArray[i].toObject();

        DockEntry entry;
        entry.oldDockName = dockObject["dockName"].toString();
        entry.oldDesktopWindow = dockObject["desktopWindow"].toString();
        entry.oldDesktopWindowWithProgramName = dockObject["desktopWindowWithProgramName"].toString();
        entry.oldDockId = dockObject["dockId"].toString();

        entry.newDockName = entry.oldDockName;
        entry.newDesktopWindow = entry.oldDesktopWindow;
        entry.newDesktopWindowWithProgramName = entry.oldDesktopWindowWithProgramName;
        entry.newDockId = entry.oldDockId;

        // Add the entry to the active list
        dockEntries.append(entry);

        // Populate UI with the values from the entry
        QLineEdit *dockNameField = new QLineEdit(entry.oldDockName);
        QComboBox *desktopWindowDropdown = new QComboBox();
        desktopWindowDropdown->addItem(entry.oldDesktopWindowWithProgramName);

        tableWidget->setCellWidget(i, 0, dockNameField);
        tableWidget->setCellWidget(i, 1, desktopWindowDropdown);

        QString previousDockName = entry.newDockName;

        // Connect signals to ensure editing is handled correctly
        auto saveDockEntryAndAddRow = [this, tableWidget, dockNameField, desktopWindowDropdown, i, previousDockName]() mutable {
            QString dockName = dockNameField->text().trimmed();
            QString desktopWindow = extractWindowTitle(desktopWindowDropdown->currentText());
            QString desktopWindowWithProgramName = desktopWindowDropdown->currentText();
            QString dockId = QString::fromStdString(PLUGIN_PREFIX) + dockName;

            // Check if the dockName is already in use
            for (const DockEntry &entry : dockEntries) {
                // blog(LOG_INFO, "Dock name '%s' check to see if already taken.", entry.newDockName.toStdString().c_str());
                // blog(LOG_INFO, "Previous Dock Name (Current Table Row) '%s'", previousDockName.toStdString().c_str());
                // blog(LOG_INFO, "Current Dock Name (Current Table Row) '%s'", dockName.toStdString().c_str());
                
                if (entry.newDockName == dockName) {
                    // Dock name is already taken, reset to previous value
                    dockNameField->setText(previousDockName);  // Reset to the previous valid name
                    // blog(LOG_INFO, "Dock name '%s' is already taken, resetting to '%s'", dockName.toStdString().c_str(), previousDockName.toStdString().c_str());
                    return;
                }
            }

            previousDockName = dockName;

            // Validate and handle existing rows
            if (i >= 0 && i < tableWidget->rowCount()) {
                // Update the entry
                DockEntry updatedEntry;
                updatedEntry.newDockName = dockName;
                updatedEntry.newDesktopWindow = desktopWindow;
                updatedEntry.newDesktopWindowWithProgramName = desktopWindowWithProgramName;
                updatedEntry.newDockId = dockId;

                dockEntries[i] = updatedEntry;  // Update the existing entry in the list

                // blog(LOG_INFO, "Dock entry updated: Dock Name = %s, Desktop Window = %s", dockName.toStdString().c_str(), desktopWindow.toStdString().c_str());
            }
        };

        // Connect the signal for each QLineEdit
        QObject::connect(dockNameField, &QLineEdit::editingFinished, saveDockEntryAndAddRow);
    }

    // blog(LOG_INFO, "Dock entries successfully loaded into UI and active list.");
}

void WindowDockUI::applyChanges() {
    // blog(LOG_INFO, "applyChanges called");

    // Load the existing dock configurations
    QJsonArray docksArray = loadConfigFile();

    QMap<QString, QJsonObject> currentDocksMap;
    for (const QJsonValue &value : docksArray) {
        QJsonObject dockObject = value.toObject();
        QString dockId = dockObject["dockId"].toString();
        currentDocksMap.insert(dockId, dockObject);
    }

    // Identify docks to remove
    QStringList docksToRemove;
    for (const QString &dockId : currentDocksMap.keys()) {
        bool found = false;
        for (const DockEntry &entry : dockEntries) {
            if (dockId == entry.oldDockId) {
                found = true;
                break;
            }
        }
        if (!found) {
            docksToRemove.append(dockId);
        }
    }

    // blog(LOG_INFO, "Current docks in config:");
    // for (const QString &dockId : currentDocksMap.keys()) {
        // blog(LOG_INFO, "Config Dock ID: %s", dockId.toStdString().c_str());
    // }

    // blog(LOG_INFO, "UI docks:");
    // for (const DockEntry &entry : dockEntries) {
    //     blog(
    //         LOG_INFO, "UI Dock Entry: oldDockId=%s, newDockId=%s,  oldDockName=%s, newDockName=%s, oldDesktopWindow=%s, newDesktopWindow=%s,  oldDesktopWindowWithProgramName=%s, newDesktopWindowWithProgramName=%s", 
    //         entry.oldDockId.toStdString().c_str(), entry.newDockId.toStdString().c_str(),
    //         entry.oldDockName.toStdString().c_str(), entry.newDockName.toStdString().c_str(),
    //         entry.oldDesktopWindow.toStdString().c_str(), entry.newDesktopWindow.toStdString().c_str(),
    //         entry.oldDesktopWindowWithProgramName.toStdString().c_str(), entry.newDesktopWindowWithProgramName.toStdString().c_str()
    //     );
    // }

    // Remove docks that are no longer present in the UI
    for (const QString &dockId : docksToRemove) {
        // blog(LOG_INFO, "Removing dock: %s", dockId.toStdString().c_str());
        
        releaseEmbeddedWindowByDockId(dockId.toStdString().c_str());
        obs_frontend_remove_dock(dockId.toStdString().c_str());
        activeDocks.remove(dockId);
        currentDocksMap.remove(dockId);
    }

    // Handle new or modified docks
    for (const DockEntry &entry : dockEntries) {
        if (entry.isNew()) {
            createOrUpdateDock(entry.newDockId.toStdString().c_str(), entry.newDockName.toStdString().c_str(), entry.newDesktopWindow.toStdString().c_str());
            // blog(LOG_INFO, "Created new dock: %s", entry.newDockId.toStdString().c_str());
        } else if (entry.isModified()) {
            if (currentDocksMap.contains(entry.oldDockId)) {
                obs_frontend_remove_dock(entry.oldDockId.toStdString().c_str());
                activeDocks.remove(entry.oldDockId.toStdString().c_str());
                // blog(LOG_INFO, "Removed old dock: %s", entry.oldDockId.toStdString().c_str());
            }
            createOrUpdateDock(entry.newDockId.toStdString().c_str(), entry.newDockName.toStdString().c_str(), entry.newDesktopWindow.toStdString().c_str());
            // blog(LOG_INFO, "Updated dock: %s", entry.newDockId.toStdString().c_str());
        } else if (entry.isUnchanged()) {
            createOrUpdateDock(entry.newDockId.toStdString().c_str(), entry.newDockName.toStdString().c_str(), entry.newDesktopWindow.toStdString().c_str());
            // blog(LOG_INFO, "Unchanged dock: %s", entry.newDockId.toStdString().c_str());
        }
    }

    // Save updated dock entries to the config file
    saveDockEntries(dockEntries);
}


// Save dock entries to configuration
void WindowDockUI::saveDockEntries(const QList<DockEntry> &dockEntries) {
    // blog(LOG_INFO, "saveDockEntries called");

    QString configDir = QDir::homePath() + "/AppData/Roaming/obs-studio/plugin_config/window-dock";
    QString filePath = configDir + "/" + CONFIG_FILE;

    QDir dir(configDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            blog(LOG_ERROR, "Failed to create directory: %s", configDir.toStdString().c_str());
            return;
        }
    }

    QJsonArray docksArray;

    // Iterate over dockEntries instead of tableWidget
    for (const DockEntry &entry : dockEntries) {
        if (!entry.newDockName.isEmpty() && entry.newDesktopWindow != obs_module_text("DockManagement.DesktopWindowComboBoxPlaceholder")) {
            QJsonObject dockObject;
            dockObject["dockId"] = entry.newDockId;
            dockObject["dockName"] = entry.newDockName;
            dockObject["desktopWindow"] = entry.newDesktopWindow;
            dockObject["desktopWindowWithProgramName"] = entry.newDesktopWindowWithProgramName;
            docksArray.append(dockObject);

            // blog(LOG_INFO, "Saved dock entry: %s (Window: %s)", entry.newDockName.toStdString().c_str(), entry.newDesktopWindow.toStdString().c_str());
        }
    }

    QJsonDocument doc(docksArray);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        blog(LOG_ERROR, "Failed to open config file for writing: %s", filePath.toStdString().c_str());
        return;
    }
    file.write(doc.toJson());
    file.close();

    // blog(LOG_INFO, "Docks successfully saved to file: %s", filePath.toStdString().c_str());
}




/*-------------------------------------------------------------------------------------*/
/*-----------------------------------DOCK MANAGEMENT-----------------------------------*/
/*-------------------------------------------------------------------------------------*/




QJsonObject WindowDockUI::getDockConfigById(const QString &dockId) {
    // blog(LOG_INFO, "getDockConfigById called");
    
    QJsonArray docksArray = loadConfigFile(); // Load config as QJsonArray

    for (const QJsonValue &value : docksArray) {
        if (value.isObject()) {
            QJsonObject dockObj = value.toObject();
            
            // Check if the current object has the correct dockId
            if (dockObj.value("dockId").toString() == dockId) {
                return dockObj; // Return the matching dock config
            }
        }
    }

    return QJsonObject(); // Return an empty object if no match is found
}

void WindowDockUI::attemptWindowCapture(const QString &dockId, const QString &windowTitle) {
    // blog(LOG_INFO, "attemptWindowCapture called");
    
    QJsonObject dockConfig = getDockConfigById(dockId); // Get the dock config by dockId

    if (dockConfig.isEmpty()) {
        // blog(LOG_INFO, "Dock ID not found in config file: %s", dockId.toStdString().c_str());
        return;
    }

    QString dockName = dockConfig.value("dockName").toString();
    QString desktopWindow = dockConfig.value("desktopWindow").toString();

    // Attempt to find and dock the window
    HWND hwnd = FindWindow(NULL, windowTitle.toStdWString().c_str());
    if (hwnd) {
        createOrUpdateDock(dockId, dockName, windowTitle);
    } else {
        // blog(LOG_INFO, "Failed to find window: %s", windowTitle.toStdString().c_str());
    }
}

// Function to detach/remove the embedded window
void WindowDockUI::detachEmbeddedWindow(const QString &dockId) {
    // blog(LOG_INFO, "detachEmbeddedWindow called");

    auto dockIter = activeDocks.find(dockId);
    if (dockIter != activeDocks.end()) {
        auto dockWidget = dockIter.value();
        
        HWND hwnd = dockWidget->getEmbeddedHwnd();
        if (hwnd) {
            // Reparent the window back to the desktop (or its original parent)
            SetParent(hwnd, nullptr);

            // Restore the window's previous style and apply changes
            SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

            // Restore the window's original position and size
            RECT originalRect;
            GetWindowRect(hwnd, &originalRect);
            SetWindowPos(hwnd, nullptr, originalRect.left, originalRect.top,
                         originalRect.right - originalRect.left,
                         originalRect.bottom - originalRect.top,
                         SWP_NOZORDER | SWP_FRAMECHANGED);

            // Ensure the window is visible
            ShowWindow(hwnd, SW_SHOW);

            // Clear the embedded HWND in the dock widget
            dockWidget->setEmbeddedHwnd(nullptr);

            // Clear the existing layout and set blank content
            clearLayout(dockWidget->layout());

            QString windowTitle = getDockConfigById(dockId)["desktopWindow"].toString();
            QWidget *blankWidget = createBlankDockContent(dockId, windowTitle);

            if (!dockWidget->layout()) {
                dockWidget->setLayout(new QVBoxLayout());  // or any other layout type
            }
            dockWidget->layout()->addWidget(blankWidget);

            // blog(LOG_INFO, "Detached window from dock with ID: %s", dockId.toStdString().c_str());
        } else {
            // blog(LOG_INFO, "No embedded window found for dock ID: %s", dockId.toStdString().c_str());
        }
    }
}

void WindowDockUI::releaseEmbeddedWindow(EmbeddedWindowWidget *dockWidget) {
    if (!dockWidget) {
        // blog(LOG_INFO, "Invalid dockWidget passed to releaseEmbeddedWindow");
        return;
    }

    // Get the embedded window handle (HWND)
    HWND hwnd = dockWidget->getEmbeddedHwnd();
    if (hwnd) {
        // Reparent the window back to the desktop (or its original parent)
        SetParent(hwnd, nullptr);

        // Restore the window's previous style and apply changes
        SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        // Restore the window's original position and size
        RECT originalRect;
        GetWindowRect(hwnd, &originalRect);
        SetWindowPos(hwnd, nullptr, originalRect.left, originalRect.top,
                     originalRect.right - originalRect.left,
                     originalRect.bottom - originalRect.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);

        // Ensure the window is visible
        ShowWindow(hwnd, SW_SHOW);

        // Clear the embedded HWND in the dock widget
        dockWidget->setEmbeddedHwnd(nullptr);

        // blog(LOG_INFO, "Embedded window released successfully.");
    } else {
        // blog(LOG_INFO, "No embedded window to release.");
    }
}

void WindowDockUI::releaseEmbeddedWindowByDockId(const QString &dockId) {
    // Find the dock widget by dockId
    auto dockIter = activeDocks.find(dockId);
    if (dockIter != activeDocks.end()) {
        auto dockWidget = dockIter.value();
        
        if (dockWidget) {
            // Call releaseEmbeddedWindow to perform the actual cleanup
            releaseEmbeddedWindow(dockWidget);
        } else {
            // blog(LOG_INFO, "Dock widget for dockId: %s is null", dockId.toStdString().c_str());
        }
    } else {
        // blog(LOG_INFO, "No dock widget found for dockId: %s", dockId.toStdString().c_str());
    }
}

void WindowDockUI::freeEmbeddedWindowsOnClose() {
    // blog(LOG_INFO, "freeEmbeddedWindowsOnClose called");

    // Iterate over all active docks
    for (auto dockIter = activeDocks.begin(); dockIter != activeDocks.end(); ++dockIter) {
        auto dockWidget = dockIter.value();

        // Use the releaseEmbeddedWindow function to free the embedded window
        releaseEmbeddedWindow(dockWidget);

        // blog(LOG_INFO, "Freed embedded window for dock: %s", dockIter.key().toStdString().c_str());
    }

    // blog(LOG_INFO, "All embedded windows have been freed.");
}

void WindowDockUI::clearLayout(QLayout *layout) {
    if (layout) {
        QLayoutItem *item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
}

void WindowDockUI::restoreDocksOnStartup() {
    // blog(LOG_INFO, "restoreDocksOnStartup called");

    QJsonArray docksArray = loadConfigFile();
    if (docksArray.isEmpty()) {
        // blog(LOG_INFO, "Config file is empty or failed to load. No dock entries to load.");
        return;
    }

    // Load existing docks from the config
    for (const QJsonValue &value : docksArray) {
        QJsonObject dockObject = value.toObject();
        QString dockId = dockObject["dockId"].toString();
        QString dockName = dockObject["dockName"].toString();
        QString windowTitle = dockObject["desktopWindow"].toString();

        initiateDockCreationOnStartup(dockId, dockName, windowTitle);
    }
}

void WindowDockUI::createOrUpdateDock(const QString &dockId, const QString &dockName, const QString &windowTitle) {
    // blog(LOG_INFO, "createOrUpdateDock called");
    // Attempt to find and update the dock
    auto existingDock = activeDocks.find(dockId);
    if (existingDock != activeDocks.end()) {
        // Dock exists
        updateDockContent(existingDock.value(), dockId, windowTitle);
    } else {
        // Dock does not exist
        createDockContent(dockId, dockName, windowTitle);
    }
}

EmbeddedWindowWidget* WindowDockUI::createBlankDockContent(const QString &dockId, const QString &windowTitle) {
    // blog(LOG_INFO, "createBlankDockContent called");
    EmbeddedWindowWidget *blankWidget = new EmbeddedWindowWidget();

    // Create the main vertical layout
    QVBoxLayout *mainLayout = new QVBoxLayout(blankWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // Add a stretch to the top to push the label and button to the center
    mainLayout->addStretch();

    // Add the label to the layout
    QLabel *messageLabel = new QLabel(obs_module_text("BlankDock.Description"), blankWidget);
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setWordWrap(true);

    // Make the label expand horizontally
    messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainLayout->addWidget(messageLabel);

    // Add the button below the label
    QPushButton *captureButton = new QPushButton(obs_module_text("BlankDock.CaptureWindow"), blankWidget);
    captureButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mainLayout->addWidget(captureButton, 0, Qt::AlignCenter);

    // Add a stretch to the bottom to keep the content vertically centered
    mainLayout->addStretch();

    blankWidget->setLayout(mainLayout);

    // Connect the button to the capture logic
    connect(captureButton, &QPushButton::clicked, this, [this, dockId, windowTitle]() {
        // blog(LOG_INFO, "Attempt window capture - Dock Id: %s, Window Title: %s", dockId.toStdString().c_str(), windowTitle.toStdString().c_str());
        // Attempt to capture the window based on the config file
        attemptWindowCapture(dockId, windowTitle);
    });

    return blankWidget;
}

EmbeddedWindowWidget* WindowDockUI::createDockContent(const QString &dockId, const QString &dockName, const QString &windowTitle) {
    // blog(LOG_INFO, "createDockContent called");

    EmbeddedWindowWidget *dockWidget = new EmbeddedWindowWidget();

    HWND hwnd = FindWindow(NULL, windowTitle.toStdWString().c_str());
    if (hwnd) {
        updateDockContent(dockWidget, dockId, windowTitle);
    } else {
        QWidget *blankWidget = createBlankDockContent(dockId, windowTitle);
        dockWidget->setLayout(new QVBoxLayout());
        dockWidget->layout()->addWidget(blankWidget);
    }

    // Add the dock to active docks
    activeDocks[dockId] = dockWidget;

    // Try to add the dock immediately
    if (!obs_frontend_add_dock_by_id(dockId.toStdString().c_str(), dockName.toStdString().c_str(), dockWidget)) {
        // blog(LOG_INFO, "Failed to add dock: %s", dockId.toStdString().c_str());
        return nullptr;
    } else {
        dockWidget->show();
    }

    // Return the created dock widget
    return dockWidget;
}

void WindowDockUI::updateDockContent(EmbeddedWindowWidget *dockWidget, const QString &dockId, const QString &windowTitle) {
    // blog(LOG_INFO, "updateDockContent called");

    HWND hwnd = FindWindow(NULL, windowTitle.toStdWString().c_str());
    if (hwnd) {
        // Ensure the embedded window does not have any toolbars or borders
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        // Set the embedded window handle in the dock widget
        dockWidget->setEmbeddedHwnd(hwnd);
        // blog(LOG_INFO, "Reparented window: HWND = %p, Widget WinId = %p", (void*)hwnd, (void*)dockWidget->winId());

        // Adjust the embedded window size to fit within the dock without borders or toolbars
        HWND contentWidgetWinId = (HWND)dockWidget->winId();
        RECT rect;
        GetClientRect(contentWidgetWinId, &rect);
        SetWindowPos(hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW | SWP_FRAMECHANGED);

        // Ensure the dock widget is visible and properly positioned
        dockWidget->show();
        dockWidget->raise();
        dockWidget->activateWindow();

        // Adjust the window size to account for DPI scaling
        dockWidget->adjustWindowSize();

        // Log the new size and position
        // blog(LOG_INFO, "Setting window position and size: left = %d, top = %d, width = %d, height = %d", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    } else {
        // blog(LOG_INFO, "Failed to find window with title: %s", windowTitle.toStdString().c_str());
    }
}

void WindowDockUI::initiateDockCreationOnStartup(const QString &dockId, const QString &dockName, const QString &windowTitle) {
    // blog(LOG_INFO, "initiateDockCreationOnStartup called");
    EmbeddedWindowWidget *dockWidget = new EmbeddedWindowWidget();

    // Initially set the dock to have blank content
    QWidget *blankWidget = createBlankDockContent(dockId, windowTitle);
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(blankWidget);
    dockWidget->setLayout(layout);

    // Add the dock to active docks
    activeDocks[dockId] = dockWidget;

    // Try to add the blank dock immediately
    if (!obs_frontend_add_dock_by_id(dockId.toStdString().c_str(), dockName.toStdString().c_str(), dockWidget)) {
        // blog(LOG_INFO, "Failed to add blank dock: %s", dockId.toStdString().c_str());
        delete dockWidget;
        return;
    } else {
        dockWidget->show();
    }

    QTimer *searchTimer = new QTimer(this);
    // Capture attemptCount by value in the lambda to ensure it is handled correctly
    connect(searchTimer, &QTimer::timeout, [this, dockId, dockWidget, windowTitle, attemptCount = 0, maxSearchAttempts = 5, searchTimer]() mutable {
        // blog(LOG_INFO, "Search attempt %d for window: %s", attemptCount + 1, windowTitle.toStdString().c_str());

        HWND hwnd = FindWindow(NULL, windowTitle.toStdWString().c_str());
        if (hwnd) {
            // blog(LOG_INFO, "Window found: %s", windowTitle.toStdString().c_str());
            updateDockContent(dockWidget, dockId, windowTitle);
            searchTimer->stop();
            searchTimer->deleteLater();  // Clean up timer after use
            return;
        }

        attemptCount++;
        if (attemptCount >= maxSearchAttempts) {
            // blog(LOG_INFO, "Window not found after %d attempts: %s", maxSearchAttempts, windowTitle.toStdString().c_str());
            searchTimer->stop();
            searchTimer->deleteLater();  // Clean up timer after max attempts
        }
    });

    searchTimer->start(6000); // Check every 6 seconds
}