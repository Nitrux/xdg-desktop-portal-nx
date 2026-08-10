// SPDX-License-Identifier: BSD-3-Clause
import QtQuick
import QtQuick.Controls
import org.mauikit.controls as Maui
import org.mauikit.filebrowsing as FB

Maui.ApplicationWindow {
    id: root

    required property var bridge
    property string dialogTitle: qsTr("Choose a File")
    property bool saveMode: false
    property bool modalDialog: true
    property bool multiple: false
    property bool directoryMode: false
    property var nameFilters: []
    property string initialFolder: ""
    property string suggestedName: ""
    property string acceptLabel: ""
    property bool completed: false

    title: dialogTitle
    width: 760
    height: 560
    minimumWidth: 520
    minimumHeight: 360
    visible: true
    modality: modalDialog ? Qt.ApplicationModal : Qt.NonModal

    Maui.Page {
        anchors.fill: parent

        Maui.Holder {
            anchors.fill: parent
            title: root.dialogTitle
            body: qsTr("Select a location in the file chooser.")
            emoji: "folder-open"
        }
    }

    FB.FileDialog {
        id: chooser
        parent: root.contentItem
        mode: root.saveMode ? FB.FileDialog.Modes.Save : FB.FileDialog.Modes.Open
        singleSelection: !root.multiple
        suggestedFileName: root.suggestedName

        onFinished: (urls) => {
            root.completed = true
            root.bridge.accept(urls)
            root.close()
        }
        onClosed: {
            if (!root.completed) {
                root.completed = true
                root.bridge.reject()
                root.close()
            }
        }
    }

    Connections {
        target: root.bridge
        function onCloseUiRequested() {
            root.completed = true
            chooser.close()
            root.close()
        }
    }

    Component.onCompleted: {
        chooser.currentPath = root.initialFolder
        chooser.browser.currentFMList.onlyDirs = root.directoryMode
        chooser.browser.currentFMList.filters = root.nameFilters
        if (root.acceptLabel.length > 0 && chooser.actions.length > 1)
            chooser.actions[1].text = root.acceptLabel.replace("_", "&")
        chooser.open()
    }

    onClosing: (close) => {
        if (!completed) {
            completed = true
            bridge.reject()
        }
    }
}

