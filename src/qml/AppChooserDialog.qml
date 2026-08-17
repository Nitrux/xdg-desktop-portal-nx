// SPDX-License-Identifier: BSD-3-Clause
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.mauikit.controls as Maui

Maui.ApplicationWindow {
    id: root

    required property var bridge
    property string dialogTitle: qsTr("Choose an Application")
    property string subject: ""
    property bool modalDialog: true
    property var applications: []
    property string lastChoice: ""
    property string selectedId: ""
    property bool completed: false

    title: dialogTitle
    width: 620
    height: 560
    minimumWidth: 440
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

    function rebuild() {
        appModel.clear()
        const needle = searchField.text.toLocaleLowerCase()
        for (let i = 0; i < applications.length; ++i) {
            const app = applications[i]
            const haystack = (app.name + " " + app.comment + " " + app.desktopId).toLocaleLowerCase()
            if (needle.length === 0 || haystack.indexOf(needle) !== -1)
                appModel.append(app)
        }
    }

    function acceptSelection() {
        if (selectedId.length === 0)
            return
        completed = true
        bridge.accept(selectedId)
        visible = false
    }

    ListModel {
        id: appModel
    }

    Maui.Page {
        anchors.fill: parent
        background: null
        headerMargins: Maui.Style.contentMargins
        headBar.visible: true
        headBar.forceCenterMiddleContent: false

        headBar.middleContent: Maui.SearchField {
            id: searchField
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            placeholderText: qsTr("Choose an application")
            onTextChanged: root.rebuild()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Maui.Style.space.medium

            Maui.SectionItem {
                Layout.fillWidth: true
                Layout.leftMargin: Maui.Style.contentMargins
                Layout.rightMargin: Maui.Style.contentMargins
                Layout.topMargin: Maui.Style.space.small
                visible: root.subject.length > 0
                flat: false
                label1.text: qsTr("Open %1 with:").arg(root.subject)
                label1.elide: Text.ElideMiddle
            }

            Maui.ListBrowser {
                id: appList
                clip: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: appModel
                currentIndex: -1

                delegate: Maui.ListBrowserDelegate {
                    width: ListView.view.width
                    isCurrentItem: root.selectedId === model.desktopId
                    template.iconSource: model.icon.length > 0 ? model.icon : "application-x-executable"
                    template.label1.text: model.name
                    template.label2.text: model.comment.length > 0 ? model.comment : model.desktopId
                    onClicked: root.selectedId = model.desktopId
                    onDoubleClicked: {
                        root.selectedId = model.desktopId
                        root.acceptSelection()
                    }
                }
            }

            GridLayout {
                id: actionBar
                Layout.fillWidth: true
                Layout.leftMargin: Maui.Style.contentMargins
                Layout.rightMargin: Maui.Style.contentMargins
                Layout.bottomMargin: Maui.Style.contentMargins
                rows: 1
                columns: 2
                rowSpacing: Maui.Style.space.small
                columnSpacing: Maui.Style.space.small

                readonly property real buttonWidth: Math.max(0,
                    (width - columnSpacing) / columns)

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: actionBar.buttonWidth
                    text: qsTr("Cancel")
                    onClicked: {
                        root.completed = true
                        root.bridge.reject()
                        root.visible = false
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: actionBar.buttonWidth
                    text: qsTr("Open")
                    enabled: root.selectedId.length > 0
                    Maui.Controls.status: Maui.Controls.Positive
                    onClicked: root.acceptSelection()
                }
            }
        }
    }

    Connections {
        target: root.bridge
        function onCloseUiRequested() {
            root.completed = true
            root.visible = false
        }
    }

    function selectLastChoice() {
        selectedId = ""
        rebuild()
        for (let i = 0; i < applications.length; ++i) {
            if (applications[i].desktopId === lastChoice) {
                selectedId = lastChoice
                break
            }
        }
    }

    function present() {
        completed = false
        searchField.text = ""
        selectLastChoice()
        visible = true
    }

    function dismiss() {
        completed = true
        visible = false
    }

    onApplicationsChanged: rebuild()
    onClosing: (close) => {
        if (!completed) {
            completed = true
            if (bridge)
                bridge.reject()
        }
        visible = false
    }
}
