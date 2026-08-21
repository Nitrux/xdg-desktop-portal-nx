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

    function localPath(path) {
        const value = String(path || "")
        if (!value.startsWith("file:///"))
            return value

        try {
            return decodeURIComponent(value.substring(7))
        } catch (error) {
            return value.substring(7)
        }
    }

    title: dialogTitle
    width: 760
    height: 560
    minimumWidth: 520
    minimumHeight: 360
    visible: false
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

        sidebarComponent: PlacesSideBar
        {
            anchors.fill: parent
            hiddenPaths: chooser.hiddenSidebarPaths
                .filter(path => String(path) !== "file:///")
            currentPath: chooser.browser.currentPath
            onPlaceClicked: (path) => chooser.browser.openFolder(root.localPath(path))
        }

        onFinished: (urls) => {
            root.completed = true
            if (root.bridge)
                root.bridge.accept(urls)
        }
        onClosed: {
            chooser.browser.cancelSearch()
            if (!root.completed) {
                root.completed = true
                if (root.bridge)
                    root.bridge.reject()
            }
            root.visible = false
        }
    }

    Connections {
        target: root.bridge
        function onCloseUiRequested() {
            root.completed = true
            chooser.close()
            root.visible = false
        }
    }

    function updateActions() {
        if (chooser.actions.length <= 1)
            return

        if (chooser.actions[0].text.length === 0)
            chooser.actions[0].text = qsTr("Cancel")

        chooser.actions[1].text = root.acceptLabel.length > 0
            ? root.acceptLabel.replace("_", "&")
            : root.saveMode ? qsTr("Save") : qsTr("Open")
    }

    function present() {
        root.completed = false
        chooser.currentPath = root.localPath(root.initialFolder)
        chooser.browser.currentFMList.onlyDirs = root.directoryMode
        chooser.browser.currentFMList.filters = root.nameFilters
        chooser.textField.text = root.suggestedName
        root.updateActions()
        root.visible = true
        Qt.callLater(function() {
            if (!root.completed)
                chooser.open()
        })
    }

    function dismiss() {
        root.completed = true
        if (chooser.opened)
            chooser.close()
        root.visible = false
    }
    onClosing: (close) => {
        if (!completed) {
            completed = true
            chooser.close()
            if (bridge)
                bridge.reject()
        }
        root.visible = false
    }
}

