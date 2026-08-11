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

    color: "transparent"
    background: null

    Maui.WindowBlur {
        view: root
        geometry: Qt.rect(0, 0, root.width, root.height)
        windowRadius: Maui.Style.radiusV
        enabled: true
    }

    Rectangle {
        anchors.fill: parent
        color: Maui.Theme.backgroundColor
        opacity: 0.76
        radius: Maui.Style.radiusV
        border.color: Qt.rgba(1, 1, 1, 0)
        border.width: 1
    }

    FB.FileDialog {
        id: chooser
        parent: root.contentItem
        mode: root.saveMode ? FB.FileDialog.Modes.Save : FB.FileDialog.Modes.Open
        // Use the portal window itself as the chooser surface.
        filling: true
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
        if (chooser.actions.length > 1) {
            // MauiKit's catalog can be unavailable in a minimal portal install.
            // Keep the buttons usable instead of leaving their labels empty.
            if (chooser.actions[0].text.length === 0)
                chooser.actions[0].text = qsTr("Cancel")

            if (root.acceptLabel.length > 0) {
                chooser.actions[1].text = root.acceptLabel.replace("_", "&")
            } else if (chooser.actions[1].text.length === 0) {
                chooser.actions[1].text = root.saveMode ? qsTr("Save") : qsTr("Open")
            }
        }
        chooser.open()
    }

    onClosing: (close) => {
        if (!completed) {
            completed = true
            bridge.reject()
        }
    }
}

